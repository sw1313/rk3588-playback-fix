// SPDX-License-Identifier: MIT
#include "../src/transfer_policy.h"
#include <cstdio>
#include <cstdlib>
using rk_hdr::Plane;
int main(){
    unsigned cases=0;
    auto check=[&](int original,int desired,std::map<uint32_t,Plane> planes,int expected){
        ++cases;int actual=rk_hdr::outputTransfer(original,desired,planes);
        if(actual!=expected){std::fprintf(stderr,"FAIL case %u expected=%d actual=%d\n",cases,expected,actual);std::exit(1);}
    };
    check(3,2,{{1,{10,70,2}}},2); // HLG -> PQ
    check(2,3,{{1,{10,70,0}}},3); // PQ -> HLG, original HWC's HLG encoding
    check(2,3,{{1,{10,70,2}}},2); // queued older PQ frame, not the current HLG
    check(3,2,{{1,{10,70,0}}},3); // queued older HLG frame
    check(2,-1,{{1,{10,70,0}}},2); // no validated DEVICE HDR layer / mixed HDR
    check(2,0,{{1,{10,70,0}}},2); // SDR UI cannot select HLG
    check(2,3,{{1,{0,70,2}},{2,{10,70,0}}},3); // disabled plane ignored
    check(2,3,{{1,{10,0,2}},{2,{10,70,0}}},3);
    check(2,3,{{1,{10,70,2}},{2,{10,70,0}}},2); // conflicting active PQ
    check(2,3,{{1,{10,70,4}},{2,{10,70,0}}},2); // unknown active transfer
    check(2,3,{{1,{10,70,-1}},{2,{10,70,0}}},2); // incomplete plane state
    check(2,3,{},2);
    check(2,3,{{1,{0,0,0}}},2);
    for(int original: {0,1,4,5})for(int desired: {-1,2,3})
        check(original,desired,{{1,{10,70,desired==2?2:0}}},original); // never enable HDR
    std::printf("PASS %u HDR transaction policy cases\n",cases);
}
