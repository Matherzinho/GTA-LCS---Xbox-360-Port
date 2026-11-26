// Opt-in integration test. Setup spawns one unlocked car and places Toni
// beside its driver's door. Walking, entering, driving and exiting use pad
// inputs through the production input adapter; no SetPedInCar shortcut.
#include "common.h"
#include "xsdk_input.h"
#include "xsdk_gameplay_test.h"
#include "xsdk_safehouse_test.h"
#include "Game.h"
#include "Pad.h"
#include "Camera.h"
#include "CutsceneMgr.h"
#include "PlayerPed.h"
#include "PlayerInfo.h"
#include "World.h"
#include "Automobile.h"
#include "Streaming.h"
#include "ModelIndices.h"
#include "GenericGameStorage.h"
#include "FileMgr.h"
#include "Timer.h"
#include "Script.h"
#include "Pools.h"
#include "Radar.h"
#include "Stats.h"
#include "Frontend.h"
#include "gpu/xdev.h"
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
extern "C" void xsdk_log_write(const char*,...);
static bool enabled=false,finished=false,reloaded=false;
static unsigned phase=0,frames=0,total=0,stable=0;
static CVector initial,previousWalk,carInitial,savedPosition;
static uint32 readySince=0;
static float walked=0,driven=0,savedHealth=0;
static int savedMoney=0;static CAutomobile *fixture;
static char saveDirectory[260],saveName[260];
static float distance2D(CVector a,CVector b){a-=b;return Sqrt(a.x*a.x+a.y*a.y);}
static void result(bool pass,const char *reason) {
 xsdk_log_write("LCS GAMEPLAY %s phase=%u frames=%u walk=%f drive=%f: %s",pass?"PASS":"FAIL",phase,total,walked,driven,reason);
 FILE *f=fopen("game:\\gameplay-test-result.json","w");
 if(f){char path[260];strcpy(path,saveName);for(char*p=path;*p;++p)if(*p=='\\')*p='/';
 fprintf(f,"{\"status\":\"%s\",\"phase\":%u,\"frames\":%u,\"walk_metres\":%.6f,\"drive_metres\":%.6f,\"save_file\":\"%s\",\"reason\":\"%s\",\"fixture\":\"Spawned unlocked Kuruma and positioned player at door; movement, entry, driving and exit use scripted pad inputs; save uses isolated slot 8 and normal reload lifecycle\"}\n",pass?"PASS":"FAIL",phase,total,walked,driven,path,reason);fclose(f);}
 finished=true;lcs_test_set_pad(nullptr);
}
static void next(unsigned n){phase=n;frames=0;stable=0;xsdk_log_write("LCS GAMEPLAY phase=%u",phase);}
void lcs_gameplay_test_init(){if(lcs_safehouse_test_init())return;enabled=access("game:\\gameplay-test.txt",F_OK)==0;if(enabled)xsdk_log_write("LCS GAMEPLAY: opt-in scripted controller test, isolated save directory");}
void lcs_gameplay_test_before() {
 if(lcs_safehouse_test_active()){lcs_safehouse_test_before();return;}
 if(!enabled || finished)return;
 GLFWgamepadstate p={};p.axes[4]=p.axes[5]=-1;
 if(phase==0 && CCutsceneMgr::IsCutsceneProcessing() && frames%60==1)p.buttons[0]=1;
 if(phase==0 && FindPlayerPed() && FindPlayerPed()->bInVehicle && !CPad::GetPad(0)->ArePlayerControlsDisabled() && frames%120==1)p.buttons[3]=1;
 if(phase==1)p.axes[1]=-.9f;
 if(phase==3 && frames%120==1)p.buttons[3]=1;
 if(phase==4){p.buttons[0]=1;p.axes[0]=.15f;}
 if(phase==5)p.buttons[2]=1;
 if(phase==6 && frames%120==1)p.buttons[3]=1;
 lcs_test_set_pad(&p);
}
void lcs_gameplay_test_after() {
 if(lcs_safehouse_test_active()){lcs_safehouse_test_after();return;}
 if(!enabled || finished)return;
 ++frames;++total;CPlayerPed *ped=FindPlayerPed();
 if(total%120==0 && ped)xsdk_log_write("LCS GAMEPLAY state phase=%u frame=%u pedstate=%d inCar=%d disabled=%d pos=%f,%f,%f",phase,frames,ped->m_nPedState,ped->bInVehicle,CPad::GetPad(0)->ArePlayerControlsDisabled(),ped->GetPosition().x,ped->GetPosition().y,ped->GetPosition().z);
 if(total>12000){result(false,"test timeout");return;}
 if(phase==0) {
  if(ped && !CCutsceneMgr::IsCutsceneProcessing() && !TheCamera.m_WideScreenOn && !CTimer::GetIsPaused() && !CPad::GetPad(0)->ArePlayerControlsDisabled())++stable;else stable=0;
  if(stable==1)readySince=CTimer::GetTimeInMilliseconds();
  if(stable>60 && CTimer::GetTimeInMilliseconds()>readySince+6000 && !ped->bInVehicle && !ped->EnteringCar()){initial=previousWalk=ped->GetPosition();CPad::GetPad(0)->Mode=0;next(1);}return;
 }
 if(!ped){result(false,"player missing");return;}
 if(phase==1 && (CCutsceneMgr::IsCutsceneProcessing() || TheCamera.m_WideScreenOn || CPad::GetPad(0)->ArePlayerControlsDisabled() || distance2D(previousWalk,ped->GetPosition())>2)) {xsdk_log_write("LCS GAMEPLAY: script interrupted walking; waiting for stable control again");next(0);return;}
 if(phase==1)previousWalk=ped->GetPosition();
 if(phase==1 && frames>=120){walked=distance2D(initial,ped->GetPosition());if(walked<1){result(false,"walk input did not move player");return;}xdev_capture_ppm("game:\\gameplay-walking.ppm");next(2);}
 if(phase==2) {
  CStreaming::RequestModel(MI_KURUMA,STREAMFLAGS_DONT_REMOVE);CStreaming::LoadAllRequestedModels(false);
  if(!CStreaming::HasModelLoaded(MI_KURUMA)){result(false,"Kuruma model did not load");return;}
  fixture=new CAutomobile(MI_KURUMA,MISSION_VEHICLE);CVector p=ped->GetPosition();p.y+=5;p.z+=1;
  fixture->SetPosition(p);fixture->SetHeading(0);fixture->SetStatus(STATUS_ABANDONED);fixture->m_nDoorLock=CARLOCK_UNLOCKED;CWorld::Add(fixture);fixture->PlaceOnRoadProperly();
  p=fixture->GetPosition();p.x-=1.8f;p.y+=.3f;p.z+=.3f;ped->Teleport(p);ped->SetHeading(-HALFPI);ped->m_fRotationCur=ped->m_fRotationDest=-HALFPI;
  next(3);return;
 }
 if(phase==3) {
  if(ped->bInVehicle && ped->m_pMyVehicle==fixture){carInitial=fixture->GetPosition();next(4);return;}
  if(frames>900){result(false,"Y did not enter fixture vehicle");return;}
 }
 if(phase==4 && frames>=150){driven=distance2D(carInitial,fixture->GetPosition());if(driven<3){result(false,"accelerate input did not drive vehicle");return;}xdev_capture_ppm("game:\\gameplay-driving.ppm");next(5);return;}
 if(phase==5 && frames>=90){next(6);return;}
 if(phase==6){if(!ped->bInVehicle)++stable;else stable=0;if(stable>=45){next(7);}else if(frames>900){result(false,"Y did not exit vehicle");return;}}
 if(phase==7) {
  timespec t;clock_gettime(CLOCK_REALTIME,&t);snprintf(saveDirectory,sizeof saveDirectory,"game:\\userfiles\\gameplay-%08x-%08x",unsigned(t.tv_sec),unsigned(t.tv_nsec));
  if(mkdir(saveDirectory,0777)){result(false,"cannot create isolated save directory");return;}
  PcSaveHelper.SetSaveDirectory(saveDirectory);ped->m_fHealth=83;CWorld::Players[0].m_nMoney=4321;savedPosition=ped->GetPosition();savedHealth=ped->m_fHealth;savedMoney=CWorld::Players[0].m_nMoney;
  for(int i=0;i<CTheScripts::NumSaveVars;++i) {const uint8 *raw=(const uint8*)CTheScripts::SavedVarIndices+2*i;uint16 variable=raw[0]|uint16(raw[1])<<8;if(CTheScripts::GetSaveVarIndex(variable)<0){result(false,"persistent SCM variable lookup has wrong endian");return;}}
  xsdk_log_write("LCS GAMEPLAY: %d persistent SCM variable indices verified",CTheScripts::NumSaveVars);
  xsdk_log_write("LCS GAMEPLAY verified walk=%f drive=%f",walked,driven);
  if(CTheScripts::IsPlayerOnAMission()){int ret=PcSaveHelper.SaveSlot(7);if(ret && PcSaveHelper.nErrorCode!=SAVESTATUS_SUCCESSFUL)result(true,"walking/vehicle and mission-save rejection verified; positive safehouse cycle is a separate regression");else result(false,"mission save should have been rejected");return;}
  int ret=PcSaveHelper.SaveSlot(7);strncpy(saveName,ValidSaveName,sizeof saveName-1);
  if(ret || PcSaveHelper.nErrorCode!=SAVESTATUS_SUCCESSFUL || !CheckDataNotCorrupt(7,saveName)){result(false,"real save/checksum failed");return;}
  xsdk_log_write("LCS GAMEPLAY saved: %s health=%f money=%d onMission=%d",saveName,savedHealth,savedMoney,CTheScripts::IsPlayerOnAMission());
  ped->m_fHealth=23;CWorld::Players[0].m_nMoney=savedMoney+12345;
  CVector changed=savedPosition;changed.x+=7;ped->Teleport(changed);
  strcpy(LoadFileName,saveName);FrontEndMenuManager.m_bWantToLoad=true;FrontEndMenuManager.m_bWantToRestart=true;next(8);return;
 }
 if(phase==8 && reloaded){if(frames<90)return;
  float positionError=distance2D(savedPosition,ped->GetPosition());
  xsdk_log_write("LCS GAMEPLAY restored health=%f money=%d positionError=%f",ped->m_fHealth,CWorld::Players[0].m_nMoney,positionError);
  if(Abs(ped->m_fHealth-savedHealth)>.01f || CWorld::Players[0].m_nMoney!=savedMoney || positionError>2){result(false,"loaded gameplay state differs from saved state");return;}
  xdev_capture_ppm("game:\\gameplay-loaded.ppm");result(true,"walk, enter, drive, exit, save/checksum and reload state verified");
 }
}
void lcs_gameplay_test_reloaded(bool success){if(lcs_safehouse_test_active()){lcs_safehouse_test_reloaded(success);return;}if(!enabled || finished || phase!=8)return;if(!success){result(false,"normal reload failed");return;}reloaded=true;frames=0;}
