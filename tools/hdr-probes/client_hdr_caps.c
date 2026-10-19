/* SPDX-License-Identifier: MIT
 * Read-only EGL configuration inventory. Creates no context, surface or buffer,
 * and never changes the display's mode, dataspace or HDR metadata.
 */
#include <EGL/egl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){
 EGLDisplay d=eglGetDisplay(EGL_DEFAULT_DISPLAY);EGLint major,minor,n;
 if(d==EGL_NO_DISPLAY||!eglInitialize(d,&major,&minor)){fprintf(stderr,"EGL initialization failed 0x%x\n",eglGetError());return 1;}
 if(!eglGetConfigs(d,NULL,0,&n)||n<0||n>4096)return 2;
 EGLConfig *configs=calloc(n?n:1,sizeof(*configs));if(!configs)return 2;
 EGLint cap=n;if(!eglGetConfigs(d,configs,cap,&n))return 2;
 const char *ex=eglQueryString(d,EGL_EXTENSIONS);
 printf("EGL %d.%d configs=%d colorspace=%d bt2020_pq=%d bt2020_hlg=%d scrgb=%d\n",major,minor,n,
  ex&&strstr(ex,"EGL_KHR_gl_colorspace")!=NULL,ex&&strstr(ex,"EGL_EXT_gl_colorspace_bt2020_pq")!=NULL,
  ex&&strstr(ex,"EGL_EXT_gl_colorspace_bt2020_hlg")!=NULL,ex&&strstr(ex,"EGL_EXT_gl_colorspace_scrgb")!=NULL);
 unsigned ten=0,wide=0;
 for(int i=0;i<n&&i<cap;i++){
  EGLint r=0,g=0,b=0,a=0,type=0,id=0,visual=0;
  eglGetConfigAttrib(d,configs[i],EGL_RED_SIZE,&r);eglGetConfigAttrib(d,configs[i],EGL_GREEN_SIZE,&g);
  eglGetConfigAttrib(d,configs[i],EGL_BLUE_SIZE,&b);eglGetConfigAttrib(d,configs[i],EGL_ALPHA_SIZE,&a);
  eglGetConfigAttrib(d,configs[i],EGL_SURFACE_TYPE,&type);eglGetConfigAttrib(d,configs[i],EGL_CONFIG_ID,&id);
  eglGetConfigAttrib(d,configs[i],EGL_NATIVE_VISUAL_ID,&visual);
  if(r>=10&&g>=10&&b>=10){wide++;if(r==10&&g==10&&b==10)ten++;
   printf("WIDE_CONFIG id=%d rgba=%d/%d/%d/%d window=%d pbuffer=%d visual=%d\n",id,r,g,b,a,!!(type&EGL_WINDOW_BIT),!!(type&EGL_PBUFFER_BIT),visual);}
 }
 printf("SUMMARY rgb10_configs=%u wide_configs=%u actual_HDR_composition=NOT_TESTED\n",ten,wide);
 free(configs);eglTerminate(d);return 0;
}
