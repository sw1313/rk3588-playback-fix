/* SPDX-License-Identifier: MIT
 * Exact-firmware isolated CheckGLESLayer compatibility test. Never opens a
 * compositor device. The raw test objects model ONLY the hash-pinned factory
 * method's read-only fields, confirmed by its ARM64 code. Not a generic HWC ABI.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void u32(unsigned char *p,int off,uint32_t v){memcpy(p+off,&v,4);}
static void f32(unsigned char *p,int off,float v){memcpy(p+off,&v,4);}
int main(int argc,char **argv){
 CHECK(argc==3);void *old=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL),*next=dlopen(argv[2],RTLD_NOW|RTLD_LOCAL);
 CHECK(old&&next);
 const char *name="_ZN7android7Vop358814CheckGLESLayerEPNS_11DrmHwcLayerE";
 bool(*before)(void*,void*)=dlsym(old,name),(*after)(void*,void*)=dlsym(next,name);CHECK(before&&after&&before!=after);
 uint32_t a,b;memcpy(&a,(char*)before+0x68,4);memcpy(&b,(char*)after+0x68,4);
 CHECK(a==0x34000328&&b==0xd503201f);
 /* The single instruction must be the only function-byte difference. */
 for(unsigned i=0;i<0x278;i++)if(i<0x68||i>=0x6c)CHECK(((char*)before)[i]==((char*)after)[i]);
 int widths[]={3,4,640,641},destinations[]={3,4,959,960};unsigned tests=0,relaxed=0;
 for(int s=0;s<4;s++)for(int d=0;d<4;d++)for(int afbc=0;afbc<2;afbc++)
 for(int type=1;type<=3;type++)for(int invalid=0;invalid<2;invalid++)for(int yuv=0;yuv<2;yuv++)for(int handle=0;handle<2;handle++){
  unsigned char layer[512]={0},saved[512],object[512]={0};
  uintptr_t pointer=handle;memcpy(layer,&pointer,sizeof(pointer));
  u32(layer,0x90,invalid?(uint32_t)-1:0);u32(layer,0x98,type);
  f32(layer,0xa0,0);f32(layer,0xa4,0);f32(layer,0xa8,widths[s]);f32(layer,0xac,360);
  u32(layer,0xb0,0);u32(layer,0xb4,0);u32(layer,0xb8,destinations[d]);u32(layer,0xbc,540);
  layer[0x109]=afbc;layer[0x10a]=yuv;f32(layer,0x110,(float)widths[s]/destinations[d]);
  memcpy(saved,layer,sizeof(layer));bool x=before(object,layer),z=after(object,layer);
  bool reject=widths[s]<4||destinations[d]<4||invalid||type==3||(type==1&&(!yuv||!handle));
  bool legacy=!afbc&&widths[s]<destinations[d]&&(widths[s]%16==1||destinations[d]%2==1);
  CHECK(x==(reject||legacy)&&z==reject);CHECK(!memcmp(saved,layer,sizeof(layer)));
  tests++;relaxed+=x&&!z;
 }
 printf("PASS exact factory method cases=%u obsolete_rejections_removed=%u other_guards_retained=1 input_unchanged=1\n",tests,relaxed);
 dlclose(next);dlclose(old);return 0;
}
