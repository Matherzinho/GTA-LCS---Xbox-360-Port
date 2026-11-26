// Real AppEventHandler/CGame startup, kept separate from all diagnostic probes.
#include "common.h"
#include "crossplatform.h"
#include "platform.h"
#include "main.h"
#include "Game.h"
#include "GenericGameStorage.h"
#include "xsdk_gameplay_test.h"
#include "xsdk_safehouse_test.h"
#include "Frontend.h"
#include "ControllerConfig.h"
#include "FileMgr.h"
#include "Text.h"
#include "Timer.h"
#include "Camera.h"
#include "Script.h"
#include "gpu/xdev.h"
#include "rw_xsdk_backend.h"
#include <unistd.h>
#include <time.h>
#include <malloc.h>
extern bool b_FoundRecentSavedGameWantToLoad;
extern "C" void lcs_audio_report();
extern "C" void lcs_stream_report();
extern "C" void lcs_stress_init();
extern "C" void lcs_video_init();
extern "C" void lcs_video_before();
extern "C" void lcs_video_after(double);
extern "C" void lcs_stress_step();
extern "C" void lcs_stress_sample(double);
extern "C" void xsdk_log_write(const char*,...);
// Bank mapping is owned by the native sample manager.

static double monotonicMs() {
 timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
 return double(t.tv_sec)*1000.0+double(t.tv_nsec)/1000000.0;
}
int main() {
 xsdk_log_write("LCS BOOT: native XSDK, real game flow, native audio, basic renderer; internal build");
 if(chdir("game:\\data")) return 1;
 xsdk_log_write("LCS BOOT stage 1: rsINITIALIZE");
 if(RsEventHandler(rsINITIALIZE,nil)==rsEVENTERROR) return 2;
 xsdk_log_write("LCS BOOT stage 2: rsRWINITIALIZE");
 rw::EngineOpenParams open={1280,720};
 if(RsEventHandler(rsRWINITIALIZE,&open)==rsEVENTERROR) return 3;
 RwRect r={0,0,1280,720}; RsEventHandler(rsCAMERASIZE,&r);
 _InputInitialiseJoys();
 xsdk_log_write("LCS BOOT stage 3: InitialiseOnceAfterRW");
 if(!CGame::InitialiseOnceAfterRW() || !TheText.IsLoaded()) return 4;
 xsdk_log_write("LCS BOOT stage 4: frontend requested");
 FrontEndMenuManager.m_bGameNotLoaded=true; FrontEndMenuManager.m_bStartUpFrontEndRequested=true;
 gGameState=GS_FRONTEND;
 lcs_gameplay_test_init();lcs_stress_init();lcs_video_init();
 unsigned menuFrames=0,gameFrames=0;
 bool autostart=access("game:\\boot-city.txt",F_OK)==0;
 bool diagnostics=access("game:\\boot-diagnostics.txt",F_OK)==0;
 bool tracePose=access("game:\\boot-pose-trace.txt",F_OK)==0;
 bool gameplayFixture=access("game:\\gameplay-test.txt",F_OK)==0;
 bool profilePerformance=access("game:\\performance-log.txt",F_OK)==0;
 unsigned perfFrames=0,perfOverBudget=0; double perfTotal=0,perfMax=0;
 while(!RsGlobal.quit) {
  const double frameStart=monotonicMs();
  if(gGameState==GS_FRONTEND) {
   lcs_gameplay_test_before();
   RsEventHandler(rsFRONTENDIDLE,nil);
   if(lcs_safehouse_test_active())lcs_gameplay_test_after();
   if(++menuFrames==30 && diagnostics) { xdev_capture_ppm("game:\\boot-menu.ppm"); xsdk_log_write("LCS BOOT MENU: 30 frontend frames completed, menuActive=%d",FrontEndMenuManager.m_bMenuActive); }
   if(autostart && menuFrames==120) FrontEndMenuManager.DoSettingsBeforeStartingAGame();
   if(!FrontEndMenuManager.m_bMenuActive) {
    FrontEndMenuManager.m_bMenuActive=false; FrontEndMenuManager.m_bWantToRestart=false; FrontEndMenuManager.m_bStartUpFrontEndRequested=false;
    xsdk_log_write("LCS BOOT stage 5: InitialiseGame (autostart=%d)",autostart);
    gGameState=GS_INIT_PLAYING_GAME;
    // Establish the native world first, then perform the same validated reload
    // used in-game. Previously Load Game from the title menu started a new game.
    bool loadRequested=FrontEndMenuManager.m_bWantToLoad || b_FoundRecentSavedGameWantToLoad;
    FrontEndMenuManager.m_bWantToLoad=false;
    InitialiseGame(); FrontEndMenuManager.m_bGameNotLoaded=false; gGameState=GS_PLAYING_GAME;
    if(loadRequested){FrontEndMenuManager.m_bWantToLoad=true;FrontEndMenuManager.m_bWantToRestart=true;}
    xsdk_log_write("LCS BOOT stage 6: city initialization returned");
   }
  } else {
   if(gameFrames==30) { // Optional load-only regression, disabled in normal runs.
    FILE *f=fopen("game:\\reload-test.txt","r");
    if(f){char path[256]={};if(fgets(path,sizeof path,f)){path[strcspn(path,"\r\n")]=0;strcpy(LoadFileName,path);FrontEndMenuManager.m_bWantToLoad=true;FrontEndMenuManager.m_bWantToRestart=true;}fclose(f);}
   }
   if(FrontEndMenuManager.m_bWantToRestart || b_FoundRecentSavedGameWantToLoad) {
    if(FrontEndMenuManager.m_bWantToLoad || b_FoundRecentSavedGameWantToLoad) {
     FrontEndMenuManager.m_bWantToLoad=true;CTimer::Stop();
     xsdk_log_write("LCS BOOT: normal saved-game restart");
     CGame::ShutDownForRestart();CGame::InitialiseWhenRestarting();
     bool loaded=PcSaveHelper.nErrorCode==SAVESTATUS_SUCCESSFUL;
     FrontEndMenuManager.m_bWantToRestart=false;b_FoundRecentSavedGameWantToLoad=false;
     lcs_gameplay_test_reloaded(loaded);xsdk_log_write("LCS BOOT: saved-game restart returned loaded=%d",loaded);
    } else {
     CTimer::Stop();CGame::ShutDown();FrontEndMenuManager.m_bWantToRestart=false;
     gGameState=GS_INIT_PLAYING_GAME;InitialiseGame();gGameState=GS_PLAYING_GAME;
    }
   }
   lcs_gameplay_test_before();
   lcs_stress_step();lcs_video_before();
   RsEventHandler(rsIDLE,lcs_safehouse_test_render()?(void*)TRUE:nil);
   lcs_gameplay_test_after();
   if(++gameFrames==30 && diagnostics) { xdev_capture_ppm("game:\\boot-city.ppm"); xsdk_log_write("LCS BOOT CITY: 30 game frames completed"); }
   if(diagnostics && (gameFrames==30 || gameFrames%120==0)) { lcs_audio_report(); xsdk_log_write("LCS STATE frames=%u time=%u pause=%d/%d menu=%d restart=%d foundload=%d fade=%d camera=%f,%f,%f",gameFrames,CTimer::GetTimeInMilliseconds(),CTimer::GetIsUserPaused(),CTimer::GetIsCodePaused(),FrontEndMenuManager.m_bMenuActive,FrontEndMenuManager.m_bWantToRestart,b_FoundRecentSavedGameWantToLoad,TheCamera.GetScreenFadeStatus(),TheCamera.GetPosition().x,TheCamera.GetPosition().y,TheCamera.GetPosition().z); xdev_capture_ppm("game:\\boot-city-latest.ppm"); if(gameFrames<=720 && gameFrames%120==0) { char path[96]; snprintf(path,sizeof path,"game:\\boot-frame-%05u.ppm",gameFrames); xdev_capture_ppm(path); snprintf(path,sizeof path,"game:\\skin-pose-%05u.json",gameFrames); if(tracePose) rw::xsdk::dumpSkinPose(path); } }
  }
  if(access("game:\\boot-stop.txt",F_OK)==0) RsGlobal.quit=true;
  const double workMs=monotonicMs()-frameStart;
  if(gGameState==GS_PLAYING_GAME){lcs_stress_sample(workMs);lcs_video_after(workMs);}
  if(profilePerformance && gGameState==GS_PLAYING_GAME) {
   ++perfFrames; perfTotal+=workMs; if(workMs>perfMax)perfMax=workMs;
   if(workMs>1000.0/30.0)++perfOverBudget;
   if(perfFrames==300) {
    xsdk_log_write("LCS PERF frames=%u work_avg_ms=%.3f work_max_ms=%.3f over_33ms=%u diagnostics=%d test=%d cap=%u fxaa=%d",perfFrames,perfTotal/perfFrames,perfMax,perfOverBudget,diagnostics,lcs_safehouse_test_active(),FrontEndMenuManager.m_PrefsFrameLimiter?30u:60u,xdev_fxaa_enabled);
    xdev_perf_report();
    lcs_stream_report();
    struct mallinfo heap=mallinfo();
    xsdk_log_write("LCS PERF heap_used_kib=%u heap_arena_kib=%u heap_free_kib=%u",unsigned(heap.uordblks/1024),unsigned(heap.arena/1024),unsigned(heap.fordblks/1024));
    perfFrames=perfOverBudget=0;perfTotal=perfMax=0;
   }
  }
  // RsGlobal.maxFPS alone does not throttle this native event loop. Keep
  // ordinary gameplay at the selected 30/60 Hz cap; fixtures retain their cadence.
  const bool testing=lcs_safehouse_test_active() || gameplayFixture;
  const unsigned cap=FrontEndMenuManager.m_PrefsFrameLimiter?30:60;
  RsGlobal.maxFPS=cap;
  double waitMs=testing?1.0:1000.0/cap-workMs;
  if(waitMs>0) { timespec delay={0,long(waitMs*1000000.0)}; nanosleep(&delay,nil); }
 }
 RsEventHandler(rsRWTERMINATE,nil); RsEventHandler(rsTERMINATE,nil); psTerminate(); return 0;
}
