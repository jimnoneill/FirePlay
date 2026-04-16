// FirePlay JNI bridge — wires UxPlay's protocol library into the
// Android process. Renderers are no-ops for now; the goal of this layer
// is to get pairing + AirPlay HTTP working end-to-end.

#include <jni.h>
#include <string>
#include <android/log.h>
#include <pthread.h>

extern "C" {
#include "../../../../lib-uxplay/raop.h"
#include "../../../../lib-uxplay/dnssd.h"
#include "../../../../lib-uxplay/logger.h"
#include "../../../../lib-uxplay/stream.h"
}
#include "audio_renderer_aaudio.h"
#include "video_renderer_mediacodec.h"
#include <android/native_window_jni.h>

extern void notify_main(const char *method);

#define TAG "FirePlay-jni"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

JavaVM *g_jvm = nullptr;
jclass  g_main_class = nullptr;
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *) {
    g_jvm = vm;
    JNIEnv *env = nullptr;
    if (vm->GetEnv((void **)&env, JNI_VERSION_1_6) == JNI_OK) {
        jclass local = env->FindClass("org/fireplay/MainActivity");
        if (local) g_main_class = (jclass)env->NewGlobalRef(local);
    }
    return JNI_VERSION_1_6;
}

namespace {

raop_t   *g_raop = nullptr;
dnssd_t  *g_dnssd = nullptr;
logger_t *g_logger = nullptr;

void log_cb(void *, int level, const char *msg) {
    /* Map all UxPlay levels to at least INFO so logcat shows them.
       Android's default logcat filter hides DEBUG. */
    int prio = (level <= LOGGER_ERR)     ? ANDROID_LOG_ERROR :
               (level <= LOGGER_WARNING) ? ANDROID_LOG_WARN  : ANDROID_LOG_INFO;
    __android_log_print(prio, "FirePlay-uxplay", "%s", msg);
}

void cb_audio_process(void *, raop_ntp_t *, audio_decode_struct *data) {
    if (!data || data->data_len <= 0) return;
    static unsigned char last_ct = 0xff;
    if (data->ct != last_ct) {
        LOGI("audio ct changed: %u -> %u (len=%d)", last_ct, data->ct, data->data_len);
        last_ct = data->ct;
    }
    if (data->ct == 2) {
        fireplay_audio_push_alac(data->data, data->data_len);
    } else if (data->ct == 8) {
        fireplay_audio_push_aac_eld(data->data, data->data_len);
    }
}
void cb_conn_init_renderer(void *) {
    fireplay_audio_init();
    notify_main("onConnectionStart");
    LOGI("conn_init: audio+video ready");
}
void cb_conn_destroy_renderer(void *) {
    fireplay_audio_shutdown();
    fireplay_video_shutdown();
    LOGI("conn_destroy: audio+video torn down");
}
void cb_video_process(void *, raop_ntp_t *, video_decode_struct *data) {
    if (!data || data->data_len <= 0) return;
    fireplay_video_push_nal(data->data, data->data_len, data->ntp_time_local);
}
void cb_video_pause  (void *)                                                  {}
void cb_video_resume (void *)                                                  {}
void cb_conn_feedback(void *)                                                  {}
void cb_conn_reset   (void *, int reason)                                      { LOGI("conn_reset reason=%d", reason); }
void cb_video_reset  (void *, reset_type_t)                                    {}
void cb_conn_teardown(void *, bool *t96, bool *t110)                           { if (t96) *t96 = false; if (t110) *t110 = false; }
void cb_audio_flush  (void *)                                                  {}
void cb_video_flush  (void *)                                                  {}
double cb_audio_set_client_volume(void *)                                      { return -30.0; }
void cb_audio_set_volume(void *, float)                                        {}
void cb_audio_set_metadata(void *, const void *, int)                          {}
void cb_audio_set_coverart(void *, const void *, int)                          {}
void cb_audio_stop_coverart_rendering(void *)                                  {}
void cb_audio_remote_control_id(void *, const char *, const char *)            {}
void cb_audio_set_progress(void *, uint32_t *, uint32_t *, uint32_t *)         {}
void cb_audio_get_format(void *, unsigned char *ct, unsigned short *spf, bool *usingScreen, bool *isMedia, uint64_t *audioFormat) {
    /* Inputs are populated by UxPlay from iPhone's SETUP — log only, don't clobber. */
    LOGI("audio_get_format ct=%u spf=%u screen=%d media=%d fmt=0x%llx",
         ct?*ct:0, spf?*spf:0, usingScreen?*usingScreen:0, isMedia?*isMedia:0,
         audioFormat?(unsigned long long)*audioFormat:0);
}
void cb_video_report_size(void *, float *, float *, float *, float *)          {}
void cb_mirror_video_running(void *, bool running)                             { LOGI("mirror_video_running=%d", running); }
void cb_report_client_request(void *, char *deviceid, char *model, char *name, bool *admit) {
    LOGI("client request deviceid=%s model=%s name=%s", deviceid?deviceid:"", model?model:"", name?name:"");
    if (admit) *admit = true;
}
void cb_display_pin(void *, char *)                                            {}
void cb_register_client(void *, const char *device_id, const char *pk_str, const char *name) {
    LOGI("register_client device=%s name=%s", device_id?device_id:"", name?name:"");
}
bool cb_check_register(void *, const char *)                                   { return true; }
const char *cb_passwd(void *, int *len)                                        { if (len) *len = 0; return nullptr; }
void cb_export_dacp(void *, const char *, const char *)                        {}
int  cb_video_set_codec(void *, video_codec_t codec) {
    LOGI("video_codec=%d", codec);
    fireplay_video_set_codec(codec == VIDEO_CODEC_H265 ? 1 : 0);
    return 0;
}
void cb_on_video_play (void *, const char *, const float)                      {}
void cb_on_video_scrub(void *, const float)                                    {}
void cb_on_video_rate (void *, const float)                                    {}
void cb_on_video_stop (void *)                                                 {}
void cb_on_video_acquire_playback_info(void *, playback_info_t *)              {}
float cb_on_video_playlist_remove(void *)                                      { return 0.0f; }

} // namespace

