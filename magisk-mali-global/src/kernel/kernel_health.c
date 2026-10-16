#define _GNU_SOURCE
#include <sys/system_properties.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include "health_policy_v2.h"
static volatile sig_atomic_t stopping;
static void stop_watch(int signal_number) { (void)signal_number; stopping=1; }

/* Local boot health: verified KMD/CSF, post-restore marker, audio IPC and boot.
 * No PC acknowledgement or network connectivity is required.
 * Deliberately has NO service/property setter, reboot, GPU ioctl or unbind.
 * Only observes and writes root-private diagnostic/ack files. */
static uint64_t now_ms(void) {
    struct timespec t; clock_gettime(CLOCK_BOOTTIME,&t);
    return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;
}
static int safe_path(const char *s) {
    const char *prefix="/data/adb/rk3588-mali-global/kernel-run/";
    return !strncmp(s,prefix,strlen(prefix)) && !strstr(s,"..") && strlen(s)<450;
}
static int private_file(const char *s,int flags) {
    if (!safe_path(s)) return -1;
    int fd=open(s,flags|O_CLOEXEC|O_NOFOLLOW,0600);struct stat st;
    if (fd>=0 && (fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_uid!=0)) {close(fd);fd=-1;}
    return fd;
}
static int running(void) {
    const char *names[]={"surfaceflinger","zygote","zygote_secondary","netd","audioserver","vendor.audio-hal","vendor.hwcomposer-2-1"};
    char key[PROP_NAME_MAX],value[PROP_VALUE_MAX];
    for (unsigned i=0;i<sizeof(names)/sizeof(names[0]);i++) {
        snprintf(key,sizeof(key),"init.svc.%s",names[i]);value[0]=0;
        __system_property_get(key,value);if (strcmp(value,"running"))return 0;
    }
    return 1;
}
static int candidate_ready(void){
 const char *paths[]={"/sys/module/rk_mali_next/parameters/trial_fw_size","/sys/module/rk_mali_next/parameters/trial_fw_hi","/sys/module/rk_mali_next/parameters/trial_fw_lo"};
 const unsigned long expected[]={266240,2619577260UL,3266300732UL};char buf[64],link[256];
 ssize_t n=readlink("/sys/bus/platform/devices/fb000000.gpu/driver",link,sizeof(link)-1);if(n<=0)return 0;link[n]=0;if(!strstr(link,"/rk_next_mali"))return 0;
 for(unsigned i=0;i<3;i++){int fd=open(paths[i],O_RDONLY|O_CLOEXEC);if(fd<0)return 0;n=read(fd,buf,sizeof(buf)-1);close(fd);if(n<=0)return 0;buf[n]=0;if(strtoul(buf,NULL,10)!=expected[i])return 0;}
 return 1;
}
static int captured(const char *const args[],char *out,size_t size) {
    int pipes[2];if(pipe2(pipes,O_CLOEXEC))return 0;
    pid_t pid=fork();if(pid<0){close(pipes[0]);close(pipes[1]);return 0;}
    if(!pid) {
        setpgid(0,0);dup2(pipes[1],1);dup2(pipes[1],2);close(pipes[0]);close(pipes[1]);
        execv(args[0],(char *const *)args);_exit(127);
    }
    setpgid(pid,pid);close(pipes[1]);fcntl(pipes[0],F_SETFL,O_NONBLOCK);
    uint64_t end=now_ms()+3000;size_t used=0;int done=0,status=0,complete=0;
    while(now_ms()<end) {
        char buf[4096];ssize_t n;
        while((n=read(pipes[0],buf,sizeof(buf)))>0) {
            size_t take=(size_t)n;if(take>size-used-1)take=size-used-1;
            memcpy(out+used,buf,take);used+=take;
        }
        if(!done && waitpid(pid,&status,WNOHANG)==pid)done=1;
        if(done && n==0){complete=1;break;}
        struct pollfd f={.fd=pipes[0],.events=POLLIN};poll(&f,1,20);
    }
    if(!done) {kill(-pid,SIGKILL);waitpid(pid,&status,WNOHANG);}
    close(pipes[0]);out[used]=0;
    return complete && WIFEXITED(status) && !WEXITSTATUS(status) && used && !strstr(out,"DUMP TIMEOUT");
}
static unsigned observe(const char *ackfile) {
    unsigned bits=running()?1u:0;uint64_t stamp=0;
    if(candidate_ready())bits|=2;
    int fd=private_file(ackfile,O_RDONLY);
    if(fd>=0){if(read(fd,&stamp,sizeof(stamp))!=(ssize_t)sizeof(stamp))stamp=0;close(fd);}
    if(transition_mark_valid(now_ms(),stamp))bits|=4;
    char buf[262144];
    const char *sf[]={"/system/bin/dumpsys","-t","2","SurfaceFlinger","--list",NULL};
    const char *activity[]={"/system/bin/dumpsys","-t","2","activity","activities",NULL};
    const char *window[]={"/system/bin/dumpsys","-t","2","window","displays",NULL};
    if(captured(sf,buf,sizeof(buf)))bits|=8;
    if(captured(activity,buf,sizeof(buf)) && strstr(buf,"ACTIVITY MANAGER ACTIVITIES"))bits|=16;
    if(captured(window,buf,sizeof(buf)) && strstr(buf,"WINDOW MANAGER DISPLAY CONTENTS"))bits|=32;
    const char *audio[]={"/system/bin/dumpsys","-t","2","media.audio_policy",NULL};
    const char *processes[]={"/system/bin/dumpsys","-t","2","activity","processes",NULL};
    if(captured(audio,buf,sizeof(buf)) && strstr(buf,"AudioPolicyManager:"))bits|=64;
    if(captured(processes,buf,sizeof(buf)) && strstr(buf,"mProcessesReady=true") &&
       strstr(buf,"mSystemReady=true") && strstr(buf,"mBooted=true") &&
       strstr(buf,"mBooting=false") && strstr(buf,"mBootAnimationComplete=true"))bits|=128;

    return bits;
}
static void log_sync(int fd,const char *phase,unsigned bits,enum verdict v,unsigned count) {
    char line[200];int n=snprintf(line,sizeof(line),"%llu %s bits=%u verdict=%d consecutive=%u\n",(unsigned long long)now_ms(),phase,bits,(int)v,count);
    if(n<0 || write(fd,line,n)!=n || fsync(fd))_exit(80);
}
static pid_t start_logcat(int fd) {
    pid_t pid=fork();if(pid)return pid;
    setpgid(0,0);dup2(fd,1);dup2(fd,2);
    const char *args[]={"/system/bin/logcat","-b","main","-b","system","-b","events","-v","monotonic","-T","1",NULL};
    execv(args[0],(char *const *)args);_exit(127);
}
static pid_t start_kmsg(int output) {
    pid_t pid=fork();if(pid)return pid;
    signal(SIGTERM,SIG_DFL);
    setpgid(0,0);int input=open("/dev/kmsg",O_RDONLY|O_NONBLOCK|O_CLOEXEC);
    if(input<0)_exit(21);
    if(lseek(input,0,SEEK_END)<0)_exit(22);
    size_t total=0;char buf[8192];
    while(total<8*1024*1024) {
        struct pollfd p={.fd=input,.events=POLLIN};poll(&p,1,500);
        ssize_t n;
        while((n=read(input,buf,sizeof(buf)))>0) {
            if(write(output,buf,n)!=n)_exit(23);total+=(size_t)n;
            if(total>=8*1024*1024)break;
        }
        if(n<0 && errno!=EAGAIN && errno!=EPIPE && errno!=EINTR)_exit(24);
        if(fsync(output))_exit(25);
    }
    close(input);_exit(0);
}
static int policy_test(void) {
    struct health_policy p={.deadline=45000};enum verdict v=HEALTH_WAIT;unsigned cases=0;
    for(unsigned t=0;t<=30000;t+=3000)v=health_sample(&p,t,HEALTH_ALL);
    if(v!=HEALTH_READY)return 11;cases++;
    for(unsigned missing=1;missing<=128;missing<<=1) {
        memset(&p,0,sizeof(p));p.deadline=45000;
        for(unsigned t=0;t<=45000;t+=3000)v=health_sample(&p,t,HEALTH_ALL&~missing);
        if(v!=HEALTH_EXPIRED)return 12;cases++;
    }
    memset(&p,0,sizeof(p));p.deadline=90000;
    for(unsigned t=0;t<30000;t+=3000)health_sample(&p,t,HEALTH_ALL);
    health_sample(&p,30000,1);
    if(health_sample(&p,33000,HEALTH_ALL)!=HEALTH_WAIT || p.consecutive!=1)return 13;cases++;
    memset(&p,0,sizeof(p));p.deadline=90000;
    for(unsigned t=0;t<=27000;t+=3000)health_sample(&p,t,HEALTH_ALL);
    if(health_sample(&p,50000,HEALTH_ALL)!=HEALTH_WAIT || p.consecutive!=1)return 14;cases++;
    if(!transition_mark_valid(20000,12000) || transition_mark_valid(170001,12000) || transition_mark_valid(20000,20001) || transition_mark_valid(20000,0))return 15;cases++;
    p=(struct health_policy){.deadline=45000,.since=0,.last=44000,.consecutive=15};
    if(health_sample(&p,45001,HEALTH_ALL)!=HEALTH_EXPIRED)return 16;cases++;
    printf("PASS %u policy cases; no service writes or reboot path\n",cases);return 0;
}

