#include "aidl/android/hardware/common/NativeHandle.h"
#include <android/binder_ibinder.h>
#include <android/binder_parcel.h>
#include <cstdio>
#include <unistd.h>
#define CHECK(x) do{if(!(x)){std::printf("FAIL NativeHandle line %d\n",__LINE__);return 1;}}while(0)
using aidl::android::hardware::common::NativeHandle;
static void *create(void *data){return data;}
static void destroy(void*){}
static binder_status_t transact(AIBinder*,transaction_code_t,const AParcel*,AParcel*){return STATUS_OK;}
int main(){
 auto cls=AIBinder_Class_define("rk.nativehandle.local",create,destroy,transact);CHECK(cls);
 auto binder=AIBinder_new(cls,nullptr);CHECK(binder);AParcel *parcel=nullptr;CHECK(AIBinder_prepareTransaction(binder,&parcel)==STATUS_OK);
 int start=AParcel_getDataPosition(parcel),pipefd[2];CHECK(pipe(pipefd)==0);
 NativeHandle input;input.ints={-1,0,1,0x7fffffff};input.fds.emplace_back(pipefd[0]);CHECK(input.writeToParcel(parcel)==STATUS_OK);
 CHECK(AParcel_setDataPosition(parcel,start)==STATUS_OK);NativeHandle output;CHECK(output.readFromParcel(parcel)==STATUS_OK);
 CHECK(output.ints==input.ints&&output.fds.size()==1);CHECK(write(pipefd[1],"X",1)==1);char c=0;CHECK(read(output.fds[0].get(),&c,1)==1&&c=='X');close(pipefd[1]);
 CHECK(AParcel_setDataPosition(parcel,start)==STATUS_OK);NativeHandle empty;CHECK(empty.writeToParcel(parcel)==STATUS_OK);CHECK(AParcel_setDataPosition(parcel,start)==STATUS_OK);NativeHandle outEmpty;CHECK(outEmpty.readFromParcel(parcel)==STATUS_OK&&outEmpty.fds.empty()&&outEmpty.ints.empty());
 CHECK(AParcel_setDataPosition(parcel,start)==STATUS_OK);CHECK(AParcel_writeInt32(parcel,3)==STATUS_OK);CHECK(AParcel_setDataPosition(parcel,start)==STATUS_OK);NativeHandle malformed;CHECK(malformed.readFromParcel(parcel)==STATUS_BAD_VALUE);
 AParcel_delete(parcel);AIBinder_decStrong(binder);puts("PASS NativeHandle V2 fd/int roundtrip, empty vectors, malformed size rejection");return 0;
}
