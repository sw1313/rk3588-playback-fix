// SPDX-License-Identifier: MIT
// Exact-ABI adapter for the pinned API32 AllocateEmpty call. The accompanying
// source-referenced byte recipe fixes the P010 stride/depth double counting.
// No data, dimensions, buffer ownership, fences or decoder statistics change.
#include "Codec2Buffer.h"
#include <media/stagefright/foundation/AMessage.h>
#include <cutils/properties.h>
#include <android/log.h>
#include <link.h>
#include <elf.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <functional>
#include <utility>
#include <dlfcn.h>

namespace {
using namespace android;
using Alloc = std::function<sp<ABuffer>(size_t)>;
using Empty = sp<ConstGraphicBlockBuffer>(*)(const sp<AMessage>&, Alloc);
Empty originalEmpty = nullptr;
#if defined(__LP64__)
using Ehdr=Elf64_Ehdr;using Phdr=Elf64_Phdr;using Dyn=Elf64_Dyn;using Sym=Elf64_Sym;using Rela=Elf64_Rela;
constexpr uintptr_t emptyRva=0x94f78;
constexpr int jumpType=R_AARCH64_JUMP_SLOT;
const char emptyName[]="_ZN7android23ConstGraphicBlockBuffer13AllocateEmptyERKNS_2spINS_8AMessageEEENSt3__18functionIFNS1_INS_7ABufferEEEmEEE";
#define RSYM ELF64_R_SYM
#define RTYPE ELF64_R_TYPE
#else
using Ehdr=Elf32_Ehdr;using Phdr=Elf32_Phdr;using Dyn=Elf32_Dyn;using Sym=Elf32_Sym;using Rela=Elf32_Rel;
constexpr uintptr_t emptyRva=0x6a7cd;
constexpr int jumpType=R_ARM_JUMP_SLOT;
const char emptyName[]="_ZN7android23ConstGraphicBlockBuffer13AllocateEmptyERKNS_2spINS_8AMessageEEENSt3__18functionIFNS1_INS_7ABufferEEEjEEE";
#define RSYM ELF32_R_SYM
#define RTYPE ELF32_R_TYPE
#endif
static_assert(sizeof(Alloc)==(sizeof(void*)==8?48:24),"Pinned platform libc++ std::function ABI");
sp<ConstGraphicBlockBuffer> poolEmpty(const sp<AMessage>& format, Alloc alloc) {
    int32_t color=0,width=0,height=0;
    if (!format || !format->findInt32("color-format",&color) || color!=54 ||
        !format->findInt32("width",&width) || !format->findInt32("height",&height) ||
        width<=0 || height<=0 || width>8192 || height>8192)
        return originalEmpty(format,std::move(alloc));
    const size_t oldBytes=size_t((width+15)&~15)*size_t((height+15)&~15)*3/2;
    // Selectively implements the upstream P010 24bpp AllocateEmpty correction.
    // The pinned original supplies the checked aligned YUV420 12bpp count.
    // Unexpected callback sizes are not reinterpreted.
    Alloc p010=[alloc=std::move(alloc),oldBytes](size_t bytes) {
        return alloc(bytes==oldBytes?bytes*2:bytes);
    };
    return originalEmpty(format,std::move(p010));
}
int protection(void *ptr) {
    FILE *f=fopen("/proc/self/maps","r");if(!f)return -1;
    char line[512],perms[5];unsigned long a,b;int result=-1;
    while(fgets(line,sizeof(line),f))if(sscanf(line,"%lx-%lx %4s",&a,&b,perms)==3 &&
        uintptr_t(ptr)>=a && uintptr_t(ptr)<b){result=(perms[0]=='r'?PROT_READ:0)|(perms[1]=='w'?PROT_WRITE:0)|(perms[2]=='x'?PROT_EXEC:0);break;}
    fclose(f);return result;
}
bool install(uintptr_t base) {
    auto e=reinterpret_cast<const Ehdr*>(base);
    if(memcmp(e->e_ident,ELFMAG,SELFMAG) ||
       e->e_machine!=(sizeof(void*)==8?EM_AARCH64:EM_ARM))return false;
    auto ph=reinterpret_cast<const Phdr*>(base+e->e_phoff);const Dyn *dyn=nullptr;
    for(unsigned i=0;i<e->e_phnum;i++)if(ph[i].p_type==PT_DYNAMIC)dyn=reinterpret_cast<const Dyn*>(base+ph[i].p_vaddr);
    if(!dyn)return false;
    auto address=[base](uintptr_t v){return v<base?base+v:v;};
    const Sym *sym=nullptr;const char *str=nullptr;const Rela *rel=nullptr;size_t bytes=0;
    for(auto d=dyn;d->d_tag!=DT_NULL;d++){
        if(d->d_tag==DT_SYMTAB)sym=reinterpret_cast<const Sym*>(address(d->d_un.d_ptr));
        if(d->d_tag==DT_STRTAB)str=reinterpret_cast<const char*>(address(d->d_un.d_ptr));
        if(d->d_tag==DT_JMPREL)rel=reinterpret_cast<const Rela*>(address(d->d_un.d_ptr));
        if(d->d_tag==DT_PLTRELSZ)bytes=d->d_un.d_val;
    }
    if(!sym || !str || !rel || bytes>1024*1024)return false;
    void **slot=nullptr;
    for(size_t i=0;i<bytes/sizeof(*rel);i++)if(RTYPE(rel[i].r_info)==jumpType &&
        !strcmp(str+sym[RSYM(rel[i].r_info)].st_name,emptyName)){
        if(slot)return false;slot=reinterpret_cast<void**>(base+rel[i].r_offset);
    }
    if(!slot || *slot!=reinterpret_cast<void*>(base+emptyRva))return false;
    const int prot=protection(slot);const long pageSize=sysconf(_SC_PAGESIZE);
    if(prot<0 || pageSize<=0)return false;
    void *page=reinterpret_cast<void*>(uintptr_t(slot)&~(uintptr_t(pageSize)-1));
    if(mprotect(page,pageSize,prot|PROT_WRITE))return false;
    originalEmpty=reinterpret_cast<Empty>(base+emptyRva);
    __atomic_store_n(slot,reinterpret_cast<void*>(poolEmpty),__ATOMIC_RELEASE);
    if(mprotect(page,pageSize,prot)){
        __atomic_store_n(slot,reinterpret_cast<void*>(originalEmpty),__ATOMIC_RELEASE);
        mprotect(page,pageSize,prot);originalEmpty=nullptr;return false;
    }
    return true;
}
int discover(dl_phdr_info *info,size_t,void *opaque) {
    // Automatic binding only targets the installed framework path. Private
    // installer probes attach explicitly inside their own process instead.
    if(!strcmp(info->dlpi_name,"/system/lib64/libsfplugin_ccodec.so") ||
       !strcmp(info->dlpi_name,"/system/lib/libsfplugin_ccodec.so")){
        *static_cast<bool*>(opaque)=install(info->dlpi_addr);return 1;
    }
    return 0;
}
__attribute__((constructor)) void start() {
    if(!property_get_bool("debug.rkcodec2.p010_pool",false))return;
    char api[PROP_VALUE_MAX],board[PROP_VALUE_MAX];property_get("ro.build.version.sdk",api,"");property_get("ro.board.platform",board,"");
    if(strcmp(api,"32") || strcmp(board,"rk3588"))return;
    bool ok=false;dl_iterate_phdr(discover,&ok);
    __android_log_print(ok?ANDROID_LOG_INFO:ANDROID_LOG_ERROR,"RKCodec2Pool10","pinned P010 pool adapter %s",ok?"active":"rejected");
}
}
// Used by the isolated fixture to attach only to its private dlopen handle.
// It does not open a HAL, set a system property or modify another process.
extern "C" __attribute__((visibility("default"))) int rk_pool10_attach(void *handle) {
    void *symbol=dlsym(handle,emptyName);Dl_info info{};
    if(!symbol || !dladdr(symbol,&info) || reinterpret_cast<uintptr_t>(symbol)!=reinterpret_cast<uintptr_t>(info.dli_fbase)+emptyRva)return 2;
    return install(reinterpret_cast<uintptr_t>(info.dli_fbase))?0:3;
}
