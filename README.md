# FirePlay

Open-source AirPlay 2 receiver for Fire TV / Android (armeabi-v7a / arm64).

Fills the gap left by closed-source commercial apps (AirReceiver, AirScreen,
AirPin) that currently own the "AirPlay 2 on Android" market. Native APK,
no Termux, no X server.

## Status

Phase 0: Scaffolding. Nothing works yet.

## Architecture

- `lib-uxplay/` — AirPlay 2 protocol (SRP pairing, RAOP, mirror buffer,
  FairPlay handshake emulation) — forked from
  [FDH2/UxPlay](https://github.com/FDH2/UxPlay), GPL-3.0.
- `renderers-android/` — Android-native video (MediaCodec + SurfaceView)
  and audio (AAudio) sinks, replacing UxPlay's GStreamer renderers.
- `jni/` — C/C++ bridge between `lib-uxplay` and the Android Activity.
- `app/` — Android Studio / Gradle module. Entry Activity, launcher intent,
  Bonjour/NSD wiring.

## Scope

Target protocol features, in order:
1. mDNS/Bonjour advertisement with modern AirPlay 2 TXT records
2. SRP-6a pairing + Ed25519 verify (iOS 17+ handshake)
3. Photo receive (iPhone Photos.app → Fire TV)
4. Video receive (YouTube/VLC/Infuse AirPlay → Fire TV)
5. Audio receive (AirPlay audio / AirTunes)
6. Screen mirror (iPhone Control Center → Mirror to Fire TV)

Out of scope (FairPlay DRM): Netflix, Disney+, HBO streaming from iPhone.

## Build

TODO once Phase 0 compiles.

## License

GPL-3.0 (inherited from UxPlay).
