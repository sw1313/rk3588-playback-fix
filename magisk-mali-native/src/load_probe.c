#include <dlfcn.h>
#include <stdio.h>
int main(int argc,char **argv) {
 if(argc!=2)return 2;
 setbuf(stdout,NULL);printf("Loading candidate in independent process\n");
 void *h=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
 if(!h){printf("LOAD_FAIL %s\n",dlerror());return 1;}
 puts("LOAD_PASS");
 printf("EGL initialize=%d Vulkan HAL=%d\n",dlsym(h,"eglInitialize")!=NULL,dlsym(h,"HMI")!=NULL);
 dlclose(h);return 0;
}
