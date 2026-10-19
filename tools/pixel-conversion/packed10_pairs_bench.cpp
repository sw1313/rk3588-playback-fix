// SPDX-License-Identifier: MIT
// Cached-memory comparison only; not a playback FPS measurement.
#include "RKPacked10Wide.h"
#include <vector>
#include <cstdio>
#include <time.h>
#include <algorithm>
static double now(){timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
using Convert=bool(*)(const uint8_t*,size_t,size_t,size_t,uint8_t*,uint8_t*,size_t,size_t,size_t,size_t,bool);
__attribute__((noinline)) double bench(Convert f,const std::vector<uint8_t>&s,std::vector<uint8_t>&d,size_t w,size_t h,int n){double start=now();for(int i=0;i<n;i++)if(!f(s.data(),s.size(),w*5/4,h,d.data(),d.data()+w*h*2,w*2,w*2,w,h,true))return -1;return (now()-start)*1000/n;}
int main(){
 for(auto size:{std::pair<size_t,size_t>{1280,720},{1920,1080},{3840,2160}}){
  size_t w=size.first,h=size.second;std::vector<uint8_t>s(w*h*15/8),a(w*h*3),b(a.size());unsigned seed=3588;for(auto &v:s){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;v=seed;}
  bench(rkPacked10ToP010,s,a,w,h,3);bench(rkPacked10ToP010Wide,s,b,w,h,3);if(a!=b)return 3;
  for(int batch=0;batch<8;batch++){double oldms,newms;if(batch%2){newms=bench(rkPacked10ToP010Wide,s,b,w,h,10);oldms=bench(rkPacked10ToP010,s,a,w,h,10);}else{oldms=bench(rkPacked10ToP010,s,a,w,h,10);newms=bench(rkPacked10ToP010Wide,s,b,w,h,10);}if(a!=b)return 4;printf("PAIR width=%zu height=%zu batch=%d old_ms=%.4f new_ms=%.4f exact=1 checksum=%u\n",w,h,batch,oldms,newms,unsigned(b.back()));}
 }
}
