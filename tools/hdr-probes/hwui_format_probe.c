/* SPDX-License-Identifier: MIT
 * Isolated exact-firmware DeviceInfo regression. Display APIs and abort logger
 * are replaced only in this test process. No live app/HWUI instance is patched.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <elf.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <setjmp.h>
#include <stdarg.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
#if __SIZEOF_POINTER__ == 8
typedef Elf64_Ehdr EH;typedef Elf64_Phdr PH;typedef Elf64_Dyn DY;typedef Elf64_Sym SY;typedef Elf64_Rela RE;
#define SYM(i) ELF64_R_SYM(i)
#define TYPE(i) ELF64_R_TYPE(i)
#define SLOT R_AARCH64_JUMP_SLOT
#define RELOC DT_JMPREL
#define METHOD 0x47b7bc
#define ISOLATED 0xa2eeb9
#define FIELD 16
#else
typedef Elf32_Ehdr EH;typedef Elf32_Phdr PH;typedef Elf32_Dyn DY;typedef Elf32_Sym SY;typedef Elf32_Rel RE;
#define SYM(i) ELF32_R_SYM(i)
#define TYPE(i) ELF32_R_TYPE(i)
#define SLOT R_ARM_JUMP_SLOT
#define RELOC DT_JMPREL
#define METHOD 0x2b5a85
#define ISOLATED (0x2b5a94+0x2b4ac7)
#define FIELD 8
#endif
static jmp_buf abort_env;
static int format,display_count=1,display_type=0,releases;
static const char *error_text;
static void *scratch_displays[2]={(void*)1,(void*)2};
static int acquire(void ***out){*out=scratch_displays;return display_count;}
static int type(void *display){(void)display;return display_type;}
static void preferred(void *display,int *space,int *out){(void)display;*space=0;*out=format;}
static void release(void **display){CHECK(display==scratch_displays);releases++;}
__attribute__((noreturn)) static void assertion(const char *cond,const char *tag,const char *fmt,...){(void)cond;CHECK(tag&&!strcmp(tag,"OpenGLRenderer"));error_text=fmt;longjmp(abort_env,1);}
static void patch(void *module,const char *name,void *fn){
 uintptr_t base=(uintptr_t)module;EH *e=module;PH *ph=(void*)(base+e->e_phoff);DY *dyn=NULL;
 for(unsigned i=0;i<e->e_phnum;i++)if(ph[i].p_type==PT_DYNAMIC)dyn=(void*)(base+ph[i].p_vaddr);
 CHECK(dyn);SY *syms=NULL;const char *str=NULL;RE *rela=NULL;size_t bytes=0;
 for(DY *d=dyn;d->d_tag;d++){
  uintptr_t a=d->d_un.d_ptr;if(a<base)a+=base;
  if(d->d_tag==DT_SYMTAB)syms=(void*)a;if(d->d_tag==DT_STRTAB)str=(void*)a;
  if(d->d_tag==RELOC)rela=(void*)a;if(d->d_tag==DT_PLTRELSZ)bytes=d->d_un.d_val;
 }
 CHECK(syms&&str&&rela);
 for(size_t i=0;i<bytes/sizeof(*rela);i++)if(!strcmp(str+syms[SYM(rela[i].r_info)].st_name,name)){
  CHECK(TYPE(rela[i].r_info)==SLOT);void **slot=(void*)(base+rela[i].r_offset);size_t p=sysconf(_SC_PAGESIZE);
  CHECK(!mprotect((void*)((uintptr_t)slot&~(p-1)),p,PROT_READ|PROT_WRITE));*slot=fn;
  CHECK(!mprotect((void*)((uintptr_t)slot&~(p-1)),p,PROT_READ));return;
 }
 CHECK(0);
}
int main(int argc,char **argv){setbuf(stdout,NULL);CHECK(argc==3);
 void *lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!lib)fprintf(stderr,"LOAD %s\n",dlerror());CHECK(lib);
 void *anchor=dlsym(lib,"_ZNK12SkColorSpace8toXYZD50EP15skcms_Matrix3x3");CHECK(anchor);Dl_info info;CHECK(dladdr(anchor,&info));
 const char *names[]={"_ZN7android32ADisplay_acquirePhysicalDisplaysEPPPNS_8ADisplayE","_ZN7android23ADisplay_getDisplayTypeEPNS_8ADisplayE","_ZN7android36ADisplay_getPreferredWideColorFormatEPNS_8ADisplayEP10ADataSpaceP22AHardwareBuffer_Format","_ZN7android16ADisplay_releaseEPPNS_8ADisplayE","__android_log_assert"};
 void *fns[]={(void*)acquire,(void*)type,(void*)preferred,(void*)release,(void*)assertion};
 for(int i=0;i<5;i++)patch(info.dli_fbase,names[i],fns[i]);
 unsigned char *isolated=(void*)((uintptr_t)info.dli_fbase+ISOLATED);*isolated=0;
 void (*update)(void*)=(void*)((uintptr_t)info.dli_fbase+METHOD);
 int upgraded=atoi(argv[2]);unsigned cases=0;
 for(int f=-1;f<=64;f++){
  unsigned char object[64],expected[64];memset(object,0xa5,sizeof(object));memcpy(expected,object,sizeof(object));format=f;releases=0;error_text=NULL;
  int wanted=f==1?4:f==22?13:f==43&&upgraded?7:-1;
  if(!setjmp(abort_env)){update(object);CHECK(wanted>=0);int value;memcpy(&value,object+FIELD,4);CHECK(value==wanted&&releases==1);memcpy(expected+FIELD,&wanted,4);CHECK(!memcmp(object,expected,sizeof(object)));}
  else CHECK(wanted<0&&error_text&&strstr(error_text,"unsupported pixel format")&&releases==0);
  cases++;
 }
 for(int missing=0;missing<2;missing++){
  unsigned char object[64]={0};display_count=missing?1:0;display_type=missing?1:0;error_text=NULL;
  if(!setjmp(abort_env)){update(object);CHECK(0);}else CHECK(error_text&&strstr(error_text,missing?"valid physical":"acquire physical"));cases++;
 }
 *isolated=1;unsigned char object[64],expected[64];memset(object,0xa5,sizeof(object));memcpy(expected,object,sizeof(object));update(object);CHECK(!memcmp(object,expected,sizeof(object)));cases++;
 printf("PASS HWUI ABI=%zu upgraded=%d actual_method_cases=%u RGB10_type7=%d other_formats_and_errors_preserved=1 no_live_display_API=1\n",sizeof(void*)*8,upgraded,cases,upgraded);
 return 0;
}
