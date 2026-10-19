// SPDX-License-Identifier: MIT
// Exact-firmware cold binding. No driver unbind, client stop, or runtime swap.
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/reboot.h>
#include <sys/syscall.h>
#include <sys/system_properties.h>
#include <sys/sysmacros.h>
#include <linux/fs.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#ifdef RK_COLD_TEST
#define BASE "/data/local/tmp/rk-cold-fixture"
#define STATE BASE "/state"
#define MOD BASE "/module"
#define GPU BASE "/gpu-driver"
#define BOOT_DEVICE BASE "/boot-device"
#define BOOT_ID BASE "/boot-id"
#define FW_FILE BASE "/firmware"
#define GLES32 BASE "/gles32"
#define GLES64 BASE "/gles64"
#define ZN_MOD BASE "/zygisk-next"
#define LSP_MOD BASE "/lsposed"
#else
#define STATE "/data/adb/rk3588-mali-global"
#define MOD "/data/adb/modules/rk3588_mali_global"
#define GPU "/sys/bus/platform/devices/fb000000.gpu/driver"
#define BOOT_DEVICE "/dev/block/mmcblk0p7"
#define BOOT_ID "/proc/sys/kernel/random/boot_id"
#define FW_FILE "/vendor/etc/firmware/rk_next_csffw.bin"
#define GLES32 "/vendor/lib/egl/libGLES_mali.so"
#define GLES64 "/vendor/lib64/egl/libGLES_mali.so"
#define ZN_MOD "/data/adb/modules/zygisksu"
#define LSP_MOD "/data/adb/modules/zygisk_lsposed"
#endif
#define STORE STATE "/boot"
#define RUN STATE "/kernel-run"
struct profile {char id[65],original[65],kmd[65],csf[65],gles32[65],gles64[65];uint64_t bytes;};
static struct profile cfg;
static const char *const services[]={"vendor.gralloc-4-0","vendor.hwcomposer-2-1","surfaceflinger"};
#define SERVICE_COUNT (sizeof(services)/sizeof(services[0]))
static uint64_t now_ms(void){struct timespec t;clock_gettime(CLOCK_BOOTTIME,&t);return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
static int text_read(const char *path,char *out,size_t size){
 int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return 0;
 ssize_t n=read(fd,out,size-1);close(fd);if(n<=0)return 0;out[n]=0;out[strcspn(out,"\r\n")]=0;return 1;
}
static int owned(const char *path){struct stat s;return !lstat(path,&s)&&S_ISREG(s.st_mode)&&s.st_uid==0&&!(s.st_mode&022);}
static int private_dir(const char *path){struct stat s;return !lstat(path,&s)&&S_ISDIR(s.st_mode)&&s.st_uid==0&&!(s.st_mode&022);}
static int atomic_text(const char *path,const char *value){
 char tmp[PATH_MAX];if(snprintf(tmp,sizeof(tmp),"%s.tmp.%d",path,getpid())>=(int)sizeof(tmp))return 0;
 int fd=open(tmp,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)return 0;
 size_t n=strlen(value);int ok=write(fd,value,n)==(ssize_t)n&&!fsync(fd);close(fd);
 if(ok)ok=!rename(tmp,path);if(!ok)unlink(tmp);
 char parent[PATH_MAX];snprintf(parent,sizeof(parent),"%s",path);char *slash=strrchr(parent,'/');if(!slash)return 0;*slash=0;
 int dir=open(parent,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);if(dir>=0){if(fsync(dir))ok=0;close(dir);}else ok=0;return ok;
}
static void event(const char *value){
 int fd=open(STATE "/cold-journal",O_WRONLY|O_CREAT|O_APPEND|O_CLOEXEC|O_NOFOLLOW,0600);
 if(fd>=0){char b[256];int n=snprintf(b,sizeof(b),"%llu %s\n",(unsigned long long)now_ms(),value);if(n>0&&n<(int)sizeof(b)){ssize_t ignored=write(fd,b,n);(void)ignored;fsync(fd);}close(fd);}
}
static int hex64(const char *s){if(strlen(s)!=64)return 0;for(unsigned i=0;i<64;i++)if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return 0;return 1;}
static int profile_read(const char *path){
 if(!owned(path))return 0;FILE *f=fopen(path,"re");if(!f)return 0;
 char row[256],key[80],value[160],extra;unsigned seen=0;int ok=1;memset(&cfg,0,sizeof(cfg));
 while(fgets(row,sizeof(row),f)){
  if(sscanf(row,"%79s %159s %c",key,value,&extra)!=2){ok=0;break;}unsigned bit=0;char *dest=NULL;
  if(!strcmp(key,"profile_id")){bit=1;dest=cfg.id;}
  else if(!strcmp(key,"original_sha256")){bit=2;dest=cfg.original;}
  else if(!strcmp(key,"kmd_sha256")){bit=4;dest=cfg.kmd;}
  else if(!strcmp(key,"csf_sha256")){bit=8;dest=cfg.csf;}
  else if(!strcmp(key,"gles32_sha256")){bit=16;dest=cfg.gles32;}
  else if(!strcmp(key,"gles64_sha256")){bit=32;dest=cfg.gles64;}
  else if(!strcmp(key,"original_bytes")){char *end;bit=64;cfg.bytes=strtoull(value,&end,10);if(*end||!cfg.bytes)ok=0;}
  else {ok=0;break;}
  if(seen&bit){ok=0;break;}seen|=bit;if(dest){if(!hex64(value)){ok=0;break;}memcpy(dest,value,65);}
 }fclose(f);
#ifndef RK_COLD_TEST
 if(strcmp(cfg.original,"eabcbc53b3e56846036c8cb23fd77d246ad919aae427757e17364cb777186264")||cfg.bytes!=50331648)ok=0;
#endif
 return ok&&seen==127;
}
static int command(const char *const args[],char *out,size_t cap,unsigned timeout){
 int p[2];if(pipe2(p,O_CLOEXEC))return 0;pid_t pid=fork();if(pid<0){close(p[0]);close(p[1]);return 0;}
 if(!pid){setpgid(0,0);dup2(p[1],1);close(p[0]);close(p[1]);execv(args[0],(char *const*)args);_exit(127);}
 setpgid(pid,pid);close(p[1]);fcntl(p[0],F_SETFL,O_NONBLOCK);size_t used=0;int done=0,status=0,eof=0,overflow=0;uint64_t until=now_ms()+timeout;
 while(now_ms()<until){char buf[4096];ssize_t n;while((n=read(p[0],buf,sizeof(buf)))>0){size_t take=(size_t)n;if(take>cap-used-1){take=cap-used-1;overflow=1;}memcpy(out+used,buf,take);used+=take;}
  if(n==0)eof=1;if(!done&&waitpid(pid,&status,WNOHANG)==pid)done=1;if(done&&eof)break;struct pollfd f={p[0],POLLIN,0};poll(&f,1,20);
 }if(!done){
  kill(-pid,SIGKILL);
  /* A child blocked in a native kernel operation may not reap immediately.
   * Do not turn a bounded display-loader timeout into an unbounded wait and
   * prevent the caller from recording failure/requesting reboot recovery. */
  uint64_t reap_until=now_ms()+500;
  while(waitpid(pid,&status,WNOHANG)==0&&now_ms()<reap_until)usleep(1000);
 }close(p[0]);out[used]=0;return done&&eof&&!overflow&&WIFEXITED(status)&&!WEXITSTATUS(status);
}
static int digest(const char *path,char hash[65]){char b[1024];const char *a[]={"/system/bin/sha256sum",path,NULL};if(!command(a,b,sizeof(b),15000)||strlen(b)<65||b[64]!=' ')return 0;memcpy(hash,b,64);hash[64]=0;return hex64(hash);}
static int matches(const char *path,const char *expected){char h[65];return digest(path,h)&&!strcmp(h,expected);}
static int image_hash(char h[65]){return owned(STORE "/image.sha256")&&text_read(STORE "/image.sha256",h,65)&&hex64(h);}
static int verify_original(void){
 struct stat s;return owned(STORE "/original.img")&&!stat(STORE "/original.img",&s)&&(uint64_t)s.st_size==cfg.bytes&&matches(STORE "/original.img",cfg.original);
}
static int verify_files(void){
 char h[65];struct stat s;
 return verify_original()&&
  image_hash(h)&&owned(STORE "/candidate.img")&&!stat(STORE "/candidate.img",&s)&&(uint64_t)s.st_size==cfg.bytes&&matches(STORE "/candidate.img",h);
}
static int verify_payload(void){
 char cache[PATH_MAX];snprintf(cache,sizeof(cache),STATE "/cache/%s.bin",cfg.csf);
 return owned(MOD "/kernel/rk_mali_next.ko")&&matches(MOD "/kernel/rk_mali_next.ko",cfg.kmd)&&
  matches(MOD "/payload/lib/egl/libGLES_mali.so",cfg.gles32)&&matches(MOD "/payload/lib64/egl/libGLES_mali.so",cfg.gles64)&&matches(cache,cfg.csf);
}
static int target_open(void){
 int fd=open(BOOT_DEVICE,O_RDWR|O_CLOEXEC|O_NOFOLLOW|O_SYNC);if(fd<0)return -1;struct stat s;uint64_t bytes=0;
#ifdef RK_COLD_TEST
 if(fstat(fd,&s)||!S_ISREG(s.st_mode))goto bad;bytes=(uint64_t)s.st_size;
#else
 if(fstat(fd,&s)||!S_ISBLK(s.st_mode)||major(s.st_rdev)!=179||minor(s.st_rdev)!=7||ioctl(fd,BLKGETSIZE64,&bytes))goto bad;
#endif
 if(bytes!=cfg.bytes)goto bad;return fd;
bad:close(fd);return -1;
}
static int copy_image(const char *path,int fd,int candidate){
#ifndef RK_COLD_TEST
 (void)candidate;
#endif
 int input=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(input<0)return 0;
 char *buf=malloc(262144);int ok=buf!=NULL;uint64_t offset=0;
 while(ok&&offset<cfg.bytes){size_t n=cfg.bytes-offset>262144?262144:(size_t)(cfg.bytes-offset);ssize_t got=pread(input,buf,n,offset);if(got!=(ssize_t)n){ok=0;break;}
#ifdef RK_COLD_TEST
  if(candidate&&!access(STATE "/fail-write",F_OK)){ssize_t ignored=pwrite(fd,buf,16,0);(void)ignored;ok=0;break;}
#endif
  size_t wrote=0;while(wrote<n){ssize_t rc=pwrite(fd,buf+wrote,n-wrote,offset+wrote);if(rc<=0){ok=0;break;}wrote+=(size_t)rc;}offset+=n;
 }
 free(buf);close(input);if(fsync(fd))ok=0;sync();return ok;
}
static int replace_boot_locked(int candidate){
 char next[65],current[65];if(!(candidate?(verify_files()&&verify_payload()):verify_original())||!image_hash(next))return 0;
 int fd=target_open();if(fd<0)return 0;
 if(!digest(BOOT_DEVICE,current)|| (strcmp(current,cfg.original)&&strcmp(current,next))){close(fd);event("REFUSED_FOREIGN_BOOT");return 0;}
 const char *expected=candidate?next:cfg.original;if(!strcmp(current,expected)){close(fd);return 1;}
 int ok=copy_image(candidate?STORE "/candidate.img":STORE "/original.img",fd,candidate)&&matches(BOOT_DEVICE,expected);
 if(!ok&&candidate){
  // This process owns the operation lock and has just written this known disk.
  // Recover our own partial write; never broaden the foreign-boot acceptance.
  int recovered=verify_original()&&copy_image(STORE "/original.img",fd,0)&&matches(BOOT_DEVICE,cfg.original);
  event(recovered?"BOOT_WRITE_FAILED_ORIGINAL_RECOVERED":"BOOT_WRITE_FAILED_MANUAL_LOADER_RECOVERY_REQUIRED");
 }
 close(fd);
 event(ok?(candidate?"BOOT_CANDIDATE_WRITTEN_VERIFIED":"BOOT_ORIGINAL_RESTORED_VERIFIED"):"BOOT_WRITE_OR_VERIFY_FAILED");return ok;
}
static int replace_boot(int candidate){
 int lock=open(STORE "/operation.lock",O_RDWR|O_CREAT|O_CLOEXEC|O_NOFOLLOW,0600);if(lock<0)return 0;
 uint64_t until=now_ms()+15000;
 while(flock(lock,LOCK_EX|LOCK_NB)){
  if(errno!=EWOULDBLOCK||now_ms()>=until){close(lock);return 0;}usleep(100000);
 }
 int ok=replace_boot_locked(candidate);close(lock);return ok;
}
static int reboot_original(const char *reason){
 event(reason);if(!replace_boot(0)){event("RECOVERY_IMAGE_RESTORE_FAILED_MANUAL_RECOVERY_REQUIRED");return 31;}
 if(!atomic_text(STATE "/kernel-mode","stock\n")||!atomic_text(STATE "/cold-mode","off\n"))return 32;sync();
#ifdef RK_COLD_TEST
 atomic_text(STATE "/reboot-requested","original\n");return 30;
#else
 reboot(RB_AUTOBOOT);event("REBOOT_SYSCALL_FAILED_POWER_RECOVERY_REQUIRED");return 33;
#endif
}
static int value_is(const char *path,const char *wanted){char b[128];return text_read(path,b,sizeof(b))&&!strcmp(b,wanted);}
static int boot_id(char out[80]){return text_read(BOOT_ID,out,80)&&strlen(out)>4;}
static int selected(void){return !access(MOD,F_OK)&&access(MOD "/disable",F_OK)&&access(MOD "/remove",F_OK)&&value_is(STATE "/mode","on")&&value_is(STATE "/kernel-mode","next")&&value_is(STATE "/cold-mode","on");}
static int prop_get(const char *key,char *out){
#ifdef RK_COLD_TEST
 char path[PATH_MAX];snprintf(path,sizeof(path),BASE "/properties/%s",key);out[0]=0;return text_read(path,out,PROP_VALUE_MAX);
#else
 return __system_property_get(key,out);
#endif
}
static int prop_set(const char *key,const char *value){
#ifdef RK_COLD_TEST
 (void)value;char line[160];snprintf(line,sizeof(line),"CONTROL %s",key);event(line);return 0;
#else
 return __system_property_set(key,value);
#endif
}
static int gpu_is_next(void){char link[256];ssize_t n=readlink(GPU,link,sizeof(link)-1);if(n<=0)return 0;link[n]=0;return !strcmp(link,"../../../bus/platform/drivers/rk_next_mali");}
static const char *kmd_args(void){
 // Root-owned selection is latched before allocator/HWC/SurfaceFlinger run.
 const int safe=owned(STATE "/fence-mode")&&value_is(STATE "/fence-mode","on");
 return safe?"oneboot_trial=1 csf_fence_safe=1":"oneboot_trial=1 csf_fence_safe=0";
}
static int load_kmd(void){
#ifdef RK_COLD_TEST
 event(kmd_args());if(value_is(STATE "/fail-load","yes"))return 0;return !symlink("../../../bus/platform/drivers/rk_next_mali",GPU);
#else
 int fd=open(MOD "/kernel/rk_mali_next.ko",O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return 0;pid_t p=fork();if(p<0){close(fd);return 0;}
 if(!p){
  int rc=syscall(__NR_finit_module,fd,kmd_args(),0);_exit(rc?1:0);
 }close(fd);int status=0;uint64_t end=now_ms()+15000;
 do{if(waitpid(p,&status,WNOHANG)==p)return WIFEXITED(status)&&!WEXITSTATUS(status);usleep(20000);}while(now_ms()<end);kill(p,SIGKILL);return 0;
#endif
}
static int paused_pid(const char *service,pid_t *out){
 char key[128],val[PROP_VALUE_MAX],path[PATH_MAX],buf[4096];snprintf(key,sizeof(key),"init.svc_debug_pid.%s",service);prop_get(key,val);
 char *end;long p=strtol(val,&end,10);if(*end||p<=1||p>INT_MAX)return 0;
#ifdef RK_COLD_TEST
 snprintf(path,sizeof(path),BASE "/process-%ld",p);
#else
 snprintf(path,sizeof(path),"/proc/%ld/status",p);
#endif
 int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return 0;ssize_t n=read(fd,buf,sizeof(buf)-1);close(fd);if(n<=0)return 0;buf[n]=0;
 char *state=strstr(buf,"State:"),*parent=strstr(buf,"PPid:");if(!state||!parent)return 0;state+=6;while(*state==' '||*state=='\t')state++;
 if((*state!='T'&&*state!='t')||strtol(parent+5,NULL,10)!=1)return 0;
#ifdef RK_COLD_TEST
 snprintf(path,sizeof(path),BASE "/process-exe-%ld",p);if(!value_is(path,"init"))return 0;
#else
 // The debug PID property is only a locator. Require an unexecuted init child,
 // rather than trusting that property or a stopped graphics process alone.
 struct stat parent_exe,child_exe;snprintf(path,sizeof(path),"/proc/%ld/exe",p);
 if(stat("/proc/1/exe",&parent_exe)||stat(path,&child_exe)||parent_exe.st_dev!=child_exe.st_dev||parent_exe.st_ino!=child_exe.st_ino)return 0;
#endif
 *out=(pid_t)p;return 1;
}
static int continue_service(const char *service,pid_t pid){
 pid_t current;if(!paused_pid(service,&current)||current!=pid)return 0;
#ifdef RK_COLD_TEST
 char line[160];snprintf(line,sizeof(line),"CONTINUE %s pid=%d",service,pid);event(line);return 1;
#else
 int fd=syscall(__NR_pidfd_open,pid,0);if(fd<0)return 0;
 int ok=paused_pid(service,&current)&&current==pid&&!syscall(__NR_pidfd_send_signal,fd,SIGCONT,NULL,0);close(fd);return ok;
#endif
}
static int receipt_matches(const char *boot){char b[256],expected[256];snprintf(expected,sizeof(expected),"%s %s released",boot,cfg.id);return text_read(STATE "/cold-receipt",b,sizeof(b))&&!strcmp(b,expected);}
static int run_preflight(void){
 char boot[80],previous[80];if(!boot_id(boot))return 20;event("COLD_PREFLIGHT_BEFORE_MAGISK_MODULES");
 // Resolve known rollbacks before starting a third-party injector. Intentional
 // early reboots must not look like an unfinished Zygisk injection to it.
 if(text_read(STATE "/cold-pending",previous,sizeof(previous))&&strcmp(previous,boot)&&!value_is(STATE "/cold-qualified",previous))return reboot_original("PREVIOUS_COLD_BOOT_UNQUALIFIED");
 if(!selected())return reboot_original("COLD_DISABLED_OR_MODULE_REMOVED");
 if(!verify_files()||!verify_payload())return reboot_original("COLD_PREFLIGHT_PAYLOAD_MISMATCH");
 int zn=!access(ZN_MOD,F_OK)&&access(ZN_MOD "/disable",F_OK)&&access(ZN_MOD "/remove",F_OK);
 int lsp=zn&&!access(LSP_MOD,F_OK)&&access(LSP_MOD "/disable",F_OK)&&access(LSP_MOD "/remove",F_OK);
 if(!atomic_text(STATE "/zygisk-required-boot",zn?boot:"off")||!atomic_text(STATE "/lsposed-required-boot",lsp?boot:"off"))return reboot_original("COLD_INJECTION_INTENT_WRITE_FAILED");
 return 0;
}
static int run_barrier(void){
 char boot[80];if(!boot_id(boot))return 20;uint64_t end=now_ms()+90000;
 // init's exec blocks its action queue, not control-message handling. The
 // separate stage worker can still clear graphics sigstop and release them.
 do{
  if(gpu_is_next()&&receipt_matches(boot)){event("INIT_GRAPHICS_BARRIER_PASSED_ZYGOTE_UNMODIFIED");return 0;}
#ifdef RK_COLD_TEST
  break;
#else
  usleep(50000);
#endif
 }while(now_ms()<end);
 return reboot_original("COLD_INIT_BARRIER_TIMEOUT");
}
#ifdef RKQMS_DISPLAY_STAGE
#include "../../magisk-decode/qms-cold-stage.h"
#endif
static int run_stage(void){
 int stage_lock=open(STATE "/cold-stage.lock",O_RDWR|O_CREAT|O_CLOEXEC|O_NOFOLLOW,0600);
 if(stage_lock<0||flock(stage_lock,LOCK_EX|LOCK_NB))return 69;
 char boot[80],previous[80];if(!boot_id(boot))return 20;event("COLD_LOADER_STARTED");
 if(text_read(STATE "/cold-pending",previous,sizeof(previous))&&strcmp(previous,boot)&&!value_is(STATE "/cold-qualified",previous))return reboot_original("PREVIOUS_COLD_BOOT_UNQUALIFIED");
 uint64_t deadline=now_ms()+30000;int ready=0;
 do{
  // Magisk may still be promoting modules_update. Only a complete current-boot
  // plan is readiness; a missing module becomes rollback at the deadline.
  if(!access(MOD "/disable",F_OK)||!access(MOD "/remove",F_OK))return reboot_original("COLD_DISABLED_OR_MODULE_REMOVED");
  if(selected()&&value_is(MOD "/active-mode","on")&&value_is(STATE "/cold-plan",boot)&&matches(FW_FILE,cfg.csf)&&matches(GLES32,cfg.gles32)&&matches(GLES64,cfg.gles64)){ready=1;break;}
#ifdef RK_COLD_TEST
  break;
#else
  usleep(100000);
#endif
 }while(now_ms()<deadline);
 if(!ready)return reboot_original("COLD_OVERLAYS_NOT_READY");
 pid_t pids[SERVICE_COUNT];int held=0;deadline=now_ms()+15000;
 do{
  held=1;for(unsigned i=0;i<SERVICE_COUNT;i++)if(!paused_pid(services[i],&pids[i]))held=0;
  if(held)break;
#ifdef RK_COLD_TEST
  break;
#else
  usleep(100000);
#endif
 }while(now_ms()<deadline);
 if(!held)return reboot_original("COLD_GRAPHICS_GATE_NOT_HELD");
 char pid_receipt[512];size_t used=0;
 for(unsigned i=0;i<SERVICE_COUNT;i++){
  char line[160];snprintf(line,sizeof(line),"GATE_HELD service=%s pid=%d preexec=1",services[i],pids[i]);event(line);
  int n=snprintf(pid_receipt+used,sizeof(pid_receipt)-used,"%s %d\n",services[i],pids[i]);
  if(n<0||(size_t)n>=sizeof(pid_receipt)-used)return reboot_original("COLD_PID_RECEIPT_OVERFLOW");used+=(size_t)n;
 }
 if(!atomic_text(STATE "/cold-pids",pid_receipt))return reboot_original("COLD_PID_RECEIPT_FAILED");
#ifdef RKQMS_DISPLAY_STAGE
 // Decode owns display selection/payload/rollback. Candidate builds alone
 // enable this rendezvous; no release of graphics after a failed display bind.
 int display_stage=qms_before_graphics(boot);if(display_stage)return display_stage;
#endif
 struct stat s;if(!lstat(GPU,&s))return reboot_original("REFUSED_ALREADY_BOUND_GPU");if(errno!=ENOENT)return reboot_original("GPU_OWNERSHIP_UNKNOWN");
 if(!owned(MOD "/kernel/rk_mali_next.ko")||!matches(MOD "/kernel/rk_mali_next.ko",cfg.kmd))return reboot_original("COLD_KMD_HASH_MISMATCH");
 if(!atomic_text(STATE "/cold-pending",boot))return reboot_original("COLD_LATCH_WRITE_FAILED");event("GPU_UNBOUND_GRAPHICS_PAUSED_LOADING_KMD");
 if(!load_kmd()||!gpu_is_next())return reboot_original("COLD_FIRST_BIND_FAILED");
 // This process is exec_background: init remains free to acknowledge controls.
 for(unsigned i=0;i<SERVICE_COUNT;i++)if(prop_set("ctl.sigstop_off",services[i]))return reboot_original("COLD_GATE_CLEAR_FAILED");
 for(unsigned i=0;i<SERVICE_COUNT;i++)if(!continue_service(services[i],pids[i]))return reboot_original("COLD_GRAPHICS_RELEASE_FAILED");
 if((mkdir(RUN,0700)&&errno!=EEXIST)||!private_dir(RUN))return reboot_original("COLD_RECEIPT_DIRECTORY_FAILED");
 unlink(RUN "/result");
 uint64_t stamp=now_ms();int ack=open(RUN "/health.restored",O_WRONLY|O_TRUNC|O_CREAT|O_CLOEXEC|O_NOFOLLOW,0600);if(ack<0)return reboot_original("COLD_ACK_FAILED");
 int ackok=write(ack,&stamp,sizeof(stamp))==(ssize_t)sizeof(stamp)&&!fsync(ack);close(ack);if(!ackok)return reboot_original("COLD_ACK_FAILED");
 char receipt[256];snprintf(receipt,sizeof(receipt),"%s %s released",boot,cfg.id);if(!atomic_text(STATE "/cold-receipt",receipt))return reboot_original("COLD_RECEIPT_FAILED");
 event("COLD_FIRST_BIND_GRAPHICS_RELEASED_NO_CLIENT_RESTART");
#ifdef RK_COLD_TEST
 return 0;
#else
 deadline=now_ms()+180000;do{if(value_is(STATE "/cold-qualified",boot)&&!access(STATE "/cold-qualified",R_OK)){event("COLD_BOOT_QUALIFIED");return 0;}usleep(250000);}while(now_ms()<deadline);
 return reboot_original("COLD_BOOT_QUALIFICATION_TIMEOUT");
#endif
}
#ifdef RK_COLD_TEST
static int pidfd_test(void){
 pid_t p=fork();if(p<0)return 71;if(!p){raise(SIGSTOP);_exit(42);}
 int status=0;if(waitpid(p,&status,WUNTRACED)!=p||!WIFSTOPPED(status))return 72;
 int fd=syscall(__NR_pidfd_open,p,0);int ok=fd>=0&&!syscall(__NR_pidfd_send_signal,fd,SIGCONT,NULL,0);
 if(fd>=0)close(fd);if(!ok)kill(p,SIGKILL);waitpid(p,&status,0);
 return ok&&WIFEXITED(status)&&WEXITSTATUS(status)==42?0:73;
}
#endif
int main(int argc,char **argv){
#ifdef RK_COLD_TEST
 if(argc==2&&!strcmp(argv[1],"pidfd-test"))return pidfd_test();
#endif
 if(getuid()!=0||argc<2||argc>3)return 64;const char *profile=argc==3?argv[2]:STORE "/profile";
 if(!private_dir(STATE)||!private_dir(STORE))return 65;
 if(!profile_read(profile)){fprintf(stderr,"cold profile unavailable or unsupported\n");return 65;}
 if(!strcmp(argv[1],"verify")){if(argc!=2)return 64;int ok=verify_files()&&verify_payload();printf("COLD_PROFILE_VERIFIED=%d\n",ok);return ok?0:66;}
 if(!strcmp(argv[1],"stage"))return run_stage();
 if(!strcmp(argv[1],"barrier"))return run_barrier();
 if(!strcmp(argv[1],"preflight"))return run_preflight();
 if(argc!=2)return 64;
 if(!strcmp(argv[1],"qualify")){char boot[80];if(!boot_id(boot)||!receipt_matches(boot)||!gpu_is_next()||!value_is(RUN "/result","COLD_AUTOMATED_PASS"))return 67;if(!atomic_text(STATE "/cold-qualified",boot))return 68;unlink(STATE "/cold-pending");sync();return 0;}
 if(strcmp(argv[1],"apply")&&strcmp(argv[1],"restore"))return 64;
 int ok=replace_boot(!strcmp(argv[1],"apply"));return ok?0:70;
}