extern "C" {

static char g_mac_str[32] = "00:15:5d:62:9a:dd";
static char g_pi_str[64] = "00000000-0000-0000-0000-000000000000";
static char g_keyfile[512] = "";

JNIEXPORT jint JNICALL
Java_org_fireplay_MainActivity_nativeStart(JNIEnv *env, jclass, jstring jname, jint airplayPort, jint raopPort, jstring jmac, jstring jpi, jstring jkeyfile) {
    const char *name = env->GetStringUTFChars(jname, nullptr);
    const char *mac  = env->GetStringUTFChars(jmac, nullptr);
    const char *pi   = env->GetStringUTFChars(jpi, nullptr);
    const char *kf   = env->GetStringUTFChars(jkeyfile, nullptr);
    LOGI("nativeStart name=%s airplay=%d raop=%d mac=%s pi=%s keyfile=%s", name, airplayPort, raopPort, mac, pi, kf);
    strncpy(g_mac_str, mac, sizeof(g_mac_str)-1);
    strncpy(g_pi_str, pi, sizeof(g_pi_str)-1);
    strncpy(g_keyfile, kf, sizeof(g_keyfile)-1);
    env->ReleaseStringUTFChars(jkeyfile, kf);
    env->ReleaseStringUTFChars(jmac, mac);
    env->ReleaseStringUTFChars(jpi, pi);

    if (g_raop) {
        LOGW("nativeStart called while already running");
        env->ReleaseStringUTFChars(jname, name);
        return 0;
    }

    raop_callbacks_t cb = {};
    cb.cls = nullptr;
    cb.audio_process = cb_audio_process;
    cb.video_process = cb_video_process;
    cb.video_pause = cb_video_pause;
    cb.video_resume = cb_video_resume;
    cb.conn_feedback = cb_conn_feedback;
    cb.conn_reset = cb_conn_reset;
    cb.video_reset = cb_video_reset;
    cb.conn_init = cb_conn_init_renderer;
    cb.conn_destroy = cb_conn_destroy_renderer;
    cb.conn_teardown = cb_conn_teardown;
    cb.audio_flush = cb_audio_flush;
    cb.video_flush = cb_video_flush;
    cb.audio_set_client_volume = cb_audio_set_client_volume;
    cb.audio_set_volume = cb_audio_set_volume;
    cb.audio_set_metadata = cb_audio_set_metadata;
    cb.audio_set_coverart = cb_audio_set_coverart;
    cb.audio_stop_coverart_rendering = cb_audio_stop_coverart_rendering;
    cb.audio_remote_control_id = cb_audio_remote_control_id;
    cb.audio_set_progress = cb_audio_set_progress;
    cb.audio_get_format = cb_audio_get_format;
    cb.video_report_size = cb_video_report_size;
    cb.mirror_video_running = cb_mirror_video_running;
    cb.report_client_request = cb_report_client_request;
    cb.display_pin = cb_display_pin;
    cb.register_client = cb_register_client;
    cb.check_register = cb_check_register;
    cb.passwd = cb_passwd;
    cb.export_dacp = cb_export_dacp;
    cb.video_set_codec = cb_video_set_codec;
    cb.on_video_play = cb_on_video_play;
    cb.on_video_scrub = cb_on_video_scrub;
    cb.on_video_rate = cb_on_video_rate;
    cb.on_video_stop = cb_on_video_stop;
    cb.on_video_acquire_playback_info = cb_on_video_acquire_playback_info;
    cb.on_video_playlist_remove = cb_on_video_playlist_remove;

    LOGI("calling raop_init");
    g_raop = raop_init(&cb);
    if (!g_raop) {
        LOGE("raop_init returned null");
        env->ReleaseStringUTFChars(jname, name);
        return -1;
    }
    LOGI("raop_init OK");

    raop_set_log_level(g_raop, LOGGER_DEBUG);
    raop_set_log_callback(g_raop, log_cb, nullptr);
    LOGI("logger wired");

    /* Stable device ID derived from a fake MAC. iOS will key client trust on this. */
    /* Parse MAC from string "aa:bb:cc:dd:ee:ff" into 6 bytes.
       On Android 9, wlan0 MAC may be randomized. Use our advertised MAC
       so iPhone's key derivation matches. */
    char hw_addr[6] = {};
    { unsigned int m[6];
      const char *mac = g_mac_str;
      if (sscanf(mac, "%x:%x:%x:%x:%x:%x", &m[0],&m[1],&m[2],&m[3],&m[4],&m[5]) == 6)
          for (int i=0;i<6;i++) hw_addr[i]=(char)m[i];
      LOGI("hw_addr=%02x:%02x:%02x:%02x:%02x:%02x",
           (uint8_t)hw_addr[0],(uint8_t)hw_addr[1],(uint8_t)hw_addr[2],
           (uint8_t)hw_addr[3],(uint8_t)hw_addr[4],(uint8_t)hw_addr[5]);
    }
    int err = 0;
    g_dnssd = dnssd_init(name, (int)strlen(name), hw_addr, sizeof(hw_addr), &err, 0);
    if (!g_dnssd || err) {
        LOGE("dnssd_init err=%d", err);
        if (g_raop) { raop_destroy(g_raop); g_raop = nullptr; }
        env->ReleaseStringUTFChars(jname, name);
        return -2;
    }

    LOGI("dnssd_init OK, calling raop_set_dnssd");
    raop_set_dnssd(g_raop, g_dnssd);

    LOGI("calling raop_init2");
    int r2 = raop_init2(g_raop, /*nohold=*/1, g_mac_str, g_keyfile);
    LOGI("raop_init2 rc=%d", r2);

    /* Enable HLS so iPhone's GET / probe is accepted. Also set video lang so
       airplay_video_init can register the HLS player. */
    raop_set_plist(g_raop, "hls", 1);
    raop_set_lang(g_raop, "en");
    LOGI("HLS enabled");

    unsigned short port = (unsigned short)airplayPort;
    LOGI("calling raop_start_httpd port=%d", port);
    int rc = raop_start_httpd(g_raop, &port);
    LOGI("raop_start_httpd rc=%d port=%d", rc, port);

    env->ReleaseStringUTFChars(jname, name);
    return rc;
}

JNIEXPORT void JNICALL
Java_org_fireplay_MainActivity_nativeStop(JNIEnv *, jclass) {
    LOGI("nativeStop");
    if (g_raop) { raop_stop_httpd(g_raop); raop_destroy(g_raop); g_raop = nullptr; }
    if (g_dnssd) { dnssd_destroy(g_dnssd); g_dnssd = nullptr; }
    fireplay_video_shutdown();
    g_logger = nullptr;
}

JNIEXPORT void JNICALL
Java_org_fireplay_MainActivity_nativeSetSurface(JNIEnv *env, jclass, jobject surface) {
    if (surface) {
        ANativeWindow *anw = ANativeWindow_fromSurface(env, surface);
        fireplay_video_set_surface(anw);
        if (anw) ANativeWindow_release(anw);  // renderer keeps its own ref
    } else {
        fireplay_video_set_surface(nullptr);
    }
}

static jobject buildTxtMap(JNIEnv *env,
                           const std::pair<const char*, const char*> *entries,
                           size_t n) {
    jclass mapClass = env->FindClass("java/util/LinkedHashMap");
    jmethodID mapCtor = env->GetMethodID(mapClass, "<init>", "()V");
    jmethodID putMethod = env->GetMethodID(mapClass, "put",
        "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    jobject map = env->NewObject(mapClass, mapCtor);
    for (size_t i = 0; i < n; ++i) {
        jstring k = env->NewStringUTF(entries[i].first);
        jstring v = env->NewStringUTF(entries[i].second);
        env->CallObjectMethod(map, putMethod, k, v);
        env->DeleteLocalRef(k);
        env->DeleteLocalRef(v);
    }
    return map;
}

JNIEXPORT jobject JNICALL
Java_org_fireplay_MainActivity_nativeGetTxtRecordsAirplay(JNIEnv *env, jclass) {
    /* pk must match the Ed25519 key raop_init2 generated — read from dnssd
       which was updated by dnssd_set_pk(). Falls back to static if not set. */
    int pk_len = 0;
    const char *dnssd_pk = g_dnssd ? dnssd_get_pk(g_dnssd, &pk_len) : nullptr;
    static const char *fallback_pk = "000bd18473cc2c0feb76ff3e5dda20184333f9a39cbfec7bcae7eb2d0bf2c0a6";
    const char *pk = (dnssd_pk && pk_len > 0) ? dnssd_pk : fallback_pk;
    const std::pair<const char*, const char*> kRecords[] = {
        {"vv", "2"},
        {"srcvers", "220.68"},
        {"pi", g_pi_str},
        {"pk", pk},
        {"model", "AppleTV3,2"},
        {"flags", "0x4"},
        {"pw", "false"},
        {"features", "0x527FFEE6,0x0"},
        {"deviceid", g_mac_str}
    };
    return buildTxtMap(env, kRecords, sizeof(kRecords)/sizeof(kRecords[0]));
}

JNIEXPORT jobject JNICALL
Java_org_fireplay_MainActivity_nativeGetTxtRecordsRaop(JNIEnv *env, jclass) {
    int rpk_len = 0;
    const char *rpk = g_dnssd ? dnssd_get_pk(g_dnssd, &rpk_len) : nullptr;
    static const char *fpk = "000bd18473cc2c0feb76ff3e5dda20184333f9a39cbfec7bcae7eb2d0bf2c0a6";
    const char *raop_pk = (rpk && rpk_len > 0) ? rpk : fpk;
    const std::pair<const char*, const char*> kRecords[] = {
        {"vs", "220.68"}, {"vn", "65537"}, {"tp", "UDP"}, {"sf", "0x4"},
        {"pk", raop_pk},
        {"md", "0,1,2"}, {"et", "0,3,5"}, {"da", "true"},
        {"cn", "0,1,2,3"}, {"ch", "2"}, {"ss", "16"}, {"sr", "44100"},
        {"txtvers", "1"}, {"ft", "0x527FFEE6,0x0"}, {"am", "AppleTV3,2"}
    };
    return buildTxtMap(env, kRecords, sizeof(kRecords)/sizeof(kRecords[0]));
}

} // extern "C"
