// SPDX-License-Identifier: GPL-2.0-only
/* Research only. Default is object inspection; RKQMS_CARRIER additionally
 * permits explicitly selected scoped commits, original-value register echo
 * and baseline factory recovery. No QMS API or hardware activation.
 * Device references and native locks are held only during initialization.
 */
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include "display_contract.h"

struct drm_device;
struct drm_connector;
struct drm_crtc;
/* Native begin/next/end access exactly these two words in the pinned image. */
struct drm_connector_list_iter {
    struct drm_device *dev;
    struct drm_connector *connector;
};
extern void drm_connector_list_iter_begin(struct drm_device *, struct drm_connector_list_iter *);
extern struct drm_connector *drm_connector_list_iter_next(struct drm_connector_list_iter *);
extern void drm_connector_list_iter_end(struct drm_connector_list_iter *);
extern struct drm_crtc *drm_crtc_from_index(struct drm_device *, int);

static bool reject_guard;
module_param(reject_guard, bool, 0444);
static int ready, stage, connectors, crtcs, tables_checked, qp_connector_found;
static int crtc_funcs_offset = -1, crtc_helper_offset = -1;
static int connector_funcs_offset = -1, connector_helper_offset = -1;
static int bridge_funcs_offset = -1, mode_config_funcs_offset = -1;
module_param(ready, int, 0444);
module_param(stage, int, 0444);
module_param(connectors, int, 0444);
module_param(crtcs, int, 0444);
module_param(tables_checked, int, 0444);
module_param(qp_connector_found, int, 0444);
module_param(crtc_funcs_offset, int, 0444);
module_param(crtc_helper_offset, int, 0444);
module_param(connector_funcs_offset, int, 0444);
module_param(connector_helper_offset, int, 0444);
module_param(bridge_funcs_offset, int, 0444);
module_param(mode_config_funcs_offset, int, 0444);

static unsigned long anchor;
static bool read_word(unsigned long p, unsigned long *value)
{
    return p && !copy_from_kernel_nofault(value, (void *)p, sizeof(*value));
}

static bool fingerprint(unsigned long p, unsigned int count, u64 expected)
{
    unsigned char bytes[64];
    u64 value = 14695981039346656037ULL;
    unsigned int i;
    if (count > sizeof(bytes) || copy_from_kernel_nofault(bytes, (void *)p, count))
        return false;
    for (i = 0; i < count; ++i)
        value = (value ^ bytes[i]) * 1099511628211ULL;
    return value == expected;
}

static bool table_matches(unsigned int index)
{
    const struct display_table_contract *c = &display_tables[index];
    unsigned int i;
    for (i = 0; i < c->words; ++i) {
        unsigned long actual, expected = c->slots[i];
        if (c->relative_mask & (1ULL << i))
            expected = anchor + (long)c->slots[i];
        if (!read_word(anchor + c->delta + i * 8, &actual) || actual != expected)
            return false;
    }
    return true;
}

/* Read only within a separately verified allocation's conservative prefix.
 * A table found in the prefix is a layout observation, not permission to hook.
 */
static int find_table(unsigned long object, unsigned int bytes, unsigned int table)
{
    unsigned int offset;
    unsigned long word, expected = anchor + display_tables[table].delta;
    int found = -1;
    for (offset = 0; offset + 8 <= bytes; offset += 8) {
        if (!read_word(object + offset, &word))
            return -EFAULT;
        if (word == expected) {
            if (found >= 0)
                return -EEXIST;
            found = offset;
        }
    }
    return found >= 0 ? found : -ENOENT;
}

static int match_name(struct device *dev, const void *name)
{
    return !strcmp(dev_name(dev), name);
}

#ifdef RKQMS_INSPECT_ABI
#include "qms_abi.h"
#endif
#ifdef RKQMS_LIVE_LINK
#include "qms_link.h"
#endif
#ifdef RKQMS_OWNER
#include "owner.h"
#endif

