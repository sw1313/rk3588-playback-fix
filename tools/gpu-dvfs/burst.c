/* SPDX-License-Identifier: MIT
 * Bounded offscreen GLES workload for dynamic-clock response measurements.
 * Does not write sysfs, change clocks, or skip submitted GPU work.
 */
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
static uint64_t now_ns(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return (uint64_t)t.tv_sec*1000000000+t.tv_nsec; }
static long frequency(void) { long v=-1; FILE *f=fopen("/sys/class/devfreq/fb000000.gpu/cur_freq","r"); if(f){if(fscanf(f,"%ld",&v)!=1)v=-1;fclose(f);}return v; }
static GLuint shader(GLenum type,const char *source) {
 GLuint s=glCreateShader(type); GLint ok=0;glShaderSource(s,1,&source,NULL);glCompileShader(s);glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
 if(!ok){char log[1024];glGetShaderInfoLog(s,sizeof(log),NULL,log);fprintf(stderr,"%s\n",log);return 0;}return s;
}
int main(int argc,char **argv) {
 int bursts=argc>1?atoi(argv[1]):12,frames=argc>2?atoi(argv[2]):12;
 CHECK(bursts>0&&bursts<=40&&frames>0&&frames<=120);setbuf(stdout,NULL);
 EGLDisplay d=eglGetDisplay(EGL_DEFAULT_DISPLAY); CHECK(d!=EGL_NO_DISPLAY);CHECK(eglInitialize(d,NULL,NULL));
 EGLint attr[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};
 EGLConfig cfg;EGLint count=0;CHECK(eglChooseConfig(d,attr,&cfg,1,&count)&&count==1);
 EGLint size[]={EGL_WIDTH,512,EGL_HEIGHT,512,EGL_NONE}, ctxattr[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
 EGLSurface surface=eglCreatePbufferSurface(d,cfg,size);EGLContext ctx=eglCreateContext(d,cfg,EGL_NO_CONTEXT,ctxattr);
 CHECK(surface!=EGL_NO_SURFACE&&ctx!=EGL_NO_CONTEXT);CHECK(eglMakeCurrent(d,surface,surface,ctx));
 fprintf(stderr,"GL %s %s\n",glGetString(GL_RENDERER),glGetString(GL_VERSION));
 const char *vs="#version 300 es\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,0,1);}";
 const char *fs="#version 300 es\nprecision highp float;out vec4 color;uniform float seed;void main(){vec3 p=vec3(gl_FragCoord.xy*.001,seed);for(int i=0;i<96;i++)p=fract(p.yzx*1.017+sin(p.zxy+float(i)*.01)*.13);color=vec4(p,1);}";
 GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs);CHECK(v&&f);
 GLuint program=glCreateProgram();glAttachShader(program,v);glAttachShader(program,f);glLinkProgram(program);GLint ok=0;glGetProgramiv(program,GL_LINK_STATUS,&ok);CHECK(ok);
 glUseProgram(program);glUniform1f(glGetUniformLocation(program,"seed"),.37f);glViewport(0,0,512,512);glDrawArrays(GL_TRIANGLES,0,3);glFinish();CHECK(glGetError()==GL_NO_ERROR);
 puts("burst,frame,start_ns,elapsed_ns,freq_before,freq_after,pixel_hash");
 uint32_t expected=0;
 for(int b=0;b<bursts;b++) {
  usleep(800000);
  for(int i=0;i<frames;i++) {
   long before=frequency();uint64_t start=now_ns();glDrawArrays(GL_TRIANGLES,0,3);glFinish();uint64_t elapsed=now_ns()-start;
   CHECK(glGetError()==GL_NO_ERROR);long after=frequency();
   unsigned char px[4*8*8];glReadPixels(0,0,8,8,GL_RGBA,GL_UNSIGNED_BYTE,px);CHECK(glGetError()==GL_NO_ERROR);
   uint32_t hash=2166136261u;for(size_t n=0;n<sizeof(px);n++)hash=(hash^px[n])*16777619u;
   if(b==0&&i==0)expected=hash;CHECK(hash==expected);
   printf("%d,%d,%" PRIu64 ",%" PRIu64 ",%ld,%ld,%" PRIu32 "\n",b,i,start,elapsed,before,after,hash);
  }
 }
 glDeleteProgram(program);glDeleteShader(v);glDeleteShader(f);
 CHECK(eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT));
 eglDestroyContext(d,ctx);eglDestroySurface(d,surface);eglTerminate(d);return 0;
}
