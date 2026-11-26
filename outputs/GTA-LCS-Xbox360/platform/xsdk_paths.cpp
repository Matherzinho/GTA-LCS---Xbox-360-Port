#include "xsdk_paths.h"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

static bool sep(char c) { return c=='/' || c=='\\'; }
int lcs_xsdk_normalize_path(const char *path,char *out,size_t capacity)
{
 if(!path || !out || !capacity) { errno=EINVAL; return -1; }
 if(!*path) { errno=ENOENT; return -1; }
 char input[2048],base[1024];
 size_t len=std::strlen(path),prefix=0;
 if(len>=sizeof input) { errno=ENAMETOOLONG; return -1; }
 const char *colon=std::strchr(path,':');
 const char *first=path; while(*first && !sep(*first)) ++first;
 if((colon && colon<first) || sep(*path)) std::memcpy(input,path,len+1);
 else {
  if(!getcwd(base,sizeof base)) return -1;
  size_t b=std::strlen(base);
  if(b+1+len>=sizeof input) { errno=ENAMETOOLONG; return -1; }
  std::memcpy(input,base,b); input[b]='\\'; std::memcpy(input+b+1,path,len+1);
 }
 char result[1024]; size_t length=0,root=0;
 first=input; while(*first && !sep(*first)) ++first;
 colon=std::strchr(input,':');
 if(colon && colon<first) {
  prefix=size_t(colon-input)+1;
  if(prefix+1>=sizeof result) { errno=ENAMETOOLONG; return -1; }
  std::memcpy(result,input,prefix); length=prefix;
 }
 result[length++]='\\'; root=length;
 const char *p=input+prefix;
 while(*p) {
  while(sep(*p)) ++p;
  const char *end=p; while(*end && !sep(*end)) ++end;
  size_t n=size_t(end-p);
  if(n==2 && p[0]=='.' && p[1]=='.') {
   while(length>root && result[length-1]!='\\') --length;
   if(length>root) --length;
  } else if(n && !(n==1 && *p=='.')) {
   size_t slash=length>root?1:0;
   if(length+slash+n>=sizeof result) { errno=ENAMETOOLONG; return -1; }
   if(slash) result[length++]='\\';
   std::memcpy(result+length,p,n); length+=n;
  }
  p=end;
 }
 if(length>=capacity) { errno=ENAMETOOLONG; return -1; }
 result[length]=0; std::memcpy(out,result,length+1); return 0;
}

extern "C" int access(const char *path,int mode)
{
 if(mode&~(R_OK|W_OK|X_OK)) { errno=EINVAL; return -1; }
 struct stat st; if(stat(path,&st)) return -1;
 if(((mode&R_OK) && !(st.st_mode&S_IRUSR)) || ((mode&W_OK) && !(st.st_mode&S_IWUSR)) || ((mode&X_OK) && !(st.st_mode&S_IXUSR))) { errno=EACCES; return -1; }
 return 0;
}
extern "C" char *realpath(const char *path,char *out)
{
 char resolved[1024]; if(lcs_xsdk_normalize_path(path,resolved,sizeof resolved)) return nullptr;
 struct stat st; if(stat(resolved,&st)) return nullptr;
 if(!out) { out=(char*)std::malloc(1024); if(!out) { errno=ENOMEM; return nullptr; } }
 std::strcpy(out,resolved); return out;
}
const char *_psGetUserFilesFolder() { return "game:\\userfiles"; }
