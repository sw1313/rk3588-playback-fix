// SPDX-License-Identifier: GPL-2.0-only
/* Passive research observer: standard tracepoint registration, atomic counters
 * only. Does not enable vblank, replace DRM callbacks, register IRQs, touch MMIO
 * or expose a display control API. Unregister plus SRCU/RCU grace periods before
 * module exit. Events have pipe identity only, not DRM device identity.
 */
#include <linux/module.h>
#include <linux/tracepoint.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/delay.h>
#include "display_contract.h"

static bool reject_guard;
static int ready, matches;
static struct tracepoint *target;
static atomic_t events[4] = { ATOMIC_INIT(0), ATOMIC_INIT(0), ATOMIC_INIT(0), ATOMIC_INIT(0) };
static atomic_t other_events = ATOMIC_INIT(0);
module_param(reject_guard, bool, 0444);
module_param(ready, int, 0444);
module_param(matches, int, 0444);

static int counter_get(char *buffer, const struct kernel_param *parameter)
{
    return scnprintf(buffer, PAGE_SIZE, "%d", atomic_read(parameter->arg));
}
static const struct kernel_param_ops counter_ops = { .get = counter_get };
module_param_cb(pipe0_events, &counter_ops, &events[0], 0444);
module_param_cb(pipe1_events, &counter_ops, &events[1], 0444);
module_param_cb(pipe2_events, &counter_ops, &events[2], 0444);
module_param_cb(pipe3_events, &counter_ops, &events[3], 0444);
module_param_cb(other_events, &counter_ops, &other_events, 0444);

static bool fingerprint(unsigned long address, unsigned int count, u64 expected)
{
    unsigned char bytes[64];
    unsigned int i;
    u64 hash = 14695981039346656037ULL;
    if (count > sizeof(bytes) || copy_from_kernel_nofault(bytes, (void *)address, count))
        return false;
    for (i = 0; i < count; ++i)
        hash = (hash ^ bytes[i]) * 1099511628211ULL;
    return hash == expected;
}

static void find_tracepoint(struct tracepoint *tp, void *unused)
{
    unsigned long name;
    char buffer[32];
    if ((unsigned long)tp != (unsigned long)printk + DISPLAY_VBLANK_TP_DELTA)
        return;
    if (copy_from_kernel_nofault(&name, tp, sizeof(name)) || !name ||
        copy_from_kernel_nofault(buffer, (void *)name, sizeof(buffer)))
        return;
    buffer[sizeof(buffer) - 1] = '\0';
    if (strcmp(buffer, "drm_vblank_event"))
        return;
    target = tp;
    ++matches;
}

/* Original __traceiter and reference drm_trace.h both pass these five args.
 * No sleeping, allocation, logging, DRM calls or register access here.
 */
static void observe_vblank(void *unused, int pipe, unsigned int sequence,
                           ktime_t time, bool high_precision)
{
    if (pipe >= 0 && pipe < ARRAY_SIZE(events))
        atomic_inc(&events[pipe]);
    else
        atomic_inc(&other_events);
}

static int __init observer_init(void)
{
    unsigned int i;
    int ret;
    if (reject_guard)
        return -ENOEXEC;
    for (i = 0; i < ARRAY_SIZE(display_functions); ++i)
        if (!fingerprint((unsigned long)printk + display_functions[i].delta,
                         display_functions[i].bytes, display_functions[i].fnv))
            return -ENOEXEC;
    for_each_kernel_tracepoint(find_tracepoint, NULL);
    if (matches != 1) {
        target = NULL;
        return -ENODEV;
    }
    ret = tracepoint_probe_register(target, (void *)observe_vblank, NULL);
    if (ret) {
        target = NULL;
        return ret;
    }
    ready = 1;
    pr_info("RK_VBLANK_OBSERVER: registered passive atomic counters; no display writes; QMS not implemented\n");
    return 0;
}

static void __exit observer_exit(void)
{
    int ret;
    ready = 0;
    /* Removing a probe can allocate when other observers are registered. If
     * removal fails, never return and free executable callback memory while
     * the tracepoint may still reference it. Normal teardown takes one call;
     * failure leaves the module in GOING until native removal succeeds.
     */
    do {
        ret = tracepoint_probe_unregister(target, (void *)observe_vblank, NULL);
        if (ret)
            msleep(20);
    } while (ret);
    tracepoint_synchronize_unregister();
    target = NULL;
    pr_info("RK_VBLANK_OBSERVER: unregistered and synchronized; counts=%d,%d,%d,%d other=%d\n",
            atomic_read(&events[0]), atomic_read(&events[1]),
            atomic_read(&events[2]), atomic_read(&events[3]), atomic_read(&other_events));
}
module_init(observer_init);
module_exit(observer_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Exact-image passive DRM vblank research observer; no QMS implementation");
