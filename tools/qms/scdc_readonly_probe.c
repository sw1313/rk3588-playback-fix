/* SPDX-License-Identifier: MIT
 * Exact-board DDC diagnostics via the native I2C adapter, not direct MMIO.
 * Reads only TMDS_CONFIG(20), SCRAMBLER_STATUS(21), STATUS_FLAGS_0(40).
 * Never reads clear-on-read CED counters or writes SCDC configuration.
 * The write I2C message selects the read offset, as required by I2C reads.
 */
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <time.h>
#include <unistd.h>

static unsigned long long ns(clockid_t clock)
{
    struct timespec t;
    clock_gettime(clock,&t);
    return (unsigned long long)t.tv_sec*1000000000ULL+t.tv_nsec;
}
static int connected(void)
{
    char s[32]={0};
    int fd=open("/sys/class/drm/card0-HDMI-A-1/status",O_RDONLY|O_CLOEXEC);
    if(fd<0)return 0;
    ssize_t n=read(fd,s,sizeof(s)-1);close(fd);
    return n>0 && !strcmp(s,"connected\n");
}
static int read_status(int fd,unsigned char offset,unsigned char *value)
{
    if(offset!=0x20 && offset!=0x21 && offset!=0x40){errno=EINVAL;return -1;}
    struct i2c_msg messages[2]={
        {.addr=0x54,.flags=0,.len=1,.buf=&offset},
        {.addr=0x54,.flags=I2C_M_RD,.len=1,.buf=value},
    };
    struct i2c_rdwr_ioctl_data xfer={.msgs=messages,.nmsgs=2};
    int n=ioctl(fd,I2C_RDWR,&xfer);
    if(n==2)return 0;
    if(n>=0)errno=EIO;
    return -1; /* No retry or forced I2C client acquisition. */
}
int main(int argc,char **argv)
{
    unsigned bus,samples=1;char extra,path[PATH_MAX],resolved[PATH_MAX],expected[PATH_MAX];
    unsigned long funcs=0;struct stat st;
    if(getuid()!=0 || (argc!=2 && argc!=3) ||
       sscanf(argv[1],"/dev/i2c-%u%c",&bus,&extra)!=1 || bus>255){
        fputs("usage (root): scdc-readonly64 /dev/i2c-N [samples 1..600, 100ms, max60s]\n",stderr);return 2;
    }
    if(argc==3){
        char *end;unsigned long n=strtoul(argv[2],&end,10);
        if(!*argv[2] || *end || n<1 || n>600)return 2;
        samples=(unsigned)n;
    }
    snprintf(path,sizeof(path),"/sys/class/i2c-dev/i2c-%u/device",bus);
    snprintf(expected,sizeof(expected),"/sys/devices/platform/fde80000.hdmi/i2c-%u",bus);
    if(!realpath(path,resolved) || strcmp(resolved,expected) || !connected()){
        fputs("Refused: native HDMI DDC adapter/connected-sink identity not verified\n",stderr);return 3;
    }
    int fd=open(argv[1],O_RDWR|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0 || fstat(fd,&st) || !S_ISCHR(st.st_mode) || major(st.st_rdev)!=89 ||
       minor(st.st_rdev)!=bus || ioctl(fd,I2C_FUNCS,&funcs) || !(funcs&I2C_FUNC_I2C)){
        fputs("Refused: unsupported I2C device\n",stderr);if(fd>=0)close(fd);return 4;
    }
    setvbuf(stdout,NULL,_IOLBF,0);
    puts("{\"diagnostic_only\":true,\"SCDC_configuration_writes\":false,\"CED_counter_reads\":false}");
    unsigned char previous[3]={0},v[3]={0};unsigned done=0;
    unsigned long long deadline=ns(CLOCK_MONOTONIC)+60000000000ULL;
    for(unsigned i=0;i<samples && ns(CLOCK_MONOTONIC)<deadline;++i){
        if(!connected()){fputs("HDMI disconnected; stopped\n",stderr);close(fd);return 5;}
        unsigned long long begin=ns(CLOCK_MONOTONIC);
        if(read_status(fd,0x20,v) || read_status(fd,0x21,v+1) || read_status(fd,0x40,v+2)){
            perror("SCDC read; stopped without retries");close(fd);return 6;
        }
        ++done;
        if(!i || memcmp(previous,v,3))
            printf("{\"mono_ns\":%llu,\"wall_ns\":%llu,\"read_duration_ns\":%llu,\"sample\":%u,\"tmds_config\":%u,\"scrambler_status\":%u,\"status0\":%u,\"clock_detected\":%u,\"channel_locks\":[%u,%u,%u]}\n",
                ns(CLOCK_MONOTONIC),ns(CLOCK_REALTIME),ns(CLOCK_MONOTONIC)-begin,
                i,v[0],v[1],v[2],v[2]&1,(v[2]>>1)&1,(v[2]>>2)&1,(v[2]>>3)&1);
        memcpy(previous,v,3);
        if(i+1<samples){struct timespec delay={0,100000000};while(nanosleep(&delay,&delay)&&errno==EINTR){}}
    }
    printf("{\"samples_completed\":%u,\"sink_QMS_acknowledgement\":false}\n",done);
    close(fd);return 0;
}