static int emit_capture_fixture(int fail) {
    char data[1024];memset(data,'x',sizeof(data));
    const char *start="CAPTURE_FIXTURE_BEGIN\n",*end="\nCAPTURE_FIXTURE_END\n";
    if(write(1,start,strlen(start))!=(ssize_t)strlen(start))return 3;
    for(int i=0;i<210;i++)if(write(1,data,sizeof(data))!=(ssize_t)sizeof(data))return 3;
    if(write(1,end,strlen(end))!=(ssize_t)strlen(end))return 3;
    return fail?7:0;
}
static int capture_test(void) {
    char out[262144];
    const char *good[]={"/proc/self/exe","emit-capture-fixture",NULL};
    const char *bad[]={"/proc/self/exe","emit-capture-fixture-fail",NULL};
    if(!captured(good,out,sizeof(out)) || !strstr(out,"CAPTURE_FIXTURE_BEGIN") ||
       !strstr(out,"CAPTURE_FIXTURE_END"))return 71;
    if(captured(bad,out,sizeof(out)))return 72;
    puts("PASS 2 capture cases: full large output footer and child failure rejection");return 0;
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"emit-capture-fixture"))return emit_capture_fixture(0);
    if(argc==2 && !strcmp(argv[1],"emit-capture-fixture-fail"))return emit_capture_fixture(1);
    if(argc==2 && !strcmp(argv[1],"capture-test"))return capture_test();

    if(argc==2 && !strcmp(argv[1],"policy-test"))return policy_test();
    if(getuid()!=0)return 2;
    if(argc!=5 || strcmp(argv[1],"watch"))return 2;
    char *tail;long duration=strtol(argv[4],&tail,10);
    if(*tail || duration<5 || duration>90)return 2;
    int journal=private_file(argv[2],O_WRONLY|O_APPEND|O_CREAT);if(journal<0 || !safe_path(argv[3]))return 3;
    signal(SIGHUP,SIG_IGN);signal(SIGTERM,stop_watch);signal(SIGINT,stop_watch);setsid();prctl(PR_SET_NAME,"rk-health-watch",0,0,0);
    char logfile[512];snprintf(logfile,sizeof(logfile),"%s.logcat",argv[2]);
    int logfd=private_file(logfile,O_WRONLY|O_CREAT|O_TRUNC);if(logfd<0)return 3;
    pid_t logger=start_logcat(logfd);if(logger<0)return 3;
    snprintf(logfile,sizeof(logfile),"%s.kmsg",argv[2]);
    int kmsgfd=private_file(logfile,O_WRONLY|O_CREAT|O_TRUNC);if(kmsgfd<0)return 3;
    pid_t kmsg=start_kmsg(kmsgfd);if(kmsg<0)return 3;
    struct health_policy p={.deadline=now_ms()+(uint64_t)duration*1000};
    enum verdict v=HEALTH_WAIT;unsigned bits=0;log_sync(journal,"BEGIN",0,v,0);
    while(v==HEALTH_WAIT && !stopping) {
        bits=observe(argv[3]);v=health_sample(&p,now_ms(),bits);log_sync(journal,"SAMPLE",bits,v,p.consecutive);
        fsync(logfd);struct stat st;
        if(logger>0 && !fstat(logfd,&st) && st.st_size>8*1024*1024){kill(logger,SIGTERM);logger=-1;}
        if(v==HEALTH_WAIT)usleep(1000000);
    }
    if(stopping)v=HEALTH_EXPIRED;
    if(logger>0){kill(logger,SIGTERM);waitpid(logger,NULL,WNOHANG);}fsync(logfd);close(logfd);
    kill(kmsg,SIGTERM);waitpid(kmsg,NULL,WNOHANG);fsync(kmsgfd);close(kmsgfd);
    log_sync(journal,v==HEALTH_READY?"END_READY":"END_UNHEALTHY_NO_ACTION",bits,v,p.consecutive);
    close(journal);return v==HEALTH_READY?0:77;
}
