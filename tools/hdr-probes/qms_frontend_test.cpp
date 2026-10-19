// SPDX-License-Identifier: MIT
// Actual public HWC callback bodies, fully injected transport/native callbacks.
// Does not open a device, load a HAL, modeset, draw, vote FPS or send input.
#define RKQMS_HWC_CANDIDATE 1
#include "../../magisk-hdr/src/hwc_bridge.cpp"
namespace {
rkqms_status fake{};
int sets=0,gets=0,powers=0,commands=0,queries=0,failQuery=0,failCommand=0;
bool beforeNative=false;
int fakeStatus(void*,rkqms_status *out){++queries;if(failQuery)return -EIO;*out=fake;return 0;}
int fakeCommand(void*,int restore,rkqms_command *c){
    ++commands;if(failCommand)return -EIO;
    ++fake.serial;fake.completed_serial=fake.serial;fake.observed_ns+=100000000;
    fake.state=restore?RKQMS_BASE:RKQMS_ACTIVE;fake.effective_tfr=restore?fake.base_tfr:c->target_tfr;
    c->result=fake;return 0;
}
int32_t nativeSet(hwc2_device_t*,hwc2_display_t,hwc2_config_t id){++sets;beforeNative=rk_qms_clean(&fake)||rk_qms_native_safe(&fake);return id==99?HWC2_ERROR_BAD_CONFIG:HWC2_ERROR_NONE;}
int32_t nativeGet(hwc2_device_t*,hwc2_display_t,hwc2_config_t *id){++gets;*id=42;return HWC2_ERROR_NONE;}
int32_t nativePower(hwc2_device_t*,hwc2_display_t,int32_t){++powers;beforeNative=rk_qms_clean(&fake)||rk_qms_native_safe(&fake);return HWC2_ERROR_NONE;}
int32_t nativeAttribute(hwc2_device_t*,hwc2_display_t,hwc2_config_t id,int32_t a,int32_t *v){
    if(id==99)return HWC2_ERROR_BAD_CONFIG;
    if(a==HWC2_ATTRIBUTE_WIDTH)*v=id==2?1280:1920;
    else if(a==HWC2_ATTRIBUTE_HEIGHT)*v=id==2?720:1080;
    else if(a==HWC2_ATTRIBUTE_VSYNC_PERIOD)*v=id==2?16666666:41666666;
    else return HWC2_ERROR_BAD_PARAMETER;
    return HWC2_ERROR_NONE;
}
hwc2_function_pointer_t fakeGet(hwc2_device_t*,int32_t descriptor){
    switch(descriptor){
    case HWC2_FUNCTION_SET_ACTIVE_CONFIG:return (hwc2_function_pointer_t)nativeSet;
    case HWC2_FUNCTION_GET_ACTIVE_CONFIG:return (hwc2_function_pointer_t)nativeGet;
    case HWC2_FUNCTION_SET_POWER_MODE:return (hwc2_function_pointer_t)nativePower;
    case HWC2_FUNCTION_GET_DISPLAY_ATTRIBUTE:return (hwc2_function_pointer_t)nativeAttribute;
    default:return nullptr;
    }
}
void reset(){
    fake={};fake.abi=RKQMS_ABI;fake.bytes=sizeof(fake);fake.generation=99;fake.link_cookie=123;
    fake.connector=11;fake.crtc=12;fake.proof=RKQMS_PROOF_ALL;fake.state=RKQMS_BASE;
    fake.width=1920;fake.height=1080;fake.tfr_mask=0x7fe;fake.base_tfr=fake.effective_tfr=10;
    fake.bits_per_component=10;fake.eotf=2;
    sets=gets=powers=commands=queries=failQuery=failCommand=0;beforeNative=false;
    qmsClient={};qmsEnabled=true;originalGet=fakeGet;qmsTransport={nullptr,fakeStatus,fakeCommand};
}
}
#define CHECK(x) do{if(!(x)){printf("FAIL QMS HWC callback line %d\n",__LINE__);return 1;}++checks;}while(0)
int main(){
    unsigned int checks=0;hwc2_config_t id=0;
    reset();qmsEnabled=false;
    CHECK(qmsSetConfig(nullptr,0,1)==HWC2_ERROR_NONE && sets==1 && !commands && !queries);
    reset();CHECK(getFunction(nullptr,HWC2_FUNCTION_SET_ACTIVE_CONFIG)==(hwc2_function_pointer_t)qmsSetConfig);
    CHECK(getFunction(nullptr,HWC2_FUNCTION_GET_ACTIVE_CONFIG)==(hwc2_function_pointer_t)qmsGetConfig);
    CHECK(getFunction(nullptr,HWC2_FUNCTION_SET_POWER_MODE)==(hwc2_function_pointer_t)qmsPower);
    CHECK(qmsSetConfig(nullptr,0,1)==HWC2_ERROR_NONE && !sets && commands==1);
    CHECK(qmsGetConfig(nullptr,0,&id)==HWC2_ERROR_NONE && id==1 && !gets);
    CHECK(fake.bits_per_component==10 && fake.eotf==2);
    CHECK(qmsSetConfig(nullptr,1,2)==HWC2_ERROR_NONE && sets==1 && commands==1);
    CHECK(qmsSetConfig(nullptr,0,2)==HWC2_ERROR_NONE && sets==2 && commands==2 && beforeNative);
    reset();CHECK(qmsSetConfig(nullptr,0,1)==HWC2_ERROR_NONE);
    failCommand=1;
    CHECK(qmsSetConfig(nullptr,0,2)==HWC2_ERROR_NO_RESOURCES && !sets);
    CHECK(qmsPower(nullptr,0,0)==HWC2_ERROR_NO_RESOURCES && !powers);
    failQuery=1;
    CHECK(qmsGetConfig(nullptr,0,&id)==HWC2_ERROR_NO_RESOURCES && !gets);
    CHECK(qmsSetConfig(nullptr,0,1)==HWC2_ERROR_NO_RESOURCES && !sets);
    reset();CHECK(qmsSetConfig(nullptr,0,1)==HWC2_ERROR_NONE);
    CHECK(qmsPower(nullptr,0,0)==HWC2_ERROR_NONE && powers==1 && beforeNative && !qmsClient.logical_valid);
    reset();CHECK(qmsSetConfig(nullptr,0,99)==HWC2_ERROR_BAD_CONFIG && sets==1 && !commands);
    reset();CHECK(qmsSetConfig(nullptr,0,1)==HWC2_ERROR_NONE);
    ++fake.generation;CHECK(qmsGetConfig(nullptr,0,&id)==HWC2_ERROR_NO_RESOURCES && !gets);
    CHECK(qmsGetConfig(nullptr,0,nullptr)==HWC2_ERROR_BAD_PARAMETER);
    reset();fake.state=RKQMS_QUIESCENT;fake.proof=0;fake.link_cookie=0;fake.effective_tfr=0;
    CHECK(qmsSetConfig(nullptr,0,1)==HWC2_ERROR_NONE && sets==1 && !commands);
    CHECK(qmsGetConfig(nullptr,0,&id)==HWC2_ERROR_NONE && id==42 && gets==1);
    printf("PASS %u actual HWC callback assertions; injected transport only; no display IO\n",checks);
    return 0;
}
