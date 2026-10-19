// SPDX-License-Identifier: GPL-2.0-only
/* Exact-image E-DDC repair, scoped to the native QP HDMI adapter.
 * Equivalent fixes: segment << 7 and EXT_READ for segmented EDID transfers.
 * Ordinary EDID/SCDC/HDCP transfers still use the factory algorithm.
 * Publish under the I2C root bus lock; retain adapter/module until reboot.
 * No executable-code patches, EDID overrides, SCDC writes or mode changes.
 */
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/regmap.h>
#include <linux/uaccess.h>
#include "ddc_contract.h"

static struct i2c_adapter *adapter;
static const struct i2c_algorithm *factory;
static struct i2c_algorithm repaired;
static void *qp, *native_i2c;
static struct regmap *map;
static bool inspect_only;
static unsigned int online, extended_reads, read_errors;
module_param(inspect_only, bool, 0400);
module_param(online, uint, 0444);
module_param(extended_reads, uint, 0444);
module_param(read_errors, uint, 0444);

static bool fingerprint(unsigned long address, unsigned int size, u64 expected)
{
    u8 bytes[64]; unsigned int i,n; u64 hash=14695981039346656037ULL;
    while(size) {
        n=min_t(unsigned int,size,sizeof(bytes));
        if(copy_from_kernel_nofault(bytes,(void *)address,n)) return false;
        for(i=0;i<n;i++) hash=(hash^bytes[i])*1099511628211ULL;
        size-=n;address+=n;
    }
    return hash==expected;
}

static int bits(unsigned int reg, unsigned int mask, unsigned int value)
{
    return regmap_update_bits(map,reg,mask,value);
}

static int repaired_xfer(struct i2c_adapter *adap,struct i2c_msg *msgs,int num)
{
    struct mutex *lock=native_i2c+0x400;
    struct completion *cmp=native_i2c+0x420;
    u8 *stat=native_i2c+0x440, *slave=native_i2c+0x444;
    u8 *is_reg=native_i2c+0x445, *is_segment=native_i2c+0x446;
    unsigned int value,i; int ret=0;
    /* Fixed standard E-DDC form, not a generic replacement I2C stack. */
    if(num!=3 || msgs[0].addr!=0x30 || (msgs[0].flags&~I2C_M_DMA_SAFE) || msgs[0].len!=1 ||
       msgs[1].addr!=0x50 || (msgs[1].flags&~I2C_M_DMA_SAFE) || msgs[1].len!=1 ||
       msgs[2].addr!=0x50 || (msgs[2].flags&~I2C_M_DMA_SAFE)!=I2C_M_RD ||
       !msgs[2].len || msgs[2].len>128 || !msgs[0].buf ||
       !msgs[1].buf || !msgs[2].buf || !msgs[0].buf[0])
        return factory->master_xfer(adap,msgs,num);
    mutex_lock(lock);
    if(bits(0x3024,5,5) || bits(0xf4,0xfe0,0x50<<5) ||
       bits(0xf8,0x7f,0x30) || bits(0xf8,0x7f80,msgs[0].buf[0]<<7)) {
        ret=-EIO;goto out;
    }
    *slave=msgs[1].buf[0];*is_reg=1;*is_segment=1;
    for(i=0;i<msgs[2].len;i++) {
        reinit_completion(cmp);
        if(bits(0xf4,0xff000,(*slave)++<<12) || bits(0xf4,0x1e,0x10)) {
            ret=-EIO;break;
        }
        if(!wait_for_completion_timeout(cmp,msecs_to_jiffies(100))) {
            ret=-ETIMEDOUT;break;
        }
        if(READ_ONCE(*stat)&4) {ret=-EIO;break;}
        if(regmap_read(map,0x10c,&value)) {ret=-EIO;break;}
        msgs[2].buf[i]=value&255;
        if(bits(0xf4,0x1e,0)) {ret=-EIO;break;}
    }
out:
    *is_segment=0;
    if(bits(0xf4,0x1e,0) && !ret) ret=-EIO;
    if(ret) {regmap_write(map,0xec,1);++read_errors;}
    if(bits(0x3024,5,0) && !ret) ret=-EIO;
    if(!ret) ++extended_reads;
    mutex_unlock(lock);
    return ret?ret:num;
}

static int __init repair_init(void)
{
    unsigned long anchor=(unsigned long)printk,word;
    unsigned int i;int ret=-ENOEXEC;
    BUILD_BUG_ON(sizeof(struct module)!=896);
    BUILD_BUG_ON(offsetof(struct i2c_adapter,algo)!=0x10);
    BUILD_BUG_ON(offsetof(struct i2c_adapter,lock_ops)!=0x20);
    BUILD_BUG_ON(offsetof(struct i2c_adapter,dev)+offsetof(struct device,driver_data)!=0xe8);
    BUILD_BUG_ON(sizeof(struct i2c_algorithm)!=40);
    BUILD_BUG_ON(offsetof(struct completion,done)!=0);
    BUILD_BUG_ON(sizeof(struct completion)!=32);
    for(i=0;i<ARRAY_SIZE(ddc_functions);i++)
        if(!fingerprint(anchor+ddc_functions[i].delta,ddc_functions[i].size,ddc_functions[i].hash)) {
            pr_info("RK_DECODE_DDC: exact-image code check rejected index=%u\n",i);return -ENOEXEC;
        }
    adapter=i2c_get_adapter(10);
    if(!adapter) return -ENODEV;
    if(!adapter->dev.parent || strcmp(dev_name(adapter->dev.parent),"fde80000.hdmi")) goto fail;
    factory=READ_ONCE(adapter->algo);
    if((unsigned long)factory!=anchor+DDC_ALGO_DELTA) goto fail;
    for(i=0;i<5;i++) {
        unsigned long expected=ddc_algo_slots[i]?anchor+ddc_algo_slots[i]:0;
        if(copy_from_kernel_nofault(&word,(void *)factory+i*8,8) || word!=expected) goto fail;
    }
    qp=i2c_get_adapdata(adapter);
    if(!qp || copy_from_kernel_nofault(&native_i2c,qp+0x830,8) || !native_i2c ||
       copy_from_kernel_nofault(&map,qp+0xbe0,8) || !map ||
       native_i2c!=(void *)adapter) goto fail;
    /* Publication changes only this adapter's data pointer. The factory
     * algorithm remains immutable and ordinary transfers delegate to it. */
    if(inspect_only) {online=2;i2c_put_adapter(adapter);adapter=NULL;return 0;}
    memcpy(&repaired,factory,sizeof(repaired));repaired.master_xfer=repaired_xfer;
    i2c_lock_bus(adapter,I2C_LOCK_ROOT_ADAPTER);
    if(READ_ONCE(adapter->algo)!=factory) {
        i2c_unlock_bus(adapter,I2C_LOCK_ROOT_ADAPTER);goto fail;
    }
    __module_get(THIS_MODULE);
    smp_store_release(&adapter->algo,&repaired);
    online=1;
    i2c_unlock_bus(adapter,I2C_LOCK_ROOT_ADAPTER);
    pr_info("RK_DECODE_DDC: E-DDC segment shift/EXT_READ repaired; reboot-only rollback; no EDID override\n");
    return 0;
fail:
    i2c_put_adapter(adapter);adapter=NULL;return ret;
}
static void __exit repair_exit(void) { }
module_init(repair_init);
module_exit(repair_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Exact-image Decode Stack native HDMI E-DDC repair");
