// SPDX-License-Identifier: MIT
// Exact native Layer prologue/trampoline + actual policy callback. No Binder,
// graphics service, buffers, display changes, player input or screenshots.
#define RK_REFRESH_SESSION_TEST 1
#include "../../magisk-hdr/src/refresh_session.cpp"
#include <stdlib.h>
#include <initializer_list>
using namespace rk_refresh_session;
bool flag=true,automatic=true;unsigned nativeCalls=0;bool nativeResult=true;
FrameRate received{};
int property(const char* name,char* out){const char* v=!strcmp(name,"debug.rkdecode.refresh_session")?(flag?"1":"0"):!strcmp(name,"persist.vendor.resolution.HDMI-A-0")?(automatic?"Auto":"fixed"):"0";strcpy(out,v);return strlen(v);}
bool native(void* layer,const FrameRate* r){++nativeCalls;received=*r;if(nativeResult)memcpy(static_cast<char*>(layer)+kFrameRate,r,sizeof(*r));return nativeResult;}
FrameRate fps(float value,int type=2,int strategy=1){return {value,0,value>0?int64_t(1000000000.f/value):0,type,strategy};}
#define CHECK(x) do{if(!(x)){printf("FAIL refresh-session line %d\n",__LINE__);return 1;}++checks;}while(0)
int main(int argc,char**argv){
 unsigned checks=0;CHECK(argc==2);readProperty=property;
 constexpr size_t size=0x5e0000;
 auto* image=static_cast<unsigned char*>(mmap(nullptr,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));CHECK(image!=MAP_FAILED);
 FILE* f=fopen(argv[1],"rb");CHECK(f);CHECK(fseek(f,kLayerSet,SEEK_SET)==0);CHECK(fread(image+kLayerSet,1,0xb0,f)==0xb0);fclose(f);
 CHECK(memcmp(image+kLayerSet,kLayerBytes,sizeof(kLayerBytes))==0);
 image[kLayerSet]^=1;CHECK(attach(uintptr_t(image))==-2);image[kLayerSet]^=1;
 CHECK(attach(uintptr_t(image))==0);CHECK(attach(uintptr_t(image))==-1);
 alignas(16) char layer[0x800]{};auto* current=reinterpret_cast<FrameRate*>(layer+kFrameRate);
 auto entry=reinterpret_cast<Set>(image+kLayerSet);FrameRate zero=fps(0,0,2);
 // Real original native machine code: feature disabled and feature enabled
 // with unchanged input both return false through the relocated prologue.
 CHECK(!entry(layer,&zero));image[kNativePage+0x478]=1;
 *current=fps(23.976024f);CHECK(!entry(layer,current));
 CHECK(!entry(layer,&zero));CHECK(current->rate==23.976024f && held==1);
 // Then verify changed/native-return cases with an injected native operation.
 original=native;nativeCalls=0;held=0;sources=0;
 FrameRate a=fps(23.976024f);*current={};CHECK(set(layer,&a) && nativeCalls==1);CHECK(!memcmp(&received,&a,24));
 for(int i=0;i<12;++i){CHECK(set(layer,&zero));CHECK(!memcmp(&received,&a,24));CHECK(set(layer,&a));}
 CHECK(held==12 && nativeCalls==25 && sources==13);
 FrameRate b=fps(25);CHECK(set(layer,&b));CHECK(set(layer,&zero));CHECK(!memcmp(&received,&b,24));
 alignas(16) char other[0x800]{};CHECK(set(other,&zero));CHECK(received.rate==0);
 CHECK(set(other,&a));CHECK(set(layer,&zero));CHECK(received.rate==25);
 flag=false;CHECK(set(layer,&zero));CHECK(received.rate==0);flag=true;
 CHECK(set(layer,&b));automatic=false;CHECK(set(layer,&zero));CHECK(received.rate==0);automatic=true;
 // Default/nonvideo and NoVote requests intentionally end the retained vote.
 for(int type: {0,1,3}){FrameRate v=fps(30,type);CHECK(set(layer,&v));CHECK(set(layer,&zero));CHECK(received.rate==0);}
 CHECK(set(layer,&a));nativeResult=false;unsigned n=nativeCalls;CHECK(!set(layer,&zero));CHECK(nativeCalls==n+1 && !memcmp(&received,&a,24));nativeResult=true;
 // Destroy/recreate is modelled by fresh zero-initialized storage; policy has
 // no layer cache, strong or weak reference to leak into a new object.
 memset(layer,0,sizeof(layer));CHECK(set(layer,&zero));CHECK(received.rate==0);
 for(float value: {1.f,24.f,29.97003f,59.94006f,120.f}){FrameRate v=fps(value);CHECK(set(layer,&v));CHECK(set(layer,&zero));CHECK(!memcmp(&received,&v,24));}
 for(float value: {-1.f,0.f,121.f,INFINITY,NAN}){FrameRate v=fps(24);v.rate=value;*current=v;CHECK(set(layer,&zero));CHECK(received.rate==0);}
 FrameRate invalid=fps(24);invalid.period=0;*current=invalid;CHECK(set(layer,&zero));CHECK(received.rate==0);
 *current=a;FrameRate noVote=fps(0,3,2);CHECK(set(layer,&noVote));CHECK(received.type==3);
 *current=a;FrameRate invalidZero=fps(0,0,1);CHECK(set(layer,&invalidZero));CHECK(received.rate==0);
 puts("PASS real ARM64 original prologue/trampoline and Layer callback; no live display IO");printf("checks=%u\n",checks);return 0;
}
