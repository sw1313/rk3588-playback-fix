// SPDX-License-Identifier: GPL-2.0-only
/* Explicit one-boot module startup gate; reboot is the only recovery once armed.
 * This helper installs one-shot writable driver callbacks for resource handover.
 * It never writes firmware/MMIO/kernel text/boot or raw regulator counters.
 * Armed module stays pinned until reboot; never dereference old kbdev after unbind.
 */
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include "fixture_guards.h"
#include "gpu_layout.h"
#include "identity_guards.h"
#include "original_fw_state_guard.h"
#include "original_power_guard.h"

static struct device *target;
static struct mutex *misc_mutex;
static const void *(*get_list)(void);
static void (*put_list)(void);
static unsigned long original_kbdev, expected_open, anchor;
static const struct file_operations *original_fops;
static int ready, pulses, restored, armed;
static int power_prepared;
static int (*original_power_on)(void *);
static bool reject_guard,allow_initialized;
module_param(power_prepared,int,0444);
module_param(allow_initialized,bool,0444);
static atomic_t denied = ATOMIC_INIT(0);
module_param(ready,int,0444);
module_param(pulses,int,0444);
module_param(restored,int,0444);
module_param(armed,int,0444);
module_param(reject_guard,bool,0444);

static bool fingerprint(unsigned long addr,size_t n,u64 expected)
{
    unsigned char buf[64];u64 h=14695981039346656037ULL;size_t off,count,i;
    for(off=0;off<n;off+=count) {
        count=min_t(size_t,sizeof(buf),n-off);
        if(copy_from_kernel_nofault(buf,(void *)(addr+off),count))return false;
        for(i=0;i<count;i++)h=(h^buf[i])*1099511628211ULL;
    }
    return h==expected;
}
static int reject_open(struct inode *inode,struct file *file)
{
    atomic_inc(&denied);return -EAGAIN;
}
static const struct file_operations gate_fops={.owner=THIS_MODULE,.open=reject_open,.llseek=no_llseek};

/* Caller holds device lock during pulses; list lock protects initial lookup. */
static int inspect_locked(unsigned long *kb_out,struct device **dev_out,
                          const struct file_operations **fops_out)
{
    unsigned long head=(unsigned long)get_list(),node,next,prev,kb,dev,ops,name,open_fn,owner;
    char label[16];int id,next_id,ret=-ESTALE;
    if(!head||copy_from_kernel_nofault(&node,(void *)head,8)||node==head)goto out;
    if(copy_from_kernel_nofault(&next,(void *)node,8)||next!=head||
       copy_from_kernel_nofault(&prev,(void *)(node+8),8)||prev!=head)goto out;
    kb=node-GPU_LIST_OFFSET;
    if(copy_from_kernel_nofault(&dev,(void *)(kb+0x20),8)||!dev||
       copy_from_kernel_nofault(&ops,(void *)(kb+0x38),8)||!ops||
       copy_from_kernel_nofault(&name,(void *)(kb+0x30),8)||!name||
       copy_from_kernel_nofault(label,(void *)name,16)||memcmp(label,"mali0\0",6)||
       copy_from_kernel_nofault(&open_fn,(void *)(ops+0x70),8)||open_fn!=expected_open||
       copy_from_kernel_nofault(&owner,(void *)ops,8)||owner||
       copy_from_kernel_nofault(&id,(void *)(kb+0x110),4)||id!=0||
       copy_from_kernel_nofault(&next_id,(void *)(anchor+IDENTITY_COUNTER_DELTA),4)||next_id!=1)goto out;
    *kb_out=kb;*dev_out=(void *)dev;*fops_out=(void *)ops;
    if(!target) {
        /* Pin the parent while the original instance is still on its list. */
        if(!get_device((void *)dev))goto out;
    }
    ret=0;
out:
    put_list();return ret;
}

