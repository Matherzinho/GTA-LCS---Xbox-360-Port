// Native IMG streaming. Cooperative completion replaces POSIX signal cancellation.
#include "common.h"
#include "CdStream.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include <xsdk/log.h>

namespace {
struct Image { int fd; uint64 bytes; char name[64]; } images[MAX_CDIMAGES];
struct Channel { int image; uint32 sector,count; void *buffer; int status; bool pending,reading; } channels[MAX_CDCHANNELS];
pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t work=PTHREAD_COND_INITIALIZER,done=PTHREAD_COND_INITIALIZER;
pthread_t thread;
int numImages,numChannels,lastChannel=-1,lastPosition;
bool running,stopping;
bool valid(int channel) { return channel>=0 && channel<numChannels; }
// Four bounded read-through windows. Prefetch only after sequential access;
// random reads stay exact-sized so slow USB does not read unused megabytes.
constexpr size_t WINDOW=256*1024;
struct ReadWindow { int image=-1; uint64 offset=0; size_t size=0; unsigned stamp=0; unsigned char data[WINDOW]; } readCache[4];
unsigned readStamp; int previousImage=-1; uint64 previousEnd;
uint32 cacheHits,cacheMisses,readCalls,readKiB,servedKiB;
static size_t cachedRead(int image,int fd,uint64 fileSize,uint64 offset,unsigned char *out,size_t bytes) {
 size_t copied=0;
 while(copied<bytes) {
  uint64 at=offset+copied; ReadWindow *slot=nullptr;
  for(auto &w:readCache)if(w.image==image && at>=w.offset && at-w.offset<w.size){slot=&w;break;}
  if(slot)++cacheHits;
  else {
   ++cacheMisses;slot=&readCache[0];
   for(auto &w:readCache)if(w.stamp<slot->stamp)slot=&w;
   slot->image=-1;slot->size=0;
   size_t want=bytes-copied;if(want>WINDOW)want=WINDOW;
   if(previousImage==image && previousEnd==at && want<64*1024)want=64*1024;
   if(want>fileSize-at)want=size_t(fileSize-at);
   while(slot->size<want) {
    ssize_t got=pread(fd,slot->data+slot->size,want-slot->size,at+slot->size);
    ++readCalls;if(got<0 && errno==EINTR)continue;if(got<=0)break;
    slot->size+=size_t(got);readKiB+=uint32(got)/1024;
   }
   if(!slot->size)break;
   slot->image=image;slot->offset=at;
  }
  slot->stamp=++readStamp;size_t n=slot->size-size_t(at-slot->offset);
  if(n>bytes-copied)n=bytes-copied;
  std::memcpy(out+copied,slot->data+size_t(at-slot->offset),n);copied+=n;
 }
 previousImage=image;previousEnd=offset+copied;servedKiB+=copied/1024;
 return copied;
}
void *worker(void*) {
 pthread_mutex_lock(&lock);
 for(;;) {
  int id=-1;
  for(int n=0;n<numChannels;++n) { int i=(lastChannel+1+n)%numChannels; if(channels[i].pending) { id=i; break; } }
  if(id<0) { if(stopping) break; pthread_cond_wait(&work,&lock); continue; }
  lastChannel=id; Channel &c=channels[id]; c.pending=false; c.reading=true;
  int image=c.image; int fd=images[image].fd;uint64 fileSize=images[image].bytes; uint64 offset=uint64(c.sector)*CDSTREAM_SECTOR_SIZE;
  size_t bytes=size_t(c.count)*CDSTREAM_SECTOR_SIZE,readBytes=0; unsigned char *out=(unsigned char*)c.buffer;
  pthread_mutex_unlock(&lock);
  readBytes=cachedRead(image,fd,fileSize,offset,out,bytes);
  pthread_mutex_lock(&lock);
  c.status=readBytes==bytes?STREAM_NONE:STREAM_ERROR;
  c.reading=false; c.count=0; pthread_cond_broadcast(&done);
 }
 pthread_mutex_unlock(&lock); return nullptr;
}
}
#ifdef FLUSHABLE_STREAMING
bool flushStream[MAX_CDCHANNELS];
#endif
void CdStreamInitThread() { /* Thread ownership belongs to CdStreamInit/Shutdown. */ }
void CdStreamInit(int32 count) {
 if(running) CdStreamShutdown();
 if(count<=0 || count>MAX_CDCHANNELS) { xsdk_log_write("LCS stream: invalid channel count %d",count); return; }
 for(auto &w:readCache){w.image=-1;w.size=0;w.stamp=0;}previousImage=-1;readStamp=cacheHits=cacheMisses=readCalls=readKiB=servedKiB=0;
 std::memset(images,0,sizeof images); std::memset(channels,0,sizeof channels);
#ifdef FLUSHABLE_STREAMING
 std::memset(flushStream,0,sizeof flushStream);
#endif
 numImages=0; numChannels=count; lastChannel=-1; stopping=false; lastPosition=0;
 int error=pthread_create(&thread,nullptr,worker,nullptr);
 running=error==0;
 if(error) { numChannels=0; xsdk_log_write("LCS stream: worker creation failed %d",error); }
}
bool CdStreamAddImage(const char *path) {
 if(!running || !path || std::strlen(path)>=sizeof images[0].name || numImages>=MAX_CDIMAGES) return false;
 int fd=open(path,O_RDONLY); if(fd<0) return false;
 struct stat st; if(fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_size<0) { close(fd); return false; }
 pthread_mutex_lock(&lock);
 Image &im=images[numImages++]; im.fd=fd; im.bytes=st.st_size; std::strcpy(im.name,path);
 pthread_mutex_unlock(&lock); return true;
}
uint32 GetGTA3ImgSize() { return numImages?uint32(images[0].bytes):0; }
int32 CdStreamRead(int32 id,void *buffer,uint32 offset,uint32 count) {
 int image=_GET_INDEX(offset); uint32 sector=_GET_OFFSET(offset);
 if(!running || !valid(id) || !buffer || image>=numImages || uint64(sector)*2048+uint64(count)*2048>images[image].bytes || uint64(count)*2048>SIZE_MAX) return STREAM_ERROR;
 pthread_mutex_lock(&lock);
 Channel &c=channels[id];
 if(c.pending || c.reading) {
  if(c.buffer==buffer && c.image==image && c.sector==sector && c.count>=count) { pthread_mutex_unlock(&lock); return STREAM_SUCCESS; }
  pthread_mutex_unlock(&lock);
#ifdef FLUSHABLE_STREAMING
  flushStream[id]=true; CdStreamSync(id); pthread_mutex_lock(&lock);
#else
  return STREAM_NONE;
#endif
 }
 c.image=image; c.sector=sector; c.count=count; c.buffer=buffer; c.status=STREAM_NONE; c.pending=count!=0;
 lastPosition=offset+count; pthread_cond_signal(&work); pthread_mutex_unlock(&lock); return STREAM_SUCCESS;
}
int32 CdStreamSync(int32 id) {
 if(!running || !valid(id)) return STREAM_ERROR;
 pthread_mutex_lock(&lock); Channel &c=channels[id]; bool discard=false;
#ifdef FLUSHABLE_STREAMING
 discard=flushStream[id]; flushStream[id]=false;
#endif
 if(discard && c.pending) { c.pending=false; c.count=0; }
 while(c.pending || c.reading) pthread_cond_wait(&done,&lock);
 // CdStreamSync's flush contract is completion (NONE), not queue status.
 int result=discard?STREAM_NONE:c.status; c.status=result;
 pthread_mutex_unlock(&lock); return result;
}
int32 CdStreamGetStatus(int32 id) {
 if(!running || !valid(id)) return STREAM_ERROR;
 pthread_mutex_lock(&lock); Channel &c=channels[id];
 int result=c.reading?STREAM_READING:c.pending?STREAM_WAITING:c.status;
 if(!c.reading && !c.pending) c.status=STREAM_NONE;
 pthread_mutex_unlock(&lock); return result;
}
int32 CdStreamGetLastPosn() { return lastPosition; }
char *CdStreamGetImageName(int32 id) { return id>=0 && id<numImages?images[id].name:nullptr; }
int32 CdStreamGetNumImages() { return numImages; }
void CdStreamRemoveImages() {
 if(!running) return;
 for(int i=0;i<numChannels;++i) {
#ifdef FLUSHABLE_STREAMING
  flushStream[i]=true;
#endif
  CdStreamSync(i);
 }
 pthread_mutex_lock(&lock);
 for(auto &w:readCache){w.image=-1;w.size=0;w.stamp=0;}previousImage=-1;
 for(int i=0;i<numImages;++i) close(images[i].fd);
 numImages=0; pthread_mutex_unlock(&lock);
}
void CdStreamShutdown() {
 if(!running) return;
 CdStreamRemoveImages(); pthread_mutex_lock(&lock); stopping=true; pthread_cond_signal(&work); pthread_mutex_unlock(&lock);
 pthread_join(thread,nullptr); running=false; numChannels=0;
}
void AddToQueue(Queue *q,int32 item) {
 if(!q || !q->items || q->size<2) return;
 int next=(q->tail+1)%q->size; if(next==q->head) return;
 q->items[q->tail]=item; q->tail=next;
}
int32 GetFirstInQueue(Queue *q) { return !q || q->head==q->tail?-1:q->items[q->head]; }
void RemoveFirstInQueue(Queue *q) { if(q && q->head!=q->tail) q->head=(q->head+1)%q->size; }

extern "C" void lcs_stream_report() {
 pthread_mutex_lock(&lock);
 bool busy=false;for(int i=0;i<numChannels;++i)busy|=channels[i].reading;
 if(!busy)xsdk_log_write("LCS STREAM cache_hits=%u misses=%u physical_reads=%u physical_kib=%u served_kib=%u cache_reserved_kib=1024",cacheHits,cacheMisses,readCalls,readKiB,servedKiB);
 pthread_mutex_unlock(&lock);
}
