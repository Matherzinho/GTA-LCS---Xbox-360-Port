// Native XSDK sample manager: PCM SFX + stereo VAG radio/cutscenes.
// Portable VAG decoding/bank mapping from pinned reLCS PowerPC reference;
// native render-driver output, mixer, cache and thread lifecycle are local.
#include "common.h"
#include "sampman.h"
#include "AudioManager.h"
#include "crossplatform.h"
#include "xsdk_sound_sink.h"
#include "lcs_binary.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
static void audioSleep(unsigned us) { timespec delay={time_t(us/1000000),long(us%1000000)*1000}; nanosleep(&delay,nullptr); }
extern "C" void xsdk_log_write(const char*,...);
#include "xsdk_audio_codec.h"
cSampleManager SampleManager;
bool8 _bSampmanInitialised=FALSE;
uint32 BankStartOffset[MAX_SFX_BANKS];
uint32 nNumMP3s=0;
int gBankStartOffset[67];
static void
SetUpDebugBanksInfo()
{
	gBankStartOffset[0] = SFX_AIR_BRAKES;
	gBankStartOffset[1] = SFX_EMPTY;
	gBankStartOffset[2] = SFX_AMBULAN_VOICE_1_VAN_1;
	gBankStartOffset[3] = SFX_FE_BACK;
	gBankStartOffset[4] = SFX_CAR_ACCEL_1;
	gBankStartOffset[5] = SFX_CAR_ACCEL_2;
	gBankStartOffset[6] = SFX_CAR_ACCEL_3;
	gBankStartOffset[7] = SFX_CAR_ACCEL_4;
	gBankStartOffset[8] = SFX_CAR_ACCEL_5;
	gBankStartOffset[9] = SFX_CAR_ACCEL_6;
	gBankStartOffset[10] = SFX_CAR_ACCEL_7;
	gBankStartOffset[11] = SFX_CAR_ACCEL_8;
	gBankStartOffset[12] = SFX_CAR_ACCEL_9;
	gBankStartOffset[13] = SFX_CAR_ACCEL_10;
	gBankStartOffset[14] = SFX_CAR_ACCEL_11;
	gBankStartOffset[15] = SFX_CAR_ACCEL_12;
	gBankStartOffset[16] = SFX_CAR_CHAINSAW_IDLE;
	gBankStartOffset[17] = SFX_RC_IDLE;
	gBankStartOffset[18] = SFX_CAR_RC_HELI;
	gBankStartOffset[19] = SFX_CAR_ACCEL_16;
	gBankStartOffset[20] = SFX_CAR_ACCEL_17;
	gBankStartOffset[21] = SFX_CAR_ACCEL_18;
	gBankStartOffset[22] = SFX_CAR_ACCEL_19;
	gBankStartOffset[23] = SFX_CAR_ACCEL_20;
	gBankStartOffset[24] = SFX_CAR_ACCEL_21;
	gBankStartOffset[25] = SFX_CAR_AFTER_ACCEL_21;
	gBankStartOffset[26] = SFX_CAR_FINGER_OFF_ACCEL_21;
	gBankStartOffset[27] = SFX_CAR_ACCEL_22;
	gBankStartOffset[28] = SFX_CAR_AFTER_ACCEL_22;
	gBankStartOffset[29] = SFX_CAR_FINGER_OFF_ACCEL_22;
	gBankStartOffset[30] = SFX_HELI_APACHE_1;
	gBankStartOffset[31] = SFX_HELI_UNUSED_1;
	gBankStartOffset[32] = SFX_HELI_UNUSED_2;
	gBankStartOffset[33] = SFX_HELI_UNUSED_3;
	gBankStartOffset[34] = SFX_HELI_UNUSED_4;
	gBankStartOffset[35] = SFX_SEAPLANE_LOW;
	gBankStartOffset[36] = SFX_PLANE_UNUSED_1;
	gBankStartOffset[37] = SFX_PLANE_UNUSED_2;
	gBankStartOffset[38] = SFX_PLANE_UNUSED_3;
	gBankStartOffset[39] = SFX_PLANE_UNUSED_4;
	gBankStartOffset[40] = SFX_BUILDINGS_BANK_ALARM;
	gBankStartOffset[41] = SFX_BUILDING_SNORE;
	gBankStartOffset[42] = SFX_BUILDING_BAR_1;
	gBankStartOffset[43] = SFX_BUILDING_BAR_2;
	gBankStartOffset[44] = SFX_BUILDING_BAR_3;
	gBankStartOffset[45] = SFX_BUILDING_BAR_4;
	gBankStartOffset[46] = SFX_BUILDING_MALIBU_1;
	gBankStartOffset[47] = SFX_BUILDING_MALIBU_2;
	gBankStartOffset[48] = SFX_BUILDING_MALIBU_3;
	gBankStartOffset[49] = SFX_BUILDING_STRIP_1;
	gBankStartOffset[50] = SFX_BUILDING_STRIP_2;
	gBankStartOffset[51] = SFX_BUILDING_STRIP_3;
	gBankStartOffset[52] = SFX_BUILDING_CHURCH;
	gBankStartOffset[53] = SFX_BUILDING_FAN_1;
	gBankStartOffset[54] = SFX_BUILDING_FAN_2;
	gBankStartOffset[55] = SFX_BUILDING_INSECTS_1;
	gBankStartOffset[56] = SFX_BUILDING_INSECTS_2;
	gBankStartOffset[57] = SFX_CLUB_1;
	gBankStartOffset[58] = SFX_CLUB_2;
	gBankStartOffset[59] = SFX_CLUB_3;
	gBankStartOffset[60] = SFX_CLUB_4;
	gBankStartOffset[61] = SFX_FOOTSTEP_GRASS_1;
	gBankStartOffset[62] = SFX_FOOTSTEP_GRAVEL_1;
	gBankStartOffset[63] = SFX_FOOTSTEP_WOOD_1;
	gBankStartOffset[64] = SFX_FOOTSTEP_METAL_1;
	gBankStartOffset[65] = SFX_FOOTSTEP_WATER_1;
	gBankStartOffset[66] = SFX_FOOTSTEP_SAND_1;
}