#include "resource_handover_impl.h"
static int pulse_set(const char *value,const struct kernel_param *kp)
{
 struct device *dev;const struct file_operations *ops;struct miscdevice *mdev;
 unsigned long kb;unsigned char initialized=255;struct mutex *fw_lock;
 int request,ret=kstrtoint(value,0,&request);
 if(ret)return ret;
 if(request!=1 || armed || pulses)return -EINVAL;
 if(!ready || !target)return -ENODEV;
 if(!device_trylock(target))return -EBUSY;
 ret=inspect_locked(&kb,&dev,&ops);
 if(ret || kb!=original_kbdev || dev!=target || ops!=original_fops){ret=-ESTALE;goto device_done;}
 mdev=(void *)(kb+0x28);fw_lock=(void *)(kb+0x2af8);
 if(mdev->parent!=target || !target->driver || strcmp(target->driver->name,"mali")){ret=-ESTALE;goto device_done;}
 if(!mutex_trylock(misc_mutex)){ret=-EBUSY;goto device_done;}
 if(mdev->fops!=original_fops){ret=-ESTALE;goto misc_done;}
 if(!mutex_trylock(fw_lock)){ret=-EBUSY;goto misc_done;}
 if(copy_from_kernel_nofault(&initialized,(void *)(kb+ORIGINAL_FW_FLAG_OFFSET),1) || initialized>1 || (initialized && !allow_initialized)){ret=-EBUSY;goto fw_done;}
 if(!try_module_get(THIS_MODULE)){ret=-ENODEV;goto fw_done;}
 WRITE_ONCE(mdev->fops,&gate_fops);WRITE_ONCE(armed,1);pulses=1;ret=0;
 pr_info("RK_MALI_BOOT_GUARD: gate armed firmware_initialized=%u explicit_quiesced_trial=%u; reboot-only recovery\n",initialized,allow_initialized);
fw_done:
 mutex_unlock(fw_lock);
misc_done:
 mutex_unlock(misc_mutex);
device_done:
 device_unlock(target);return ret;
}
static int prepare_power_set(const char *value,const struct kernel_param *kp)
{
 int request,ret=kstrtoint(value,0,&request);
 if(ret)return ret;
 if(request!=1 || !ready || !armed || power_prepared || !original_power_on)return -EINVAL;
 if(!device_trylock(target))return -EBUSY;
 if(!target->driver || strcmp(target->driver->name,"mali") ||
    (unsigned long)dev_get_drvdata(target)!=original_kbdev ||
    ((struct miscdevice *)(original_kbdev+0x28))->fops!=&gate_fops){ret=-ESTALE;goto done;}
 pr_info("RK_MALI_BOOT_GUARD: native regulator-clock-domain preparation begins\n");
 ret=original_power_on((void *)original_kbdev);
 if(ret>=0)ret=resource_prepare();
 if(ret>=0){power_prepared=1;ret=0;pr_info("RK_MALI_BOOT_GUARD: native power preparation complete\n");}
done:
 device_unlock(target);return ret;
}
static const struct kernel_param_ops power_ops={.set=prepare_power_set};
module_param_cb(prepare_power,&power_ops,NULL,0200);
static int pulse_get(char *buffer,const struct kernel_param *kp)
{
    return scnprintf(buffer,PAGE_SIZE,"%d %d %d %d\n",pulses,restored,armed,atomic_read(&denied));
}
static const struct kernel_param_ops pulse_ops={.set=pulse_set,.get=pulse_get};
module_param_cb(arm,&pulse_ops,NULL,0600);

static int __init pulse_init(void)
{
    unsigned long lookup_addr,get,put,init,lock;
    unsigned long (*lookup)(const char *);
    struct device *dev;int ret;
    BUILD_BUG_ON(sizeof(struct module)!=896);
    BUILD_BUG_ON(offsetof(struct miscdevice,fops)!=0x10);
    BUILD_BUG_ON(offsetof(struct file_operations,open)!=0x70);
    anchor=(unsigned long)printk;lookup_addr=anchor+FIXTURE_LOOKUP_DELTA;
    if(reject_guard||!fingerprint(anchor,32,FIXTURE_PRINTK_FNV)||
       !fingerprint(lookup_addr,32,FIXTURE_LOOKUP_FNV))return -ENOEXEC;
    lookup=(void *)lookup_addr;
    if(lookup("printk")!=anchor||lookup("kallsyms_lookup_name")!=lookup_addr)return -ENOEXEC;
    get=lookup("kbase_device_get_list");put=lookup("kbase_device_put_list");init=lookup("kbase_csf_firmware_init");
    lock=lookup("misc_mtx");expected_open=init+GPU_OPEN_REL_DELTA;
    if(!get||!put||!init||lock!=anchor+FIXTURE_MUTEX_DELTA||
       !fingerprint(get,GPU_GET_SIZE,GPU_GET_FNV)||!fingerprint(put,GPU_PUT_SIZE,GPU_PUT_FNV)||
       !fingerprint(init,GPU_INIT_SIZE,GPU_INIT_FNV)||!fingerprint(expected_open,GPU_OPEN_SIZE,GPU_OPEN_FNV)||
       !fingerprint(anchor+FIXTURE_OPEN_DELTA,64,FIXTURE_OPEN_FNV)||
       !fingerprint(anchor+IDENTITY_IDINIT_DELTA,64,IDENTITY_IDINIT_FNV))return -ENOEXEC;
    if(!fingerprint(lookup("kbase_device_firmware_init_once"),ORIGINAL_FW_INIT_SIZE,ORIGINAL_FW_INIT_FNV))return -ENOEXEC;
    if(!fingerprint(anchor+ORIGINAL_POWER_ON_DELTA,ORIGINAL_POWER_ON_SIZE,ORIGINAL_POWER_ON_FNV) ||
       !fingerprint(anchor+ORIGINAL_POWER_CALLSITE_DELTA,16,ORIGINAL_POWER_CALLSITE_FNV))return -ENOEXEC;
    original_power_on=(void *)(anchor+ORIGINAL_POWER_ON_DELTA);
    get_list=(void *)get;put_list=(void *)put;misc_mutex=(void *)lock;
    ret=inspect_locked(&original_kbdev,&dev,&original_fops);
    if(ret)return ret;
    {
        unsigned long callback=0;
        if(copy_from_kernel_nofault(&callback,(void *)(original_kbdev+ORIGINAL_POWER_CALLBACK_OFFSET),8) ||
           callback!=(unsigned long)original_power_on){put_device(dev);return -ENOEXEC;}
    }
    target=dev;
    ret=resource_guard_init();if(ret){put_device(target);target=NULL;return ret;}
    ready=1;
    pr_info("RK_MALI_BOOT_GUARD: original identity verified; gate remains open\n");
    return 0;
}
static void __exit pulse_exit(void)
{
    WARN_ON(armed || hooks_installed || bridge_held_count);ready=0;if(progress_file)filp_close(progress_file,NULL);if(target)put_device(target);
}
module_init(pulse_init);
module_exit(pulse_exit);
MODULE_LICENSE("GPL");
MODULE_VERSION("0.2.0-resource-handover-rc1");
