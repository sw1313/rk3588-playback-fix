// SPDX-License-Identifier: GPL-2.0-only
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/compat.h>
#include <linux/uaccess.h>
#include "endpoint.h"

static DEFINE_MUTEX(endpoint_lock);
static struct rkqms_controller *endpoint_controller;
static bool endpoint_online;

static long endpoint_ioctl(struct file *file,unsigned int request,unsigned long argument)
{
    void __user *user=(void __user *)argument;
    struct rkqms_controller *c;
    long ret;
    (void)file;
    /* The native module's file_operations.owner holds the module reference.
     * This mutex drains existing calls before the controller can be stopped.
     * GET_STATUS and bounded commands never acquire native modeset locks.
     */
    if (!mutex_trylock(&endpoint_lock)) return -EBUSY;
    c=endpoint_controller;
    if (!endpoint_online || !c) {ret=-ENODEV;goto out;}
    if (request==RKQMS_GET_STATUS) {
        struct rkqms_status status={0};
        ret=rkqms_controller_status(c,&status);
        if (!ret && copy_to_user(user,&status,sizeof(status))) ret=-EFAULT;
    } else if (request==RKQMS_SWITCH || request==RKQMS_RESTORE) {
        struct rkqms_command command;
        if (copy_from_user(&command,user,sizeof(command))) {ret=-EFAULT;goto out;}
        /* Result is output-only; discard user bytes before any callback.
         * The session checks ABI, size, identity, serial and reserved input.
         */
        memset(&command.result,0,sizeof(command.result));
        ret=rkqms_controller_command(c,&command,request==RKQMS_RESTORE);
        if (copy_to_user(user,&command,sizeof(command))) ret=-EFAULT;
    } else ret=-ENOTTY;
out:
    mutex_unlock(&endpoint_lock);return ret;
}

#ifdef CONFIG_COMPAT
static long endpoint_compat_ioctl(struct file *file,unsigned int request,unsigned long argument)
{
    /* UAPI has identical fixed layout. Do not reinterpret 32-bit pointers as
     * kernel objects or change an ioctl's encoded size/direction.
     */
    return endpoint_ioctl(file,request,(unsigned long)compat_ptr(argument));
}
#endif

static const struct file_operations endpoint_fops={
    .owner=THIS_MODULE,
    .unlocked_ioctl=endpoint_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl=endpoint_compat_ioctl,
#endif
};
static struct miscdevice endpoint_device={
    .minor=MISC_DYNAMIC_MINOR,.name="rk_decode_qms",.fops=&endpoint_fops,.mode=0600,
};

int rkqms_endpoint_register(struct rkqms_controller *c)
{
    struct rkqms_status status;
    int ret;
    /* Native misc_register/open/vfs_ioctl offsets independently audited in
     * the pinned Image. No dereference of a pristine-header struct file.
     */
    BUILD_BUG_ON(offsetof(struct file_operations,owner)!=0);
    BUILD_BUG_ON(offsetof(struct file_operations,unlocked_ioctl)!=0x50);
    BUILD_BUG_ON(offsetof(struct file_operations,compat_ioctl)!=0x58);
    BUILD_BUG_ON(offsetof(struct file_operations,open)!=0x70);
    BUILD_BUG_ON(offsetof(struct miscdevice,fops)!=0x10);
    BUILD_BUG_ON(offsetof(struct miscdevice,list)!=0x18);
    BUILD_BUG_ON(offsetof(struct miscdevice,this_device)!=0x30);
    BUILD_BUG_ON(offsetof(struct miscdevice,mode)!=0x48);
    if (!c || !c->worker || rkqms_controller_status(c,&status) ||
        status.proof!=RKQMS_PROOF_ALL || status.state!=RKQMS_BASE ||
        !status.generation || !status.link_cookie) return -EOPNOTSUPP;
    mutex_lock(&endpoint_lock);
    if (endpoint_controller) {ret=-EBUSY;goto out;}
    endpoint_controller=c;
    ret=misc_register(&endpoint_device);
    if (ret) endpoint_controller=NULL;
    else endpoint_online=true;
out:
    mutex_unlock(&endpoint_lock);return ret;
}

void rkqms_endpoint_unregister(void)
{
    bool registered;
    mutex_lock(&endpoint_lock);
    registered=endpoint_online;endpoint_online=false;endpoint_controller=NULL;
    mutex_unlock(&endpoint_lock);
    if (registered) misc_deregister(&endpoint_device);
}
