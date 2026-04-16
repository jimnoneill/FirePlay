// FirePlay audio renderer: ALAC decode + AAudio output.
// iPhone Music sends ALAC-encoded packets over AirPlay 2 RAOP. We decode
// each packet to interleaved 16-bit stereo PCM via Apple's open-source ALAC
// decoder, then write to an AAudio output stream.

#include "audio_renderer_aaudio.h"
#include <ALACDecoder.h>
#include <ALACBitUtilities.h>
#include <aaudio/AAudio.h>
#include <android/log.h>
#include <atomic>
#include <mutex>
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
constexpr int kFrameSize  = 352;  // ALAC AirPlay frame size in samples per channel

ALACDecoder *g_decoder = nullptr;
AAudioStream *g_stream = nullptr;
std::mutex g_lock;
std::vector<uint8_t> g_pcm_buf;

// Apple ALAC magic cookie for 44.1kHz/16-bit/2ch/352-frame ALAC.
// Built per the format described in ALACMagicCookieDescription.txt.
const uint8_t kMagicCookie[] = {
    // ALACSpecificConfig
    0x00, 0x00, 0x01, 0x60,        // frameLength = 352
    0x00,                          // compatibleVersion
    0x10,                          // bitDepth = 16
    0x28,                          // pb (rice param)
    0x0a,                          // mb
    0x0e,                          // kb
    0x02,                          // numChannels
    0x00, 0xff,                    // maxRun
    0x00, 0x00, 0x10, 0x00,        // maxFrameBytes
    0x00, 0x02, 0x80, 0x00,        // avgBitRate (~163 kbps)
    0x00, 0x00, 0xac, 0x44,        // sampleRate = 44100
};

bool open_aaudio() {
    AAudioStreamBuilder *builder = nullptr;
    aaudio_result_t r = AAudio_createStreamBuilder(&builder);
    if (r != AAUDIO_OK) { LOGE("createStreamBuilder %d", r); return false; }
    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setSampleRate(builder, kSampleRate);
    AAudioStreamBuilder_setChannelCount(builder, kChannels);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_NONE);
    AAudioStreamBuilder_setUsage(builder, AAUDIO_USAGE_MEDIA);
    AAudioStreamBuilder_setBufferCapacityInFrames(builder, kSampleRate);  // ~1s buffer

    r = AAudioStreamBuilder_openStream(builder, &g_stream);
    AAudioStreamBuilder_delete(builder);
    if (r != AAUDIO_OK) { LOGE("openStream %d", r); return false; }
    /* Set buffer size to ~250ms to give us headroom over RTP jitter. */
    AAudioStream_setBufferSizeInFrames(g_stream, kSampleRate / 4);
    r = AAudioStream_requestStart(g_stream);
    if (r != AAUDIO_OK) { LOGE("requestStart %d", r); return false; }
    LOGI("AAudio output opened: %d Hz, %d ch, PCM_I16, bufCap=%d bufSize=%d",
         kSampleRate, kChannels,
         AAudioStream_getBufferCapacityInFrames(g_stream),
         AAudioStream_getBufferSizeInFrames(g_stream));
    return true;
}

bool open_decoder() {
    g_decoder = new ALACDecoder();
    int32_t status = g_decoder->Init((void *)kMagicCookie, sizeof(kMagicCookie));
    if (status != 0) {
        LOGE("ALACDecoder Init failed status=%d", status);
        delete g_decoder; g_decoder = nullptr;
        return false;
    }
    LOGI("ALAC decoder initialized");
    return true;
}

} // namespace

void fireplay_audio_init() {
    std::lock_guard<std::mutex> g(g_lock);
    if (!g_decoder)  open_decoder();
    if (!g_stream)   open_aaudio();
    g_pcm_buf.assign(kFrameSize * kChannels * sizeof(int16_t) * 4, 0);
}

void fireplay_audio_shutdown() {
    std::lock_guard<std::mutex> g(g_lock);
    if (g_stream) {
        AAudioStream_requestStop(g_stream);
        AAudioStream_close(g_stream);
        g_stream = nullptr;
    }
    if (g_decoder) {
        delete g_decoder;
        g_decoder = nullptr;
    }
}

void fireplay_audio_push_alac(const uint8_t *encoded, int encoded_len) {
    std::lock_guard<std::mutex> g(g_lock);
    if (!g_decoder || !g_stream || encoded_len <= 0) return;

    BitBuffer bits;
    BitBufferInit(&bits, const_cast<uint8_t *>(encoded), (uint32_t)encoded_len);
    uint32_t out_frames = 0;
    int32_t status = g_decoder->Decode(&bits, g_pcm_buf.data(), kFrameSize, kChannels, &out_frames);
    if (status != 0 || out_frames == 0) {
        LOGW("ALAC decode failed status=%d frames=%u (encoded_len=%d)", status, out_frames, encoded_len);
        return;
    }

    /* Blocking write up to 100ms — prevents drops when buffer is briefly full. */
    aaudio_result_t r = AAudioStream_write(g_stream, g_pcm_buf.data(), out_frames, 100 * 1000 * 1000LL);

    static int n = 0, ok = 0, short_writes = 0, errs = 0;
    n++;
    if (r < 0) { errs++; }
    else if ((uint32_t)r < out_frames) { short_writes++; }
    else { ok++; }
    if (n % 100 == 0) {
        LOGI("decode#%d ok=%d short=%d err=%d state=%d xrun=%d",
             n, ok, short_writes, errs,
             AAudioStream_getState(g_stream),
             AAudioStream_getXRunCount(g_stream));
    }
    if (r < 0 && (errs == 1 || errs % 50 == 0)) LOGW("AAudio write err %d", r);
}
