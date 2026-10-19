// SPDX-License-Identifier: MIT
// Exact API32 SurfaceFlinger adaptation. Physical HDR targets only; the original
// rendering, BufferQueue ownership, dimensions and fences remain unchanged.
#include <system/window.h>
#include <ui/GraphicBufferMapper.h>
#include <sys/system_properties.h>
#include <android/log.h>
#include <link.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include "target_readback.h"

namespace {
constexpr uintptr_t kSpace=0x1f2468,kFormat=0x1f247c,kSlot=0x59ea78;
constexpr unsigned char spaceBytes[]={0x00,0x0c,0x40,0xf9,0xe2,0x03,0x01,0x2a,0x61,0x02,0x80,0x52,0x03,0x4c,0x40,0xf9,0x60,0x00,0x1f,0xd6};
constexpr unsigned char formatBytes[]={0x00,0x0c,0x40,0xf9,0xe2,0x03,0x01,0x2a,0x21,0x01,0x80,0x52,0x03,0x4c,0x40,0xf9,0x60,0x00,0x1f,0xd6};
using Method=void(*)(void*,int32_t);
Method originalSpace=nullptr,originalFormat=nullptr;
int (*originalQueue)(ANativeWindow*,ANativeWindowBuffer*,int)=nullptr;
[[noreturn]] void fail(const char* message);
int queue(ANativeWindow* window,ANativeWindowBuffer* buffer,int fence){
 if(buffer && buffer->format==43){
  uint32_t fourcc=0;uint64_t modifier=0;auto& mapper=android::GraphicBufferMapper::get();
  if(mapper.getPixelFormatFourCC(buffer->handle,&fourcc) || fourcc!=0x30334241 ||
     mapper.getPixelFormatModifier(buffer->handle,&modifier) || modifier!=0x0800000000000001ULL)
   fail("physical RGB10 target storage is not supported AB30 AFBC");
 }
 char trace[PROP_VALUE_MAX]{};__system_property_get("debug.rkhdr.trace",trace);
 if(buffer && !strcmp(trace,"1")){
  auto& mapper=android::GraphicBufferMapper::get();uint32_t fourcc=0;uint64_t usage=0,bytes=0;
  int rc=mapper.getPixelFormatFourCC(buffer->handle,&fourcc);
  mapper.getUsage(buffer->handle,&usage);mapper.getAllocationSize(buffer->handle,&bytes);
  __android_log_print(ANDROID_LOG_INFO,"RKSfTarget10","queue format=%d fourcc=%08x metadata=%d usage=%llx bytes=%llu",buffer->format,fourcc,rc,(unsigned long long)usage,(unsigned long long)bytes);
 }
 char capture[PROP_VALUE_MAX]{};__system_property_get("debug.rkhdr.capture_target",capture);
 if(buffer && buffer->format==43 && !strcmp(capture,"1")){
  __system_property_set("debug.rkhdr.capture_target","0");rk_target_readback::capture(buffer,fence,true);
 }
 return originalQueue(window,buffer,fence);
}
static_assert(offsetof(ANativeWindow,perform)==0x98,"pinned ARM64 window ABI");
[[noreturn]] void fail(const char* message) {
 __android_log_print(ANDROID_LOG_FATAL,"RKSfTarget10","%s",message);
 __system_property_set("vendor.rkhdr.sf_target10","failed");
 __system_property_set("vendor.rkhdr.sf_target10_error","1");
 abort(); // Never continue signalling HDR with an unverified eight-bit target.
}
void dataspace(void* self,int32_t space) {
 originalSpace(self,space);
 void* display=*reinterpret_cast<void**>(static_cast<char*>(self)+0x10);
 auto table=*reinterpret_cast<uintptr_t**>(display);
 auto isVirtual=reinterpret_cast<bool(*)(const void*)>(table[0x1f8/8]);
 if(isVirtual(display))return;
 auto window=*reinterpret_cast<ANativeWindow**>(static_cast<char*>(self)+0x18);
 // Transfer/standard masks accept both range encodings; unknown and all SDR
 // dataspaces restore the previous physical target format after HDR exit.
 const bool hdr=(space&0x3f0000)==0x60000 &&
     ((space&0x7c00000)==0x1c00000 || (space&0x7c00000)==0x2000000);
 int current=0;
 if(window->query(window,NATIVE_WINDOW_FORMAT,&current) || current<=0)fail("cannot query physical target format");
 // This exact SF initializes physical targets to RGBA8888, and its Composer
 // 2.1 channel cannot issue later format requests. Restore that pinned value;
 // keep no stale per-window pointers across display recreation or hotplug.
 if(!hdr && current!=43)return;
 if(hdr && current!=1 && current!=43)fail("unexpected original physical target format");
 if(hdr && window->queueBuffer!=queue){
  if(!window->queueBuffer)fail("physical window has no queue callback");
  if(originalQueue && originalQueue!=window->queueBuffer)fail("unexpected physical window queue ABI");
  originalQueue=window->queueBuffer;window->queueBuffer=queue;
 }
 const int desired=hdr?43:1;
 if(current!=desired){
  originalFormat(self,desired);
  int actual=0;
  if(window->query(window,NATIVE_WINDOW_FORMAT,&actual) || actual!=desired)fail("physical target format request rejected");
  __android_log_print(ANDROID_LOG_INFO,"RKSfTarget10","physical target dataspace=0x%x format=%d",space,desired);
 }
}
int attach(uintptr_t base) {
 if(originalSpace)return -1;
 auto slot=reinterpret_cast<Method*>(base+kSlot);
 if(memcmp(reinterpret_cast<void*>(base+kSpace),spaceBytes,sizeof(spaceBytes)) ||
    memcmp(reinterpret_cast<void*>(base+kFormat),formatBytes,sizeof(formatBytes)) ||
    *slot!=reinterpret_cast<Method>(base+kSpace))return -2;
 long page=sysconf(_SC_PAGESIZE);if(page<=0 || (page&(page-1)))return -3;
 auto start=reinterpret_cast<void*>((base+kSlot)&~uintptr_t(page-1));
 if(mprotect(start,page,PROT_READ|PROT_WRITE))return -4;
 originalSpace=*slot;originalFormat=reinterpret_cast<Method>(base+kFormat);
 *slot=dataspace;
 if(mprotect(start,page,PROT_READ))fail("cannot restore target vtable RELRO protection");
 return 0;
}
bool contains(const dl_phdr_info* info,uintptr_t offset,size_t size,unsigned flags) {
 for(int i=0;i<info->dlpi_phnum;i++){
  const auto& p=info->dlpi_phdr[i];
  if(p.p_type==PT_LOAD && (p.p_flags&flags)==flags && offset>=p.p_vaddr &&
     offset-p.p_vaddr<=p.p_memsz && size<=p.p_memsz-(offset-p.p_vaddr))return true;
 }
 return false;
}
int findMain(dl_phdr_info* info,size_t,void* result) {
 if(info->dlpi_name && info->dlpi_name[0] && strcmp(info->dlpi_name,"/system/bin/surfaceflinger"))return 0;
 if(!contains(info,kSpace,40,PF_R|PF_X) || !contains(info,kSlot,sizeof(void*),PF_R))return 1;
 *static_cast<int*>(result)=attach(info->dlpi_addr);return 1;
}
__attribute__((constructor)) void init() {
 char path[256]{};ssize_t n=readlink("/proc/self/exe",path,sizeof(path)-1);
 if(n<=0 || strcmp(path,"/system/bin/surfaceflinger"))return;
 char value[PROP_VALUE_MAX]{};__system_property_get("debug.rkhdr.client_hdr10",value);
 if(strcmp(value,"1"))return;
 __system_property_get("vendor.rkhdr.sf_target10_error",value);
 if(!strcmp(value,"1"))fail("physical target failure latched for this boot");
 int result=-5;dl_iterate_phdr(findMain,&result);
 if(!result){
  // Both services start behind the first-binding barrier. Do not let SF cache
  // NATIVE-only capabilities before the paired allocator has initialized.
  for(unsigned i=0;i<1000;i++){
   __system_property_get("vendor.rkhdr.alloc10",value);if(!strcmp(value,"active"))break;
   if(!strcmp(value,"failed"))break;usleep(10000);
  }
  if(strcmp(value,"active"))fail("paired RGB10 allocator not ready before SF initialization");
 }
 __system_property_set("vendor.rkhdr.sf_target10",result==0?"active":"failed");
 if(result)fail("pinned SurfaceFlinger target adapter ABI mismatch");
}
}
// This entry only attaches to a caller-provided isolated fixture. It cannot
// locate or modify another process; the installer never executes the real SF.
extern "C" __attribute__((visibility("default"))) int rk_sf_target10_test_attach(uintptr_t base){return attach(base);}
