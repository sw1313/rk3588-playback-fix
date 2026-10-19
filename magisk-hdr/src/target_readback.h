// SPDX-License-Identifier: MIT
// Root-requested one-shot diagnostic of the real, completed SF target.
// No drawing, buffer metadata changes, fence consumption or ordinary capture.
#pragma once
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#include <pthread.h>
#include <poll.h>
#include <fcntl.h>

namespace rk_target_readback {
struct Request {ANativeWindowBuffer* buffer;int fence;int result=1;bool realSf=false;};
inline void* worker(void* opaque){
 auto& r=*static_cast<Request*>(opaque);auto b=r.buffer;
 EGLDisplay display=EGL_NO_DISPLAY;EGLContext context=EGL_NO_CONTEXT;
 EGLSurface surface=EGL_NO_SURFACE;EGLImageKHR image=EGL_NO_IMAGE_KHR;
 PFNEGLDESTROYIMAGEKHRPROC destroy=nullptr;GLuint texture=0,fb=0;uint32_t* pixels=nullptr;
 int bits=0;size_t count=size_t(b->width)*b->height;
 do{
  if(r.fence>=0){pollfd p{r.fence,POLLIN,0};if(poll(&p,1,3000)!=1 || !(p.revents&POLLIN)){r.result=2;break;}}
  if(b->width<=0 || b->height<=0 || count>3840U*2160U){r.result=3;break;}
  display=eglGetDisplay(EGL_DEFAULT_DISPLAY);EGLint major,minor,n;EGLConfig config;
  EGLint attrs[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_NONE};
  if(!eglInitialize(display,&major,&minor) || !eglChooseConfig(display,attrs,&config,1,&n) || n<1){r.result=4;break;}
  EGLint ca[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE},pa[]={EGL_WIDTH,1,EGL_HEIGHT,1,EGL_NONE};
  context=eglCreateContext(display,config,EGL_NO_CONTEXT,ca);surface=eglCreatePbufferSurface(display,config,pa);
  if(context==EGL_NO_CONTEXT || surface==EGL_NO_SURFACE || !eglMakeCurrent(display,surface,surface,context)){r.result=5;break;}
  auto create=reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(eglGetProcAddress("eglCreateImageKHR"));
  destroy=reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImageKHR"));
  auto target=reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));
  if(!create || !destroy || !target){r.result=6;break;}
  EGLint ia[]={EGL_IMAGE_PRESERVED_KHR,EGL_TRUE,EGL_NONE};
  image=create(display,EGL_NO_CONTEXT,EGL_NATIVE_BUFFER_ANDROID,reinterpret_cast<EGLClientBuffer>(b),ia);
  if(image==EGL_NO_IMAGE_KHR){r.result=7;break;}
  glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);target(GL_TEXTURE_2D,image);
  glGenFramebuffers(1,&fb);glBindFramebuffer(GL_FRAMEBUFFER,fb);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
  if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){r.result=8;break;}
  glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE,&bits);
  if(bits!=10){r.result=9;break;}
  pixels=static_cast<uint32_t*>(calloc(count,sizeof(uint32_t)));if(!pixels){r.result=10;break;}
  glReadPixels(0,0,b->width,b->height,GL_RGBA,GL_UNSIGNED_INT_2_10_10_10_REV,pixels);
  if(glGetError()!=GL_NO_ERROR){r.result=11;break;}
  // The user/root prepares this private directory for a bounded test. Nothing
  // is emitted unless the root debug property explicitly requests a snapshot.
  FILE* out=fopen("/data/misc/rk-hdr-precision/target10.raw","wb");if(!out){r.result=12;break;}
  bool ok=fwrite(pixels,sizeof(uint32_t),count,out)==count;ok &= fclose(out)==0;
  r.result=ok?0:13;
 }while(false);
 FILE* report=fopen("/data/misc/rk-hdr-precision/report.json","w");
 if(report){fprintf(report,"{\"result\":%d,\"width\":%d,\"height\":%d,\"bits\":%d,\"format\":%d,\"original_fence_retained\":true,\"real_SF_target\":%s,\"diagnostic_not_performance_run\":true}\n",r.result,b->width,b->height,bits,b->format,r.realSf?"true":"false");fclose(report);}
 free(pixels);if(fb)glDeleteFramebuffers(1,&fb);if(texture)glDeleteTextures(1,&texture);
 if(image!=EGL_NO_IMAGE_KHR && destroy)destroy(display,image);
 // EGL_DEFAULT_DISPLAY is shared with SF. Destroy only our worker's objects;
 // eglTerminate would also invalidate the compositor's existing EGL resources.
 if(display!=EGL_NO_DISPLAY){eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);if(surface!=EGL_NO_SURFACE)eglDestroySurface(display,surface);if(context!=EGL_NO_CONTEXT)eglDestroyContext(display,context);}
 return nullptr;
}
inline void capture(ANativeWindowBuffer* buffer,int fence,bool realSf=false){
 uint64_t usage=0;if(android::GraphicBufferMapper::get().getUsage(buffer->handle,&usage) || (usage&0x4000))return;
 Request request{buffer,fence>=0?fcntl(fence,F_DUPFD_CLOEXEC,0):-1,1,realSf};
 if(fence>=0 && request.fence<0)return;
 pthread_t thread;if(!pthread_create(&thread,nullptr,worker,&request))pthread_join(thread,nullptr);
 if(request.fence>=0)close(request.fence);
 __android_log_print(ANDROID_LOG_INFO,"RKSfTarget10","one-shot actual target readback result=%d",request.result);
}
}
