/* SPDX-License-Identifier: MIT
 * Use the source-built client against original remaining FFmpeg DSOs. Writes
 * visible P010 samples for an independent precision comparison, not screenshots.
 */
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>
#include <libavutil/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dlfcn.h>
static FILE *output;
static int frames,format=-1,errors,ended;
static int receive(AVCodecContext *c){
 AVFrame *f=av_frame_alloc();if(!f)return AVERROR(ENOMEM);
 int r;
 while((r=avcodec_receive_frame(c,f))>=0){
  if(f->format!=AV_PIX_FMT_P010LE){fprintf(stderr,"Unexpected format %s\n",av_get_pix_fmt_name(f->format));errors++;av_frame_unref(f);continue;}
  if(!frames)printf("FORMAT %s %dx%d range=%d primaries=%d transfer=%d matrix=%d\n",av_get_pix_fmt_name(f->format),f->width,f->height,f->color_range,f->color_primaries,f->color_trc,f->colorspace);
  if(format>=0&&format!=f->format)errors++;format=f->format;
  for(int plane=0;plane<2;plane++){
   int rows=plane?(f->height+1)/2:f->height,bytes=plane?((f->width+1)/2)*4:f->width*2;
   for(int y=0;y<rows;y++)if(fwrite(f->data[plane]+(size_t)y*f->linesize[plane],1,bytes,output)!=(size_t)bytes)errors++;
  }
  frames++;av_frame_unref(f);
 }
 av_frame_free(&f);if(r==AVERROR_EOF)ended=1;return r==AVERROR(EAGAIN)||r==AVERROR_EOF?0:r;
}
int main(int argc,char **argv){
 setbuf(stdout,NULL);if(argc!=3)return 2;AVFormatContext *fmt=NULL;
 const char *privateSf=getenv("RK_PRIVATE_SFPLUGIN");
 if(privateSf){
  void *lib=dlopen(privateSf,RTLD_NOW|RTLD_GLOBAL);if(!lib){fprintf(stderr,"Private SF plugin load failed: %s\n",dlerror());return 8;}
  int (*attach)(void*)=(int(*)(void*))dlsym(lib,"rk_pool10_attach");
  if(!attach || attach(lib)){fprintf(stderr,"Private pool attach rejected\n");return 8;}
  puts("PRIVATE_P010_POOL_ATTACHED");
 }
 if(avformat_open_input(&fmt,argv[1],NULL,NULL)<0||avformat_find_stream_info(fmt,NULL)<0)return 3;
 int stream=av_find_best_stream(fmt,AVMEDIA_TYPE_VIDEO,-1,-1,NULL,0);if(stream<0)return 3;
 const AVCodec *decoder=avcodec_find_decoder_by_name("hevc_mediacodec");if(!decoder)return 3;
 AVCodecContext *c=avcodec_alloc_context3(decoder);if(!c||avcodec_parameters_to_context(c,fmt->streams[stream]->codecpar)<0)return 3;
 c->pkt_timebase=fmt->streams[stream]->time_base;AVDictionary *opts=NULL;av_dict_set(&opts,"ndk_codec","1",0);
 if(avcodec_open2(c,decoder,&opts)<0)return 4;av_dict_free(&opts);
 output=fopen(argv[2],"wb");if(!output)return 5;
 AVPacket *packet=av_packet_alloc();if(!packet)return 5;
 while(av_read_frame(fmt,packet)>=0){
  if(packet->stream_index==stream){int r=avcodec_send_packet(c,packet);if(r==AVERROR(EAGAIN)){if(receive(c)<0)return 6;r=avcodec_send_packet(c,packet);}if(r<0||receive(c)<0)return 6;}
  av_packet_unref(packet);
 }
 if(avcodec_send_packet(c,NULL)<0||receive(c)<0)return 6;
 int64_t deadline=av_gettime_relative()+10000000;
 while(!ended && av_gettime_relative()<deadline){if(receive(c)<0)return 6;if(!ended)av_usleep(10000);}
 if(!ended){fprintf(stderr,"Decoder did not reach EOS before deadline\n");errors++;}
 av_packet_free(&packet);avcodec_free_context(&c);avformat_close_input(&fmt);if(fclose(output))errors++;
 printf("RESULT frames=%d errors=%d P010=%d\n",frames,errors,format==AV_PIX_FMT_P010LE);return errors||!frames?7:0;
}
