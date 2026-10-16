// SPDX-License-Identifier: MIT
/* Explicit actual KMD/CSF experiment, using the verified client recovery order.
 * Transition subprocess unbinds the original GPU and binds the candidate. No boot writes.
 * Preserve native audio services. Failure after a transition requests one ordinary
 * init reboot from an independent process; this is NOT kernel-hang protection.
 */
#define _GNU_SOURCE
#include <sys/system_properties.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include "audio_preserving_policy.h"

static const char *prefix="/data/adb/rk3588-mali-global/kernel-run/";
static const char *clients[]={"surfaceflinger","zygote_secondary","zygote"};
static const char *audio[]={"audioserver","vendor.audio-hal"};
static char restore_marker[512];
static char audio_pid[2][PROP_VALUE_MAX],client_pid[3][PROP_VALUE_MAX],initial_boot[64];
static int journal=-1,heartbeat_fd=-1,live_cycle;
struct shared { atomic_int transition,disarmed,reboot_once; atomic_llong restored_at; };
static struct shared *shared;
static uint64_t ms(void){struct timespec t;clock_gettime(CLOCK_BOOTTIME,&t);return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
static void beat(void){if(heartbeat_fd>=0){ssize_t ignored=write(heartbeat_fd,"H",1);(void)ignored;}}
static void record(const char *fmt,...){
 char body[512],row[640];va_list ap;va_start(ap,fmt);vsnprintf(body,sizeof(body),fmt,ap);va_end(ap);
 int n=snprintf(row,sizeof(row),"%llu pid=%d %s\n",(unsigned long long)ms(),getpid(),body);
 if(n>0 && n<(int)sizeof(row) && journal>=0){ssize_t ignored=write(journal,row,n);(void)ignored;fsync(journal);}
}
static int path_ok(const char *s){return !strncmp(s,prefix,strlen(prefix)) && !strstr(s,"..") && strlen(s)<440;}
static int read_text(const char *path,char *out,size_t size){
 int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return 0;
 ssize_t n=read(fd,out,size-1);close(fd);if(n<=0)return 0;out[n]=0;out[strcspn(out,"\r\n")]=0;return 1;
}
static void prop(const char *base,const char *service,char *out){
 char key[128];snprintf(key,sizeof(key),"%s.%s",base,service);out[0]=0;__system_property_get(key,out);
}
static int running(const char *service){char s[PROP_VALUE_MAX];prop("init.svc",service,s);return !strcmp(s,"running");}
static int same_boot(void){char b[64];return read_text("/proc/sys/kernel/random/boot_id",b,sizeof(b)) && !strcmp(b,initial_boot);}
static int audio_unchanged(void){
 for(unsigned i=0;i<2;i++){char pid[PROP_VALUE_MAX];prop("init.svc_debug_pid",audio[i],pid);if(!running(audio[i]) || strcmp(pid,audio_pid[i]))return 0;}return 1;
}
static int request(const char *key,const char *service){
 if(!live_cycle || !allowed(key,service) || !same_boot() || !audio_unchanged())return -1;
 beat();record("request %s %s",key,service);int rc=__system_property_set(key,service);record("request-return rc=%d",rc);return rc;
}
static int wait_state(const char *service,const char *wanted,unsigned timeout){
 uint64_t end=ms()+timeout;char s[PROP_VALUE_MAX];
 do{beat();prop("init.svc",service,s);if(!strcmp(s,wanted))return audio_unchanged() && same_boot();usleep(20000);}while(ms()<end);
 record("service-timeout %s wanted=%s",service,wanted);return 0;
}
/* Bound child calls and consume complete output, including data queued at exit. */
static int command(const char *const argv[],char *out,size_t size,unsigned timeout){
 int fds[2];if(pipe2(fds,O_CLOEXEC))return 0;
 pid_t child=fork();if(child<0){close(fds[0]);close(fds[1]);return 0;}
 if(!child){setpgid(0,0);dup2(fds[1],1);dup2(fds[1],2);close(fds[0]);close(fds[1]);execv(argv[0],(char *const *)argv);_exit(127);}
 setpgid(child,child);close(fds[1]);fcntl(fds[0],F_SETFL,O_NONBLOCK);
 size_t used=0;int done=0,status=0,complete=0,truncated=0;uint64_t end=ms()+timeout;
 while(ms()<end){
  char buf[4096];ssize_t n;beat();
  while((n=read(fds[0],buf,sizeof(buf)))>0){size_t take=(size_t)n;if(take>size-used-1){take=size-used-1;truncated=1;}memcpy(out+used,buf,take);used+=take;}
  if(!done && waitpid(child,&status,WNOHANG)==child)done=1;
  if(done && n==0){complete=1;break;}
  struct pollfd p={.fd=fds[0],.events=POLLIN};poll(&p,1,20);
 }
 if(!done){kill(-child,SIGKILL);waitpid(child,&status,WNOHANG);}close(fds[0]);out[used]=0;
 return complete && !truncated && WIFEXITED(status) && !WEXITSTATUS(status) && !strstr(out,"DUMP TIMEOUT");
}
static int ready_ipc(void){
 char *out=malloc(524288);if(!out)return 0;
 const char *a[]={"/system/bin/dumpsys","-t","2","media.audio_policy",NULL};
 const char *b[]={"/system/bin/dumpsys","-t","2","activity","processes",NULL};
 int ok=command(a,out,524288,3500) && strstr(out,"AudioPolicyManager:");
 if(ok)ok=command(b,out,524288,3500) && strstr(out,"mProcessesReady=true") && strstr(out,"mSystemReady=true") && strstr(out,"mBooted=true") && strstr(out,"mBooting=false") && strstr(out,"mBootAnimationComplete=true");
 free(out);return ok;
}
static int original_gpu(void){
 char version[128],driver[128];ssize_t n=readlink("/sys/class/misc/mali0/device/driver",driver,sizeof(driver)-1);
 if(n<0)return 0;driver[n]=0;
 return !strcmp(driver,"../../../bus/platform/drivers/mali") && read_text("/sys/module/bifrost_kbase/version",version,sizeof(version)) && !strcmp(version,"g12p0-01eac0 (UK version 1.11)");
}
static int snapshot(const char *phase){
 DIR *dir=opendir("/sys/kernel/debug/mali0/ctx");if(!dir){record("contexts phase=%s unavailable",phase);return -1;}
 struct dirent *e;int count=0;
 while((e=readdir(dir)))if(e->d_name[0]!='.' && strcmp(e->d_name,"defaults"))count++;
 closedir(dir);record("contexts phase=%s count=%d",phase,count);return count;
}
static int persist_kernel_fallback(void){
 const char *dir="/data/adb/rk3588-mali-global";
 char tmp[256];snprintf(tmp,sizeof(tmp),"%s/kernel-mode.failure.%d",dir,getpid());
 int fd=open(tmp,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)return 0;
 int ok=write(fd,"stock\n",6)==6 && !fsync(fd);close(fd);
 if(ok)ok=!rename(tmp,"/data/adb/rk3588-mali-global/kernel-mode");
 if(!ok)unlink(tmp);
 fd=open(dir,O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(fd>=0){ok=ok&&!fsync(fd);close(fd);}else ok=0;
 return ok;
}
static int mark_restored(void){
 int fd=open(restore_marker,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)return 0;
 uint64_t stamp=ms();int ok=write(fd,&stamp,sizeof(stamp))==sizeof(stamp) && !fsync(fd);close(fd);return ok;
}
static void fallback_reboot(const char *reason){
 if(!reboot_allowed(atomic_load(&shared->transition),atomic_load(&shared->disarmed),atomic_load(&shared->reboot_once),same_boot()))_exit(0);
 int expected=0;if(!atomic_compare_exchange_strong(&shared->reboot_once,&expected,1))_exit(0);
 record("fallback-choice-persisted=%d",persist_kernel_fallback());
 /* A pending boot marker also prevents retry if persistence is interrupted. */
 int rc=__system_property_set("sys.powerctl","reboot");
 record("fallback-init-reboot reason=%s rc=%d requested_once=1",reason,rc);_exit(rc?92:0);
}
static void supervise(int control,int ready){
 setsid();prctl(PR_SET_NAME,"rk-audio-safe",0,0,0);signal(SIGHUP,SIG_IGN);
 int null=open("/dev/null",O_RDWR|O_CLOEXEC);if(null>=0){dup2(null,0);dup2(null,1);dup2(null,2);if(null>2)close(null);}
 if(write(ready,"R",1)!=1)_exit(91);close(ready);
 uint64_t end=ms()+120000;const char *reason="controller-lost";
 while(ms()<end){
  struct pollfd p={.fd=control,.events=POLLIN|POLLHUP};int rc;do{rc=poll(&p,1,12000);}while(rc<0 && errno==EINTR);
  char c=0;if(rc>0 && read(control,&c,1)==1){if(c=='H')continue;if(c=='D' && atomic_load(&shared->disarmed)){record("supervisor-disarmed");_exit(0);}}
  if(rc==0)reason="heartbeat-timeout";break;
 }
 if(ms()>=end)reason="absolute-deadline";
 if(!atomic_load(&shared->transition)){record("controller-lost-before-transition no-actions");_exit(0);}
 fallback_reboot(reason);
}
static int wait_health(const char *path){
 uint64_t deadline=ms()+65000;char data[32768];
 while(ms()<deadline){
  beat();
  if(!same_boot() || !audio_unchanged()){record("unexpected-audio-or-boot-change fail-immediately");return 0;}
  for(unsigned i=0;i<3;i++){
   char pid[PROP_VALUE_MAX];prop("init.svc_debug_pid",clients[i],pid);
   if(!running(clients[i]) || strcmp(pid,client_pid[i])){record("unexpected-client-exit %s fail-immediately",clients[i]);return 0;}
  }
  int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
  if(fd>=0){ssize_t n=read(fd,data,sizeof(data)-1);close(fd);if(n>0){
   data[n]=0;char *ready=strstr(data," END_READY");
   if(ready){char *row=ready;while(row>data && row[-1]!='\n')row--;uint64_t stamp=strtoull(row,NULL,10),mark=(uint64_t)atomic_load(&shared->restored_at);
    if(mark && stamp>=mark+30000 && strstr(row,"bits=255") && audio_unchanged() && same_boot() && ready_ipc())return 1;
    record("reject-stale-or-incomplete-health");return 0;
   }
   if(strstr(data,"END_UNHEALTHY_NO_ACTION"))return 0;
  }}usleep(150000);
 }return 0;
}
static int restore(void){
 char oldpid[PROP_VALUE_MAX],newpid[PROP_VALUE_MAX];prop("init.svc_debug_pid","netd",oldpid);
 if(!oldpid[0] || !audio_unchanged())return 0;
 /* Match the useful Zygote hook without restarting native audio/media services. */
 const char *vdc[]={"/system/bin/vdc","volume","abort_fuse",NULL};char out[4096];
 record("abort-fuse-before-framework-start");
 if(!command(vdc,out,sizeof(out),4000)){record("abort-fuse-failed no-additional-service-requests");return 0;}
 /* netd's onrestart restarts both Zygotes on this ROM. Explicit stop/start
  * reconstructs its state while deliberately avoiding that recursive hook. */
 if(request("ctl.stop","netd") || !wait_state("netd","stopped",4000))return 0;
 if(request("ctl.start","netd"))return 0;
 uint64_t end=ms()+12000;int changed=0;
 while(ms()<end){beat();prop("init.svc_debug_pid","netd",newpid);if(running("netd") && newpid[0] && strcmp(oldpid,newpid)){changed=1;break;}usleep(50000);}
 if(!changed || !audio_unchanged())return 0;
 /* A surviving Rockchip composer may retain an active mode ID absent from its
  * refreshed modes list after its client dies. Recreate composer while SF is
  * explicitly stopped; do not trigger its onrestart->SF->Zygote cascade. */
 if(request("ctl.start","vendor.hwcomposer-2-1") || !wait_state("vendor.hwcomposer-2-1","running",6000))return 0;
 const char *order[]={"surfaceflinger","zygote_secondary","zygote"};
 for(unsigned i=0;i<3;i++)if(request("ctl.start",order[i]) || !wait_state(order[i],"running",6000))return 0;
 for(unsigned i=0;i<3;i++){prop("init.svc_debug_pid",clients[i],client_pid[i]);if(!client_pid[i][0])return 0;}
 atomic_store(&shared->restored_at,(long long)ms());if(!mark_restored())return 0;record("restore-started audio-pids-preserved=1 no-primary-restart=1");return 1;
}
static int candidate_command(const char *script,unsigned timeout){
 const char *args[]={"/system/bin/sh",script,NULL};char out[32768];
 record("candidate-command %s",script);
 int ok=command(args,out,sizeof(out),timeout);
 record("candidate-command-complete success=%d",ok);
 return ok;
}
static int policy_test(void){
 unsigned tests=0;
 for(unsigned i=0;i<3;i++)for(unsigned j=0;j<2;j++){if(!allowed(j?"ctl.start":"ctl.stop",clients[i]))return 10;tests++;}
 const char *bad[][2]={{"ctl.restart","zygote"},{"ctl.restart","zygote_secondary"},{"ctl.restart","audioserver"},{"ctl.stop","audioserver"},{"ctl.restart","vendor.audio-hal"},{"ctl.restart","netd"},{"sys.powerctl","reboot"},{"ctl.start","unknown"}};
 for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++){if(allowed(bad[i][0],bad[i][1]))return 11;tests++;}
 if(!allowed("ctl.stop","netd") || !allowed("ctl.start","netd"))return 12;tests+=2;
 if(!allowed("ctl.stop","vendor.hwcomposer-2-1") || !allowed("ctl.start","vendor.hwcomposer-2-1") || allowed("ctl.restart","vendor.hwcomposer-2-1"))return 14;tests+=3;
 for(unsigned bits=0;bits<16;bits++){int expected=(bits==9);if(reboot_allowed(!!(bits&1),!!(bits&2),!!(bits&4),!!(bits&8))!=expected)return 13;tests++;}
 printf("PASS %u policy cases: audio/zygote restart refused; single-reboot eligibility gated\n",tests);return 0;
}
int main(int argc,char **argv){
 if(argc==2 && !strcmp(argv[1],"policy-test"))return policy_test();
 if(argc!=4 || getuid())return 64;
 int preflight=!strcmp(argv[1],"preflight"),idle=!strcmp(argv[1],"idle-controller-loss"),reboot_test=!strcmp(argv[1],"reboot-fallback-test");
 live_cycle=!strcmp(argv[1],"composer-kmd-trial");
 if(!preflight && !idle && !reboot_test && !live_cycle)return 64;
 if(!path_ok(argv[2]) || !path_ok(argv[3]))return 64;
 snprintf(restore_marker,sizeof(restore_marker),"%s.restored",argv[3]);
 if(!read_text("/proc/sys/kernel/random/boot_id",initial_boot,sizeof(initial_boot)) || !original_gpu())return 65;
 for(unsigned i=0;i<2;i++){prop("init.svc_debug_pid",audio[i],audio_pid[i]);if(!running(audio[i]) || !audio_pid[i][0])return 66;}
 for(unsigned i=0;i<3;i++)if(!running(clients[i]))return 67;
 if(!running("vendor.hwcomposer-2-1"))return 67;
 if(!ready_ipc())return 68;
 journal=open(argv[2],O_WRONLY|O_APPEND|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(journal<0)return 69;
 record("preflight-ready mode=%s audio-preserved=1 gpu-original=1",argv[1]);
 if(preflight){record("preflight-complete no-actions");return 0;}
 char lock[512];snprintf(lock,sizeof(lock),"%sactive-controller.lock",prefix);
 int lockfd=open(lock,O_RDWR|O_CREAT|O_CLOEXEC|O_NOFOLLOW,0600);struct stat st;
 if(lockfd<0 || fstat(lockfd,&st) || !S_ISREG(st.st_mode) || st.st_uid || flock(lockfd,LOCK_EX|LOCK_NB))return 70;
 shared=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS,-1,0);if(shared==MAP_FAILED)return 71;
 atomic_init(&shared->transition,0);atomic_init(&shared->disarmed,0);atomic_init(&shared->reboot_once,0);atomic_init(&shared->restored_at,0);
 signal(SIGHUP,SIG_IGN);signal(SIGPIPE,SIG_IGN);
 int control[2],ready[2];if(pipe2(control,O_CLOEXEC) || pipe2(ready,O_CLOEXEC))return 72;
 pid_t child=fork();if(child<0)return 73;
 if(!child){close(control[1]);close(ready[0]);supervise(control[0],ready[1]);_exit(74);}
 close(control[0]);close(ready[1]);char c=0;struct pollfd p={.fd=ready[0],.events=POLLIN};
 if(poll(&p,1,1500)!=1 || read(ready[0],&c,1)!=1 || c!='R'){close(control[1]);return 75;}
 close(ready[0]);heartbeat_fd=control[1];
 if(idle){record("kill-controller-before-transition");kill(getpid(),SIGKILL);return 76;}
 if(reboot_test){record("explicit-reboot-fallback-test no-client-stop=1");atomic_store(&shared->transition,1);kill(getpid(),SIGKILL);return 77;}
 if(!live_cycle)return 78;
 snapshot("initial");atomic_store(&shared->transition,1);int ok=1;
 for(unsigned i=0;i<3 && ok;i++)ok=request("ctl.stop",clients[i])==0 && wait_state(clients[i],"stopped",3000);
 if(ok)ok=request("ctl.stop","vendor.hwcomposer-2-1")==0 && wait_state("vendor.hwcomposer-2-1","stopped",4000);
 if(ok)ok=snapshot("stopped")==0;
 if(ok)ok=candidate_command("/data/adb/modules/rk3588_mali_global/kernel-transition.sh",40000);
 if(ok)ok=restore();
 if(ok)ok=wait_health(argv[3]);
 if(ok)ok=candidate_command("/data/adb/modules/rk3588_mali_global/kernel-verify.sh",25000);
 if(ok){snapshot("restored");record("qualified-recovery audio-pids-preserved=1");atomic_store(&shared->disarmed,1);if(write(control[1],"D",1)!=1)ok=0;}
 close(control[1]);heartbeat_fd=-1;
 if(!ok){record("recovery-not-qualified reboot-fallback-required");fallback_reboot("main-recovery-failed");}
 int status=0;if(waitpid(child,&status,0)!=child || !WIFEXITED(status) || WEXITSTATUS(status))return 79;
 record("complete qualified=1");return 0;
}
