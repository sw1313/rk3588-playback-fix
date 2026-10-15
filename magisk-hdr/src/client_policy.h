// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstdint>
#include <cstring>
namespace rk_hdr {
inline bool nativeSdrTarget(uint32_t fourcc,int32_t dataspace) {
    // Verified managed NATIVE compositor: AB24 / UNKNOWN is its sRGB target.
    return fourcc==0x34324241 && dataspace==0;
}
inline bool nextClientTarget(bool previous,bool hasBuffer,bool known,uint32_t fourcc,int32_t dataspace) {
    // The exact stock HWC ignores null client-target submissions and retains
    // its previous target. Preserve the matching classification as well.
    return hasBuffer ? known && nativeSdrTarget(fourcc,dataspace) : previous;
}
inline bool clientSdrRoute(bool known,bool nativeSdr,bool singlePrimary,
                           bool clientHdr,bool deviceHdr,bool unknownHdr) {
    return known && nativeSdr && singlePrimary && clientHdr && !deviceHdr && !unknownHdr;
}
inline bool overrideForceDisable(bool scoped,const char *key) {
    return scoped && key && !std::strcmp(key,"vendor.hwc.hdr_force_disable");
}
}
