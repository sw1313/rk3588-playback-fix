// SPDX-License-Identifier: MIT
// Exact API32 ARM64 framework import adapter. No kernel, audio HAL, pixel,
// decoder, player, fence or watchdog modification.
#include "event_guard.h"
#include <dlfcn.h>
#include <link.h>
#include <elf.h>
#include <sys/mman.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <android/log.h>
#include <sys/system_properties.h>
namespace {
using Receive=int(*)(char*,int);using Fd=int(*)();
Receive original=nullptr;Fd getFd=nullptr;
bool propOn(){char v[PROP_VALUE_MAX]={};__system_property_get("debug.rkhdr.hdmi_audio_guard",v);return !strcmp(v,"1");}
bool server(){char b[32]={};int f=open("/proc/self/cmdline",O_RDONLY|O_CLOEXEC);int n=f<0?-1:read(f,b,sizeof(b)-1);if(f>=0)close(f);return n>0&&!strcmp(b,"system_server");}
int physical(){char b[32]={};int f=open("/sys/class/drm/card0-HDMI-A-1/status",O_RDONLY|O_CLOEXEC);int n=f<0?-1:read(f,b,sizeof(b)-1);if(f>=0)close(f);if(n<=0)return -1;return !strcmp(b,"connected\n")?1:!strcmp(b,"disconnected\n")?0:-1;}
struct RealOps {
 int read(char *p,int n,int timeout){
  if(timeout<0)return original(p,n);
  struct pollfd fd{getFd(),POLLIN,0};if(fd.fd<0)return -1;
  int r=poll(&fd,1,timeout);if(r<=0)return r;
  return original(p,n);
 }
 int physical(){return ::physical();}
 int64_t nowMs(){timespec t{};clock_gettime(CLOCK_MONOTONIC,&t);return int64_t(t.tv_sec)*1000+t.tv_nsec/1000000;}
 void coalesced(){__android_log_print(ANDROID_LOG_INFO,"RKHdmiEvent","coalesced transient extcon reset; DRM remained connected");}
};
int receive(char *p,int n){
 if(!propOn() || !server())return original(p,n);
 thread_local bool reported=false;
 if(!reported){reported=true;__system_property_set("vendor.rkhdr.hdmi_event","active");}
 // UEventObserver has one receiving thread. Storage remains thread-local so
 // other callers/processes retain the original socket/event behavior.
 thread_local rkhdmi::State state{};RealOps ops;
 return rkhdmi::next(state,p,n,ops);
}
int protection(void *ptr){FILE*f=fopen("/proc/self/maps","r");if(!f)return -1;char l[512],perms[5];unsigned long a,b;int r=-1;
 while(fgets(l,sizeof(l),f))if(sscanf(l,"%lx-%lx %4s",&a,&b,perms)==3&&(uintptr_t)ptr>=a&&(uintptr_t)ptr<b){r=(perms[0]=='r'?PROT_READ:0)|(perms[1]=='w'?PROT_WRITE:0)|(perms[2]=='x'?PROT_EXEC:0);break;}fclose(f);return r;}
bool attach(uintptr_t base){
 const auto *e=(const Elf64_Ehdr*)base;if(memcmp(e->e_ident,ELFMAG,SELFMAG)||e->e_machine!=EM_AARCH64)return false;
 void *h=dlopen("libhardware_legacy.so",RTLD_NOW|RTLD_LOCAL);if(!h)return false;
 auto real=(Receive)dlsym(h,"uevent_next_event");auto fd=(Fd)dlsym(h,"uevent_get_fd");
 if(!real||!fd)return false;
 const Elf64_Phdr*ph=(const Elf64_Phdr*)(base+e->e_phoff);bool inside=false;
 constexpr uintptr_t rva=0x207570;
 for(unsigned i=0;i<e->e_phnum;i++)if(ph[i].p_type==PT_LOAD&&rva>=ph[i].p_vaddr&&rva+8<=ph[i].p_vaddr+ph[i].p_memsz)inside=true;
 if(!inside)return false;
 void **slot=(void**)(base+rva);if(*slot==(void*)receive)return true;if(*slot!=(void*)real)return false;
 int prot=protection(slot);long page=sysconf(_SC_PAGESIZE);if(prot<0||page<=0)return false;
 void *start=(void*)((uintptr_t)slot&~(uintptr_t(page)-1));if(mprotect(start,page,prot|PROT_WRITE))return false;
 original=real;getFd=fd;__atomic_store_n(slot,(void*)receive,__ATOMIC_RELEASE);
 if(mprotect(start,page,prot)){__atomic_store_n(slot,(void*)real,__ATOMIC_RELEASE);mprotect(start,page,prot);return false;}
 return true;
}
int discover(dl_phdr_info*i,size_t,void*o){if(!strcmp(i->dlpi_name,"/system/lib64/libandroid_runtime.so")){*(bool*)o=attach(i->dlpi_addr);return 1;}return 0;}
__attribute__((constructor)) void start(){if(!propOn())return;char api[PROP_VALUE_MAX]={},board[PROP_VALUE_MAX]={};__system_property_get("ro.build.version.sdk",api);__system_property_get("ro.board.platform",board);if(strcmp(api,"32")||strcmp(board,"rk3588"))return;bool ok=false;dl_iterate_phdr(discover,&ok);__android_log_print(ok?ANDROID_LOG_INFO:ANDROID_LOG_ERROR,"RKHdmiEvent","pinned framework import %s",ok?"active":"rejected");}
}
extern "C" __attribute__((visibility("default"))) int rk_hdmi_event_attach(void *h){void*p=dlsym(h,"_ZN7android34register_android_os_UEventObserverEP7_JNIEnv");Dl_info i{};if(!p||!dladdr(p,&i)||(uintptr_t)p!=(uintptr_t)i.dli_fbase+0x15b7c0)return 2;return attach((uintptr_t)i.dli_fbase)?0:3;}
