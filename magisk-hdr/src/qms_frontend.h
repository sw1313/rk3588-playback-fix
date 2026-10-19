/* SPDX-License-Identifier: MIT */
/* Included inside hwc_bridge.cpp's private namespace, candidate builds only.
 * Boot token is emitted only by the guarded Decode cold display stage.
 * A set property without a functioning kernel endpoint fails module open.
 */
std::mutex qmsMutex;
rk_qms_client qmsClient{};
bool qmsEnabled=false;

int qmsFd() {
    int fd=open(RKQMS_DEVICE,O_RDWR|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0)return -1;
    struct stat st{};unsigned maj=0,min=0;char extra;
    FILE *identity=fopen("/sys/class/misc/rk_decode_qms/dev","re");
    bool ok=identity && fscanf(identity,"%u:%u %c",&maj,&min,&extra)==2;
    if(identity)fclose(identity);
    if(!ok || fstat(fd,&st) || !S_ISCHR(st.st_mode) || major(st.st_rdev)!=maj || minor(st.st_rdev)!=min){close(fd);return -1;}
    return fd;
}
int qmsStatus(void*,rkqms_status *s) {
    int fd=qmsFd();if(fd<0)return -ENODEV;
    s->abi=RKQMS_ABI;s->bytes=sizeof(*s);
    int ret=ioctl(fd,RKQMS_GET_STATUS,s);int saved=errno;close(fd);
    return ret<0?-saved:ret;
}
int qmsCommand(void*,int restore,rkqms_command *c) {
    int fd=qmsFd();if(fd<0)return -ENODEV;
    // Never retry an interrupted command: the kernel may have accepted it.
    int ret=ioctl(fd,restore?RKQMS_RESTORE:RKQMS_SWITCH,c);int saved=errno;close(fd);
    return ret<0?-saved:ret;
}
rk_qms_transport qmsTransport={nullptr,qmsStatus,qmsCommand};
bool qmsBootRequested() {
    char wanted[PROPERTY_VALUE_MAX]{},boot[80]{};
    property_get("vendor.rkhdr.qms_boot",wanted,"");
    if(!*wanted)return false;
    int fd=open("/proc/sys/kernel/random/boot_id",O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0)return false;
    ssize_t n=read(fd,boot,sizeof(boot)-1);close(fd);if(n<=0)return false;
    boot[strcspn(boot,"\r\n")]=0;
    return !strcmp(wanted,boot);
}
bool qmsStart() {
    rkqms_status s{};
    if(qmsStatus(nullptr,&s) || !rk_qms_status_valid(&s))return false;
    qmsEnabled=true;property_set("vendor.rkhdr.qms","active");return true;
}
int32_t qmsSetConfig(hwc2_device_t *d,hwc2_display_t display,hwc2_config_t id) {
    auto native=reinterpret_cast<HWC2_PFN_SET_ACTIVE_CONFIG>(originalGet(d,HWC2_FUNCTION_SET_ACTIVE_CONFIG));
    if(!qmsEnabled || display!=0)return native(d,display,id);
    std::lock_guard<std::mutex> lock(qmsMutex);
    auto attribute=reinterpret_cast<HWC2_PFN_GET_DISPLAY_ATTRIBUTE>(originalGet(d,HWC2_FUNCTION_GET_DISPLAY_ATTRIBUTE));
    int32_t w=0,h=0,period=0,result=RK_QMS_NATIVE;
    if(!attribute || attribute(d,display,id,HWC2_ATTRIBUTE_WIDTH,&w) ||
        attribute(d,display,id,HWC2_ATTRIBUTE_HEIGHT,&h) ||
        attribute(d,display,id,HWC2_ATTRIBUTE_VSYNC_PERIOD,&period) || w<=0 || h<=0)
        result=rk_qms_restore(&qmsClient,&qmsTransport);
    else result=rk_qms_switch(&qmsClient,&qmsTransport,id,uint32_t(w),uint32_t(h),period);
    if(result==RK_QMS_DONE)return HWC2_ERROR_NONE;
    if(result==RK_QMS_BLOCK)return HWC2_ERROR_NO_RESOURCES;
    return native(d,display,id);
}
int32_t qmsGetConfig(hwc2_device_t *d,hwc2_display_t display,hwc2_config_t *id) {
    auto native=reinterpret_cast<HWC2_PFN_GET_ACTIVE_CONFIG>(originalGet(d,HWC2_FUNCTION_GET_ACTIVE_CONFIG));
    if(!qmsEnabled || display!=0)return native(d,display,id);
    if(!id)return HWC2_ERROR_BAD_PARAMETER;
    std::lock_guard<std::mutex> lock(qmsMutex);
    int result=rk_qms_active(&qmsClient,&qmsTransport,id);
    if(result==RK_QMS_DONE)return HWC2_ERROR_NONE;
    if(result==RK_QMS_BLOCK)return HWC2_ERROR_NO_RESOURCES;
    return native(d,display,id);
}
int32_t qmsPower(hwc2_device_t *d,hwc2_display_t display,int32_t mode) {
    auto native=reinterpret_cast<HWC2_PFN_SET_POWER_MODE>(originalGet(d,HWC2_FUNCTION_SET_POWER_MODE));
    if(!qmsEnabled || display!=0)return native(d,display,mode);
    std::lock_guard<std::mutex> lock(qmsMutex);
    if(rk_qms_restore(&qmsClient,&qmsTransport)==RK_QMS_BLOCK)return HWC2_ERROR_NO_RESOURCES;
    return native(d,display,mode);
}
