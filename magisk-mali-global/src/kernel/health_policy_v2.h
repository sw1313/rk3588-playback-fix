#ifndef RK_HEALTH_POLICY_V2_H
#define RK_HEALTH_POLICY_V2_H
#include <stdint.h>
#define HEALTH_ALL 255u
enum verdict { HEALTH_WAIT, HEALTH_READY, HEALTH_EXPIRED };
struct health_policy { uint64_t deadline, since, last; unsigned consecutive; };
static int transition_mark_valid(uint64_t now, uint64_t ack) {
    return ack && ack <= now && now-ack <= 150000;
}
static enum verdict health_sample(struct health_policy *p, uint64_t now, unsigned bits) {
    if (bits != HEALTH_ALL || (p->consecutive && (now < p->last || now-p->last > 8000))) {
        p->consecutive=0; p->since=0;
    }
    if (bits==HEALTH_ALL) {
        if (!p->consecutive) p->since=now;
        p->consecutive++;
    }
    p->last=now;
    if (now>=p->deadline) return HEALTH_EXPIRED;
    if (p->consecutive>=10 && now-p->since>=30000) return HEALTH_READY;
    return HEALTH_WAIT;
}
#endif
