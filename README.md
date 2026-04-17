<p align="center">
  <img src="branding/icon-1024.png" alt="FirePlay" width="160">
</p>

<h1 align="center">FirePlay</h1>

<p align="center">
  <strong>Open-source AirPlay 2 Apple Device receiver for Fire TV &amp; Android</strong>
</p>

<p align="center">
  <a href="#install">Install</a> •
  <a href="#what-it-does">What It Does</a> •
  <a href="#how-it-works">How It Works</a> •
  <a href="#build-from-source">Build</a> •
  <a href="#credits">Credits</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Fire%20OS-7%20%28Android%209%29-FF9900?style=flat-square&logo=amazon" alt="Fire OS 7">
  <img src="https://img.shields.io/badge/iOS-17%20%2F%2018-000000?style=flat-square&logo=apple&logoColor=white" alt="iOS 17/18">
  <img src="https://img.shields.io/badge/AirPlay-2-lightgrey?style=flat-square" alt="AirPlay 2">
  <img src="https://img.shields.io/badge/License-GPL--3.0-blue?style=flat-square" alt="License GPL-3.0">
</p>

---

## Why?

AirReceiver, AirScreen, and AirPin all charge you to do AirPlay on Fire TV. Every "open-source AirPlay for Android" project on GitHub was either dead, audio-only, or required a Raspberry Pi. This project fixes that.

```
┌─────────────┐    mDNS + RTSP    ┌──────────────────────────────────┐
│   iPhone    │◄─────────────────►│          FirePlay (APK)          │
│             │                   │                                  │
│  Music      │    ALAC audio     │   ALACDecoder ──► AAudio out     │
│  Photos     │    H.264 video    │   MediaCodec  ──► SurfaceView   │
│  Video      │    AAC-ELD audio  │   MediaCodec  ──► AAudio out     │
│             │                   │                                  │
│  AirPlay 2  │    FairPlay pair  │   UxPlay protocol lib (GPL-3)    │
└─────────────┘                   └──────────────────────────────────┘
```

---

## Install

### Option 1 — Downloader app (no computer needed)

1. On your Fire TV, install the free **Downloader** app from the Amazon Appstore.
2. Go to **Settings → My Fire TV → Developer Options** → turn on **Install unknown apps** for Downloader.
   <br>*(No Developer Options? Settings → My Fire TV → About → click "Fire TV" 7 times.)*
3. Open Downloader and type:
   ```
   is.gd/fireplay_tv
   ```
4. Press **Go → Install → Open**. You'll see the FirePlay splash screen — it's now listening.
5. On your iPhone: **Music** or **Photos** → tap the **AirPlay** icon → pick **FirePlay**.

### Option 2 — ADB sideload

```bash
adb connect <fire-tv-ip>:5555
adb install fireplay-v0.1.0-debug.apk
```

APKs attached to every [GitHub release](https://github.com/jimnoneill/FirePlay/releases).

### Option 3 — F-Droid (coming soon)

F-Droid metadata is ready under `fastlane/metadata/`. Submission planned at v0.2.
Amazon Appstore is intentionally not a target — Apple has a history of DMCA-noticing AirPlay reimplementations.

---

## What It Does

| iPhone source | What you get on the TV |
|---|---|
| **Music** app | Lossless ALAC audio through TV speakers |
| **Photos** (still) | Full-resolution photo displayed |
| **Photos** (video) | H.264 video + AAC-ELD sound |
| YouTube etc. (legacy AirPlay) | HLS video |

### What it does NOT do (yet)

| Feature | Status |
|---|---|
| iPhone screen mirroring (Control Center) | Planned for v0.2 |
| Netflix / Disney+ / HBO casting | Blocked by Apple FairPlay DRM — no open-source project can do this |

---

## Status

| Milestone | Status |
|---|---|
| mDNS advertisement (JmDNS) | ✅ |
| iOS 17/18 pairing (FairPlay 2-round) | ✅ |
| Music — ALAC → AAudio | ✅ |
| Photos/Video — H.264 → MediaCodec → Surface | ✅ |
| Video sound — AAC-ELD → MediaCodec → AAudio | ✅ |
| Per-device identity (no collisions on multi-receiver LANs) | ✅ |
| Idle splash screen with branding | ✅ |
| iPhone screen mirroring | 🚧 |
| F-Droid release | 🚧 |

---

## How It Works

**Protocol layer** (`lib-uxplay/`) — vendored from [FDH2/UxPlay](https://github.com/FDH2/UxPlay), GPL-3.
Handles SRP-6a pairing, FairPlay emulation, RAOP RTP demux, NTP sync.

**Audio** (`audio_renderer_aaudio.cpp`):
- Music: ALAC packets → Apple's open-source [ALACDecoder](https://github.com/macosforge/alac) → PCM → AAudio output
- Video sound: AAC-ELD packets → Android `AMediaCodec` → PCM → same AAudio stream

**Video** (`video_renderer_mediacodec.cpp`):
- H.264 NAL units from iPhone → `AMediaCodec` hardware decoder → `ANativeWindow` (SurfaceView)
- Zero CPU-copy rendering path

**Discovery**:
- Android's built-in NsdManager is unreliable advertising to Apple clients
- We use [JmDNS](https://github.com/jmdns/jmdns) with hand-built Bonjour TXT records instead

---

## Build from Source

**Requirements**: Android NDK r27+, SDK with `platforms;android-28`, JDK 17.

```bash
git clone https://github.com/jimnoneill/FirePlay.git
cd FirePlay
# Windows: edit local.properties with your SDK/NDK paths, then:
gradle assembleDebug
adb install app/build/outputs/apk/debug/app-debug.apk
```

The build script `build.ps1` sets up env vars for Windows ARM64 machines.

---

## Tested on

| Device | OS | Result |
|---|---|---|
| Fire TV Cube 3rd Gen (AFTGAZL) | Fire OS 7.7.1 | ✅ Music + Photos + Video |
| iPhone 13 | iOS 18 (AirPlay/870.14.1) | ✅ Full pairing + streaming |

The APK ships both `armeabi-v7a` and `arm64-v8a` — any Fire TV / Android TV running Android 9+ should work.

---

## Credits

- [FDH2/UxPlay](https://github.com/FDH2/UxPlay) — AirPlay 2 protocol stack (GPL-3)
- [macosforge/alac](https://github.com/macosforge/alac) — Apple ALAC decoder (APSL)
- [KDAB/android_openssl](https://github.com/KDAB/android_openssl) — prebuilt OpenSSL for Android
- [libimobiledevice/libplist](https://github.com/libimobiledevice/libplist) — Apple plist parser (LGPL-2.1)
- [jmdns/jmdns](https://github.com/jmdns/jmdns) — Apple-compatible mDNS in Java

---

## Support

If FirePlay saved you the $5 AirReceiver fee — or if you just think open-source AirPlay on Android should exist — you can help keep it going.

<p align="center">
  <a href="https://paypal.me/jimnoneill"><img src="https://img.shields.io/badge/Donate-PayPal-00457C?style=for-the-badge&logo=paypal" alt="Donate via PayPal"></a>
</p>

---

## License

GPL-3.0 (inherited from UxPlay). See [LICENSE](LICENSE).

---

<p align="center">
  <sub>Built with relentless determination and Claude Code.</sub>
</p>
