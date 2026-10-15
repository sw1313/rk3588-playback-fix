#define main private_probe_main
#include "../graphics_probe.c"
#undef main
#include <jni.h>
JNIEXPORT jint JNICALL Java_com_sw1313_malitest_MainActivity_checkVulkan(JNIEnv *env,jclass type,jstring path) {
 (void)type;const char *name=(*env)->GetStringUTFChars(env,path,NULL);
 FILE *file=freopen(name,"w",stdout);(*env)->ReleaseStringUTFChars(env,path,name);if(!file)return 99;setbuf(stdout,NULL);
 void *h=dlopen("libvulkan.so",RTLD_NOW|RTLD_LOCAL);if(!h){puts(dlerror());return 98;}return vk_test(h);
}
