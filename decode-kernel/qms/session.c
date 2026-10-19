// SPDX-License-Identifier: GPL-2.0-only
#include "session.h"
#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

static int zero_words(const __u32 *p, unsigned int count)
{
    while (count--) if (*p++) return 0;
    return 1;
}

int rkqms_session_init(struct rkqms_session *s, const struct rkqms_mode *mode, rq_u8 vic,
                      const struct rkqms_caps *caps, const struct rkqms_io *io,
                      const struct rkqms_status *id)
{
    int ret;
    struct rkqms_state core;
    if (!s || !mode || !caps || !io || !id || id->abi != RKQMS_ABI ||
        id->bytes != sizeof(*id) || !id->link_cookie || !id->connector || !id->crtc ||
        id->proof != RKQMS_PROOF_ALL || !id->bits_per_component ||
        id->reserved0 || !zero_words(id->reserved, 3) ||
        !io->begin || !io->end || !io->write_packet || !io->update_packet_enable || !io->commit_timing)
        return RKQMS_ENOTSUP;
    ret = rkqms_init(&core, mode, vic, caps, id->generation);
    if (ret) return ret;
    memset(s, 0, sizeof(*s));
    s->transaction.core = core;
    s->transaction.io = *io;
    s->status.abi = RKQMS_ABI; s->status.bytes = sizeof(s->status);
    s->status.generation = id->generation; s->status.link_cookie = id->link_cookie;
    s->status.connector = id->connector; s->status.crtc = id->crtc; s->status.proof = id->proof;
    s->status.width = mode->width; s->status.height = mode->height;
    s->status.tfr_mask = caps->tfr_mask; s->status.base_tfr = core.base_tfr;
    s->status.effective_tfr = core.base_tfr; s->status.state = RKQMS_BASE;
    s->status.bits_per_component = id->bits_per_component;
    s->status.colorspace = id->colorspace; s->status.eotf = id->eotf;
    return 0;
}

static int constant_timing(const struct rkqms_session *s)
{
    const struct rkqms_state *c;
    const struct rkqms_timing *t;
    if (!s || s->transaction.needs_native_restore || s->status.error ||
        s->status.proof!=RKQMS_PROOF_ALL) return 0;
    c=&s->transaction.core;t=rkqms_timing(c->vic,c->current_tfr);
    return t && !t->bits && c->generation==s->status.generation &&
        s->status.effective_tfr==c->current_tfr && c->vtem_enabled==1 && c->packet_const==1 &&
        c->packet_tfr==c->current_tfr && c->current_vtotal==t->vtotal;
}

int rkqms_session_constant(const struct rkqms_session *s)
{
    return constant_timing(s) && s->status.state==RKQMS_ACTIVE &&
        s->transaction.core.phase==RKQMS_STABLE && !s->awaiting_latch &&
        !s->constant_reanchor && s->status.serial &&
        s->status.completed_serial==s->status.serial;
}

static int forward_observation(const struct rkqms_session *s,rq_u32 sequence,rq_u64 ns,int same)
{
    rq_u32 delta=sequence-s->status.sequence;
    if (!ns || !s->have_observation || delta>=0x80000000U) return RKQMS_EIO;
    if (!delta) return ns==s->status.observed_ns ? (same?0:RKQMS_EAGAIN) : RKQMS_EIO;
    return ns>s->status.observed_ns ? 0 : RKQMS_EIO;
}

