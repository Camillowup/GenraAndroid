// SPDX-License-Identifier: GPL-3.0-only
// GENRA BOX for Android: the bridge to Java (ch.camillowup.genrabox.Native) and the audio output (AAudio).
#include <aaudio/AAudio.h>
#include <android/log.h>
#include <jni.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

#include "BoxEngine.hpp"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "GenraBox", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "GenraBox", __VA_ARGS__)

namespace {

gbox::Engine* engine() {
	static gbox::Engine* e = new gbox::Engine();  // lives as long as the process
	return e;
}

std::mutex streamMtx;
AAudioStream* stream = nullptr;
std::atomic<bool> wanted{false};
std::string lastExportName;
std::string audioText = "Audio: off";

const int CHUNK = 512;
float bufL[CHUNK], bufR[CHUNK];

aaudio_data_callback_result_t dataCallback(AAudioStream*, void*, void* audioData, int32_t numFrames) {
	float* out = static_cast<float*>(audioData);
	int done = 0;
	while (done < numFrames) {
		int n = numFrames - done < CHUNK ? numFrames - done : CHUNK;
		engine()->render(bufL, bufR, n);
		for (int i = 0; i < n; i++) {
			out[2 * (done + i)] = bufL[i];
			out[2 * (done + i) + 1] = bufR[i];
		}
		done += n;
	}
	return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

bool openStream();

void closeStream() {
	if (stream) {
		AAudioStream_requestStop(stream);
		AAudioStream_close(stream);
		stream = nullptr;
	}
}

// the device changed (headphones in or out, Bluetooth): a new stream, on a thread of its own
void errorCallback(AAudioStream*, void*, aaudio_result_t err) {
	LOGE("stream error %d", (int) err);
	if (err != AAUDIO_ERROR_DISCONNECTED)
		return;
	std::thread([] {
		std::lock_guard<std::mutex> lk(streamMtx);
		closeStream();
		if (wanted.load())
			openStream();
	}).detach();
}

bool openStream() {
	AAudioStreamBuilder* b = nullptr;
	if (AAudio_createStreamBuilder(&b) != AAUDIO_OK)
		return false;
	AAudioStreamBuilder_setDirection(b, AAUDIO_DIRECTION_OUTPUT);
	AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
	AAudioStreamBuilder_setSharingMode(b, AAUDIO_SHARING_MODE_SHARED);
	AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_FLOAT);
	AAudioStreamBuilder_setChannelCount(b, 2);
	AAudioStreamBuilder_setDataCallback(b, dataCallback, nullptr);
	AAudioStreamBuilder_setErrorCallback(b, errorCallback, nullptr);
	aaudio_result_t r = AAudioStreamBuilder_openStream(b, &stream);
	AAudioStreamBuilder_delete(b);
	if (r != AAUDIO_OK || !stream) {
		LOGE("openStream failed: %s", AAudio_convertResultToText(r));
		audioText = std::string("Audio: ") + AAudio_convertResultToText(r);
		stream = nullptr;
		return false;
	}
	int sr = AAudioStream_getSampleRate(stream);
	int burst = AAudioStream_getFramesPerBurst(stream);
	// two bursts of buffer: low latency, but room for a busy moment
	AAudioStream_setBufferSizeInFrames(stream, burst * 2);
	engine()->setSampleRate((float) sr);
	r = AAudioStream_requestStart(stream);
	if (r != AAUDIO_OK) {
		LOGE("requestStart failed: %s", AAudio_convertResultToText(r));
		audioText = std::string("Audio: ") + AAudio_convertResultToText(r);
		closeStream();
		return false;
	}
	char t[128];
	std::snprintf(t, sizeof t, "%d Hz · burst %d frames · buffer %d frames", sr, burst,
	              (int) AAudioStream_getBufferSizeInFrames(stream));
	audioText = t;
	LOGI("audio: %s", t);
	return true;
}

std::string str(JNIEnv* env, jstring s) {
	if (!s)
		return {};
	const char* c = env->GetStringUTFChars(s, nullptr);
	std::string r = c ? c : "";
	if (c)
		env->ReleaseStringUTFChars(s, c);
	return r;
}
/** UTF-8 into a Java string (NewStringUTF wants modified UTF-8: fine for the text here, but go through bytes). */
jstring jstr(JNIEnv* env, const std::string& s) {
	jbyteArray bytes = env->NewByteArray((jsize) s.size());
	env->SetByteArrayRegion(bytes, 0, (jsize) s.size(), reinterpret_cast<const jbyte*>(s.data()));
	jclass cls = env->FindClass("java/lang/String");
	jmethodID ctor = env->GetMethodID(cls, "<init>", "([BLjava/lang/String;)V");
	jstring enc = env->NewStringUTF("UTF-8");
	jstring out = (jstring) env->NewObject(cls, ctor, bytes, enc);
	env->DeleteLocalRef(bytes);
	env->DeleteLocalRef(enc);
	env->DeleteLocalRef(cls);
	return out;
}

}  // namespace

