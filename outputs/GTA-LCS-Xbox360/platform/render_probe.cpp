// Internal GPU integration test. This does not start or simulate the game.
#include <cstdio>
#include "rw.h"
#include "rw_xsdk_types.h"
#include "gpu/xdev.h"

static void quad(rw::Texture *t,float x,float y,float w,float h,int mode=0,unsigned char alpha=255)
{
 rw::xsdk::Im2DVertex v[4]={};
 for(int i=0;i<4;++i) {
  v[i].x=x+(i/2)*w; v[i].y=y+(i%2)*h; v[i].w=1;
  v[i].r=v[i].g=v[i].b=255; v[i].a=alpha; v[i].u=i/2; v[i].v=i%2;
 }
 rw::SetRenderStatePtr(rw::TEXTURERASTER,t?t->raster:nil);
 if(mode==1) { rw::uint16 ix[]={0,1,3,2}; rw::im2d::RenderIndexedPrimitive(rw::PRIMTYPETRIFAN,v,4,ix,4); }
 else if(mode==2) { rw::uint16 ix[]={0,1,2,2,1,3}; rw::im2d::RenderIndexedPrimitive(rw::PRIMTYPETRILIST,v,4,ix,6); }
 else rw::im2d::RenderPrimitive(rw::PRIMTYPETRISTRIP,v,4);
}
static rw::Texture *colorTest()
{
 rw::Image *img=rw::Image::create(2,2,32); img->allocate();
 const unsigned char rgba[]={255,0,0,255,0,255,0,255,0,0,255,255,255,255,255,255};
 for(int y=0;y<2;++y) for(int x=0;x<8;++x) img->pixels[y*img->stride+x]=rgba[y*8+x];
 rw::Raster *r=rw::Raster::createFromImage(img); img->destroy();
 return r?rw::Texture::create(r):nil;
}
int main()
{
 if(!rw::Engine::init() || !rw::Engine::open(nil) || !rw::Engine::start()) return 1;
 rw::StreamFile s;
 if(!s.open("game:\\data\\models\\hud.txd","rb") || !rw::findChunk(&s,rw::ID_TEXDICTIONARY,nil,nil)) return 2;
 rw::TexDictionary *txd=rw::TexDictionary::streamRead(&s); s.close();
 if(!txd || txd->count()==0) return 3;
 xdev_log("HUD dictionary: %d real textures",txd->count());
 // Native pixel aspect: square textures stay square at 1280x720.
 xdev_clear(0xff203040,1,1);
 int i=0,captured=1;
 rw::Texture *test=colorTest(); if(!test) return 5;
 FORLIST(lnk,txd->textures) {
  rw::Texture *t=rw::Texture::fromDict(lnk);
  xdev_log("texture %s: %dx%d",t->name,t->raster->width,t->raster->height);
  float scale=128.0f/(t->raster->width>t->raster->height?t->raster->width:t->raster->height);
  int cell=i%32;
  quad(t,24+(cell%8)*156,24+(cell/8)*156,t->raster->width*scale,t->raster->height*scale);
  if(++i%32==0) {
   xdev_present(); captured&=xdev_capture_ppm("game:\\render-probe.ppm");
   xdev_clear(0xff203040,1,1);
  }
 }
 rw::SetRenderState(rw::TEXTUREFILTER,rw::Texture::NEAREST);
 rw::SetRenderState(rw::TEXTUREADDRESS,rw::Texture::CLAMP);
 quad(test,24,640,64,64); quad(test,120,640,64,64,1); quad(test,216,640,64,64,2);
 rw::SetRenderState(rw::VERTEXALPHA,1); quad(nil,320,640,64,64,0,128);
 // A write must invalidate the cached GPU copy, without freeing data still in use.
 unsigned char *pixels=test->raster->lock(0,rw::Raster::LOCKWRITE);
 if(!pixels) return 6;
 for(int y=0;y<2;++y) for(int x=0;x<2;++x) { unsigned char *p=pixels+y*test->raster->stride+x*4; p[0]=255; p[1]=255; p[2]=0; p[3]=255; }
 test->raster->unlock(0);
 quad(test,416,640,64,64);
 xdev_present();
 captured&=xdev_capture_ppm("game:\\render-probe-2.ppm");
 xdev_log("render probe: %d quads, capture=%d",i,captured);
 test->destroy(); txd->destroy(); rw::Engine::stop(); rw::Engine::close(); rw::Engine::term();
 return captured?0:4;
}
