// SPDX-License-Identifier: Apache-2.0
#include "../src/client_policy.h"
#include <cstdio>
#include <cstdlib>
int main(){
    unsigned cases=0;
    auto check=[&](bool value,bool expected){++cases;if(value!=expected){std::fprintf(stderr,"FAIL client policy case %u\n",cases);std::exit(1);}};
    check(rk_hdr::nativeSdrTarget(0x34324241,0),true);
    check(rk_hdr::nativeSdrTarget(0x30334241,0),false); // ten-bit target not inferred to be SDR
    check(rk_hdr::nativeSdrTarget(0x34324241,0x11c60000),false); // explicitly PQ
    check(rk_hdr::nativeSdrTarget(0x34324241,0x12060000),false); // explicitly HLG
    check(rk_hdr::nativeSdrTarget(0,0),false); // missing mapper description
    check(rk_hdr::nextClientTarget(true,false,false,0,0),true); // null submission retains current SDR target
    check(rk_hdr::nextClientTarget(false,false,false,0,0),false); // unknown target not promoted by null
    check(rk_hdr::nextClientTarget(true,true,false,0,0),false); // new unknown buffer invalidates classification
    check(rk_hdr::nextClientTarget(true,true,true,0x30334241,0),false); // real target changes to ten-bit
    check(rk_hdr::clientSdrRoute(true,true,true,true,false,false),true);
    check(rk_hdr::clientSdrRoute(true,true,true,true,true,false),false); // mixed direct HDR retains HDR
    check(rk_hdr::clientSdrRoute(true,true,true,false,true,false),false); // hardware video preferred
    check(rk_hdr::clientSdrRoute(true,true,true,true,false,true),false); // incomplete plane plan
    check(rk_hdr::clientSdrRoute(false,true,true,true,false,false),false); // failed validation
    check(rk_hdr::clientSdrRoute(true,false,true,true,false,false),false); // HDR or unknown target
    check(rk_hdr::clientSdrRoute(true,true,false,true,false,false),false); // multiple displays
    check(rk_hdr::clientSdrRoute(true,true,true,false,false,false),false); // desktop, no HDR source
    check(rk_hdr::overrideForceDisable(true,"vendor.hwc.hdr_force_disable"),true);
    check(rk_hdr::overrideForceDisable(false,"vendor.hwc.hdr_force_disable"),false);
    check(rk_hdr::overrideForceDisable(true,"vendor.hwc.hdr_video_area"),false);
    check(rk_hdr::overrideForceDisable(true,nullptr),false);
    std::printf("PASS %u client SDR policy cases\n",cases);
}
