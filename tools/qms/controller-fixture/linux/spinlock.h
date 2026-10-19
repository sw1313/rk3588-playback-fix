#ifndef RKQMS_SPINLOCK_FIXTURE_H
#define RKQMS_SPINLOCK_FIXTURE_H
#include "../fixture.h"
static inline void spin_lock(spinlock_t *p){pthread_mutex_lock(p);}
static inline void spin_unlock(spinlock_t *p){pthread_mutex_unlock(p);}
#endif
