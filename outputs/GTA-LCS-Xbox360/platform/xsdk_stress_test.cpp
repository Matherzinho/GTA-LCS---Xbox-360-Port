// Opt-in destructive scene fixture. Never enabled in the console package.
#include "common.h"
#include "PlayerPed.h"
#include "PlayerInfo.h"
#include "World.h"
#include "Automobile.h"
#include "Streaming.h"
#include "ModelIndices.h"
#include "Explosion.h"
#include "CutsceneMgr.h"
#include "Frontend.h"
#include "Pad.h"
#include "Timer.h"
#include "gpu/xdev.h"
#include <unistd.h>
extern "C" void xsdk_log_write(const char*,...);
namespace {
bool enabled,finished,proof;unsigned ready,phase,frame,peak,blown,bursts;CAutomobile *cars[8];
struct Sample{unsigned n=0,over=0;double sum=0,max=0;} samples[3];
void result(bool pass,const char *reason) {
 FILE *f=fopen("game:\\stress-result.json","w");
 if(f){fprintf(f,"{\"status\":\"%s\",\"reason\":\"%s\",\"cars_exploded\":%u,\"peak_active_explosions\":%u,\"phases\":[",pass?"PASS":"FAIL",reason,blown,peak);
 for(int i=0;i<3;++i){Sample &s=samples[i];fprintf(f,"%s{\"name\":\"%s\",\"frames\":%u,\"work_avg_ms\":%.3f,\"work_max_ms\":%.3f,\"over_33ms\":%u}",i?",":"",i==0?"eight_cars":i==1?"explosions":"burning_wrecks",s.n,s.n?s.sum/s.n:0,s.max,s.over);}fprintf(f,"],\"scope\":\"Synthetic eight-car explosion fixture, full rendering; Xenia host timings, not console FPS or combat/campaign certification\"}\n");fclose(f);}
 if(FindPlayerPed())FindPlayerPed()->bExplosionProof=proof;
 xsdk_log_write("LCS STRESS %s explosions=%u peak=%u: %s",pass?"PASS":"FAIL",blown,peak,reason);finished=true;
}
}
extern "C" void lcs_stress_init(){enabled=access("game:\\stress-test.txt",F_OK)==0;}
extern "C" void lcs_stress_step() {
 if(!enabled || finished)return;CPlayerPed *p=FindPlayerPed();
 if(!phase){
  if(!p || CCutsceneMgr::IsCutsceneProcessing() || FrontEndMenuManager.m_bMenuActive || CTimer::GetIsPaused() || CPad::GetPad(0)->ArePlayerControlsDisabled())return;
  if(++ready<180)return;
  CStreaming::LoadSceneCollision(CVector(1158,-283,22));CStreaming::LoadScene(CVector(1158,-283,22));
  p->Teleport(CVector(1158,-283,22));p->SetHeading(0);p->m_fRotationCur=p->m_fRotationDest=0;proof=p->bExplosionProof;p->bExplosionProof=true;
  CStreaming::RequestModel(MI_KURUMA,STREAMFLAGS_DONT_REMOVE);CStreaming::LoadAllRequestedModels(false);
  if(!CStreaming::HasModelLoaded(MI_KURUMA)){result(false,"stress vehicle streaming failed");return;}
  for(int i=0;i<8;++i){cars[i]=new CAutomobile(MI_KURUMA,MISSION_VEHICLE);cars[i]->SetStatus(STATUS_ABANDONED);cars[i]->SetPosition(CVector(1150+(i%4)*5,-264+(i/4)*7,22));CWorld::Add(cars[i]);cars[i]->PlaceOnRoadProperly();}
  phase=1;frame=0;xsdk_log_write("LCS STRESS: eight cars, 300 baseline / 240 explosions / 300 burning wreck frames; no save writes");return;
 }
 if(phase==2 && (frame==60 || frame==120))for(int i=0;i<4;++i)
  if(CExplosion::AddExplosion(nil,nil,EXPLOSION_ROCKET,CVector(1150+i*5,-261,22),0))++bursts;
 if(phase==2 && frame%30==0 && frame/30<8){cars[frame/30]->BlowUpCar(nil);if(cars[frame/30]->m_fHealth<=0)++blown;}
 unsigned active=0;for(int i=0;i<NUM_EXPLOSIONS;++i)if(CExplosion::GetExplosionActiveCounter(i))++active;if(active>peak)peak=active;
}
extern "C" void lcs_stress_sample(double workMs) {
 if(!enabled || finished || !phase)return;
 // Setup is excluded; timing includes full rendering and normal engine updates.
 if(frame==0 && phase==1){++frame;return;}
 Sample &s=samples[phase-1];++s.n;s.sum+=workMs;if(workMs>s.max)s.max=workMs;if(workMs>1000.0/30.0)++s.over;
 ++frame;unsigned limit=phase==2?240:300;
 if(s.n>=limit){xdev_capture_ppm(phase==1?"game:\\stress-cars.ppm":phase==2?"game:\\stress-explosions.ppm":"game:\\stress-wrecks.ppm");
  xdev_perf_report();if(phase==3){result(blown==8 && peak>=4 && bursts==8,"completed fully rendered eight-car explosion and burning wreck sequence");return;}++phase;frame=0;}
}
