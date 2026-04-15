# FirePlay milestones

## M0 — Reference validation (TODAY)
Prove UxPlay (Linux) still pairs with iOS 17+ and receives iPhone
Photos/Video in 2026. If this fails, the whole project is dead before we
start porting.
- Build UxPlay in WSL Ubuntu.
- Run on LAN, have iPhone AirPlay to it.
- Confirm: mDNS discovery, pairing, photo receive, video receive, mirror.

## M1 — Protocol library on Android
Cross-compile UxPlay `lib/*.c` for Android armeabi-v7a API 28 (Fire OS 7).
- OpenSSL for Android.
- libplist for Android (or inline minimal parser).
- dns_sd: switch to Android NSD via JNI.
- Produce `libairplay.a`.

## M2 — Skeleton APK
Minimal Android Activity that loads `libairplay.so`, advertises AirPlay 2
over NSD, and prints pairing events to logcat. No video/audio yet.
- iPhone sees FirePlay in Photos AirPlay menu.
- Tapping it completes pairing and shows a log event.

## M3 — Video rendering
Replace UxPlay's GStreamer video pipeline with Android `MediaCodec` + a
full-screen `SurfaceView`. H.264 only initially, JPEG photos later.

## M4 — Audio rendering
Replace GStreamer audio with `AAudio` (Android 8+) for AirPlay audio.

## M5 — Mirror support
Full iPhone screen mirroring via Control Center.

## M6 — Packaging
F-Droid metadata, reproducible Gradle build, GitHub releases, signed APK.
