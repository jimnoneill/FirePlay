// FirePlay video renderer: H.264 NAL → AMediaCodec → Surface.
//
// Key design: SPS+PPS capture is decoupled from Surface availability.
// After reboot, video frames arrive before the Activity (and its Surface)
// exists. We extract SPS+PPS from the first frame, buffer it, and configure
// MediaCodec with CSD-0 when the Surface finally appears.

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
#include <unistd.h>

#define TAG "FirePlay-video"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

#include <jni.h>
extern JavaVM *g_jvm;
extern jclass  g_main_class;

void notify_main(const char *method) {
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
bool           g_csd_sent = false;
std::thread    g_drain_thread;
std::atomic<bool> g_drain_run{false};
std::vector<uint8_t> g_sps_pps;

void drain_loop() {
    int rendered = 0;
    while (g_drain_run.load()) {
        AMediaCodecBufferInfo info;
        ssize_t idx = AMediaCodec_dequeueOutputBuffer(g_codec, &info, 10 * 1000);
        if (idx >= 0) {
            AMediaCodec_releaseOutputBuffer(g_codec, idx, true);
            rendered++;
            if (rendered == 1) notify_main("onFirstFrame");
            if (rendered <= 3 || rendered % 60 == 0)
                LOGI("out#%d size=%d", rendered, info.size);
        } else if (idx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            AMediaFormat *fmt = AMediaCodec_getOutputFormat(g_codec);
            LOGI("format: %s", AMediaFormat_toString(fmt));
            AMediaFormat_delete(fmt);
        } else if (idx == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
            static int idle = 0;
            if (++idle % 500 == 0) LOGI("drain idle rendered=%d", rendered);
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
        g_csd_sent = false;
        notify_main("onMediaIdle");
    }
}

bool start_codec_locked(int width, int height) {
    if (!g_window) return false;
    const char *mime = g_is_h265 ? "video/hevc" : "video/avc";
    g_codec = AMediaCodec_createDecoderByType(mime);
    if (!g_codec) { LOGE("createDecoder failed"); return false; }

    AMediaFormat *fmt = AMediaFormat_new();
    AMediaFormat_setString(fmt, AMEDIAFORMAT_KEY_MIME, mime);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_WIDTH, width);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_HEIGHT, height);

    if (!g_sps_pps.empty()) {
        AMediaFormat_setBuffer(fmt, "csd-0", g_sps_pps.data(), g_sps_pps.size());
        LOGI("codec configured with CSD-0 (%zu bytes)", g_sps_pps.size());
    }

    media_status_t s = AMediaCodec_configure(g_codec, fmt, g_window, NULL, 0);
    AMediaFormat_delete(fmt);
    if (s != AMEDIA_OK) { LOGE("configure %d", s); return false; }
    s = AMediaCodec_start(g_codec);
    if (s != AMEDIA_OK) { LOGE("start %d", s); return false; }
    g_started = true;
    g_csd_sent = !g_sps_pps.empty();
    g_drain_run = true;
    g_drain_thread = std::thread(drain_loop);
    LOGI("codec started %dx%d %s csd=%d", width, height, mime, g_csd_sent);
    return true;
}

// Scan for SPS+PPS in Annex-B stream and store them.
void extract_sps_pps(const uint8_t *data, int len) {
    if (g_sps_pps.size() > 0) return;  // already have them
    // Look for start codes and NAL types
    for (int i = 0; i + 4 < len; i++) {
        if (data[i]==0 && data[i+1]==0 && data[i+2]==0 && data[i+3]==1) {
            int nal_type = data[i+4] & 0x1f;
            if (nal_type == 7 || nal_type == 8) {
                // Found SPS or PPS — just store the entire buffer as SPS+PPS
                g_sps_pps.assign(data, data + len);
                LOGI("extracted SPS+PPS from stream (%d bytes, first NAL type=%d)", len, nal_type);
                return;
            }
        }
    }
}

} // namespace

