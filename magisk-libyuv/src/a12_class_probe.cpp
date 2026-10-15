// SPDX-License-Identifier: Apache-2.0
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <new>
#include <vector>
#include "libyuv.h"
using namespace libyuv;
#define REQUIRE(x) do{if(!(x)){printf("FAIL %d\n",__LINE__);return 3;}}while(0)
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen(argv[1],"rb");REQUIRE(f);fseek(f,0,SEEK_END);size_t n=ftell(f);rewind(f);std::vector<uint8_t> jpeg(n);REQUIRE(fread(jpeg.data(),1,n,f)==n);fclose(f);
 alignas(MJpegDecoder) unsigned char storage[sizeof(MJpegDecoder)+64];memset(storage,0xa5,sizeof(storage));
 MJpegDecoder*d=new(storage) MJpegDecoder();
 auto guard=[&](){for(size_t i=sizeof(MJpegDecoder);i<sizeof(storage);i++)if(storage[i]!=0xa5)return false;return true;};
 REQUIRE(guard());REQUIRE(d->LoadFrame(jpeg.data(),jpeg.size())==1);REQUIRE(guard());
 REQUIRE(d->GetWidth()==640&&d->GetHeight()==368&&d->GetNumComponents()==3);
 size_t y=640*368,c=y/4;std::vector<uint8_t> a(y+c*2),b(a.size());uint8_t*planes[]={a.data(),a.data()+y,a.data()+y+c};
 REQUIRE(d->DecodeToBuffers(planes,640,368)==1);REQUIRE(guard());
 REQUIRE(MJPGToI420(jpeg.data(),jpeg.size(),b.data(),640,b.data()+y,320,b.data()+y+c,320,640,368,640,368)==0);REQUIRE(a==b);
 uint8_t invalid[20]={0};REQUIRE(d->LoadFrame(invalid,sizeof(invalid))==0);REQUIRE(guard());d->~MJpegDecoder();REQUIRE(guard());
 printf("PASS old_API32_class_size=%zu canary=1 jpeg_exact=1 bool_int_return=1 invalid_input=1\n",sizeof(MJpegDecoder));return 0;
}
