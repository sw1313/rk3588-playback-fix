#include "../fixture.h"
#include <stddef.h>
#define DEFINE_MUTEX(n) struct mutex n={PTHREAD_MUTEX_INITIALIZER}
#define THIS_MODULE ((void *)1)
#define BUILD_BUG_ON(x) _Static_assert(!(x),"fixture layout")
#define __user
