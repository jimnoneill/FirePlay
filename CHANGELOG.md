# Changelog

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.0] 2026-04-21

First tagged release. Matches F-Droid metadata at `metadata/org.fireplay.yml`.

What works on Fire OS 7.7.1 (AFTGAZL) paired with iOS 18:

* Apple Music playback (ALAC, AAC-ELD)
* Photos (stills plus H.264 clip previews)
* AirPlay video with sound (H.264 + AAC-ELD)
* Auto-launch when the iPhone connects, including after a cold boot
* SurfaceView video path using AMediaCodec with zero CPU copy
* SRP-6a pairing and FairPlay emulation from vendored UxPlay
* JmDNS-based Bonjour advertisement (NsdManager is too flaky for Apple clients)

Known gaps: no iPhone screen mirroring from Control Center, no DRM-protected
casts (Netflix, Disney+, etc.) since those require a FairPlay license Apple
doesn't grant to non-certified receivers.

[Unreleased]: https://github.com/jimnoneill/FirePlay/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/jimnoneill/FirePlay/releases/tag/v0.1.0
