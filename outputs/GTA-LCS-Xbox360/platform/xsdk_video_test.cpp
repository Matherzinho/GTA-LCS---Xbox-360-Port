#include "common.h"
#include "Frontend.h"
#include "PlayerPed.h"
#include "World.h"
#include "Timer.h"
#include "Pad.h"
#include "CutsceneMgr.h"
#include "xsdk_input.h"
#include "gpu/xdev.h"
#include <unistd.h>
extern "C" void xsdk_log_write(const char*,...);
namespace {bool enabled,done,oldCap;int8 oldAA;unsigned phase,frame,ready;int capRow=-1,aaRow=-1;
struct Timing{unsigned n=0,over=0;double sum=0,max=0;} timing[3];
void closeMenu(){FrontEndMenuManager.m_bShutDownFrontEndRequested=true;FrontEndMenuManager.SwitchMenuOnAndOff();}
void select(int row){if(!FrontEndMenuManager.m_bMenuActive){FrontEndMenuManager.m_bStartUpFrontEndRequested=true;FrontEndMenuManager.SwitchMenuOnAndOff();}FrontEndMenuManager.SwitchToNewScreen(MENUPAGE_GRAPHICS_SETTINGS);FrontEndMenuManager.m_nCurrOption=row;}
void finish(bool ok,const char *reason){FILE*f=fopen("game:\\video-result.json","w");if(f){fprintf(f,"{\"status\":\"%s\",\"reason\":\"%s\",\"menu_cap_row\":%d,\"menu_fxaa_row\":%d,\"timings\":[",ok?"PASS":"FAIL",reason,capRow,aaRow);for(int i=0;i<3;++i){Timing&t=timing[i];fprintf(f,"%s{\"mode\":\"%s\",\"frames\":%u,\"avg_work_ms\":%.3f,\"max_work_ms\":%.3f,\"over_33ms\":%u}",i?",":"",i==0?"60_FXAA":i==1?"30_FXAA":"30_no_FXAA",t.n,t.n?t.sum/t.n:0,t.max,t.over);}fprintf(f,"]}\n");fclose(f);}FrontEndMenuManager.m_PrefsFrameLimiter=oldCap;xdev_fxaa_enabled=oldAA;FrontEndMenuManager.SaveSettings();closeMenu();lcs_test_set_pad(nullptr);done=true;xsdk_log_write("LCS VIDEO %s: %s",ok?"PASS":"FAIL",reason);}
}
extern "C" void lcs_video_init(){enabled=access("game:\\video-test.txt",F_OK)==0;xsdk_log_write("LCS VIDEO init enabled=%d",enabled);}
extern "C" void lcs_video_before(){
 if(!enabled||done)return;GLFWgamepadstate pad={};pad.axes[4]=pad.axes[5]=-1;
 if(!phase){if(!FindPlayerPed())return;if(++ready<180)return;
  oldCap=FrontEndMenuManager.m_PrefsFrameLimiter;oldAA=xdev_fxaa_enabled;
  for(int i=0;i<NUM_MENUROWS;++i){auto&e=aScreens[MENUPAGE_GRAPHICS_SETTINGS].m_aEntries[i];if(!strcmp(e.m_EntryName,"FEM_FRM"))capRow=i;if(!strcmp(e.m_EntryName,"XSFXAA"))aaRow=i;}
  if(capRow<0||aaRow<0){finish(false,"video menu options missing");return;}
  FrontEndMenuManager.m_PrefsFrameLimiter=true;xdev_fxaa_enabled=0;select(capRow);phase=1;frame=0;
 }
 if((phase==1||phase==2||phase==4||phase==6)&&frame==30)pad.buttons[12]=1;
 lcs_test_set_pad(&pad);
}
extern "C" void lcs_video_after(double work){
 if(!enabled||done||!phase)return;++frame;
 if(phase==1&&frame==60){if(FrontEndMenuManager.m_PrefsFrameLimiter){finish(false,"menu did not switch to 60 cap");return;}xdev_capture_ppm("game:\\video-menu-60.ppm");select(aaRow);phase=2;frame=0;}
 else if(phase==2&&frame==60){if(!xdev_fxaa_enabled){finish(false,"menu did not enable FXAA");return;}xdev_capture_ppm("game:\\video-menu-fxaa.ppm");closeMenu();phase=3;frame=0;}
 else if(phase==4&&frame==60){if(!FrontEndMenuManager.m_PrefsFrameLimiter){finish(false,"menu did not restore 30 cap");return;}xdev_capture_ppm("game:\\video-menu-30.ppm");closeMenu();phase=5;frame=0;}
 else if(phase==6&&frame==60){if(xdev_fxaa_enabled){finish(false,"menu did not disable FXAA");return;}closeMenu();phase=7;frame=0;}
 else if(phase==3||phase==5||phase==7){
  if(frame>60){int i=phase==3?0:phase==5?1:2;Timing&t=timing[i];++t.n;t.sum+=work;if(work>t.max)t.max=work;if(work>1000.0/30.0)++t.over;}
  if(frame==240){xdev_capture_ppm(phase==3?"game:\\video-world-60-fxaa.ppm":phase==5?"game:\\video-world-30-fxaa.ppm":"game:\\video-world-30-off.ppm");if(phase==7){finish(true,"native graphics menu toggled 30/60 cap and FXAA on/off; fully rendered gameplay continued");return;}select(phase==3?capRow:aaRow);++phase;frame=0;}
 }
}
