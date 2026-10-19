// SPDX-License-Identifier: MIT
// Candidate adapter for the EXACT factory HWC ABI, not a generic DrmHwc2 ABI.
// Selective semantic backport of Rockchip de31070c66c61ee2bd90c42c6877d26c4c5d7c24:
// query and switch only modes reported through sf_modes_. No count change.
// Boot-latched independent mode_cache switch in hwc_bridge.cpp. Installer pins
// the factory hash; the bridge checks symbols/import slots. Not release-verified.
// Caller must retain the HWC's serialization.
#ifndef RK_HDR_MODE_CACHE_H
#define RK_HDR_MODE_CACHE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef struct {
    int (*original_attribute)(void*,uint32_t,int,int*);
    int (*original_active)(void*,uint32_t);
    uint32_t (*id)(const void*),(*width)(const void*),(*height)(const void*);
    float (*refresh)(const void*);
    uint32_t (*mm_width)(const void*),(*mm_height)(const void*);
    void (*best)(void*,const void*),(*current)(void*,const void*);
    int (*update)(void*,int);
    void (*display_info)(void*,int);
} rk_mode_cache_ops;
static inline void *rk_mode_ptr(const void *p,size_t o){void *v;memcpy(&v,(const char*)p+o,sizeof(v));return v;}
static inline const void *rk_mode_find(const rk_mode_cache_ops *o,const void *d,uint32_t id){
    uintptr_t b=(uintptr_t)rk_mode_ptr(d,0x208),e=(uintptr_t)rk_mode_ptr(d,0x210),cap=(uintptr_t)rk_mode_ptr(d,0x218);
    if(!b || b%8 || e<b || cap<e || (e-b)%0x60 || (cap-b)%0x60 || (cap-b)/0x60>4096)return NULL;
    for(uintptr_t p=b;p<e;p+=0x60)if(o->id((const void*)p)==id)return (const void*)p;
    return NULL;
}
static inline int rk_mode_attribute(const rk_mode_cache_ops *o,void *d,uint32_t id,int a,int *v){
    if(!((const unsigned char*)d)[0x344])return o->original_attribute(d,id,a,v);
    if(!v)return 4; // HWC2::Error::BadParameter
    const void *m=rk_mode_find(o,d,id);if(!m)return 1;
    const void *c=rk_mode_ptr(d,0x1f8);
    uint32_t w=o->mm_width(c),h=o->mm_height(c);
    switch(a){
    case 1:*v=o->width(m);break;
    case 2:*v=o->height(m);break;
    case 3:{float r=o->refresh(m);if(!(r>0))return 1;*v=(int32_t)(1000000000.0f/r);break;}
    case 4:*v=w?(o->width(m)*25400U)/w:-1;break;
    case 5:*v=h?(o->height(m)*25400U)/h:-1;break;
    default:*v=-1;return 1;
    }
    return 0;
}
static inline int rk_mode_active(const rk_mode_cache_ops *o,void *d,uint32_t id){
    if(!((const unsigned char*)d)[0x344])return o->original_active(d,id);
    const void *m=rk_mode_find(o,d,id);if(!m)return 1;
    void *c=rk_mode_ptr(d,0x1f8);
    o->best(c,m);o->current(c,m);
    uint32_t w=o->width(m),h=o->height(m);char *p=(char*)d;
    memcpy(p+0x35c,&w,4);memcpy(p+0x360,&h,4);
    int32_t frame[]={0,0,(int32_t)w,(int32_t)h};float crop[]={0,0,(float)w,(float)h};
    memcpy(p+0x2a0,frame,sizeof(frame));memcpy(p+0x2b4,crop,sizeof(crop));
    int handle;memcpy(&handle,p+0x220,4);
    // Preserve the factory call sequence and return semantics. Changing DRM
    // commit error handling belongs to a separate, live-verified fix.
    o->update(rk_mode_ptr(d,8),handle);o->display_info(c,handle);
    return 0;
}
#endif
