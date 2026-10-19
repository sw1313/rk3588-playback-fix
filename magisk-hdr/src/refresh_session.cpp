// SPDX-License-Identifier: MIT
// Exact API32 policy at Layer::setFrameRate, common to Binder and BLAST
// transactions. Keep only an existing fixed-source vote on that same Layer.
#include <android/log.h>
#include <sys/system_properties.h>
#include <sys/mman.h>
#include <link.h>
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
namespace rk_refresh_session {
constexpr uintptr_t kLayerSet=0x14b6cc, kFrameRate=0x6b8, kNativePage=0x5c4000;
constexpr unsigned char kLayerBytes[]={
 0xfd,0x7b,0xbe,0xa9,0xf4,0x4f,0x01,0xa9,0xfd,0x03,0x00,0x91,0xc8,0x23,0x00,0xb0,
 0x08,0xe1,0x51,0x39,0x48,0x02,0x00,0x34,0x00,0xb8,0x46,0xbd,0x21,0x00,0x40,0xbd};
// Passed indirectly (24 bytes). Keep the exact native fractional Fps/duration.
struct FrameRate {float rate;uint32_t padding;int64_t period;int32_t type;int32_t seamlessness;};
static_assert(sizeof(FrameRate)==24 && offsetof(FrameRate,type)==16 && offsetof(FrameRate,seamlessness)==20);
using Set=bool(*)(void*,const FrameRate*);
Set original=nullptr;
int (*readProperty)(const char*,char*)=__system_property_get;
unsigned calls=0,held=0,sources=0,logs=0;
bool enabled(){
 char v[PROP_VALUE_MAX]{};readProperty("debug.rkdecode.refresh_session",v);
 if(strcmp(v,"1"))return false;
 readProperty("persist.vendor.resolution.HDMI-A-0",v);return !strcmp(v,"Auto");
}
bool fixed(const FrameRate& r){
 // Vendor converts public FIXED_SOURCE=1 to internal ExactOrMultiple=2.
 return isfinite(r.rate) && r.rate>=1 && r.rate<=120 && r.type==2 &&
        r.period>=8000000 && r.period<=1000000000 &&
        (r.seamlessness==0 || r.seamlessness==1);
}
bool set(void* layer,const FrameRate* incoming){
 __atomic_add_fetch(&calls,1u,__ATOMIC_RELAXED);
 FrameRate previous{},retained{};const FrameRate* forwarded=incoming;
 if(layer && incoming && enabled()){
  memcpy(&previous,static_cast<char*>(layer)+kFrameRate,sizeof(previous));
  if(incoming->rate==0 && incoming->type==0 && incoming->seamlessness==2 && fixed(previous)){
   retained=previous;forwarded=&retained;__atomic_add_fetch(&held,1u,__ATOMIC_RELAXED);
  }else if(fixed(*incoming))__atomic_add_fetch(&sources,1u,__ATOMIC_RELAXED);
 }
 // One native call and its real result. Native hierarchy propagation,
 // transaction flags and hidden/removed-layer voting remain in control.
 bool result=original(layer,forwarded);
 char trace[PROP_VALUE_MAX]{};readProperty("debug.rkdecode.session_trace",trace);
 if(!strcmp(trace,"1") && __atomic_load_n(&logs,__ATOMIC_RELAXED)<96){
  __atomic_add_fetch(&logs,1u,__ATOMIC_RELAXED);char event[PROP_VALUE_MAX];
  snprintf(event,sizeof(event),"calls=%u hold=%u src=%u in=%.3f out=%.3f type=%d changed=%d",
   __atomic_load_n(&calls,__ATOMIC_RELAXED),__atomic_load_n(&held,__ATOMIC_RELAXED),
   __atomic_load_n(&sources,__ATOMIC_RELAXED),incoming?incoming->rate:-1.f,
   forwarded?forwarded->rate:-1.f,incoming?incoming->type:-1,result);
  __android_log_print(ANDROID_LOG_INFO,"RKRefreshSession","%s",event);
  __system_property_set("vendor.rkdecode.session_event",event);
 }
 return result;
}
bool contains(const dl_phdr_info* info,uintptr_t offset,size_t bytes,unsigned flags){
 for(int i=0;i<info->dlpi_phnum;++i){const auto& p=info->dlpi_phdr[i];
  if(p.p_type==PT_LOAD && (p.p_flags&flags)==flags && offset>=p.p_vaddr &&
   offset-p.p_vaddr<=p.p_memsz && bytes<=p.p_memsz-(offset-p.p_vaddr))return true;}
 return false;
}
void flush(void* begin,size_t n){__builtin___clear_cache(static_cast<char*>(begin),static_cast<char*>(begin)+n);}
int attach(uintptr_t base){
 if(original)return -1;
 auto* target=reinterpret_cast<unsigned char*>(base+kLayerSet);
 if(memcmp(target,kLayerBytes,sizeof(kLayerBytes)))return -2;
 long page=sysconf(_SC_PAGESIZE);if(page<=0 || (page&(page-1)))return -3;
 // Startup only, before SF main/service threads. No live code changes,
 // RWX mappings, generic relocator or per-frame workers.
 auto* code=static_cast<unsigned char*>(mmap(nullptr,page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
 if(code==MAP_FAILED)return -4;
 uint32_t words[]={0xa9be7bfd,0xa9014ff4,0x910003fd,0x580000a8,
                   0x580000d0,0xd61f0200,0xd503201f,0xd503201f};
 memcpy(code,words,sizeof(words));
 // Relocate the exact prologue ADRP x8. Resume at the following LDRB.
 uintptr_t native=base+kNativePage,resume=base+kLayerSet+16;
 memcpy(code+32,&native,8);memcpy(code+40,&resume,8);flush(code,48);
 if(mprotect(code,page,PROT_READ|PROT_EXEC)){munmap(code,page);return -5;}
 void* start=reinterpret_cast<void*>((base+kLayerSet)&~uintptr_t(page-1));
 if(mprotect(start,page,PROT_READ|PROT_WRITE)){munmap(code,page);return -6;}
 uint32_t jump[]={0x58000050,0xd61f0200};uintptr_t callback=reinterpret_cast<uintptr_t>(set);
 original=reinterpret_cast<Set>(code);
 memcpy(target,jump,8);memcpy(target+8,&callback,8);flush(target,16);
 if(mprotect(start,page,PROT_READ|PROT_EXEC)){
  memcpy(target,kLayerBytes,16);flush(target,16);
  if(mprotect(start,page,PROT_READ|PROT_EXEC))_exit(126);
  original=nullptr;munmap(code,page);return -7;
 }
 return 0;
}
int find(dl_phdr_info* info,size_t,void* result){
 if(info->dlpi_name && *info->dlpi_name && strcmp(info->dlpi_name,"/system/bin/surfaceflinger"))return 0;
 if(!contains(info,kLayerSet,sizeof(kLayerBytes),PF_R|PF_X) || !contains(info,kNativePage+0x478,1,PF_R))return 1;
 *static_cast<int*>(result)=attach(info->dlpi_addr);return 1;
}
#ifndef RK_REFRESH_SESSION_TEST
__attribute__((constructor)) void init(){
 char path[256]{};if(readlink("/proc/self/exe",path,sizeof(path)-1)<=0 || strcmp(path,"/system/bin/surfaceflinger"))return;
 char flag[PROP_VALUE_MAX]{};readProperty("debug.rkdecode.refresh_session",flag);if(strcmp(flag,"1"))return;
 int rc=-8;dl_iterate_phdr(find,&rc);
 __system_property_set("vendor.rkdecode.refresh_session",rc==0?"active":"failed");
 __android_log_print(rc?ANDROID_LOG_ERROR:ANDROID_LOG_INFO,"RKRefreshSession",
  "attach=%d; common Layer vote, Auto only, original Fps and native return retained",rc);
}
#endif
}
