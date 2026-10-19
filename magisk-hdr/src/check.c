// SPDX-License-Identifier: MIT
#include <dlfcn.h>
#include <stdio.h>
int main(int argc,char **argv) {
    if(argc!=3)return 2;
    void *lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if(!lib){fprintf(stderr,"LOAD %s\n",dlerror());return 3;}
    int (*check)(const char*)=dlsym(lib,"rk_hdr_hwc_check");
    if(!check)return 4;
    int result=check(argv[2]);
    printf("HDR_HWC_CHECK %d\n",result);if(result)return result;
    int (*mode)(const char*)=dlsym(lib,"rk_hdr_mode_cache_check");
    if(!mode)return 5;
    result=mode(argv[2]);printf("MODE_CACHE_ABI_CHECK %d\n",result);if(result)return result;
    int (*target)(const char*)=dlsym(lib,"rk_hdr_target_plan_check");if(!target)return 6;
    result=target(argv[2]);printf("RGB10_TARGET_PLAN_ABI_CHECK %d\n",result);return result;
}
