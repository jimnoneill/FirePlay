// FirePlay JNI bridge. Currently a skeleton — returns placeholder values so
// the Kotlin MainActivity can wire up NSD and we can see "FirePlay" appear
// on the iPhone's AirPlay picker. Protocol code wires in next milestone.

#include <jni.h>
#include <string>
#include <android/log.h>

#define TAG "FirePlay-jni"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

extern "C" {

JNIEXPORT jint JNICALL
Java_org_fireplay_MainActivity_nativeStart(JNIEnv *env, jclass, jstring jname, jint airplayPort, jint raopPort) {
    const char *name = env->GetStringUTFChars(jname, nullptr);
    LOGI("nativeStart name=%s airplay=%d raop=%d", name, airplayPort, raopPort);
    env->ReleaseStringUTFChars(jname, name);
    // TODO: bind TCP sockets for AirPlay HTTP + RAOP, start UxPlay protocol engine.
    return 0;
}

JNIEXPORT void JNICALL
Java_org_fireplay_MainActivity_nativeStop(JNIEnv *, jclass) {
    LOGI("nativeStop");
    // TODO: teardown.
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
    // Values cribbed from UxPlay's _airplay._tcp TXT on Linux. pk will become
    // per-device Ed25519 once the real pairing code is wired.
    static const std::pair<const char*, const char*> kRecords[] = {
        {"vv", "2"},
        {"srcvers", "220.68"},
        {"pi", "2e388006-13ba-4041-9a67-25dd4a43d536"},
        {"pk", "000bd18473cc2c0feb76ff3e5dda20184333f9a39cbfec7bcae7eb2d0bf2c0a6"},
        {"model", "AppleTV3,2"},
        {"flags", "0x4"},
        {"pw", "false"},
        {"features", "0x527FFEE6,0x0"},
        {"deviceid", "00:15:5d:62:9a:dd"}
    };
    return buildTxtMap(env, kRecords, sizeof(kRecords)/sizeof(kRecords[0]));
}

JNIEXPORT jobject JNICALL
Java_org_fireplay_MainActivity_nativeGetTxtRecordsRaop(JNIEnv *env, jclass) {
    static const std::pair<const char*, const char*> kRecords[] = {
        {"vs", "220.68"},
        {"vn", "65537"},
        {"tp", "UDP"},
        {"sf", "0x4"},
        {"pk", "000bd18473cc2c0feb76ff3e5dda20184333f9a39cbfec7bcae7eb2d0bf2c0a6"},
        {"md", "0,1,2"},
        {"et", "0,3,5"},
        {"da", "true"},
        {"cn", "0,1,2,3"},
        {"ch", "2"},
        {"ss", "16"},
        {"sr", "44100"},
        {"txtvers", "1"},
        {"ft", "0x527FFEE6,0x0"},
        {"am", "AppleTV3,2"}
    };
    return buildTxtMap(env, kRecords, sizeof(kRecords)/sizeof(kRecords[0]));
}

} // extern "C"
