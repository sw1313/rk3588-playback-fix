// SPDX-License-Identifier: MIT
// Actual allocator format selection; properties are mocked only in this process.
// Never opens the allocator HAL or allocates/submits a display buffer.
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
using Select=uint64_t(*)(uint64_t,int,uint64_t,int);
static int linear;
static int property(const char* key,char* out,const char* fallback){
 const char* value=fallback?fallback:"";
 if(!strcmp(key,"ro.board.platform"))value="rk3588";
 else if(!strcmp(key,"vendor.gralloc.fb_size"))value="8294400";
 else if(!strcmp(key,"vendor.gralloc.no_afbc_for_fb_target_layer"))value=linear?"1":"0";
 snprintf(out,92,"%s",value);return strlen(out);
}
static int setProperty(const char*,const char*){return 0;}
static void replace(uintptr_t where,void* fn){
 long page=sysconf(_SC_PAGESIZE);void* base=reinterpret_cast<void*>(where&~uintptr_t(page-1));
 CHECK(mprotect(base,page,PROT_READ|PROT_WRITE)==0);*reinterpret_cast<void**>(where)=fn;
 CHECK(mprotect(base,page,PROT_READ)==0);
}
struct Case{uint64_t format;int type;uint64_t usage;int size;uint64_t before;};
int main(int argc,char** argv){
 CHECK(argc==3);linear=atoi(argv[2]);void* lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!lib)fprintf(stderr,"%s\n",dlerror());CHECK(lib);
 auto original=reinterpret_cast<Select>(dlsym(lib,"_Z26mali_gralloc_select_formatm24mali_gralloc_format_typemi"));Dl_info info{};CHECK(original && dladdr(reinterpret_cast<void*>(original),&info));
 uintptr_t base=uintptr_t(info.dli_fbase);CHECK(uintptr_t(original)==base+0xbbe8);
 replace(base+0xe7e0,reinterpret_cast<void*>(property));replace(base+0xe7f0,reinterpret_cast<void*>(setProperty));
 const uint64_t formats[]={1,2,3,4,5,16,17,22,43,0x100,0x101,0x104};
 const uint64_t usages[]={0x300,0xb00,0x1a00,0x1b00,0x1a33,0x20001a00,0x1400,0x100};
 std::vector<Case> cases;
 for(auto f:formats)for(auto u:usages)for(int type=0;type<2;type++)for(int size:{4096,8294400}){
  *reinterpret_cast<uint32_t*>(base+0x10524)=8294400;
  cases.push_back({f,type,u,size,original(f,type,u,size)});
 }
 auto attach=reinterpret_cast<int(*)(void*)>(dlsym(lib,"rk_alloc10_attach"));CHECK(attach && attach(lib)==0);
 auto selected=*reinterpret_cast<Select*>(base+0xe780);CHECK(selected!=original);
 unsigned changed=0;
 for(const auto& c:cases){
  *reinterpret_cast<uint32_t*>(base+0x10524)=8294400;
  uint64_t result=selected(c.format,c.type,c.usage,c.size);
  bool fix=c.format==43 && (c.usage&0x1000) && (c.before==1 || c.before==0x100000001ULL);
  CHECK(result==(fix?((c.before&0xffffffff00000000ULL)|43):c.before));if(fix)changed++;
 }
 CHECK(linear?changed==0:changed>0);printf("PASS actual allocator selection cases=%zu changed_RGB10_FB=%u linear_property=%d other_formats_usages_retained=1 real_property_writes=0 HAL_opened=0\n",cases.size(),changed,linear);
 return 0;
}
