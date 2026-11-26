#pragma once
namespace rw { namespace xsdk {
extern Device renderdevice;
void dumpSkinPose(const char *path);
struct WorldStyle {
 RGBAf ambient, emissive;
 float emissiveScale, colourScale;
 int phase; // 0 opaque, 1 deferred alpha
 uint8 fade;
 bool mobile;
};
bool renderWorldObject(Atomic *atomic,const WorldStyle &style);
void registerPlatformPlugins();
void invalidateRaster(Raster *raster);
} }
