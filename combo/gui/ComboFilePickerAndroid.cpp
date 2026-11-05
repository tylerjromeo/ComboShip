// combo/gui/ComboFilePickerAndroid.cpp
// Android backend for ComboFilePicker::PickFile. The Storage Access Framework hands back a content
// URI, not a path, so MainActivity copies the chosen document into the app cache and reports that
// path through nativeHandleSelectedFile. Same blocking handshake as linkzenic's SoH Android port
// (soh/soh/Extractor/Extract.cpp there); called from the SDL thread, answered on the UI thread.
#ifdef __ANDROID__
#include "ComboFilePicker.h"

#include <SDL2/SDL.h>
#include <jni.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>

namespace {
std::mutex sPickerMutex;
std::condition_variable sPickerReady;
bool sPickerPending = false;
std::string sPickedPath;
} // namespace

// Bound by JNI from MainActivity (package com.dishii.soh, see Android/app/build.gradle). comboui is
// System.loadLibrary'd by MainActivity.getLibraries() precisely so this resolves.
extern "C" JNIEXPORT void JNICALL Java_com_dishii_soh_MainActivity_nativeHandleSelectedFile(JNIEnv* env, jobject,
                                                                                         jstring path) {
    std::lock_guard<std::mutex> lock(sPickerMutex);
    sPickedPath.clear();
    if (path) {
        if (const char* utf = env->GetStringUTFChars(path, nullptr)) {
            sPickedPath = utf;
            env->ReleaseStringUTFChars(path, utf);
        }
    }
    sPickerPending = false;
    sPickerReady.notify_one();
}

namespace ComboFilePicker {

std::string PickFileAndroid() {
    auto* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    auto activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if (!env || !activity) {
        return {};
    }
    // A fresh name per pick: the setup screen keeps each slot's path, so re-picking one ROM must
    // not overwrite the file the other slot still points at.
    static std::atomic<int> sPickCount{ 0 };
    const std::string fileName = "comboship-pick-" + std::to_string(sPickCount++) + ".z64";

    jclass cls = env->GetObjectClass(activity);
    jmethodID picker = env->GetMethodID(cls, "openFilePickerAs", "(Ljava/lang/String;)V");
    {
        std::lock_guard<std::mutex> lock(sPickerMutex);
        sPickedPath.clear();
        sPickerPending = true;
    }
    bool failed = picker == nullptr || env->ExceptionCheck();
    if (!failed) {
        jstring jName = env->NewStringUTF(fileName.c_str());
        env->CallVoidMethod(activity, picker, jName);
        env->DeleteLocalRef(jName);
        failed = env->ExceptionCheck();
    }
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
    if (failed) {
        std::lock_guard<std::mutex> lock(sPickerMutex);
        sPickerPending = false;
        return {};
    }
    std::unique_lock<std::mutex> lock(sPickerMutex);
    sPickerReady.wait(lock, [] { return !sPickerPending; });
    return sPickedPath;
}

} // namespace ComboFilePicker
#endif
