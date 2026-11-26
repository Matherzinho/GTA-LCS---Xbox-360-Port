#pragma once
#include <stdint.h>
static inline uint32_t lcs_load_u32le(const void *p) {
 const uint8_t *b=(const uint8_t*)p;
 return (uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);
}
static inline void lcs_store_u32le(void *p,uint32_t n) {
 uint8_t *b=(uint8_t*)p; b[0]=(uint8_t)n;b[1]=(uint8_t)(n>>8);b[2]=(uint8_t)(n>>16);b[3]=(uint8_t)(n>>24);
}