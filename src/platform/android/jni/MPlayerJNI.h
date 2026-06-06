#pragma once

#include <jni.h>

class MPlayerJNI {
public:
    static void registerNatives(JavaVM* vm);

    static jlong nativeCreate(JNIEnv* env, jobject thiz);
    static void nativeDestroy(JNIEnv* env, jobject thiz, jlong handle);
    static jboolean nativeOpen(JNIEnv* env, jobject thiz, jlong handle, jstring url);
    static void nativeClose(JNIEnv* env, jobject thiz, jlong handle);
    static void nativePlay(JNIEnv* env, jobject thiz, jlong handle);
    static void nativePause(JNIEnv* env, jobject thiz, jlong handle);
    static void nativeStop(JNIEnv* env, jobject thiz, jlong handle);
    static void nativeSeek(JNIEnv* env, jobject thiz, jlong handle, jdouble seconds);
    static void nativeSetVolume(JNIEnv* env, jobject thiz, jlong handle, jfloat volume);
    static void nativeSetSpeed(JNIEnv* env, jobject thiz, jlong handle, jfloat speed);
    static jdouble nativeGetDuration(JNIEnv* env, jobject thiz, jlong handle);
    static jdouble nativeGetCurrentPosition(JNIEnv* env, jobject thiz, jlong handle);
    static void nativeSetSurface(JNIEnv* env, jobject thiz, jlong handle, jobject surface);
    static void nativeSetDecoderType(JNIEnv* env, jobject thiz, jlong handle, jint type);
    static jboolean nativeIsHardwareDecoding(JNIEnv* env, jobject thiz, jlong handle);
};