void fireplay_video_set_surface(void *anw) {
    std::lock_guard<std::mutex> g(g_lock);
    if (g_window) { ANativeWindow_release(g_window); g_window = nullptr; }
    if (anw) {
        g_window = (ANativeWindow *)anw;
        ANativeWindow_acquire(g_window);
        LOGI("surface set %dx%d", ANativeWindow_getWidth(g_window),
             ANativeWindow_getHeight(g_window));
        // If we have buffered SPS+PPS but no codec yet, start now.
        if (!g_codec && !g_sps_pps.empty()) {
            start_codec_locked(1920, 1080);
        }
    } else {
        teardown_codec_locked();
    }
}

void fireplay_video_set_codec(int is_h265) {
    std::lock_guard<std::mutex> g(g_lock);
    if (g_started && g_is_h265 == (bool)is_h265) return;
    teardown_codec_locked();
    g_is_h265 = (bool)is_h265;
    g_csd_sent = false;
    g_sps_pps.clear();
    LOGI("codec=%s", g_is_h265 ? "h265" : "h264");
}

void fireplay_video_set_sps_pps(const uint8_t *sps, int sps_len, const uint8_t *pps, int pps_len) {
    std::lock_guard<std::mutex> g(g_lock);
    g_sps_pps.clear();
    const uint8_t sc[] = {0x00, 0x00, 0x00, 0x01};
    g_sps_pps.insert(g_sps_pps.end(), sc, sc + 4);
    g_sps_pps.insert(g_sps_pps.end(), sps, sps + sps_len);
    g_sps_pps.insert(g_sps_pps.end(), sc, sc + 4);
    g_sps_pps.insert(g_sps_pps.end(), pps, pps + pps_len);
    g_csd_sent = false;
    LOGI("SPS+PPS stored: %d + %d bytes", sps_len, pps_len);
}

void fireplay_video_push_nal(const uint8_t *data, int len, uint64_t pts_ntp) {
    (void)pts_ntp;
    if (len <= 0) return;

    std::lock_guard<std::mutex> g(g_lock);

    // Always try to extract SPS+PPS from the stream.
    extract_sps_pps(data, len);

    if (!g_window) return;  // No Surface yet — frames buffered via SPS+PPS extract only.

    if (!g_codec) {
        if (!start_codec_locked(1920, 1080)) return;
    }

    // Feed SPS+PPS as CODEC_CONFIG if not yet sent and codec just started without CSD.
    if (!g_csd_sent && !g_sps_pps.empty()) {
        ssize_t ci = AMediaCodec_dequeueInputBuffer(g_codec, 50 * 1000);
        if (ci >= 0) {
            size_t ccap = 0;
            uint8_t *cbuf = AMediaCodec_getInputBuffer(g_codec, ci, &ccap);
            if (cbuf && ccap >= g_sps_pps.size()) {
                memcpy(cbuf, g_sps_pps.data(), g_sps_pps.size());
                AMediaCodec_queueInputBuffer(g_codec, ci, 0, g_sps_pps.size(), 0,
                    AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG);
                LOGI("fed CSD (%zu bytes)", g_sps_pps.size());
            }
        }
        g_csd_sent = true;
    }

    static int n = 0, queued = 0, drop_in = 0;
    n++;
    static int64_t pts_counter = 0;
    int64_t pts_us = pts_counter++ * 33333LL;

    ssize_t idx = AMediaCodec_dequeueInputBuffer(g_codec, 10 * 1000);
    if (idx < 0) { drop_in++; }
    else {
        size_t cap = 0;
        uint8_t *buf = AMediaCodec_getInputBuffer(g_codec, idx, &cap);
        if (!buf || (int)cap < len) {
            AMediaCodec_queueInputBuffer(g_codec, idx, 0, 0, 0, 0);
        } else {
            memcpy(buf, data, len);
            AMediaCodec_queueInputBuffer(g_codec, idx, 0, len, pts_us, 0);
            queued++;
        }
    }
    if (n <= 5 || n % 60 == 0) {
        uint8_t t = len > 4 ? data[4] & 0x1f : 0;
        LOGI("nal#%d len=%d q=%d drop=%d type=0x%02x", n, len, queued, drop_in, t);
    }
}

void fireplay_video_shutdown() {
    std::lock_guard<std::mutex> g(g_lock);
    teardown_codec_locked();
    if (g_window) { ANativeWindow_release(g_window); g_window = nullptr; }
    g_sps_pps.clear();
    LOGI("shutdown");
}
