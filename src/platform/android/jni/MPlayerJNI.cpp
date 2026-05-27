#include "MPlayerJNI.h"
#include "core/controller/PlayerController.h"
#include "GLESRenderer.h"
#include "AndroidAudioOutput.h"

#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "MPlayerJNI", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "MPlayerJNI", __VA_ARGS__)

static const char* kClassName = "com/mplayer/MPlayerNative";

static JNINativeMethod methods[] = {
    {"nativeCreate", "()J", (void*)MPlayerJNI::nativeCreate},
    {"nativeDestroy", "(J)V", (void*)MPlayerJNI::nativeDestroy},
    {"nativeOpen", "(JLjava/lang/String;)Z", (void*)MPlayerJNI::nativeOpen},
    {"nativeClose", "(J)V", (void*)MPlayerJNI::nativeClose},
    {"nativePlay", "(J)V", (void*)MPlayerJNI::nativePlay},
    {"nativePause", "(J)V", (void*)MPlayerJNI::nativePause},
    {"nativeStop", "(J)V", (void*)MPlayerJNI::nativeStop},
    {"nativeSeek", "(JD)V", (void*)MPlayerJNI::nativeSeek},
    {"nativeSetVolume", "(JF)V", (void*)MPlayerJNI::nativeSetVolume},
    {"nativeSetSpeed", "(JF)V", (void*)MPlayerJNI::nativeSetSpeed},
    {"nativeGetDuration", "(J)D", (void*)MPlayerJNI::nativeGetDuration},
    {"nativeGetCurrentPosition", "(J)D", (void*)MPlayerJNI::nativeGetCurrentPosition},
    {"nativeSetSurface", "(JLjava/lang/Object;)V", (void*)MPlayerJNI::nativeSetSurface},
};

void MPlayerJNI::registerNatives(JavaVM* vm) {
    JNIEnv* env = nullptr;
    vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);

    jclass cls = env->FindClass(kClassName);
    if (!cls) {
        LOGE("Failed to find class %s", kClassName);
        return;
    }

    env->RegisterNatives(cls, methods, sizeof(methods) / sizeof(methods[0]));
    LOGI("JNI natives registered");
}

jlong MPlayerJNI::nativeCreate(JNIEnv* env, jobject thiz) {
    auto* player = new PlayerController();
    return reinterpret_cast<jlong>(player);
}

void MPlayerJNI::nativeDestroy(JNIEnv* env, jobject thiz, jlong handle) {
    auto* player = reinterpret_cast<PlayerController*>(handle);
    if (player) {
        player->close();
        delete player;
    }
}

jboolean MPlayerJNI::nativeOpen(JNIEnv* env, jobject thiz, jlong handle, jstring url) {
    auto* player = reinterpret_cast<PlayerController*>(handle);
    if (!player) return JNI_FALSE;

    const char* urlStr = env->GetStringUTFChars(url, nullptr);
    bool result = player->open(urlStr);
    env->ReleaseStringUTFChars(url, urlStr);

    return result ? JNI_TRUE : JNI_FALSE;
}

void MPlayerJNI::nativeClose(JNIEnv* env, jobject thiz, jlong handle) {
    auto* player = reinterpret_cast<PlayerController*>(handle);
    if (player) player->close();
}

void MPlayerJNI::nativePlay(JNIEnv* env, jobject thiz, jlong handle) {
    auto* player = reinterpret_cast<PlayerController*>(handle);
    if (player) player->play();
}

void MPlayerJNI::nativePause(JNIEnv* env, jobject thiz, jlong handle) {
    auto* player = reinterpret_cast<PlayerController*>(handle);
    if (player) player->pause();
}

void MPlayerJNI::nativeStop(JNIEnv* env, jobject thiz, jlong handle) {
    auto* player = reinterpret_cast<PlayerController*>(handle);
    if (player) player->stop();
}

void MPlayerJNI::nativeSeek(JNIEnv* env, jobject thiz, jlong handle, jdouble seconds) {
    auto* player = reinterpret_cast<PlayerController*>(handle);
    if (player) player->seek(seconds);
}

void MPlayerJNI::nativeSetVolume(JNIEnv* env, jobject thiz, jlong handle, jfloat volume) {
    auto* player = reinterpret_cast<PlayerController*>(handle);
    if (player) player->setVolume(volume);
}

void MPlayerJNI::nativeSetSpeed(JNIEnv* env, jobject thiz, jlong handle, jfloat speed) {
    auto* player = reinterpret_cast<PlayerController*>(handle);
    if (player) player->setSpeed(speed);
}

jdouble MPlayerJNI::nativeGetDuration(JNIEnv* env, jobject thiz, jlong handle) {
    auto* player = reinterpret_cast<PlayerController*>(handle);
    return player ? player->duration() : 0.0;
}

jdouble MPlayerJNI::nativeGetCurrentPosition(JNIEnv* env, jobject thiz, jlong handle) {
    auto* player = reinterpret_cast<PlayerController*>(handle);
    return player ? player->currentPosition() : 0.0;
}

void MPlayerJNI::nativeSetSurface(JNIEnv* env, jobject thiz, jlong handle, jobject surface) {
    auto* player = reinterpret_cast<PlayerController*>(handle);
    if (!player || !surface) return;

    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (!window) {
        LOGE("Failed to get ANativeWindow from Surface");
        return;
    }

    auto renderer = std::make_unique<GLESRenderer>();
    if (renderer->init(window)) {
        player->setRenderer(std::move(renderer));
    }

    auto audio = std::make_unique<AndroidAudioOutput>();
    player->setAudioOutput(std::move(audio));
}

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    MPlayerJNI::registerNatives(vm);
    return JNI_VERSION_1_6;
}
