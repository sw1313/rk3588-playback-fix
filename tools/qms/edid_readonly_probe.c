/* SPDX-License-Identifier: MIT
 * Diagnostic E-DDC block reads through the verified factory HDMI I2C adapter.
 * Only EDID offset/segment address selection; no EDID/configuration writes.
 * Fixed four-block capture, no scan, no retry and no connector overrides.
 */
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
#include <unistd.h>

int main(void)
{
    char resolved[PATH_MAX], status[32]={0};
    struct stat st;
    unsigned long funcs=0;
    if (getuid()!=0 ||
        !realpath("/sys/class/i2c-dev/i2c-10/device",resolved) ||
        strcmp(resolved,"/sys/devices/platform/fde80000.hdmi/i2c-10")) return 2;
    int fd=open("/sys/class/drm/card0-HDMI-A-1/status",O_RDONLY|O_CLOEXEC);
    if(fd<0) return 2;
    ssize_t n=read(fd,status,sizeof(status)-1); close(fd);
    if(n<=0 || strcmp(status,"connected\n")) return 2;
    fd=open("/dev/i2c-10",O_RDWR|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0) return 3;
    if(fstat(fd,&st) || !S_ISCHR(st.st_mode) || major(st.st_rdev)!=89 ||
       minor(st.st_rdev)!=10 || ioctl(fd,I2C_FUNCS,&funcs) || !(funcs&I2C_FUNC_I2C)) {
        close(fd);return 3;
    }
    for(unsigned block=0;block<4;block++) {
        unsigned char segment=block>>1,offset=(block&1)*128,data[128]={0};
        struct i2c_msg msgs[3]={
            {.addr=0x30,.len=1,.buf=&segment},
            {.addr=0x50,.len=1,.buf=&offset},
            {.addr=0x50,.flags=I2C_M_RD,.len=128,.buf=data},
        };
        struct i2c_rdwr_ioctl_data xfer={.msgs=msgs+(segment?0:1),.nmsgs=segment?3:2};
        if(ioctl(fd,I2C_RDWR,&xfer)!=(int)xfer.nmsgs) {
            perror("E-DDC read stopped without retries");close(fd);return 4;
        }
        printf("%u ",block);
        for(unsigned i=0;i<128;i++) printf("%02x",data[i]);
        putchar('\n');
    }
    close(fd);return 0;
}
