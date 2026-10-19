// SPDX-License-Identifier: GPL-2.0-only
#include "lifecycle.h"
#include <stddef.h>
void *memset(void *p,int v,size_t n){unsigned char *b=p;while(n--)*b++=(unsigned char)v;return p;}
void *memcpy(void *p,const void *q,size_t n){unsigned char *a=p;const unsigned char *b=q;while(n--)*a++=*b++;return p;}
int memcmp(const void *p,const void *q,size_t n){const unsigned char *a=p,*b=q;while(n--){if(*a!=*b)return *a<*b?-1:1;++a;++b;}return 0;}
#define CHECK(x) do{if(!(x))return __LINE__;}while(0)
unsigned int lifecycle_test_cases;
int main(void){
    struct rkqms_lifecycle s;
    rkqms_lifecycle_init(&s);CHECK(s.epoch==1 && s.blocked && !s.owns_vtem);++lifecycle_test_cases;
    CHECK(rkqms_lifecycle_check(&s,1)==RKQMS_ESTALE);++lifecycle_test_cases;
    CHECK(rkqms_lifecycle_arm(&s,0)==RKQMS_ESTALE);++lifecycle_test_cases;
    CHECK(rkqms_lifecycle_arm(&s,2)==RKQMS_ESTALE);++lifecycle_test_cases;
    CHECK(!rkqms_lifecycle_arm(&s,1) && !rkqms_lifecycle_check(&s,1));++lifecycle_test_cases;
    rkqms_lifecycle_claim_vtem(&s);CHECK(s.owns_vtem && s.dirty);++lifecycle_test_cases;
    CHECK(rkqms_lifecycle_arm(&s,1)==RKQMS_EBUSY);++lifecycle_test_cases;
    rkqms_lifecycle_invalidate(&s,RKQMS_LIFE_DISABLE);
    CHECK(s.epoch==2 && s.blocked && s.reasons==RKQMS_LIFE_DISABLE && s.owns_vtem);++lifecycle_test_cases;
    CHECK(rkqms_lifecycle_check(&s,1)==RKQMS_ESTALE);++lifecycle_test_cases;
    rkqms_lifecycle_clear_result(&s,-5);CHECK(s.owns_vtem && s.dirty && s.error==-5);++lifecycle_test_cases;
    CHECK(rkqms_lifecycle_arm(&s,2)==RKQMS_EBUSY);++lifecycle_test_cases;
    rkqms_lifecycle_clear_result(&s,0);CHECK(!s.owns_vtem && !s.dirty && !s.error && s.blocked);++lifecycle_test_cases;
    CHECK(rkqms_lifecycle_check(&s,2)==RKQMS_ESTALE);++lifecycle_test_cases;
    CHECK(!rkqms_lifecycle_arm(&s,2) && !s.reasons);++lifecycle_test_cases;
    rkqms_lifecycle_invalidate(&s,RKQMS_LIFE_MODE);rkqms_lifecycle_invalidate(&s,RKQMS_LIFE_ENABLE);
    CHECK(s.epoch==4 && s.reasons==(RKQMS_LIFE_MODE|RKQMS_LIFE_ENABLE));++lifecycle_test_cases;
    s.epoch=~(rq_u64)0;rkqms_lifecycle_invalidate(&s,RKQMS_LIFE_LINK_LOST);
    CHECK(!s.epoch && rkqms_lifecycle_arm(&s,0)==RKQMS_ESTALE);++lifecycle_test_cases;
    rkqms_lifecycle_invalidate(&s,RKQMS_LIFE_SUSPEND);CHECK(!s.epoch && s.blocked);++lifecycle_test_cases;
    return 0;
}
