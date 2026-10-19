/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef RKQMS_CONTROLLER_H
#define RKQMS_CONTROLLER_H
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include "session.h"
/* Binding operations are process-context, blocking and exact-image guarded.
 * frame: native locks + live link/HDR validation + carrier/window + finish.
 * recover: drain native work, disable ONLY owned VTEM, restore native timing,
 *          verify hardware/link. Returning zero is a hardware assertion.
 * Only the worker owns session. No operation may take request_lock. Foreign
 * callbacks invalidate via a binding-owned epoch, without blocking on this
 * worker. The publication spinlock is NEVER held across native operations.
 */
struct rkqms_controller_ops {
    void *context;
    int (*frame)(void *, struct rkqms_session *);
    int (*recover)(void *, struct rkqms_session *);
    /* Optional boot-bound owner. Called only while ordinary BASE/QUIESCENT,
     * never while processing a ticket. May replace session only after native
     * drain, verified clean hardware and a fresh physical qualification. */
    int (*refresh)(void *, struct rkqms_session *);
};
struct rkqms_controller {
    struct mutex request_lock;
    spinlock_t publication_lock;
    wait_queue_head_t wake;
    struct task_struct *worker;
    struct rkqms_session session;
    struct rkqms_controller_ops ops;
    struct rkqms_status published;
    struct rkqms_command inbox;
    u64 request_id, accepted_id, rejected_id, deadline_ns;
    int request_error;
    bool pending, restore;
    bool stopping, recovery_attempted;
    bool command_ready;
    int stop_error;
};
int rkqms_controller_start(struct rkqms_controller *, const struct rkqms_session *,
                           const struct rkqms_controller_ops *);
int rkqms_controller_status(struct rkqms_controller *, struct rkqms_status *);
int rkqms_controller_command(struct rkqms_controller *, struct rkqms_command *, int restore);
/* Does not unpublish native hooks or permit module unload on recovery failure.
 * Functional bindings remain pinned until reboot, independently of this task.
 */
int rkqms_controller_stop(struct rkqms_controller *);
#endif