int rkqms_session_latch_repeatable(const struct rkqms_session *s)
{
    const struct rkqms_state *c;
    const struct rkqms_timing *t;
    if (!s || s->transaction.needs_native_restore || s->transaction.last_error ||
        s->status.error || s->status.proof!=RKQMS_PROOF_ALL ||
        s->status.state!=RKQMS_CHANGING || !s->have_observation ||
        !s->awaiting_latch || s->constant_reanchor || !s->status.serial ||
        s->status.serial!=s->status.completed_serial+1 ||
        s->committed_sequence!=s->status.sequence) return 0;
    c=&s->transaction.core;t=rkqms_timing(c->vic,c->current_tfr);
    return t && !t->bits && c->phase==RKQMS_STABLE &&
        c->generation==s->status.generation && c->sequence_valid &&
        c->last_sequence==s->status.sequence && c->vtem_enabled==1 &&
        c->packet_const==1 && c->packet_tfr==c->current_tfr &&
        c->requested_tfr==c->current_tfr && c->current_vtotal==t->vtotal &&
        (c->caps.tfr_mask&(1U<<c->current_tfr));
}

int rkqms_session_latch_observe_verified(struct rkqms_session *s,rq_u64 generation,
                                        rq_u32 sequence,rq_u64 ns)
{
    int ret;
    if (!rkqms_session_latch_repeatable(s) || generation!=s->status.generation)
        return RKQMS_ESTALE;
    ret=forward_observation(s,sequence,ns,0);if(ret)return ret;
    if ((rq_u32)(sequence-s->status.sequence)<2) return RKQMS_EAGAIN;
    /* The prior native transaction already passed its exact +1 hardware
     * latch check. An unchanged integer output then repeats autonomously.
     * Observe this later real frame without replaying plans or writes. The
     * caller still requires the current native transaction's exact +1 finish
     * and identity checks before publishing this staged completion. */
    s->status.sequence=sequence;s->status.observed_ns=ns;
    s->transaction.core.last_sequence=sequence;++s->transaction.core.revision;
    s->status.completed_serial=s->status.serial;
    s->status.effective_tfr=s->transaction.core.current_tfr;
    s->status.state=RKQMS_ACTIVE;s->awaiting_latch=0;
    return 0;
}

int rkqms_session_repeatable(const struct rkqms_session *s)
{
    const struct rkqms_state *c;
    const struct rkqms_timing *t;
    if (!s || s->transaction.needs_native_restore || s->transaction.last_error ||
        s->status.error || s->status.proof!=RKQMS_PROOF_ALL ||
        s->status.state!=RKQMS_CHANGING || !s->have_observation ||
        s->awaiting_latch || s->constant_reanchor || !s->status.serial ||
        s->status.serial!=s->status.completed_serial+1) return 0;
    c=&s->transaction.core;t=rkqms_timing(c->vic,c->current_tfr);
    if (!t || t->bits || c->current_vtotal!=t->vtotal || !c->vtem_enabled ||
        c->packet_const>1 || !c->packet_tfr || c->packet_tfr>13 ||
        !(c->caps.tfr_mask&(1U<<c->packet_tfr)) ||
        c->generation!=s->status.generation || !c->sequence_valid ||
        c->last_sequence!=s->status.sequence) return 0;
    switch(c->phase) {
    case RKQMS_BASE_CONST: case RKQMS_BASE_VARIABLE: case RKQMS_TARGET_NOTICE:
    case RKQMS_APPLY: case RKQMS_CONFIRM: case RKQMS_RESTORE_NOTICE:
    case RKQMS_RESTORE_APPLY: case RKQMS_RESTORE_SETTLE:
    case RKQMS_RESTORE_CONST: case RKQMS_RESTORE_OFF:
        return 1;
    default: return 0;
    }
}

int rkqms_session_repeat_verified(struct rkqms_session *s,rq_u64 generation,rq_u32 sequence,rq_u64 ns)
{
    int ret;
    if (!rkqms_session_repeatable(s) || generation!=s->status.generation) return RKQMS_ESTALE;
    ret=forward_observation(s,sequence,ns,0);if(ret)return ret;
    if ((rq_u32)(sequence-s->status.sequence)<2) return RKQMS_EAGAIN;
    /* No phase/ticket/I/O update. The next action runs once at the actual
     * observed frame; extra repetitions only lengthen the current notice. */
    s->have_observation=0;s->transaction.core.sequence_valid=0;
    return 0;
}

