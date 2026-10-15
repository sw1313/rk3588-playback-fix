// SPDX-License-Identifier: Apache-2.0
// Android 12 firmware-pinned HIDL bridge: the installed vendor library owns the
// complete IComponentStore implementation. Never substitute guessed vtables.
#include <C2PlatformSupport.h>
#include <android/hidl/base/1.0/IBase.h>
#include <hidl/HidlTransportSupport.h>
#include <binder/ProcessState.h>
#include <utils/RefBase.h>
#include <dlfcn.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <string>
namespace android { void SetUpMinijail(const std::string&,const std::string&); }
int main(int argc,char**argv){
 using namespace android;
 bool check=argc==2&&!strcmp(argv[1],"--check-only");
 signal(SIGPIPE,SIG_IGN);
 if(!check)SetUpMinijail("/vendor/etc/seccomp_policy/android.hardware.media.c2@1.1-seccomp_policy","/vendor/etc/seccomp_policy/android.hardware.media.c2@1.1-extended-seccomp-policy");
 ProcessState::initWithDriver("/dev/vndbinder");ProcessState::self()->startThreadPool();hardware::configureRpcThreadpool(8,true);
 auto components=GetCodec2PlatformComponentStore();
 if(check){auto traits=components->listComponents();for(auto&t:traits)printf("COMPONENT %s rank=%u\n",t->name.c_str(),t->rank);}
 void*impl=dlopen("libcodec2_hidl@1.1.so",RTLD_NOW|RTLD_LOCAL);void*iface=dlopen("android.hardware.media.c2@1.1.so",RTLD_NOW|RTLD_LOCAL);
 if(!impl||!iface){fprintf(stderr,"HIDL LOAD %s\n",dlerror());return 3;}
 using Construct=void(*)(void*,const std::shared_ptr<C2ComponentStore>&);
 using Register=int32_t(*)(void*,const std::string&);
 auto construct=(Construct)dlsym(impl,"_ZN7android8hardware5media2c24V1_15utils14ComponentStoreC1ERKNSt3__110shared_ptrI16C2ComponentStoreEE");
 auto registerService=(Register)dlsym(iface,"_ZN7android8hardware5media2c24V1_115IComponentStore17registerAsServiceERKNSt3__112basic_stringIcNS5_11char_traitsIcEENS5_9allocatorIcEEEE");
 if(!construct||!registerService){fprintf(stderr,"HIDL SYMBOL %s\n",dlerror());return 4;}
 // Exact ARM32 allocation size (0x64) used by this firmware's stock service.
 // Installation must verify both stock service and HIDL library hashes.
 void*raw=::operator new(100);construct(raw,components);
 sp<hidl::base::V1_0::IBase> store(reinterpret_cast<hidl::base::V1_0::IBase*>(raw));
 if(check){size_t count=0;auto result=store->interfaceChain([&](const auto &chain){count=chain.size();for(auto &name:chain)printf("HIDL_INTERFACE %s\n",name.c_str());});if(!result.isOk()||count<3)return 6;fprintf(stdout,"HIDL_CONSTRUCTION_PASS\n");return 0;}
 int32_t rc=registerService(raw,"default");fprintf(stderr,"CODEC2_REGISTRATION=%d\n",rc);if(rc)return 5;
 hardware::joinRpcThreadpool();return 0;
}