static int __init display_probe_init(void)
{
    static const char * const names[] = {
        "display-subsystem", "fde80000.hdmi", "fdd90000.vop"
#if defined(RKQMS_LIVE_LINK) || defined(RKQMS_OWNER)
        ,"fed60000.hdmiphy"
#endif
    };
    struct device *devices[ARRAY_SIZE(names)] = { NULL };
    struct drm_connector_list_iter iter;
    struct drm_connector *connector;
    unsigned long (*lookup)(const char *);
    unsigned long drm, hdmi, vop, qp, parent, word;
    unsigned int i, locked = 0;
    int ret = -ENOEXEC, offset;

    BUILD_BUG_ON(sizeof(struct module) != 896);
    BUILD_BUG_ON(offsetof(struct device, driver_data) != 0x78);
    BUILD_BUG_ON(sizeof(iter) != 16);
    if (reject_guard)
        return -ENOEXEC;
    stage = 1;
    anchor = (unsigned long)printk;
    for (i = 0; i < ARRAY_SIZE(display_functions); ++i)
        if (!fingerprint(anchor + display_functions[i].delta,
                         display_functions[i].bytes, display_functions[i].fnv) &&
            !(display_functions[i].jump_label_alternate &&
              fingerprint(anchor + display_functions[i].delta, display_functions[i].bytes,
                          display_functions[i].jump_label_alternate)))
            {
                u32 word;unsigned int n;
                pr_info("RK_DISPLAY_PROBE: code fingerprint rejected index=%u\n", i);
                for(n=0;n<display_functions[i].bytes;n+=4)
                    if(!copy_from_kernel_nofault(&word,(void *)(anchor+display_functions[i].delta+n),4))
                        pr_info("RK_DISPLAY_PROBE: rejected chunk offset=%u word=%08x\n",n,word);
                return -ENOEXEC;
            }
    lookup = (void *)(anchor + DISPLAY_LOOKUP_DELTA);
    if (lookup("printk") != anchor ||
        lookup("kallsyms_lookup_name") != (unsigned long)lookup)
        return -ENOEXEC;
    stage = 2;
    for (i = 0; i < ARRAY_SIZE(display_tables); ++i) {
        if (!table_matches(i))
            { pr_info("RK_DISPLAY_PROBE: callback table rejected index=%u\n", i); return -ENOEXEC; }
        ++tables_checked;
    }
    stage = 3;
    for (i = 0; i < ARRAY_SIZE(devices); ++i) {
        devices[i] = bus_find_device(&platform_bus_type, NULL, names[i], match_name);
        if (!devices[i]) { ret = -ENODEV; goto out; }
        if (!device_trylock(devices[i])) { ret = -EBUSY; goto out; }
        ++locked;
    }
    stage = 4;
    drm = (unsigned long)dev_get_drvdata(devices[0]);
    hdmi = (unsigned long)dev_get_drvdata(devices[1]);
    vop = (unsigned long)dev_get_drvdata(devices[2]);
    if (!drm || !hdmi || !vop) { ret = -ENODEV; goto out; }
    if (!read_word(drm + 0x18, &parent) || parent != (unsigned long)devices[0] ||
        !read_word(hdmi, &parent) || parent != (unsigned long)devices[1] ||
        !read_word(hdmi + 0x90, &word) || word != drm ||
        !read_word(hdmi + 0x100, &qp) || !qp ||
        !read_word(qp + 0x828, &parent) || parent != (unsigned long)devices[1]) {
        ret = -ESTALE; goto out;
    }
    /* Parent identity and original bind stores are verified before calling
     * native DRM iteration. Never reinterpret a guessed private structure.
     */
    stage = 5;
    if (find_table(drm, 0x80, DISPLAY_DRM_DRIVER) < 0) {
        ret = -ESTALE; goto out;
    }
    stage = 6;
    /* Original drm_dev_alloc requests 0x640 bytes, not generic DRM headers. */
    mode_config_funcs_offset = find_table(drm, 0x640, DISPLAY_MODE_CONFIG);
    connector_funcs_offset = find_table(qp, 0x700, DISPLAY_QP_CONNECTOR);
    connector_helper_offset = find_table(qp, 0x700, DISPLAY_QP_CONNECTOR_HELPER);
    bridge_funcs_offset = find_table(qp, 0xc78, DISPLAY_QP_BRIDGE);
    if (mode_config_funcs_offset < 0 || connector_funcs_offset < 0 ||
        connector_helper_offset < 0 || bridge_funcs_offset < 0) {
        ret = -ESTALE; goto out;
    }

    stage = 7;
    drm_connector_list_iter_begin((void *)drm, &iter);
    while ((connector = drm_connector_list_iter_next(&iter))) {
        if (++connectors > 16) { ret = -E2BIG; break; }
        if ((unsigned long)connector == qp)
            ++qp_connector_found;
    }
    drm_connector_list_iter_end(&iter);
    if (connectors > 16 || qp_connector_found != 1) {
        ret = -ESTALE; goto out;
    }
    stage = 8;
    for (i = 0; i < 4; ++i) {
        struct drm_crtc *crtc = drm_crtc_from_index((void *)drm, i);
        if (!crtc) { ret = -ESTALE; goto out; }
        offset = find_table((unsigned long)crtc, 0x300, DISPLAY_CRTC_FUNCS);
        if (offset < 0 || (i && offset != crtc_funcs_offset)) {
            ret = -ESTALE; goto out;
        }
        crtc_funcs_offset = offset;
        offset = find_table((unsigned long)crtc, 0x300, DISPLAY_CRTC_HELPER);
        if (offset < 0 || (i && offset != crtc_helper_offset)) {
            ret = -ESTALE; goto out;
        }
        crtc_helper_offset = offset;
        ++crtcs;
    }
    stage = 9;
#ifdef RKQMS_OWNER
#ifdef RKQMS_OWNER_INSPECT
    ret=rkqms_owner_inspect(anchor,devices,drm,hdmi,qp,vop);
#else
    ret=rkqms_owner_start(anchor,devices,drm,hdmi,qp,vop);
#endif
    if(ret) goto out;
    ready=1;ret=0;goto out;
#endif
#ifdef RKQMS_LIVE_LINK
    ret=inspect_live_link(drm,hdmi,qp,vop,devices[3]);
    if (ret) goto out;
#endif
#ifdef RKQMS_INSPECT_ABI
    ret = inspect_qms_abi(drm, qp, vop, devices[1], devices[2]);
    if (ret) goto out;
#endif
    for (i = 0; i < ARRAY_SIZE(display_tables); ++i)
        if (!table_matches(i)) { ret = -ESTALE; goto out; }
    ready = 1;
    ret = 0;
#ifdef RKQMS_CARRIER
    pr_info("RK_DISPLAY_PROBE: scoped native carrier/recovery probe passed; connectors=%d crtcs=%d; no QMS activation or endpoint\n",
#else
    pr_info("RK_DISPLAY_PROBE: read-only object identity passed; connectors=%d crtcs=%d; no callbacks/properties/IRQs/MMIO changed; QMS not implemented\n",
#endif
            connectors, crtcs);
out:
    while (locked)
        device_unlock(devices[--locked]);
    for (i = 0; i < ARRAY_SIZE(devices); ++i)
#ifdef RKQMS_OWNER
        if (!rkqms_owner_published() && devices[i])
#else
        if (devices[i])
#endif
            put_device(devices[i]);
    if (ret)
        pr_info("RK_DISPLAY_PROBE: identity gate rejected stage=%d (%d); no QMS timing changes\n", stage, ret);
    return ret;
}

static void __exit display_probe_exit(void)
{
#ifdef RKQMS_OWNER
    /* Published owners have an explicit native module reference, so normal
     * delete_module refuses them. Reboot is the supported rollback boundary. */
    if(rkqms_owner_published()) return;
#endif
    ready = 0;
    pr_info("RK_DISPLAY_PROBE: unloaded; no retained device pointers or hooks\n");
}
module_init(display_probe_init);
module_exit(display_probe_exit);
MODULE_LICENSE("GPL");
#ifdef RKQMS_OWNER
#ifdef RKQMS_OWNER_SYNC_INSPECT
MODULE_DESCRIPTION("Cold factory clock-sync trial; native modeset possible; no QMS publication");
#else
#ifdef RKQMS_OWNER_INSPECT
MODULE_DESCRIPTION("Read-only Decode owner preparation diagnostic; no publication or QMS");
#else
MODULE_DESCRIPTION("Decode Stack exact-image cold QMS candidate; reboot-only rollback");
#endif
#endif
#else
MODULE_DESCRIPTION("Exact-image read-only DRM extension feasibility probe; no QMS implementation");
#endif
