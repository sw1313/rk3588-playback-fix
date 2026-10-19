// SPDX-License-Identifier: MIT
// Calls copies of the real pinned SF methods against isolated window/display
// objects. No Binder, HAL, display, process property or live compositor changes.
#include <system/window.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <initializer_list>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static int format=1,spaces=0,formats=0,lastSpace=0;
static bool virtualDisplay=false;
static int queueCalls=0,lastFence=0;
static int originalQueue(ANativeWindow*,ANativeWindowBuffer*,int fence){queueCalls++;lastFence=fence;return -17;}
static int query(const ANativeWindow*,int what,int* out){CHECK(what==NATIVE_WINDOW_FORMAT);*out=format;return 0;}
static int perform(ANativeWindow*,int operation,...){
 va_list a;va_start(a,operation);int value=va_arg(a,int);va_end(a);
 if(operation==NATIVE_WINDOW_SET_BUFFERS_DATASPACE){spaces++;lastSpace=value;}
 else {CHECK(operation==NATIVE_WINDOW_SET_BUFFERS_FORMAT);formats++;format=value;}
 return 0;
}
static bool isVirtual(const void*){return virtualDisplay;}
int main(int argc,char** argv){
 CHECK(argc==3);void* lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);CHECK(lib);
 auto attach=reinterpret_cast<int(*)(uintptr_t)>(dlsym(lib,"rk_sf_target10_test_attach"));CHECK(attach);
 constexpr size_t size=0x5a0000;auto base=static_cast<char*>(mmap(nullptr,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));CHECK(base!=MAP_FAILED);
 FILE* f=fopen(argv[2],"rb");CHECK(f);CHECK(fseek(f,0x1f2468,SEEK_SET)==0);CHECK(fread(base+0x1f2468,1,40,f)==40);fclose(f);
 using Fn=void(*)(void*,int32_t);auto slot=reinterpret_cast<Fn*>(base+0x59ea78);*slot=reinterpret_cast<Fn>(base+0x1f2468);
 __builtin___clear_cache(base+0x1f2468,base+0x1f2468+40);
 CHECK(mprotect(base+0x1f2000,4096,PROT_READ|PROT_EXEC)==0);
 CHECK(mprotect(base+0x59e000,4096,PROT_READ)==0);
 CHECK(attach(uintptr_t(base))==0);CHECK(attach(uintptr_t(base))!=0);
 ANativeWindow windows[16]{};unsigned wi=0;
 uintptr_t vt[0x200/8]{};vt[0x1f8/8]=uintptr_t(isVirtual);void* display=vt;
 uintptr_t self[4]{};self[2]=uintptr_t(&display);
 unsigned cases=0;
 for(unsigned generation=0;generation<16;generation++){
  const int original=1;
  auto& window=windows[wi++];window.query=query;window.perform=perform;window.queueBuffer=originalQueue;self[3]=uintptr_t(&window);
  format=original;formats=0;
  for(int range:{0x8000000,0x10000000}){
   for(int transfer:{0x1c00000,0x2000000}){
    const int hdr=0x60000|transfer|range;
    virtualDisplay=true;int before=formats;(*slot)(self,hdr);CHECK(format==original && formats==before && lastSpace==hdr);cases++;
    virtualDisplay=false;(*slot)(self,hdr);CHECK(format==43 && formats==before+1 && lastSpace==hdr);cases++;
    (*slot)(self,hdr);CHECK(format==43 && formats==before+1);cases++;
    (*slot)(self,0x8810000);CHECK(format==original && formats==before+2);cases++;
   }
  }
  for(int s:{0,0x8810000,0x8860000,0x8840000}){int before=formats;(*slot)(self,s);CHECK(format==original && formats==before);cases++;}
 }
 CHECK(windows[0].queueBuffer(&windows[0],nullptr,123)==-17 && queueCalls==1 && lastFence==123);cases++;
 CHECK(spaces==int(cases)-1);printf("PASS actual SF methods physical PQ/HLG, SDR restore, virtual unchanged, queue/fence forwarding: %u cases\n",cases);
 munmap(base,size);return 0;
}
