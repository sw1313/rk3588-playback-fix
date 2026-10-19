// SPDX-License-Identifier: GPL-2.0-only
/* Real engine + session/core. Only low-level native functions are injected;
 * this tests publication/error policy, not kernel ABI or HDMI hardware.
 */
#include "session.h"
#include <stddef.h>
#include <errno.h>
#include <linux/ktime.h>
void *memset(void *p,int v,size_t n){unsigned char *b=p;while(n--)*b++=(unsigned char)v;return p;}
void *memcpy(void *p,const void *q,size_t n){unsigned char *a=p;const unsigned char *b=q;while(n--)*a++=*b++;return p;}
int memcmp(const void *p,const void *q,size_t n){const unsigned char *a=p,*b=q;while(n--){if(*a!=*b)return *a<*b?-1:1;++a;++b;}return 0;}
typedef unsigned long long u64;
typedef unsigned int u32;
struct task_struct {int unused;};
static struct task_struct task,other_task;
static struct task_struct *current=&task;
struct drm_crtc {int unused;};
struct drm_crtc_state {int unused;};
struct drm_atomic_state {int unused;};
struct rkqms_registers {
    struct drm_crtc *crtc;
    struct task_struct *task;
    struct drm_atomic_state *transaction;
    struct drm_crtc_state *old;
    unsigned long commit_mutex;
    void (*flush)(struct drm_crtc *,struct drm_crtc_state *);
    u32 sequence;
    rq_u64 generation;
    unsigned char vtem_before,flushed;
    int error;
};
static unsigned int flushes,begins,ends,repeats;
static int begin_error,finish_error,hold_error;
static u64 actual_sequence=5;
static ktime_t actual_ns=1000000;
static void native_flush(struct drm_crtc *c,struct drm_crtc_state *o){(void)c;(void)o;++flushes;}
u64 drm_crtc_vblank_count_and_time(struct drm_crtc *c,ktime_t *n){(void)c;*n=actual_ns;return actual_sequence;}
static int rkqms_registers_begin(void *p,rq_u64 g,rq_u32 seq){(void)p;(void)g;(void)seq;++begins;return begin_error;}
static void rkqms_registers_end(void *p){struct rkqms_registers *r=p;++ends;if(!r->flushed){r->flushed=1;r->flush(r->crtc,r->old);}}
static int rkqms_registers_finish(struct rkqms_registers *r){(void)r;return finish_error;}
static int rkqms_registers_hold(struct rkqms_registers *r,const struct rkqms_state *s){(void)r;(void)s;return hold_error;}
static int rkqms_registers_repeat(struct rkqms_registers *r,const struct rkqms_state *s){(void)r;(void)s;++repeats;return hold_error;}
#define RKQMS_NATIVE_REGISTERS_H
#include "native_engine.h"
#define CHECK(x) do{if(!(x))return __LINE__;}while(0)
unsigned int engine_test_cases;
static struct rkqms_session live;
static struct rkqms_registers registers;
static struct rkqms_native_step step;
static struct drm_crtc crtc,other_crtc;
static struct drm_crtc_state old;
static struct drm_atomic_state transaction;
static int write_packet(void *p,rq_u32 o,rq_u32 v){(void)p;(void)o;(void)v;return 0;}
static int write_timing(void *p,rq_u16 t,rq_u16 v){(void)p;(void)t;(void)v;return 0;}
static int reset(void){
    struct rkqms_mode m={594000,5,3840,2160,4400,2250,2168,2178};
    struct rkqms_caps caps={0x7fe,1,1};
    struct rkqms_io io={&registers,rkqms_registers_begin,write_packet,write_packet,write_timing,rkqms_registers_end};
    struct rkqms_status id={0};
    begin_error=finish_error=hold_error=0;flushes=begins=ends=repeats=0;current=&task;
    actual_sequence=5;actual_ns=1000000;
    memset(&registers,0,sizeof(registers));registers.crtc=&crtc;
    id.abi=RKQMS_ABI;id.bytes=sizeof(id);id.generation=7;id.link_cookie=9;
    id.connector=1;id.crtc=2;id.proof=RKQMS_PROOF_ALL;id.bits_per_component=10;
    return rkqms_session_init(&live,&m,97,&caps,&io,&id);
}
static void prepare(void){rkqms_native_step_prepare(&step,&live,&registers,&transaction,123,native_flush);}
int main(void){
    struct rkqms_session before;
    struct rkqms_command command={0};
    unsigned int i;
    CHECK(!reset());prepare();before=live;
    rkqms_native_step_window(&step,&crtc,&old);
    CHECK(!memcmp(&before,&live,sizeof(live)) && flushes==1 && begins==1 && ends==1);
    CHECK(!rkqms_native_step_finish(&step,0));
    CHECK(live.have_observation && live.status.sequence==5 && live.status.observed_ns==1000000);
    ++engine_test_cases;
    /* Window mutations must never publish a partially advanced session. */
    for(i=0;i<8;++i){
        CHECK(!reset());prepare();before=live;
        switch(i){case 0:begin_error=-EIO;break;case 1:finish_error=-EIO;break;
        case 2:actual_ns=0;break;case 3:current=&other_task;break;default:break;}
        if(i!=4)rkqms_native_step_window(&step,i==5?&other_crtc:&crtc,&old);
        if(i==6)rkqms_native_step_window(&step,&crtc,&old);
        CHECK(!memcmp(&before,&live,sizeof(live)));
        CHECK(rkqms_native_step_finish(&step,i==7?-EIO:0)<0);
        CHECK(live.status.state==RKQMS_RECOVERY && live.transaction.needs_native_restore);
        CHECK(!live.have_observation && !live.status.observed_ns && !live.status.completed_serial);
        ++engine_test_cases;
    }
    /* A confirmed staged ticket must not escape if native finish fails. */
    CHECK(!reset());command.abi=RKQMS_ABI;command.bytes=sizeof(command);
    command.generation=7;command.link_cookie=9;command.connector=1;command.crtc=2;
    command.target_tfr=2;command.timeout_ms=1000;
    CHECK(!rkqms_session_submit(&live,&command,0));
    live.transaction.core.phase=RKQMS_STABLE;live.transaction.core.current_tfr=2;
    live.transaction.core.requested_tfr=2;live.transaction.core.vtem_enabled=1;
    live.awaiting_latch=1;live.committed_sequence=4;
    prepare();rkqms_native_step_window(&step,&crtc,&old);
    CHECK(step.staged.status.completed_serial==1 && !live.status.completed_serial);
    finish_error=-EIO;CHECK(rkqms_native_step_finish(&step,0)==-EIO);
    CHECK(!live.status.completed_serial && live.status.state==RKQMS_RECOVERY);
    ++engine_test_cases;
    /* Cancellation/epoch/serial races must not be overwritten by staging. */
    for(i=0;i<4;++i){
        CHECK(!reset());live.have_observation=1;live.status.sequence=4;
        live.status.observed_ns=900000;
        live.transaction.core.phase=RKQMS_STABLE;
        live.transaction.core.current_tfr=2;
        if(i==0)actual_ns=0;
        if(i==1)actual_ns=900000;
        if(i==2)actual_sequence=4;
        if(i==3)actual_sequence=6;
        prepare();before=live;rkqms_native_step_window(&step,&crtc,&old);
        CHECK(step.observation_diagnostic==(i==0?91:i==1?92:93));
        CHECK(step.previous_sequence==4&&step.sampled_sequence==actual_sequence);
        CHECK(step.previous_ns==900000&&step.sampled_ns==(u64)actual_ns);
        CHECK(!memcmp(&before,&live,sizeof(live)));
        CHECK(rkqms_native_step_finish(&step,0)==-EIO);
        CHECK(live.status.state==RKQMS_RECOVERY&&!live.status.completed_serial);
        ++engine_test_cases;
    }
    for(i=0;i<3;++i){
        CHECK(!reset());prepare();rkqms_native_step_window(&step,&crtc,&old);
        if(i==0)++live.status.generation;
        if(i==1)++live.status.serial;
        if(i==2)++live.transaction.core.revision;
        CHECK(rkqms_native_step_finish(&step,0)==-ESTALE && !live.have_observation);
        ++engine_test_cases;
    }
    for(i=0;i<4;++i){
        CHECK(!reset());live.transaction.core.phase=RKQMS_STABLE;
        live.transaction.core.current_tfr=live.transaction.core.packet_tfr=2;
        live.transaction.core.packet_const=live.transaction.core.vtem_enabled=1;
        live.transaction.core.current_vtotal=5625;live.status.effective_tfr=2;
        live.status.state=RKQMS_ACTIVE;live.status.serial=live.status.completed_serial=1;
        live.have_observation=1;live.status.sequence=4;live.status.observed_ns=900000;
        command.target_tfr=3;command.expected_serial=1;
        CHECK(!rkqms_session_submit(&live,&command,0)&&live.constant_reanchor);
        actual_sequence=7;actual_ns=2000000;
        if(i==1)hold_error=-ESTALE;if(i==3)finish_error=-EIO;
        prepare();before=live;rkqms_native_step_window(&step,&crtc,&old);
        CHECK(!memcmp(&before,&live,sizeof(live)));
        if(i){CHECK(rkqms_native_step_finish(&step,i==2?-EIO:0)<0);
            CHECK(live.status.state==RKQMS_RECOVERY&&live.status.completed_serial==1);
        }else{
            CHECK(!rkqms_native_step_finish(&step,0));
            CHECK(step.reanchored&&!live.constant_reanchor&&live.status.sequence==7);
            CHECK(live.status.state==RKQMS_CHANGING&&live.status.completed_serial==1);
            CHECK(live.transaction.core.phase==RKQMS_TARGET_NOTICE);
            actual_sequence=9;actual_ns=3000000;prepare();rkqms_native_step_window(&step,&crtc,&old);
            CHECK(!rkqms_native_step_finish(&step,0)&&live.status.completed_serial==1);
            CHECK(step.repeated&&repeats==1&&live.transaction.core.phase==RKQMS_APPLY);
        }
        ++engine_test_cases;
    }
    /* Replay actual multi-frame activation, rate changes and disable with
     * a missed observation at every repeatable notice. Every action still
     * runs once, final ticket latch and fractional frames stay exact +1. */
    {
        static const unsigned int targets[]={2,3,1,10,0};
        unsigned int j,k,phase,frames,notices=0;
        CHECK(!reset());actual_sequence=100;actual_ns=1000000000;
        for(j=0;j<sizeof(targets)/sizeof(targets[0]);++j){
            command.expected_serial=live.status.serial;command.target_tfr=targets[j];
            CHECK(!rkqms_session_submit(&live,&command,!targets[j]));
            for(k=0;k<20&&live.status.completed_serial!=live.status.serial;++k){
                int repeatable=rkqms_session_repeatable(&live);
                phase=live.transaction.core.phase;frames=live.transaction.core.phase_frames;
                actual_sequence+=repeatable?2:1;actual_ns+=repeatable?83333334:41666667;
                prepare();before=live;rkqms_native_step_window(&step,&crtc,&old);
                CHECK(!memcmp(&before,&live,sizeof(live)));
                CHECK(!rkqms_native_step_finish(&step,0));
                CHECK(step.repeated==repeatable);
                if(repeatable){
                    ++notices;
                    /* A repeated BASE_CONST advances its minimum wait by
                     * just one, never by the observed sequence gap. */
                    if(phase==RKQMS_BASE_CONST&&frames==0)
                        CHECK(live.transaction.core.phase==phase&&live.transaction.core.phase_frames==1);
                }
            }
            CHECK(live.status.completed_serial==live.status.serial);
            ++engine_test_cases;
        }
        CHECK(notices>=8);
    }
    /* Reject delay on pending latch / fractional timing / changed hardware,
     * retain prior completed ticket even if the staged reanchor succeeded. */
    for(i=0;i<7;++i){
        CHECK(!reset());command.expected_serial=0;command.target_tfr=2;
        CHECK(!rkqms_session_submit(&live,&command,0));
        live.have_observation=1;live.status.sequence=4;live.status.observed_ns=900000;
        live.transaction.core.phase=RKQMS_TARGET_NOTICE;
        live.transaction.core.sequence_valid=1;live.transaction.core.last_sequence=4;
        live.transaction.core.current_tfr=live.transaction.core.packet_tfr=2;
        live.transaction.core.current_vtotal=5625;live.transaction.core.vtem_enabled=1;
        actual_sequence=6;actual_ns=2000000;
        if(i==0)live.awaiting_latch=1;
        if(i==1){live.transaction.core.current_tfr=1;live.transaction.core.current_vtotal=5630;}
        if(i==2)hold_error=-ESTALE;
        if(i==3)finish_error=-EIO;
        if(i==4)actual_sequence=4;
        if(i==5)actual_sequence=3;
        if(i==6)actual_ns=900000;
        prepare();rkqms_native_step_window(&step,&crtc,&old);
        CHECK(step.initial_phase==RKQMS_TARGET_NOTICE&&step.initial_awaiting_latch==(i==0));
        CHECK(rkqms_native_step_finish(&step,0)<0);
        CHECK(live.status.state==RKQMS_RECOVERY&&!live.status.completed_serial);
        ++engine_test_cases;
    }
    /* Actual failure: prior integer M_CONST commit latched successfully, but
     * the final observation arrives +2 (77015 -> 77017). Do not synthesize a
     * missing plan or publish a ticket before current native finish. */
    for(i=0;i<8;++i){
        unsigned int n;
        CHECK(!reset());command.expected_serial=0;command.target_tfr=10;
        CHECK(!rkqms_session_submit(&live,&command,0));
        for(n=0;n<12&&!live.awaiting_latch;++n){
            ++actual_sequence;actual_ns+=16666667;
            prepare();rkqms_native_step_window(&step,&crtc,&old);
            CHECK(!rkqms_native_step_finish(&step,0));
        }
        CHECK(rkqms_session_latch_repeatable(&live)&&live.awaiting_latch);
        CHECK(live.status.completed_serial==0);
        actual_sequence+=2;actual_ns+=33333334;
        if(i==1)hold_error=-ESTALE;
        if(i==2)finish_error=-EIO;
        if(i==4)actual_ns=live.status.observed_ns;
        if(i==5)actual_sequence=live.status.sequence-1;
        if(i==6)live.transaction.core.packet_const=0;
        if(i==7){live.transaction.core.current_tfr=1;live.transaction.core.current_vtotal=5630;}
        prepare();before=live;rkqms_native_step_window(&step,&crtc,&old);
        CHECK(!memcmp(&before,&live,sizeof(live)));
        if(i==0){
            CHECK(step.latch_reobserved&&step.staged.status.completed_serial==1);
            CHECK(begins>0&&flushes>0);
            CHECK(!rkqms_native_step_finish(&step,0));
            CHECK(live.status.completed_serial==1&&rkqms_session_constant(&live));
            CHECK(live.status.sequence==actual_sequence&&!live.awaiting_latch);
        }else{
            CHECK(rkqms_native_step_finish(&step,i==3?-EIO:0)<0);
            CHECK(!live.status.completed_serial&&live.status.state==RKQMS_RECOVERY);
        }
        ++engine_test_cases;
    }
    return 0;
}
