// SPDX-License-Identifier: MIT
// Isolated real BufferQueue allocation/format transitions; no display submission.
#include <gui/BufferQueue.h>
#include <gui/BufferItemConsumer.h>
#include <gui/Surface.h>
#include <ui/GraphicBufferMapper.h>
#include <system/window.h>
#include <poll.h>
#include <unistd.h>
#include <cstdio>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
int main(){
 using namespace android;
 EGLDisplay ed=eglGetDisplay(EGL_DEFAULT_DISPLAY);EGLint major,minor,count;EGLConfig config;
 EGLint attrs[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_NONE};
 if(!eglInitialize(ed,&major,&minor) || !eglChooseConfig(ed,attrs,&config,1,&count) || count<1)return 8;
 EGLint ca[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE},pa[]={EGL_WIDTH,1,EGL_HEIGHT,1,EGL_NONE};
 EGLContext ec=eglCreateContext(ed,config,EGL_NO_CONTEXT,ca);EGLSurface es=eglCreatePbufferSurface(ed,config,pa);
 if(!eglMakeCurrent(ed,es,es,ec))return 9;
 auto create=reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(eglGetProcAddress("eglCreateImageKHR"));
 auto destroy=reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImageKHR"));
 auto target=reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));
 if(!create || !destroy || !target)return 10;
 sp<IGraphicBufferProducer> producer;sp<IGraphicBufferConsumer> consumer;
 BufferQueue::createBufferQueue(&producer,&consumer);
 sp<BufferItemConsumer> sink=new BufferItemConsumer(consumer,0x800,3,false);
 sp<Surface> surface=new Surface(producer,false);ANativeWindow* window=surface.get();
 if(native_window_api_connect(window,NATIVE_WINDOW_API_EGL) || native_window_set_buffers_dimensions(window,3840,2160) || native_window_set_usage(window,0x300))return 2;
 int errors=0;int formats[]={1,43,1,43,1};
 for(int format:formats){
  if(native_window_set_buffers_format(window,format))return 3;
  ANativeWindowBuffer* buffer=nullptr;int fence=-1;
  if(window->dequeueBuffer(window,&buffer,&fence))return 4;
  if(fence>=0){pollfd p{fence,POLLIN,0};if(poll(&p,1,3000)!=1)return 5;close(fence);}
  uint32_t fourcc=0;int rc=GraphicBufferMapper::get().getPixelFormatFourCC(buffer->handle,&fourcc);
  printf("QUEUE request=%d buffer_format=%d query=%d fourcc=%08x\n",format,buffer->format,rc,fourcc);
  EGLint ia[]={EGL_IMAGE_PRESERVED_KHR,EGL_TRUE,EGL_NONE};EGLImageKHR image=create(ed,EGL_NO_CONTEXT,EGL_NATIVE_BUFFER_ANDROID,reinterpret_cast<EGLClientBuffer>(buffer),ia);
  if(image==EGL_NO_IMAGE_KHR)return 11;
  GLuint texture,fb;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);target(GL_TEXTURE_2D,image);
  glGenFramebuffers(1,&fb);glBindFramebuffer(GL_FRAMEBUFFER,fb);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
  if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)return 12;
  GLint bits=0;glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE,&bits);
  glDisable(GL_DITHER);glClearColor(511.f/1023,513.f/1023,515.f/1023,1);glClear(GL_COLOR_BUFFER_BIT);glFinish();
  if(format==43){
   uint32_t pixel=0;glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_INT_2_10_10_10_REV,&pixel);
   GLenum error=glGetError();printf("GPU bits=%d pixel=%08x error=%x\n",bits,pixel,error);
   errors+=error || bits!=10 || (pixel&1023)!=511 || ((pixel>>10)&1023)!=513 || ((pixel>>20)&1023)!=515;
  }
  if(GraphicBufferMapper::get().getPixelFormatFourCC(buffer->handle,&fourcc))return 13;
  printf("POST_GPU request=%d fourcc=%08x\n",format,fourcc);
  glDeleteFramebuffers(1,&fb);glDeleteTextures(1,&texture);destroy(ed,image);
  if(window->cancelBuffer(window,buffer,-1))return 6;
  errors+=rc || fourcc!=(format==43?0x30334241U:0x34324241U);
 }
 if(native_window_api_disconnect(window,NATIVE_WINDOW_API_EGL))return 7;
 eglMakeCurrent(ed,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroySurface(ed,es);eglDestroyContext(ed,ec);eglTerminate(ed);
 return errors?1:0;
}
