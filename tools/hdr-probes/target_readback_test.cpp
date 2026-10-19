// SPDX-License-Identifier: MIT
// Scratch target only: validates exact ten-bit reading and preservation of the
// caller EGL resources and original producer fence. No display submission.
#include <system/window.h>
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferMapper.h>
#include <android/log.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include "../../magisk-hdr/src/target_readback.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
int main(){
 EGLDisplay d=eglGetDisplay(EGL_DEFAULT_DISPLAY);EGLint major,minor,n;EGLConfig config;
 EGLint attrs[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_NONE};
 CHECK(eglInitialize(d,&major,&minor) && eglChooseConfig(d,attrs,&config,1,&n) && n);
 EGLint ca[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE},pa[]={EGL_WIDTH,1,EGL_HEIGHT,1,EGL_NONE};
 auto context=eglCreateContext(d,config,EGL_NO_CONTEXT,ca);auto surface=eglCreatePbufferSurface(d,config,pa);
 CHECK(eglMakeCurrent(d,surface,surface,context));
 auto create=reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(eglGetProcAddress("eglCreateImageKHR"));
 auto target=reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));
 auto sync=reinterpret_cast<PFNEGLCREATESYNCKHRPROC>(eglGetProcAddress("eglCreateSyncKHR"));
 auto dupFence=reinterpret_cast<PFNEGLDUPNATIVEFENCEFDANDROIDPROC>(eglGetProcAddress("eglDupNativeFenceFDANDROID"));
 CHECK(create && target && sync && dupFence);
 android::sp<android::GraphicBuffer> b=new android::GraphicBuffer(64,64,android::PixelFormat(43),1,0xb00,"TargetReadOnlyFixture");CHECK(!b->initCheck());
 EGLint ia[]={EGL_IMAGE_PRESERVED_KHR,EGL_TRUE,EGL_NONE};auto image=create(d,EGL_NO_CONTEXT,EGL_NATIVE_BUFFER_ANDROID,reinterpret_cast<EGLClientBuffer>(b->getNativeBuffer()),ia);CHECK(image!=EGL_NO_IMAGE_KHR);
 GLuint texture,fb;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);target(GL_TEXTURE_2D,image);
 glGenFramebuffers(1,&fb);glBindFramebuffer(GL_FRAMEBUFFER,fb);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
 CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
 glDisable(GL_DITHER);glClearColor(511.f/1023,513.f/1023,515.f/1023,1);glClear(GL_COLOR_BUFFER_BIT);
 auto completion=sync(d,EGL_SYNC_NATIVE_FENCE_ANDROID,nullptr);glFlush();int fence=dupFence(d,completion);CHECK(fence>=0);
 rk_target_readback::capture(b->getNativeBuffer(),fence);
 CHECK(fcntl(fence,F_GETFD)>=0 && eglGetCurrentContext()==context && eglGetCurrentDisplay()==d);
 uint32_t expected=511U|(513U<<10)|(515U<<20)|(3U<<30),pixel=0;
 glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_INT_2_10_10_10_REV,&pixel);CHECK(!glGetError() && pixel==expected);
 FILE* raw=fopen("/data/misc/rk-hdr-precision/target10.raw","rb");CHECK(raw);
 for(unsigned i=0;i<64*64;i++){CHECK(fread(&pixel,4,1,raw)==1 && pixel==expected);}CHECK(fgetc(raw)==EOF);fclose(raw);close(fence);
 puts("PASS read-only scratch RGB10 pixels=4096 exact_511_513_515=1 original_fence_open=1 original_EGL_resources_usable=1 no_display_submission=1");
}
