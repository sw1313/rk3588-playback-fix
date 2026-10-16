// SPDX-License-Identifier: Apache-2.0
#include "RKPacked10.h"
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
static uint32_t seed=0x358810;static uint32_t rnd(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static double now(){timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static void check(bool yes,const char*why){if(!yes){fprintf(stderr,"FAIL %s\n",why);exit(1);}}
static void run(size_t w,size_t h,size_t pad,size_t offset) {
 const size_t ss=(w*10+7)/8+pad,sh=h+(pad?2:0),size=ss*(sh+h/2),ds=w*2+pad+3;
 const size_t pg=size_t(sysconf(_SC_PAGESIZE)),aligned=(size+pg-1)/pg*pg;
 uint8_t*map=(uint8_t*)mmap(nullptr,aligned+pg,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
 check(map!=MAP_FAILED,"mmap");check(mprotect(map+aligned,pg,PROT_NONE)==0,"guard");
 uint8_t*src=map+aligned-size;memset(src,0,size);
 std::vector<uint16_t> samples(w*(h+h/2));
 for(size_t p=0;p<2;p++)for(size_t y=0;y<(p?h/2:h);y++)for(size_t x=0;x<w;x++){
  uint16_t v=uint16_t((w*h<1024?rnd():x+y*w+p*17)&1023);samples[(p?w*h:0)+y*w+x]=v;
  uint8_t*row=src+(p?ss*sh:0)+y*ss;
  for(size_t b=0;b<10;b++)if(v&(1u<<b))row[(x*10+b)/8]|=uint8_t(1u<<((x*10+b)%8));
 }
 for(bool fast:{false,true}){
  std::vector<uint8_t>a(ds*h+offset+32,0xa5),b(ds*h/2+offset+32,0xa5);
  check(rkPacked10ToP010(src,size,ss,sh,a.data()+offset,b.data()+offset,ds,ds,w,h,fast),"valid conversion");
  for(size_t p=0;p<2;p++){
   auto&d=p?b:a;for(size_t y=0;y<(p?h/2:h);y++){
    for(size_t x=0;x<w;x++){size_t k=offset+y*ds+x*2;uint16_t got=d[k]|(uint16_t(d[k+1])<<8);check(got==uint16_t(samples[(p?w*h:0)+y*w+x]<<6),"ten-bit value");}
    for(size_t x=w*2;x<ds;x++)check(d[offset+y*ds+x]==0xa5,"row padding");
   }
   for(size_t i=0;i<offset;i++)check(d[i]==0xa5,"prefix");
   for(size_t i=offset+(p?h/2:h)*ds;i<d.size();i++)check(d[i]==0xa5,"suffix");
  }
  auto before=a;
  check(!rkPacked10ToP010(src,size-1,ss,sh,a.data()+offset,b.data()+offset,ds,ds,w,h,fast),"short source rejected");
  check(!rkPacked10ToP010(src,size,ss,sh,a.data()+offset,b.data()+offset,ds,ds,w+1,h,fast),"odd width rejected");
  check(a==before,"invalid call wrote destination");
 }
 munmap(map,aligned+pg);
}
int main(){
 size_t cases=0;for(size_t w:{2,4,6,8,14,16,18,30,32,34,62,64,66,854,1280,1920,3840,4096,7680,8192})for(size_t pad:{0,1,17}){run(w,w<128?6:2,pad,pad%3);cases++;}
 for(int i=0;i<80;i++){run(2*(rnd()%100+1),2*(rnd()%8+1),rnd()%16,rnd()%4);cases++;}
 printf("PASS cases=%zu both_fast_and_scalar exact_10bit guard_pages padding invalid_inputs\n",cases);
 const size_t w=3840,h=2160,ss=w*10/8,n=ss*h*3/2;
 std::vector<uint8_t>src(n),dst(w*h*3);for(auto&v:src)v=uint8_t(rnd());
 for(bool fast:{false,true}){double t=now();for(int i=0;i<12;i++)check(rkPacked10ToP010(src.data(),n,ss,h,dst.data(),dst.data()+w*h*2,w*2,w*2,w,h,fast),"benchmark");double dt=now()-t;printf("BENCH fast=%d frames=12 ms_per_4k_frame=%.3f checksum=%u\n",int(fast),dt/12*1000,unsigned(dst[11]));}
}
