// Internal integration test using an actual converted LCS vehicle, not a game build.
#include <cstdio>
#include <cstring>
#include <cmath>
#include "rw.h"
#include "lcs_endian.h"
#include "gpu/xdev.h"

static rw::TexDictionary *genericTextures;
static int missingTextures;
static rw::Texture *findTexture(const char *name)
{
 rw::TexDictionary *current=rw::TexDictionary::getCurrent();
 rw::Texture *t=current?current->find(name):nil;
 if(!t && genericTextures) t=genericTextures->find(name);
 if(!t) { ++missingTextures; xdev_log("missing model texture: %s",name); }
 return t;
}

static bool openAsset(rw::StreamFile &s,const char *name,rw::uint32 chunk)
{
 FILE *dir=fopen("game:\\data\\models\\gta3.dir","rb");
 if(!dir) return false;
 unsigned char row[32]; unsigned offset=0; bool found=false;
 while(fread(row,1,32,dir)==32) if(std::strncmp((char*)row+8,name,24)==0) { offset=lcs_load_u32le(row)*2048u; found=true; break; }
 fclose(dir);
 if(!found || !s.open("game:\\data\\models\\gta3.img","rb")) return false;
 s.seek(offset,SEEK_SET); return rw::findChunk(&s,chunk,nil,nil);
}
int main()
{
 using namespace rw;
 if(!Engine::init()) return 1;
 registerMeshPlugin(); registerNativeDataPlugin(); registerHAnimPlugin();
 if(!Engine::open(nil) || !Engine::start()) return 2;
 StreamFile s;
 if(!s.open("game:\\data\\models\\generic.txd","rb") || !findChunk(&s,ID_TEXDICTIONARY,nil,nil)) return 10;
 genericTextures=TexDictionary::streamRead(&s); s.close();
 if(!genericTextures) return 11;
 Texture *(*oldFind)(const char*)=Texture::findCB; Texture::findCB=findTexture;
 if(!openAsset(s,"kuruma.txd",ID_TEXDICTIONARY)) return 3;
 TexDictionary *txd=TexDictionary::streamRead(&s); s.close();
 if(!txd) return 4;
 TexDictionary::setCurrent(txd); Texture::setLoadTextures(true);
 if(!openAsset(s,"kuruma.dff",ID_CLUMP)) return 5;
 Clump *model=Clump::streamRead(&s); s.close();
 if(!model || missingTextures) return 6;
 xdev_log("kuruma: %d atomics, %d textures",model->countAtomics(),txd->count());
 BBox box; V3d first={0,0,0}; box.initialize(&first); int verts=0,tris=0,unsupported=0;
 FORLIST(lnk,model->atomics) {
  Atomic *a=Atomic::fromClump(lnk); Geometry *g=a->geometry;
  if(!g || !g->morphTargets || !g->morphTargets[0].vertices || (g->flags&Geometry::NATIVE)) { ++unsupported; continue; }
  xdev_log("geometry: %d vertices, %d triangles, flags=%x",g->numVertices,g->numTriangles,g->flags);
  verts+=g->numVertices; tris+=g->numTriangles;
  for(int i=0;i<g->numVertices;++i) { V3d p; V3d::transformPoints(&p,&g->morphTargets[0].vertices[i],1,a->getFrame()->getLTM()); box.addPoint(&p); }
 }
 if(unsupported || !verts || !tris) { xdev_log("unsupported model geometry: %d",unsupported); return 7; }
 V3d center=scale(add(box.inf,box.sup),.5f), span=sub(box.sup,box.inf);
 float radius=std::sqrt(dot(span,span))*.5f;
 Frame *cf=Frame::create(); Matrix cm; cm.setIdentity();
 cm.pos=add(center,V3d{1.4f*radius,-2*radius,1.1f*radius}); cm.lookAt(sub(center,cm.pos),V3d{0,0,1});
 cf->transform(&cm,COMBINEREPLACE);
 Camera *cam=Camera::create(); cam->setFrame(cf);
 cam->frameBuffer=Raster::create(1280,720,32,Raster::CAMERA);
 cam->zBuffer=Raster::create(1280,720,32,Raster::ZBUFFER);
 if(!cam->frameBuffer || !cam->zBuffer) return 8;
 V2d view={.577350269f*16/9,.577350269f}; cam->setViewWindow(&view);
 cam->setNearPlane(.05f); cam->setFarPlane(radius*10);
 RGBA bg={32,48,64,255}; cam->clear(&bg,Camera::CLEARIMAGE|Camera::CLEARZ);
 SetRenderState(ZTESTENABLE,1); SetRenderState(ZWRITEENABLE,1); SetRenderState(CULLMODE,CULLNONE);
 SetRenderState(GSALPHATEST,1); SetRenderState(GSALPHATESTREF,128);
 cam->beginUpdate(); model->render(); cam->endUpdate(); cam->showRaster(0);
 int capture=xdev_capture_ppm("game:\\model-probe.ppm");
 xdev_log("model probe: %d vertices, %d triangles, capture=%d",verts,tris,capture);
 cam->frameBuffer->destroy(); cam->zBuffer->destroy(); cam->frameBuffer=cam->zBuffer=nil;
 cam->destroy(); cf->destroy(); model->destroy(); TexDictionary::setCurrent(nil); txd->destroy();
 genericTextures->destroy(); genericTextures=nil; Texture::findCB=oldFind;
 Engine::stop(); Engine::close(); Engine::term();
 return capture?0:9;
}
