#include "jni_bridge.h"
#include "video_processor.h"
#include "downloader.h"
#include <iostream>

namespace dovedioplus {

class JNIBridge::Impl {
public:
    std::unique_ptr<VideoProcessor> processor = std::make_unique<VideoProcessor>();
    std::unique_ptr<Downloader> downloader = std::make_unique<Downloader>();
    JavaVM* jvm = nullptr;
    jobject progressCallback = nullptr;

    std::string j2s(JNIEnv* env, jstring js) {
        if (!js) return "";
        const char* c = env->GetStringUTFChars(js, nullptr);
        std::string s(c);
        env->ReleaseStringUTFChars(js, c);
        return s;
    }

    jstring s2j(JNIEnv* env, const std::string& s) {
        return env->NewStringUTF(s.c_str());
    }
};

JNIBridge::JNIBridge() : pImpl_(std::make_unique<Impl>()) {}
JNIBridge::~JNIBridge() = default;

bool JNIBridge::initialize(JNIEnv* env) {
    env->GetJavaVM(&pImpl_->jvm);
    return true;
}

jboolean JNIBridge::extractAudio(JNIEnv* env, jstring vp, jstring op, jint q) {
    return pImpl_->processor->extractAudio(pImpl_->j2s(env, vp), pImpl_->j2s(env, op), q);
}

jstring JNIBridge::getVideoInfo(JNIEnv* env, jstring vp) {
    return pImpl_->s2j(env, pImpl_->processor->getVideoInfo(pImpl_->j2s(env, vp)));
}

jboolean JNIBridge::convertFormat(JNIEnv* env, jstring ip, jstring op, jstring fmt) {
    return pImpl_->processor->convertFormat(pImpl_->j2s(env, ip), pImpl_->j2s(env, op), pImpl_->j2s(env, fmt));
}

jboolean JNIBridge::extractFrame(JNIEnv* env, jstring vp, jdouble ts, jstring op) {
    return pImpl_->processor->extractFrame(pImpl_->j2s(env, vp), ts, pImpl_->j2s(env, op));
}

jint JNIBridge::batchProcess(JNIEnv* env, jobjectArray inputs, jstring od, jstring fmt) {
    int len = env->GetArrayLength(inputs);
    std::vector<std::string> paths;
    for (int i = 0; i < len; i++) {
        jstring s = (jstring)env->GetObjectArrayElement(inputs, i);
        paths.push_back(pImpl_->j2s(env, s));
        env->DeleteLocalRef(s);
    }
    return pImpl_->processor->batchProcess(paths, pImpl_->j2s(env, od), pImpl_->j2s(env, fmt));
}

void JNIBridge::setProgressCallback(JNIEnv* env, jobject cb) {
    if (pImpl_->progressCallback) env->DeleteGlobalRef(pImpl_->progressCallback);
    pImpl_->progressCallback = env->NewGlobalRef(cb);
    pImpl_->processor->setProgressCallback([this](double p) {
        if (!pImpl_->jvm || !pImpl_->progressCallback) return;
        JNIEnv* e; bool detach = false;
        if (pImpl_->jvm->GetEnv((void**)&e, JNI_VERSION_1_8) == JNI_EDETACHED) {
            pImpl_->jvm->AttachCurrentThread((void**)&e, nullptr);
            detach = true;
        }
        jclass cls = e->GetObjectClass(pImpl_->progressCallback);
        jmethodID mid = e->GetMethodID(cls, "onProgress", "(D)V");
        if (mid) e->CallVoidMethod(pImpl_->progressCallback, mid, p);
        e->DeleteLocalRef(cls);
        if (detach) pImpl_->jvm->DetachCurrentThread();
    });
}

void JNIBridge::cancel() { pImpl_->processor->cancel(); }
jstring JNIBridge::getLastError(JNIEnv* env) { return pImpl_->s2j(env, pImpl_->processor->getLastError()); }

} // namespace dovedioplus

static dovedioplus::JNIBridge* getBridge(JNIEnv* env, jobject obj) {
    jclass cls = env->GetObjectClass(obj);
    jlong ptr = env->GetLongField(obj, env->GetFieldID(cls, "nativePtr", "J"));
    env->DeleteLocalRef(cls);
    return reinterpret_cast<dovedioplus::JNIBridge*>(ptr);
}

JNIEXPORT void JNICALL Java_com_example_server_utils_NativeVideoProcessor_init(JNIEnv* env, jobject obj) {
    auto* b = new dovedioplus::JNIBridge();
    b->initialize(env);
    jclass cls = env->GetObjectClass(obj);
    env->SetLongField(obj, env->GetFieldID(cls, "nativePtr", "J"), reinterpret_cast<jlong>(b));
    env->DeleteLocalRef(cls);
}

JNIEXPORT jboolean JNICALL Java_com_example_server_utils_NativeVideoProcessor_extractAudio(JNIEnv* env, jobject obj, jstring vp, jstring op, jint q) {
    return getBridge(env, obj)->extractAudio(env, vp, op, q);
}

JNIEXPORT jstring JNICALL Java_com_example_server_utils_NativeVideoProcessor_getVideoInfo(JNIEnv* env, jobject obj, jstring vp) {
    return getBridge(env, obj)->getVideoInfo(env, vp);
}

JNIEXPORT jboolean JNICALL Java_com_example_server_utils_NativeVideoProcessor_convertFormat(JNIEnv* env, jobject obj, jstring ip, jstring op, jstring fmt) {
    return getBridge(env, obj)->convertFormat(env, ip, op, fmt);
}

JNIEXPORT jboolean JNICALL Java_com_example_server_utils_NativeVideoProcessor_extractFrame(JNIEnv* env, jobject obj, jstring vp, jdouble ts, jstring op) {
    return getBridge(env, obj)->extractFrame(env, vp, ts, op);
}

JNIEXPORT jint JNICALL Java_com_example_server_utils_NativeVideoProcessor_batchProcess(JNIEnv* env, jobject obj, jobjectArray inputs, jstring od, jstring fmt) {
    return getBridge(env, obj)->batchProcess(env, inputs, od, fmt);
}

JNIEXPORT void JNICALL Java_com_example_server_utils_NativeVideoProcessor_cancel(JNIEnv* env, jobject obj) {
    getBridge(env, obj)->cancel();
}

JNIEXPORT jstring JNICALL Java_com_example_server_utils_NativeVideoProcessor_getLastError(JNIEnv* env, jobject obj) {
    return getBridge(env, obj)->getLastError(env);
}
