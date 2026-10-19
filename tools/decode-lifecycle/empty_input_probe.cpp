// SPDX-License-Identifier: MIT
// Feed ordinary empty non-EOS works between real compressed packets. Empty
// works must release input ownership without creating or dropping video frames.
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkImageReader.h>
#include <android/hardware_buffer.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <chrono>
static double now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
int main(int argc,char **argv){
 if(argc!=4&&argc!=5)return 2;setbuf(stdout,nullptr);int count=atoi(argv[2]),empties=atoi(argv[3]),flushAfter=argc==5?atoi(argv[4]):0;if(count<1||count>300||empties<0||empties>300||flushAfter<0||flushAfter>=count)return 2;
 int fd=open(argv[1],O_RDONLY);struct stat st{};if(fd<0||fstat(fd,&st))return 3;
 auto *ex=AMediaExtractor_new();if(AMediaExtractor_setDataSourceFd(ex,fd,0,st.st_size))return 4;
 AMediaFormat *format=nullptr;const char *mime=nullptr;
 for(size_t t=0;t<AMediaExtractor_getTrackCount(ex);t++){auto *f=AMediaExtractor_getTrackFormat(ex,t);if(AMediaFormat_getString(f,"mime",&mime)&&!strncmp(mime,"video/",6)){format=f;AMediaExtractor_selectTrack(ex,t);break;}AMediaFormat_delete(f);}
 if(!format)return 5;int32_t width=0,height=0;AMediaFormat_getInt32(format,"width",&width);AMediaFormat_getInt32(format,"height",&height);
 AImageReader *reader=nullptr;ANativeWindow *window=nullptr;
 if(AImageReader_newWithUsage(width,height,AIMAGE_FORMAT_PRIVATE,AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE,8,&reader)||AImageReader_getWindow(reader,&window))return 6;
 auto *codec=AMediaCodec_createDecoderByType(mime);if(!codec)return 7;
 if(AMediaCodec_configure(codec,format,window,nullptr,0)||AMediaCodec_start(codec))return 8;
 int packets=0,empty=0,outputs=0,images=0,errors=0,flushes=0,beforeFlush=0;bool inEos=false,outEos=false;
 double begin=now();
 while(now()-begin<12&&!outEos){
  if(!inEos){ssize_t ix=AMediaCodec_dequeueInputBuffer(codec,1000);if(ix>=0){
   if(empty<empties){if(AMediaCodec_queueInputBuffer(codec,ix,0,0,0,0))errors++;else empty++;}
   else {size_t cap=0;auto *data=AMediaCodec_getInputBuffer(codec,ix,&cap);ssize_t n=packets<count?AMediaExtractor_readSampleData(ex,data,cap):-1;int64_t pts=AMediaExtractor_getSampleTime(ex);inEos=n<0;
    if(AMediaCodec_queueInputBuffer(codec,ix,0,inEos?0:size_t(n),inEos?0:pts,inEos?AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM:0))errors++;
    else if(!inEos){++packets;AMediaExtractor_advance(ex);}
   }
  }}
  AMediaCodecBufferInfo info{};ssize_t ix=AMediaCodec_dequeueOutputBuffer(codec,&info,1000);
  if(ix>=0){bool eos=info.flags&AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM;bool frame=!(info.flags&AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG)&&(!eos||info.size>0);if(frame)outputs++;
   if(AMediaCodec_releaseOutputBuffer(codec,ix,frame))errors++;outEos=eos;
  }
  // Synchronous image acquisition retains the platform's fence wait.
  AImage *image=nullptr;while(AImageReader_acquireNextImage(reader,&image)==AMEDIA_OK){++images;AImage_delete(image);image=nullptr;}
  if(flushAfter&&!flushes&&outputs>=flushAfter){
   if(AMediaCodec_flush(codec)||AMediaExtractor_seekTo(ex,0,AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC)){errors++;break;}
   beforeFlush=outputs;outputs=0;packets=0;empty=0;inEos=false;outEos=false;flushes++;
  }
  if(errors)break;
 }
 double elapsed=now()-begin;auto stopBegin=now();auto stop=AMediaCodec_stop(codec);double stopMs=(now()-stopBegin)*1000;
 AMediaCodec_delete(codec);AImageReader_delete(reader);AMediaFormat_delete(format);AMediaExtractor_delete(ex);close(fd);
 bool pass=empty==empties&&packets==count&&outputs==count&&outEos&&!errors&&!stop&&(!flushAfter||flushes==1);
 printf("RESULT pass=%d empty=%d packets=%d outputs=%d images=%d eos=%d errors=%d flushes=%d before_flush_outputs=%d elapsed=%.3f stop_ms=%.3f\n",pass,empty,packets,outputs,images,outEos,errors,flushes,beforeFlush,elapsed,stopMs);return pass?0:10;
}
