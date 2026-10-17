// SPDX-License-Identifier: MIT
#include "csf_group_core.h"
void *memset(void *p,int v,__SIZE_TYPE__ n){volatile unsigned char *q=p;for(__SIZE_TYPE__ i=0;i<n;i++)q[i]=v;return p;}
struct mock {unsigned creates,binds,terms,other;int fail_create,fail_bind,fail_term;uint8_t next,seen[3];};
static void init(uint8_t *a) {for(int i=0;i<32;i++)a[i]=0; a[0]=1;for(int i=8;i<24;i++)a[i]=255;a[24]=1;a[26]=1;a[27]=64;a[28]=64;}
static int invoke(void *ctx,int fd,unsigned long req,void *arg) {
    (void)fd;struct mock *m=ctx;uint8_t *a=arg;
    if(req==GROUP_CREATE){if(a[24]!=3)return -71;if(m->fail_create)return -5;m->creates++;a[0]=++m->next;a[4]=99;return 0;}
    if(req==QUEUE_BIND){if(m->fail_bind)return -5;m->seen[m->binds%3]=a[9];m->binds++;a[0]=19;return 0;}
    if(req==GROUP_TERM){if(m->fail_term)return -5;m->terms++;return 0;}m->other++;return 17;
}
#define T(c,n) if(!(c))return n
long test_all(void) {
 struct csf_groups s={0};struct mock m={0};uint8_t a[32],handle=0;
 T(csf_group_ioctl(&s,8,invoke,&m,11,123,0)==17,1);T(m.other==1,2);
 for(int round=0;round<1000;round++){
  for(int j=0;j<3;j++){init(a);T(csf_group_ioctl(&s,8,invoke,&m,11,GROUP_CREATE,a)==0,3);if(!j)handle=a[0];T(a[0]==handle,4);}
  for(int j=0;j<3;j++){for(int k=0;k<16;k++)a[k]=0;a[8]=handle;T(csf_group_ioctl(&s,8,invoke,&m,11,QUEUE_BIND,a)==0,5);}
  T(m.seen[0]==0&&m.seen[1]==1&&m.seen[2]==2,6);
  for(int j=0;j<3;j++){a[0]=handle;T(csf_group_ioctl(&s,8,invoke,&m,11,GROUP_TERM,a)==0,7);T(m.terms==(unsigned)(round+(j==2)),8);}
 }
 T(m.creates==1000&&m.binds==3000&&m.terms==1000,9);
 init(a);a[24]=2;T(csf_group_ioctl(&s,8,invoke,&m,11,GROUP_CREATE,a)==-22,10);
 init(a);m.fail_create=1;T(csf_group_ioctl(&s,8,invoke,&m,11,GROUP_CREATE,a)==-5,11);m.fail_create=0;
 init(a);T(!csf_group_ioctl(&s,8,invoke,&m,11,GROUP_CREATE,a),12);handle=a[0];
 for(int i=0;i<16;i++)a[i]=0;a[8]=handle;m.fail_bind=1;T(csf_group_ioctl(&s,8,invoke,&m,11,QUEUE_BIND,a)==-5,13);T(a[9]==0,14);m.fail_bind=0;T(!csf_group_ioctl(&s,8,invoke,&m,11,QUEUE_BIND,a),15);
 a[0]=handle;m.fail_term=1;T(csf_group_ioctl(&s,8,invoke,&m,11,GROUP_TERM,a)==-5,16);m.fail_term=0;T(!csf_group_ioctl(&s,8,invoke,&m,11,GROUP_TERM,a),17);
 // Distinct initialization threads and fds must never share a CSG.
 init(a);T(!csf_group_ioctl(&s,10,invoke,&m,11,GROUP_CREATE,a),18);uint8_t h0=a[0];
 init(a);T(!csf_group_ioctl(&s,11,invoke,&m,11,GROUP_CREATE,a),19);uint8_t h1=a[0];T(h0!=h1,20);
 init(a);T(!csf_group_ioctl(&s,10,invoke,&m,12,GROUP_CREATE,a),21);uint8_t h2=a[0];T(h0!=h2,22);
 a[0]=h0;T(!csf_group_ioctl(&s,10,invoke,&m,11,GROUP_TERM,a),23);
 a[0]=h1;T(!csf_group_ioctl(&s,11,invoke,&m,11,GROUP_TERM,a),24);
 a[0]=h2;T(!csf_group_ioctl(&s,10,invoke,&m,12,GROUP_TERM,a),25);
 // All 32 tracking slots have bounded capacity; no unknown-handle reuse.
 for(int i=0;i<32;i++){init(a);T(!csf_group_ioctl(&s,100+i,invoke,&m,11,GROUP_CREATE,a),26);}
 init(a);T(csf_group_ioctl(&s,200,invoke,&m,11,GROUP_CREATE,a)==-28,27);
 csf_group_forget_fd(&s,11);init(a);T(!csf_group_ioctl(&s,300,invoke,&m,11,GROUP_CREATE,a),28);
 return 0;
}