extern "C" {

JNIEXPORT void JNICALL Java_ch_camillowup_genrabox_Native_init(JNIEnv* env, jclass, jstring dir) {
	engine()->init(str(env, dir));
}

JNIEXPORT jboolean JNICALL Java_ch_camillowup_genrabox_Native_start(JNIEnv*, jclass) {
	std::lock_guard<std::mutex> lk(streamMtx);
	wanted = true;
	if (stream)
		return JNI_TRUE;
	return openStream() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_ch_camillowup_genrabox_Native_stop(JNIEnv*, jclass) {
	std::lock_guard<std::mutex> lk(streamMtx);
	wanted = false;
	closeStream();
	audioText = "Audio: off";
}

JNIEXPORT jstring JNICALL Java_ch_camillowup_genrabox_Native_meta(JNIEnv* env, jclass) {
	return jstr(env, engine()->meta());
}

JNIEXPORT jstring JNICALL Java_ch_camillowup_genrabox_Native_state(JNIEnv* env, jclass, jint known) {
	return jstr(env, engine()->state(known));
}

JNIEXPORT jstring JNICALL Java_ch_camillowup_genrabox_Native_fast(JNIEnv* env, jclass) {
	return jstr(env, engine()->fast());
}

JNIEXPORT void JNICALL Java_ch_camillowup_genrabox_Native_cmd(JNIEnv*, jclass, jint op, jint a, jint b, jfloat f) {
	engine()->cmd(op, a, b, f);
}

JNIEXPORT void JNICALL Java_ch_camillowup_genrabox_Native_savePreset(JNIEnv* env, jclass, jint track, jstring name) {
	engine()->savePreset(track, str(env, name));
}

JNIEXPORT void JNICALL Java_ch_camillowup_genrabox_Native_save(JNIEnv*, jclass) {
	engine()->saveSession();
}

JNIEXPORT jbyteArray JNICALL Java_ch_camillowup_genrabox_Native_exportMidi(JNIEnv* env, jclass, jint what, jint idx) {
	std::vector<uint8_t> d = engine()->exportMidi(what, idx, lastExportName);
	jbyteArray out = env->NewByteArray((jsize) d.size());
	if (!d.empty())
		env->SetByteArrayRegion(out, 0, (jsize) d.size(), reinterpret_cast<const jbyte*>(d.data()));
	return out;
}

JNIEXPORT jstring JNICALL Java_ch_camillowup_genrabox_Native_exportName(JNIEnv* env, jclass) {
	return jstr(env, lastExportName);
}

JNIEXPORT jstring JNICALL Java_ch_camillowup_genrabox_Native_audioInfo(JNIEnv* env, jclass) {
	std::lock_guard<std::mutex> lk(streamMtx);
	return jstr(env, audioText);
}

}  // extern "C"
