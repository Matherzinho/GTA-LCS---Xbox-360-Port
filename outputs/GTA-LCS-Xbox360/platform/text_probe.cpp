// Runs the production GXT byte-order readers on all supplied English tables.
#include "common.h"
#include "FileMgr.h"
#include "Text.h"
#include <cstring>
#include <unistd.h>
extern "C" void xsdk_log_write(const char*,...);
#define CHECK(c) do { if(!(c)) { xsdk_log_write("LCS TEXT FAIL line %d: %s",__LINE__,#c); return 1; } } while(0)
static bool table(CText &parser,int fd,CKeyArray &keys,CData &data) {
 bool key=false,dat=false; size_t offset=0;
 for(int n=0;n<16 && (!key || !dat);++n) {
  ChunkHeader header; parser.ReadChunkHeader(&header,fd,&offset);
  if(header.size<0) return false;
  if(!std::memcmp(header.magic,"TKEY",4)) { keys.Load(header.size,fd,&offset); key=true; }
  else if(!std::memcmp(header.magic,"TDAT",4)) { data.Load(header.size,fd,&offset); dat=true; }
  else if(CFileMgr::Seek(fd,header.size,SEEK_CUR)) return false;
 }
 if(!key || !dat || keys.numEntries<=0 || data.numChars<=0) return false;
 for(int i=0;i<keys.numEntries;++i) {
  uint32 off=keys.entries[i].valueOffset;
  if((off&1) || off/2>=uint32(data.numChars)) return false;
  int j=off/2; while(j<data.numChars && data.chars[j]) ++j;
  if(j==data.numChars) return false;
  if(i && std::strncmp(keys.entries[i-1].key,keys.entries[i].key,8)>0) return false;
 }
 char first[9]={}; std::memcpy(first,keys.entries[0].key,8); uint8 result=0;
 wchar *text=keys.Search(first,data.chars,&result);
 return result && text==(wchar*)((uint8*)data.chars+keys.entries[0].valueOffset);
}
int main() {
 CHECK(chdir("game:\\data")==0); CFileMgr::Initialise();
 int fd=CFileMgr::OpenFile("TEXT\\AMERICAN.GXT"); CHECK(fd>0);
 CText parser; CHECK(!parser.IsLoaded());
 size_t offset=0; ChunkHeader h; parser.ReadChunkHeader(&h,fd,&offset);
 CHECK(!std::memcmp(h.magic,"TABL",4)); CMissionTextOffsets missions;
 missions.Load(h.size,fd,&offset,0); CHECK(missions.size==91 && missions.data[0].offset==1100);
 CKeyArray keys; CData data; CHECK(table(parser,fd,keys,data)); CHECK(keys.numEntries==2745);
 uint8 found=0; wchar *stats=keys.Search("FEP_STA",data.chars,&found);
 const wchar expected[]={'S','t','a','t','s',0}; CHECK(found && std::memcmp(stats,expected,sizeof expected)==0);
 int total=keys.numEntries;
 for(int i=1;i<missions.size;++i) {
  CHECK(!CFileMgr::Seek(fd,missions.data[i].offset,SEEK_SET));
  char name[8]; CHECK(CFileMgr::Read(fd,name,8)==8 && !std::memcmp(name,missions.data[i].szMissionName,8));
  CHECK(table(parser,fd,keys,data)); total+=keys.numEntries;
 }
 CHECK(CFileMgr::CloseFile(fd)==0);
 // A truncated chunk header must terminate parsing, not leave a stale length.
 int broken=CFileMgr::OpenFileForWriting("game:\\userfiles\\lcs-text-truncated.gxt"); CHECK(broken>0);
 CHECK(CFileMgr::Write(broken,"TKE",3)==3 && CFileMgr::CloseFile(broken)==0);
 broken=CFileMgr::OpenFile("game:\\userfiles\\lcs-text-truncated.gxt"); CHECK(broken>0);
 parser.ReadChunkHeader(&h,broken,&offset); CHECK(h.size==-1); CFileMgr::CloseFile(broken);
 xsdk_log_write("LCS TEXT PASS: 91 English GXT tables, %d text keys, UCS-2, offsets, menu lookup and truncated header",total);
 return 0;
}
