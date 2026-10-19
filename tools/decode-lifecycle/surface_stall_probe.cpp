// SPDX-License-Identifier: MIT
// Stop a real GPU-only Surface decoder while codec outputs are held by client.
// This bounds the test process, not the decoder's legitimate pause duration.
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkImageReader.h>
#include <android/hardware_buffer.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
static double now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
int main(int argc,char **argv){
 if(argc!=3)return 2;setbuf(stdout,nullptr);int rounds=atoi(argv[2]);if(rounds<1||rounds>30)return 2;
 for(int r=0;r<rounds;r++){
  int fd=open(argv[1],O_RDONLY);struct stat st{};if(fd<0||fstat(fd,&st))return 3;
  auto *ex=AMediaExtractor_new();if(AMediaExtractor_setDataSourceFd(ex,fd,0,st.st_size))return 4;
  AMediaFormat *format=nullptr;const char *mime=nullptr;
  for(size_t t=0;t<AMediaExtractor_getTrackCount(ex);t++){auto *f=AMediaExtractor_getTrackFormat(ex,t);if(AMediaFormat_getString(f,"mime",&mime)&&!strncmp(mime,"video/",6)){format=f;AMediaExtractor_selectTrack(ex,t);break;}AMediaFormat_delete(f);}
  if(!format)return 5;int32_t width=0,height=0;AMediaFormat_getInt32(format,"width",&width);AMediaFormat_getInt32(format,"height",&height);
  AImageReader *reader=nullptr;ANativeWindow *window=nullptr;
  if(AImageReader_newWithUsage(width,height,AIMAGE_FORMAT_PRIVATE,AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE,2,&reader)||AImageReader_getWindow(reader,&window))return 6;
  auto *codec=AMediaCodec_createDecoderByType(mime);if(!codec)return 7;
  if(AMediaCodec_configure(codec,format,window,nullptr,0)||AMediaCodec_start(codec))return 8;
  std::atomic<bool> stopping{false};std::atomic<int> packets{0},outputs{0},errors{0};
  std::thread feeder([&]{bool inEos=false;while(!stopping){
   if(!inEos){ssize_t ix=AMediaCodec_dequeueInputBuffer(codec,1000);if(ix>=0&&!stopping){size_t cap=0;auto *data=AMediaCodec_getInputBuffer(codec,ix,&cap);auto n=AMediaExtractor_readSampleData(ex,data,cap);auto pts=AMediaExtractor_getSampleTime(ex);inEos=n<0;
    if(AMediaCodec_queueInputBuffer(codec,ix,0,inEos?0:n,inEos?0:pts,inEos?AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM:0)&&!stopping)errors++;else if(!inEos){packets++;AMediaExtractor_advance(ex);}
   }}
   AMediaCodecBufferInfo info{};auto ix=AMediaCodec_dequeueOutputBuffer(codec,&info,1000);
   // Retain every dequeued output index until stop. ImageReader alone may drop
   // queued images asynchronously, so simply not acquiring images is no proof
   // of exhausted decoder buffers. Stop legitimately releases held outputs.
   if(ix>=0&&!stopping)outputs++;
  }});
  double start=now();while(now()-start<3&&outputs<6)usleep(5000);
  // Leave the actual decoded output buffers held, then request stop.
  usleep(500000);stopping=true;int before=outputs;double begin=now();auto status=AMediaCodec_stop(codec);double stopMs=(now()-begin)*1000;feeder.join();
  AMediaCodec_delete(codec);AImageReader_delete(reader);AMediaFormat_delete(format);AMediaExtractor_delete(ex);close(fd);
  bool pass=!status&&before>=2&&!errors;
  printf("ROUND %d pass=%d packets=%d outputs=%d errors=%d stop_ms=%.3f\n",r,pass,int(packets),before,int(errors),stopMs);if(!pass)return 10;
 }
 puts("PASS stalled Surface stop rounds complete");return 0;
}
