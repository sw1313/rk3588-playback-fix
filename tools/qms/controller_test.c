// SPDX-License-Identifier: GPL-2.0-only
/* Actual controller/session/core, pthread stand-ins for kernel primitives and
 * injected native operations. No DRM, HDMI, ioctl, root or module loading.
 */
#include "controller.h"
#include "endpoint.h"
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
unsigned int fixture_reads,fixture_writes;
bool fixture_bad_read,fixture_bad_write;
static struct miscdevice *fixture_misc;
int misc_register(struct miscdevice *m){if(fixture_misc)return -EBUSY;fixture_misc=m;return 0;}
void misc_deregister(struct miscdevice *m){if(fixture_misc==m)fixture_misc=NULL;}
static long dispatch(unsigned int request,void *p){return fixture_misc->fops->unlocked_ioctl(NULL,request,(unsigned long)p);}
struct fixture {
    struct rkqms_controller c;
    unsigned int frames,restores,delay_ms,refreshes;
    int frame_error,recovery_error;
    rq_u32 sequence;
    rq_u64 ns;
};
static int stop(struct fixture *f){int r=rkqms_controller_stop(&f->c);pthread_mutex_destroy(&f->c.request_lock.value);pthread_mutex_destroy(&f->c.publication_lock);pthread_cond_destroy(&f->c.wake.cond);pthread_mutex_destroy(&f->c.wake.lock);return r;}
static int begin(void *p,rq_u64 g,rq_u32 seq){(void)p;(void)g;(void)seq;return 0;}
static int write_word(void *p,rq_u32 o,rq_u32 v){(void)p;(void)o;(void)v;return 0;}
static int timing(void *p,rq_u16 t,rq_u16 v){(void)p;(void)t;(void)v;return 0;}
static void end(void *p){(void)p;}
static int native_frame(void *p,struct rkqms_session *s){
    struct fixture *f=p;__atomic_add_fetch(&f->frames,1,__ATOMIC_RELEASE);
    msleep(READ_ONCE(f->delay_ms));
    if(f->frame_error)return f->frame_error;
    f->ns+=16666667;
    ++f->sequence;
    if(rkqms_session_constant(s))return rkqms_session_hold_verified(s,s->status.generation,f->sequence,f->ns);
    if(s->constant_reanchor){int e=rkqms_session_anchor_verified(s,s->status.generation,f->sequence,f->ns);if(e)return e;}
    return rkqms_session_frame(s,s->status.generation,f->sequence,f->ns);
}
static int native_recover(void *p,struct rkqms_session *s){
    struct fixture *f=p;(void)s;__atomic_add_fetch(&f->restores,1,__ATOMIC_RELEASE);return f->recovery_error;
}
static int native_refresh(void *p,struct rkqms_session *s){
    struct fixture *f=p;(void)s;
    __atomic_add_fetch(&f->refreshes,1,__ATOMIC_RELEASE);return -EOPNOTSUPP;
}
static int start_with_refresh(struct fixture *f,int refresh){
    struct rkqms_mode m={148500,5,1920,1080,2200,1125,1084,1089};
    struct rkqms_caps caps={0x7fe,1,1};
    struct rkqms_io io={f,begin,write_word,write_word,timing,end};
    struct rkqms_status id={0};struct rkqms_session session;
    struct rkqms_controller_ops ops={.context=f,.frame=native_frame,.recover=native_recover};int ret;
    memset(f,0,sizeof(*f));f->delay_ms=1;
    if(refresh)ops.refresh=native_refresh;
    id.abi=RKQMS_ABI;id.bytes=sizeof(id);id.generation=7;id.link_cookie=9;
    id.connector=1;id.crtc=2;id.proof=RKQMS_PROOF_ALL;id.bits_per_component=10;
    ret=rkqms_session_init(&session,&m,16,&caps,&io,&id);if(ret)return ret;
    return rkqms_controller_start(&f->c,&session,&ops);
}
static int start(struct fixture *f){return start_with_refresh(f,0);}
static struct rkqms_command command(struct fixture *f,unsigned int target){
    struct rkqms_command c={0};struct rkqms_status s={0};rkqms_controller_status(&f->c,&s);
    c.abi=RKQMS_ABI;c.bytes=sizeof(c);c.generation=s.generation;c.link_cookie=s.link_cookie;
    c.connector=s.connector;c.crtc=s.crtc;c.target_tfr=target;c.expected_serial=s.serial;c.timeout_ms=100;return c;
}
struct caller {struct fixture *f;struct rkqms_command command;int result;u64 elapsed;};
static void *call_command(void *p){struct caller *a=p;u64 t=ktime_get_ns();a->result=rkqms_controller_command(&a->f->c,&a->command,0);a->elapsed=ktime_get_ns()-t;return 0;}
static int wait_frames(struct fixture *f){unsigned int i;for(i=0;i<200;++i){if(READ_ONCE(f->frames))return 0;msleep(1);}return -1;}
int main(void){
    struct fixture f;struct rkqms_command c;struct rkqms_status s;unsigned int cases=0;
    struct caller caller;pthread_t thread;
    CHECK(!start(&f));c=command(&f,5);CHECK(!rkqms_controller_command(&f.c,&c,0));
    CHECK(c.result.state==RKQMS_ACTIVE && c.result.completed_serial==1);++cases;
    {unsigned int n=READ_ONCE(f.frames);msleep(160);CHECK(!rkqms_controller_status(&f.c,&s));
    CHECK(s.state==RKQMS_ACTIVE && !s.error && READ_ONCE(f.frames)>n && READ_ONCE(f.frames)<=n+2);++cases;}
    /* Wake from the constant watchdog before its 100ms timeout. */
    msleep(10);
    {u64 now=ktime_get_ns();c=command(&f,8);c.timeout_ms=1000;
    CHECK(!rkqms_controller_command(&f.c,&c,0));CHECK(ktime_get_ns()-now<80000000ULL);++cases;}
    c=command(&f,0);CHECK(!rkqms_controller_command(&f.c,&c,1));CHECK(c.result.state==RKQMS_BASE);++cases;
    c=command(&f,5);c.reserved[2]=1;CHECK(rkqms_controller_command(&f.c,&c,0)==-EINVAL);++cases;
    CHECK(!stop(&f));CHECK(READ_ONCE(f.restores)==1);++cases;
    /* A long native fence wait must not hold the publication lock. */
    CHECK(!start(&f));WRITE_ONCE(f.delay_ms,250);
    caller=(struct caller){.f=&f,.command=command(&f,2)};
    CHECK(!pthread_create(&thread,0,call_command,&caller));CHECK(!wait_frames(&f));
    c=command(&f,5);CHECK(rkqms_controller_command(&f.c,&c,0)==-EBUSY);++cases;
    rkqms_controller_status(&f.c,&s);CHECK(s.state==RKQMS_CHANGING);++cases;
    pthread_join(thread,0);CHECK(caller.result==-ETIMEDOUT && caller.elapsed<200000000ULL);++cases;
    msleep(200);rkqms_controller_status(&f.c,&s);
    CHECK(s.state==RKQMS_QUIESCENT && !s.proof && !s.completed_serial && READ_ONCE(f.restores)==1);++cases;
    CHECK(!stop(&f));++cases;
    CHECK(!start(&f));f.frame_error=-EIO;f.recovery_error=-EIO;
    c=command(&f,2);CHECK(rkqms_controller_command(&f.c,&c,0)<0);
    msleep(20);rkqms_controller_status(&f.c,&s);
    CHECK(s.state==RKQMS_RECOVERY && s.error==-EIO && READ_ONCE(f.restores)==1);++cases;
    CHECK(stop(&f)==-EIO);++cases;
    /* Cancel a request queued behind stable maintenance before acceptance;
     * it must not switch rates after the caller has already timed out.
     */
    CHECK(!start(&f));c=command(&f,2);CHECK(!rkqms_controller_command(&f.c,&c,0));
    {unsigned int n=READ_ONCE(f.frames),i;WRITE_ONCE(f.delay_ms,250);
    for(i=0;i<200 && READ_ONCE(f.frames)<=n;++i)msleep(1);
    CHECK(READ_ONCE(f.frames)>n);}
    c=command(&f,5);CHECK(rkqms_controller_command(&f.c,&c,0)==-ETIMEDOUT);++cases;
    msleep(200);rkqms_controller_status(&f.c,&s);
    CHECK(s.serial==1 && s.state==RKQMS_ACTIVE && !READ_ONCE(f.restores));++cases;
    WRITE_ONCE(f.delay_ms,1);CHECK(!stop(&f));++cases;
    /* An unavailable idle refresh must not turn ordinary native modes into
     * recovery, and endpoint publication must gate commands independently. */
    CHECK(!start_with_refresh(&f,1));WRITE_ONCE(f.c.command_ready,false);
    c=command(&f,2);CHECK(rkqms_controller_command(&f.c,&c,0)==-ENODEV);
    CHECK(!f.c.request_id && !READ_ONCE(f.frames));++cases;
    msleep(120);rkqms_controller_status(&f.c,&s);
    CHECK(READ_ONCE(f.refreshes)>0 && s.state==RKQMS_BASE && !s.error && !READ_ONCE(f.restores));++cases;
    WRITE_ONCE(f.c.command_ready,true);c=command(&f,2);c.timeout_ms=1000;
    CHECK(!rkqms_controller_command(&f.c,&c,0));
    {unsigned int n=READ_ONCE(f.refreshes);msleep(30);CHECK(READ_ONCE(f.refreshes)==n);}
    CHECK(!stop(&f));++cases;
    printf("controller_cases=%u; ",cases);cases=0;
    /* Actual ioctl dispatcher, fake misc registration/copy primitives only.
     * No character device is created or opened by these tests.
     */
    CHECK(!start(&f));CHECK(!rkqms_endpoint_register(&f.c));CHECK(fixture_misc);++cases;
    CHECK(rkqms_endpoint_register(&f.c)==-EBUSY);++cases;
    fixture_reads=fixture_writes=0;
    CHECK(!dispatch(RKQMS_GET_STATUS,&s) && fixture_reads==0 && fixture_writes==1);++cases;
    CHECK(dispatch(RKQMS_SWITCH^(1U<<8),&c)==-ENOTTY);++cases;
    c=command(&f,2);fixture_bad_read=true;CHECK(dispatch(RKQMS_SWITCH,&c)==-EFAULT);fixture_bad_read=false;++cases;
    c=command(&f,2);c.abi=99;CHECK(dispatch(RKQMS_SWITCH,&c)==-EINVAL && !READ_ONCE(f.frames));++cases;
    c=command(&f,2);fixture_reads=fixture_writes=0;
    CHECK(!dispatch(RKQMS_SWITCH,&c) && c.result.state==RKQMS_ACTIVE && fixture_reads==1 && fixture_writes==1);++cases;
    c=command(&f,0);CHECK(!dispatch(RKQMS_RESTORE,&c) && c.result.state==RKQMS_BASE);++cases;
    fixture_bad_write=true;CHECK(dispatch(RKQMS_GET_STATUS,&s)==-EFAULT);fixture_bad_write=false;++cases;
    rkqms_endpoint_unregister();CHECK(!fixture_misc);++cases;
    CHECK(!stop(&f));++cases;
    printf("endpoint_cases=%u; native_ops=injected; display_IO=0\n",cases);
    return 0;
}
