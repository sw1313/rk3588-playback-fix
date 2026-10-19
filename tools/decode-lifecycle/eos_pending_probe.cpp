// SPDX-License-Identifier: MIT
// Direct C2 listener census: non-picture HEVC AUD packets must not leave work
// ownership outstanding after true decoder EOS. No fake image completions.
#include <C2ComponentFactory.h>
#include <C2Config.h>
#include <C2PlatformSupport.h>
#include <media/NdkMediaExtractor.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <vector>
#include <algorithm>
using namespace android;
struct Sink : C2Component::Listener {
 std::mutex mutex;std::condition_variable ready;
 std::map<uint64_t,int> done;std::vector<int64_t> timestamps;
 bool eos=false;int errors=0;size_t frames=0;int aud=0;bool reenter=false;int reentered=0;
 void onTripped_nb(std::weak_ptr<C2Component>,std::vector<std::shared_ptr<C2SettingResult>>) override {std::lock_guard<std::mutex> l(mutex);errors++;ready.notify_all();}
 void onError_nb(std::weak_ptr<C2Component>,uint32_t) override {std::lock_guard<std::mutex> l(mutex);errors++;ready.notify_all();}
 void onWorkDone_nb(std::weak_ptr<C2Component> component,std::list<std::unique_ptr<C2Work>> works) override {
  std::lock_guard<std::mutex> l(mutex);
  for(auto &w:works){
   if(w->result!=C2_OK||w->workletsProcessed!=1||w->worklets.size()!=1){errors++;continue;}
   auto &out=w->worklets.front()->output;
   auto id=w->input.ordinal.frameIndex.peeku();
   if(reenter&&id>=1&&id<=uint64_t(aud)&&out.buffers.empty()){
    std::list<std::unique_ptr<C2Work>> empty;auto c=component.lock();
    if(!c||c->queue_nb(&empty)!=C2_OK)errors++;else reentered++;
    if(out.ordinal.frameIndex.peeku()!=id||out.ordinal.timestamp.peekll()!=w->input.ordinal.timestamp.peekll())errors++;
   }
   if(!(out.flags&C2FrameData::FLAG_INCOMPLETE))done[w->input.ordinal.frameIndex.peeku()]++;
   for(auto &b:out.buffers){if(!b){errors++;continue;}frames++;timestamps.push_back(out.ordinal.timestamp.peekll());}
   if(out.flags&C2FrameData::FLAG_END_OF_STREAM)eos=true;
  }ready.notify_all();
 }
};
static bool annexb(const std::vector<uint8_t> &v){return v.size()>3&&v[0]==0&&v[1]==0&&(v[2]==1||(v[2]==0&&v[3]==1));}
static std::vector<uint8_t> convert(const uint8_t *p,size_t n,bool config){
 std::vector<uint8_t> in(p,p+n),out;if(annexb(in))return in;
 if(config&&n>=23&&p[0]==1){size_t x=23;for(unsigned a=0;a<p[22];a++){
  if(x+3>n)return {};x++;unsigned count=(p[x]<<8)|p[x+1];x+=2;
  for(unsigned b=0;b<count;b++){if(x+2>n)return {};size_t len=(p[x]<<8)|p[x+1];x+=2;if(x+len>n)return {};out.insert(out.end(),{0,0,0,1});out.insert(out.end(),p+x,p+x+len);x+=len;}
 }return out;}
 size_t x=0;while(x<n){if(x+4>n)return {};size_t len=(size_t(p[x])<<24)|(size_t(p[x+1])<<16)|(size_t(p[x+2])<<8)|p[x+3];x+=4;if(!len||x+len>n)return {};out.insert(out.end(),{0,0,0,1});out.insert(out.end(),p+x,p+x+len);x+=len;}return out;
}
int main(int argc,char **argv){
 if(argc!=5&&argc!=6)return 2;setbuf(stdout,nullptr);const int limit=atoi(argv[3]),aud=atoi(argv[4]);if(limit<1||limit>120||aud<0||aud>12)return 2;
 int fd=open(argv[2],O_RDONLY);struct stat st{};if(fd<0||fstat(fd,&st))return 3;
 auto *ex=AMediaExtractor_new();if(AMediaExtractor_setDataSourceFd(ex,fd,0,st.st_size))return 4;
 AMediaFormat *format=nullptr;for(size_t i=0;i<AMediaExtractor_getTrackCount(ex);i++){auto *f=AMediaExtractor_getTrackFormat(ex,i);const char *mime=nullptr;if(AMediaFormat_getString(f,"mime",&mime)&&!strcmp(mime,"video/hevc")){format=f;AMediaExtractor_selectTrack(ex,i);break;}AMediaFormat_delete(f);}if(!format)return 5;
 int32_t width=0,height=0;AMediaFormat_getInt32(format,"width",&width);AMediaFormat_getInt32(format,"height",&height);
 void *csd=nullptr;size_t csdSize=0;if(!AMediaFormat_getBuffer(format,"csd-0",&csd,&csdSize))return 6;
 void *lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!lib){printf("LOAD_FAIL %s\n",dlerror());return 7;}
 auto create=reinterpret_cast<C2ComponentFactory*(*)(std::string)>(dlsym(lib,"CreateRKCodec2Factory"));auto destroy=reinterpret_cast<void(*)(C2ComponentFactory*)>(dlsym(lib,"DestroyRKCodec2Factory"));if(!create||!destroy)return 8;
 auto factory=create("c2.rk.hevc.decoder");std::shared_ptr<C2Component> c;if(!factory||factory->createComponent(1,&c)!=C2_OK)return 9;
 auto sink=std::make_shared<Sink>();sink->aud=aud;sink->reenter=argc==6&&atoi(argv[5])==1;if(c->setListener_vb(sink,C2_MAY_BLOCK)!=C2_OK)return 10;
 C2StreamPictureSizeInfo::output size(0u,width,height);C2StreamPixelFormatInfo::output pixel(0u,0x36);std::vector<std::unique_ptr<C2SettingResult>> failures;
 if(c->intf()->config_vb({&size,&pixel},C2_MAY_BLOCK,&failures)!=C2_OK||c->start()!=C2_OK)return 11;
 std::shared_ptr<C2BlockPool> pool;if(GetCodec2BlockPool(C2BlockPool::BASIC_LINEAR,c,&pool)!=C2_OK)return 12;
 uint64_t id=0;bool failed=false;
 auto send=[&](const std::vector<uint8_t>&data,int64_t pts,C2FrameData::flags_t flags){
  std::unique_ptr<C2Work> w(new C2Work);w->worklets.emplace_back(new C2Worklet);w->workletsProcessed=0;w->input.ordinal.frameIndex=id++;w->input.ordinal.timestamp=pts;w->input.flags=flags;
  if(!data.empty()){std::shared_ptr<C2LinearBlock>b;if(pool->fetchLinearBlock(data.size(),C2MemoryUsage(C2MemoryUsage::CPU_READ,C2MemoryUsage::CPU_WRITE),&b)!=C2_OK)return false;auto view=b->map().get();if(view.error())return false;memcpy(view.data(),data.data(),data.size());w->input.buffers.push_back(C2Buffer::CreateLinearBuffer(b->share(0,data.size(),C2Fence())));}
  std::list<std::unique_ptr<C2Work>> works;works.push_back(std::move(w));return c->queue_nb(&works)==C2_OK&&works.empty();
 };
 auto header=convert(static_cast<uint8_t*>(csd),csdSize,true);if(header.empty()||!send(header,0,C2FrameData::FLAG_CODEC_CONFIG))return 13;
 for(int i=0;i<aud;i++)if(!send({0,0,0,1,0x46,0x01,0x50},0,C2FrameData::flags_t(0)))return 14;
 std::vector<uint8_t> buffer(4*1024*1024);int packets=0;std::vector<int64_t> expectedPts;
 for(;packets<limit;packets++){ssize_t n=AMediaExtractor_readSampleData(ex,buffer.data(),buffer.size());if(n<=0)break;auto packet=convert(buffer.data(),n,false);auto pts=AMediaExtractor_getSampleTime(ex);if(packet.empty()||!send(packet,pts,C2FrameData::flags_t(0))){failed=true;break;}expectedPts.push_back(pts);AMediaExtractor_advance(ex);}
 if(!failed&&!send({},0,C2FrameData::FLAG_END_OF_STREAM))failed=true;
 {std::unique_lock<std::mutex>l(sink->mutex);sink->ready.wait_for(l,std::chrono::seconds(12),[&]{return sink->eos||sink->errors;});}
 usleep(200000);
 // Snapshot before stop: stop/flush can return outstanding work and must not
 // be mistaken for successful EOS-time ownership completion.
 std::vector<uint64_t> missing;int errors=0,duplicates=0,reentered=0;size_t frames=0;bool eos=false,ptsExact=false;
 {std::lock_guard<std::mutex>l(sink->mutex);errors=sink->errors;frames=sink->frames;eos=sink->eos;reentered=sink->reentered;for(uint64_t i=0;i<id;i++){if(sink->done[i]!=1)missing.push_back(i);if(sink->done[i]>1)duplicates++;}auto actual=sink->timestamps;std::sort(actual.begin(),actual.end());std::sort(expectedPts.begin(),expectedPts.end());ptsExact=actual==expectedPts;}
 auto stopped=c->stop();auto released=c->release();c.reset();destroy(factory);AMediaFormat_delete(format);AMediaExtractor_delete(ex);close(fd);
 printf("RESULT packets=%d aud=%d inputs=%llu frames=%zu eos=%d errors=%d pts_exact=%d duplicates=%d reentered=%d stop=%d release=%d missing_before_stop=%zu ids=",packets,aud,(unsigned long long)id,frames,eos,errors,ptsExact,duplicates,reentered,int(stopped),int(released),missing.size());for(auto i:missing)printf("%llu,",(unsigned long long)i);puts("");
 return failed||packets!=limit||frames!=size_t(limit)||!eos||errors||!ptsExact||duplicates||(sink->reenter&&reentered!=aud)||stopped!=C2_OK||released!=C2_OK?15:0;
}
