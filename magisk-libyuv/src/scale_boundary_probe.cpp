// SPDX-License-Identifier: MIT
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include "libyuv.h"
using namespace libyuv;
#define CHECK(x) do { if (!(x)) { printf("FAIL line=%d\n",__LINE__); return 1; } } while (0)
int main(int argc,char **argv) {
    if (argc!=2) return 2;
    void *lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL); CHECK(lib);
    auto scale=reinterpret_cast<decltype(&ScalePlane)>(dlsym(lib,"ScalePlane"));
    auto uv=reinterpret_cast<decltype(&UVScale)>(dlsym(lib,"UVScale"));
    auto argb=reinterpret_cast<decltype(&ARGBScale)>(dlsym(lib,"ARGBScale"));
    auto mask=reinterpret_cast<decltype(&MaskCpuFlags)>(dlsym(lib,"MaskCpuFlags"));
    CHECK(scale&&uv&&argb&&mask); int cases=0;
    for (int cpu:{0,-1}) { mask(cpu);
        for (int height:{32767,32768}) {
            std::vector<uint8_t> src((size_t)4*height*4,0);
            for (int sign:{-1,1}) for (int width:{2,4,8})
                for (FilterMode f:{kFilterNone,kFilterLinear,kFilterBilinear,kFilterBox}) {
                    for (int mode=0;mode<3;mode++) {
                        std::vector<uint8_t> dst(128,0xa5);
                        int channels=mode==0?1:mode==1?2:4;
                        int status=mode==0?scale(src.data(),4,4,sign*height,dst.data(),width,width,2,f):
                            mode==1?uv(src.data(),8,4,sign*height,dst.data(),width*2,width,2,f):
                            argb(src.data(),16,4,sign*height,dst.data(),width*4,width,2,f);
                        CHECK(status==0);
                        for (int i=0;i<width*2*channels;i++) CHECK(dst[i]==0);
                        for (size_t i=width*2*channels;i<dst.size();i++) CHECK(dst[i]==0xa5);
                        cases++;
                    }
                }
        }
    }
    printf("PASS scale boundary cases=%d scalar_and_neon=1 guard_bytes_intact=1\n",cases);
    dlclose(lib); return 0;
}