int rkqms_session_hold_verified(struct rkqms_session *s,rq_u64 generation,rq_u32 sequence,rq_u64 ns)
{
    int ret;
    if (!rkqms_session_constant(s) || generation!=s->status.generation) return RKQMS_ESTALE;
    ret=forward_observation(s,sequence,ns,0);if(ret)return ret;
    /* Real observation only. No synthetic catch-up plan, I/O, phase advance
     * or ticket completion. Every intervening hardware frame is identical. */
    s->status.sequence=sequence;s->status.observed_ns=ns;
    s->transaction.core.last_sequence=sequence;++s->transaction.core.revision;
    return 0;
}

int rkqms_session_anchor_verified(struct rkqms_session *s,rq_u64 generation,rq_u32 sequence,rq_u64 ns)
{
    int ret;
    if (!s || !s->constant_reanchor || !constant_timing(s) ||
        generation!=s->status.generation || s->status.state!=RKQMS_CHANGING ||
        s->status.serial!=s->status.completed_serial+1 || s->awaiting_latch ||
        (s->transaction.core.phase!=RKQMS_BASE_VARIABLE &&
         s->transaction.core.phase!=RKQMS_RESTORE_NOTICE &&
         s->transaction.core.phase!=RKQMS_STABLE)) return RKQMS_ESTALE;
    ret=forward_observation(s,sequence,ns,1);if(ret)return ret;
    /* The FIRST native notice after verified autonomous hold establishes its
     * own real frame. All subsequent transition frames remain exactly +1. */
    s->have_observation=0;s->transaction.core.sequence_valid=0;s->constant_reanchor=0;
    return 0;
}

int rkqms_session_submit(struct rkqms_session *s, const struct rkqms_command *c, int restore)
{
    int ret,was_off,was_constant;
    if (!s || !c || c->abi != RKQMS_ABI || c->bytes != sizeof(*c) ||
        !zero_words(c->reserved, 4) || c->timeout_ms < 100 || c->timeout_ms > 2000 ||
        (restore ? c->target_tfr != 0 : (!c->target_tfr || c->target_tfr > 13)))
        return RKQMS_EINVAL;
    if (c->generation != s->status.generation || c->link_cookie != s->status.link_cookie ||
        c->connector != s->status.connector || c->crtc != s->status.crtc ||
        c->expected_serial != s->status.serial)
        return RKQMS_ESTALE;
    if (s->status.state == RKQMS_QUIESCENT) return RKQMS_ENOTSUP;
    if (s->status.state == RKQMS_RECOVERY || s->status.state == RKQMS_DISCONNECTED)
        return s->status.error ? s->status.error : RKQMS_EIO;
    if (s->status.state == RKQMS_CHANGING) return RKQMS_EBUSY;
    if (s->status.proof != RKQMS_PROOF_ALL) return RKQMS_ENOTSUP;
    if (s->status.serial == ~(rq_u64)0) return RKQMS_ESTALE;
    was_off=s->transaction.core.phase==RKQMS_OFF;
    was_constant=rkqms_session_constant(s);
    ret = rkqms_request(&s->transaction.core, (rq_u8)c->target_tfr,
                       c->generation, s->status.serial + 1);
    if (ret) return ret;
    if (was_off) s->have_observation=0;
    s->constant_reanchor=was_constant;
    ++s->status.serial;
    s->status.state = RKQMS_CHANGING;
    s->status.error = 0;
    /* Even no-op commands wait for an actual observation after acceptance.
     * During an interrupted transition, RESTORE must instead use the native
     * recovery path. Never shortcut the multi-frame state machine.
     */
    s->awaiting_latch = 0;
    return 0;
}

void rkqms_session_fault(struct rkqms_session *s, int error)
{
    if (!s) return;
    s->status.error = error < 0 ? error : RKQMS_EIO;
    s->status.state = RKQMS_RECOVERY;
    s->status.effective_tfr = 0;
    s->awaiting_latch = 0;
    s->constant_reanchor=0;
    s->transaction.needs_native_restore = 1;
    s->transaction.last_error = s->status.error;
    rkqms_invalidate(&s->transaction.core, s->status.generation);
}

