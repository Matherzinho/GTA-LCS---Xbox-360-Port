// Opt-in integration regression. Route setup uses teleports to real mission
// triggers; no campaign flags/counters are fabricated. Save/load menus and
// post-load movement/vehicle actions use the production controller adapter.
#include "common.h"
#include "xsdk_safehouse_test.h"
#include "xsdk_input.h"
#include "Game.h"
#include "Pad.h"
#include "Camera.h"
#include "CutsceneMgr.h"
#include "World.h"
#include "Pools.h"
#include "PlayerPed.h"
#include "PlayerInfo.h"
#include "Automobile.h"
#include "Streaming.h"
#include "ModelIndices.h"
#include "ModelInfo.h"
#include "Script.h"
#include "Stats.h"
#include "Radar.h"
#include "Frontend.h"
#include "GenericGameStorage.h"
#include "gpu/xdev.h"
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
extern "C" void xsdk_log_write(const char*,...);
namespace {
enum Stage { INTRO, ENTER, ROUTE, EXIT, SAVE_UI, SAVE_FINISH, LOAD_UI, VERIFY, WALK, LEAVE_HOME, SPAWN_CAR, ENTER_TEST_CAR, DRIVE, BRAKE, EXIT_TEST_CAR, NEXT_MISSION, MISSION_ROUTE, DONE };
bool active=false,cold=false,loaded=false,armed=false,prepareCheckpoint=false,nextStarted=false,guardPreserved=false,menuCaptured=false;
int campaignTarget=0;bool campaignComplete=false;
unsigned commandSerial=0,manualButtons=0,manualFrames=0;
Stage stage=INTRO;unsigned frames=0,total=0,route=0,stable=0;
uint32 stableSince=0,stageSince=0;int missionCar=-1;CAutomobile *testCar=nullptr;
CVector savedPos,walkStart,driveStart;float walked=0,driven=0,health=0,progress=0;
uint32 expectedInventory=0;float savedArmour=-1;bool inventoryVerified=false;
int money=0,passed=0,visits=0;char missionName[9]={},skin[32]={},savePath[260]={},directory[260]={};
uint32 now(){return CTimer::GetTimeInMilliseconds();}
float dist(CVector a,CVector b){a-=b;return Sqrt(a.x*a.x+a.y*a.y);}
bool control(){return FindPlayerPed() && !CPad::GetPad(0)->ArePlayerControlsDisabled() && !CCutsceneMgr::IsCutsceneProcessing() && !CTimer::GetIsPaused() && !FrontEndMenuManager.m_bMenuActive;}
void next(Stage s){stage=s;frames=0;stable=0;stageSince=now();xsdk_log_write("SAFEHOUSE stage=%d route=%u",int(s),route);}
void finish(bool ok,const char *why){
 xsdk_log_write("SAFEHOUSE %s cold=%d walk=%f drive=%f passed=%d progress=%f next=%d guard=%d: %s",ok?"PASS":"FAIL",cold,walked,driven,CStats::MissionsPassed,CStats::ProgressMade,nextStarted,guardPreserved,why);
 char resultPath[128];snprintf(resultPath,sizeof resultPath,"game:\\campaign-vic%d-result.json",campaignTarget);
 FILE *f=fopen(campaignTarget?resultPath:(cold?"game:\\safehouse-cold-result.json":"game:\\safehouse-warm-result.json"),"w");
 if(f){char path[260];strcpy(path,savePath);for(char*p=path;*p;++p)if(*p=='\\')*p='/';
 fprintf(f,"{\"status\":\"%s\",\"cold\":%s,\"stage\":%d,\"frames\":%u,\"save_file\":\"%s\",",ok?"PASS":"FAIL",cold?"true":"false",int(stage),total,path);
 fprintf(f,"\"campaign_target\":%d,\"mission_completed_by_scripts\":%s,",campaignTarget,campaignComplete?"true":"false");
 fprintf(f,"\"inventory_verified\":%s,\"saved_armour\":%.6f,",inventoryVerified?"true":"false",savedArmour);
 fprintf(f,"\"saved_missions\":%d,\"loaded_missions\":%d,\"saved_progress\":%.6f,\"loaded_progress\":%.6f,\"saved_money\":%d,\"loaded_money\":%d,",passed,CStats::MissionsPassed,progress,CStats::ProgressMade,money,CWorld::Players[0].m_nMoney);
 fprintf(f,"\"walk_metres\":%.6f,\"drive_metres\":%.6f,\"next_mission_started\":%s,\"mission_save_preserved_slot\":%s,\"reason\":\"%s\",",walked,driven,nextStarted?"true":"false",guardPreserved?"true":"false",why);
 fprintf(f,"\"fixture\":\"Mission setup uses actual triggers without changing campaign flags; menus and post-load actions use scripted controller input; sparse rendering only during setup; next-mission setup clears wanted level and stages at its real contact\"}\n");fclose(f);}
 stage=DONE;lcs_test_set_pad(nullptr);
}
void stageAt(CVector p){
 // Load collision before relocating the test fixture; this is not gameplay teleport support.
 CStreaming::LoadSceneCollision(p);CStreaming::LoadScene(p);
 CPlayerPed *ped=FindPlayerPed();if(ped->bInVehicle){CVehicle*v=ped->m_pMyVehicle;v->Teleport(p);v->SetMoveSpeed(0,0,0);v->SetTurnSpeed(0,0,0);}else{ped->Teleport(p);ped->SetMoveSpeed(0,0,0);}
 xsdk_log_write("SAFEHOUSE fixture at %f,%f,%f",p.x,p.y,p.z);
}
CVehicle *car(){return missionCar>=0?CPools::GetVehicle(missionCar):nullptr;}
void atDoor(CVehicle *v){CVector p=v->GetPosition()-v->GetRight()*1.8f+v->GetForward()*.3f+v->GetUp()*.3f;stageAt(p);float h=v->GetForward().Heading()-HALFPI;FindPlayerPed()->SetHeading(h);FindPlayerPed()->m_fRotationCur=FindPlayerPed()->m_fRotationDest=h;}
bool coord(CVector &p){for(int i=0;i<NUMRADARBLIPS;++i){auto&b=CRadar::ms_RadarTrace[i];if(b.m_bInUse && b.m_eBlipType==BLIP_COORD && b.m_eRadarSprite==0 && b.m_eBlipDisplay){p=b.m_vecPos;return true;}}return false;}
bool changedClothes(){for(CRunningScript*t=CTheScripts::pActiveScripts;t;t=t->next)if(!strncmp(t->m_abScriptName,"vic1",4))return t->m_anLocalVariables[3]==1;return false;}
uint32 inventory(){uint32 hash=2166136261u;CPlayerPed*p=FindPlayerPed();if(!p)return 0;for(unsigned i=0;i<TOTAL_WEAPON_SLOTS;++i){auto&w=p->GetWeapon(i);hash=(hash^uint32(w.m_eWeaponType))*16777619u;if(w.m_eWeaponType!=WEAPONTYPE_UNARMED)hash=(hash^w.m_nAmmoTotal)*16777619u;}return hash;}
void snapshot(){expectedInventory=inventory();savedArmour=FindPlayerPed()->m_fArmour;CPlayerPed *p=FindPlayerPed();health=p->m_fHealth;money=CWorld::Players[0].m_nMoney;progress=CStats::ProgressMade;passed=CStats::MissionsPassed;visits=CStats::SafeHouseVisits;savedPos=p->GetPosition();memcpy(missionName,CStats::LastMissionPassedName,8);strncpy(skin,CModelInfo::GetModelInfo(p->GetModelIndex())->GetModelName(),sizeof(skin)-1);armed=true;
 xsdk_log_write("SAFEHOUSE snapshot money=%d health=%f passed=%d progress=%f visits=%d mission=%.8s skin=%s pos=%f,%f,%f",money,health,passed,progress,visits,missionName,skin,savedPos.x,savedPos.y,savedPos.z);
}
void profile(){FILE*f=fopen("game:\\safehouse-profile.txt","w");if(f){fprintf(f,"%s\n%.9g %d %.9g %d %d\n%.9g %.9g %.9g\n%.8s %s\n",savePath,health,money,progress,passed,visits,savedPos.x,savedPos.y,savedPos.z,missionName,skin);fprintf(f,"%u %.9g\n",expectedInventory,savedArmour);fclose(f);}}
}
bool lcs_safehouse_test_init(){
 {FILE*f=fopen("game:\\campaign-test.txt","r");if(f){fscanf(f,"%d",&campaignTarget);fclose(f);}}
 active=access("game:\\safehouse-test.txt",F_OK)==0;cold=access("game:\\safehouse-resume.txt",F_OK)==0;bool repeatWarm=active && cold;active|=cold;if(!active)return false;
 if(cold){FILE*f=fopen("game:\\safehouse-resume.txt","r");bool ok=f && fscanf(f,"%259s %f %d %f %d %d %f %f %f %8s %31s",savePath,&health,&money,&progress,&passed,&visits,&savedPos.x,&savedPos.y,&savedPos.z,missionName,skin)==11;if(f && ok)fscanf(f,"%u %f",&expectedInventory,&savedArmour);if(f)fclose(f);if(!ok){finish(false,"invalid cold-load test profile");return true;}strcpy(directory,savePath);char*slash=strrchr(directory,'\\');if(!slash){finish(false,"invalid save directory");return true;}*slash=0;PcSaveHelper.SetSaveDirectory(directory);stage=LOAD_UI;armed=true;prepareCheckpoint=visits==0 || repeatWarm;if(prepareCheckpoint)cold=false;}
 else{timespec t;clock_gettime(CLOCK_REALTIME,&t);snprintf(directory,sizeof directory,"game:\\userfiles\\safehouse-%08x-%08x",unsigned(t.tv_sec),unsigned(t.tv_nsec));if(mkdir(directory,0777)){finish(false,"isolated save directory unavailable");return true;}PcSaveHelper.SetSaveDirectory(directory);snprintf(savePath,sizeof savePath,"%s\\GTAVCsf8.b",directory);}
 xsdk_log_write("SAFEHOUSE integration cold=%d directory=%s",cold,directory);return true;
}
bool lcs_safehouse_test_active(){return active;}
bool lcs_safehouse_test_render(){if(campaignTarget && stage==MISSION_ROUTE)return total%60==0;return !active || stage>=VERIFY || FrontEndMenuManager.m_bMenuActive || total%60==0;}
void lcs_safehouse_test_before(){
 if(!active || stage==DONE)return;GLFWgamepadstate p={};p.axes[4]=p.axes[5]=-1;
 if((stage==INTRO || stage==ROUTE || stage==MISSION_ROUTE) && CCutsceneMgr::IsCutsceneProcessing() && frames%60==1)p.buttons[0]=1;
 CPlayerPed *ped=FindPlayerPed();
 if(stage==MISSION_ROUTE && TheCamera.m_WideScreenOn && !(campaignTarget==4 && route==1) && frames%60==1)p.buttons[0]=1;
 if(stage==MISSION_ROUTE && campaignTarget==4 && route==1){int shop;memcpy(&shop,CTheScripts::ScriptSpace+4368,4);if(shop==4 && frames%60==1)p.buttons[0]=1;if(shop==5 && frames%60==1)p.buttons[3]=1;}
 if(stage==MISSION_ROUTE && campaignTarget==4 && route==3 && control()){p.buttons[5]=frames%120<100;p.buttons[1]=frames%24<18;}
 if(stage==MISSION_ROUTE && route==6 && campaignTarget==4 && ped && !ped->bInVehicle && !ped->EnteringCar() && frames%120==1)p.buttons[3]=1;
 if(stage==MISSION_ROUTE && route==2 && ped && !ped->bInVehicle && !ped->EnteringCar() && frames%120==1)p.buttons[3]=1;
 if(stage==MISSION_ROUTE && campaignTarget==3 && route>=1 && control()){p.buttons[5]=frames%120<100;p.buttons[1]=frames%60<50;}
 if(stage==MISSION_ROUTE && route==4 && ped && ped->bInVehicle && ped->Driving() && frames%120==1)p.buttons[3]=1;
 if((stage==ENTER || stage==ENTER_TEST_CAR) && ped && !ped->bInVehicle && !ped->EnteringCar() && frames%120==1)p.buttons[3]=1;
 if((stage==EXIT || stage==EXIT_TEST_CAR) && ped && ped->bInVehicle && ped->Driving() && frames%120==1)p.buttons[3]=1;
 if(stage==WALK)p.axes[1]=-.7f;
 if(stage==DRIVE){p.buttons[0]=1;p.axes[0]=.12f;}
 if(stage==BRAKE)p.buttons[2]=1;
 if(stage==SAVE_UI && FrontEndMenuManager.m_bMenuActive){
  int screen=FrontEndMenuManager.m_nCurrScreen;
  if(screen==MENUPAGE_CHOOSE_SAVE_SLOT){FrontEndMenuManager.m_nCurrOption=7;if(frames%30==1)p.buttons[0]=1;}
  else if(screen==MENUPAGE_SAVE_OVERWRITE_CONFIRM){FrontEndMenuManager.m_nCurrOption=2;if(frames%30==1)p.buttons[0]=1;}
  else if(screen==MENUPAGE_SAVE_SUCCESSFUL){FrontEndMenuManager.m_nCurrOption=1;if(frames%30==1)p.buttons[0]=1;}
 }
 if(stage==LOAD_UI && FrontEndMenuManager.m_bMenuActive){
  int screen=FrontEndMenuManager.m_nCurrScreen;
  if(screen==MENUPAGE_CHOOSE_LOAD_SLOT){FrontEndMenuManager.m_nCurrOption=7;if(frames%30==1)p.buttons[0]=1;}
  else if(screen==MENUPAGE_LOAD_SLOT_CONFIRM){FrontEndMenuManager.m_nCurrOption=2;if(frames%30==1)p.buttons[0]=1;}
 }
 // Optional controller/position fixture input, active only in campaign tests.
 if(campaignTarget && total%60==0){FILE*f=fopen("game:\\campaign-input.txt","r");if(f){unsigned serial;char action[16];float a,b,c;if(fscanf(f,"%u %15s %f %f %f",&serial,action,&a,&b,&c)==5 && serial!=commandSerial){commandSerial=serial;if(!strcmp(action,"pad")){manualButtons=unsigned(a)&32767;manualFrames=unsigned(Clamp(b,0.f,10000.f));}else if(!strcmp(action,"stage") && FindPlayerPed())stageAt(CVector(a,b,c));xsdk_log_write("CAMPAIGN operator input serial=%u action=%s values=%f,%f,%f",serial,action,a,b,c);}fclose(f);}}
 if(manualFrames){for(unsigned i=0;i<15;++i)p.buttons[i]=(manualButtons>>i)&1;--manualFrames;}
 lcs_test_set_pad(&p);
}
void lcs_safehouse_test_after(){
 if(!active || stage==DONE)return;++total;++frames;
 if(total>150000){finish(false,"integration frame budget exceeded");return;}
 CPlayerPed *ped=FindPlayerPed();
 if(total%120==0)xsdk_log_write("SAFEHOUSE state stage=%d route=%u frames=%u menu=%d screen=%d controls=%d passed=%d progress=%f pos=%f,%f,%f",int(stage),route,frames,FrontEndMenuManager.m_bMenuActive,FrontEndMenuManager.m_nCurrScreen,CPad::GetPad(0)->ArePlayerControlsDisabled(),CStats::MissionsPassed,CStats::ProgressMade,ped?ped->GetPosition().x:0,ped?ped->GetPosition().y:0,ped?ped->GetPosition().z:0);
 if(stage==LOAD_UI){
  if(frames==60){PcSaveHelper.PopulateSlotInfo();if(Slots[7]!=SLOT_OK){finish(false,"cold title menu did not recognize saved slot");return;}FrontEndMenuManager.SwitchToNewScreen(MENUPAGE_CHOOSE_LOAD_SLOT);}
  return;
 }
 if(stage==INTRO){
  if(control() && !TheCamera.m_WideScreenOn){if(!stable++)stableSince=now();}else stable=0;
  if(stable && now()-stableSince>6000 && !ped->bInVehicle){for(int i=0;i<CPools::GetVehiclePool()->GetSize();++i){CVehicle*v=CPools::GetVehiclePool()->GetSlot(i);if(v && v->VehicleCreatedBy==MISSION_VEHICLE && !v->pDriver){missionCar=CPools::GetVehicleRef(v);break;}}if(!car()){finish(false,"first mission car missing");return;}CPad::GetPad(0)->Mode=0;atDoor(car());next(ENTER);}return;
 }
 if(!ped){finish(false,"player missing after initialization");return;}
 if(stage==ENTER){if(ped->bInVehicle && ped->m_pMyVehicle==car()){next(ROUTE);if(route==5)route=6;}return;}
 if(stage==ROUTE && control()){
  CVector goal;
  if(route==0 && ped->bInVehicle && coord(goal) && goal.y> -500){goal.z+=.8f;stageAt(goal);route=1;}
  else if(route==1 && !ped->bInVehicle && coord(goal)){goal.z+=1;stageAt(goal);route=2;}
  else if(route==2 && !ped->bInVehicle && ped->GetPosition().z>22){stageAt(CVector(1137.912f,-250.6f,23.1f));route=3;}
  else if(route==3 && changedClothes()){stageAt(CVector(1146.47f,-246.03f,23.1f));route=4;}
  else if(route==4 && !ped->bInVehicle && ped->GetPosition().z<20 && car()){atDoor(car());route=5;next(ENTER);}
  else if(route==6 && ped->bInVehicle && coord(goal) && goal.y< -1000){goal.z+=.8f;stageAt(goal);route=7;}
  else if(route==7 && CStats::MissionsPassed>=1 && !CTheScripts::IsPlayerOnAMission()){route=8;next(EXIT);}
  else if(route==8 && !ped->bInVehicle && ped->IsPedInControl()){
   // Preserve a real post-mission checkpoint before testing the house UI.
   snapshot();char check[260];if(PcSaveHelper.SaveSlot(7) || !CheckDataNotCorrupt(7,check)){finish(false,"post-mission checkpoint write/checksum failed");return;}profile();armed=false;
   xsdk_log_write("SAFEHOUSE setup checkpoint verified: %s",savePath);
   stageAt(CVector(1147.6f,-262.9f,17.3f));route=9;
  }
  else if(route==9 && !ped->bInVehicle && ped->GetPosition().z>22){stageAt(CVector(1138.3f,-241.95f,23.1f));next(SAVE_UI);}
  else if(route==10 && now()-stageSince>1000 && !CTheScripts::IsPlayerOnAMission()){stageAt(CVector(1138.3f,-241.95f,23.1f));next(SAVE_UI);}
  return;
 }
 if(stage==EXIT){if(!ped->bInVehicle && ped->IsPedInControl())++stable;else stable=0;if(stable>=45)next(ROUTE);return;}
 if(stage==SAVE_UI){
  if(!menuCaptured && FrontEndMenuManager.m_bMenuActive && FrontEndMenuManager.m_nMenuFadeAlpha>=255){xdev_capture_ppm("game:\\safehouse-save-menu.ppm");menuCaptured=true;}
  if(!armed && FrontEndMenuManager.m_bMenuActive && FrontEndMenuManager.m_nCurrScreen==MENUPAGE_CHOOSE_SAVE_SLOT){snapshot();}
  if(armed && FrontEndMenuManager.m_nCurrScreen==MENUPAGE_SAVE_SUCCESSFUL){char path[260];if(!CheckDataNotCorrupt(7,path)){finish(false,"safehouse UI save failed checksum");return;}profile();xsdk_log_write("SAFEHOUSE UI file verified: %s",savePath);}
  if(armed && !FrontEndMenuManager.m_bMenuActive && !FrontEndMenuManager.m_bActivateSaveMenu){char path[260];if(!CheckDataNotCorrupt(7,path)){finish(false,"safehouse save file missing");return;}profile();next(SAVE_FINISH);}return;
 }
 if(stage==SAVE_FINISH){if(control() && ++stable>45){ped->m_fHealth=23;CWorld::Players[0].m_nMoney=money+777;strcpy(LoadFileName,savePath);FrontEndMenuManager.m_bWantToLoad=true;FrontEndMenuManager.m_bWantToRestart=true;next(VERIFY);}return;}
 if(stage==VERIFY && loaded){
  if(frames<120)return;
  float error=(savedPos-ped->GetPosition()).Magnitude();bool inventoryOK=!expectedInventory || inventory()==expectedInventory;bool armourOK=savedArmour<0 || Abs(ped->m_fArmour-savedArmour)<.01f;inventoryVerified=inventoryOK && expectedInventory;bool state=inventoryOK && armourOK && Abs(ped->m_fHealth-health)<.01f && CWorld::Players[0].m_nMoney==money && CStats::MissionsPassed==passed && Abs(CStats::ProgressMade-progress)<.001f && !memcmp(CStats::LastMissionPassedName,missionName,8) && !strcmp(CModelInfo::GetModelInfo(ped->GetModelIndex())->GetModelName(),skin) && CStats::SafeHouseVisits==visits;
  if(frames==120 || frames%120==0)xsdk_log_write("SAFEHOUSE restored state=%d health=%f money=%d passed=%d progress=%f error=%f controls=%d",state,ped->m_fHealth,CWorld::Players[0].m_nMoney,CStats::MissionsPassed,CStats::ProgressMade,error,CPad::GetPad(0)->ArePlayerControlsDisabled());
  if(frames==120 || frames%120==0)xsdk_log_write("SAFEHOUSE restored visits=%d expected=%d mission=%.8s expected=%.8s skin=%s expected=%s",CStats::SafeHouseVisits,visits,CStats::LastMissionPassedName,missionName,CModelInfo::GetModelInfo(ped->GetModelIndex())->GetModelName(),skin);
  if(!state || error>2){finish(false,"restored player/story state differs");return;}
  if(control() && ped->IsPedInControl() && !CTheScripts::IsPlayerOnAMission() && TheCamera.GetScreenFadeStatus()==FADE_0){
   if(prepareCheckpoint){prepareCheckpoint=false;armed=false;
    if(visits>0){stageAt(CVector(1142.6f,-246.03f,23.1f));route=10;}else{stageAt(CVector(1147.6f,-262.9f,17.3f));route=9;}next(ROUTE);
   }
   else if(campaignTarget && passed<campaignTarget){stageAt(CVector(1146.47f,-246.03f,23.1f));next(LEAVE_HOME);}
   else{xdev_capture_ppm("game:\\safehouse-loaded.ppm");walkStart=ped->GetPosition();next(WALK);}
  }else if(frames>600)finish(false,"player controls did not recover after load");return;
 }
 if(stage==WALK && frames>=90){walked=dist(walkStart,ped->GetPosition());if(walked<.3f){finish(false,"post-load walking failed");return;}stageAt(CVector(1146.47f,-246.03f,23.1f));next(LEAVE_HOME);return;}
 if(stage==LEAVE_HOME){if(control() && ped->GetPosition().z<20){next(campaignTarget && passed<campaignTarget?NEXT_MISSION:SPAWN_CAR);}return;}
 if(stage==SPAWN_CAR){stageAt(CVector(1158.f,-283.f,22.f));CStreaming::RequestModel(MI_KURUMA,STREAMFLAGS_DONT_REMOVE);CStreaming::LoadAllRequestedModels(false);if(!CStreaming::HasModelLoaded(MI_KURUMA)){finish(false,"post-load Kuruma unavailable");return;}testCar=new CAutomobile(MI_KURUMA,MISSION_VEHICLE);testCar->SetPosition(CVector(1158.f,-279.f,22.f));testCar->SetHeading(-HALFPI);testCar->SetStatus(STATUS_ABANDONED);testCar->m_nDoorLock=CARLOCK_UNLOCKED;CWorld::Add(testCar);testCar->PlaceOnRoadProperly();atDoor(testCar);next(ENTER_TEST_CAR);return;}
 if(stage==ENTER_TEST_CAR){if(ped->bInVehicle && ped->m_pMyVehicle==testCar && ped->Driving()){driveStart=testCar->GetPosition();next(DRIVE);}else if(frames>900)finish(false,"post-load Y enter failed");return;}
 if(stage==DRIVE && frames>=150){driven=dist(driveStart,testCar->GetPosition());if(driven<2){finish(false,"post-load driving failed");return;}xdev_capture_ppm("game:\\safehouse-driving.ppm");next(BRAKE);return;}
 if(stage==BRAKE && frames>=90){next(EXIT_TEST_CAR);return;}
 if(stage==EXIT_TEST_CAR){if(!ped->bInVehicle && ped->IsPedInControl())++stable;else stable=0;if(stable>=45)next(NEXT_MISSION);else if(frames>900)finish(false,"post-load Y exit failed");}
 if(stage==MISSION_ROUTE){
  CRunningScript *mission=nullptr;for(CRunningScript*t=CTheScripts::pActiveScripts;t;t=t->next)if(t->m_abScriptName[0]=='v' && t->m_abScriptName[1]=='i' && t->m_abScriptName[2]=='c' && t->m_abScriptName[3]==char('0'+campaignTarget)){mission=t;break;}
  if(total%120==0 && mission)xsdk_log_write("CAMPAIGN vic%d route=%u ip=%u locals=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",campaignTarget,route,mission->m_nIp-CTheScripts::MainScriptSize,mission->m_anLocalVariables[0],mission->m_anLocalVariables[1],mission->m_anLocalVariables[2],mission->m_anLocalVariables[3],mission->m_anLocalVariables[4],mission->m_anLocalVariables[5],mission->m_anLocalVariables[6],mission->m_anLocalVariables[7],mission->m_anLocalVariables[8],mission->m_anLocalVariables[9]);
  if(ped->m_fHealth<=0){finish(false,"player died during mission");return;}
  if(!mission && CStats::MissionsPassed<campaignTarget && frames>600){finish(false,"mission ended without completing objective");return;}
  if(CStats::MissionsPassed==campaignTarget && !CTheScripts::IsPlayerOnAMission() && control()){
   campaignComplete=true;route=4;
   if(!ped->bInVehicle && ped->IsPedInControl()){armed=false;menuCaptured=false;stageAt(CVector(1147.6f,-262.9f,17.3f));route=9;next(ROUTE);}return;
  }
  if(!mission || !control())return;
  if(campaignTarget==2){
   if(route==0){stageAt(CVector(1343.2131f,-251.4959f,49.7163f));route=1;return;}
   if(route==1 && mission->m_anLocalVariables[2]>0){
    CStreaming::RequestModel(MI_KURUMA,STREAMFLAGS_DONT_REMOVE);CStreaming::LoadAllRequestedModels(false);testCar=new CAutomobile(MI_KURUMA,MISSION_VEHICLE);testCar->SetStatus(STATUS_ABANDONED);testCar->m_nDoorLock=CARLOCK_UNLOCKED;CWorld::Add(testCar);testCar->Teleport(CVector(1340.f,-256.f,49.6f));testCar->SetHeading(0);testCar->SetMoveSpeed(0,0,0);testCar->PlaceOnRoadProperly();atDoor(testCar);route=2;return;
   }
   int ref=mission->m_anLocalVariables[2];CPed *follower=ref>0 && (ref>>8)<CPools::GetPedPool()->GetSize()?CPools::GetPed(ref):nullptr;
   if(route==2 && ped->bInVehicle && ped->Driving() && follower && follower->bInVehicle && follower->m_pMyVehicle==ped->m_pMyVehicle){
    float x,y;memcpy(&x,&mission->m_anLocalVariables[18],4);memcpy(&y,&mission->m_anLocalVariables[19],4);
    stageAt(CVector(x,y,15.77f));route=3;return;
   }
  }
  if(campaignTarget==3){
   if(route==0){CVector goal;if(coord(goal)){goal.z+=1;stageAt(goal);route=1;}return;}
   const int slots[]={15736,15740,15744};CPed*enemy=nullptr;
   for(int slot:slots){int ref;memcpy(&ref,CTheScripts::ScriptSpace+slot,4);CPed*q=ref>0 && (ref>>8)<CPools::GetPedPool()->GetSize()?CPools::GetPed(ref):nullptr;if(q && q->m_fHealth>0 && !q->DyingOrDead()){enemy=q;break;}}
   if(enemy && (frames%120==0 || dist(ped->GetPosition(),enemy->GetPosition())>4)){
    if(ped->bInVehicle){return;}
    CVector pos=enemy->GetPosition()-enemy->GetForward()*(enemy->m_fHealth<30?.8f:1.1f);pos.z+=.1f;stageAt(pos);
    float h=(enemy->GetPosition()-ped->GetPosition()).Heading();ped->SetHeading(h);ped->m_fRotationCur=ped->m_fRotationDest=h;TheCamera.SetCameraDirectlyBehindForFollowPed_CamOnAString();
    xsdk_log_write("CAMPAIGN combat target health=%f player=%f",enemy->m_fHealth,ped->m_fHealth);
   }
  }
  if(campaignTarget==4){
   int shop;memcpy(&shop,CTheScripts::ScriptSpace+4368,4);
   if(total%120==0)xsdk_log_write("CAMPAIGN snuff route=%u shop=%d L6=%d ammo=%u health=%f",route,shop,mission->m_anLocalVariables[6],ped->GetWeapon()->m_nAmmoTotal,ped->m_fHealth);
   if(route==0){CVector goal;if(coord(goal)){goal.z+=1;stageAt(goal);route=1;}return;}
   if(route==1){
    if(shop==1 && dist(ped->GetPosition(),CVector(1069.11f,-398.86f,15.21f))>1.5f)stageAt(CVector(1069.11f,-398.86f,15.21f));
    CVector goal;if(shop==0 && coord(goal) && goal.x<1000){goal.z+=1;stageAt(goal);route=2;}return;
   }
   auto globalPed=[](int off){int ref;memcpy(&ref,CTheScripts::ScriptSpace+off,4);return ref>0 && (ref>>8)<CPools::GetPedPool()->GetSize()?CPools::GetPed(ref):nullptr;};
   CPed*enemy=globalPed(15736);
   if(route==2 && enemy && enemy->m_fHealth>0){CVector pos=enemy->GetPosition()-enemy->GetForward()*5.f;stageAt(pos);float h=(enemy->GetPosition()-ped->GetPosition()).Heading();ped->SetHeading(h);ped->m_fRotationCur=ped->m_fRotationDest=h;TheCamera.SetCameraDirectlyBehindForFollowPed_CamOnAString();route=3;return;}
   if(route==3){
    if(enemy && enemy->m_fHealth>0 && frames%240==0){xsdk_log_write("CAMPAIGN snuff boss health=%f armour=%f",enemy->m_fHealth,enemy->m_fArmour);TheCamera.SetCameraDirectlyBehindForFollowPed_CamOnAString();}
    if(mission->m_anLocalVariables[6]==2){float x,y,z;memcpy(&x,&mission->m_anLocalVariables[58],4);memcpy(&y,&mission->m_anLocalVariables[59],4);memcpy(&z,&mission->m_anLocalVariables[60],4);stageAt(CVector(x,y,z));route=5;}return;
   }
   if(route==5 && mission->m_anLocalVariables[21]==1){int ref;memcpy(&ref,CTheScripts::ScriptSpace+15788,4);CVehicle*v=ref>0 && (ref>>8)<CPools::GetVehiclePool()->GetSize()?CPools::GetVehicle(ref):nullptr;if(v){testCar=(CAutomobile*)v;atDoor(v);route=6;}return;}
   if(route==6 && ped->bInVehicle && ped->m_pMyVehicle==testCar && ped->Driving()){stageAt(CVector(925.5f,-361.f,10.f));route=7;return;}
  }
  if(frames>140000)finish(false,"mission route exceeded frame budget");return;
 }
 if(stage==NEXT_MISSION){
  if(frames==1){bool found=false;CVector goal;for(int i=0;i<NUMRADARBLIPS;++i){auto&b=CRadar::ms_RadarTrace[i];if(b.m_bInUse && b.m_eBlipType==BLIP_CONTACT_POINT && b.m_eRadarSprite==RADAR_SPRITE_VINCENZO){goal=b.m_vecPos;found=true;break;}}
   if(!found){finish(false,"next mission contact missing after load");return;}ped->SetWantedLevel(0);goal.z+=1;stageAt(goal);
  }
  if(CTheScripts::bAlreadyRunningAMissionScript){
   for(CRunningScript*t=CTheScripts::pActiveScripts;t;t=t->next)if(t->m_nIp>=CTheScripts::MainScriptSize && t->m_abScriptName[0]=='v' && t->m_abScriptName[1]=='i' && t->m_abScriptName[2]=='c' && t->m_abScriptName[3]==char('0'+passed+1)){nextStarted=true;xsdk_log_write("SAFEHOUSE next mission started: %.8s",t->m_abScriptName);break;}
   if(nextStarted){
    auto fingerprint=[](const char *path){FILE*f=fopen(path,"rb");if(!f)return uint32(0);uint32 hash=2166136261u;unsigned char b[4096];size_t n;while((n=fread(b,1,sizeof b,f)))for(size_t i=0;i<n;++i)hash=(hash^b[i])*16777619u;fclose(f);return hash;};
    uint32 before=fingerprint(savePath);int ret=PcSaveHelper.SaveSlot(7);guardPreserved=ret!=0 && before && before==fingerprint(savePath);
    if(!guardPreserved){finish(false,"active mission save did not preserve prior slot");return;}
    if(campaignTarget && !campaignComplete){route=0;nextStarted=false;guardPreserved=false;next(MISSION_ROUTE);return;}
    finish(true,"safehouse save/load, restored progress, walking/driving, next mission start and protected save slot verified");return;
   }
  }
  if(frames>900)finish(false,"next mission did not start after load");
 }

}
void lcs_safehouse_test_reloaded(bool ok){if(!active || stage==DONE)return;if(!ok){finish(false,"native load lifecycle failed");return;}loaded=true;testCar=nullptr;next(VERIFY);}