namespace {
using PCM=std::vector<int16>;
struct Channel {
 std::shared_ptr<PCM> pcm; double pos=0; uint32 rate=0,loopStart=0; int32 loopEnd=-1;
 uint32 loops=1,played=0,volume=127,pan=63; bool playing=false,is3D=false;
 float x=0,y=0,z=0,minDist=1,maxDist=100;
};
Channel channels[NUM_CHANNELS];
std::shared_ptr<PCM> cache[TOTAL_AUDIO_SAMPLES];
uint32 stamps[TOTAL_AUDIO_SAMPLES],stamp,cacheBytes;
constexpr uint32 CACHE_LIMIT=32u*1024u*1024u,RING=32768;
int16 streamRing[RING*2];
uint32 head=0,tail=0,streamVersion=0,streamId=UINT32_MAX,streamStart=0,streamConsumed=0;
uint32 streamLengths[TOTAL_STREAMED_SOUNDS];
double streamFrac=0; bool streamPlaying=false,streamPaused=false,streamEOF=false,streamLoop=false;
uint32 streamVol=127,streamPan=63; bool streamFX=false;
uint32 fxMaster=127,fxFade=127,musicMaster=127,musicFade=127; bool mono=false,released=false;
bool bankLoaded[MAX_SFX_BANKS]; uint32 missionSamples[8],pedSamples[MAX_PEDSFX],pedNext;
pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_t audioThread,decodeThread;
bool audioCreated=false,decodeCreated=false,running=false;
FILE *rawFile=nullptr; uint32 rawSize=0;
uint32 mixedFrames=0,nonzeroFrames=0,sfxStarts=0,streamStarts=0;
struct Lock { Lock(){pthread_mutex_lock(&mutex);} ~Lock(){pthread_mutex_unlock(&mutex);} };
static bool valid(uint32 n) {return n<NUM_CHANNELS;}
static uint32 cap(uint32 n) {return n>127?127:n;}
static float clamp(float n){if(n>1)return 1;if(n< -1)return -1;return n;}
static std::shared_ptr<PCM> loadSample(uint32 id,const tSample *table) {
 if(id>=TOTAL_AUDIO_SAMPLES || !rawFile)return {};
 if(cache[id]) {stamps[id]=++stamp;return cache[id];}
 const tSample &s=table[id]; if(!s.nSize || (s.nSize&1) || s.nOffset>rawSize || s.nSize>rawSize-s.nOffset)return {};
 auto pcm=std::make_shared<PCM>(s.nSize/2);
 if(fseek(rawFile,s.nOffset,SEEK_SET) || fread(pcm->data(),1,s.nSize,rawFile)!=s.nSize)return {};
 for(auto &sample:*pcm) sample=LittleI16(sample);
 { Lock lock;
  while(cacheBytes+s.nSize>CACHE_LIMIT) {
   uint32 oldest=UINT32_MAX,chosen=UINT32_MAX;
   for(uint32 i=0;i<TOTAL_AUDIO_SAMPLES;++i) if(cache[i] && cache[i].use_count()==1 && stamps[i]<oldest) {oldest=stamps[i];chosen=i;}
   if(chosen==UINT32_MAX)break;
   cacheBytes-=cache[chosen]->size()*2;cache[chosen].reset();
  }
  cache[id]=pcm;cacheBytes+=s.nSize;stamps[id]=++stamp;
 }
 return pcm;
}
static void gains(Channel &c,float &l,float &r) {
 float v=c.volume/127.0f*fxMaster/127.0f*fxFade/127.0f,p=c.pan<=63?(int(c.pan)-63)/63.0f:(int(c.pan)-63)/64.0f;
 if(c.is3D) {float d=std::sqrt(c.x*c.x+c.y*c.y+c.z*c.z);float ref=c.minDist>0?c.minDist:1;
  float dist=d<ref?ref:d;if(c.maxDist>ref && dist>c.maxDist)dist=c.maxDist;v*=ref/dist;p=d>1.0e-5f?c.x/d:0;
 }
 if(!std::isfinite(p) || !std::isfinite(v)) {l=r=0;return;}
 if(p< -1)p=-1;if(p>1)p=1;if(mono)p=0;
 l=std::sqrt(.5f*(1-p))*v/32768.0f;r=std::sqrt(.5f*(1+p))*v/32768.0f;
}
static void mix(float *out) {
 memset(out,0,6*256*sizeof(float));
 if(released)return;
 for(auto &c:channels) {
  if(!c.playing || !c.pcm || !c.rate)continue;
  float gl,gr;gains(c,gl,gr);double step=c.rate/48000.0;uint32 len=c.pcm->size();
  if(step<=0 || step>16 || !std::isfinite(c.pos)){c.playing=false;continue;}
  for(unsigned i=0;i<256;++i) {
   uint32 end=c.loopEnd>0 && uint32(c.loopEnd)<len?c.loopEnd:len;
   bool looping=c.loops==0 || c.played+1<c.loops;if(!looping)end=len;
   while(c.pos>=end && looping && end>c.loopStart) {c.pos-=end-c.loopStart;++c.played;looping=c.loops==0 || c.played+1<c.loops;if(!looping)end=len;}
   if(c.pos>=len || c.pos<0 || (c.pos>=end && !looping)){c.playing=false;break;}
   uint32 n=uint32(c.pos),next=n+1<len?n+1:n;float t=float(c.pos-n);
   float sample=(*c.pcm)[n]+((*c.pcm)[next]-(*c.pcm)[n])*t;
   out[i]+=sample*gl;out[256+i]+=sample*gr;c.pos+=step;
  }
 }
 if(streamPlaying && !streamPaused) {
  float volume=streamVol/127.0f*(streamFX?fxMaster:musicMaster)/127.0f*(streamFX?fxFade:musicFade)/127.0f/32768.0f;
  float gl=streamPan<=63?1.0f:(127-streamPan)/64.0f,gr=streamPan>=63?1.0f:streamPan/63.0f;
  for(unsigned i=0;i<256;++i) {
   uint32 avail=head-tail;if(!avail || (avail<2 && !streamEOF)){if(streamEOF && !avail)streamPlaying=false;break;}
   uint32 at=(tail&(RING-1))*2,next=avail>1?((tail+1)&(RING-1))*2:at;float t=float(streamFrac);
   float l=streamRing[at]+(streamRing[next]-streamRing[at])*t,r=streamRing[at+1]+(streamRing[next+1]-streamRing[at+1])*t;
   if(mono)l=r=(l+r)*.5f;out[i]+=l*volume*gl;out[256+i]+=r*volume*gr;
   streamFrac+=32000.0/48000;uint32 adv=uint32(streamFrac);streamFrac-=adv;tail+=adv;streamConsumed+=adv;
  }
 }
 for(unsigned i=0;i<512;++i)out[i]=clamp(out[i]);
}
static void *audioMain(void*) {
 FILE *capture=nullptr; uint32 captured=0; // Optional native-output capture, no effect on normal runs.
 if(access("game:\\audio-capture.txt",F_OK)==0) {capture=fopen("game:\\audio-output.pcm","wb");}
 while(true) {
  {Lock lock;if(!running)break;}
  float *frame=nullptr;if(!lcs_sound_next(&frame))continue;
  {Lock lock;mix(frame);++mixedFrames;bool nonzero=false;for(unsigned i=0;i<512;++i)nonzero|=frame[i]!=0;if(nonzero)++nonzeroFrames;}
  lcs_sound_queue();
  if(capture && captured<48000*15) {
   unsigned char pcm[1024];for(unsigned i=0;i<256;++i)for(unsigned ch=0;ch<2;++ch){int16 s=int16(clamp(frame[ch*256+i])*32767);unsigned a=(i*2+ch)*2;pcm[a]=uint16(s)&255;pcm[a+1]=uint16(s)>>8;}
   fwrite(pcm,1,sizeof pcm,capture);captured+=256;
   if(captured>=48000*15){fclose(capture);capture=nullptr;}
  }
 }
 if(capture)fclose(capture);return nullptr;
}
static void *decodeMain(void*) {
 std::unique_ptr<VbDecoder> decoder;uint32 version=UINT32_MAX;
 int16 pcm[28*64*2];
 while(true) {
  uint32 requested,id,start,free;bool loop;
  {Lock lock;if(!running)break;requested=streamVersion;id=streamId;start=streamStart;free=RING-(head-tail);loop=streamLoop;}
  if(requested!=version) {
   version=requested;decoder.reset();
   if(id<TOTAL_STREAMED_SOUNDS) {char path[128];snprintf(path,sizeof path,"game:\\data\\%s.VB",StreamedNameTable[id]);decoder.reset(new VbDecoder(path,32000));if(decoder->IsOpened())decoder->Seek(start);else {xsdk_log_write("LCS audio: cannot open %s",path);decoder.reset();}}
  }
  if(!decoder || free<28*64) {audioSleep(2000);continue;}
  uint32 got=decoder->Decode(pcm,28*64);
  if(!got && loop){decoder->Seek(0);got=decoder->Decode(pcm,28*64);}
  {Lock lock;if(version==streamVersion) {
   for(uint32 i=0;i<got;++i) {uint32 at=((head+i)&(RING-1))*2;streamRing[at]=pcm[i*2];streamRing[at+1]=pcm[i*2+1];}
   head+=got;streamEOF=got==0;
  }}
  if(!got)audioSleep(2000);
 }
 return nullptr;
}
static void requestStream(uint32 id,uint32 pos,bool play) {
 Lock lock;streamId=id;streamStart=pos;streamConsumed=0;head=tail=0;streamFrac=0;streamEOF=false;streamPlaying=play;streamPaused=false;++streamVersion;if(play)++streamStarts;
}
} // namespace
cSampleManager::cSampleManager() {m_nEffectsVolume=m_nMusicVolume=m_nEffectsFadeVolume=m_nMusicFadeVolume=127;m_nMP3BoostVolume=64;m_bInitialised=FALSE;}
cSampleManager::~cSampleManager() {} // AudioManager owns explicit shutdown before static destruction.
bool8 cSampleManager::Initialise() {
 if(m_bInitialised)return TRUE;
 SetUpDebugBanksInfo();BankStartOffset[0]=0;BankStartOffset[SFX_BANK_PED_COMMENTS]=SAMPLEBANK_MAX;
 for(auto &v:missionSamples)v=UINT32_MAX;for(auto &v:pedSamples)v=UINT32_MAX;
 if(!InitialiseSampleBanks() || !lcs_sound_open()) {xsdk_log_write("LCS audio: initialization failed");if(rawFile){fclose(rawFile);rawFile=nullptr;}return FALSE;}
 // Warm common effects during loading, before the mixer starts. Keep the
 // existing 32 MiB LRU; cap startup prewarming at 16 MiB, no worker races.
 unsigned warmed=0;
 for(uint32 id=0;id<SAMPLEBANK_MAX;++id) {
  if(m_aSamples[id].nSize>16u*1024u*1024u-cacheBytes)continue;
  if(loadSample(id,m_aSamples))++warmed;
 }
 xsdk_log_write("LCS AUDIO prewarm common=%u bytes=%u budget=16777216",warmed,cacheBytes);
 running=true;
 if(pthread_create(&audioThread,nullptr,audioMain,nullptr)){running=false;lcs_sound_close();fclose(rawFile);rawFile=nullptr;return FALSE;}audioCreated=true;
 if(pthread_create(&decodeThread,nullptr,decodeMain,nullptr)){ {Lock lock;running=false;}lcs_sound_close();pthread_join(audioThread,nullptr);audioCreated=false;fclose(rawFile);rawFile=nullptr;return FALSE;}decodeCreated=true;
 m_bInitialised=_bSampmanInitialised=TRUE;xsdk_log_write("LCS audio: native XSDK output, PCM SFX, stereo VAG stream, 48000 Hz");return TRUE;
}
void cSampleManager::Terminate() {
 if(!m_bInitialised)return;
 {Lock lock;running=false;}lcs_sound_close();if(audioCreated)pthread_join(audioThread,nullptr);if(decodeCreated)pthread_join(decodeThread,nullptr);audioCreated=decodeCreated=false;
 if(rawFile)fclose(rawFile);rawFile=nullptr;for(auto &c:channels)c=Channel{};for(auto &p:cache)p.reset();cacheBytes=0;m_bInitialised=_bSampmanInitialised=FALSE;
}
bool8 cSampleManager::InitialiseSampleBanks() {
 FILE *desc=fcaseopen("game:\\data\\audio\\sfx.sdt","rb");rawFile=fcaseopen("game:\\data\\audio\\sfx.raw","rb");if(!desc || !rawFile){if(desc)fclose(desc);return FALSE;}
 bool ok=fread(m_aSamples,sizeof(tSample),TOTAL_AUDIO_SAMPLES,desc)==TOTAL_AUDIO_SAMPLES;fclose(desc);if(!ok)return FALSE;
 fseek(rawFile,0,SEEK_END);rawSize=ftell(rawFile);rewind(rawFile);
 for(auto &s:m_aSamples){s.nOffset=LittleU32(s.nOffset);s.nSize=LittleU32(s.nSize);s.nFrequency=LittleU32(s.nFrequency);s.nLoopStart=LittleU32(s.nLoopStart);s.nLoopEnd=LittleI32(s.nLoopEnd);if(s.nOffset>rawSize || s.nSize>rawSize-s.nOffset)return FALSE;}
 for(uint32 i=0;i<TOTAL_STREAMED_SOUNDS;++i){char path[128];snprintf(path,sizeof path,"game:\\data\\%s.VB",StreamedNameTable[i]);FILE*f=fcaseopen(path,"rb");if(f){fseek(f,0,SEEK_END);uint32 blocks=ftell(f)/16384;streamLengths[i]=uint32(uint64(blocks)*14336*1000/32000);fclose(f);}}
 return TRUE;
}
bool8 cSampleManager::CheckForAnAudioFileOnCD(){return TRUE;} char cSampleManager::GetCDAudioDriveLetter(){return 0;}
bool8 cSampleManager::IsMP3RadioChannelAvailable(){return FALSE;}
void cSampleManager::ReleaseDigitalHandle(){Lock l;released=true;} void cSampleManager::ReacquireDigitalHandle(){Lock l;released=false;}
void cSampleManager::UpdateEffectsVolume(){}
void cSampleManager::SetEffectsMasterVolume(uint8 v){Lock l;fxMaster=m_nEffectsVolume=cap(v);} void cSampleManager::SetMusicMasterVolume(uint8 v){Lock l;musicMaster=m_nMusicVolume=cap(v);}
void cSampleManager::SetEffectsFadeVolume(uint8 v){Lock l;fxFade=m_nEffectsFadeVolume=cap(v);} void cSampleManager::SetMusicFadeVolume(uint8 v){Lock l;musicFade=m_nMusicFadeVolume=cap(v);}
void cSampleManager::SetMP3BoostVolume(uint8 v){m_nMP3BoostVolume=v;} void cSampleManager::SetMonoMode(bool8 v){Lock l;mono=m_nMonoMode=v;}
bool8 cSampleManager::LoadSampleBank(uint8 b){if(b>=MAX_SFX_BANKS)return FALSE;bankLoaded[b]=true;return TRUE;}
void cSampleManager::UnloadSampleBank(uint8 b){if(b<MAX_SFX_BANKS)bankLoaded[b]=false;}
int8 cSampleManager::IsSampleBankLoaded(uint8 b){return b<MAX_SFX_BANKS && bankLoaded[b]?LOADING_STATUS_LOADED:LOADING_STATUS_NOT_LOADED;}
uint8 cSampleManager::IsMissionAudioLoaded(uint8 slot,uint32 id){return slot<8 && missionSamples[slot]==id?LOADING_STATUS_LOADED:LOADING_STATUS_NOT_LOADED;}
bool8 cSampleManager::LoadMissionAudio(uint8 slot,uint32 id){if(slot>=8 || !loadSample(id,m_aSamples))return FALSE;missionSamples[slot]=id;return TRUE;}
uint8 cSampleManager::IsPedCommentLoaded(uint32 id){return _GetPedCommentSlot(id)>=0?LOADING_STATUS_LOADED:LOADING_STATUS_NOT_LOADED;}
bool8 cSampleManager::LoadPedComment(uint32 id){if(!loadSample(id,m_aSamples))return FALSE;pedSamples[pedNext]=id;pedNext=(pedNext+1)%MAX_PEDSFX;return TRUE;}
int32 cSampleManager::_GetPedCommentSlot(uint32 id){for(uint32 i=0;i<MAX_PEDSFX;++i)if(pedSamples[i]==id)return i;return -1;}
int32 cSampleManager::GetBankContainingSound(uint32 id){return id>=TOTAL_AUDIO_SAMPLES?INVALID_SFX_BANK:id>=SAMPLEBANK_MAX?SFX_BANK_PED_COMMENTS:SFX_BANK_0;}
uint32 cSampleManager::GetSampleBaseFrequency(uint32 id){return id<TOTAL_AUDIO_SAMPLES?m_aSamples[id].nFrequency:0;}
uint32 cSampleManager::GetSampleLoopStartOffset(uint32 id){return id<TOTAL_AUDIO_SAMPLES?m_aSamples[id].nLoopStart:0;}
int32 cSampleManager::GetSampleLoopEndOffset(uint32 id){return id<TOTAL_AUDIO_SAMPLES?m_aSamples[id].nLoopEnd:-1;}
uint32 cSampleManager::GetSampleLength(uint32 id){return id<TOTAL_AUDIO_SAMPLES?m_aSamples[id].nSize/2:0;}
bool8 cSampleManager::UpdateReverb(){return FALSE;} void cSampleManager::SetChannelReverbFlag(uint32,bool8){} // No advertised reverb provider.
bool8 cSampleManager::InitialiseChannel(uint32 n,uint32 id,uint8 bank){if(!valid(n) || bank>=MAX_SFX_BANKS)return FALSE;auto data=loadSample(id,m_aSamples);if(!data)return FALSE;Lock l;channels[n]=Channel{};channels[n].pcm=data;channels[n].rate=m_aSamples[id].nFrequency;channels[n].is3D=n<MAXCHANNELS;return TRUE;}
void cSampleManager::SetChannelEmittingVolume(uint32 n,uint32 v){if(valid(n)){Lock l;channels[n].volume=cap(v);}}
void cSampleManager::SetChannelVolume(uint32 n,uint32 v){SetChannelEmittingVolume(n,v);}
void cSampleManager::SetChannel3DPosition(uint32 n,float x,float y,float z){if(valid(n)){Lock l;channels[n].x=-x;channels[n].y=y;channels[n].z=z;}}
void cSampleManager::SetChannel3DDistances(uint32 n,float max,float min){if(valid(n)){Lock l;channels[n].minDist=min;channels[n].maxDist=max;}}
void cSampleManager::SetChannelPan(uint32 n,uint32 p){if(valid(n)){Lock l;channels[n].pan=cap(p);}}
void cSampleManager::SetChannelFrequency(uint32 n,uint32 f){if(valid(n)){Lock l;channels[n].rate=f;}}
void cSampleManager::SetChannelLoopPoints(uint32 n,uint32 start,int32 end){if(valid(n)){Lock l;channels[n].loopStart=start/2;channels[n].loopEnd=end<0?-1:end/2;}}
void cSampleManager::SetChannelLoopCount(uint32 n,uint32 count){if(valid(n)){Lock l;channels[n].loops=count;}}
bool8 cSampleManager::GetChannelUsedFlag(uint32 n){if(!valid(n))return FALSE;Lock l;return channels[n].playing;}
void cSampleManager::StartChannel(uint32 n){if(valid(n)){Lock l;channels[n].pos=0;channels[n].played=0;channels[n].playing=bool(channels[n].pcm);++sfxStarts;}}
void cSampleManager::StopChannel(uint32 n){if(valid(n)){Lock l;channels[n].playing=false;}}
void cSampleManager::PreloadStreamedFile(uint32 id,uint8 n){if(n<MAX_STREAMS)requestStream(id,0,false);}
void cSampleManager::StartPreloadedStreamedFile(uint8 n){if(n<MAX_STREAMS){Lock l;streamPlaying=true;++streamStarts;}}
bool8 cSampleManager::StartStreamedFile(uint32 id,uint32 pos,uint8 n){if(n>=MAX_STREAMS || id>=TOTAL_STREAMED_SOUNDS || !streamLengths[id])return FALSE;requestStream(id,pos,true);return TRUE;}
void cSampleManager::StopStreamedFile(uint8 n){if(n<MAX_STREAMS)requestStream(UINT32_MAX,0,false);}
void cSampleManager::PauseStream(bool8 paused,uint8 n){if(n<MAX_STREAMS){Lock l;streamPaused=paused;}}
int32 cSampleManager::GetStreamedFilePosition(uint8 n){if(n>=MAX_STREAMS)return 0;Lock l;uint32 ms=streamStart+uint32(uint64(streamConsumed)*1000/32000);return streamId<TOTAL_STREAMED_SOUNDS && streamLengths[streamId]?ms%streamLengths[streamId]:0;}
int32 cSampleManager::GetStreamedFileLength(uint8 id){return id<TOTAL_STREAMED_SOUNDS?streamLengths[id]:0;}
bool8 cSampleManager::IsStreamPlaying(uint8 n){if(n>=MAX_STREAMS)return FALSE;Lock l;return streamPlaying && !streamPaused;}
void cSampleManager::SetStreamedVolumeAndPan(uint8 v,uint8 pan,bool8 effect,uint8 n){if(n<MAX_STREAMS){Lock l;streamVol=cap(v);streamPan=cap(pan);streamFX=effect;}}
void cSampleManager::SetStreamedFileLoopFlag(bool8 v,uint8 n){if(n<MAX_STREAMS){Lock l;streamLoop=v;}}
void cSampleManager::SetSpeakerConfig(int32){} uint32 cSampleManager::GetMaximumSupportedChannels(){return MAXCHANNELS;}
uint32 cSampleManager::GetNum3DProvidersAvailable(){return 1;} void cSampleManager::SetNum3DProvidersAvailable(uint32){}
char *cSampleManager::Get3DProviderName(uint8){static char name[]="XSDK native stereo";return name;}
void cSampleManager::Set3DProviderName(uint8,char*){} int8 cSampleManager::GetCurrent3DProviderIndex(){return 0;} int8 cSampleManager::SetCurrent3DProvider(uint8){return 0;} int8 cSampleManager::AutoDetect3DProviders(){return 0;}
extern "C" void lcs_audio_report(){unsigned played=0,silent=0;lcs_sound_stats(&played,&silent);Lock l;xsdk_log_write("LCS AUDIO frames=%u nonzero=%u sfx=%u streams=%u played=%u silent=%u cache=%u",mixedFrames,nonzeroFrames,sfxStarts,streamStarts,played,silent,cacheBytes);}
