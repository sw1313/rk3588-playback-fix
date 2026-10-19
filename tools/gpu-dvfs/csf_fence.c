/* SPDX-License-Identifier: MIT
 * Exercise the actual KCPU fence ABI, including queue deletion before fd close.
 * Does not enable tracing or change driver settings. Run with tracing ONLY
 * after verifying csf_fence_safe=Y; the original path has a known trace crash.
 */
#define MALI_USE_CSF 1
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <linux/sync_file.h>
#include "mali_base_kernel.h"
#include "mali_kbase_ioctl.h"
#define CHECK(x) do { if (!(x)) {perror(#x);fprintf(stderr,"line %d\n",__LINE__);return 1;} } while (0)
int main(int argc, char **argv) {
 int rounds=argc>1?atoi(argv[1]):256;CHECK(rounds>0&&rounds<=4096);
 int errors=argc>2&&!strcmp(argv[2],"fdlimit"),failed=0;
 if(errors){struct rlimit limit={64,64};CHECK(setrlimit(RLIMIT_NOFILE,&limit)==0);}
 setbuf(stdout,NULL);
 int gpu=open("/dev/mali0",O_RDWR|O_CLOEXEC);CHECK(gpu>=0);
 struct kbase_ioctl_version_check version={BASE_UK_VERSION_MAJOR,BASE_UK_VERSION_MINOR};
 CHECK(ioctl(gpu,KBASE_IOCTL_VERSION_CHECK,&version)==0);
 CHECK(version.major==BASE_UK_VERSION_MAJOR&&version.minor==BASE_UK_VERSION_MINOR);
 struct kbase_ioctl_set_flags flags={0};CHECK(ioctl(gpu,KBASE_IOCTL_SET_FLAGS,&flags)==0);
 for(int i=0;i<rounds;i++) {
  struct kbase_ioctl_kcpu_queue_new queue={0};CHECK(ioctl(gpu,KBASE_IOCTL_KCPU_QUEUE_CREATE,&queue)==0);
  struct base_fence fence={.basep={.fd=-1,.stream_fd=-1}};
  struct base_kcpu_command command={.type=BASE_KCPU_COMMAND_TYPE_FENCE_SIGNAL};
  command.info.fence.fence=(uint64_t)(uintptr_t)&fence;
  struct kbase_ioctl_kcpu_queue_enqueue enqueue={.addr=(uint64_t)(uintptr_t)&command,.nr_commands=1,.id=queue.id};
  if(errors&&!(i%8)) {
   int held[64],n=0,extra;
   while(n<64&&(extra=open("/dev/null",O_RDONLY|O_CLOEXEC))>=0)held[n++]=extra;
   CHECK(n<64&&errno==EMFILE);
   CHECK(ioctl(gpu,KBASE_IOCTL_KCPU_QUEUE_ENQUEUE,&enqueue)==-1&&errno==EMFILE);
   CHECK(fence.basep.fd==-1);failed++;
   while(n)CHECK(close(held[--n])==0);
  }
  CHECK(ioctl(gpu,KBASE_IOCTL_KCPU_QUEUE_ENQUEUE,&enqueue)==0&&fence.basep.fd>=0);
  struct pollfd p={.fd=fence.basep.fd,.events=POLLIN};CHECK(poll(&p,1,3000)==1&&(p.revents&POLLIN));
  struct kbase_ioctl_kcpu_queue_delete del={.id=queue.id};CHECK(ioctl(gpu,KBASE_IOCTL_KCPU_QUEUE_DELETE,&del)==0);
  /* fd holds a metadata reference after queue deletion. Query it before close. */
  struct sync_fence_info detail={0};
  struct sync_file_info info={.num_fences=1,.sync_fence_info=(uint64_t)(uintptr_t)&detail};
  CHECK(ioctl(fence.basep.fd,SYNC_IOC_FILE_INFO,&info)==0&&info.status==1&&info.num_fences==1);
  CHECK(detail.status==1&&strlen(detail.driver_name)>0&&strlen(detail.obj_name)>0);
  if(!i)printf("fence driver=%s timeline=%s\n",detail.driver_name,detail.obj_name);
  CHECK(close(fence.basep.fd)==0);
 }
 CHECK(close(gpu)==0);printf("PASS ABI%zu rounds=%d fd_failures=%d fences signaled; metadata alive after queue deletion\n",sizeof(void*)*8,rounds,failed);return 0;
}
