/* SPDX-License-Identifier: MIT
 * Read-only live HDMI/QMS correlation. No input, modeset or register writes.
 * Records state changes and kernel uevents only; not a frame logger.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/netlink.h>
#include "../../decode-kernel/qms/uapi.h"
static volatile sig_atomic_t stopping;
static void stop(int sig){(void)sig;stopping=1;}
static uint64_t now_ms(clockid_t clock){struct timespec t;clock_gettime(clock,&t);return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
static void prefix(void){printf("mono_ms=%llu wall_ms=%llu ",(unsigned long long)now_ms(CLOCK_MONOTONIC),(unsigned long long)now_ms(CLOCK_REALTIME));}
static void physical(char *out,size_t n){
 int fd=open("/sys/class/drm/card0-HDMI-A-1/status",O_RDONLY|O_CLOEXEC);ssize_t got=fd<0?-1:read(fd,out,n-1);
 if(fd>=0)close(fd);if(got<=0){snprintf(out,n,"unknown");return;}out[got]=0;out[strcspn(out,"\r\n")]=0;
}
static int event_field(const char *s){
 static const char *allowed[]={"ACTION=","DEVPATH=","SUBSYSTEM=","HOTPLUG=","CONNECTOR=","CONNECTOR_NAME=","STATE=","STATUS=","SEQNUM="};
 for(unsigned i=0;i<sizeof(allowed)/sizeof(*allowed);++i)if(!strncmp(s,allowed[i],strlen(allowed[i])))return 1;return 0;
}
int main(int argc,char **argv){
 unsigned seconds=argc==2?(unsigned)strtoul(argv[1],0,10):0;
 if(getuid()!=0||!seconds||seconds>900)return 2;
 signal(SIGTERM,stop);signal(SIGINT,stop);setvbuf(stdout,0,_IOLBF,0);
 int fd=open(RKQMS_DEVICE,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0){perror("QMS open");return 3;}
 int nl=socket(AF_NETLINK,SOCK_DGRAM|SOCK_CLOEXEC,NETLINK_KOBJECT_UEVENT);if(nl<0)return 4;
 struct sockaddr_nl addr={.nl_family=AF_NETLINK,.nl_pid=(unsigned)getpid(),.nl_groups=1};
 int bytes=262144;setsockopt(nl,SOL_SOCKET,SO_RCVBUF,&bytes,sizeof(bytes));
 if(bind(nl,(void*)&addr,sizeof(addr))){perror("netlink bind");return 5;}
 struct rkqms_status previous={0};char old_physical[32]="";
 /* The current trial endpoint serializes ioctls. Take one initial snapshot,
  * then observe only sysfs and netlink so diagnostics cannot contend with
  * a live HWC switch. A busy snapshot is unknown, never a display fault. */
 if(ioctl(fd,RKQMS_GET_STATUS,&previous)){memset(&previous,0,sizeof(previous));}
 close(fd);fd=-1;
 uint64_t end=now_ms(CLOCK_MONOTONIC)+(uint64_t)seconds*1000;
 prefix();printf("START read_only=1 pid=%d\n",getpid());
 while(!stopping&&now_ms(CLOCK_MONOTONIC)<end){
  struct rkqms_status status=previous;char phys[32];physical(phys,sizeof(phys));
  if(strcmp(phys,old_physical)){
   prefix();printf("STATUS physical=%s generation=%llu serial=%llu completed=%llu state=%u tfr=%u bpc=%u eotf=%u error=%d\n",phys,
    (unsigned long long)status.generation,(unsigned long long)status.serial,(unsigned long long)status.completed_serial,status.state,status.effective_tfr,status.bits_per_component,status.eotf,status.error);
   previous=status;snprintf(old_physical,sizeof(old_physical),"%s",phys);
  }
  struct pollfd p={.fd=nl,.events=POLLIN};int ret=poll(&p,1,100);
  if(ret<0&&errno!=EINTR)break;
  if(ret>0&&(p.revents&POLLIN)){
   char data[8192];struct sockaddr_nl sender={0};socklen_t length=sizeof(sender);
   ssize_t size=recvfrom(nl,data,sizeof(data)-1,0,(void*)&sender,&length);
   if(size<=0||sender.nl_pid)continue;data[size]=0;
   int relevant=0;
   for(size_t i=0;i<(size_t)size;){size_t len=strnlen(data+i,(size_t)size-i);if(len==(size_t)size-i)break;
    if(!strcmp(data+i,"SUBSYSTEM=drm")||!strcmp(data+i,"SUBSYSTEM=extcon"))relevant=1;i+=len+1;}
   if(!relevant)continue;physical(phys,sizeof(phys));prefix();printf("UEVENT physical=%s",phys);
   for(size_t i=0;i<(size_t)size;){size_t len=strnlen(data+i,(size_t)size-i);if(len==(size_t)size-i)break;
    if(event_field(data+i))printf(" %s",data+i);i+=len+1;}putchar('\n');
  }
 }
 prefix();puts("END");close(nl);return 0;
}
