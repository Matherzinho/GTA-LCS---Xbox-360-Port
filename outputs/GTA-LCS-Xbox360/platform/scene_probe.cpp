// Controlled integration test of the native scene path, not gameplay.
#include <cstdio>
#include <cstring>
#include <cmath>
#include "rw.h"
#include "rw_xsdk_types.h"
#include "gpu/xdev.h"
#include "rw_xsdk_backend.h"
using namespace rw;
static Camera *cam;
static Atomic *quad;
static void capture(const char *name) {
 RGBA bg={16,24,32,255}; cam->clear(&bg,Camera::CLEARIMAGE|Camera::CLEARZ);
 cam->beginUpdate(); quad->render(); cam->endUpdate(); cam->showRaster(0);
 if(!xdev_capture_ppm(name)) { xdev_log("scene capture failed"); }
}
static bool testHierarchy() {
 int32 flags[]={0,HAnimHierarchy::PUSH,HAnimHierarchy::POP,HAnimHierarchy::PUSH|HAnimHierarchy::POP,0,HAnimHierarchy::POP,0};
 HAnimHierarchy *h=HAnimHierarchy::create(7,flags,nil,HAnimHierarchy::LOCALSPACEMATRICES,sizeof(HAnimInterpFrame));
 h->interpolator->applyCB=AnimInterpolatorInfo::find(1)->applyCB;
 h->interpolator->currentInterpKeyFrameSize=sizeof(HAnimInterpFrame);
 V3d offsets[]={{10,0,0},{0,2,0},{0,0,3},{0,4,0},{0,0,5},{0,6,0},{7,0,0}};
 V3d expected[]={{10,0,0},{10,2,0},{10,2,3},{10,4,0},{10,0,5},{10,6,5},{7,0,0}};
 Frame *parent=Frame::create(),*child=Frame::create(); parent->addChild(child); h->parentFrame=child;
 Matrix root; root.setIdentity(); root.pos={3,4,5}; root.flags=0; parent->transform(&root,COMBINEREPLACE);
 for(int i=0;i<7;++i) { HAnimInterpFrame *f=(HAnimInterpFrame*)h->interpolator->getInterpFrame(i); f->q=makeQuat(1,0,0,0); f->t=offsets[i]; }
 for(int rotation=0;rotation<2;++rotation) {
  h->flags|=HAnimHierarchy::LOCALSPACEMATRICES;
  if(rotation) {
   ((HAnimInterpFrame*)h->interpolator->getInterpFrame(0))->q=makeQuat(.707106781f,0,0,.707106781f);
   V3d rotated[]={{10,0,0},{8,0,0},{8,0,3},{6,0,0},{10,0,5},{4,0,5},{7,0,0}};
   std::memcpy(expected,rotated,sizeof expected);
  }
 for(int space=0;space<2;++space) {
  if(space) h->flags&=~HAnimHierarchy::LOCALSPACEMATRICES;
  h->updateMatrices();
  for(int i=0;i<7;++i) {
   V3d target=space?add(expected[i],root.pos):expected[i],delta=sub(h->matrices[i].pos,target);
   if(dot(delta,delta)>1.0e-8f) { xdev_log("hierarchy mismatch space=%d node=%d got=%f,%f,%f",space,i,h->matrices[i].pos.x,h->matrices[i].pos.y,h->matrices[i].pos.z); return false; }
  }
 }
 }
 h->parentFrame=nil; h->destroy(); child->destroy(); parent->destroy();
 xdev_log("hierarchy regression PASS: sibling limbs, PUSH/POP/combined, rotated parents, local/global matrices"); return true;
}
int main() {
 if(!Engine::init()) return 1;
 registerMeshPlugin(); registerNativeDataPlugin(); registerHAnimPlugin(); registerSkinPlugin();
 if(!Engine::open(nil) || !Engine::start()) return 2;
 if(!testHierarchy()) return 30;
 World *world=World::create();
 cam=Camera::create(); Frame *cf=Frame::create(); cam->setFrame(cf);
 cam->frameBuffer=Raster::create(1280,720,32,Raster::CAMERA);
 cam->zBuffer=Raster::create(1280,720,32,Raster::ZBUFFER);
 V2d vw={16.0f/9,1}; cam->setViewWindow(&vw); cam->setNearPlane(.1f); cam->setFarPlane(100);
 world->addCamera(cam);
 Geometry *g=Geometry::create(4,2,Geometry::POSITIONS|Geometry::NORMALS|Geometry::LIGHT|Geometry::MODULATE);
 const V3d points[4]={{-.5f,-.5f,4},{.5f,-.5f,4},{.5f,.5f,4},{-.5f,.5f,4}};
 for(int i=0;i<4;++i) { g->morphTargets[0].vertices[i]=points[i]; g->morphTargets[0].normals[i]={0,0,-1}; }
 g->triangles[0]={{0,1,2},0}; g->triangles[1]={{0,2,3},0};
 Material *mat=Material::create(); mat->color={128,255,255,255}; mat->surfaceProps={1,0,1};
 g->matList.appendMaterial(mat); mat->destroy(); g->calculateBoundingSphere();
 Skin *skin=(Skin*)rwMalloc(sizeof(Skin),MEMDUR_EVENT|ID_SKIN); skin->init(2,2,4); skin->numWeights=2;
 for(int i=0;i<2;++i) { skin->usedBones[i]=i; ((Matrix*)skin->inverseMatrices)[i].setIdentity(); }
 for(int i=0;i<4;++i) { for(int j=0;j<4;++j) {skin->indices[i*4+j]=j==1?1:0; skin->weights[i*4+j]=j==0?.25f:j==1?.75f:0;} }
 Skin::set(g,skin); quad=Atomic::create(); Frame *qf=Frame::create(); quad->setFrame(qf); quad->setGeometry(g,0); g->destroy();
 Matrix placement; placement.setIdentity(); placement.pos={.4f,-.2f,0}; placement.flags=0; qf->transform(&placement,COMBINEREPLACE);
 HAnimHierarchy *hier=HAnimHierarchy::create(2,nil,nil,HAnimHierarchy::LOCALSPACEMATRICES,36);
 for(int i=0;i<2;++i) hier->matrices[i].setIdentity(); Skin::setHierarchy(quad,hier); Skin::setPipeline(quad,0);
 Light *amb=Light::create(Light::AMBIENT); amb->setColor(.25f,.25f,.25f); world->addLight(amb);
 Light *sun=Light::create(Light::DIRECTIONAL); Frame *sf=Frame::create(); sun->setFrame(sf); sun->setColor(.5f,.5f,.5f); world->addLight(sun);
 SetRenderState(ZTESTENABLE,1); SetRenderState(ZWRITEENABLE,1); SetRenderState(CULLMODE,CULLNONE);
 capture("game:\\scene-bind.ppm");
 hier->matrices[1].pos.x=1; hier->matrices[1].flags=0; capture("game:\\scene-pose.ppm");
 hier->flags&=~HAnimHierarchy::LOCALSPACEMATRICES;
 for(int i=0;i<2;++i) { Matrix pose=hier->matrices[i]; Matrix::mult(&hier->matrices[i],&pose,&placement); }
 capture("game:\\scene-world-pose.ppm");
 sun->setColor(0,0,0); capture("game:\\scene-ambient.ppm");
 sun->setColor(.5f,.5f,.5f);
 SetRenderState(CULLMODE,CULLBACK); capture("game:\\scene-cull-back.ppm");
 SetRenderState(CULLMODE,CULLFRONT); capture("game:\\scene-cull-front.ppm");
 SetRenderState(CULLMODE,CULLNONE);
 hier->flags|=HAnimHierarchy::LOCALSPACEMATRICES;
 for(int i=0;i<2;++i) { hier->matrices[i].setIdentity(); hier->matrices[i].right.x=-1; hier->matrices[i].at.z=-1; hier->matrices[i].pos.z=8; hier->matrices[i].flags=0; }
 capture("game:\\scene-normal-flip.ppm");
 // Reuse the real object pipeline to verify depth order and near-plane clipping.
 Skin::setHierarchy(quad,nil); g=quad->geometry; g->flags=Geometry::POSITIONS|Geometry::MODULATE;
 for(int order=0;order<2;++order) {
  RGBA bg={16,24,32,255}; cam->clear(&bg,Camera::CLEARIMAGE|Camera::CLEARZ); cam->beginUpdate();
  for(int draw=0;draw<2;++draw) {
   bool near=(draw==order); Matrix m; m.setIdentity(); m.pos.z=near?-1.0f:1.0f; m.flags=0;
   qf->transform(&m,COMBINEREPLACE); mat->color=near?makeRGBA(255,0,0,255):makeRGBA(0,0,255,255); quad->render();
  }
  cam->endUpdate(); cam->showRaster(0); xdev_capture_ppm(order?"game:\\scene-depth-reverse.ppm":"game:\\scene-depth.ppm");
 }
 Matrix origin; origin.setIdentity(); qf->transform(&origin,COMBINEREPLACE); mat->color={255,0,0,255};
 for(int i=0;i<4;++i) g->morphTargets[0].vertices[i].z=-1;
 capture("game:\\scene-behind.ppm");
 for(int i=0;i<4;++i) g->morphTargets[0].vertices[i].z=i<2?.05f:2;
 capture("game:\\scene-near-clip.ppm");
 // World prelight/emission must be scaled after the texture sample.
 RGBA prelight[4]; for(int i=0;i<4;++i) { prelight[i]={64,128,192,255}; g->morphTargets[0].vertices[i]=points[i]; }
 g->flags=Geometry::POSITIONS|Geometry::PRELIT; g->colors=prelight;
 Image *image=Image::create(2,2,32); image->allocate();
 for(int y=0;y<2;++y) for(int x=0;x<2;++x) { uint8 *px=image->pixels+y*image->stride+x*4; px[0]=px[1]=px[2]=128; px[3]=255; }
 Texture *texture=Texture::create(Raster::createFromImage(image)); image->destroy(); mat->texture=texture; mat->color={255,255,255,255};
 xsdk::WorldStyle ws={}; ws.ambient={.5f,.25f,1,1}; ws.emissive={.125f,.25f,0,1}; ws.emissiveScale=.5f; ws.colourScale=255.0f/128; ws.fade=255;
 auto worldCapture=[&](const char *path) {
  RGBA bg={16,24,32,255}; cam->clear(&bg,Camera::CLEARIMAGE|Camera::CLEARZ); cam->beginUpdate();
  bool deferred=xsdk::renderWorldObject(quad,ws); cam->endUpdate(); cam->showRaster(0); xdev_capture_ppm(path); return deferred;
 };
 if(worldCapture("game:\\scene-world-light.ppm")) return 20;
 mat->color.alpha=128;
 if(!worldCapture("game:\\scene-world-deferred.ppm")) return 21;
 ws.phase=1; worldCapture("game:\\scene-world-alpha.ppm");
 SetRenderState(GSALPHATEST,1);SetRenderState(GSALPHATESTREF,128);
 worldCapture("game:\\scene-world-dual.ppm");SetRenderState(GSALPHATEST,0);
 mat->texture=nil; ws.phase=0; worldCapture("game:\\scene-world-proxy.ppm");
 // Regression: nonzero mesh base must preserve the same projected quad.
 Atomic *originalQuad=quad;
 for(int base=0;base<=4;base+=4) {
  Geometry *range=Geometry::create(4+base,2,Geometry::POSITIONS);
  for(int i=0;i<base;++i)range->morphTargets[0].vertices[i]={0,0,4};
  for(int i=0;i<4;++i)range->morphTargets[0].vertices[i+base]=points[i];
  range->triangles[0]={{uint16(base),uint16(base+1),uint16(base+2)},0};
  range->triangles[1]={{uint16(base),uint16(base+2),uint16(base+3)},0};
  Material *white=Material::create();range->matList.appendMaterial(white);white->destroy();
  range->calculateBoundingSphere();quad=Atomic::create();quad->setFrame(Frame::create());quad->setGeometry(range,0);range->destroy();
  capture(base?"game:\\scene-range-offset.ppm":"game:\\scene-range-zero.ppm");
  Frame *frame=quad->getFrame();quad->destroy();frame->destroy();
 }
 quad=originalQuad;
 // FXAA edge regression and unfiltered HUD overlay, same camera/scene.
 Geometry *edge=Geometry::create(3,1,Geometry::POSITIONS);
 edge->morphTargets[0].vertices[0]={-1,-.7f,4};edge->morphTargets[0].vertices[1]={1,.9f,4};edge->morphTargets[0].vertices[2]={1,-.7f,4};
 edge->triangles[0]={{0,1,2},0};Material *white=Material::create();edge->matList.appendMaterial(white);white->destroy();edge->calculateBoundingSphere();
 Atomic *triangle=Atomic::create();triangle->setFrame(Frame::create());triangle->setGeometry(edge,0);edge->destroy();
 for(int aa=0;aa<2;++aa){
  RGBA bg={16,24,32,255};cam->clear(&bg,Camera::CLEARIMAGE|Camera::CLEARZ);cam->beginUpdate();triangle->render();
  xdev_fxaa_enabled=aa;xdev_postprocess();if(aa && !xdev_fxaa_enabled)return 40;
  xsdk::Im2DVertex overlay[4]={};for(int i=0;i<4;++i){overlay[i].x=20+(i/2)*16;overlay[i].y=20+(i%2)*16;overlay[i].w=1;overlay[i].r=255;overlay[i].a=255;}
  SetRenderStatePtr(TEXTURERASTER,nil);SetRenderState(ZTESTENABLE,0);SetRenderState(ZWRITEENABLE,0);SetRenderState(VERTEXALPHA,0);
  im2d::RenderPrimitive(PRIMTYPETRISTRIP,overlay,4);
  cam->endUpdate();cam->showRaster(0);xdev_capture_ppm(aa?"game:\\scene-fxaa-on.ppm":"game:\\scene-fxaa-off.ppm");
 }
 xdev_fxaa_enabled=0;
 xdev_log("scene probe completed: weighted bones, local/world hierarchy, ambient/directional, front/back cull");
 // Process teardown releases the diagnostic heap; real game lifecycle is separate.
 return 0;
}
