// SPDX-License-Identifier: Apache-2.0
#include <dlfcn.h>
#include <stdio.h>
int main(int argc,char **argv) {
    if(argc!=3)return 2;
    void *lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if(!lib){fprintf(stderr,"LOAD %s\n",dlerror());return 3;}
    int (*check)(const char*)=dlsym(lib,"rk_hdr_hwc_check");
    if(!check)return 4;
    int result=check(argv[2]);
    printf("HDR_HWC_CHECK %d\n",result);return result;
}
