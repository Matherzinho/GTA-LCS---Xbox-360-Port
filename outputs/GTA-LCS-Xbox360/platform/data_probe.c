#include <xsdk/log.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "lcs_endian.h"
static long file_size(const char *p){FILE*f=fopen(p,"rb");if(!f)return -1;fseek(f,0,SEEK_END);long n=ftell(f);fclose(f);return n;}
int main(void){
 xsdk_log_write("LCS XSDK internal data probe; no game renderer or gameplay");
 if(sizeof(void*)!=4 || sizeof(uint32_t)!=4){xsdk_log_write("FAIL: native ABI");return 1;}
 unsigned char test[]={0x78,0x56,0x34,0x12};
 if(lcs_load_u32le(test)!=0x12345678u){xsdk_log_write("FAIL: little endian reader");return 2;}
 unsigned char encoded[4]={};lcs_store_u32le(encoded,0x12345678u);
 if(memcmp(test,encoded,4)){xsdk_log_write("FAIL: little endian writer");return 3;}
 long image_size=file_size("data/models/gta3.img");
 FILE *f=fopen("data/models/gta3.dir","rb");if(!f||image_size<=0){xsdk_log_write("FAIL: model files unavailable");return 4;}
 unsigned char entry[32];unsigned entries=0;size_t got;
 while((got=fread(entry,1,sizeof entry,f))==sizeof entry){
  uint64_t start=(uint64_t)lcs_load_u32le(entry)*2048u;
  uint64_t end=start+(uint64_t)lcs_load_u32le(entry+4)*2048u;
  if(start>=(uint64_t)image_size || end>(uint64_t)image_size+2047u){fclose(f);xsdk_log_write("FAIL: model directory range");return 5;}
  entries++;
 }
 int bad=ferror(f)||got;fclose(f);if(bad||!entries){xsdk_log_write("FAIL: incomplete model directory");return 6;}
 f=fopen("data/models/hud.txd","rb");if(!f||fread(entry,1,12,f)!=12){xsdk_log_write("FAIL: HUD dictionary unavailable");return 7;}fclose(f);
 if(lcs_load_u32le(entry)!=0x16u){xsdk_log_write("FAIL: HUD dictionary byte order");return 8;}
 long bank_size=file_size("data/AUDIO/sfx.RAW");f=fopen("data/AUDIO/sfx.SDT","rb");
 if(!f||bank_size<=0){xsdk_log_write("FAIL: audio files unavailable");return 9;}
 unsigned samples=0;
 while((got=fread(entry,1,20,f))==20){
  uint64_t end=(uint64_t)lcs_load_u32le(entry)+lcs_load_u32le(entry+4);
  if(end>(uint64_t)bank_size){fclose(f);xsdk_log_write("FAIL: audio bank range");return 10;}samples++;
 }
 bad=ferror(f)||got;fclose(f);if(bad||!samples){xsdk_log_write("FAIL: incomplete audio table");return 11;}
 xsdk_log_write("LCS XSDK DATA PROBE PASS: %u model entries, %u audio samples, HUD dictionary, 32-bit ABI and LE read/write",entries,samples);
 return 0;
}