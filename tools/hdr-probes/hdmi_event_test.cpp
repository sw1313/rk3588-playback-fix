// SPDX-License-Identifier: MIT
#include "../../magisk-hdmi-event/src/event_guard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <algorithm>
#include <dlfcn.h>
using namespace rkhdmi;
static int checks=0;
static void check(bool ok){++checks;if(!ok){fprintf(stderr,"FAIL assertion %d\n",checks);exit(1);}}
static std::string event(int state,const char *path="/devices/platform/fde80000.hdmi/extcon/extcon5"){
 std::string s="change@";s+=path;s+='\0';for(auto f:{std::string("ACTION=change"),std::string("SUBSYSTEM=extcon"),std::string("NAME=fde80000.hdmi"),std::string("DEVPATH=")+path,std::string("STATE=HDMI=")+std::to_string(state)}){s+=f;s+='\0';}return s;
}
struct Packet{int64_t time;std::string data;};
struct Mock {
 int64_t t=0,disconnectAt=-1,unreadableAt=-1;size_t index=0;int merged=0;std::vector<Packet> packets;
 int read(char*b,int n,int timeout){
  if(index>=packets.size()){if(timeout<0)return -1;t+=timeout;return 0;}
  auto &p=packets[index];if(timeout>=0&&p.time>t+timeout){t+=timeout;return 0;}t=std::max(t,p.time);if((int)p.data.size()>n)return -1;memcpy(b,p.data.data(),p.data.size());index++;return p.data.size();
 }
 int physical(){return unreadableAt>=0&&t>=unreadableAt?-1:disconnectAt>=0&&t>=disconnectAt?0:1;}
 int64_t nowMs(){return t;}void coalesced(){merged++;}
};
static std::string nextEvent(State&s,Mock&m){char b[capacity];int n=next(s,b,sizeof(b),m);check(n>0);return {b,(size_t)n};}
int main(int argc,char**argv){
 auto off=event(0),on=event(1),other=event(0,"/other/extcon");
 check(hdmiState(off.data(),off.size())==0);check(hdmiState(on.data(),on.size())==1);check(hdmiState(other.data(),other.size())==-1);
 check(hdmiState(off.data(),off.size()-1)==-1);
 for(int delay:{1,225,250,255,399}){State s{};Mock m;m.packets={{0,off},{delay,on}};check(nextEvent(s,m)==on);check(m.merged==1&&s.bytes==0&&m.t==delay);}
 {State s{};Mock m;m.packets={{0,off}};check(nextEvent(s,m)==off);check(m.t==400&&m.merged==0);}
 {State s{};Mock m;m.disconnectAt=0;m.packets={{0,off},{20,on}};check(nextEvent(s,m)==off);check(m.t==0&&m.merged==0);check(nextEvent(s,m)==on);}
 {State s{};Mock m;m.disconnectAt=50;m.packets={{0,off},{200,on}};check(nextEvent(s,m)==off);check(m.t==50);check(nextEvent(s,m)==on);check(m.merged==0);}
 {State s{};Mock m;m.unreadableAt=20;m.packets={{0,off},{250,on}};check(nextEvent(s,m)==off);check(m.t==20&&m.merged==0);}
 {State s{};Mock m;m.packets={{0,off},{20,other},{250,on}};check(nextEvent(s,m)==other);check(m.t==20&&s.bytes==(int)off.size());check(nextEvent(s,m)==on);check(m.merged==1);}
 {State s{};Mock m;m.packets={{0,off},{100,off},{200,off},{300,off},{450,on}};check(nextEvent(s,m)==off);check(m.t==400&&m.merged==0);check(nextEvent(s,m)==on);}
 // Status/timeout can change while poll/read is completing: preserve order.
 {State s{};Mock m;m.disconnectAt=5;m.packets={{0,off},{7,on}};check(nextEvent(s,m)==off);check(s.readyBytes==(int)on.size());check(nextEvent(s,m)==on);check(m.merged==0);}
 {State s{};Mock m;m.packets={{0,other}};check(nextEvent(s,m)==other);check(m.t==0&&m.merged==0);}
 if(argc==3){void*shim=dlopen(argv[2],RTLD_NOW|RTLD_LOCAL);if(!shim){fprintf(stderr,"shim %s\n",dlerror());return 2;}void*h=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!h){fprintf(stderr,"runtime %s\n",dlerror());return 2;}auto attach=(int(*)(void*))dlsym(shim,"rk_hdmi_event_attach");check(attach&&attach(h)==0);}
 printf("PASS HDMI transient event cases %d; real disconnect, unknown status, timeout, unrelated events and packet order preserved\n",checks);
}
