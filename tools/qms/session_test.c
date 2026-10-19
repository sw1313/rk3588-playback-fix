// SPDX-License-Identifier: GPL-2.0-only
/* Executes real session/core/client code with injected I/O. No native backend,
 * ioctl registration, device, timings or hardware proof is supplied here.
 */
#include "session.h"
#include "edid.h"
#include "../../magisk-hdr/src/qms_client.h"
#include <stddef.h>
#ifdef RKQMS_PRIVATE_EDID
#include "private_edid.h"
#endif
_Static_assert(sizeof(struct rkqms_status) == 120, "fixed UAPI status size");
_Static_assert(sizeof(struct rkqms_command) == 184, "fixed UAPI command size");
_Static_assert(offsetof(struct rkqms_status, link_cookie) == 32, "fixed aligned cookie");
_Static_assert(offsetof(struct rkqms_command, result) == 64, "fixed result offset");
void *memset(void *p,int v,size_t n){unsigned char *b=p;while(n--)*b++=(unsigned char)v;return p;}
void *memcpy(void *p,const void *q,size_t n){unsigned char *a=p;const unsigned char *b=q;while(n--)*a++=*b++;return p;}
int memcmp(const void *p,const void *q,size_t n){const unsigned char *a=p,*b=q;while(n--){if(*a!=*b)return *a<*b?-1:1;++a;++b;}return 0;}
#define CHECK(x) do{if(!(x))return __LINE__;}while(0)
struct {unsigned int gates, lifecycle, frontend, rate_cases, private_edid;} test_results;
struct fixture {
    struct rkqms_session s;
    rq_u32 sequence, io_calls, io_fail, ends;
    rq_u64 ns;
    int status_fail, command_fail, forge, commands, native_recovery;
};
static int op(struct fixture *f){return ++f->io_calls==f->io_fail?-5:0;}
static int begin(void *p,rq_u64 g,rq_u32 seq){(void)g;(void)seq;return op(p);}
static int write_packet(void *p,rq_u32 offset,rq_u32 value){(void)offset;(void)value;return op(p);}
static int enable(void *p,rq_u32 mask,rq_u32 value){(void)mask;(void)value;return op(p);}
static int timing(void *p,rq_u16 total,rq_u16 vfp){(void)total;(void)vfp;return op(p);}
static void end(void *p){++((struct fixture*)p)->ends;}
static int init(struct fixture *f,rq_u32 proof,rq_u64 generation){
    struct rkqms_mode m={148500,5,1920,1080,2200,1125,1084,1089};
    struct rkqms_caps caps={0x7fe,1,1};
    struct rkqms_io io={f,begin,write_packet,enable,timing,end};
    struct rkqms_status id={0};
    memset(f,0,sizeof(*f));
    id.abi=RKQMS_ABI;id.bytes=sizeof(id);id.connector=11;id.crtc=12;
    id.proof=proof;id.generation=generation;id.link_cookie=123;
    id.bits_per_component=10;id.colorspace=9;id.eotf=2;
    return rkqms_session_init(&f->s,&m,16,&caps,&io,&id);
}
static int frame(struct fixture *f){int e;f->ns+=16666667;++f->sequence;
    if(f->s.constant_reanchor){e=rkqms_session_anchor_verified(&f->s,f->s.status.generation,f->sequence,f->ns);if(e)return e;}
    return rkqms_session_frame(&f->s,f->s.status.generation,f->sequence,f->ns);}
