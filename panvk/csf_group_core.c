// SPDX-License-Identifier: MIT
// Group the pinned RC3's three subqueues into three CSIs in one kernel CSG.
// This changes scheduling topology only; no commands, waits or completion
// signals are removed. Caller holds the mutex. Callback returns -errno.
#include "csf_group_core.h"
static void copy_bytes(void *out, const void *in, unsigned n) {
    uint8_t *d=out; const uint8_t *s=in; for(unsigned i=0;i<n;i++) d[i]=s[i];
}
static int shape(const uint8_t *a) {
    const uint8_t expected[32]={1,0,0,0,0,0,0,0,
      255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
      1,0,1,64,64,0,0,0};
    for(unsigned i=0;i<32;i++) if(a[i]!=expected[i]) return 0;
    return 1;
}
int csf_group_ioctl(struct csf_groups *s, uint32_t owner, csf_call call, void *ctx,
                    int fd, unsigned long req, void *arg) {
    if(req!=GROUP_CREATE && req!=QUEUE_BIND && req!=GROUP_TERM)
        return call(ctx,fd,req,arg);
    if(!arg) return -22;
    uint8_t *a=arg;
    if(req==GROUP_CREATE) {
        if(!shape(a)) return -22; // refuse an unrecognized creation shape
        struct csf_group *g=0,*empty=0;
        for(unsigned i=0;i<32;i++) {
            struct csf_group *p=&s->group[i];
            if(!p->used) {if(!empty) empty=p; continue;}
            if(p->fd==fd && p->owner==owner && p->created<3 && !p->closed) {g=p;break;}
        }
        if(g) { copy_bytes(a,g->output,8);g->created++;g->refs++;return 0; }
        if(!empty) return -28;
        uint8_t tmp[32];copy_bytes(tmp,a,32);tmp[24]=3;
        int rc=call(ctx,fd,req,tmp);if(rc) return rc;
        g=empty; g->fd=fd;g->owner=owner;g->handle=tmp[0];g->created=1;g->refs=1;
        g->bound=0;g->closed=0;g->used=1;copy_bytes(g->output,tmp,8);copy_bytes(a,tmp,32);return 0;
    }
    uint8_t handle=req==QUEUE_BIND?a[8]:a[0];
    struct csf_group *g=0;
    for(unsigned i=0;i<32;i++)
        if(s->group[i].used && s->group[i].fd==fd && s->group[i].handle==handle) {g=&s->group[i];break;}
    if(!g) return call(ctx,fd,req,arg);
    if(req==QUEUE_BIND) {
        if(a[9]!=0 || g->bound>=g->created || g->bound>=3 || g->closed) return -22;
        uint8_t tmp[16];copy_bytes(tmp,a,16);tmp[9]=g->bound;
        int rc=call(ctx,fd,req,tmp);if(rc) return rc;
        g->bound++;copy_bytes(a,tmp,16);return 0;
    }
    g->closed=1;
    if(g->refs>1) {g->refs--;return 0;}
    int rc=call(ctx,fd,req,arg);if(!rc) g->used=0;return rc;
}

void csf_group_forget_fd(struct csf_groups *s,int fd) {
 for(unsigned i=0;i<32;i++)if(s->group[i].used && s->group[i].fd==fd)s->group[i].used=0;
}
