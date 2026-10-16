// SPDX-License-Identifier: MIT
#include <C2ComponentFactory.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string>
#include <memory>
int main(int argc,char **argv) {
 if(argc!=2)return 2;
 const char *libs[]={"av1dec_gav1","vp8dec","vp9dec"};
 const char *names[]={"c2.android.av1.decoder","c2.android.vp8.decoder","c2.android.vp9.decoder"};
 for(int i=0;i<3;++i) {
  std::string path=std::string(argv[1])+"/libcodec2_soft_"+libs[i]+".so";
  void *l=dlopen(path.c_str(),RTLD_NOW|RTLD_LOCAL);
  if(!l){puts(dlerror());return 3;}
  auto create=(C2ComponentFactory*(*)())dlsym(l,"CreateCodec2Factory");
  auto destroy=(void(*)(C2ComponentFactory*))dlsym(l,"DestroyCodec2Factory");
  if(!create||!destroy||!dlsym(l,"__cfi_check"))return 4;
  auto *f=create();if(!f)return 5;
  std::shared_ptr<C2ComponentInterface> intf;
  auto rc=f->createInterface(1,&intf);
  if(rc!=C2_OK||!intf||intf->getName()!=names[i])return 6;
  intf.reset();destroy(f);
  printf("PASS factory %s with CFI entry\n",names[i]);
  // Keep DSOs loaded until process exit; factory code may register global types.
 }
 return 0;
}
