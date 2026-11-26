#pragma once
#include <stdint.h>
#include <string.h>
// Scalar little-endian values copied from disk into native scalar objects.
static inline uint32_t LittleU32(uint32_t x) {
#ifdef BIGENDIAN
 return __builtin_bswap32(x);
#else
 return x;
#endif
}
static inline int32_t LittleI32(int32_t x) { return (int32_t)LittleU32((uint32_t)x); }
static inline int16_t LittleI16(int16_t x) {
#ifdef BIGENDIAN
 return (int16_t)__builtin_bswap16((uint16_t)x);
#else
 return x;
#endif
}
static inline float LittleF32(float x) { uint32_t u; memcpy(&u,&x,4); u=LittleU32(u); memcpy(&x,&u,4); return x; }
static inline void LittleSwap32(void *data,int count) {
#ifdef BIGENDIAN
 unsigned char *p=(unsigned char*)data;
 for(int i=0;i<count;++i,p+=4) { uint32_t u; memcpy(&u,p,4); u=LittleU32(u); memcpy(p,&u,4); }
#endif
}