int rkqms_session_frame(struct rkqms_session *s, rq_u64 generation, rq_u32 sequence, rq_u64 ns)
{
    int ret, latch;
    if (!s || generation != s->status.generation) return RKQMS_ESTALE;
    if (s->constant_reanchor) return RKQMS_ESTALE;
    if (s->status.state == RKQMS_QUIESCENT) return RKQMS_ENOTSUP;
    if (s->status.state == RKQMS_RECOVERY || s->status.state == RKQMS_DISCONNECTED)
        return s->status.error ? s->status.error : RKQMS_EIO;
    if (!ns || (s->have_observation && ns <= s->status.observed_ns) ||
        (s->have_observation && s->transaction.core.phase != RKQMS_OFF &&
         (rq_u32)(sequence - s->status.sequence) != 1)) {
        rkqms_session_fault(s, RKQMS_EIO);
        return RKQMS_EIO;
    }
    latch = s->awaiting_latch && (rq_u32)(sequence - s->committed_sequence) == 1;
    ret = rkqms_frame(&s->transaction, generation, sequence);
    if (ret && ret != RKQMS_EAGAIN) {
        rkqms_session_fault(s, ret);
        return ret;
    }
    s->status.sequence = sequence; s->status.observed_ns = ns; s->have_observation = 1;
    if (s->status.state != RKQMS_CHANGING) return 0;
    if (s->transaction.core.phase != RKQMS_STABLE && s->transaction.core.phase != RKQMS_OFF)
        return 0;
    if (latch) {
        s->status.completed_serial = s->status.serial;
        s->status.effective_tfr = s->transaction.core.current_tfr;
        s->status.state = s->transaction.core.phase == RKQMS_OFF ? RKQMS_BASE : RKQMS_ACTIVE;
        s->awaiting_latch = 0;
    } else {
        s->awaiting_latch = 1;
        s->committed_sequence = sequence;
    }
    return 0;
}

int rkqms_session_result(const struct rkqms_session *s, rq_u64 generation, rq_u64 serial)
{
    if (!s || generation != s->status.generation || serial != s->status.serial || !serial)
        return RKQMS_ESTALE;
    if (s->status.state == RKQMS_RECOVERY || s->status.state == RKQMS_DISCONNECTED)
        return s->status.error ? s->status.error : RKQMS_EIO;
    return s->status.completed_serial == serial ? 0 : RKQMS_EAGAIN;
}

void rkqms_session_disconnect(struct rkqms_session *s, rq_u64 next_generation)
{
    if (!s) return;
    rkqms_session_fault(s, RKQMS_ESTALE);
    s->status.generation = next_generation;
    s->status.state = RKQMS_DISCONNECTED;
    s->status.proof = 0;
    s->status.link_cookie = 0;
    rkqms_invalidate(&s->transaction.core, next_generation);
    /* Reconnection requires a new init after native restore + fresh EDID,
     * mode, HDR, link and power verification. Never reuse old serial/cookie.
     */
}

void rkqms_session_quiesced(struct rkqms_session *s)
{
    if (!s) return;
    rkqms_session_fault(s, RKQMS_ESTALE);
    /* A drained/cancelled request is not a successful rate switch. Start a
     * different epoch and clear its tickets, so old ioctl waiters fail stale.
     * Generation wrap to zero deliberately leaves an unusable status.
     */
    ++s->status.generation;
    s->status.serial = s->status.completed_serial = 0;
    rkqms_invalidate(&s->transaction.core, s->status.generation);
    s->status.state = RKQMS_QUIESCENT;
    s->status.error = 0; s->status.proof = 0; s->status.link_cookie = 0;
    s->status.effective_tfr = 0;
}
