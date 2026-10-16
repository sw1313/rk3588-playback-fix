// SPDX-License-Identifier: MIT
// One-shot trial supervisor and observational handle scan. No GPU ioctl/MMIO.
#define _GNU_SOURCE
#include <sys/system_properties.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <poll.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
static int number(const char *s){if(!*s)return 0;while(*s){if(*s<'0'||*s>'9')return 0;s++;}return 1;}
static int scan(void){
 struct stat gpu;unsigned files=0,maps=0,denied=0;DIR *proc;struct dirent *p;
 if(stat("/dev/mali0",&gpu) || !S_ISCHR(gpu.st_mode))return 2;
 proc=opendir("/proc");if(!proc)return 2;
 while((p=readdir(proc))){if(!number(p->d_name))continue;
  char path[512];snprintf(path,sizeof(path),"/proc/%s/fd",p->d_name);DIR *fds=opendir(path);struct dirent *f;
  if(!fds){if(errno!=ENOENT && errno!=ESRCH)denied++;continue;}
  while((f=readdir(fds))){if(!number(f->d_name))continue;
   struct stat st;if(fstatat(dirfd(fds),f->d_name,&st,0)){if(errno!=ENOENT && errno!=ESRCH)denied++;continue;}
   if(S_ISCHR(st.st_mode) && st.st_rdev==gpu.st_rdev)files++;
  }
  closedir(fds);
  snprintf(path,sizeof(path),"/proc/%s/maps",p->d_name);FILE *fp=fopen(path,"re");
  if(!fp){if(errno!=ENOENT && errno!=ESRCH)denied++;continue;}
  char *line=NULL;size_t n=0;while(getline(&line,&n,fp)>0)if(strstr(line,"/dev/mali0"))maps++;
  free(line);fclose(fp);
 }
 closedir(proc);
 printf("visible_gpu_files=%u gpu_vmas=%u denied=%u\n",files,maps,denied);
 return files||maps||denied?3:0;
}

int main(void){return getuid()?64:scan();}