static struct rkqms_command command(struct fixture *f,rq_u32 tfr){
    struct rkqms_command c;rk_qms_command_init(&c,&f->s.status,tfr);return c;
}
static int finish(struct fixture *f){unsigned int i;for(i=0;i<20;++i){int e=frame(f);if(e)return e;if(!rkqms_session_result(&f->s,f->s.status.generation,f->s.status.serial))return 0;}return -110;}
static int status(void *p,struct rkqms_status *s){struct fixture *f=p;if(f->status_fail)return -5;*s=f->s.status;return 0;}
static int send(void *p,int restore,struct rkqms_command *c){
    struct fixture *f=p;int e;++f->commands;
    if(f->command_fail)return -5;
    if(f->native_recovery){rkqms_session_quiesced(&f->s);return -116;}
    e=rkqms_session_submit(&f->s,c,restore);if(e)return e;
    e=finish(f);c->result=f->s.status;
    switch(f->forge){
    case 1:--c->result.completed_serial;break;
    case 2:++c->result.generation;break;
    case 3:++c->result.link_cookie;break;
    case 4:c->result.effective_tfr=10;break;
    case 5:c->result.state=RKQMS_CHANGING;break;
    case 6:c->result.observed_ns=0;break;
    case 7:c->result.reserved[1]=1;break;
    case 8:++c->result.crtc;break;
    case 9:c->result.proof&=~RKQMS_PROOF_VSYNC;break;
    case 10:c->result.bits_per_component=8;break;
    case 11:c->result.eotf=0;break;
    case 12:c->result.width=1280;break;
    }
    return e;
}
static int gates(void){
    struct fixture f;struct rkqms_command c,good;struct rkqms_session before;
    unsigned int i;
    for(i=0;i<6;++i){CHECK(init(&f,RKQMS_PROOF_ALL&~(1U<<i),99)==RKQMS_ENOTSUP);++test_results.gates;}
    CHECK(init(&f,RKQMS_PROOF_ALL,0)==RKQMS_ENOTSUP);++test_results.gates;
    CHECK(!init(&f,RKQMS_PROOF_ALL,99));good=command(&f,2);before=f.s;
    for(i=0;i<12;++i){c=good;switch(i){
        case 0:++c.abi;break;case 1:--c.bytes;break;case 2:c.reserved[3]=1;break;
        case 3:++c.generation;break;case 4:++c.link_cookie;break;
        case 5:++c.expected_serial;break;case 6:++c.connector;break;case 7:++c.crtc;break;
        case 8:c.timeout_ms=99;break;case 9:c.timeout_ms=2001;break;
        case 10:c.target_tfr=256;break;case 11:c.target_tfr=0;break;
    }CHECK(rkqms_session_submit(&f.s,&c,0)<0);CHECK(!memcmp(&before,&f.s,sizeof(before)));++test_results.gates;}
    c=good;c.target_tfr=13;CHECK(rkqms_session_submit(&f.s,&c,0)==RKQMS_ENOTSUP);++test_results.gates;
    CHECK(rkqms_session_submit(&f.s,&good,1)==RKQMS_EINVAL);++test_results.gates;
    CHECK(!rkqms_session_submit(&f.s,&good,0));c=command(&f,5);
    CHECK(rkqms_session_submit(&f.s,&c,0)==RKQMS_EBUSY);++test_results.gates;
    return 0;
}
static int lifecycle(void){
    struct fixture f;struct rkqms_command c;unsigned int i;
    CHECK(!init(&f,RKQMS_PROOF_ALL,99));c=command(&f,1);CHECK(!rkqms_session_submit(&f.s,&c,0));
    for(i=0;i<6;++i){CHECK(!frame(&f));CHECK(rkqms_session_result(&f.s,99,1)==RKQMS_EAGAIN);}
    CHECK(f.s.transaction.core.phase==RKQMS_STABLE && f.s.status.state==RKQMS_CHANGING);
    CHECK(!frame(&f));CHECK(!rkqms_session_result(&f.s,99,1));CHECK(f.s.status.state==RKQMS_ACTIVE);
    CHECK(f.s.status.effective_tfr==1 && f.s.status.bits_per_component==10 && f.s.status.eotf==2);
    ++test_results.lifecycle;
    c=command(&f,1);CHECK(!rkqms_session_submit(&f.s,&c,0));
    CHECK(rkqms_session_result(&f.s,99,1)==RKQMS_ESTALE);
    CHECK(!frame(&f));CHECK(rkqms_session_result(&f.s,99,2)==RKQMS_EAGAIN);
    CHECK(!frame(&f));CHECK(!rkqms_session_result(&f.s,99,2));++test_results.lifecycle;
    c=command(&f,0);CHECK(!rkqms_session_submit(&f.s,&c,1));
    for(i=0;i<6;++i){CHECK(!frame(&f));CHECK(rkqms_session_result(&f.s,99,3)==RKQMS_EAGAIN);}
    CHECK(!frame(&f));CHECK(!rkqms_session_result(&f.s,99,3));CHECK(rk_qms_clean(&f.s.status));
    CHECK(!f.s.transaction.core.vtem_enabled);++test_results.lifecycle;
    /* No VTEM cadence exists during an ordinary OFF idle. A subsequent
     * activation anchors to its first real frame, then enforces +1 again. */
    f.sequence+=120;f.ns+=2000000000ULL;c=command(&f,2);
    CHECK(!rkqms_session_submit(&f.s,&c,0));CHECK(!finish(&f));++test_results.lifecycle;
    ++f.sequence;CHECK(frame(&f)==RKQMS_EIO);++test_results.lifecycle;
    CHECK(!init(&f,RKQMS_PROOF_ALL,99));
    f.sequence=0xfffffffeU;c=command(&f,2);CHECK(!rkqms_session_submit(&f.s,&c,0));CHECK(!finish(&f));
    CHECK(f.sequence==5);++test_results.lifecycle;
    for(i=1;i<=12;++i){
        CHECK(!init(&f,RKQMS_PROOF_ALL,99));f.io_fail=i;c=command(&f,1);
        CHECK(!rkqms_session_submit(&f.s,&c,0));CHECK(finish(&f)==-5);
        CHECK(f.s.status.state==RKQMS_RECOVERY && !f.s.status.completed_serial && !f.s.status.effective_tfr);
        CHECK(f.s.transaction.needs_native_restore);++test_results.lifecycle;
    }
    CHECK(!init(&f,RKQMS_PROOF_ALL,99));c=command(&f,2);CHECK(!rkqms_session_submit(&f.s,&c,0));CHECK(!frame(&f));
    ++f.sequence;CHECK(frame(&f)==RKQMS_EIO);CHECK(!f.s.status.completed_serial);++test_results.lifecycle;
    CHECK(!init(&f,RKQMS_PROOF_ALL,99));CHECK(!frame(&f));
    CHECK(rkqms_session_frame(&f.s,99,2,f.ns)==RKQMS_EIO);++test_results.lifecycle;
    CHECK(!init(&f,RKQMS_PROOF_ALL,99));c=command(&f,2);CHECK(!rkqms_session_submit(&f.s,&c,0));CHECK(!frame(&f));
    rkqms_session_disconnect(&f.s,100);CHECK(rkqms_session_frame(&f.s,99,2,f.ns+1)==RKQMS_ESTALE);
    CHECK(f.s.status.state==RKQMS_DISCONNECTED && !f.s.status.proof);
    CHECK(rkqms_session_result(&f.s,100,1)==RKQMS_ESTALE);++test_results.lifecycle;
    rkqms_session_quiesced(&f.s);CHECK(rk_qms_native_safe(&f.s.status));
    CHECK(rkqms_session_frame(&f.s,f.s.status.generation,2,f.ns+1)==RKQMS_ENOTSUP);
    CHECK(rkqms_session_result(&f.s,100,1)==RKQMS_ESTALE);++test_results.lifecycle;
    return 0;
}
static int frontend(void){
    struct fixture f;struct rk_qms_client c={0};struct rk_qms_transport t={&f,status,send};
    uint32_t id=999;unsigned int i;
    CHECK(!init(&f,RKQMS_PROOF_ALL,99));
    CHECK(rk_qms_switch(&c,&t,55,1920,1080,16666666)==RK_QMS_NATIVE);CHECK(!f.commands);++test_results.frontend;
    CHECK(rk_qms_switch(&c,&t,56,1920,1080,41708333)==RK_QMS_DONE);
    CHECK(rk_qms_active(&c,&t,&id)==RK_QMS_DONE && id==56);++test_results.frontend;
    CHECK(rk_qms_switch(&c,&t,57,1280,720,41666666)==RK_QMS_NATIVE);
    CHECK(rk_qms_clean(&f.s.status));CHECK(!c.logical_valid);++test_results.frontend;
    CHECK(rk_qms_switch(&c,&t,58,1920,1080,41666666)==RK_QMS_DONE);
    f.status_fail=1;CHECK(rk_qms_active(&c,&t,&id)==RK_QMS_BLOCK);
    memset(&c,0,sizeof(c));CHECK(rk_qms_switch(&c,&t,55,1920,1080,16666666)==RK_QMS_BLOCK);++test_results.frontend;
    f.status_fail=0;CHECK(rk_qms_active(&c,&t,&id)==RK_QMS_BLOCK);++test_results.frontend;
    CHECK(rk_qms_restore(&c,&t)==RK_QMS_NATIVE);CHECK(rk_qms_clean(&f.s.status));++test_results.frontend;
    for(i=1;i<=12;++i){
        CHECK(!init(&f,RKQMS_PROOF_ALL,99));memset(&c,0,sizeof(c));f.forge=i;
        /* Both forged replies must fail. Recovery may nevertheless be
         * independently confirmed BASE by the subsequent genuine status.
         */
        CHECK(rk_qms_switch(&c,&t,90,1920,1080,41666666)!=RK_QMS_DONE);
        CHECK(!c.logical_valid);CHECK(rk_qms_clean(&f.s.status));++test_results.frontend;
    }
    CHECK(!init(&f,RKQMS_PROOF_ALL,99));memset(&c,0,sizeof(c));
    CHECK(rk_qms_switch(&c,&t,59,1920,1080,41666666)==RK_QMS_DONE);
    f.command_fail=1;CHECK(rk_qms_switch(&c,&t,60,1280,720,16666666)==RK_QMS_BLOCK);++test_results.frontend;
    f.command_fail=0;f.native_recovery=1;
    CHECK(rk_qms_restore(&c,&t)==RK_QMS_NATIVE);CHECK(rk_qms_native_safe(&f.s.status));++test_results.frontend;
    CHECK(rk_qms_active(&c,&t,&id)==RK_QMS_NATIVE);++test_results.frontend;
    CHECK(rk_qms_switch(&c,&t,60,1280,720,16666666)==RK_QMS_NATIVE);++test_results.frontend;
    CHECK(!init(&f,RKQMS_PROOF_ALL,99));f.s.status.bytes=0;
    CHECK(rk_qms_switch(&c,&t,60,1280,720,16666666)==RK_QMS_BLOCK);++test_results.frontend;
    CHECK(!init(&f,RKQMS_PROOF_ALL,99));memset(&c,0,sizeof(c));
    CHECK(rk_qms_switch(&c,&t,60,1920,1080,41666666)==RK_QMS_DONE);
    ++f.s.status.link_cookie;CHECK(rk_qms_active(&c,&t,&id)==RK_QMS_BLOCK);++test_results.frontend;
    CHECK(!init(&f,RKQMS_PROOF_ALL,99));memset(&c,0,sizeof(c));
    CHECK(rk_qms_switch(&c,&t,60,1920,1080,41666666)==RK_QMS_DONE);
    f.s.status.eotf=0;CHECK(rk_qms_active(&c,&t,&id)==RK_QMS_BLOCK);++test_results.frontend;
    return 0;
}
static int rates(void){
    unsigned int i;rq_u32 n,d;
    for(i=1;i<=13;++i){CHECK(!rkqms_rate(i,&n,&d));int32_t p=(int32_t)((rq_u64)1000000000*d/n);
        CHECK(rk_qms_tfr(p)==i);CHECK(rk_qms_tfr(p+100)==0);CHECK(rk_qms_tfr(p-100)==0);++test_results.rate_cases;}
    CHECK(!rk_qms_tfr(0) && !rk_qms_tfr(-1) && !rk_qms_tfr(2147483647));++test_results.rate_cases;
    return 0;
}
static int constant_hold(void){
    struct fixture f;struct rkqms_command c;struct rkqms_session before;
    struct rkqms_mode m={594000,5,3840,2160,4400,2250,2168,2178};
    struct rkqms_caps caps={0x7fe,1,1};struct rkqms_io io;struct rkqms_status id;
    unsigned int calls;rq_u64 serial;
    CHECK(!init(&f,RKQMS_PROOF_ALL,99));io=f.s.transaction.io;id=f.s.status;
    CHECK(!rkqms_session_init(&f.s,&m,97,&caps,&io,&id));
    c=command(&f,2);CHECK(!rkqms_session_submit(&f.s,&c,0)&&!finish(&f));
    CHECK(rkqms_session_constant(&f.s));calls=f.io_calls;serial=f.s.status.completed_serial;
    f.sequence+=2;f.ns+=83333334;
    CHECK(!rkqms_session_hold_verified(&f.s,99,f.sequence,f.ns));
    CHECK(f.io_calls==calls && f.s.status.completed_serial==serial && f.s.transaction.core.phase==RKQMS_STABLE);
    ++test_results.lifecycle;before=f.s;
    CHECK(rkqms_session_hold_verified(&f.s,99,f.sequence,f.ns)==RKQMS_EAGAIN);
    CHECK(!memcmp(&before,&f.s,sizeof(before)));++test_results.lifecycle;
    CHECK(rkqms_session_hold_verified(&f.s,99,f.sequence+1,f.ns)==RKQMS_EIO);
    CHECK(rkqms_session_hold_verified(&f.s,98,f.sequence+1,f.ns+1)==RKQMS_ESTALE);
    CHECK(!memcmp(&before,&f.s,sizeof(before)));++test_results.lifecycle;
    c=command(&f,0);CHECK(!rkqms_session_submit(&f.s,&c,1)&&f.s.constant_reanchor);
    CHECK(rkqms_session_frame(&f.s,99,f.sequence+3,f.ns+125000000)==RKQMS_ESTALE);
    f.sequence+=3;f.ns+=125000000;
    CHECK(!rkqms_session_anchor_verified(&f.s,99,f.sequence,f.ns));
    CHECK(f.s.status.completed_serial==serial && f.s.transaction.core.phase==RKQMS_RESTORE_NOTICE);
    CHECK(!rkqms_session_frame(&f.s,99,f.sequence,f.ns));
    CHECK(f.s.transaction.core.phase==RKQMS_RESTORE_APPLY && f.s.status.completed_serial==serial);++test_results.lifecycle;
    CHECK(rkqms_session_frame(&f.s,99,f.sequence+2,f.ns+83333334)==RKQMS_EIO);
    CHECK(f.s.status.state==RKQMS_RECOVERY&&f.s.status.completed_serial==serial);++test_results.lifecycle;
    CHECK(!init(&f,RKQMS_PROOF_ALL,99));io=f.s.transaction.io;id=f.s.status;
    CHECK(!rkqms_session_init(&f.s,&m,97,&caps,&io,&id));c=command(&f,1);
    CHECK(!rkqms_session_submit(&f.s,&c,0)&&!finish(&f));
    CHECK(!rkqms_session_constant(&f.s));before=f.s;
    CHECK(rkqms_session_hold_verified(&f.s,99,f.sequence+2,f.ns+83333334)==RKQMS_ESTALE);
    CHECK(!memcmp(&before,&f.s,sizeof(before)));++test_results.lifecycle;
    CHECK(rkqms_session_frame(&f.s,99,f.sequence+2,f.ns+83333334)==RKQMS_EIO);++test_results.lifecycle;
    return 0;
}
static int late_integer_latch(void){
    struct fixture f;struct rkqms_command c;struct rkqms_session before;
    unsigned int i,n,calls;
    for(i=0;i<13;++i){
        CHECK(!init(&f,RKQMS_PROOF_ALL,99));c=command(&f,10);
        CHECK(!rkqms_session_submit(&f.s,&c,0));
        for(n=0;n<12&&!f.s.awaiting_latch;++n)CHECK(!frame(&f));
        CHECK(rkqms_session_latch_repeatable(&f.s));
        if(i==1)f.s.awaiting_latch=0;
        if(i==2)f.s.transaction.core.packet_const=0;
        if(i==3)f.s.transaction.core.packet_tfr=2;
        if(i==4)f.s.transaction.core.requested_tfr=2;
        if(i==5)--f.s.committed_sequence;
        if(i==6)f.s.status.proof=0;
        if(i==7){f.s.transaction.core.current_tfr=1;f.s.transaction.core.current_vtotal=1126;}
        if(i==8)f.s.transaction.needs_native_restore=1;
        if(i==9)f.s.transaction.core.phase=RKQMS_OFF;
        before=f.s;calls=f.io_calls;
        if(i==0){
            CHECK(!rkqms_session_latch_observe_verified(&f.s,99,f.sequence+2,f.ns+33333334));
            CHECK(f.s.status.completed_serial==1&&rkqms_session_constant(&f.s));
            CHECK(f.io_calls==calls&&f.s.status.sequence==f.sequence+2);
        }else{
            rq_u64 generation=i==10?98:99;
            rq_u32 seq=i==11?f.sequence-1:f.sequence+2;
            rq_u64 ns=i==12?f.ns:f.ns+33333334;
            CHECK(rkqms_session_latch_observe_verified(&f.s,generation,seq,ns)<0);
            CHECK(!memcmp(&before,&f.s,sizeof(before))&&f.io_calls==calls);
        }
        ++test_results.lifecycle;
    }
    return 0;
}
int main(void){
    int e;
    if((e=gates()) || (e=lifecycle()) || (e=frontend()) || (e=rates()) || (e=constant_hold()) || (e=late_integer_latch()))return e;
#ifdef RKQMS_PRIVATE_EDID
    struct rkqms_sink sink;
    CHECK(!rkqms_parse_edid(private_edid,sizeof(private_edid),&sink));
    CHECK(sink.qms && sink.qms_tfr_min && !sink.qms_tfr_max && sink.vrr_min==40 && sink.vrr_max==120);
    CHECK(sink.tfr_mask==0x7fe);++test_results.private_edid;
#endif
    return 0;
}
