// SPDX-License-Identifier: MIT
// Correct only the pinned RK3588 allocator's forced eight-bit HW_FB selection.
// Keep the original usage/heap/protection/size calculations and allocation ABI.
#include <link.h>
#include <elf.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <sys/system_properties.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdint>
namespace {
using Select=uint64_t(*)(uint64_t,int,uint64_t,int);
constexpr char symbol[]="_Z26mali_gralloc_select_formatm24mali_gralloc_format_typemi";
Select original=nullptr;
uint64_t select(uint64_t format,int type,uint64_t usage,int size){
 uint64_t result=original(format,type,usage,size);
 // The original FB fast path selects RGBA8888/AFBC regardless of requested
 // RGB10. Keep the original AFBC choice: this firmware's RGB10 Cluster
 // planes reject linear RGB10. Retain all ten bits and the selected layout;
 // never relabel an eight-bit allocation, clear usage bits, or add a copy.
 if(format==43 && (usage&0x1000) && (result==1 || result==0x100000001ULL))return (result&0xffffffff00000000ULL)|43;
 return result;
}
int protection(void* p){
 FILE* f=fopen("/proc/self/maps","r");if(!f)return -1;
 char line[512],perms[5];unsigned long a,b;int result=-1;
 while(fgets(line,sizeof(line),f))if(sscanf(line,"%lx-%lx %4s",&a,&b,perms)==3 && uintptr_t(p)>=a && uintptr_t(p)<b){
  result=(perms[0]=='r'?PROT_READ:0)|(perms[1]=='w'?PROT_WRITE:0)|(perms[2]=='x'?PROT_EXEC:0);break;
 }
 fclose(f);return result;
}
int attach(uintptr_t base){
 if(original)return 1;
 auto e=reinterpret_cast<Elf64_Ehdr*>(base);
 if(memcmp(e->e_ident,ELFMAG,SELFMAG) || e->e_machine!=EM_AARCH64)return 2;
 auto ph=reinterpret_cast<Elf64_Phdr*>(base+e->e_phoff);Elf64_Dyn* dyn=nullptr;
 for(unsigned i=0;i<e->e_phnum;i++)if(ph[i].p_type==PT_DYNAMIC)dyn=reinterpret_cast<Elf64_Dyn*>(base+ph[i].p_vaddr);
 if(!dyn)return 3;
 const Elf64_Sym* syms=nullptr;const char* strings=nullptr;const Elf64_Rela* rel=nullptr;size_t bytes=0;
 auto addr=[base](uintptr_t v){return v<base?base+v:v;};
 for(auto d=dyn;d->d_tag!=DT_NULL;d++){
  if(d->d_tag==DT_SYMTAB)syms=reinterpret_cast<Elf64_Sym*>(addr(d->d_un.d_ptr));
  if(d->d_tag==DT_STRTAB)strings=reinterpret_cast<char*>(addr(d->d_un.d_ptr));
  if(d->d_tag==DT_JMPREL)rel=reinterpret_cast<Elf64_Rela*>(addr(d->d_un.d_ptr));
  if(d->d_tag==DT_PLTRELSZ)bytes=d->d_un.d_val;
 }
 if(!syms || !strings || !rel || bytes>1024*1024)return 4;
 void** slot=nullptr;
 for(size_t i=0;i<bytes/sizeof(*rel);i++)if(ELF64_R_TYPE(rel[i].r_info)==R_AARCH64_JUMP_SLOT && !strcmp(strings+syms[ELF64_R_SYM(rel[i].r_info)].st_name,symbol)){
  if(slot || rel[i].r_offset!=0xe780)return 5;slot=reinterpret_cast<void**>(base+rel[i].r_offset);
 }
 if(!slot || *slot!=reinterpret_cast<void*>(base+0xbbe8))return 6;
 int prot=protection(slot);long page=sysconf(_SC_PAGESIZE);if(prot<0 || page<=0)return 7;
 void* start=reinterpret_cast<void*>(uintptr_t(slot)&~uintptr_t(page-1));
 if(mprotect(start,page,prot|PROT_WRITE))return 8;
 original=reinterpret_cast<Select>(*slot);__atomic_store_n(slot,reinterpret_cast<void*>(select),__ATOMIC_RELEASE);
 if(mprotect(start,page,prot)){__atomic_store_n(slot,reinterpret_cast<void*>(original),__ATOMIC_RELEASE);mprotect(start,page,prot);original=nullptr;return 9;}
 return 0;
}
int discover(dl_phdr_info* info,size_t,void* result){
 if(!strcmp(info->dlpi_name,"/vendor/lib64/hw/android.hardware.graphics.allocator@4.0-impl-bifrost.so")){
  *static_cast<int*>(result)=attach(info->dlpi_addr);return 1;
 }
 return 0;
}
__attribute__((constructor)) void init(){
 char value[PROP_VALUE_MAX]{};__system_property_get("debug.rkhdr.client_hdr10",value);if(strcmp(value,"1"))return;
 char path[256]{};if(readlink("/proc/self/exe",path,sizeof(path)-1)<=0 || strcmp(path,"/vendor/bin/hw/android.hardware.graphics.allocator@4.0-service"))return;
 int result=-1;dl_iterate_phdr(discover,&result);
 __system_property_set("vendor.rkhdr.alloc10",result==0?"active":"failed");
}
}
extern "C" __attribute__((visibility("default"))) int rk_alloc10_attach(void* lib){
 void* fn=dlsym(lib,symbol);Dl_info info{};
 if(!fn || !dladdr(fn,&info) || uintptr_t(fn)!=uintptr_t(info.dli_fbase)+0xbbe8)return 10;
 return attach(uintptr_t(info.dli_fbase));
}
