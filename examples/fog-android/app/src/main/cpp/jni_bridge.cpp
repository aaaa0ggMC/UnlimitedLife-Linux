// jni_bridge.cpp —— JNI 桥（纯 TU，不 import 任何模块）
// 平台 C 头只能出现在这里；alib6 侧经 extern "C" alib_probe_cstr() 交互。
#include <jni.h>
#include <android/log.h>

extern "C" const char* alib_probe_cstr();

namespace {
constexpr const char* kLogTag = "AlibProbe";
} // namespace

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "libfogandroid.so loaded (alib6 port)");
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_fogandroid_MainActivity_alibProbe(JNIEnv* env, jobject /*thiz*/) {
    const char* text = alib_probe_cstr();
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "probe:\n%s", text);
    return env->NewStringUTF(text);
}
