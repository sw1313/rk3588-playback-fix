// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include <stddef.h>
namespace android { namespace rk_hdr {
// Bounded structural validation of the ST2094-40 payload after its six-byte
// T.35 prefix. Never read padding beyond the MPP metadata's declared length.
inline bool validPlus(const uint8_t *p, size_t bytes) {
    if (!p || bytes < 16 || bytes > 907) return false;
    size_t bit=0; bool ok=true;
    auto get=[&](size_t n)->uint32_t {
        if (n>32 || bit+n>bytes*8) {ok=false;return 0;}
        uint32_t v=0;while(n--){v=(v<<1)|((p[bit/8]>>(7-bit%8))&1);++bit;}return v;
    };
    auto skip=[&](size_t n) {if(bit+n>bytes*8)ok=false;else bit+=n;};
    if (get(8)>1) return false;
    uint32_t windows=get(2);if(windows<1 || windows>3)return false;
    skip(153*(windows-1));skip(27);
    auto grid=[&]() {
        uint32_t rows=get(5), cols=get(5);
        if(rows<2 || rows>25 || cols<2 || cols>25)ok=false;
        else skip(rows*cols*4);
    };
    if(get(1))grid();
    for(uint32_t w=0;w<windows && ok;++w){
        skip(68);uint32_t count=get(4);
        for(uint32_t n=0;n<count && ok;++n){if(get(7)>100)ok=false;skip(17);}
        skip(10);
    }
    if(get(1))grid();
    for(uint32_t w=0;w<windows && ok;++w){
        if(get(1)){skip(24);uint32_t anchors=get(4);skip(anchors*10);}
        if(get(1))skip(6);
    }
    return ok;
}

} }
