# Changelog

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.3] 2026-04-21

First automated release built by CI. No runtime changes from 0.1.2.

* Added `.github/workflows/release.yml`. Tag pushes now produce a
  signed-on-the-fly APK plus its SHA256 and attach them to a GitHub
  Release.
* `CHANGELOG.md` and `docs/releasing.md` spell out the SemVer release
  process going forward.
* README rewritten in a plain human voice.
* F-Droid metadata updated to track `v0.1.3`.

## [0.1.2] 2026-04-16

* Splash screen survives a Fire TV reboot and continues to draw
  correctly on cold start.

## [0.1.1] 2026-04-16

* Buffered SPS and PPS and configured CSD-0 on Surface arrival so
  video survives a reboot mid-stream.
* Mapped UxPlay DEBUG logs into Android INFO so they show up in
  `logcat` without needing verbose filters.

## [0.1.0] 2026-04-16

First public tagged build. What works on Fire OS 7.7.1 (AFTGAZL) paired
with iOS 18:

* Apple Music playback (ALAC, AAC-ELD).
* Photos, both stills and the short H.264 clips the iPhone generates.
* AirPlay video casts with sound (H.264 + AAC-ELD).
* Auto-launch when the iPhone connects, including after a cold boot.
* SurfaceView video path using AMediaCodec with zero CPU copy.
* SRP-6a pairing and FairPlay emulation from vendored UxPlay.
* JmDNS-based Bonjour advertisement, since NsdManager was too flaky
  for Apple clients.

Known gaps: no iPhone screen mirroring from Control Center, and no
DRM-protected casts (Netflix, Disney+, etc.) because those require a
FairPlay license Apple doesn't grant to non-certified receivers.

[Unreleased]: https://github.com/jimnoneill/FirePlay/compare/v0.1.3...HEAD
[0.1.3]: https://github.com/jimnoneill/FirePlay/releases/tag/v0.1.3
[0.1.2]: https://github.com/jimnoneill/FirePlay/releases/tag/v0.1.2
[0.1.1]: https://github.com/jimnoneill/FirePlay/releases/tag/v0.1.1
[0.1.0]: https://github.com/jimnoneill/FirePlay/releases/tag/v0.1.0
