/* pthread stand-in: condition is checked under the wait lock, so a command
 * published before or during entry cannot be lost. Not a kernel include. */
#ifndef RKQMS_FIXTURE_WAIT_H
#define RKQMS_FIXTURE_WAIT_H
#include "../fixture.h"
typedef struct { pthread_mutex_t lock; pthread_cond_t cond; } wait_queue_head_t;
static inline void init_waitqueue_head(wait_queue_head_t *w){
    pthread_condattr_t attr;pthread_mutex_init(&w->lock,0);
    pthread_condattr_init(&attr);pthread_condattr_setclock(&attr,CLOCK_MONOTONIC);
    pthread_cond_init(&w->cond,&attr);pthread_condattr_destroy(&attr);
}
static inline void wake_up(wait_queue_head_t *w){
    pthread_mutex_lock(&w->lock);pthread_cond_broadcast(&w->cond);pthread_mutex_unlock(&w->lock);
}
#define wait_event_interruptible_timeout(w,condition,ms) do { \
    struct timespec end;clock_gettime(CLOCK_MONOTONIC,&end); \
    end.tv_nsec+=(long)((ms)%1000)*1000000L;end.tv_sec+=(ms)/1000+end.tv_nsec/1000000000L;end.tv_nsec%=1000000000L; \
    pthread_mutex_lock(&(w).lock); \
    while(!(condition)){if(pthread_cond_timedwait(&(w).cond,&(w).lock,&end)==ETIMEDOUT)break;} \
    pthread_mutex_unlock(&(w).lock); \
}while(0)
#endif
