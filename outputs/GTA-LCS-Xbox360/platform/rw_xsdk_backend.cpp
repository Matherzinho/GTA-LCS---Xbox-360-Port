// Xenos device with portable librw native texture storage (no Microsoft D3D).
#include <cstring>
#include <vector>
#include <cmath>
#include <cstdio>
#include "rw.h"
#include "rw_xsdk_types.h"
#include "rw_xsdk_backend.h"
#include "gpu/xdev.h"

namespace rw { namespace xsdk {
RGBA im3dMaterialColor = {255,255,255,255};
SurfaceProperties im3dSurfaceProps = {1,0,1};
struct RasterGPU { uint32 texture; bool alpha; };
static int32 rasterOffset;
static uintptr states[GSALPHATESTREF+1];
static Raster *(*cpuRasterCreate)(Raster*);
static std::vector<XDevVertex> vertices;
static std::vector<uint16> meshIndices;
static std::vector<XDevVertex> immediate3D;
static Matrix immediateWorld;
static ObjPipeline *objectPipeline;
static Atomic *lastCharacter;
static RasterGPU *gpu(Raster *r) { return PLUGINOFFSET(RasterGPU,r,rasterOffset); }
static void *rasterCtor(void *p,int32 off,int32) { std::memset(PLUGINOFFSET(void,p,off),0,sizeof(RasterGPU)); return p; }
static void *rasterDtor(void *p,int32 off,int32) { xdev_tex_free(PLUGINOFFSET(RasterGPU,p,off)->texture); return p; }
void invalidateRaster(Raster *r) { if(rasterOffset && gpu(r)->texture) { xdev_tex_free(gpu(r)->texture); gpu(r)->texture=0; } }
void registerPlatformPlugins() { rasterOffset = Raster::registerPlugin(sizeof(RasterGPU),0x02534C43,rasterCtor,rasterDtor,nil); }

static RasterGPU *upload(Raster *r)
{
 if(!r) return nil;
 RasterGPU *g=gpu(r);
 if(g->texture) return g;
 Image *img=r->toImage();
 if(!img) { xdev_log("raster conversion failed: %dx%d format %x",r->width,r->height,r->format); return nil; }
 img->unpalettize(); img->convertTo32();
 g->alpha=img->hasAlpha();
 g->texture=xdev_tex_create(XDEV_FMT_RGBA8,img->width,img->height,1);
 if(g->texture) xdev_tex_upload(g->texture,0,img->pixels,img->stride);
 img->destroy();
 return g->texture ? g : nil;
}

static void setState(int32 s,void *p)
{
 if(s<0 || s>GSALPHATESTREF) return;
 states[s]=(uintptr)p;
 if(s==TEXTUREADDRESS) states[TEXTUREADDRESSU]=states[TEXTUREADDRESSV]=(uintptr)p;
}
static void *getState(int32 s) { return s>=0 && s<=GSALPHATESTREF ? (void*)states[s] : nil; }
static void resetStates()
{
 std::memset(states,0,sizeof states);
 states[TEXTUREADDRESS]=states[TEXTUREADDRESSU]=states[TEXTUREADDRESSV]=Texture::WRAP;
 states[TEXTUREFILTER]=Texture::LINEAR;
 states[SRCBLEND]=BLENDSRCALPHA; states[DESTBLEND]=BLENDINVSRCALPHA;
 states[CULLMODE]=CULLNONE; states[STENCILFUNCTION]=STENCILALWAYS;
 states[STENCILFAIL]=states[STENCILZFAIL]=states[STENCILPASS]=STENCILKEEP;
 states[STENCILFUNCTIONMASK]=states[STENCILFUNCTIONWRITEMASK]=255;
}
static XDevState drawState()
{
 XDevState s={};
 s.color_write=1; s.src_blend=states[SRCBLEND]; s.dst_blend=states[DESTBLEND];
 s.depth_test=states[ZTESTENABLE]; s.depth_write=states[ZWRITEENABLE]; s.depth_func=4;
 s.cull=states[CULLMODE]==CULLBACK ? 3 : states[CULLMODE]==CULLFRONT ? 2 : 1;
 s.stencil=states[STENCILENABLE]; s.stencil_func=states[STENCILFUNCTION];
 s.stencil_fail=states[STENCILFAIL]; s.stencil_zfail=states[STENCILZFAIL]; s.stencil_pass=states[STENCILPASS];
 s.stencil_ref=states[STENCILFUNCTIONREF]; s.stencil_mask=states[STENCILFUNCTIONMASK]; s.stencil_write_mask=states[STENCILFUNCTIONWRITEMASK];
 s.vs[0][0]=2.0f/1280; s.vs[0][1]=-2.0f/720;
 s.vs[0][2]=-1.0f-1.0f/1280; s.vs[0][3]=1.0f+1.0f/720;
 s.ps[5][2]=s.ps[12][2]=s.ps[19][2]=s.ps[26][2]=1;
 RasterGPU *g=upload((Raster*)states[TEXTURERASTER]);
 s.blend=states[VERTEXALPHA] || (g && g->alpha);
 if(g) {
  s.samplers[0].tex=g->texture;
  s.ps[0][2]=s.ps[2][0]=s.ps[4][2]=s.ps[5][3]=1;
  s.ps[7][2]=s.ps[9][0]=s.ps[11][2]=1;
 }
 unsigned filter=states[TEXTUREFILTER];
 s.samplers[0].min_linear=s.samplers[0].mag_linear=(filter==Texture::LINEAR || filter==Texture::MIPLINEAR || filter==Texture::LINEARMIPLINEAR);
 s.samplers[0].clamp_u=states[TEXTUREADDRESSU]==Texture::CLAMP;
 s.samplers[0].clamp_v=states[TEXTUREADDRESSV]==Texture::CLAMP;
 s.ps[29][3]=(states[ALPHATESTREF]&255)/255.0f;
 if(states[ALPHATESTFUNC]==ALPHAGREATEREQUAL) s.ps[30][1]=1;
 else if(states[ALPHATESTFUNC]==ALPHALESS) s.ps[30][0]=-1;
 else s.ps[30][2]=1;
 return s;
}
static uint32 primitive(PrimitiveType p)
{
 switch(p) {
 case PRIMTYPELINELIST:return XDEV_PRIM_LINES; case PRIMTYPEPOLYLINE:return XDEV_PRIM_LINE_STRIP;
 case PRIMTYPETRILIST:return XDEV_PRIM_TRIANGLES; case PRIMTYPETRISTRIP:return XDEV_PRIM_TRIANGLE_STRIP;
 case PRIMTYPETRIFAN:return XDEV_PRIM_TRIANGLE_FAN; case PRIMTYPEPOINTLIST:return XDEV_PRIM_POINTS;
 default:return 0;
 }
}
static void im2D(PrimitiveType p,void *v,int32 n,void *ix,int32 ni)
{
 if(n<=0 || !v || !primitive(p) || (ix && ni<=0)) return;
 if(ix) for(int32 i=0;i<ni;++i) if(((uint16*)ix)[i]>=n) { xdev_log("invalid Im2D index"); return; }
 Im2DVertex *in=(Im2DVertex*)v; vertices.resize(n);
 for(int32 i=0;i<n;++i) {
  XDevVertex &o=vertices[i]; const Im2DVertex &a=in[i];
  o={a.x,a.y,a.z,a.w>0 ? 1.0f/a.w : 1.0f,
   (uint32(a.a)<<24)|(uint32(a.b)<<16)|(uint32(a.g)<<8)|a.r,0,a.u,a.v,0,0};
 }
 XDevState s=drawState(); xdev_draw(&s,primitive(p),vertices.data(),n,(uint16*)ix,ni);
}
static void im2DPlain(PrimitiveType p,void *v,int32 n) { im2D(p,v,n,nil,0); }
static void im2DLine(void *v,int32 n,int32 a,int32 b) { if(a<0||b<0||a>=n||b>=n)return; uint16 ix[]={uint16(a),uint16(b)}; im2D(PRIMTYPELINELIST,v,n,ix,2); }
static void im2DTriangle(void *v,int32 n,int32 a,int32 b,int32 c) { if(a<0||b<0||c<0||a>=n||b>=n||c>=n)return; uint16 ix[]={uint16(a),uint16(b),uint16(c)}; im2D(PRIMTYPETRILIST,v,n,ix,3); }
static bool32 rasterFast(Raster *r,int32 x,int32 y)
{
 Im2DVertex v[4]={};
 const float xy[4][2]={{float(x),float(y)},{float(x),float(y+r->height)},{float(x+r->width),float(y)},{float(x+r->width),float(y+r->height)}};
 for(int i=0;i<4;++i) { v[i].x=xy[i][0]; v[i].y=xy[i][1]; v[i].w=1; v[i].r=v[i].g=v[i].b=v[i].a=255; v[i].u=i/2; v[i].v=i%2; }
 uintptr old=states[TEXTURERASTER]; states[TEXTURERASTER]=(uintptr)r;
 im2DPlain(PRIMTYPETRISTRIP,v,4); states[TEXTURERASTER]=old; return 1;
}
static Raster *rasterCreate(Raster *r)
{
 if(r->type==Raster::CAMERA || r->type==Raster::ZBUFFER) {
  r->originalWidth=r->width; r->originalHeight=r->height; r->originalStride=r->stride=0; r->originalPixels=nil; return r;
 }
 return cpuRasterCreate(r);
}
static bool clipMatrix(XDevState &s,const Matrix *world)
{
 Camera *cam=engine->currentCamera;
 if(!cam) return false;
 Matrix m=cam->viewMatrix;
 if(world) Matrix::mult(&m,world,&cam->viewMatrix);
 const V3d basis[]={m.right,m.up,m.at,m.pos};
 const bool perspective=cam->projection==Camera::PERSPECTIVE;
 for(int j=0;j<4;++j) {
  float unit=j==3?1.0f:0.0f, w=perspective?basis[j].z:unit;
  s.vs[2][j]=2*basis[j].x-w;
  s.vs[3][j]=w-2*basis[j].y;
  s.vs[4][j]=perspective ? cam->zShift*basis[j].z+cam->zScale*unit : cam->zScale*basis[j].z+cam->zShift*unit;
  s.vs[5][j]=w;
 }
 s.vs[6][0]=1;
 if(states[FOGENABLE]) {
  s.vs[1][0]=cam->fogPlane; s.vs[1][1]=cam->farPlane; s.vs[1][2]=1;
  uint32 col=states[FOGCOLOR];
  s.ps[29][0]=((col>>0)&255)/255.0f; s.ps[29][1]=((col>>8)&255)/255.0f; s.ps[29][2]=((col>>16)&255)/255.0f;
 }
 return true;
}
static void begin(Camera*) { lastCharacter=nil; }
static void end(Camera*) {}
static void clear(Camera*,RGBA *c,uint32 mode) { xdev_clear((uint32(c->alpha)<<24)|(uint32(c->red)<<16)|(uint32(c->green)<<8)|c->blue,mode&1,mode&2); }
static void show(Raster*,uint32) { xdev_present(); }
static void im3DTransform(void *data,int32 n,Matrix *world,uint32 flags)
{
 immediate3D.clear();
 if(!data || n<=0) return;
 if(world) immediateWorld=*world; else immediateWorld.setIdentity();
 Im3DVertex *v=(Im3DVertex*)data;
 immediate3D.resize(n);
 for(int32 i=0;i<n;++i) {
  uint8 alpha=flags&im3d::ALLOPAQUE?255:v[i].a;
  immediate3D[i]={v[i].position.x,v[i].position.y,v[i].position.z,1,
   (uint32(alpha)<<24)|(uint32(v[i].b)<<16)|(uint32(v[i].g)<<8)|v[i].r,0,v[i].u,v[i].v,0,0};
 }
}
static void im3DIndexed(PrimitiveType p,void *ix,int32 n)
{
 if(immediate3D.empty() || !primitive(p) || (ix && n<=0)) return;
 if(ix) for(int32 i=0;i<n;++i) if(((uint16*)ix)[i]>=immediate3D.size()) return;
 XDevState s=drawState(); if(!clipMatrix(s,&immediateWorld)) return;
 xdev_draw(&s,primitive(p),immediate3D.data(),immediate3D.size(),(uint16*)ix,n);
}
static void im3DPlain(PrimitiveType p) { im3DIndexed(p,nil,0); }
static void im3DEnd() { immediate3D.clear(); }
// CPU skinning uses the same inverse-bind * hierarchy * inverse-atomic
// convention as librw's GL/D3D skin pipelines. Transform once per atomic,
// rather than once per material; retain the original geometry and bind pose.
static std::vector<Matrix> boneMatrices;
static std::vector<V3d> objectPositions, objectNormals;
static V3d normalise(V3d n) {
 float l=dot(n,n); return l>1.0e-16f ? scale(n,1.0f/std::sqrt(l)) : V3d{0,0,0};
}
static bool deform(Atomic *a) {
 Geometry *g=a->geometry;
 const V3d *pos=g->morphTargets[0].vertices,*norm=g->morphTargets[0].normals;
 objectPositions.assign(pos,pos+g->numVertices);
 objectNormals.resize(g->numVertices);
 for(int i=0;i<g->numVertices;++i) objectNormals[i]=norm?norm[i]:V3d{0,0,0};
 Skin *skin=skinGlobals.geoOffset?Skin::get(g):nil;
 HAnimHierarchy *hier=skin && skinGlobals.atomicOffset?Skin::getHierarchy(a):nil;
 if(!skin || !hier) return false;
 if(skin->numBones==53) lastCharacter=a;
 static unsigned skinTraces; bool trace=skinTraces++<6;
 if(trace) xdev_log("skin begin bones=%d nodes=%d flags=%x verts=%d",skin->numBones,hier->numNodes,hier->flags,g->numVertices);
 if(!hier->matrices || skin->numBones<=0 || skin->numBones>hier->numNodes ||
    !skin->weights || !skin->indices || !skin->inverseMatrices) return false;
 boneMatrices.resize(skin->numBones);
 Matrix inverseAtomic; Matrix::invert(&inverseAtomic,a->getFrame()->getLTM());
 for(int i=0;i<skin->numBones;++i) {
  Matrix bind=((Matrix*)skin->inverseMatrices)[i], pose=hier->matrices[i], tmp;
  bind.flags=0; pose.flags=0;
  if(!(hier->flags&HAnimHierarchy::LOCALSPACEMATRICES)) {
   Matrix::mult(&tmp,&pose,&inverseAtomic); pose=tmp;
  }
  Matrix::mult(&boneMatrices[i],&bind,&pose);
 }
 for(int i=0;i<g->numVertices;++i) {
  V3d p={0,0,0},n={0,0,0}; float total=0;
  for(int j=0;j<4;++j) {
   float w=skin->weights[i*4+j]; unsigned bone=skin->indices[i*4+j];
   if(w==0) continue;
   if(!std::isfinite(w) || w<0 || bone>=boneMatrices.size()) return false;
   V3d tp,tn; V3d::transformPoints(&tp,&pos[i],1,&boneMatrices[bone]);
   V3d::transformVectors(&tn,&objectNormals[i],1,&boneMatrices[bone]);
   p=add(p,scale(tp,w)); n=add(n,scale(tn,w)); total+=w;
  }
  if(total>1.0e-6f) { objectPositions[i]=scale(p,1.0f/total); objectNormals[i]=normalise(n); }
 }
 if(trace) xdev_log("skin done p0=%f,%f,%f",objectPositions[0].x,objectPositions[0].y,objectPositions[0].z);
 return true;
}
void dumpSkinPose(const char *path) {
 Atomic *a=lastCharacter; if(!a || !deform(a)) return;
 Skin *skin=Skin::get(a->geometry); HAnimHierarchy *h=Skin::getHierarchy(a);
 FILE *f=std::fopen(path,"w"); if(!f) return;
 auto matrix=[&](const Matrix &m) { fprintf(f,"[%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g]",m.right.x,m.right.y,m.right.z,m.up.x,m.up.y,m.up.z,m.at.x,m.at.y,m.at.z,m.pos.x,m.pos.y,m.pos.z); };
 fprintf(f,"{\"atomic\":"); matrix(*a->getFrame()->getLTM());
 fprintf(f,",\"flags\":%d,\"bones\":[",h->flags);
 for(int i=0;i<skin->numBones;++i) {
  if(i) fputc(',',f); HAnimInterpFrame *in=(HAnimInterpFrame*)h->interpolator->getInterpFrame(i);
  fprintf(f,"{\"id\":%d,\"flags\":%d,\"q\":[%.9g,%.9g,%.9g,%.9g],\"t\":[%.9g,%.9g,%.9g],\"bind\":",h->nodeInfo[i].id,h->nodeInfo[i].flags,in->q.x,in->q.y,in->q.z,in->q.w,in->t.x,in->t.y,in->t.z);
  matrix(((Matrix*)skin->inverseMatrices)[i]); fprintf(f,",\"pose\":"); matrix(h->matrices[i]); fprintf(f,",\"deform\":"); matrix(boneMatrices[i]); fputc('}',f);
 }
 fprintf(f,"]}\n"); fclose(f);
}
struct ObjectLights {
 WorldLights data; Light *directionals[8],*locals[8];
 V3d directionalAxes[8],localAxes[8];
 ObjectLights(Atomic *a) {
  std::memset(&data,0,sizeof data);
  if(!a || !(a->geometry->flags&Geometry::LIGHT) || !engine->currentWorld) return;
  data.directionals=directionals; data.locals=locals; data.numDirectionals=data.numLocals=8;
  ((World*)engine->currentWorld)->enumerateLights(a,&data);
  // Light frames cannot change during this atomic's vertex loop. Normalise
  // their axes once here instead of taking a square root per vertex/light.
  for(int i=0;i<data.numDirectionals;++i)
   if(directionals[i]->getFrame()) directionalAxes[i]=normalise(directionals[i]->getFrame()->getLTM()->at);
  for(int i=0;i<data.numLocals;++i)
   if(locals[i]->getFrame() && locals[i]->getType()!=Light::POINT) localAxes[i]=normalise(locals[i]->getFrame()->getLTM()->at);
 }
 V3d diffuse(V3d p,V3d n) const {
  V3d color={0,0,0};
  for(int i=0;i<data.numDirectionals;++i) {
   Light *l=data.directionals[i]; if(!l->getFrame()) continue;
   float amount=std::fmax(0.0f,-dot(n,directionalAxes[i]));
   color=add(color,scale(V3d{l->color.red,l->color.green,l->color.blue},amount));
  }
  for(int i=0;i<data.numLocals;++i) {
   Light *l=data.locals[i]; if(!l->getFrame() || l->radius<=0) continue;
   Matrix *m=l->getFrame()->getLTM(); V3d dir=sub(p,m->pos); float dist=std::sqrt(dot(dir,dir));
   if(dist<1.0e-6f || dist>=l->radius) continue;
   dir=scale(dir,1.0f/dist);
   float amount=std::fmax(0.0f,-dot(n,dir))*(1.0f-dist/l->radius);
   if(l->getType()!=Light::POINT) {
    float cosine=-l->minusCosAngle, cone=dot(dir,localAxes[i]);
    if(cone<cosine) continue;
    if(l->getType()==Light::SOFTSPOT) amount*=std::fmax(0.0f,(cone-cosine)/std::fmax(1.0e-6f,1.0f-cosine));
   }
   color=add(color,scale(V3d{l->color.red,l->color.green,l->color.blue},amount));
  }
  return color;
 }
};
static uint8 colourByte(float v) { if(!(v>0)) return 0; if(v>=1) return 255; return uint8(v*255.0f+0.5f); }
static uint8 worldColour[3][256];
static void prepareWorldColour(const WorldStyle &s) {
 // Time-of-day colour is shared by thousands of world vertices. Evaluate the
 // original shader's separable RGB transfer only when those uniforms change.
 static float previous[9]; static bool valid;
 float key[]={s.ambient.red,s.ambient.green,s.ambient.blue,s.emissive.red,s.emissive.green,s.emissive.blue,s.emissiveScale,s.colourScale,s.mobile?1.0f:0.0f};
 if(valid && !std::memcmp(previous,key,sizeof key)) return;
 std::memcpy(previous,key,sizeof key); valid=true;
 for(int channel=0;channel<3;++channel) {
  float amb=key[channel],em=key[channel+3];
  if(s.mobile) { amb=std::fmax(0.0f,(amb-0.5f)*1.2f+0.6f); em=std::fmax(0.0f,(em-0.5f)*1.25f+0.55f); }
  for(int i=0;i<256;++i) {
   float c=i/255.0f;
   if(s.mobile) c=std::fmax(0.0f,(c-0.5f)*1.5f+0.75f);
   worldColour[channel][i]=colourByte(c*amb+em*s.emissiveScale);
  }
 }
}
static bool renderObjectInternal(Atomic *a,const WorldStyle *style)
{
 Geometry *g=a->geometry;
 if(!g || !g->morphTargets || !g->morphTargets[0].vertices || !g->triangles) {
  xdev_log("object needs native geometry conversion"); return false;
 }
 if(!g->meshHeader) g->buildMeshes();
 if(!g->meshHeader) return false;
 uintptr saved[GSALPHATESTREF+1]; std::memcpy(saved,states,sizeof states);
 const V3d *positions=g->morphTargets[0].vertices,*normals=g->morphTargets[0].normals;
 if(skinGlobals.geoOffset && Skin::get(g) && deform(a)) {
  positions=objectPositions.data(); normals=objectNormals.data();
 }
 XDevState objectClip={};
 if(!clipMatrix(objectClip,a->getFrame()->getLTM()))return false;
 ObjectLights lights(style?nil:a);
 if(style) prepareWorldColour(*style);
 static std::vector<V3d> diffuse;
 diffuse.resize(g->numVertices);
 bool dynamic=!style && (lights.data.numDirectionals || lights.data.numLocals) && normals;
 if(dynamic) {
  Matrix *world=a->getFrame()->getLTM(),inverseWorld;
  Matrix::invert(&inverseWorld,world);
  for(int i=0;i<g->numVertices;++i) {
   const V3d &n=normals[i];
   V3d normal=normalise(V3d{dot(n,inverseWorld.right),dot(n,inverseWorld.up),dot(n,inverseWorld.at)}),pos={0,0,0};
   if(lights.data.numLocals) V3d::transformPoints(&pos,&positions[i],1,world);
   diffuse[i]=lights.diffuse(pos,normal);
  }
 }
 bool deferred=false;
 Mesh *meshes=g->meshHeader->getMeshes();
 for(uint32 j=0;j<g->meshHeader->numMeshes;++j) {
  Mesh &mesh=meshes[j]; Material *mat=mesh.material;
  if(!mat || !mesh.indices || !mesh.numIndices) continue;
  bool vertexAlpha=mat->color.alpha!=255;
  uint32 first=g->numVertices,last=0;
  for(uint32 k=0;k<mesh.numIndices;++k) {
   if(mesh.indices[k]>=g->numVertices) { xdev_log("invalid object mesh index"); std::memcpy(states,saved,sizeof states); return false; }
   if(mesh.indices[k]<first)first=mesh.indices[k];
   if(mesh.indices[k]>last)last=mesh.indices[k];
   if((g->flags&Geometry::PRELIT) && g->colors && g->colors[mesh.indices[k]].alpha!=255) vertexAlpha=true;
  }
  if(style) {
   if(!mat->texture) continue; // Matches the original world pipeline's proxy/occluder rule.
   RasterGPU *texture=upload(mat->texture->raster);
   bool transparent=vertexAlpha || (texture && texture->alpha) || style->fade!=255;
   if(style->phase==0 && transparent) { deferred=true; continue; }
   if(style->phase==1 && !transparent) continue;
  }
  // Each material only consumes this index span. Do not convert/upload the
  // whole atomic for every material. Preserve index order (including strips).
  vertices.resize(last-first+1);
  const uint16 *drawIndices=mesh.indices;
  if(first) {
   meshIndices.resize(mesh.numIndices);
   for(uint32 k=0;k<mesh.numIndices;++k)meshIndices[k]=mesh.indices[k]-first;
   drawIndices=meshIndices.data();
  }
  xdev_mesh_span(g->numVertices,vertices.size());
  for(uint32 i=first;i<=last;++i) {
   const V3d &p=positions[i];
   RGBA c=(g->flags&Geometry::PRELIT) && g->colors?g->colors[i]:
    (g->flags&Geometry::LIGHT)?makeRGBA(0,0,0,255):makeRGBA(255,255,255,255);
   if(style) {
    c.red=worldColour[0][c.red]; c.green=worldColour[1][c.green]; c.blue=worldColour[2][c.blue];
    c.alpha=uint32(c.alpha)*style->fade/255;
   } else if(g->flags&Geometry::LIGHT) {
    const RGBAf &amb=lights.data.ambient;
    c.red=colourByte(c.red/255.0f+amb.red*mat->surfaceProps.ambient+(dynamic?diffuse[i].x:0)*mat->surfaceProps.diffuse);
    c.green=colourByte(c.green/255.0f+amb.green*mat->surfaceProps.ambient+(dynamic?diffuse[i].y:0)*mat->surfaceProps.diffuse);
    c.blue=colourByte(c.blue/255.0f+amb.blue*mat->surfaceProps.ambient+(dynamic?diffuse[i].z:0)*mat->surfaceProps.diffuse);
   }
   if(!style && (g->flags&Geometry::MODULATE)) { c.red=uint32(c.red)*mat->color.red/255; c.green=uint32(c.green)*mat->color.green/255; c.blue=uint32(c.blue)*mat->color.blue/255; }
   c.alpha=uint32(c.alpha)*mat->color.alpha/255;
   TexCoords uv=g->texCoords[0]?g->texCoords[0][i]:TexCoords{0,0};
   vertices[i-first]={p.x,p.y,p.z,1,(uint32(c.alpha)<<24)|(uint32(c.blue)<<16)|(uint32(c.green)<<8)|c.red,0,uv.u,uv.v,0,0};
  }
  Texture *tex=mat->texture; states[TEXTURERASTER]=(uintptr)(tex?tex->raster:nil);
  if(tex) { states[TEXTUREFILTER]=tex->getFilter(); states[TEXTUREADDRESSU]=tex->getAddressU(); states[TEXTUREADDRESSV]=tex->getAddressV(); }
  states[VERTEXALPHA]=vertexAlpha || (style && style->fade!=255);
  XDevState s=drawState(); if(style) s.ps[5][2]=style->colourScale;
  // Projection and fog are invariant across this atomic's material loop.
  std::memcpy(s.vs[1],objectClip.vs[1],sizeof(float)*6*4);
  std::memcpy(s.ps[29],objectClip.ps[29],sizeof(float)*3);
  uint32 prim=g->meshHeader->flags&MeshHeader::TRISTRIP?XDEV_PRIM_TRIANGLE_STRIP:XDEV_PRIM_TRIANGLES;
  bool dual=false;XDevState firstPass;
  if(states[GSALPHATEST] && s.blend) {
   // Match RW's PS2 FB_ONLY emulation: low-alpha pixels never write depth.
   std::memset(s.ps[30],0,sizeof s.ps[30]);
   if(s.depth_write) {
    s.ps[29][3]=(states[GSALPHATESTREF]&255)/255.0f; s.ps[30][1]=1;
    firstPass=s;dual=true;
    s.depth_write=0; s.ps[30][1]=0; s.ps[30][0]=-1;
   } else s.ps[30][2]=1;
  }
  if(dual)xdev_draw_dual(&firstPass,&s,prim,vertices.data(),vertices.size(),drawIndices,mesh.numIndices);
  else xdev_draw(&s,prim,vertices.data(),vertices.size(),drawIndices,mesh.numIndices);
 }
 std::memcpy(states,saved,sizeof states);
 return deferred;
}
static void renderObject(ObjPipeline*,Atomic *a) { renderObjectInternal(a,nil); }
bool renderWorldObject(Atomic *a,const WorldStyle &style) { return renderObjectInternal(a,&style); }
static int system(DeviceReq req,void *arg,int32 n)
{
 switch(req) {
 case DEVICEOPEN:return 1;
 case DEVICEINIT:{ uint32 w,h; resetStates(); return xdev_open(&w,&h); }
 case DEVICEFINALIZE:
  cpuRasterCreate=engine->driver[PLATFORM_D3D9]->rasterCreate; engine->driver[PLATFORM_D3D9]->rasterCreate=rasterCreate;
  objectPipeline=ObjPipeline::create(); objectPipeline->platform=PLATFORM_D3D9; objectPipeline->impl.render=renderObject;
  engine->driver[PLATFORM_D3D9]->defaultPipeline=objectPipeline;
  if(skinGlobals.geoOffset) skinGlobals.pipelines[PLATFORM_D3D9]=objectPipeline;
  if(matFXGlobals.materialOffset) matFXGlobals.pipelines[PLATFORM_D3D9]=objectPipeline;
  return 1;
 case DEVICETERM: vertices.clear(); immediate3D.clear(); if(objectPipeline) objectPipeline->destroy(); objectPipeline=nil; xdev_close(); return 1;
 case DEVICECLOSE:return 1;
 case DEVICEGETNUMSUBSYSTEMS:case DEVICEGETNUMVIDEOMODES:return 1;
 case DEVICEGETCURRENTSUBSYSTEM:case DEVICEGETCURRENTVIDEOMODE:return 0;
 case DEVICESETSUBSYSTEM:case DEVICESETVIDEOMODE:return n==0;
 case DEVICEGETSUBSSYSTEMINFO: if(n!=0||!arg)return 0; std::strcpy(((SubSystemInfo*)arg)->name,"XSDK Xenos"); return 1;
 case DEVICEGETVIDEOMODEINFO: if(n!=0||!arg)return 0; *(VideoMode*)arg={1280,720,32,VIDEOMODEEXCLUSIVE}; return 1;
 case DEVICEGETMAXMULTISAMPLINGLEVELS:case DEVICEGETMULTISAMPLINGLEVELS:return 1;
 case DEVICESETMULTISAMPLINGLEVELS:return n<=1;
 default:return 0;
 }
}
Device renderdevice={0,1,begin,end,clear,show,rasterFast,setState,getState,
 im2DLine,im2DTriangle,im2DPlain,im2D,im3DTransform,im3DPlain,im3DIndexed,im3DEnd,system};
} }
