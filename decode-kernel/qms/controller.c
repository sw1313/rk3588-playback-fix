// SPDX-License-Identifier: GPL-2.0-only
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/jiffies.h>
#include <linux/string.h>
#include "controller.h"

static void publish(struct rkqms_controller *c)
{
    unsigned long flags;
    spin_lock_irqsave(&c->publication_lock,flags);
    c->published=c->session.status;
    spin_unlock_irqrestore(&c->publication_lock,flags);
}

static int recover(struct rkqms_controller *c)
{
    int ret;
    if (c->recovery_attempted) return c->stop_error ? c->stop_error : -EIO;
    c->recovery_attempted=true;
    ret=c->ops.recover(c->ops.context,&c->session);
    if (ret) {rkqms_session_fault(&c->session,ret);c->stop_error=ret;}
    else rkqms_session_quiesced(&c->session);
    return ret;
}

static int run(void *p)
{
    struct rkqms_controller *c=p;
    for (;;) {
        struct rkqms_command command;
        unsigned long flags;
        u64 id=0,deadline=0;
        int ret=0,restore=0;
        bool active;
        if (kthread_should_stop() || READ_ONCE(c->stopping)) {
            if (c->session.status.state!=RKQMS_QUIESCENT) c->stop_error=recover(c);
            publish(c);return c->stop_error;
        }
        spin_lock_irqsave(&c->publication_lock,flags);
        if (c->pending) {
            command=c->inbox;restore=c->restore;id=c->request_id;
            deadline=c->deadline_ns;c->pending=false;
        }
        spin_unlock_irqrestore(&c->publication_lock,flags);
        if (id) {
            ret=ktime_get_ns()>=deadline ? -ETIMEDOUT : rkqms_session_submit(&c->session,&command,restore);
            spin_lock_irqsave(&c->publication_lock,flags);
            if (ret) {c->rejected_id=id;c->request_error=ret;}
            else c->accepted_id=id;
            c->published=c->session.status;
            spin_unlock_irqrestore(&c->publication_lock,flags);
        }
        if (!id && c->ops.refresh &&
            (c->session.status.state==RKQMS_BASE || c->session.status.state==RKQMS_QUIESCENT)) {
            ret=c->ops.refresh(c->ops.context,&c->session);
            if (ret && ret!=-EAGAIN && ret!=-EOPNOTSUPP && ret!=-ENOLINK)
                rkqms_session_fault(&c->session,ret);
            if (!ret) {c->recovery_attempted=false;c->stop_error=0;}
        }
        active=c->session.status.state==RKQMS_CHANGING || c->session.status.state==RKQMS_ACTIVE;
        if (active) {
            /* Exactly one native carrier/window/real-vblank step, preserving
             * real fence waits. No nominal sleep cadence or catch-up frames.
             */
            ret=c->ops.frame(c->ops.context,&c->session);
            if (ret) rkqms_session_fault(&c->session,ret);
            /* A blocked native fence must not block the ioctl timeout or
             * make a late completion look timely. Only the worker owns
             * session; ioctl reads a separately published snapshot.
             */
            if (c->session.status.state==RKQMS_CHANGING ||
                (c->session.status.completed_serial==c->session.status.serial && c->session.status.serial)) {
                spin_lock_irqsave(&c->publication_lock,flags);
                deadline=c->accepted_id==c->request_id ? c->deadline_ns : 0;
                spin_unlock_irqrestore(&c->publication_lock,flags);
                if (deadline && ktime_get_ns()>=deadline) rkqms_session_fault(&c->session,-ETIMEDOUT);
            }
        }
        if (c->session.status.state==RKQMS_RECOVERY && !c->recovery_attempted) recover(c);
        spin_lock_irqsave(&c->publication_lock,flags);
        c->published=c->session.status;
        /* Finished deadlines must not expire during later stable per-frame
         * fractional-rate maintenance. A rejected new command must not alter
         * an earlier in-flight ticket's deadline.
         */
        if (c->session.status.state!=RKQMS_CHANGING && c->accepted_id==c->request_id) c->deadline_ns=0;
        spin_unlock_irqrestore(&c->publication_lock,flags);
        /* Constant timing is repeated by hardware. Commands wake immediately;
         * timeout is a read-only watchdog, never a synthetic vblank clock.
         * Transitions and fractional timing keep the real native-frame path.
         */
        if (rkqms_session_constant(&c->session) || !active || ret)
            wait_event_interruptible_timeout(c->wake,
                kthread_should_stop() || READ_ONCE(c->stopping) || READ_ONCE(c->pending),
                msecs_to_jiffies(100));
    }
}

