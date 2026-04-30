#pragma once

#include <jni.h>
#include <string>
#include <memory>

namespace dovedioplus {

class JNIBridge {
public:
    JNIBridge();
    ~JNIBridge();

    bool initialize(JNIEnv* env);
    jboolean extractAudio(JNIEnv* env, jstring videoPath, jstring outputPath, jint quality);
    jstring getVideoInfo(JNIEnv* env, jstring videoPath);
    jboolean convertFormat(JNIEnv* env, jstring inputPath, jstring outputPath, jstring format);
    jboolean extractFrame(JNIEnv* env, jstring videoPath, jdouble timestamp, jstring outputPath);
    jint batchProcess(JNIEnv* env, jobjectArray inputPaths, jstring outputDir, jstring format);
    void setProgressCallback(JNIEnv* env, jobject callback);
    void cancel();
    jstring getLastError(JNIEnv* env);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace dovedioplus

extern "C" {
    JNIEXPORT void JNICALL Java_com_example_server_utils_NativeVideoProcessor_init(JNIEnv* env, jobject obj);
    JNIEXPORT jboolean JNICALL Java_com_example_server_utils_NativeVideoProcessor_extractAudio(JNIEnv* env, jobject obj, jstring videoPath, jstring outputPath, jint quality);
    JNIEXPORT jstring JNICALL Java_com_example_server_utils_NativeVideoProcessor_getVideoInfo(JNIEnv* env, jobject obj, jstring videoPath);
    JNIEXPORT jboolean JNICALL Java_com_example_server_utils_NativeVideoProcessor_convertFormat(JNIEnv* env, jobject obj, jstring inputPath, jstring outputPath, jstring format);
    JNIEXPORT jboolean JNICALL Java_com_example_server_utils_NativeVideoProcessor_extractFrame(JNIEnv* env, jobject obj, jstring videoPath, jdouble timestamp, jstring outputPath);
    JNIEXPORT jint JNICALL Java_com_example_server_utils_NativeVideoProcessor_batchProcess(JNIEnv* env, jobject obj, jobjectArray inputPaths, jstring outputDir, jstring format);
    JNIEXPORT void JNICALL Java_com_example_server_utils_NativeVideoProcessor_cancel(JNIEnv* env, jobject obj);
    JNIEXPORT jstring JNICALL Java_com_example_server_utils_NativeVideoProcessor_getLastError(JNIEnv* env, jobject obj);
}
