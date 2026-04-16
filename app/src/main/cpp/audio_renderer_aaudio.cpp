// FirePlay audio renderer.
//   - Music app (ct=2 ALAC): decoded by Apple ALAC, pushed to AAudio.
//   - Photos/Video (ct=8 AAC-ELD): decoded by Android MediaCodec, pushed to AAudio.

#include "audio_renderer_aaudio.h"
#include <ALACDecoder.h>
#include <ALACBitUtilities.h>
#include <aaudio/AAudio.h>
#include <android/log.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
#include <cstring>

#define TAG "FirePlay-audio"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

namespace {

constexpr int kSampleRate = 44100;
constexpr int kChannels   = 2;
constexpr int kBitDepth   = 16;
constexpr int kAlacFrameSize = 352;

ALACDecoder *g_alac = nullptr;
AAudioStream *g_stream = nullptr;
std::mutex g_lock;
std::vector<uint8_t> g_pcm_buf;

/* AAC ELD MediaCodec decoder. */
AMediaCodec *g_aac = nullptr;
bool g_aac_started = false;
std::thread g_aac_drain;
std::atomic<bool> g_aac_drain_run{false};

const uint8_t kAlacCookie[] = {
    0x00, 0x00, 0x01, 0x60, 0x00, 0x10, 0x28, 0x0a,
    0x0e, 0x02, 0x00, 0xff, 0x00, 0x00, 0x10, 0x00,
    0x00, 0x02, 0x80, 0x00, 0x00, 0x00, 0xac, 0x44,
};

bool open_aaudio() {
    AAudioStreamBuilder *b = nullptr;
    if (AAudio_createStreamBuilder(&b) != AAUDIO_OK) return false;
    AAudioStreamBuilder_setDirection(b, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setSampleRate(b, kSampleRate);
    AAudioStreamBuilder_setChannelCount(b, kChannels);
    AAudioStreamBuilder_setSharingMode(b, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_NONE);
    AAudioStreamBuilder_setUsage(b, AAUDIO_USAGE_MEDIA);
    AAudioStreamBuilder_setBufferCapacityInFrames(b, kSampleRate);
    aaudio_result_t r = AAudioStreamBuilder_openStream(b, &g_stream);
    AAudioStreamBuilder_delete(b);
    if (r != AAUDIO_OK) { LOGE("openStream %d", r); return false; }
    AAudioStream_setBufferSizeInFrames(g_stream, kSampleRate / 4);
    if (AAudioStream_requestStart(g_stream) != AAUDIO_OK) { LOGE("requestStart"); return false; }
    LOGI("AAudio opened: %d Hz, %d ch", kSampleRate, kChannels);
    return true;
}

void open_alac() {
    g_alac = new ALACDecoder();
    if (g_alac->Init((void *)kAlacCookie, sizeof(kAlacCookie)) != 0) {
        LOGE("ALAC init failed"); delete g_alac; g_alac = nullptr;
    } else {
        LOGI("ALAC ready");
    }
}

void aac_drain() {
    int rendered = 0;
    while (g_aac_drain_run.load()) {
        AMediaCodecBufferInfo info;
        ssize_t idx = AMediaCodec_dequeueOutputBuffer(g_aac, &info, 10 * 1000);
        if (idx >= 0) {
            size_t cap = 0;
            uint8_t *out = AMediaCodec_getOutputBuffer(g_aac, idx, &cap);
            if (out && info.size > 0 && g_stream) {
                int frames = info.size / (kChannels * sizeof(int16_t));
                AAudioStream_write(g_stream, out + info.offset, frames, 100 * 1000 * 1000LL);
                rendered++;
                if (rendered <= 3 || rendered % 200 == 0)
                    LOGI("aac out#%d size=%d frames=%d", rendered, info.size, frames);
            }
            AMediaCodec_releaseOutputBuffer(g_aac, idx, false);
        } else if (idx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            AMediaFormat *f = AMediaCodec_getOutputFormat(g_aac);
            LOGI("aac out format: %s", AMediaFormat_toString(f));
            AMediaFormat_delete(f);
        }
    }
}

bool open_aac_eld() {
    g_aac = AMediaCodec_createDecoderByType("audio/mp4a-latm");
    if (!g_aac) { LOGE("create aac decoder failed"); return false; }
    AMediaFormat *fmt = AMediaFormat_new();
    AMediaFormat_setString(fmt, AMEDIAFORMAT_KEY_MIME, "audio/mp4a-latm");
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_SAMPLE_RATE, kSampleRate);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_CHANNEL_COUNT, kChannels);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_AAC_PROFILE, 39);  // AAC_OBJECT_ELD
    /* AudioSpecificConfig for AAC ELD 44.1kHz stereo:
       AOT escape(11111) + (39-32)=7 (000111) + SRI 4 (0100) + ChCfg 2 (0010)
       = 1111 1000 1110 1000 0100 0010 = 0xF8 0xE8 0x42 */
    /* From UxPlay GStreamer caps: codec_data=f8e85000 for AAC-ELD 44.1kHz/2ch. */
    static const uint8_t csd[] = { 0xF8, 0xE8, 0x50, 0x00 };
    AMediaFormat_setBuffer(fmt, "csd-0", (void *)csd, sizeof(csd));
    media_status_t s = AMediaCodec_configure(g_aac, fmt, nullptr, nullptr, 0);
    AMediaFormat_delete(fmt);
    if (s != AMEDIA_OK) { LOGE("aac configure %d", s); return false; }
    if (AMediaCodec_start(g_aac) != AMEDIA_OK) { LOGE("aac start"); return false; }
    g_aac_started = true;
    g_aac_drain_run = true;
    g_aac_drain = std::thread(aac_drain);
    LOGI("AAC-ELD decoder started");
    return true;
}

