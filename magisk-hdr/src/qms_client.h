/* SPDX-License-Identifier: MIT */
#ifndef RK_HDR_QMS_CLIENT_H
#define RK_HDR_QMS_CLIENT_H
#include <stdint.h>
#include <string.h>
#include "../../decode-kernel/qms/uapi.h"

/* A return of NATIVE is permission to invoke the unmodified factory modeset.
 * BLOCK means physical state is uncertain: report failure, do not race a
 * native modeset against an in-flight QMS command. This client never changes
 * native DRM modes, vsync periods, fences, bit depth or HDR metadata.
 */
enum { RK_QMS_NATIVE = 0, RK_QMS_DONE = 1, RK_QMS_BLOCK = -1 };
struct rk_qms_transport {
    void *context;
    int (*status)(void *, struct rkqms_status *);
    int (*command)(void *, int restore, struct rkqms_command *);
};
struct rk_qms_client {
    struct rkqms_status identity;
    uint64_t generation, link_cookie, completed_serial;
    uint32_t logical_config, target_tfr;
    int uncertain, logical_valid;
};
static inline int rk_qms_status_valid(const struct rkqms_status *s) {
    if (s->abi != RKQMS_ABI || s->bytes != sizeof(*s) || !s->generation ||
        s->state > RKQMS_QUIESCENT || s->reserved0 || s->reserved[0] ||
        s->reserved[1] || s->reserved[2]) return 0;
    if (s->state == RKQMS_QUIESCENT)
        return !s->proof && !s->link_cookie && !s->effective_tfr && !s->error &&
            s->completed_serial == s->serial;
    return s->connector && s->crtc && s->link_cookie && s->proof == RKQMS_PROOF_ALL &&
        s->width && s->height && s->base_tfr && s->base_tfr<=13 &&
        !(s->tfr_mask & ~0x3ffeU) && (s->tfr_mask & (1U << s->base_tfr)) &&
        (s->bits_per_component==8 || s->bits_per_component==10 ||
         s->bits_per_component==12 || s->bits_per_component==16);
}
static inline int rk_qms_same_link(const struct rkqms_status *a, const struct rkqms_status *b) {
    return a->generation==b->generation && a->link_cookie==b->link_cookie &&
        a->connector==b->connector && a->crtc==b->crtc && a->width==b->width && a->height==b->height &&
        a->bits_per_component==b->bits_per_component && a->colorspace==b->colorspace &&
        a->eotf==b->eotf && a->base_tfr==b->base_tfr && a->tfr_mask==b->tfr_mask;
}
static inline int rk_qms_native_safe(const struct rkqms_status *s) {
    return rk_qms_status_valid(s) && s->state == RKQMS_QUIESCENT;
}
static inline int rk_qms_clean(const struct rkqms_status *s) {
    return rk_qms_status_valid(s) && s->state == RKQMS_BASE && !s->error &&
        s->completed_serial == s->serial && s->effective_tfr == s->base_tfr;
}
static inline void rk_qms_command_init(struct rkqms_command *c, const struct rkqms_status *s, uint32_t tfr) {
    memset(c, 0, sizeof(*c)); c->abi = RKQMS_ABI; c->bytes = sizeof(*c);
    c->generation = s->generation; c->expected_serial = s->serial; c->link_cookie = s->link_cookie;
    c->connector = s->connector; c->crtc = s->crtc; c->target_tfr = tfr; c->timeout_ms = 1000;
}
static inline int rk_qms_reply(const struct rkqms_command *c, uint32_t state, uint32_t target) {
    const struct rkqms_status *s = &c->result;
    return rk_qms_status_valid(s) && s->generation == c->generation &&
        s->link_cookie == c->link_cookie && s->connector == c->connector && s->crtc == c->crtc &&
        c->expected_serial != UINT64_MAX && s->serial == c->expected_serial + 1 &&
        s->completed_serial == s->serial && s->state == state && !s->error &&
        s->effective_tfr == target && s->observed_ns;
}
static inline int rk_qms_restore(struct rk_qms_client *c, const struct rk_qms_transport *t) {
    struct rkqms_status s; struct rkqms_command cmd;
    memset(&s, 0, sizeof(s));
    if (t->status(t->context, &s) || !rk_qms_status_valid(&s)) return RK_QMS_BLOCK;
    if (rk_qms_clean(&s) || rk_qms_native_safe(&s)) {
        c->uncertain = c->logical_valid = 0;
        return RK_QMS_NATIVE;
    }
    /* An active backend is system state, even if this process restarted. */
    c->uncertain = 1; c->logical_valid = 0;
    rk_qms_command_init(&cmd, &s, 0);
    if (t->command(t->context, 1, &cmd) || !rk_qms_reply(&cmd, RKQMS_BASE, s.base_tfr) ||
        !rk_qms_same_link(&s, &cmd.result)) {
        /* A synchronous native recovery can change generation. Only a fresh,
         * independently queried clean state permits the factory fallback.
         */
        memset(&s, 0, sizeof(s));
        if (t->status(t->context, &s) || !(rk_qms_clean(&s) || rk_qms_native_safe(&s))) return RK_QMS_BLOCK;
    }
    c->uncertain = 0;
    return RK_QMS_NATIVE;
}
static inline uint32_t rk_qms_tfr(int32_t period) {
    static const uint32_t n[] = {0,24000,24,25,30000,30,48000,48,50,60000,60,100,120000,120};
    static const uint32_t d[] = {0,1001,1,1,1001,1,1001,1,1,1001,1,1,1001,1};
    uint32_t i, match = 0;
    if (period <= 0) return 0;
    for (i = 1; i <= 13; ++i) {
        int64_t delta = (int64_t)period * n[i] - (int64_t)1000000000 * d[i];
        /* Factory float->int period may round by a few ns. Never merge
         * 24/23.976, 48/47.952 or 60/59.94 (differences are thousands of ns).
         */
        if (delta >= -(int64_t)16 * n[i] && delta <= (int64_t)16 * n[i]) {
            if (match) return 0;
            match = i;
        }
    }
    return match;
}
static inline int rk_qms_switch(struct rk_qms_client *c, const struct rk_qms_transport *t,
                                 uint32_t id, uint32_t w, uint32_t h, int32_t period) {
    struct rkqms_status s; struct rkqms_command cmd;
    uint32_t tfr = rk_qms_tfr(period);
    memset(&s, 0, sizeof(s));
    if (t->status(t->context, &s)) return RK_QMS_BLOCK;
    if (!rk_qms_status_valid(&s)) return RK_QMS_BLOCK;
    if (rk_qms_native_safe(&s)) {
        c->uncertain = c->logical_valid = 0;
        return RK_QMS_NATIVE;
    }
    if (s.state != RKQMS_BASE) c->uncertain = 1;
    if (s.state == RKQMS_RECOVERY || s.state == RKQMS_DISCONNECTED || s.error)
        return RK_QMS_BLOCK;
    if (!tfr || w != s.width || h != s.height || !(s.tfr_mask & (1U << tfr)))
        return rk_qms_restore(c, t);
    if (rk_qms_clean(&s) && tfr == s.base_tfr) {
        c->uncertain = c->logical_valid = 0;
        return RK_QMS_NATIVE;
    }
    rk_qms_command_init(&cmd, &s, tfr);
    c->uncertain = 1; c->logical_valid = 0;
    if (t->command(t->context, 0, &cmd) || !rk_qms_reply(&cmd, RKQMS_ACTIVE, tfr) ||
        !rk_qms_same_link(&s, &cmd.result))
        return rk_qms_restore(c, t);
    c->generation = cmd.result.generation; c->link_cookie = cmd.result.link_cookie;
    c->completed_serial = cmd.result.completed_serial;
    c->identity = cmd.result;
    c->target_tfr = tfr; c->logical_config = id; c->logical_valid = 1;
    return RK_QMS_DONE;
}
static inline int rk_qms_active(struct rk_qms_client *c, const struct rk_qms_transport *t, uint32_t *id) {
    struct rkqms_status s;
    memset(&s, 0, sizeof(s));
    if (t->status(t->context, &s)) return RK_QMS_BLOCK;
    if (rk_qms_clean(&s) || rk_qms_native_safe(&s)) {c->uncertain = c->logical_valid = 0; return RK_QMS_NATIVE;}
    c->uncertain = 1;
    if (!rk_qms_status_valid(&s) || !c->logical_valid || s.state != RKQMS_ACTIVE ||
        s.error || !rk_qms_same_link(&s,&c->identity) || s.generation != c->generation || s.link_cookie != c->link_cookie ||
        s.completed_serial != c->completed_serial || s.serial != c->completed_serial ||
        s.effective_tfr != c->target_tfr || !s.observed_ns) return RK_QMS_BLOCK;
    *id = c->logical_config; return RK_QMS_DONE;
}
#endif
