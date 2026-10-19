/* SPDX-License-Identifier: MIT */
/* Candidate-only rendezvous with the existing private pre-exec barrier.
 * Included by cold_boot.c after its ownership/digest/process helpers. This is
 * NOT the kernel loader: components/qms/boot-stage64 must independently verify
 * the exact running firmware, native binding and endpoint before acknowledging.
 * Functional payloads remain restricted cold trials. Default builds omit this code.
 */
#ifdef RK_COLD_TEST
#define QMS_MOD BASE "/decode"
#define QMS_STATE BASE "/display-state"
#define QMS_HWC BASE "/display-hwc"
#else
#define QMS_MOD "/data/adb/modules/rk3588_decode_stack"
#define QMS_STATE "/data/adb/rk3588-decode-qms"
#define QMS_HWC "/vendor/lib64/hw/hwcomposer.rk30board.so"
#endif

static int qms_selection(void){
 if(access(QMS_MOD "/config",F_OK)&&errno==ENOENT)return 0;
 if(!private_dir(QMS_MOD)||!owned(QMS_MOD "/config"))return -1;
 FILE *f=fopen(QMS_MOD "/config","re");if(!f)return -1;
 char row[256];int found=0,value=0;
 while(fgets(row,sizeof(row),f)){
  if(!strchr(row,'\n')&&!feof(f)){value=-1;break;}
  if(strncmp(row,"display_qms=",12))continue;
  if(found++){value=-1;break;}
  if(!strcmp(row,"display_qms=1\n")||!strcmp(row,"display_qms=1"))value=1;
  else if(strcmp(row,"display_qms=0\n")&&strcmp(row,"display_qms=0")){value=-1;break;}
 }
 if(ferror(f))value=-1;fclose(f);return value;
}
static int qms_disable_selection(void){
 if(!owned(QMS_MOD "/config"))return 0;
 FILE *f=fopen(QMS_MOD "/config","re");if(!f)return 0;
 char row[256],out[8192];size_t used=0;int ok=1;
 while(fgets(row,sizeof(row),f)){
  if(!strchr(row,'\n')&&!feof(f)){ok=0;break;}
  if(!strncmp(row,"display_qms=",12))continue;
  size_t n=strlen(row);if(n>=sizeof(out)-used-32){ok=0;break;}
  memcpy(out+used,row,n);used+=n;
 }
 if(ferror(f))ok=0;fclose(f);if(!ok)return 0;
 if(used&&out[used-1]!='\n')out[used++]='\n';
 memcpy(out+used,"display_qms=0\n",15);return atomic_text(QMS_MOD "/config",out);
}
struct qms_profile {char id[65],helper[65],module[65],hwc[65],image[65];};
static int qms_read_profile(struct qms_profile *p){
 if(!owned(QMS_MOD "/components/qms/profile"))return 0;
 FILE *f=fopen(QMS_MOD "/components/qms/profile","re");if(!f)return 0;
 char row[256],key[80],value[160],extra;unsigned seen=0;int ok=1;memset(p,0,sizeof(*p));
 while(fgets(row,sizeof(row),f)){
  if(sscanf(row,"%79s %159s %c",key,value,&extra)!=2||!hex64(value)){ok=0;break;}
  unsigned bit=0;char *dest=NULL;
  if(!strcmp(key,"profile_id")){bit=1;dest=p->id;}
  else if(!strcmp(key,"helper_sha256")){bit=2;dest=p->helper;}
  else if(!strcmp(key,"module_sha256")){bit=4;dest=p->module;}
  else if(!strcmp(key,"hwc_sha256")){bit=8;dest=p->hwc;}
  else if(!strcmp(key,"image_sha256")){bit=16;dest=p->image;}
  else {ok=0;break;}
  if(seen&bit){ok=0;break;}seen|=bit;memcpy(dest,value,65);
 }
 if(ferror(f))ok=0;fclose(f);
 return ok&&seen==31&&!strcmp(p->image,"d9b764ad8342ce71ddde45bc0978e548ae636ce6d01fb4bf3f426eba5fff0740");
}
static void qms_defer_unstarted_gpu(void){
 /* No GPU trial has begun: do not let this intentional DISPLAY-only reboot
  * become a false Mali startup failure in assemble.sh on the next boot.
  * Remove only this boot's global pending receipt, with GPU still unbound
  * and every graphics service independently verified as a held init child.
  * Never clear an in-flight cold-KMD pending marker or a previous boot's one.
  */
 char boot[80];struct stat st;pid_t held;
 if(!boot_id(boot)||!lstat(GPU,&st)||errno!=ENOENT||value_is(STATE "/cold-pending",boot)){
  event("DECODE_QMS_GPU_STATE_NOT_SAFE_TO_DEFER");return;
 }
 for(unsigned i=0;i<SERVICE_COUNT;i++)if(!paused_pid(services[i],&held)){
  event("DECODE_QMS_GRAPHICS_NOT_HELD_FOR_DEFER");return;
 }
 if(!lstat(STATE "/boot-pending",&st)){
  if(!S_ISREG(st.st_mode)||st.st_uid||!value_is(STATE "/boot-pending",boot)||unlink(STATE "/boot-pending")){
   event("DECODE_QMS_GLOBAL_PENDING_NOT_CURRENT");return;
  }
  event("DECODE_QMS_UNSTARTED_GPU_TRIAL_DEFERRED");
 }
}
static int qms_reboot_disabled(const char *reason){
 event(reason);
 if(!qms_disable_selection()){event("DECODE_QMS_DISABLE_FAILED_GRAPHICS_REMAIN_HELD");return 76;}
 qms_defer_unstarted_gpu();
 sync();
#ifdef RK_COLD_TEST
 atomic_text(STATE "/reboot-requested","display-disabled\n");return 30;
#else
 reboot(RB_AUTOBOOT);event("DECODE_QMS_REBOOT_FAILED_GRAPHICS_REMAIN_HELD");return 77;
#endif
}
static int qms_before_graphics(const char *boot){
 // Removing/disabling Decode must never retain an old display hook next boot.
 if(access(QMS_MOD,F_OK)||!access(QMS_MOD "/disable",F_OK)||!access(QMS_MOD "/remove",F_OK))return 0;
 int selected=qms_selection();if(!selected)return 0;
 if(selected<0)return qms_reboot_disabled("DECODE_QMS_INVALID_SELECTION");
 if((mkdir(QMS_STATE,0700)&&errno!=EEXIST)||!private_dir(QMS_STATE))return qms_reboot_disabled("DECODE_QMS_BAD_STATE_DIRECTORY");
 char previous[80];
 if(text_read(QMS_STATE "/pending",previous,sizeof(previous))&&strcmp(previous,boot)&&!value_is(QMS_STATE "/qualified",previous)){
  return qms_reboot_disabled("DECODE_QMS_PREVIOUS_UNQUALIFIED_DISPLAY_ROLLBACK");
 }
 struct qms_profile p;char expected[256],reply[512];
 if(!qms_read_profile(&p)||!owned(QMS_MOD "/components/qms/boot-stage64")||
    !owned(QMS_MOD "/components/qms/rk_decode_qms.ko")||
    !matches(QMS_MOD "/components/qms/boot-stage64",p.helper)||
    !matches(QMS_MOD "/components/qms/rk_decode_qms.ko",p.module)||!matches(QMS_HWC,p.hwc))
  return qms_reboot_disabled("DECODE_QMS_PAYLOAD_OR_HWC_MISMATCH");
 snprintf(expected,sizeof(expected),"%s %s prepared",boot,p.id);
 if(!owned(QMS_MOD "/active-boot-id")||!value_is(QMS_MOD "/active-boot-id",boot)||
    !owned(QMS_STATE "/plan")||!value_is(QMS_STATE "/plan",expected))
  return qms_reboot_disabled("DECODE_QMS_CURRENT_OVERLAY_PLAN_MISSING");
 pid_t pids[SERVICE_COUNT],current;
 for(unsigned i=0;i<SERVICE_COUNT;i++)if(!paused_pid(services[i],&pids[i]))return qms_reboot_disabled("DECODE_QMS_PREEXEC_GATE_MISSING");
 if(!atomic_text(QMS_STATE "/pending",boot))return qms_reboot_disabled("DECODE_QMS_PENDING_WRITE_FAILED");
 const char *args[]={QMS_MOD "/components/qms/boot-stage64","bind",boot,p.id,NULL};
 event("DECODE_QMS_PREEXEC_PREPARE_STARTED");
 if(!command(args,reply,sizeof(reply),15000))return qms_reboot_disabled("DECODE_QMS_BIND_FAILED");
 char exact_reply[258];snprintf(exact_reply,sizeof(exact_reply),"%s\n",expected);
 if(strcmp(reply,exact_reply)||!owned(QMS_STATE "/receipt")||!value_is(QMS_STATE "/receipt",expected))
  return qms_reboot_disabled("DECODE_QMS_BIND_RECEIPT_MISMATCH");
 for(unsigned i=0;i<SERVICE_COUNT;i++)if(!paused_pid(services[i],&current)||current!=pids[i])return qms_reboot_disabled("DECODE_QMS_GATE_CHANGED_DURING_BIND");
 if(prop_set("vendor.rkhdr.qms_boot",boot))return qms_reboot_disabled("DECODE_QMS_BOOT_TOKEN_FAILED");
 event("DECODE_QMS_PREPARED_BEFORE_GRAPHICS_RELEASE");return 0;
}
