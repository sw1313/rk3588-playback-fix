// SPDX-License-Identifier: MIT
#include "Hdr10PlusPayload.h"
#include <cstdio>
#include <vector>
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    FILE *f=fopen(argv[1],"rb");if(!f)return 2;
    std::vector<uint8_t> data;int ch;while((ch=fgetc(f))!=EOF)data.push_back(uint8_t(ch));fclose(f);
    if(data.size()<6 || data.size()>913)return 3;
    const uint8_t prefix[]={0xb5,0,0x3c,0,1,4};
    for(unsigned i=0;i<6;++i)if(data[i]!=prefix[i])return 3;
    using android::rk_hdr::validPlus;
    if(!validPlus(data.data()+6,data.size()-6))return 4;
    if(validPlus(nullptr,100) || validPlus(data.data()+6,908))return 5;
    for(size_t n=0;n<16;++n)if(validPlus(data.data()+6,n))return 6;
    // Exercise the production validator on bounded random/truncated input.
    uint32_t state=1;std::vector<uint8_t> random(907);
    for(unsigned n=0;n<20000;++n){
        for(auto &b:random){state=state*1664525u+1013904223u;b=state>>24;}
        validPlus(random.data(),n%908);
    }
    puts("PASS HDR10+ payload, short/oversize/null guards and 20000 bounded random inputs");return 0;
}