void close_aac_eld() {
    g_aac_drain_run = false;
    if (g_aac_drain.joinable()) g_aac_drain.join();
    if (g_aac) {
        if (g_aac_started) AMediaCodec_stop(g_aac);
        AMediaCodec_delete(g_aac);
        g_aac = nullptr;
        g_aac_started = false;
    }
}

} // namespace

void fireplay_audio_init() {
    std::lock_guard<std::mutex> g(g_lock);
    if (!g_alac)   open_alac();
    if (!g_stream) open_aaudio();
    g_pcm_buf.assign(kAlacFrameSize * kChannels * sizeof(int16_t) * 4, 0);
}

void fireplay_audio_shutdown() {
    std::lock_guard<std::mutex> g(g_lock);
    close_aac_eld();
    if (g_stream) {
        AAudioStream_requestStop(g_stream);
        AAudioStream_close(g_stream);
        g_stream = nullptr;
    }
    if (g_alac) { delete g_alac; g_alac = nullptr; }
}

void fireplay_audio_push_alac(const uint8_t *encoded, int encoded_len) {
    std::lock_guard<std::mutex> g(g_lock);
    if (!g_alac || !g_stream || encoded_len <= 0) return;
    BitBuffer bits;
    BitBufferInit(&bits, const_cast<uint8_t *>(encoded), (uint32_t)encoded_len);
    uint32_t out_frames = 0;
    if (g_alac->Decode(&bits, g_pcm_buf.data(), kAlacFrameSize, kChannels, &out_frames) != 0) return;
    if (out_frames) AAudioStream_write(g_stream, g_pcm_buf.data(), out_frames, 100 * 1000 * 1000LL);
}

void fireplay_audio_push_aac_eld(const uint8_t *encoded, int encoded_len) {
    std::lock_guard<std::mutex> g(g_lock);
    if (!g_stream) open_aaudio();
    if (!g_aac && !open_aac_eld()) return;
    if (encoded_len <= 0) return;
    ssize_t idx = AMediaCodec_dequeueInputBuffer(g_aac, 10 * 1000);
    if (idx < 0) return;
    size_t cap = 0;
    uint8_t *buf = AMediaCodec_getInputBuffer(g_aac, idx, &cap);
    if (!buf || (int)cap < encoded_len) {
        AMediaCodec_queueInputBuffer(g_aac, idx, 0, 0, 0, 0);
        return;
    }
    memcpy(buf, encoded, encoded_len);
    static int n = 0;
    n++;
    int64_t pts = (int64_t)n * 10000LL;
    AMediaCodec_queueInputBuffer(g_aac, idx, 0, encoded_len, pts, 0);
}
