#define VK_NO_PROTOTYPES
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <hardware/hwvulkan.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define REQUIRE(x) do {if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
#define LOAD(T,n) T n=(T)dlsym(h,#n);if(!n)n=(T)dlsym(RTLD_DEFAULT,#n);REQUIRE(n)
static int gl_test(void *h) {
 LOAD(PFNEGLGETDISPLAYPROC,eglGetDisplay);LOAD(PFNEGLINITIALIZEPROC,eglInitialize);
 LOAD(PFNEGLGETERRORPROC,eglGetError);LOAD(PFNEGLCHOOSECONFIGPROC,eglChooseConfig);
 LOAD(PFNEGLCREATEPBUFFERSURFACEPROC,eglCreatePbufferSurface);LOAD(PFNEGLCREATECONTEXTPROC,eglCreateContext);
 LOAD(PFNEGLMAKECURRENTPROC,eglMakeCurrent);LOAD(PFNEGLDESTROYCONTEXTPROC,eglDestroyContext);
 LOAD(PFNEGLDESTROYSURFACEPROC,eglDestroySurface);LOAD(PFNEGLTERMINATEPROC,eglTerminate);
 LOAD(PFNGLGETSTRINGPROC,glGetString);LOAD(PFNGLCLEARCOLORPROC,glClearColor);
 LOAD(PFNGLCLEARPROC,glClear);LOAD(PFNGLREADPIXELSPROC,glReadPixels);LOAD(PFNGLGETERRORPROC,glGetError);
 LOAD(PFNGLCREATESHADERPROC,glCreateShader);LOAD(PFNGLSHADERSOURCEPROC,glShaderSource);
 LOAD(PFNGLCOMPILESHADERPROC,glCompileShader);LOAD(PFNGLGETSHADERIVPROC,glGetShaderiv);
 LOAD(PFNGLCREATEPROGRAMPROC,glCreateProgram);LOAD(PFNGLATTACHSHADERPROC,glAttachShader);
 LOAD(PFNGLLINKPROGRAMPROC,glLinkProgram);LOAD(PFNGLGETPROGRAMIVPROC,glGetProgramiv);
 LOAD(PFNGLUSEPROGRAMPROC,glUseProgram);LOAD(PFNGLDRAWARRAYSPROC,glDrawArrays);
 LOAD(PFNGLVIEWPORTPROC,glViewport);LOAD(PFNGLFINISHPROC,glFinish);
 EGLDisplay d=eglGetDisplay(EGL_DEFAULT_DISPLAY);REQUIRE(d!=EGL_NO_DISPLAY);
 EGLint major=0,minor=0;
 if(!eglInitialize(d,&major,&minor)){printf("EGL_INIT_FAIL error=0x%x\n",eglGetError());return 1;}
 printf("EGL %d.%d\n",major,minor);
 EGLint attrs[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};
 EGLConfig config;EGLint count=0;REQUIRE(eglChooseConfig(d,attrs,&config,1,&count)&&count==1);
 EGLint size[]={EGL_WIDTH,64,EGL_HEIGHT,64,EGL_NONE},context[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
 EGLSurface s=eglCreatePbufferSurface(d,config,size);REQUIRE(s!=EGL_NO_SURFACE);
 EGLContext c=eglCreateContext(d,config,EGL_NO_CONTEXT,context);REQUIRE(c!=EGL_NO_CONTEXT);
 REQUIRE(eglMakeCurrent(d,s,s,c));
 printf("GL_VENDOR %s\nGL_RENDERER %s\nGL_VERSION %s\n",glGetString(GL_VENDOR),glGetString(GL_RENDERER),glGetString(GL_VERSION));
 glViewport(0,0,64,64);glClearColor(1,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
 unsigned char px[4]={0};glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);REQUIRE(px[0]==255&&px[1]==0&&px[2]==0&&px[3]==255);
 const char *vs="#version 300 es\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.0-1.0,0,1);}";
 const char *fs="#version 300 es\nprecision highp float;out vec4 color;void main(){color=vec4(0,1,0,1);}";
 GLuint vert=glCreateShader(GL_VERTEX_SHADER),frag=glCreateShader(GL_FRAGMENT_SHADER);GLint ok=0;
 glShaderSource(vert,1,&vs,NULL);glCompileShader(vert);glGetShaderiv(vert,GL_COMPILE_STATUS,&ok);REQUIRE(ok);
 glShaderSource(frag,1,&fs,NULL);glCompileShader(frag);glGetShaderiv(frag,GL_COMPILE_STATUS,&ok);REQUIRE(ok);
 GLuint prog=glCreateProgram();glAttachShader(prog,vert);glAttachShader(prog,frag);glLinkProgram(prog);glGetProgramiv(prog,GL_LINK_STATUS,&ok);REQUIRE(ok);
 glUseProgram(prog);glDrawArrays(GL_TRIANGLES,0,3);glFinish();glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
 REQUIRE(px[0]==0&&px[1]==255&&px[2]==0&&px[3]==255&&glGetError()==GL_NO_ERROR);
 REQUIRE(eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT));
 eglDestroyContext(d,c);eglDestroySurface(d,s);eglTerminate(d);puts("PASS GLES3 shader draw and exact RGBA readback");return 0;
}
#define VI(n) PFN_vk##n vk##n=(PFN_vk##n)dev->GetInstanceProcAddr(instance,"vk"#n);REQUIRE(vk##n)
#define VD(n) PFN_vk##n vk##n=(PFN_vk##n)vkGetDeviceProcAddr(device,"vk"#n);REQUIRE(vk##n)
static int vk_test(void *h) {
 hwvulkan_module_t *module=dlsym(h,HAL_MODULE_INFO_SYM_AS_STR);
 hwvulkan_device_t platform={0},*dev=NULL;
 if(module){REQUIRE(module->common.methods);REQUIRE(module->common.methods->open(&module->common,HWVULKAN_DEVICE_0,(hw_device_t**)&dev)==0);}
 else {platform.CreateInstance=(PFN_vkCreateInstance)dlsym(h,"vkCreateInstance");platform.GetInstanceProcAddr=(PFN_vkGetInstanceProcAddr)dlsym(h,"vkGetInstanceProcAddr");REQUIRE(platform.CreateInstance&&platform.GetInstanceProcAddr);dev=&platform;}
 VkApplicationInfo app={.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO,.pApplicationName="RK isolated driver validation",.apiVersion=VK_API_VERSION_1_1};
 VkInstanceCreateInfo ic={.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,.pApplicationInfo=&app};VkInstance instance=VK_NULL_HANDLE;
 VkResult rc=dev->CreateInstance(&ic,NULL,&instance);printf("Vulkan CreateInstance=%d\n",rc);REQUIRE(rc==VK_SUCCESS);
 VI(EnumeratePhysicalDevices);VI(GetPhysicalDeviceProperties);VI(GetPhysicalDeviceQueueFamilyProperties);VI(GetPhysicalDeviceMemoryProperties);VI(CreateDevice);VI(GetDeviceProcAddr);VI(DestroyInstance);
 uint32_t n=1;VkPhysicalDevice gpu;REQUIRE(vkEnumeratePhysicalDevices(instance,&n,&gpu)==VK_SUCCESS&&n==1);
 VkPhysicalDeviceProperties props;vkGetPhysicalDeviceProperties(gpu,&props);
 printf("VK_DEVICE %s API %u.%u.%u DRIVER %u\n",props.deviceName,VK_VERSION_MAJOR(props.apiVersion),VK_VERSION_MINOR(props.apiVersion),VK_VERSION_PATCH(props.apiVersion),props.driverVersion);
 uint32_t qn=0;vkGetPhysicalDeviceQueueFamilyProperties(gpu,&qn,NULL);REQUIRE(qn>0&&qn<64);VkQueueFamilyProperties queues[64];vkGetPhysicalDeviceQueueFamilyProperties(gpu,&qn,queues);uint32_t qi=0;for(;qi<qn;++qi)if(queues[qi].queueFlags&VK_QUEUE_GRAPHICS_BIT)break;REQUIRE(qi<qn);
 float priority=1;VkDeviceQueueCreateInfo qc={.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,.queueFamilyIndex=qi,.queueCount=1,.pQueuePriorities=&priority};VkDeviceCreateInfo dc={.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,.queueCreateInfoCount=1,.pQueueCreateInfos=&qc};VkDevice device;
 rc=vkCreateDevice(gpu,&dc,NULL,&device);printf("Vulkan CreateDevice=%d\n",rc);REQUIRE(rc==VK_SUCCESS);
 VD(GetDeviceQueue);VD(CreateBuffer);VD(GetBufferMemoryRequirements);VD(AllocateMemory);VD(BindBufferMemory);VD(MapMemory);VD(UnmapMemory);VD(InvalidateMappedMemoryRanges);VD(CreateCommandPool);VD(AllocateCommandBuffers);VD(BeginCommandBuffer);VD(CmdFillBuffer);VD(EndCommandBuffer);VD(QueueSubmit);VD(QueueWaitIdle);VD(DestroyCommandPool);VD(DestroyBuffer);VD(FreeMemory);VD(DestroyDevice);
 VkQueue queue;vkGetDeviceQueue(device,qi,0,&queue);
 VkBufferCreateInfo bc={.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,.size=4096,.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT,.sharingMode=VK_SHARING_MODE_EXCLUSIVE};VkBuffer buffer;REQUIRE(vkCreateBuffer(device,&bc,NULL,&buffer)==VK_SUCCESS);
 VkMemoryRequirements mr;vkGetBufferMemoryRequirements(device,buffer,&mr);VkPhysicalDeviceMemoryProperties mp;vkGetPhysicalDeviceMemoryProperties(gpu,&mp);uint32_t mi=0;for(;mi<mp.memoryTypeCount;++mi)if((mr.memoryTypeBits&(1u<<mi))&&(mp.memoryTypes[mi].propertyFlags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))break;REQUIRE(mi<mp.memoryTypeCount);
 VkMemoryAllocateInfo ma={.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,.allocationSize=mr.size,.memoryTypeIndex=mi};VkDeviceMemory memory;REQUIRE(vkAllocateMemory(device,&ma,NULL,&memory)==VK_SUCCESS);REQUIRE(vkBindBufferMemory(device,buffer,memory,0)==VK_SUCCESS);
 VkCommandPoolCreateInfo pc={.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,.queueFamilyIndex=qi};VkCommandPool pool;REQUIRE(vkCreateCommandPool(device,&pc,NULL,&pool)==VK_SUCCESS);
 VkCommandBufferAllocateInfo ca={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,.commandPool=pool,.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,.commandBufferCount=1};VkCommandBuffer command;REQUIRE(vkAllocateCommandBuffers(device,&ca,&command)==VK_SUCCESS);VkCommandBufferBeginInfo bi={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};REQUIRE(vkBeginCommandBuffer(command,&bi)==VK_SUCCESS);
 vkCmdFillBuffer(command,buffer,0,4096,0x39a75c12);REQUIRE(vkEndCommandBuffer(command)==VK_SUCCESS);VkSubmitInfo submit={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.commandBufferCount=1,.pCommandBuffers=&command};REQUIRE(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE)==VK_SUCCESS);REQUIRE(vkQueueWaitIdle(queue)==VK_SUCCESS);
 void *mapped;REQUIRE(vkMapMemory(device,memory,0,VK_WHOLE_SIZE,0,&mapped)==VK_SUCCESS);VkMappedMemoryRange range={.sType=VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,.memory=memory,.offset=0,.size=VK_WHOLE_SIZE};REQUIRE(vkInvalidateMappedMemoryRanges(device,1,&range)==VK_SUCCESS);
 for(unsigned i=0;i<1024;++i)REQUIRE(((uint32_t*)mapped)[i]==0x39a75c12);
 vkUnmapMemory(device,memory);vkDestroyCommandPool(device,pool,NULL);vkDestroyBuffer(device,buffer,NULL);vkFreeMemory(device,memory,NULL);vkDestroyDevice(device,NULL);vkDestroyInstance(instance,NULL);if(dev->common.close)dev->common.close((hw_device_t*)dev);puts("PASS Vulkan GPU fill and exact 4096-byte readback");return 0;
}
int main(int argc,char **argv) {
 if(argc!=3)return 2;setbuf(stdout,NULL);
 const char *path=getenv("RK_DRIVER_PATH");
 if(path){void *setup=dlopen("/data/local/tmp/rk-mali-native-test/setup_namespace.so",RTLD_NOW|RTLD_LOCAL);if(!setup){puts(dlerror());return 3;}int(*fn)(const char*)=dlsym(setup,"setup_driver");REQUIRE(fn&&fn(path)==0);if(!strcmp(argv[2],"gl"))REQUIRE(dlopen("libGLESv2.so",RTLD_NOW|RTLD_GLOBAL));}
 if(!strcmp(argv[1],"libEGL.so"))REQUIRE(dlopen("libGLESv2.so",RTLD_NOW|RTLD_GLOBAL));
 void *h=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!h){puts(dlerror());return 1;}
 return !strcmp(argv[2],"gl")?gl_test(h):vk_test(h);
}
