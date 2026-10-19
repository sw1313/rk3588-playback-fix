// SPDX-License-Identifier: MIT
// Preserve real hotplug. Coalesce only a bounded HDMI audio reset while the
// independent DRM connector remains physically connected; never edit packets.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace rkhdmi {
constexpr size_t capacity=4096;
constexpr int graceMs=400;
inline bool field(const char *p,size_t n,const char *key) {
 const size_t k=strlen(key);size_t i=0;
 while(i<n){const void *end=memchr(p+i,0,n-i);if(!end)return false;
  size_t len=static_cast<const char*>(end)-(p+i);
  if(len==k && !memcmp(p+i,key,k))return true;i+=len+1;
 }return false;
}
inline int hdmiState(const char *p,size_t n) {
 if(!field(p,n,"SUBSYSTEM=extcon") || !field(p,n,"NAME=fde80000.hdmi") ||
    !field(p,n,"DEVPATH=/devices/platform/fde80000.hdmi/extcon/extcon5"))return -1;
 bool zero=field(p,n,"STATE=HDMI=0"),one=field(p,n,"STATE=HDMI=1");
 return zero==one?-1:(one?1:0);
}
struct State {char pending[capacity],ready[capacity];int bytes=0,readyBytes=0;int64_t deadline=0;};
// Ops: read(buffer,length,timeoutMs), physical() -> 1/0/-1, nowMs(), coalesced().
// Unrelated events return immediately even while one HDMI-off is deferred.
template<class Ops> int next(State &s,char *out,int length,Ops &o) {
 if(length<=0)return o.read(out,length,-1);
 if(s.readyBytes){if(s.readyBytes>length)return -1;int n=s.readyBytes;memcpy(out,s.ready,n);s.readyBytes=0;return n;}
 if(length>static_cast<int>(capacity)){
  if(s.bytes){int n=s.bytes;memcpy(out,s.pending,n);s.bytes=0;return n;}return o.read(out,length,-1);
 }
 for(;;){
  if(s.bytes){
   if(s.bytes>length)return -1; // No truncation or invented replacement event.
   if(o.physical()!=1 || o.nowMs()>=s.deadline){int n=s.bytes;memcpy(out,s.pending,n);s.bytes=0;return n;}
  }
  int n=o.read(out,length,s.bytes?10:-1);
  if(n<0)return n;if(!n){if(!s.bytes)return n;continue;}
  int state=hdmiState(out,static_cast<size_t>(n));
  if(s.bytes){
   if(o.physical()!=1 || o.nowMs()>=s.deadline){
    memcpy(s.ready,out,n);s.readyBytes=n;int old=s.bytes;memcpy(out,s.pending,old);s.bytes=0;return old;
   }
   if(state==1 && o.physical()==1 && o.nowMs()<s.deadline){s.bytes=0;o.coalesced();return n;}
   if(state==0)continue; // Repeated identical off cannot extend the deadline.
   return n;
  }
  if(state==0 && n<=static_cast<int>(capacity) && o.physical()==1){
   memcpy(s.pending,out,n);s.bytes=n;s.deadline=o.nowMs()+graceMs;continue;
  }
  return n;
 }
}
}
