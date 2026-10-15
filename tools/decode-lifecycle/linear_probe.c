// SPDX-License-Identifier: Apache-2.0
// Explicit non-Surface MediaCodec lifecycle and buffer validation probe.
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaFormat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
int main(int argc,char**argv){
 if(argc<5){puts("usage: linear_probe FILE CODEC MODE FRAMES [DUMP]");return 2;}
 setbuf(stdout,NULL);int limit=atoi(argv[4]),mode=atoi(argv[3]);
 int fd=open(argv[1],O_RDONLY);struct stat st;if(fd<0||fstat(fd,&st))return 3;
 AMediaExtractor*e=AMediaExtractor_new();if(AMediaExtractor_setDataSourceFd(e,fd,0,st.st_size))return 4;
 AMediaFormat*f=NULL;const char*mime=NULL;int selected=0;
 for(size_t i=0;i<AMediaExtractor_getTrackCount(e);i++){
  AMediaFormat*t=AMediaExtractor_getTrackFormat(e,i);
  if(AMediaFormat_getString(t,"mime",&mime)&&!strncmp(mime,"video/",6)){f=t;AMediaExtractor_selectTrack(e,i);selected=1;break;}AMediaFormat_delete(t);
 }
 if(!selected)return 5;
 AMediaFormat_setInt32(f,"color-format",0x7f420888);
 printf("INPUT %s\n",AMediaFormat_toString(f));
 AMediaCodec*c=AMediaCodec_createCodecByName(argv[2]);if(!c)return 6;
 int rc=AMediaCodec_configure(c,f,NULL,NULL,0);printf("CONFIG %d\n",rc);if(rc)return 7;
 rc=AMediaCodec_start(c);printf("START %d\n",rc);if(rc)return 8;
 FILE*dump=argc>5?fopen(argv[5],"wb"):NULL;
 int queued=0,frames=0,inEos=0,eos=0,errors=0,flushes=0;unsigned long long total=0;double begin=now(),deadline=begin+(mode==4?3:25);
 while(now()<deadline&&!eos){
  if(!inEos){ssize_t ix=AMediaCodec_dequeueInputBuffer(c,1000);
   if(ix>=0){size_t cap=0;uint8_t*b=AMediaCodec_getInputBuffer(c,ix,&cap);
    ssize_t n=AMediaExtractor_readSampleData(e,b,cap);int64_t pts=AMediaExtractor_getSampleTime(e);
    if(n<0||(mode==0&&queued>=limit)){n=0;inEos=1;}
    rc=AMediaCodec_queueInputBuffer(c,ix,0,n,inEos?queued*33333:pts,inEos?AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM:0);
    if(rc){printf("QUEUE_ERROR %d\n",rc);errors++;break;}
    if(!inEos){queued++;AMediaExtractor_advance(e);}
    if(mode==1&&queued>=limit){printf("EARLY_STOP queued=%d frames=%d\n",queued,frames);break;}
   }else if(ix< -1){printf("INPUT_ERROR %zd\n",ix);errors++;break;}
  }
  AMediaCodecBufferInfo info={0};ssize_t ix=AMediaCodec_dequeueOutputBuffer(c,&info,1000);
  if(ix==AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED){AMediaFormat*o=AMediaCodec_getOutputFormat(c);printf("OUTPUT %s\n",AMediaFormat_toString(o));AMediaFormat_delete(o);}
  else if(ix>=0){
   if(info.size){size_t cap=0;uint8_t*b=AMediaCodec_getOutputBuffer(c,ix,&cap);
    if(!b||info.offset<0||info.size<0||(size_t)info.offset>cap||(size_t)info.size>cap-info.offset){puts("INVALID_OUTPUT_BOUNDS");errors++;break;}
    if(dump&&frames==0){fwrite(b+info.offset,1,info.size,dump);printf("DUMP bytes=%d capacity=%zu offset=%d\n",info.size,cap,info.offset);}
    total+=info.size;frames++;
   }
   eos=!!(info.flags&AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
   if(mode==2&&frames>=limit){printf("HELD_STOP queued=%d frames=%d held=%zd\n",queued,frames,ix);break;}
   if(mode==4)continue;
   rc=AMediaCodec_releaseOutputBuffer(c,ix,0);if(rc){printf("RELEASE_ERROR %d\n",rc);errors++;break;}
   if(mode==3&&frames>=limit&&!flushes){rc=AMediaCodec_flush(c);printf("FLUSH %d\n",rc);if(rc){errors++;break;}AMediaExtractor_seekTo(e,0,AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC);flushes++;queued=0;inEos=0;}
   else if(mode==3&&flushes&&frames>=limit*2)break;
  }else if(ix< -3){printf("OUTPUT_ERROR %zd\n",ix);errors++;break;}
 }
 double elapsed=now()-begin;printf("BEFORE_STOP queued=%d frames=%d eos=%d errors=%d elapsed=%.3f bytes=%llu\n",queued,frames,eos,errors,elapsed,total);
 double stopping=now();rc=AMediaCodec_stop(c);printf("STOP %d seconds=%.3f\n",rc,now()-stopping);if(rc)errors++;
 rc=AMediaCodec_delete(c);printf("DELETE %d\n",rc);if(rc)errors++;
 AMediaFormat_delete(f);AMediaExtractor_delete(e);close(fd);if(dump)fclose(dump);
 int pass=!errors&&frames>0&&(mode!=0||eos);
 printf("RESULT pass=%d queued=%d frames=%d eos=%d flushes=%d elapsed=%.3f\n",pass,queued,frames,eos,flushes,elapsed);
 return pass?0:10;
}
