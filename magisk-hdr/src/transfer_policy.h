// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstdint>
#include <map>

namespace rk_hdr {
struct Plane {uint64_t fb=0,crtc=0;int eotf=-1;};
// Exact hash-gated stock HWC: PQ YUV -> plane EOTF 2; HLG -> plane EOTF 0.
// Require both the validated DEVICE layer and a matching active atomic plane.
inline int outputTransfer(int original,int desired,const std::map<uint32_t,Plane> &planes) {
    if(original!=2 && original!=3)return original;
    bool pq=false,linear=false,other=false;
    for(const auto &item:planes){const auto &p=item.second;
        if(p.fb && p.crtc){pq|=p.eotf==2;linear|=p.eotf==0;other|=p.eotf!=0 && p.eotf!=2;}
    }
    if(!other && ((desired==2 && pq) || (desired==3 && linear && !pq)))return desired;
    return original;
}
}
