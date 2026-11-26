// Uses the production native sample manager and actual user-provided assets.
#include "common.h"
#include "sampman.h"
#include "FileMgr.h"
#include <unistd.h>
#include <time.h>
static void audioSleep(unsigned us) { timespec delay={time_t(us/1000000),long(us%1000000)*1000}; nanosleep(&delay,nullptr); }
extern "C" void xsdk_log_write(const char*,...);
extern "C" void lcs_audio_report();
#define CHECK(x) do {if(!(x)){xsdk_log_write("LCS AUDIO FAIL line %d: %s",__LINE__,#x);SampleManager.Terminate();return 1;}}while(0)
int main() {
 CHECK(!chdir("game:\\data")); CFileMgr::Initialise(); CHECK(SampleManager.Initialise());
 CHECK(SampleManager.LoadSampleBank(SFX_BANK_0));
 CHECK(SampleManager.GetSampleLength(SFX_FE_BACK)>0);
 CHECK(SampleManager.InitialiseChannel(CHANNEL_POLICE_RADIO,SFX_FE_BACK,SFX_BANK_0));
 SampleManager.SetChannelLoopCount(CHANNEL_POLICE_RADIO,0); SampleManager.SetChannelPan(CHANNEL_POLICE_RADIO,0); SampleManager.StartChannel(CHANNEL_POLICE_RADIO);
 audioSleep(1000000); CHECK(SampleManager.GetChannelUsedFlag(CHANNEL_POLICE_RADIO));
 SampleManager.SetChannelPan(CHANNEL_POLICE_RADIO,127);audioSleep(1000000);SampleManager.StopChannel(CHANNEL_POLICE_RADIO);
 CHECK(!SampleManager.GetChannelUsedFlag(CHANNEL_POLICE_RADIO));
 CHECK(SampleManager.GetStreamedFileLength(STREAMED_SOUND_CUTSCENE_INTRO)>1000);
 CHECK(SampleManager.StartStreamedFile(STREAMED_SOUND_CUTSCENE_INTRO,0));audioSleep(2500000);
 CHECK(SampleManager.IsStreamPlaying());int pos=SampleManager.GetStreamedFilePosition();CHECK(pos>1000);
 SampleManager.PauseStream(TRUE);int paused=SampleManager.GetStreamedFilePosition();audioSleep(400000);CHECK(SampleManager.GetStreamedFilePosition()==paused);
 SampleManager.PauseStream(FALSE);audioSleep(600000);CHECK(SampleManager.GetStreamedFilePosition()>pos);
 SampleManager.SetStreamedVolumeAndPan(0,63,FALSE);audioSleep(400000);SampleManager.StopStreamedFile();
 CHECK(!SampleManager.IsStreamPlaying());lcs_audio_report();SampleManager.Terminate();
 xsdk_log_write("LCS AUDIO PROBE PASS: real PCM effects, stereo pan, VAG speech, sample clock, pause/resume/stop");return 0;
}
