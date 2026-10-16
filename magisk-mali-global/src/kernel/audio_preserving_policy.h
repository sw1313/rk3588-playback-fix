// SPDX-License-Identifier: MIT
#ifndef RK_AUDIO_PRESERVING_POLICY_H
#define RK_AUDIO_PRESERVING_POLICY_H
static int allowed(const char *key,const char *service){
 if(strcmp(key,"ctl.start") && strcmp(key,"ctl.stop"))return 0;
 return !strcmp(service,"surfaceflinger") || !strcmp(service,"zygote_secondary") || !strcmp(service,"zygote") || !strcmp(service,"netd") || !strcmp(service,"vendor.hwcomposer-2-1");
}
static int reboot_allowed(int transitioned,int disarmed,int already,int boot_matches){
 return transitioned && !disarmed && !already && boot_matches;
}
#endif
