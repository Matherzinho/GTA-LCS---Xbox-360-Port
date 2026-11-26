// Integration diagnostic for the real game's CFileMgr and native IMG streaming.
#include "common.h"
#include "crossplatform.h"
#include "FileMgr.h"
#include "CdStream.h"
#include "xsdk_paths.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
// Keep SDK HANDLE/SYSTEMTIME definitions out of the game's desktop shim.
extern "C" void xsdk_log_write(const char *format,...);

#define CHECK(c) do { if(!(c)) { xsdk_log_write("LCS IO FAIL line %d errno %d: %s",__LINE__,errno,#c); return 1; } } while(0)
static bool writeFile(const char *path,const void *bytes,size_t size) {
 int fd=CFileMgr::OpenFileForWriting(path); if(!fd) return false;
 bool ok=CFileMgr::Write(fd,(const char*)bytes,size)==size; return CFileMgr::CloseFile(fd)==0 && ok;
}
int main() {
 CHECK(chdir("game:\\data")==0); CFileMgr::Initialise();
 char path[1024]; CHECK(lcs_xsdk_normalize_path(".\\models/../TEXT/american.gxt",path,sizeof path)==0);
 CHECK(std::strcmp(path,"game:\\data\\TEXT\\american.gxt")==0);
 CHECK(realpath("models/../MODELS/gta3.dir",path)!=nullptr);
 CHECK(access(path,F_OK)==0); CHECK(access("no-such-lcs-file",F_OK)==-1);
 CHECK(realpath("no-such-lcs-file",path)==nullptr);
 CHECK(lcs_xsdk_normalize_path("game:\\../../data",path,sizeof path)==0 && std::strcmp(path,"game:\\data")==0);
 char tiny[4]={1,2,3,4}; CHECK(lcs_xsdk_normalize_path("game:\\data",tiny,sizeof tiny)==-1 && tiny[0]==1);
 CFileMgr::SetDir("MODELS"); int dir=CFileMgr::OpenFile("GTA3.DIR"); CHECK(dir!=0); CHECK(CFileMgr::CloseFile(dir)==0);
 CFileMgr::SetDir("");
 if(mkdir(_psGetUserFilesFolder(),0777)) CHECK(errno==EEXIST);
 CFileMgr::SetDirMyDocuments();
 // Fresh fixture directory per run: Xenia may retain duplicate directory entries
 // when a rename replaces a target cached before this title started.
 timespec stamp; CHECK(clock_gettime(CLOCK_MONOTONIC,&stamp)==0);
 char scratch[64]; snprintf(scratch,sizeof scratch,"lcs-io-%08x-%08x",(unsigned)stamp.tv_sec,(unsigned)stamp.tv_nsec);
 CHECK(mkdir(scratch,0777)==0 && chdir(scratch)==0);
 xsdk_log_write("LCS IO fixture directory: %s",scratch);
 const unsigned char save[]={0x00,0x1a,0x0d,0x0a,0x80,0xff,0x42};
 CHECK(writeFile("lcs-io-a.bin",save,sizeof save));
 CHECK(caserename("lcs-io-a.bin","lcs-io-b.bin")==0);
 int fd=CFileMgr::OpenFile("LCS-IO-B.BIN"); CHECK(fd!=0);
 unsigned char loaded[sizeof save]={}; CHECK(CFileMgr::Read(fd,(const char*)loaded,sizeof loaded)==sizeof loaded);
 CHECK(std::memcmp(save,loaded,sizeof save)==0); CHECK(CFileMgr::CloseFile(fd)==0);
 CHECK(writeFile("lcs-io-c.bin",save,sizeof save));
 const char text[]="one\r\ntwo\n"; CHECK(writeFile("lcs-io-lines.txt",text,sizeof text-1));
 unsigned char line[16]={}; CHECK(CFileMgr::LoadFile("lcs-io-lines.txt",line,sizeof line,"rt")==8);
 CHECK(std::strcmp((char*)line,"one\ntwo\n")==0);
 struct { unsigned char buf[4]; unsigned char guard[4]; } small={{0},{0xaa,0xbb,0xcc,0xdd}};
 CHECK(CFileMgr::LoadFile("lcs-io-lines.txt",small.buf,sizeof small.buf,"rb")==-1 && errno==EFBIG);
 CHECK(small.guard[0]==0xaa && small.guard[3]==0xdd && small.buf[3]==0);
 WIN32_FIND_DATA found; HANDLE search=FindFirstFile("lcs-io-?.bin",&found); CHECK(search!=nullptr);
 int matches=0; do { ++matches; xsdk_log_write("LCS IO matching file: %s",found.cFileName); } while(FindNextFile(search,&found)); FindClose(search); CHECK(matches==2);
 CHECK(FindFirstFile("game:\\no-such-directory\\*.bin",&found)==nullptr);
 xsdk_log_write("LCS IO: path, binary roundtrip, rename, text CRLF, capacity guard and enumeration PASS");
 CFileMgr::SetDir(""); CdStreamInit(2); CHECK(CdStreamAddImage("MODELS\\GTA3.IMG"));
 int direct=open("models/gta3.img",O_RDONLY); CHECK(direct>=0);
 struct stat st; CHECK(fstat(direct,&st)==0 && GetGTA3ImgSize()==uint32(st.st_size));
 unsigned char a[2048],b[2048],expected[2048];
 for(int i=0;i<32;++i) {
  CHECK(CdStreamRead(0,a,i*2,1)==STREAM_SUCCESS); CHECK(CdStreamRead(1,b,i*2+1,1)==STREAM_SUCCESS);
  CHECK(CdStreamSync(0)==STREAM_NONE && CdStreamSync(1)==STREAM_NONE);
  CHECK(pread(direct,expected,sizeof expected,i*4096)==sizeof expected && std::memcmp(a,expected,sizeof a)==0);
  CHECK(pread(direct,expected,sizeof expected,i*4096+2048)==sizeof expected && std::memcmp(b,expected,sizeof b)==0);
 }
 // Cross read-window boundaries, evict all four windows, then revisit data.
 static unsigned char bulk[600*1024],reference[600*1024];
 // Reusing a busy channel with a different destination must not claim that
 // the new buffer was filled by the old request.
 std::memset(reference,0xa5,sizeof reference);
 CHECK(CdStreamRead(0,bulk,300,300)==STREAM_SUCCESS);
 int duplicate=CdStreamRead(0,reference,300,300);
 CHECK(duplicate==STREAM_NONE || duplicate==STREAM_SUCCESS);
 CHECK(CdStreamSync(0)==STREAM_NONE);
 if(duplicate==STREAM_NONE)CHECK(CdStreamRead(0,reference,300,300)==STREAM_SUCCESS && CdStreamSync(0)==STREAM_NONE);
 CHECK(pread(direct,bulk,sizeof bulk,300u*2048)==sizeof bulk);
 CHECK(!std::memcmp(bulk,reference,sizeof bulk));
 const uint32 sectors[]={0,300,600,900,1200,0,127,1};
 for(uint32 sector:sectors){
  CHECK(CdStreamRead(0,bulk,sector,300)==STREAM_SUCCESS && CdStreamSync(0)==STREAM_NONE);
  CHECK(pread(direct,reference,sizeof reference,uint64(sector)*2048)==sizeof reference);
  CHECK(!std::memcmp(bulk,reference,sizeof bulk));
 }
 CHECK(CdStreamRead(0,a,uint32(st.st_size/2048),2)==STREAM_ERROR);
 CHECK(CdStreamRead(-1,a,0,1)==STREAM_ERROR); close(direct);
#ifdef FLUSHABLE_STREAMING
 CHECK(CdStreamRead(0,a,0,1)==STREAM_SUCCESS); flushStream[0]=true;
 CHECK(CdStreamSync(0)==STREAM_NONE); CHECK(CdStreamRead(0,a,1,1)==STREAM_SUCCESS && CdStreamSync(0)==STREAM_NONE);
#endif
 unsigned char zero[4096]; for(size_t i=0;i<sizeof zero;++i) zero[i]=(i*17+5)&255;
 CHECK(writeFile("game:\\userfiles\\lcs-stream-fixture.bin",zero,sizeof zero));
 CHECK(CdStreamAddImage("game:\\userfiles\\lcs-stream-fixture.bin"));
 CHECK(CdStreamRead(0,a,(1u<<24)|1u,1)==STREAM_SUCCESS && CdStreamSync(0)==STREAM_NONE);
 CHECK(!std::memcmp(a,zero+2048,2048));
 // Closing images must wait for queued or active buffers before closing handles.
 CHECK(CdStreamRead(0,a,0,1)==STREAM_SUCCESS && CdStreamRead(1,b,1,1)==STREAM_SUCCESS);
 CdStreamRemoveImages(); CHECK(CdStreamGetNumImages()==0); CdStreamShutdown();
 // The file shrinks after its size was cached. A short read must be an error.
 int fixture=open("game:\\userfiles\\lcs-stream-fixture.bin",O_CREAT|O_TRUNC|O_RDWR,0666); CHECK(fixture>=0);
 CHECK(write(fixture,zero,sizeof zero)==sizeof zero);
 CdStreamInit(2); CHECK(CdStreamAddImage("game:\\userfiles\\lcs-stream-fixture.bin"));
 CHECK(ftruncate(fixture,100)==0); CHECK(CdStreamRead(0,a,0,1)==STREAM_SUCCESS);
 CHECK(CdStreamSync(0)==STREAM_ERROR); CdStreamShutdown(); close(fixture);
 xsdk_log_write("LCS IO PASS: 64 real IMG sector comparisons, two channels, image selector, bounds, flush, short read and active restart/shutdown");
 return 0;
}
