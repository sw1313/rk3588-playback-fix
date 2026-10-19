// SPDX-License-Identifier: MIT
// Calls the pinned actual AllocateEmpty method in this isolated process only.
#include "../../magisk-codec2/src/p010_pool.cpp"
#include <cstdlib>
int main(int argc,char**argv){
 if(argc!=3)return 2;void *lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!lib){fprintf(stderr,"%s\n",dlerror());return 3;}
 auto fn=reinterpret_cast<Empty>(dlsym(lib,emptyName));Dl_info info{};if(!fn || !dladdr(reinterpret_cast<void*>(fn),&info))return 3;
 const bool candidate=atoi(argv[2])!=0;if(candidate && !install(reinterpret_cast<uintptr_t>(info.dli_fbase)))return 4;
 originalEmpty=fn;unsigned cases=0;
 for(int format:{-1,19,21,43,54,0x7f420888})for(auto size:{std::pair<int,int>(1,1),{16,16},{17,15},{854,480},{1280,720},{3840,2160}}){
  android::sp<android::AMessage> message=new android::AMessage;message->setInt32("width",size.first);message->setInt32("height",size.second);if(format!=-1)message->setInt32("color-format",format);
  size_t bytes=0;unsigned calls=0;Alloc alloc=[&](size_t n){bytes=n;calls++;return android::sp<android::ABuffer>(new android::ABuffer(1));};
  {auto result=candidate?poolEmpty(message,std::move(alloc)):fn(message,std::move(alloc));if(!result)return 5;}
  const size_t expected=size_t((size.first+15)&~15)*size_t((size.second+15)&~15)*3/(candidate&&format==54?1:2);
  if(bytes!=expected || calls!=1){fprintf(stderr,"size mismatch format=%d %dx%d bytes=%zu expected=%zu calls=%u\n",format,size.first,size.second,bytes,expected,calls);return 6;}cases++;
 }
 printf("PASS actual AllocateEmpty cases=%u candidate=%d P010_24bpp=%d existing_formats_retained=1\n",cases,candidate,candidate);return 0;
}
