/* SPDX-License-Identifier: MIT
 * Isolated regression against one hash-pinned factory HWC. This executable
 * never opens the HAL/device. Display/connector/modes below are test objects;
 * mode setters and DRM updates are mocked in this process only. No compositor
 * process, live mode list, settings or display state is modified.
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
#include "../../magisk-hdr/src/mode_cache.h"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
typedef struct {uint32_t clock;uint16_t hd,hss,hse,ht,hs,vd,vss,vse,vt,vs;uint32_t refresh,flags,type;char name[32];} Info;
_Static_assert(sizeof(Info)==68,"drmModeModeInfo");
typedef struct {void *begin,*end,*cap;} Vec;
static void put(void *p,size_t o,const void *v,size_t n){memcpy((char*)p+o,v,n);}
static void vec(void *p,size_t o,void *b,size_t count){Vec v={b,(char*)b+count*0x60,(char*)b+count*0x60};put(p,o,&v,sizeof(v));}
static uint32_t selected[2],calls[4],seq[8],nseq;
static uint32_t (*mode_id)(const void*);
static void best(void *p,const void *m){(void)p;selected[0]=mode_id(m);calls[0]++;seq[nseq++]=0;}
static void current(void *p,const void *m){(void)p;selected[1]=mode_id(m);calls[1]++;seq[nseq++]=1;}
static int update(void *p,int d){(void)p;CHECK(d==123);calls[2]++;seq[nseq++]=2;return 0;}
static void display_info(void *p,int d){(void)p;CHECK(d==123);calls[3]++;seq[nseq++]=3;}
static void **patch(void *base,const char *name,void *fn){
 Elf64_Ehdr *e=base;Elf64_Phdr *ph=(void*)((char*)base+e->e_phoff);Elf64_Dyn *dyn=NULL;
 for(unsigned i=0;i<e->e_phnum;i++)if(ph[i].p_type==PT_DYNAMIC)dyn=(void*)((char*)base+ph[i].p_vaddr);
 CHECK(dyn);Elf64_Sym *syms=NULL;char *str=NULL;Elf64_Rela *rela=NULL;size_t bytes=0;
 for(Elf64_Dyn *d=dyn;d->d_tag;d++){
  uintptr_t a=d->d_un.d_ptr;if(a<(uintptr_t)base)a+=(uintptr_t)base;
  if(d->d_tag==DT_SYMTAB)syms=(void*)a;if(d->d_tag==DT_STRTAB)str=(void*)a;
  if(d->d_tag==DT_JMPREL)rela=(void*)a;if(d->d_tag==DT_PLTRELSZ)bytes=d->d_un.d_val;
 }
 CHECK(syms&&str&&rela);
 for(size_t i=0;i<bytes/sizeof(*rela);i++)if(!strcmp(str+syms[ELF64_R_SYM(rela[i].r_info)].st_name,name)){
  CHECK(ELF64_R_TYPE(rela[i].r_info)==R_AARCH64_JUMP_SLOT);
  void **slot=(void*)((char*)base+rela[i].r_offset);size_t page=(size_t)sysconf(_SC_PAGESIZE);
  CHECK(!mprotect((void*)((uintptr_t)slot&~(page-1)),page,PROT_READ|PROT_WRITE));*slot=fn;
  CHECK(!mprotect((void*)((uintptr_t)slot&~(page-1)),page,PROT_READ));return slot;
 }
 CHECK(0);return NULL;
}
int main(int argc,char **argv){
 setbuf(stdout,NULL);
 CHECK(argc==2);void *lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!lib)fprintf(stderr,"%s\n",dlerror());CHECK(lib);
 int (*attr)(void*,uint32_t,int,int*)=dlsym(lib,"_ZN7android9DrmHwcTwo10HwcDisplay19GetDisplayAttributeEjiPi");
 int (*active)(void*,uint32_t)=dlsym(lib,"_ZN7android9DrmHwcTwo10HwcDisplay15SetActiveConfigEj");
 void (*ctor)(void*,Info*)=dlsym(lib,"_ZN7android7DrmModeC1EP16_drmModeModeInfo");
 void (*set_id)(void*,uint32_t)=dlsym(lib,"_ZN7android7DrmMode6set_idEj");
 mode_id=dlsym(lib,"_ZNK7android7DrmMode2idEv");CHECK(attr&&active&&ctor&&set_id&&mode_id);
 rk_mode_cache_ops ops={.original_attribute=attr,.original_active=active,.id=mode_id,.best=best,.current=current,.update=update,.display_info=display_info};
 ops.width=dlsym(lib,"_ZNK7android7DrmMode9h_displayEv");ops.height=dlsym(lib,"_ZNK7android7DrmMode9v_displayEv");
 ops.refresh=dlsym(lib,"_ZNK7android7DrmMode9v_refreshEv");ops.mm_width=dlsym(lib,"_ZNK7android12DrmConnector8mm_widthEv");ops.mm_height=dlsym(lib,"_ZNK7android12DrmConnector9mm_heightEv");
 CHECK(ops.width&&ops.height&&ops.refresh&&ops.mm_width&&ops.mm_height);
 Dl_info di;CHECK(dladdr((void*)attr,&di));
 CHECK((char*)attr-(char*)di.dli_fbase==0x3a4c4 && (char*)active-(char*)di.dli_fbase==0x39904);
 patch(di.dli_fbase,"_ZN7android12DrmConnector13set_best_modeERKNS_7DrmModeE",(void*)best);
 patch(di.dli_fbase,"_ZN7android12DrmConnector16set_current_modeERKNS_7DrmModeE",(void*)current);
 patch(di.dli_fbase,"_ZN7android9DrmDevice17UpdateDisplayModeEi",(void*)update);
 patch(di.dli_fbase,"_ZN7android12DrmConnector18SetDisplayModeInfoEi",(void*)display_info);
 _Alignas(16) unsigned char disp[0x1000]={0},conn[0xa000]={0},cached[2][0x60]={{0}},raw[2][0x60]={{0}};
 Info info={.clock=594000,.hd=3840,.ht=4400,.vd=2160,.vt=2250,.refresh=24};strcpy(info.name,"3840x2160");
 for(int i=0;i<2;i++){info.refresh=i?60:24;ctor(cached[i],&info);set_id(cached[i],i?3:11);ctor(raw[i],&info);set_id(raw[i],i?44:33);}
 void *cp=conn,*drm=(void*)0x123;put(disp,0x1f8,&cp,8);put(disp,8,&drm,8);
 uint32_t handle=123;put(disp,0x220,&handle,4);disp[0x344]=1;vec(disp,0x208,cached,2);vec(conn,0x158,raw,2);
 unsigned cases=0;
 for(int a=1;a<=5;a++){int v=0x123456;CHECK(attr(disp,11,a,&v)==1&&v==0x123456);cases++;}
 CHECK(active(disp,11)==1&&nseq==0);cases++;
 printf("REPRO factory cached_config=11 raw_configs=33,44 attribute=BadConfig active=BadConfig side_effects=0\n");
 /* Control: same mode bytes, exposed through the list the old method reads.
  * This is a test-object substitution, NOT a proposed live vector swap fix. */
 vec(conn,0x158,cached,2);
 int expect[]={3840,2160,41666668,-1,-1};
 for(int a=1;a<=5;a++){int v=0;CHECK(attr(disp,11,a,&v)==0);if(a!=3)CHECK(v==expect[a-1]);else CHECK(abs(v-41666666)<=4);cases++;}
 int v=0;CHECK(attr(disp,11,999,&v)==1&&v==-1);cases++;
 CHECK(active(disp,11)==0&&selected[0]==11&&selected[1]==11&&nseq==4);cases++;
 for(unsigned i=0;i<4;i++)CHECK(calls[i]==1&&seq[i]==i);
 int32_t frame[4],res[2];float crop[4];memcpy(frame,disp+0x2a0,sizeof(frame));memcpy(crop,disp+0x2b4,sizeof(crop));memcpy(res,disp+0x35c,sizeof(res));
 CHECK(frame[0]==0&&frame[1]==0&&frame[2]==3840&&frame[3]==2160);
 CHECK(crop[0]==0&&crop[1]==0&&crop[2]==3840&&crop[3]==2160&&res[0]==3840&&res[1]==2160);
 unsigned char saved[sizeof(disp)];memcpy(saved,disp,sizeof(disp));
 CHECK(active(disp,9999)==1&&nseq==4&&!memcmp(saved,disp,sizeof(disp)));cases++;
 v=0x123456;CHECK(attr(disp,9999,1,&v)==1&&v==0x123456);cases++;
 printf("PASS factory control cases=%u valid_mode_attributes=1 client_geometry=1 call_order=1 unknown_config_rejected=1 no_hal_open=1 no_drm_access=1\n",cases);
 /* Compare the candidate's actual field writes and callbacks with the actual
  * factory implementation. Tests never mutate any live HWC object. */
 unsigned candidate_cases=0;
 for(unsigned dpi=0;dpi<2;dpi++)
 for(unsigned width=640;width<=3840;width+=640)for(unsigned height=360;height<=2160;height+=360)
 for(unsigned fps=24;fps<=120;fps+=6){
  uint32_t mw=dpi?600:0,mh=dpi?340:0;put(conn,0x30,&mw,4);put(conn,0x34,&mh,4);
  info.hd=width;info.vd=height;info.refresh=fps;ctor(cached[0],&info);set_id(cached[0],11);
  for(int a=0;a<=6;a++){
   int x=0x123456,y=x;int before=attr(disp,11,a,&x),after=rk_mode_attribute(&ops,disp,11,a,&y);
   CHECK(before==after&&x==y);candidate_cases++;
  }
  unsigned char a[sizeof(disp)],b[sizeof(disp)];memcpy(a,disp,sizeof(a));memcpy(b,disp,sizeof(b));
  nseq=0;memset(calls,0,sizeof(calls));CHECK(active(a,11)==0&&nseq==4);uint32_t order[4];memcpy(order,seq,sizeof(order));
  nseq=0;memset(calls,0,sizeof(calls));CHECK(rk_mode_active(&ops,b,11)==0&&nseq==4&&!memcmp(order,seq,sizeof(order)));
  CHECK(!memcmp(a,b,sizeof(a))&&selected[0]==11&&selected[1]==11);candidate_cases++;
 }
 printf("PASS candidate factory-equivalence cases=%u\n",candidate_cases);
 info.refresh=0;info.clock=593407;ctor(cached[0],&info);set_id(cached[0],11);
 int original_period=0,candidate_period=0;
 CHECK(attr(disp,11,3,&original_period)==0&&rk_mode_attribute(&ops,disp,11,3,&candidate_period)==0&&original_period==candidate_period);candidate_cases++;
 vec(conn,0x158,raw,2);nseq=0;
 CHECK(active(disp,11)==1&&nseq==0);CHECK(rk_mode_active(&ops,disp,11)==0&&nseq==4);candidate_cases++;
 v=0;CHECK(attr(disp,11,1,&v)==1);CHECK(rk_mode_attribute(&ops,disp,11,1,&v)==0&&v==info.hd);candidate_cases++;
 memcpy(saved,disp,sizeof(disp));nseq=0;
 CHECK(rk_mode_active(&ops,disp,33)==1&&!nseq&&!memcmp(saved,disp,sizeof(disp)));candidate_cases++;
 v=0x123456;CHECK(rk_mode_attribute(&ops,disp,33,1,&v)==1&&v==0x123456);candidate_cases++;
 /* Empty and corrupt vectors fail closed. Nonstandard display configuration
  * always uses the factory implementation (including its crop/VRR path). */
 vec(disp,0x208,cached,0);CHECK(rk_mode_active(&ops,disp,11)==1);candidate_cases++;
 uintptr_t bad=1;put(disp,0x210,&bad,8);CHECK(rk_mode_attribute(&ops,disp,11,1,&v)==1);candidate_cases++;
 printf("PASS candidate stale-and-invalid-vector cases=%u\n",candidate_cases);
 vec(disp,0x208,cached,2);disp[0x344]=0;
 uint32_t fw=1920,fh=1080,fr=60;put(disp,0x348,&fw,4);put(disp,0x34c,&fh,4);put(disp,0x350,&fr,4);
 for(int attr_id=0;attr_id<7;attr_id++){int x=123,y=123;CHECK(attr(disp,999,attr_id,&x)==rk_mode_attribute(&ops,disp,999,attr_id,&y)&&x==y);candidate_cases++;}
 printf("PASS candidate nonstandard attributes\n");
 unsigned char a[sizeof(disp)],b[sizeof(disp)];memcpy(a,disp,sizeof(a));memcpy(b,disp,sizeof(b));
 CHECK(active(a,999)==rk_mode_active(&ops,b,999)&&!memcmp(a,b,sizeof(a)));candidate_cases++;
 void (*set_crop)(void*,int,int,int,int,int,int)=dlsym(lib,"_ZN7android12DrmConnector12setCropSpiltEiiiiii");CHECK(set_crop);
 set_crop(conn,10,20,1800,1000,0,0);memcpy(a,disp,sizeof(a));memcpy(b,disp,sizeof(b));
 CHECK(active(a,999)==rk_mode_active(&ops,b,999)&&!memcmp(a,b,sizeof(a)));candidate_cases++;
 printf("PASS candidate cases=%u equivalent_attributes_and_full_object_writes=1 stale_list_fixed=1 raw_unreported_rejected=1 nonstandard_passthrough=1 hardware_validation=NOT_RUN\n",candidate_cases);
 return 0;
}
