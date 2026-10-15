// Preserve all ten decoded bits in Android P010; never dither to eight bits.
#pragma once
#include <C2Buffer.h>
#include <stdint.h>
#include <string.h>
static bool storeP010(C2GraphicView &view, const uint8_t *const src[3],
                      const int stride[3], unsigned width, unsigned height,
                      bool monochrome = false, unsigned sourceBits = 10) {
    if (sourceBits!=8 && sourceBits!=10) return false;
    auto layout = view.layout();
    if (layout.numPlanes != 3) return false;
    for (unsigned p=0; p<3; ++p) {
        auto q=layout.planes[p];
        if (q.allocatedDepth!=16 || q.bitDepth!=10 || q.rightShift!=6 ||
            q.rowInc<=0 || q.colInc<=0 || !view.data()[p]) return false;
        unsigned w=p?(width+1)/2:width, h=p?(height+1)/2:height;
        auto dst=const_cast<uint8_t *>(view.data()[p]);
        for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
            uint16_t v=512;
            if (!(p && monochrome)) {
                if (!src[p] || stride[p]<=0) return false;
                if (sourceBits==10) memcpy(&v,src[p]+size_t(y)*stride[p]+size_t(x)*2,2);
                else v=uint16_t(src[p][size_t(y)*stride[p]+x])<<2;
            }
            if (v>1023) return false;
            v<<=6;
            memcpy(dst+size_t(y)*q.rowInc+size_t(x)*q.colInc,&v,2);
        }
    }
    return true;
}
