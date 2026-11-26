#include "common.h"
#include "crossplatform.h"
#include "platform.h"
#include "FileMgr.h"
#include "PCSave.h"
#include "Frontend.h"
#include "Game.h"
#include "Camera.h"
#include "Pad.h"
#include "ControllerConfig.h"
#include "Text.h"
#include "xsdk_paths.h"
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
extern "C" void xsdk_log_write(const char*,...);

long _dwOperatingSystemVersion=OS_WINXP;
// Conservative compatibility budget, not a measurement of free physical RAM.
size_t _dwMemAvailPhys=256u*1024u*1024u;
RwUInt32 gGameState=GS_START_UP;
static psGlobalType ps;
extern "C" double psTimer() { timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return double(t.tv_sec)*1000.0+double(t.tv_nsec)/1000000.0; }
RwBool IsForegroundApp() { return TRUE; }
extern "C" RwBool psInitialize() {
 memset(&ps,0,sizeof ps); ps.fullScreen=TRUE; ps.joy1id=0; ps.joy2id=-1; RsGlobal.ps=&ps;
 RsGlobal.maximumWidth=RsGlobal.width=1280; RsGlobal.maximumHeight=RsGlobal.height=720; RsGlobal.maxFPS=30;
 if(mkdir(_psGetUserFilesFolder(),0777) && errno!=EEXIST) return FALSE;
 CFileMgr::Initialise(); C_PcSave::SetSaveDirectory(_psGetUserFilesFolder()); FrontEndMenuManager.m_PrefsLanguage=CMenuManager::LANGUAGE_AMERICAN;
 CCamera::m_bUseMouse3rdPerson=false; gGameState=GS_START_UP;
 return TRUE;
}
extern "C" void psTerminate() { RsGlobal.ps=nil; }
extern "C" RwMemoryFunctions *psGetMemoryFunctions() { return nil; }
extern "C" RwBool psInstallFileSystem() { return TRUE; }
extern "C" RwBool psNativeTextureSupport() { return TRUE; }
extern "C" RwBool psCameraBeginUpdate(RwCamera *c) { return c && RwCameraBeginUpdate(c)!=nil; }
extern "C" void psCameraShowRaster(RwCamera *c) { if(c) RwCameraShowRaster(c,nil,rwRASTERFLIPWAITVSYNC); }
extern "C" RwImage *psGrabScreen(RwCamera*) { xsdk_log_write("LCS boot: in-game screenshot API pending"); return nil; }
extern "C" void psMouseSetPos(RwV2d *p) { if(p) ps.lastMousePos=*p; }
extern "C" long _InputInitialiseMouse(bool) { return 0; }
extern "C" void _InputShutdownMouse() {}
extern "C" bool _InputMouseNeedsExclusive() { return false; }
extern "C" void _InputTranslateShiftKeyUpDown(RsKeyCodes*) {}
extern "C" void _InputInitialiseJoys() { ControlsManager.InitDefaultControlConfigJoyPad(16); }
extern "C" void HandleExit() {}
extern "C" void InitialiseLanguage() { FrontEndMenuManager.m_PrefsLanguage=CMenuManager::LANGUAGE_AMERICAN; TheText.Load(); }
extern "C" RwInt32 _psGetNumVideModes() { return 1; }
extern "C" RwChar **_psGetVideoModeList() { static char text[]="1280 X 720 X 32"; static char *modes[]={text}; return modes; }
extern "C" RwBool _psSetVideoMode(RwInt32 subsystem,RwInt32 mode) {
 if(subsystem!=0 || mode!=0 || !RwEngineSetSubSystem(0) || !RwEngineSetVideoMode(0)) return FALSE;
 RsGlobal.maximumWidth=RsGlobal.width=1280; RsGlobal.maximumHeight=RsGlobal.height=720;
 FrontEndMenuManager.m_nPrefsWidth=1280; FrontEndMenuManager.m_nPrefsHeight=720; FrontEndMenuManager.m_nPrefsDepth=32;
 return TRUE;
}
extern "C" RwBool psSelectDevice() { return _psSetVideoMode(0,0); }
extern "C" void _psSelectScreenVM(RwInt32 mode) { if(mode==0) _psSetVideoMode(0,0); }
void FlushLog() { fflush(stdout); fflush(stderr); }

void CapturePad(RwInt32 id) {
 if(id!=0) return;
 CPad *pad=CPad::GetPad(CPad::m_bMapPadOneToPadTwo?1:0);
 pad->PCTempJoyState.Clear(); GLFWgamepadstate s={};
 if(!glfwGetGamepadState(0,&s)) return;
 CControllerState &o=pad->PCTempJoyState;
 o.Cross=s.buttons[0]?255:0; o.Circle=s.buttons[1]?255:0; o.Square=s.buttons[2]?255:0; o.Triangle=s.buttons[3]?255:0;
 o.LeftShoulder1=s.buttons[4]?255:0; o.RightShoulder1=s.buttons[5]?255:0; o.Select=s.buttons[6]?255:0; o.Start=s.buttons[7]?255:0;
 o.LeftShock=s.buttons[9]?255:0; o.RightShock=s.buttons[10]?255:0;
 o.DPadUp=s.buttons[11]?255:0; o.DPadRight=s.buttons[12]?255:0; o.DPadDown=s.buttons[13]?255:0; o.DPadLeft=s.buttons[14]?255:0;
 o.LeftShoulder2=int((s.axes[4]+1)*127.5f); o.RightShoulder2=int((s.axes[5]+1)*127.5f);
 if(Abs(s.axes[0])>.3f || Abs(s.axes[1])>.3f) { o.LeftStickX=int(s.axes[0]*128); o.LeftStickY=int(s.axes[1]*128); }
 if(Abs(s.axes[2])>.3f || Abs(s.axes[3])>.3f) { o.RightStickX=int(s.axes[2]*128); o.RightStickY=int(s.axes[3]*128); }
}
