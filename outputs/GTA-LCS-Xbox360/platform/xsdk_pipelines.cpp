#include "common.h"
#include "custompipes.h"
#include "Timecycle.h"
#include "Lights.h"
#include "rw_xsdk_backend.h"
#include <vector>

// Actual drawing through the native base pipeline. No desktop renderer is used.
// CPU skin/lighting and deferred world materials use the native device.
// Gloss/rim and environment-map effects remain pending.
namespace CustomPipes {
void CreateVehiclePipe() { vehiclePipe=rw::engine->driver[rw::platform]->defaultPipeline; }
void CreateWorldPipe() { worldPipe=rw::engine->driver[rw::platform]->defaultPipeline; }
void CreateGlossPipe() { glossPipe=rw::engine->driver[rw::platform]->defaultPipeline; }
void CreateRimLightPipes() { rimPipe=rimSkinPipe=rw::engine->driver[rw::platform]->defaultPipeline; }
void DestroyVehiclePipe() { vehiclePipe=nil; }
void DestroyWorldPipe() { worldPipe=nil; }
void DestroyGlossPipe() { glossPipe=nil; }
void DestroyRimLightPipes() { rimPipe=rimSkinPipe=nil; }
}
namespace WorldRender {
int numBlendInsts[3];
struct Deferred { RpAtomic *atomic; uint8 fade; };
static std::vector<Deferred> queue[3];
static rw::xsdk::WorldStyle style(int phase,int fade) {
 rw::xsdk::WorldStyle s={}; s.phase=phase; s.fade=fade;
 s.ambient={CTimeCycle::GetAmbientRed(),CTimeCycle::GetAmbientGreen(),CTimeCycle::GetAmbientBlue(),1};
 s.emissive=pAmbient?pAmbient->color:rw::RGBAf{0,0,0,1};
 s.mobile=CustomPipes::WorldPipeSwitch==CustomPipes::WORLDPIPE_MOBILE;
 s.emissiveScale=CustomPipes::WorldPipeSwitch==CustomPipes::WORLDPIPE_PS2?0.5f:1.0f;
 s.colourScale=s.mobile?1.0f:255.0f/128.0f;
 return s;
}
void AtomicFirstPass(RpAtomic *a,int pass) {
 if(!a || pass<0 || pass>2) return;
 if(rw::xsdk::renderWorldObject(a,style(0,255))) {
  queue[pass].push_back({a,255}); numBlendInsts[pass]=queue[pass].size();
 }
}
void AtomicFullyTransparent(RpAtomic *a,int pass,int alpha) {
 if(!a || pass<0 || pass>2 || alpha<=0) return;
 queue[pass].push_back({a,uint8(alpha>255?255:alpha)}); numBlendInsts[pass]=queue[pass].size();
}
void RenderBlendPass(int pass) {
 if(pass<0 || pass>2) return;
 for(const Deferred &entry:queue[pass]) rw::xsdk::renderWorldObject(entry.atomic,style(1,entry.fade));
 queue[pass].clear(); numBlendInsts[pass]=0;
}
}
