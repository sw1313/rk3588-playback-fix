/* SPDX-License-Identifier: GPL-2.0-only */
/* Android pthread fixture. NEVER a kernel build include directory. */
#ifndef RKQMS_CONTROLLER_FIXTURE_H
#define RKQMS_CONTROLLER_FIXTURE_H
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
#define READ_ONCE(x) __atomic_load_n(&(x),__ATOMIC_ACQUIRE)
#define WRITE_ONCE(x,v) __atomic_store_n(&(x),(v),__ATOMIC_RELEASE)
#define IS_ERR(x) ((uintptr_t)(x)>=(uintptr_t)-4095)
#define PTR_ERR(x) ((long)(x))
struct mutex {pthread_mutex_t value;};
static inline void mutex_init(struct mutex *m){pthread_mutex_init(&m->value,0);}
static inline void mutex_lock(struct mutex *m){pthread_mutex_lock(&m->value);}
static inline int mutex_trylock(struct mutex *m){return !pthread_mutex_trylock(&m->value);}
static inline void mutex_unlock(struct mutex *m){pthread_mutex_unlock(&m->value);}
typedef pthread_mutex_t spinlock_t;
#define spin_lock_init(p) pthread_mutex_init((p),0)
#define spin_lock_irqsave(p,f) do{(f)=0;pthread_mutex_lock(p);}while(0)
#define spin_unlock_irqrestore(p,f) do{(void)(f);pthread_mutex_unlock(p);}while(0)
static inline void msleep(unsigned int n){struct timespec t={n/1000,(long)(n%1000)*1000000L};while(nanosleep(&t,&t)&&errno==EINTR){}}
static inline u64 ktime_get_ns(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (u64)t.tv_sec*1000000000ULL+t.tv_nsec;}
struct task_struct {pthread_t thread;bool stop;int (*run)(void *);void *arg;int result;};
static __thread struct task_struct *fixture_current;
static inline int kthread_should_stop(void){return READ_ONCE(fixture_current->stop);}
static void *fixture_task_entry(void *p){struct task_struct *t=p;fixture_current=t;t->result=t->run(t->arg);return 0;}
static inline struct task_struct *fixture_kthread_run(int (*run)(void *),void *p){
    struct task_struct *t=calloc(1,sizeof(*t));if(!t)return (void *)(intptr_t)-ENOMEM;
    t->run=run;t->arg=p;if(pthread_create(&t->thread,0,fixture_task_entry,t)){free(t);return (void *)(intptr_t)-ENOMEM;}return t;
}
#define kthread_run(fn,p,name) fixture_kthread_run(fn,p)
static inline int kthread_stop(struct task_struct *t){int ret;WRITE_ONCE(t->stop,true);pthread_join(t->thread,0);ret=t->result;free(t);return ret;}
#endif
