// SPDX-License-Identifier: MIT
// Runs an existing vendor MediaCodec against a synthetic MP4 fixture.
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

static double now(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
    return t.tv_sec+t.tv_nsec/1e9;
}

int main(int argc,char **argv) {
    setbuf(stdout,NULL);
    if(argc!=4) { fprintf(stderr,"usage: mediacodec_probe CLIP CODEC EXPECTED_FRAMES\n");return 2; }
    AMediaExtractor *ex=AMediaExtractor_new();
    int fd=open(argv[1],O_RDONLY|O_CLOEXEC);struct stat st;
    if(fd<0 || fstat(fd,&st))return 3;
    media_status_t source_rc=AMediaExtractor_setDataSourceFd(ex,fd,0,st.st_size);close(fd);
    if(source_rc!=AMEDIA_OK) { printf("SOURCE_FAIL %d\n",source_rc);return 3; }
    AMediaFormat *format=AMediaExtractor_getTrackFormat(ex,0);
    if(!format || AMediaExtractor_selectTrack(ex,0)!=AMEDIA_OK)return 4;
    printf("INPUT %s\n",AMediaFormat_toString(format));
    AMediaCodec *codec=AMediaCodec_createCodecByName(argv[2]);
    if(!codec)return 5;
    media_status_t rc=AMediaCodec_configure(codec,format,NULL,NULL,0);
    printf("CONFIGURE %d\n",rc);if(rc!=AMEDIA_OK)return 6;
    if(AMediaCodec_start(codec)!=AMEDIA_OK)return 7;
    bool input_eos=false,output_eos=false;int frames=0;
    uint64_t checksum=14695981039346656037ULL,total=0,nonzero=0;
    double deadline=now()+30;
    int result=0;
    while(!output_eos && now()<deadline) {
        if(!input_eos) {
            ssize_t index=AMediaCodec_dequeueInputBuffer(codec,1000);
            if(index>=0) {
                size_t capacity=0;uint8_t *buffer=AMediaCodec_getInputBuffer(codec,index,&capacity);
                if(!buffer || !capacity) { result=8;break; }
                ssize_t size=AMediaExtractor_readSampleData(ex,buffer,capacity);
                int64_t pts=AMediaExtractor_getSampleTime(ex);
                uint32_t flags=0;
                if(size<0) { size=0;pts=0;flags=AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM;input_eos=true; }
                if((size_t)size>capacity) { result=9;break; }
                rc=AMediaCodec_queueInputBuffer(codec,index,0,size,pts,flags);
                if(rc!=AMEDIA_OK) { result=10;break; }
                if(!input_eos)AMediaExtractor_advance(ex);
            }
        }
        AMediaCodecBufferInfo info;
        ssize_t index=AMediaCodec_dequeueOutputBuffer(codec,&info,1000);
        if(index==AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            AMediaFormat *output=AMediaCodec_getOutputFormat(codec);
            printf("OUTPUT %s\n",AMediaFormat_toString(output));AMediaFormat_delete(output);
        } else if(index>=0) {
            size_t capacity=0;uint8_t *data=AMediaCodec_getOutputBuffer(codec,index,&capacity);
            if(info.size>0) {
                if(!data || info.offset<0 || info.size<0 || (size_t)info.offset+(size_t)info.size>capacity) {
                    AMediaCodec_releaseOutputBuffer(codec,index,false);result=11;break;
                }
                for(int i=0;i<info.size;++i) { nonzero+=data[info.offset+i]!=0;checksum^=data[info.offset+i];checksum*=1099511628211ULL; }
                ++frames;total+=info.size;
            }
            output_eos=(info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM)!=0;
            AMediaCodec_releaseOutputBuffer(codec,index,false);
        } else if(index!=AMEDIACODEC_INFO_TRY_AGAIN_LATER && index!=AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
            printf("DEQUEUE_ERROR %zd\n",index);result=12;break;
        }
    }
    printf("RESULT frames=%d expected=%d eos=%d bytes=%llu nonzero=%llu fnv64=%016llx\n",frames,atoi(argv[3]),output_eos,
        (unsigned long long)total,(unsigned long long)nonzero,(unsigned long long)checksum);
    if(!output_eos || frames!=atoi(argv[3]))result=result ? result : 13;
    if(frames>0 && nonzero==0)result=result ? result : 14;
    AMediaCodec_stop(codec);AMediaCodec_delete(codec);AMediaFormat_delete(format);AMediaExtractor_delete(ex);
    return result;
}
