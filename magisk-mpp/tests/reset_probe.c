// SPDX-License-Identifier: MIT
// Exercise consumed CSD replay across EOS/reset without resubmitting headers.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "rk_mpi.h"
#include "mpp_packet.h"
#include "mpp_frame.h"
#include "mpp_buffer.h"
static uint32_t u32(FILE *f) {
    uint8_t b[4]; if (fread(b,1,4,f)!=4) exit(2);
    return b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;
}
static double now(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
    return t.tv_sec+t.tv_nsec/1e9;
}
int main(int argc,char **argv) {
    if (argc!=6) return 2;
    setbuf(stdout,NULL);
    int coding=atoi(argv[1]),bits=atoi(argv[5]);
    FILE *f=fopen(argv[2],"rb"),*csd=fopen(argv[3],"rb"),*out=fopen(argv[4],"wb");
    if (!f||!csd||!out||u32(f)!=0x50434b52) return 3;
    unsigned w=u32(f),h=u32(f),count=u32(f);
    long start=ftell(f); fseek(csd,0,SEEK_END); long n=ftell(csd); rewind(csd);
    if (n<=0||n>1024*1024) return 4;
    void *config=malloc(n); if (fread(config,1,n,csd)!=(size_t)n) return 5;
    fclose(csd);
    MppCtx ctx=NULL; MppApi *api=NULL;
    if (mpp_create(&ctx,&api)||mpp_init(ctx,MPP_CTX_DEC,coding)) return 6;
    RK_S32 timeout=0; api->control(ctx,MPP_SET_OUTPUT_TIMEOUT,&timeout);
    MppPacket extra=NULL;
    // An empty initial extra-data packet must not dereference a missing cache.
    if (mpp_packet_init(&extra,config,0)) return 7;
    mpp_packet_set_extra_data(extra);
    if (api->decode_put_packet(ctx,extra)) return 8;
    mpp_packet_deinit(&extra);
    if (mpp_packet_init(&extra,config,n)) return 9;
    mpp_packet_set_extra_data(extra);
    if (api->decode_put_packet(ctx,extra)) return 10;
    mpp_packet_deinit(&extra);
    for (int phase=0;phase<3;phase++) {
        fseek(f,start,SEEK_SET);
        unsigned sent=0,frames=0,errors=0; int eos=0;
        MppPacket packet=NULL; void *data=NULL; double deadline=now()+15;
        while (!eos&&now()<deadline) {
            if (!packet&&sent<=count) {
                unsigned length=0,flags=0; uint64_t pts=0;
                if (sent<count) {
                    length=u32(f); flags=u32(f); pts=u32(f); pts|=(uint64_t)u32(f)<<32;
                    if (length>16*1024*1024) return 11;
                }
                data=malloc(length?length:1);
                if (fread(data,1,length,f)!=length||mpp_packet_init(&packet,data,length)) return 12;
                mpp_packet_set_pts(packet,pts);
                if (sent==count||(flags&1)) mpp_packet_set_eos(packet);
                sent++;
            }
            if (packet&&!api->decode_put_packet(ctx,packet)) {
                mpp_packet_deinit(&packet); free(data); data=NULL;
            }
            MppFrame frame=NULL; api->decode_get_frame(ctx,&frame);
            if (frame) {
                if (mpp_frame_get_info_change(frame)) {
                    if (api->control(ctx,MPP_DEC_SET_INFO_CHANGE_READY,NULL)) return 13;
                } else if (mpp_frame_get_buffer(frame)) {
                    if (mpp_frame_get_width(frame)!=w||mpp_frame_get_height(frame)!=h) return 14;
                    unsigned fmt=mpp_frame_get_fmt(frame)&MPP_FRAME_FMT_MASK;
                    if (fmt!=(bits==10?MPP_FMT_YUV420SP_10BIT:MPP_FMT_YUV420SP)) return 15;
                    errors+=!!mpp_frame_get_errinfo(frame)+!!mpp_frame_get_discard(frame);
                    const uint8_t *src=mpp_buffer_get_ptr(mpp_frame_get_buffer(frame));
                    unsigned ss=mpp_frame_get_hor_stride(frame),sh=mpp_frame_get_ver_stride(frame);
                    for (unsigned p=0;p<3;p++) for (unsigned y=0;y<(p?h/2:h);y++)
                        for (unsigned x=0;x<(p?w/2:w);x++) {
                            unsigned sx=p?x*2+p-1:x;
                            size_t base=(p?(size_t)ss*sh:0)+(size_t)y*ss;
                            uint16_t v;
                            if (bits==10) {
                                unsigned bit=sx*10;
                                v=(src[base+bit/8]|(uint16_t)src[base+bit/8+1]<<8)>>(bit%8)&1023;
                            } else v=src[base+sx];
                            uint8_t b[2]={v,v>>8}; fwrite(b,1,2,out);
                        }
                    frames++;
                }
                eos=mpp_frame_get_eos(frame); mpp_frame_deinit(&frame);
            }
            usleep(1000);
        }
        if (packet) mpp_packet_deinit(&packet);
        free(data);
        printf("PHASE %d frames=%u expected=%u errors=%u eos=%d\n",phase,frames,count,errors,eos);
        if (!eos||frames!=count||errors) return 16;
        if (api->reset(ctx)) return 17;
    }
    mpp_destroy(ctx); free(config); fclose(f); fclose(out);
    puts("PASS empty CSD, consumed CSD and two reset/replay cycles");
    return 0;
}
