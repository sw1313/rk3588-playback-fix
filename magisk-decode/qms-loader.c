// SPDX-License-Identifier: MIT
/* Decode-owned exact-payload cold loader and bounded private control client.
 * No shell command construction, no input events, no screenshots or late bind.
 * qms_profile.h is generated from source-built module/HWC artifacts.
 */
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/ioctl.h>
#include <sys/system_properties.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include "qms_profile.h"
#include "../decode-kernel/qms/uapi.h"
#define MOD "/data/adb/modules/rk3588_decode_stack"
#define STATE "/data/adb/rk3588-decode-qms"
static uint64_t now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
static int read_text(const char *p,char *out,size_t cap){int fd=open(p,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return 0;ssize_t n=read(fd,out,cap-1);close(fd);if(n<=0)return 0;out[n]=0;out[strcspn(out,"\r\n")]=0;return 1;}
static int owned(const char *p){struct stat s;return !lstat(p,&s)&&S_ISREG(s.st_mode)&&s.st_uid==0&&!(s.st_mode&022);}
static int execute(const char *const args[],char *out,size_t cap){
 int pipes[2];if(pipe2(pipes,O_CLOEXEC))return 0;pid_t pid=fork();if(pid<0)return 0;
 if(!pid){dup2(pipes[1],1);close(pipes[0]);close(pipes[1]);execv(args[0],(char *const *)args);_exit(127);}
 close(pipes[1]);fcntl(pipes[0],F_SETFL,O_NONBLOCK);uint64_t end=now()+3000;int status=0,done=0;size_t used=0;
 while(now()<end){ssize_t n=read(pipes[0],out+used,cap-used-1);if(n>0)used+=(size_t)n;
  if(waitpid(pid,&status,WNOHANG)==pid){done=1;break;}if(used==cap-1)break;usleep(1000);}
 if(!done){kill(pid,SIGKILL);waitpid(pid,&status,0);}else{ssize_t n=read(pipes[0],out+used,cap-used-1);if(n>0)used+=(size_t)n;}
 close(pipes[0]);out[used]=0;return done&&WIFEXITED(status)&&!WEXITSTATUS(status);
}
static int digest(const char *p,const char *hash){char out[512];const char *args[]={"/system/bin/sha256sum",p,NULL};return owned(p)&&execute(args,out,sizeof(out))&&!strncmp(out,hash,64)&&out[64]==' ';}
static int preexec(void){
 static const char *names[]={"vendor.gralloc-4-0","vendor.hwcomposer-2-1","surfaceflinger"};
 struct stat init,child;if(stat("/proc/1/exe",&init))return 0;
 for(unsigned i=0;i<3;++i){char key[128],value[PROP_VALUE_MAX],path[128],status[4096],*end;
  snprintf(key,sizeof(key),"init.svc_debug_pid.%s",names[i]);__system_property_get(key,value);
  long pid=strtol(value,&end,10);if(*end||pid<=1||pid>INT_MAX)return 0;
  snprintf(path,sizeof(path),"/proc/%ld/status",pid);
  int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return 0;ssize_t n=read(fd,status,sizeof(status)-1);close(fd);if(n<=0)return 0;status[n]=0;
  char *s=strstr(status,"State:"),*p=strstr(status,"PPid:");if(!s||!p)return 0;s+=6;while(*s==' '||*s=='\t')++s;
  if((*s!='T'&&*s!='t')||strtol(p+5,NULL,10)!=1)return 0;
  snprintf(path,sizeof(path),"/proc/%ld/exe",pid);if(stat(path,&child)||child.st_dev!=init.st_dev||child.st_ino!=init.st_ino)return 0;
 }return 1;
}
static int endpoint(void){
 char text[80],extra;unsigned int maj,min;struct stat s;
 if(!read_text("/sys/class/misc/rk_decode_qms/dev",text,sizeof(text))||sscanf(text,"%u:%u %c",&maj,&min,&extra)!=2)return -1;
 int fd=open(RKQMS_DEVICE,O_RDWR|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return -1;
 if(fstat(fd,&s)||!S_ISCHR(s.st_mode)||major(s.st_rdev)!=maj||minor(s.st_rdev)!=min){close(fd);return -1;}return fd;
}
static int status(int fd,struct rkqms_status *s){memset(s,0,sizeof(*s));if(ioctl(fd,RKQMS_GET_STATUS,s))return 0;return s->abi==RKQMS_ABI&&s->bytes==sizeof(*s)&&s->generation;}
static int request(int fd,struct rkqms_status *s,unsigned int target){
 struct rkqms_command c={0};c.abi=RKQMS_ABI;c.bytes=sizeof(c);c.generation=s->generation;c.link_cookie=s->link_cookie;
 c.expected_serial=s->serial;c.connector=s->connector;c.crtc=s->crtc;c.target_tfr=target;c.timeout_ms=1500;
 if(ioctl(fd,target?RKQMS_SWITCH:RKQMS_RESTORE,&c))return 0;
 if(c.result.generation!=c.generation||c.result.serial!=c.expected_serial+1||c.result.completed_serial!=c.result.serial||
    c.result.error||c.result.effective_tfr!=(target?target:s->base_tfr)||c.result.state!=(target?RKQMS_ACTIVE:RKQMS_BASE)||!c.result.observed_ns)return 0;
 *s=c.result;return 1;
}
static int atomic_text(const char *p,const char *value){
 char temp[256];snprintf(temp,sizeof(temp),"%s.next.%d",p,getpid());int fd=open(temp,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)return 0;
 size_t n=strlen(value);int ok=write(fd,value,n)==(ssize_t)n&&!fsync(fd);close(fd);if(ok)ok=!rename(temp,p);if(!ok)unlink(temp);return ok;
}
static int journal(const char *stage,int error){
 char row[256];snprintf(row,sizeof(row),"%llu %s error=%d errno=%d\n",(unsigned long long)now(),stage,error,errno);
 int fd=open(STATE "/loader-journal",O_WRONLY|O_APPEND|O_CREAT|O_CLOEXEC|O_NOFOLLOW,0600);
 if(fd<0)return 0;
 size_t n=strlen(row);int ok=write(fd,row,n)==(ssize_t)n&&!fsync(fd);close(fd);return ok;
}
static int failed(int code){journal("FAILED",code);return code;}
int main(int argc,char **argv){
 if(getuid()!=0||argc<2)return 2;
 if(!strcmp(argv[1],"bind")){
  char boot[80],plan[256];struct rkqms_status s;
  if(argc!=4||!read_text("/proc/sys/kernel/random/boot_id",boot,sizeof(boot))||strcmp(boot,argv[2])||strcmp(argv[3],QMS_PROFILE_ID)||!preexec())return 3;
  if(!journal(boot,0))return 3;
  snprintf(plan,sizeof(plan),"%s %s prepared",boot,QMS_PROFILE_ID);char current[256];
  if(!read_text(STATE "/plan",current,sizeof(current))||strcmp(plan,current)||
     !digest(MOD "/components/qms/rk_decode_qms.ko",QMS_MODULE_SHA)||
     !digest("/vendor/lib64/hw/hwcomposer.rk30board.so",QMS_HWC_SHA)||!access(RKQMS_DEVICE,F_OK))return failed(4);
  int module=open(MOD "/components/qms/rk_decode_qms.ko",O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(module<0)return failed(5);
  if(!journal("FINIT_MODULE_BEGIN",0)){close(module);return failed(5);}
  int loaded=syscall(__NR_finit_module,module,"cold_bind=1 sync_native_clock=1",0);int load_errno=errno;close(module);errno=load_errno;if(loaded)return failed(6);
  if(!journal("FINIT_MODULE_COMPLETE",0))return failed(6);
  int fd=endpoint();if(fd<0)return failed(7);
  if(!status(fd,&s)||s.state!=RKQMS_BASE||s.proof!=RKQMS_PROOF_ALL||!journal("BASELINE_SWITCH_BEGIN",0)||!request(fd,&s,s.base_tfr)||
     !journal("BASELINE_RESTORE_BEGIN",0)||!request(fd,&s,0)){close(fd);return failed(8);}
  if(!journal("BASELINE_RESTORE_COMPLETE",0)){close(fd);return failed(8);}
  char result[512];snprintf(result,sizeof(result),"{\"boot\":\"%s\",\"profile\":\"%s\",\"baseline_VTEM_enable_restore\":true,\"generation\":%llu,\"serial\":%llu,\"state\":%u,\"physical_HDMI_accepted\":false}\n",boot,QMS_PROFILE_ID,(unsigned long long)s.generation,(unsigned long long)s.serial,s.state);
  close(fd);if(!atomic_text(STATE "/baseline",result))return failed(9);
  const char *label[]={"/system/bin/chcon","u:object_r:rkqms_device:s0",RKQMS_DEVICE,NULL};char out[256];
  if(chown(RKQMS_DEVICE,1000,1003)||chmod(RKQMS_DEVICE,0660)||!execute(label,out,sizeof(out))||!preexec()||!atomic_text(STATE "/receipt",plan))return failed(10);
  printf("%s\n",plan);return 0;
 }
 int fd=endpoint();if(fd<0)return 11;struct rkqms_status s;
 if(!status(fd,&s)){close(fd);return 12;}
 if(!strcmp(argv[1],"status"))printf("{\"generation\":%llu,\"cookie\":%llu,\"state\":%u,\"proof\":%u,\"serial\":%llu,\"completed\":%llu,\"effective_tfr\":%u,\"bpc\":%u,\"eotf\":%u,\"error\":%d}\n",(unsigned long long)s.generation,(unsigned long long)s.link_cookie,s.state,s.proof,(unsigned long long)s.serial,(unsigned long long)s.completed_serial,s.effective_tfr,s.bits_per_component,s.eotf,s.error);
 else if(!strcmp(argv[1],"restore")){if(!request(fd,&s,0)){close(fd);return 13;}}
 else {close(fd);return 2;}
 close(fd);return 0;
}
