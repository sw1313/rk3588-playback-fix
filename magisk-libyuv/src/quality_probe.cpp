// SPDX-License-Identifier: Apache-2.0
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dlfcn.h>
#include <vector>
#include "libyuv.h"
using namespace libyuv;
#define FN(h,name) reinterpret_cast<decltype(&name)>(dlsym(h,#name))
#define ASSERT(x) do {if(!(x)){printf("FAIL line=%d\n",__LINE__);return 3;}}while(0)
int main(int argc,char **argv){
 setbuf(stdout,NULL);if(argc!=2&&argc!=3)return 2;
 void *stock=dlopen("/system/lib64/libyuv.so",RTLD_NOW|RTLD_LOCAL);
 void *next=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!stock||!next){puts(dlerror());return 2;}
 auto copy=FN(next,I010Copy);auto toP=FN(next,I010ToP010);auto fromP=FN(next,P010ToI010);
 auto scale=FN(next,ScalePlane_16);auto mask=FN(next,MaskCpuFlags);
 ASSERT(copy&&toP&&fromP&&scale&&mask);int tests=0;
 int detected=mask(-1);printf("CPU flags=%x\n",detected);
 for(int cpu : {0,-1}) {mask(cpu);
 for(int w : {1,17,64,853,1920})for(int h:{1,19,32}){
  int cw=(w+1)/2,ch=(h+1)/2;size_t y=(size_t)w*h,c=(size_t)cw*ch;
  std::vector<uint16_t> in(y+2*c),out(in.size(),65535),p(y+2*c,65535);
  for(size_t i=0;i<in.size();i++)in[i]=(i*317+23)&1023;
  ASSERT(!copy(in.data(),w,in.data()+y,cw,in.data()+y+c,cw,out.data(),w,out.data()+y,cw,out.data()+y+c,cw,w,h));ASSERT(in==out);tests++;
  ASSERT(!toP(in.data(),w,in.data()+y,cw,in.data()+y+c,cw,p.data(),w,p.data()+y,cw*2,w,h));
  for(auto val:p)ASSERT((val&63)==0);
  ASSERT(!fromP(p.data(),w,p.data()+y,cw*2,out.data(),w,out.data()+y,cw,out.data()+y+c,cw,w,h));ASSERT(in==out);tests++;
  std::fill(out.begin(),out.end(),65535);scale(in.data(),w,w,h,out.data(),w,w,h,kFilterBilinear);
  ASSERT(!memcmp(in.data(),out.data(),y*2));tests++;
  for(int rotation:{0,90,180,270}){
   std::vector<uint8_t> i8(in.size()),a(in.size(),255),b(in.size(),255);for(size_t i=0;i<i8.size();i++)i8[i]=i*31;
   int dw=(rotation==90||rotation==270)?h:w,dcw=(rotation==90||rotation==270)?ch:cw;
   auto srot=FN(stock,I420Rotate);auto nrot=FN(next,I420Rotate);ASSERT(srot&&nrot);
   // Compare every output to a scalar coordinate oracle. Some stock NEON
   // rotation paths overrun tiny odd frames, so invoke stock on aligned cases.
   auto oracle=[&](size_t offset,int sw,int sh){int ow=(rotation==90||rotation==270)?sh:sw;
    for(int sy=0;sy<sh;sy++)for(int sx=0;sx<sw;sx++){
     int dx=sx,dy=sy;if(rotation==90){dx=sh-1-sy;dy=sx;}
     if(rotation==180){dx=sw-1-sx;dy=sh-1-sy;}
     if(rotation==270){dx=sy;dy=sw-1-sx;}
     a[offset+dy*ow+dx]=i8[offset+sy*sw+sx];}};
   oracle(0,w,h);oracle(y,cw,ch);oracle(y+c,cw,ch);
   ASSERT(!nrot(i8.data(),w,i8.data()+y,cw,i8.data()+y+c,cw,b.data(),dw,b.data()+y,dcw,b.data()+y+c,dcw,w,h,(RotationMode)rotation));
   ASSERT(a==b);tests++;
   if(w%16==0&&h%16==0){ASSERT(!srot(i8.data(),w,i8.data()+y,cw,i8.data()+y+c,cw,a.data(),dw,a.data()+y,dcw,a.data()+y+c,dcw,w,h,(RotationMode)rotation));ASSERT(a==b);tests++;}
  }
 }}
 if(argc==2){mask(-1);printf("PASS cases=%d full_10bit_roundtrip_exact=1\n",tests);dlclose(next);dlclose(stock);return 0;}
 mask(-1);FILE *f=fopen(argv[2],"rb");ASSERT(f);fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);std::vector<uint8_t> jpeg(n);ASSERT(fread(jpeg.data(),1,n,f)==(size_t)n);fclose(f);
 int w=640,h=368;size_t y=w*h,c=y/4;std::vector<uint8_t> a(y*3/2),b(a.size());
 auto s=FN(stock,MJPGToI420);auto t=FN(next,MJPGToI420);ASSERT(s&&t);
 ASSERT(!s(jpeg.data(),jpeg.size(),a.data(),w,a.data()+y,w/2,a.data()+y+c,w/2,w,h,w,h));
 ASSERT(!t(jpeg.data(),jpeg.size(),b.data(),w,b.data()+y,w/2,b.data()+y+c,w/2,w,h,w,h));ASSERT(a==b);tests++;
 printf("PASS cases=%d full_10bit_roundtrip_exact=1 jpeg_stock_equal=1\n",tests);
 dlclose(next);dlclose(stock);return 0;
}
