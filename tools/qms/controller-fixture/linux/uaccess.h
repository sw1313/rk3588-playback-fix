#include "../fixture.h"
extern unsigned int fixture_reads,fixture_writes;
extern bool fixture_bad_read,fixture_bad_write;
static inline unsigned long copy_from_user(void *to,const void *from,unsigned long n){++fixture_reads;if(fixture_bad_read)return n;memcpy(to,from,n);return 0;}
static inline unsigned long copy_to_user(void *to,const void *from,unsigned long n){++fixture_writes;if(fixture_bad_write)return n;memcpy(to,from,n);return 0;}
static inline long copy_from_kernel_nofault(void *to,const void *from,unsigned long n){memcpy(to,from,n);return 0;}
