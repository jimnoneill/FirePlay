<p align="center">
  <img src="branding/icon-1024.png" alt="FirePlay" width="160">
</p>

<h1 align="center">FirePlay</h1>

<p align="center">
  <strong>Open-source AirPlay 2 receiver for Fire TV / Android</strong>
</p>

<p align="center">
  <a href="#what-it-does">What It Does</a> •
  <a href="#install">Install</a> •
  <a href="#build-from-source">Build</a> •
  <a href="#status">Status</a> •
  <a href="#how-it-works">How It Works</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Fire%20OS-7%20%28Android%209%29-orange?style=flat-square" alt="Fire OS 7">
  <img src="https://img.shields.io/badge/iOS-17%20%2F%2018-blue?style=flat-square&logo=apple" alt="iOS 17/18">
  <img src="https://img.shields.io/badge/AirPlay-2-lightgrey?style=flat-square" alt="AirPlay 2">
  <img src="https://img.shields.io/badge/License-GPL--3.0-blue?style=flat-square" alt="License GPL-3.0">
  <a href="https://paypal.me/jimnoneill"><img src="https://img.shields.io/badge/Donate-PayPal-00457C?style=flat-square&logo=paypal" alt="Donate via PayPal"></a>
</p>

---

## What It Does

FirePlay turns your **Amazon Fire TV** (or any Android device) into a working **AirPlay 2** receiver — without paying for AirReceiver, AirScreen, or AirPin.

| iPhone source | What you get on the TV |
|---|---|
| Music app | 🔊 Lossless ALAC audio |
| Photos (still) | 🖼️ Full-res photo on screen |
| Photos (video) | 🎬 H.264 video + AAC ELD audio |
| YouTube etc. (legacy AirPlay) | 🎬 HLS video |

This is the first **open-source** Android AirPlay 2 receiver that actually pairs with iOS 17+ and streams Music, Photos, and Video end-to-end. Every prior project was either dead, audio-only, or required a Linux/Raspberry-Pi host.

What FirePlay does **not** do (yet):
- iPhone screen mirroring (Control Center → Mirror).  Comes for free once we hook the AirPlay 2 mirror RTSP variant; same code path as Photos video.
- Netflix / Disney+ / HBO casting — those are blocked by Apple's FairPlay DRM, which no open-source project has reimplemented.

## Install

### 🟢 Easiest — install on your Fire TV remote, no computer needed

