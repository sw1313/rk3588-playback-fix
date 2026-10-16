/* SPDX-License-Identifier: GPL-2.0-only
 * Device-specific, one-boot experimental resource handover. No kernel text,
 * regulator counters, MMIO, boot image, or installed firmware writes.
 * Private layout is pinned to the backed-up original image by fingerprints.
 */
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>
#include <linux/pm_runtime.h>
#include "resource_handover_guards.h"
#include "resource_handover_policy.h"

static struct platform_driver *original_pdrv;
static int (*saved_driver_remove)(struct device *);
static int (*saved_platform_remove)(struct platform_device *);
static void (*saved_platform_term)(void *);
static void (**platform_term_slot)(void *);
static void (*original_off_work)(struct work_struct *);
static struct regulator *bridge[2];
static int bridge_held[2],bridge_held_count,hooks_installed,handover_done;
static int platform_term_complete,old_object_gone;
static struct file *progress_file;
static loff_t progress_position;
static char *journal_path;
module_param(journal_path,charp,0400);
module_param(bridge_held_count,int,0444);
module_param(hooks_installed,int,0444);
module_param(handover_done,int,0444);

static int progress(const char *stage)
{
 char row[160];int n,rc;ssize_t wrote;
 pr_info("RK_RESOURCE_HANDOVER: %s\n",stage);
 if(!progress_file)return -ENODEV;
 n=scnprintf(row,sizeof(row),"%s\n",stage);
 wrote=kernel_write(progress_file,row,n,&progress_position);
 if(wrote!=n)return wrote<0?wrote:-EIO;
 rc=vfs_fsync(progress_file,0);return rc;
}
static __noreturn void resource_fail_stop(const char *stage)
{
 /* Returning would allow the driver core to free a partly closed GPU.
  * The independent supervisor requests an ordinary reboot. If the kernel
  * cannot complete it, user power recovery remains necessary. */
 progress(stage);
 for(;;)msleep(1000);
}
static void require_progress(const char *stage)
{
 if(progress(stage))resource_fail_stop("FAILED_PROGRESS_IO_REBOOT_REQUIRED");
}
static int read_owners(unsigned long kb,unsigned long *opp_out,
                       struct regulator **handles,int *counts,unsigned int *enabled)
{
 unsigned long opp,regs,own[2],other[2];unsigned int n;int count,i;
 unsigned char flag;
 if(copy_from_kernel_nofault(&n,(void *)(kb+0xf0),4) || n!=2 ||
    copy_from_kernel_nofault(&opp,(void *)(kb+0xf8),8) || IS_ERR_OR_NULL((void *)opp) ||
    copy_from_kernel_nofault(&regs,(void *)(opp+0x108),8) || !regs ||
    copy_from_kernel_nofault(&count,(void *)(opp+0x110),4) || count!=2 ||
    copy_from_kernel_nofault(&flag,(void *)(opp+0x124),1))return -ESTALE;
 for(i=0;i<2;i++) {
  if(copy_from_kernel_nofault(&own[i],(void *)(kb+0xd0+i*8),8) ||
     copy_from_kernel_nofault(&other[i],(void *)(regs+i*8),8))return -EFAULT;
  handles[i]=(void *)own[i];handles[i+2]=(void *)other[i];
 }
 for(i=0;i<4;i++) {
  if(IS_ERR_OR_NULL(handles[i]) || copy_from_kernel_nofault(&counts[i],(void *)((unsigned long)handles[i]+0x20),4))return -EFAULT;
 }
 if(!resource_counts_valid(n,count,(unsigned long long *)handles,counts,flag))return -EUCLEAN;
 *opp_out=opp;*enabled=flag;return 0;
}
static void repaired_platform_term(void *gpu)
{
 unsigned long kb=(unsigned long)gpu,platform=0,opp;
 struct regulator *handles[4];int counts[4],i;unsigned int enabled;
 unsigned char powered=0,off=0;struct delayed_work *work;
 if(kb!=original_kbdev || old_object_gone || !hooks_installed)
  resource_fail_stop("FAILED_PLATFORM_IDENTITY");
 require_progress("platform-term-enter");
 if(copy_from_kernel_nofault(&platform,(void *)(kb+0x25f8),8) || !platform)
  resource_fail_stop("FAILED_PLATFORM_CONTEXT");
 work=(void *)(platform+0x18);
 require_progress("platform-off-cancel-begin");
 cancel_delayed_work_sync(work);
 require_progress("platform-off-cancel-complete");
 if(copy_from_kernel_nofault(&powered,(void *)platform,1) || powered>1)
  resource_fail_stop("FAILED_PLATFORM_POWER_STATE");
 if(powered) {
  require_progress("platform-native-off-begin");
  original_off_work(&work->work);
  require_progress("platform-native-off-complete");
 }
 /* Devfreq and GPU jobs have already been terminated by the original
  * dev_init unwind before this platform callback. Separate bridge handles
  * keep the physical supply valid through domain detach and next probe. */
 if(read_owners(kb,&opp,handles,counts,&enabled))resource_fail_stop("FAILED_OWNER_SNAPSHOT");
 if(bridge_held_count!=2)resource_fail_stop("FAILED_SUPPLY_BRIDGE");
 require_progress("consumer-balance-begin");
 for(i=0;i<4;i++) {
  if(counts[i] && regulator_disable(handles[i]))resource_fail_stop("FAILED_CONSUMER_DISABLE");
 }
 if(read_owners(kb,&opp,handles,counts,&enabled))resource_fail_stop("FAILED_OWNER_RECHECK");
 for(i=0;i<4;i++)if(counts[i])resource_fail_stop("FAILED_NONZERO_CONSUMER_REFERENCE");
 /* This changes the stale logical enabled flag, never an enable counter.
  * All actual consumer references were balanced using regulator_disable. */
 WRITE_ONCE(*(unsigned char *)(opp+0x124),0);
 WRITE_ONCE(*(unsigned char *)(platform+1),off);
 require_progress("consumers-balanced-opp-disabled");
 saved_platform_term(gpu);
 platform_term_complete=1;
 require_progress("platform-term-return-context-destroyed");
}
static int traced_platform_remove(struct platform_device *pdev)
{
 int rc;
 if(&pdev->dev!=target || old_object_gone)resource_fail_stop("FAILED_REMOVE_IDENTITY");
 require_progress("original-remove-begin");
 rc=saved_platform_remove(pdev);
 old_object_gone=1; /* From this point NEVER dereference old kbdev/context. */
 if(rc || !platform_term_complete)resource_fail_stop("FAILED_ORIGINAL_REMOVE_RETURN");
 WRITE_ONCE(*platform_term_slot,saved_platform_term);
 require_progress("original-remove-return");
 return rc;
}
static int traced_driver_remove(struct device *dev)
{
 int rc;
 if(dev!=target)resource_fail_stop("FAILED_DRIVER_IDENTITY");
 require_progress("driver-remove-and-domain-detach-begin");
 rc=saved_driver_remove(dev);
 if(rc || !old_object_gone)resource_fail_stop("FAILED_DOMAIN_DETACH_RETURN");
 /* platform_drv_remove returns only after dev_pm_domain_detach returns. */
 require_progress("domain-detach-return");
 WRITE_ONCE(original_pdrv->remove,saved_platform_remove);
 WRITE_ONCE(original_pdrv->driver.remove,saved_driver_remove);
 hooks_installed=0;handover_done=1;
 require_progress("driver-remove-return-to-core-finalization");
 return rc;
}
static int resource_guard_init(void)
{
 unsigned long address;unsigned int i;
 BUILD_BUG_ON(offsetof(struct platform_driver,driver)!=0x28);
 BUILD_BUG_ON(offsetof(struct device_driver,remove)!=0x48);
 BUILD_BUG_ON(offsetof(struct delayed_work,work)!=0);
 for(i=0;i<ARRAY_SIZE(resource_guards);i++)
  if(!fingerprint(anchor+resource_guards[i].delta,resource_guards[i].size,resource_guards[i].fnv))return -ENOEXEC;
 original_pdrv=(void *)(anchor+RESOURCE_PDRV_DELTA);
 platform_term_slot=(void *)(anchor+RESOURCE_TERM_SLOT_DELTA);
 original_off_work=(void *)(anchor+RESOURCE_OFF_WORK_DELTA);
 address=(unsigned long)original_pdrv;
 if(address<anchor+RESOURCE_DATA_BEGIN_DELTA || address+sizeof(*original_pdrv)>anchor+RESOURCE_DATA_END_DELTA ||
    (unsigned long)platform_term_slot<anchor+RESOURCE_DATA_BEGIN_DELTA ||
    (unsigned long)platform_term_slot+8>anchor+RESOURCE_DATA_END_DELTA)return -ENOEXEC;
 if(!device_trylock(target))return -EBUSY;
 if(target->driver!=&original_pdrv->driver || target->bus!=&platform_bus_type || target->bus->remove ||
    (unsigned long)original_pdrv->remove!=anchor+RESOURCE_PLATFORM_REMOVE_DELTA ||
    (unsigned long)original_pdrv->driver.remove!=anchor+RESOURCE_DRIVER_REMOVE_DELTA ||
    (unsigned long)*platform_term_slot!=anchor+RESOURCE_PLATFORM_TERM_DELTA) {
  device_unlock(target);return -ENOEXEC;
 }
 saved_platform_remove=original_pdrv->remove;saved_driver_remove=original_pdrv->driver.remove;
 saved_platform_term=*platform_term_slot;device_unlock(target);return 0;
}
/* Called only with the original GPU's device lock held and the open gate armed. */
static int resource_prepare(void)
{
 const char *prefix="/data/adb/rk3588-mali-global/kernel-run/resource-handover-";
 unsigned long opp;struct regulator *owners[4];int counts[4],i,rc;unsigned int enabled;
 if(hooks_installed || handover_done || !journal_path ||
    strncmp(journal_path,prefix,strlen(prefix)) || strstr(journal_path,"..") ||
    strchr(journal_path+strlen(prefix),'/') || strlen(journal_path)>180)return -EINVAL;
 if(read_owners(original_kbdev,&opp,owners,counts,&enabled))return -EUCLEAN;
 if(original_pdrv->remove!=saved_platform_remove || original_pdrv->driver.remove!=saved_driver_remove ||
    *platform_term_slot!=saved_platform_term)return -ESTALE;
 /* A private empty file is created and parent-synced by the host before the
  * trial. Opening it here neither creates an un-synced directory entry nor
  * follows a link. */
 progress_file=filp_open(journal_path,O_WRONLY|O_NOFOLLOW,0);
 if(IS_ERR(progress_file)){rc=PTR_ERR(progress_file);progress_file=NULL;return rc;}
 if(i_size_read(file_inode(progress_file)) || !S_ISREG(file_inode(progress_file)->i_mode))return -EINVAL;
 if(progress("resource-prepare-begin"))return -EIO;
 for(i=0;i<2;i++) {
  bridge[i]=regulator_get(target,i?"mem":"mali");
  if(IS_ERR(bridge[i])){rc=PTR_ERR(bridge[i]);bridge[i]=NULL;goto fail;}
  rc=regulator_enable(bridge[i]);if(rc)goto fail;
  bridge_held[i]=1;bridge_held_count++;
 }
 if(progress("independent-supply-bridge-enabled"))return -EIO;
 WRITE_ONCE(*platform_term_slot,repaired_platform_term);
 WRITE_ONCE(original_pdrv->remove,traced_platform_remove);
 WRITE_ONCE(original_pdrv->driver.remove,traced_driver_remove);
 hooks_installed=1;
 return progress("resource-hooks-installed");
fail:
 for(i=1;i>=0;i--) {
  if(bridge_held[i]){if(regulator_disable(bridge[i]))continue;bridge_held[i]=0;bridge_held_count--;}
  if(bridge[i]){regulator_put(bridge[i]);bridge[i]=NULL;}
 }
 return rc;
}
static int bridge_release_set(const char *value,const struct kernel_param *kp)
{
 int request,i,rc=kstrtoint(value,0,&request);
 if(rc)return rc;
 if(request!=1 || !handover_done || hooks_installed)return -EINVAL;
 if(!device_trylock(target))return -EBUSY;
 if(!target->driver || strcmp(target->driver->name,"rk_next_mali") || !pm_runtime_suspended(target)) {
  device_unlock(target);return -EBUSY;
 }
 for(i=1;i>=0;i--) {
  if(bridge_held[i]) {
   rc=regulator_disable(bridge[i]);if(rc){device_unlock(target);return rc;}
   bridge_held[i]=0;bridge_held_count--;
  }
  if(bridge[i]){regulator_put(bridge[i]);bridge[i]=NULL;}
 }
 device_unlock(target);return progress("bridge-released-candidate-runtime-suspended");
}
static const struct kernel_param_ops bridge_release_ops={.set=bridge_release_set};
module_param_cb(release_bridge,&bridge_release_ops,NULL,0200);
