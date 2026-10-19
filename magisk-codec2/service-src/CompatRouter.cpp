// SPDX-License-Identifier: MIT
#include "CompatRouter.h"
#include <C2Config.h>
#include <cutils/properties.h>
#include <utils/Log.h>
#include <map>
#include <mutex>
#include <set>
#include <algorithm>
static std::mutex ownerMutex;
static std::map<const C2Component*,std::weak_ptr<const C2Component>> owners;
std::shared_ptr<const C2Component> CompatPoolOwner(std::shared_ptr<const C2Component> c){
 std::lock_guard<std::mutex>g(ownerMutex);auto i=owners.find(c.get());if(i!=owners.end()){auto p=i->second.lock();if(p)return p;owners.erase(i);}return c;
}
struct State {
 std::mutex control,notifications;
 std::shared_ptr<C2Component> stock,quality;
 std::shared_ptr<C2Component::Listener> listener;
 std::weak_ptr<C2Component> owner;
 std::set<uint32_t> sk,qk;
 bool selected=false,started=false,queued=false,released=false,surface=false,p010=false;
 const bool upgradeSurface=property_get_bool("debug.rkcodec2.surface_upgrade",true);
 // Native Surface rollback must not disable the existing API32 P010 capability
 // and client gates which also consume surface_upgrade.
 const bool upgradeNativeSurface=property_get_bool("debug.rkcodec2.native_surface_upgrade",true);
 const bool upgradeBuffer=property_get_bool("debug.rkcodec2.buffer_upgrade",false);
 bool buffer8=false;
 State(std::shared_ptr<C2Component>s,std::shared_ptr<C2Component>q):stock(s),quality(q){
  std::vector<std::shared_ptr<C2ParamDescriptor>> ps;
  stock->intf()->querySupportedParams_nb(&ps);for(auto &p:ps)sk.insert(p->index().type());ps.clear();
  quality->intf()->querySupportedParams_nb(&ps);for(auto &p:ps)qk.insert(p->index().type());
 }
 std::shared_ptr<C2Component> active()const{return selected?quality:stock;}
 const std::set<uint32_t>& keys()const{return selected?qk:sk;}
 c2_status_t choose(bool next){
  if(next==selected)return C2_OK;
  if(queued||released)return C2_BAD_STATE;
  bool running=started;
  if(running){auto rc=active()->stop();if(rc!=C2_OK)return rc;started=false;}
  bool previous=selected;selected=next;
  if(running){auto rc=active()->start();if(rc!=C2_OK){selected=previous;started=active()->start()==C2_OK;return rc;}started=true;}
  ALOGI("RKCodec2Compat route=%s surface=%d p010=%d",selected?"quality":"stock",surface,p010);
  return C2_OK;
 }
};
struct Relay : C2Component::Listener {
 std::weak_ptr<State> weak;
 explicit Relay(std::shared_ptr<State>s):weak(s){}
 void onWorkDone_nb(std::weak_ptr<C2Component>,std::list<std::unique_ptr<C2Work>>w)override{
  auto s=weak.lock();if(!s)return;std::shared_ptr<C2Component::Listener>l;std::weak_ptr<C2Component>o;
  {std::lock_guard<std::mutex>g(s->notifications);l=s->listener;o=s->owner;}if(l)l->onWorkDone_nb(o,std::move(w));
 }
 void onTripped_nb(std::weak_ptr<C2Component>,std::vector<std::shared_ptr<C2SettingResult>>f)override{
  auto s=weak.lock();if(!s)return;std::shared_ptr<C2Component::Listener>l;std::weak_ptr<C2Component>o;
  {std::lock_guard<std::mutex>g(s->notifications);l=s->listener;o=s->owner;}if(l)l->onTripped_nb(o,std::move(f));
 }
 void onError_nb(std::weak_ptr<C2Component>,uint32_t e)override{
  auto s=weak.lock();if(!s)return;std::shared_ptr<C2Component::Listener>l;std::weak_ptr<C2Component>o;
  {std::lock_guard<std::mutex>g(s->notifications);l=s->listener;o=s->owner;}if(l)l->onError_nb(o,e);
 }
};
struct Interface : C2ComponentInterface {
 std::shared_ptr<State>s;
 explicit Interface(std::shared_ptr<State>x):s(x){}
 C2String getName()const override{return s->stock->intf()->getName();}
 c2_node_id_t getId()const override{return s->stock->intf()->getId();}
 c2_status_t query_vb(const std::vector<C2Param*>&stack,const std::vector<C2Param::Index>&indices,c2_blocking_t block,std::vector<std::unique_ptr<C2Param>>*heap)const override{
  std::lock_guard<std::mutex>g(s->control);std::vector<C2Param*> a,b;std::vector<C2Param::Index>ia,ib;
  for(auto*p:stack)(s->keys().count(p->type().type())?a:b).push_back(p);
  for(auto&i:indices)(s->keys().count(i.type())?ia:ib).push_back(i);
  auto active=s->active()->intf(),other=(s->selected?s->stock:s->quality)->intf();
  auto r=active->query_vb(a,{},block,heap);auto x=other->query_vb(b,{},block,heap);if(r==C2_OK)r=x;
  // Preserve the caller's heap parameter order across the two implementations.
  for(auto &index:indices){auto which=s->keys().count(index.type())?active:other;auto q=which->query_vb({},{index},block,heap);if(r==C2_OK)r=q;}
  return r;
 }
 c2_status_t config_vb(const std::vector<C2Param*>&params,c2_blocking_t block,std::vector<std::unique_ptr<C2SettingResult>>*failures)override{
  std::lock_guard<std::mutex>g(s->control);if(s->released)return C2_BAD_STATE;
  bool oldP=s->p010,oldSurface=s->surface,oldBuffer=s->buffer8,surfaceConfigured=false;
  for(auto*p:params){
   if(auto f=C2StreamPixelFormatInfo::output::From(p))s->p010=f->value==54;
   if(auto pools=C2PortBlockPoolsTuning::output::From(p)){surfaceConfigured=true;s->surface=false;for(size_t i=0;i<pools->flexCount();i++)s->surface|=CompatSurfacePool(pools->m.values[i]);}
   if(auto profile=C2StreamProfileLevelInfo::input::From(p)){
    auto name=s->stock->intf()->getName();auto value=profile->profile;
    s->buffer8=(name.find(".hevc.")!=C2String::npos &&
            (value==C2Config::PROFILE_HEVC_MAIN || value==C2Config::PROFILE_HEVC_MAIN_STILL)) ||
        (name.find(".avc.")!=C2String::npos &&
            (value==C2Config::PROFILE_AVC_BASELINE || value==C2Config::PROFILE_AVC_CONSTRAINED_BASELINE ||
             value==C2Config::PROFILE_AVC_MAIN || value==C2Config::PROFILE_AVC_EXTENDED ||
             value==C2Config::PROFILE_AVC_HIGH || value==C2Config::PROFILE_AVC_CONSTRAINED_HIGH));
   }
  }
  bool next=s->p010||(s->surface?(s->upgradeSurface&&s->upgradeNativeSurface):(s->upgradeBuffer&&s->buffer8));
  auto rc=s->choose(next);if(rc!=C2_OK){s->p010=oldP;s->surface=oldSurface;s->buffer8=oldBuffer;return rc;}
  if(surfaceConfigured)ALOGI("RKCodec2Compat confirmed-route=%s surface=%d p010=%d native_upgrade=%d",s->selected?"quality":"stock",s->surface,s->p010,s->upgradeNativeSurface);
  std::vector<std::unique_ptr<C2Param>> copies;std::vector<C2Param*> mirror,active,extra;
  auto&otherKeys=s->selected?s->sk:s->qk;
  for(auto*p:params){copies.push_back(C2Param::Copy(*p));if(otherKeys.count(p->type().type()))mirror.push_back(copies.back().get());(s->keys().count(p->type().type())?active:extra).push_back(p);}
  auto current=s->active()->intf(),other=(s->selected?s->stock:s->quality)->intf();
  auto result=current->config_vb(active,block,failures);std::vector<std::unique_ptr<C2SettingResult>>ignored;
  other->config_vb(mirror,block,&ignored);
  if(!extra.empty()){auto x=other->config_vb(extra,block,failures);if(result==C2_OK)result=x;}
  return result;
 }
 c2_status_t createTunnel_sm(c2_node_id_t n)override{std::lock_guard<std::mutex>g(s->control);return s->active()->intf()->createTunnel_sm(n);}
 c2_status_t releaseTunnel_sm(c2_node_id_t n)override{std::lock_guard<std::mutex>g(s->control);return s->active()->intf()->releaseTunnel_sm(n);}
 c2_status_t querySupportedParams_nb(std::vector<std::shared_ptr<C2ParamDescriptor>>*p)const override{
  std::lock_guard<std::mutex>g(s->control);auto r=s->stock->intf()->querySupportedParams_nb(p);std::vector<std::shared_ptr<C2ParamDescriptor>> extra;auto q=s->quality->intf()->querySupportedParams_nb(&extra);for(auto&v:extra)if(!s->sk.count(v->index().type()))p->push_back(v);return r!=C2_OK?r:q;
 }
 c2_status_t querySupportedValues_vb(std::vector<C2FieldSupportedValuesQuery>&fields,c2_blocking_t block)const override{
  std::lock_guard<std::mutex>g(s->control);auto r=s->active()->intf()->querySupportedValues_vb(fields,block);if(r!=C2_OK&&r!=C2_BAD_INDEX)return r;
  C2StreamPixelFormatInfo::output pix(0u,0u);C2ParamField pf(&pix,&pix.value);
  for(auto&f:fields)if(f.status!=C2_OK||f.field()==pf){std::vector<C2FieldSupportedValuesQuery>one{f};auto other=(f.field()==pf?s->quality:(s->selected?s->stock:s->quality))->intf();other->querySupportedValues_vb(one,block);f=one.front();}
  for(auto&f:fields)if(f.status!=C2_OK)return f.status;return C2_OK;
 }
};
struct Component : C2Component {
 std::shared_ptr<State>s;std::shared_ptr<C2ComponentInterface>i;
 explicit Component(std::shared_ptr<State>x):s(x),i(std::make_shared<Interface>(x)){}
 c2_status_t setListener_vb(const std::shared_ptr<Listener>&l,c2_blocking_t)override{std::lock_guard<std::mutex>g(s->control);if(s->released||(s->started&&l))return C2_BAD_STATE;std::lock_guard<std::mutex>n(s->notifications);s->listener=l;return C2_OK;}
 c2_status_t queue_nb(std::list<std::unique_ptr<C2Work>>*w)override{std::lock_guard<std::mutex>g(s->control);s->queued=true;return s->active()->queue_nb(w);}
 c2_status_t announce_nb(const std::vector<C2WorkOutline>&w)override{std::lock_guard<std::mutex>g(s->control);return s->active()->announce_nb(w);}
 c2_status_t flush_sm(flush_mode_t m,std::list<std::unique_ptr<C2Work>>*w)override{std::lock_guard<std::mutex>g(s->control);return s->active()->flush_sm(m,w);}
 c2_status_t drain_nb(drain_mode_t m)override{std::lock_guard<std::mutex>g(s->control);return s->active()->drain_nb(m);}
 c2_status_t start()override{std::lock_guard<std::mutex>g(s->control);auto r=s->active()->start();if(r==C2_OK)s->started=true;return r;}
 c2_status_t stop()override{std::lock_guard<std::mutex>g(s->control);auto r=s->active()->stop();if(r==C2_OK){s->started=false;s->queued=false;}return r;}
 c2_status_t reset()override{std::lock_guard<std::mutex>g(s->control);auto r=s->stock->reset(),q=s->quality->reset();s->started=s->queued=s->p010=s->surface=s->selected=s->buffer8=false;return r!=C2_OK?r:q;}
 c2_status_t release()override{std::lock_guard<std::mutex>g(s->control);s->released=true;auto r=s->stock->release(),q=s->quality->release();s->started=false;return r!=C2_OK?r:q;}
 std::shared_ptr<C2ComponentInterface>intf()override{return i;}
};
c2_status_t MakeCompatRouter(const std::shared_ptr<C2Component>&stock,const std::shared_ptr<C2Component>&quality,std::shared_ptr<C2Component>*out){
 auto state=std::make_shared<State>(stock,quality);auto c=std::make_shared<Component>(state);state->owner=c;auto relay=std::make_shared<Relay>(state);
 auto r=stock->setListener_vb(relay,C2_MAY_BLOCK);if(r!=C2_OK)return r;r=quality->setListener_vb(relay,C2_MAY_BLOCK);if(r!=C2_OK)return r;
 {std::lock_guard<std::mutex>g(ownerMutex);for(auto it=owners.begin();it!=owners.end();)if(it->second.expired())it=owners.erase(it);else++it;owners[stock.get()]=c;owners[quality.get()]=c;}
 *out=c;return C2_OK;
}
