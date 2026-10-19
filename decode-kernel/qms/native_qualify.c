// SPDX-License-Identifier: GPL-2.0-only
#include "native_binding.h"

int rkqms_binding_validate(struct rkqms_native_binding *b,const struct rkqms_session *s)
{
    struct rkqms_native_link *l;
    struct rkqms_registers *r;
    int ret;
    if (!b || !b->registers || !b->life) return -EINVAL;
    if (READ_ONCE(b->life->pm_blocked)) return -EAGAIN;
    l=b->link;r=b->registers;
    if (!s || !l || !l->qualified || !l->cookie || s->status.link_cookie!=l->cookie ||
        s->status.generation!=r->generation || s->status.connector!=l->baseline.connector ||
        s->status.crtc!=l->baseline.crtc || s->status.bits_per_component!=l->baseline.bpc ||
        s->status.colorspace!=l->baseline.colorspace || s->status.eotf!=l->baseline.eotf ||
        !s->status.tfr_mask || (s->status.tfr_mask&~l->sink.tfr_mask) ||
        s->status.width!=l->baseline.mode.width || s->status.height!=l->baseline.mode.height ||
        memcmp(&s->transaction.core.base,&l->baseline.mode,sizeof(r->base)) ||
        memcmp(&r->base,&l->baseline.mode,sizeof(r->base))) return -ESTALE;
    mutex_lock((void *)(r->qp+0xb48));
    ret=rkqms_lifecycle_check(&b->life->state,r->expected_epoch);
    mutex_unlock((void *)(r->qp+0xb48));
    if (ret) return ret;
    ret=rkqms_native_link_validate(l);
    if (ret) return ret;
    mutex_lock((void *)(r->qp+0xb48));
    ret=rkqms_lifecycle_check(&b->life->state,r->expected_epoch);
    mutex_unlock((void *)(r->qp+0xb48));return ret;
}