int rkqms_controller_start(struct rkqms_controller *c,const struct rkqms_session *s,
                           const struct rkqms_controller_ops *ops)
{
    if (!c || !s || !ops || !ops->frame || !ops->recover ||
        s->status.proof!=RKQMS_PROOF_ALL || s->status.state!=RKQMS_BASE ||
        s->transaction.core.phase!=RKQMS_OFF || !s->status.generation ||
        !s->status.link_cookie || s->status.serial || s->status.completed_serial) return -EOPNOTSUPP;
    memset(c,0,sizeof(*c));mutex_init(&c->request_lock);spin_lock_init(&c->publication_lock);
    init_waitqueue_head(&c->wake);
    c->session=*s;c->published=s->status;c->ops=*ops;c->command_ready=true;
    c->worker=kthread_run(run,c,"rk_decode_qms");
    if (IS_ERR(c->worker)) {int ret=PTR_ERR(c->worker);c->worker=NULL;return ret;}
    return 0;
}

int rkqms_controller_status(struct rkqms_controller *c,struct rkqms_status *s)
{
    unsigned long flags;
    if (!c || !s) return -EINVAL;
    spin_lock_irqsave(&c->publication_lock,flags);*s=c->published;
    spin_unlock_irqrestore(&c->publication_lock,flags);return 0;
}

int rkqms_controller_command(struct rkqms_controller *c,struct rkqms_command *command,int restore)
{
    unsigned long flags;
    u64 id,deadline;
    int ret;
    if (!c || !command || command->timeout_ms<100 || command->timeout_ms>2000) return -EINVAL;
    if (!mutex_trylock(&c->request_lock)) return -EBUSY;
    deadline=ktime_get_ns()+(u64)command->timeout_ms*1000000ULL;
    spin_lock_irqsave(&c->publication_lock,flags);
    if (READ_ONCE(c->stopping) || !c->worker || !READ_ONCE(c->command_ready)) {ret=-ENODEV;goto done;}
    if (c->pending || c->published.state==RKQMS_CHANGING) {ret=-EBUSY;goto done;}
    if (c->request_id==~0ULL) {ret=-ESTALE;goto done;}
    id=++c->request_id;c->inbox=*command;c->restore=!!restore;
    c->deadline_ns=deadline;c->pending=true;
    wake_up(&c->wake);
    for (;;) {
        if (c->rejected_id==id) {ret=c->request_error;break;}
        if (c->accepted_id==id) {
            const struct rkqms_status *s=&c->published;
            if (s->generation!=command->generation || s->serial!=command->expected_serial+1) {ret=-ESTALE;break;}
            if (s->state==RKQMS_RECOVERY || s->state==RKQMS_DISCONNECTED || s->state==RKQMS_QUIESCENT) {
                ret=s->error ? s->error : -EIO;break;
            }
            if (s->completed_serial==s->serial) {ret=0;break;}
        }
        if (ktime_get_ns()>=deadline) {
            /* Cancel an unconsumed inbox. Accepted/in-flight work enforces
             * this same absolute deadline and recovers. No native fallback
             * is authorized by an ioctl timeout.
             */
            if (c->request_id==id) c->pending=false;
            ret=-ETIMEDOUT;break;
        }
        spin_unlock_irqrestore(&c->publication_lock,flags);msleep(1);
        spin_lock_irqsave(&c->publication_lock,flags);
    }
done:
    command->result=c->published;
    spin_unlock_irqrestore(&c->publication_lock,flags);
    mutex_unlock(&c->request_lock);return ret;
}

int rkqms_controller_stop(struct rkqms_controller *c)
{
    int ret;
    if (!c) return -EINVAL;
    WRITE_ONCE(c->stopping,true);
    wake_up(&c->wake);
    if (!c->worker) return c->stop_error;
    ret=kthread_stop(c->worker);c->worker=NULL;
    /* Native hooks still pinned on error; only reboot may reclaim them. */
    return ret;
}