1. On your Fire TV, install the free **Downloader** app from the Amazon Appstore (search "Downloader" by AFTVnews).
2. On the Fire TV, go to **Settings → My Fire TV → Developer Options** and turn on **Install unknown apps** for the Downloader app. (If you don't see Developer Options: Settings → My Fire TV → About → click "Fire TV" 7 times, then go back.)
3. Open Downloader, type this URL into the address bar:
   ```
   is.gd/fireplay_tv
   ```
4. Press **Go**. Downloader will fetch the latest FirePlay APK.
5. Press **Install** when the APK opens. Then **Open** to launch FirePlay (you'll see a black fullscreen — that's correct, it's now listening).
6. On your iPhone: Music or Photos → AirPlay icon → pick **FirePlay**.

### 🔵 Power users — sideload via ADB

```bash
adb connect <fire-tv-ip>:5555
adb install fireplay-v0.1.0-debug.apk
```

APK attached to every [GitHub release](https://github.com/jimnoneill/FirePlay/releases).

### 🟣 F-Droid (coming soon)

F-Droid metadata is included under `fastlane/metadata/` and `metadata/`. Once
the app stabilizes past v0.2, we'll submit a merge request to the
[fdroiddata](https://gitlab.com/fdroid/fdroiddata) repo so installs and
updates flow through the F-Droid client. (Amazon Appstore is intentionally
**not** a target — Apple has a history of DMCA-noticing AirPlay
reimplementations, and Amazon doesn't want that risk on their store.)

### Notes for Fire TV Cube 3rd Gen (AFTGAZL)

FirePlay was developed and tested against this device (Fire OS 7.7.1, armeabi-v7a). The APK ships both `armeabi-v7a` and `arm64-v8a` slices, so any Fire TV running Fire OS 7 / Android 9+ should work.

## Build from Source

You'll need:
- Android NDK r27+ (Windows on ARM users: the x86_64 NDK runs fine under Prism)
- Android SDK with `platforms;android-28`, `build-tools;34.0.0`, `cmake;3.22.1`
- JDK 17

```bash
git clone https://github.com/jimnoneill/FirePlay.git
cd FirePlay
git submodule update --init --recursive   # third_party/alac, android_openssl
gradle assembleDebug
adb install app/build/outputs/apk/debug/app-debug.apk
```

The Windows build script `build.ps1` sets up the right env vars if you're on Windows.

## Status

| Milestone | Status |
|---|---|
| Cross-compile UxPlay lib/ for Android | ✅ |
| Foreground service + JmDNS advertisement | ✅ |
| iOS 17/18 pairing (FairPlay 2-round) | ✅ |
| Music — ALAC decode → AAudio | ✅ |
| Photos still — H.264 → MediaCodec → SurfaceView | ✅ |
| Photos video sound — AAC-ELD → MediaCodec → AAudio | ✅ |
| iPhone screen mirroring | 🚧 wiring AirPlay 2 mirror RTSP next |
| F-Droid release | 🚧 |

## How It Works

```
┌────────────┐  mDNS+TCP   ┌─────────────────────────────────────────┐
│   iPhone   │◄───────────►│             FirePlay (Android)          │
│ AirPlay 2  │   RTSP +    │                                         │
│   client   │   RAOP      │   FirePlayService (foreground)          │
└────────────┘             │     ├─ JmDNS advertise _airplay/_raop   │
                           │     └─ libairplay.so (UxPlay protocol)  │
                           │            ├─ FairPlay handshake        │
                           │            ├─ ALAC pkts → ALACDecoder   │
                           │            ├─ AAC-ELD pkts → MediaCodec │
                           │            └─ H.264 NAL → MediaCodec    │
                           │                       │      │      │   │
                           │                       ▼      ▼      ▼   │
                           │                AAudio out   SurfaceView │
                           └─────────────────────────────────────────┘
```

**Protocol layer** (`lib-uxplay/`) — vendored from
[FDH2/UxPlay](https://github.com/FDH2/UxPlay), GPL-3. Handles SRP-6a pairing,
FairPlay emulation, RAOP RTP demux, NTP sync.

**Renderers** (`app/src/main/cpp/`):
- `audio_renderer_aaudio.cpp` — ALAC via Apple's open-source decoder, AAC ELD
  via Android `AMediaCodec`. Both write PCM into the same `AAudioStream`.
- `video_renderer_mediacodec.cpp` — H.264 NAL units fed to `AMediaCodec`
  output-bound to an `ANativeWindow` from the Activity's `SurfaceView`.

**Discovery** — Android's built-in `NsdManager` is unreliable advertising to
Apple clients. We use [JmDNS](https://github.com/jmdns/jmdns) instead with
hand-built Bonjour TXT records.

## Why this exists

AirReceiver / AirScreen / AirPin are all closed-source paid apps. Every prior
"open-source AirPlay for Android" project on GitHub was either:
- Audio-only (shairport-sync forks)
- Linux/Raspberry-Pi only (UxPlay, RPiPlay)
- Dead since 2017–2024 with no iOS-17 fix
- Built on a closed-source FairPlay submodule sold separately

FirePlay glues UxPlay's tested protocol code (which already handles iOS 17+ on
Linux) into a real Android APK with native renderers.

## Credits

- [FDH2/UxPlay](https://github.com/FDH2/UxPlay) — the protocol stack we
  cross-compiled. GPL-3.
- [macosforge/alac](https://github.com/macosforge/alac) — Apple's official
  ALAC decoder. APSL.
- [KDAB/android_openssl](https://github.com/KDAB/android_openssl) — prebuilt
  OpenSSL for Android.
- [libimobiledevice/libplist](https://github.com/libimobiledevice/libplist) —
  Apple property list parser. LGPL-2.1.
- [jmdns/jmdns](https://github.com/jmdns/jmdns) — Apple-friendly mDNS in Java.

## Support

If FirePlay saves you the AirReceiver fee or you want to help me keep
projects like this open, **[tip via PayPal](https://paypal.me/jimnoneill)**.

## License

GPL-3.0 (inherited from UxPlay). See [LICENSE](LICENSE).
