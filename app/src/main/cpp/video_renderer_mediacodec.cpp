// FirePlay video renderer: H.264/H.265 NAL units -> AMediaCodec -> Surface.
//
// iPhone Photos and screen mirror both stream H.264 NAL units (Annex-B) over
// AirPlay 2's RAOP mirror channel. We hand them to NDK AMediaCodec configured
// with an ANativeWindow output surface so frames render directly to the
// SurfaceView the Activity provides — zero CPU copy.

#include "video_renderer_mediacodec.h"
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
#include <cstring>

#define TAG "FirePlay-video"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

#include <jni.h>
extern JavaVM *g_jvm;
extern jclass  g_main_class;  // cached global ref, set in JNI_OnLoad

static void notify_main(const char *method) {
    if (!g_jvm || !g_main_class) return;
    JNIEnv *env = nullptr;
    bool attached = false;
    if (g_jvm->GetEnv((void **)&env, JNI_VERSION_1_6) != JNI_OK) {
        if (g_jvm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
        attached = true;
    }
    jmethodID m = env->GetStaticMethodID(g_main_class, method, "()V");
    if (m) env->CallStaticVoidMethod(g_main_class, m);
    if (attached) g_jvm->DetachCurrentThread();
}

namespace {

std::mutex g_lock;
ANativeWindow *g_window = nullptr;
AMediaCodec   *g_codec  = nullptr;
bool           g_is_h265 = false;
bool           g_started = false;
std::thread    g_drain_thread;
std::atomic<bool> g_drain_run{false};

void drain_loop() {
    int rendered = 0, try_again = 0, fmt_changed = 0;
    while (g_drain_run.load()) {
        AMediaCodecBufferInfo info;
        ssize_t idx = AMediaCodec_dequeueOutputBuffer(g_codec, &info, 10 * 1000);  // 10ms
        if (idx >= 0) {
            AMediaCodec_releaseOutputBuffer(g_codec, idx, true);
            rendered++;
            if (rendered == 1) notify_main("onFirstFrame");
            if (rendered <= 3 || rendered % 60 == 0) {
                LOGI("out#%d size=%d pts=%lld flags=%u",
                     rendered, info.size, (long long)info.presentationTimeUs, info.flags);
            }
        } else if (idx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            fmt_changed++;
            AMediaFormat *fmt = AMediaCodec_getOutputFormat(g_codec);
            LOGI("output format changed: %s", AMediaFormat_toString(fmt));
            AMediaFormat_delete(fmt);
        } else if (idx == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
            try_again++;
            if (try_again % 100 == 0) LOGI("drain idle: try_again=%d rendered=%d", try_again, rendered);
        } else {
            LOGW("drain: unexpected idx=%zd", idx);
        }
    }
}

void teardown_codec_locked() {
    g_drain_run = false;
    if (g_drain_thread.joinable()) g_drain_thread.join();
    if (g_codec) {
        if (g_started) AMediaCodec_stop(g_codec);
        AMediaCodec_delete(g_codec);
        g_codec = nullptr;
        g_started = false;
        notify_main("onMediaIdle");
    }
}

bool start_codec_locked(int width, int height) {
    if (!g_window) { LOGW("no surface"); return false; }
    const char *mime = g_is_h265 ? "video/hevc" : "video/avc";
    g_codec = AMediaCodec_createDecoderByType(mime);
    if (!g_codec) { LOGE("createDecoderByType %s failed", mime); return false; }

    AMediaFormat *fmt = AMediaFormat_new();
    AMediaFormat_setString(fmt, AMEDIAFORMAT_KEY_MIME, mime);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_WIDTH,  width);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_HEIGHT, height);

    media_status_t s = AMediaCodec_configure(g_codec, fmt, g_window, NULL, 0);
    AMediaFormat_delete(fmt);
    if (s != AMEDIA_OK) { LOGE("configure failed %d", s); return false; }
    s = AMediaCodec_start(g_codec);
    if (s != AMEDIA_OK) { LOGE("start failed %d", s); return false; }
    g_started = true;
    g_drain_run = true;
    g_drain_thread = std::thread(drain_loop);
    LOGI("codec started %dx%d %s", width, height, mime);
    return true;
}

} // namespace

void fireplay_video_set_surface(void *anw) {
    std::lock_guard<std::mutex> g(g_lock);
    if (g_window) { ANativeWindow_release(g_window); g_window = nullptr; }
    if (anw) {
        g_window = (ANativeWindow *)anw;
        ANativeWindow_acquire(g_window);
        LOGI("surface set: %dx%d", ANativeWindow_getWidth(g_window),
             ANativeWindow_getHeight(g_window));
    } else {
        teardown_codec_locked();
        LOGI("surface cleared");
    }
}

void fireplay_video_set_codec(int is_h265) {
    std::lock_guard<std::mutex> g(g_lock);
    if (g_started && g_is_h265 == (bool)is_h265) return;
    teardown_codec_locked();
    g_is_h265 = (bool)is_h265;
    LOGI("codec=%s (lazy start on first NAL with size from SPS)",
         g_is_h265 ? "h265" : "h264");
}

void fireplay_video_push_nal(const uint8_t *data, int len, uint64_t pts_ntp) {
    (void)pts_ntp;  // NTP timestamps overflow MediaCodec internal math.
    std::lock_guard<std::mutex> g(g_lock);
    if (!g_window || len <= 0) return;
    if (!g_codec) {
        if (!start_codec_locked(1920, 1080)) return;
    }

    static int n = 0, queued = 0, drop_no_input = 0, drop_too_big = 0;
    n++;
    /* Monotonic ~30fps timestamps in microseconds keep MediaCodec happy. */
    int64_t pts_us = (int64_t)n * 33333LL;

    ssize_t idx = AMediaCodec_dequeueInputBuffer(g_codec, 10 * 1000);
    if (idx < 0) { drop_no_input++; }
    else {
        size_t cap = 0;
        uint8_t *buf = AMediaCodec_getInputBuffer(g_codec, idx, &cap);
        if (!buf || (int)cap < len) {
            AMediaCodec_queueInputBuffer(g_codec, idx, 0, 0, 0, 0);
            drop_too_big++;
        } else {
            memcpy(buf, data, len);
            AMediaCodec_queueInputBuffer(g_codec, idx, 0, len, pts_us, 0);
            queued++;
        }
    }
    if (n <= 5 || n % 60 == 0) {
        unsigned char b0 = data[0], b1 = len > 1 ? data[1] : 0,
                      b2 = len > 2 ? data[2] : 0, b3 = len > 3 ? data[3] : 0,
                      b4 = len > 4 ? data[4] : 0;
        LOGI("nal#%d len=%d q=%d drop_in=%d drop_big=%d head=%02x%02x%02x%02x nal_type=0x%02x",
             n, len, queued, drop_no_input, drop_too_big, b0, b1, b2, b3, b4 & 0x1f);
    }
}

void fireplay_video_shutdown() {
    std::lock_guard<std::mutex> g(g_lock);
    teardown_codec_locked();
    if (g_window) { ANativeWindow_release(g_window); g_window = nullptr; }
    LOGI("shutdown");
}
