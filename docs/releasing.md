# Releasing

FirePlay follows Semantic Versioning. Tag pushes trigger the
`release.yml` workflow, which runs a full Android build and attaches
the APK plus its SHA256 to a GitHub Release.

## Cutting a release

1. Bump both versions in lockstep. The gradle `versionName` and the tag
   must match exactly, otherwise the workflow fails its sanity check.

        # app/build.gradle.kts
        versionCode = 2
        versionName = "0.2.0"

2. Move the unreleased bullets under a new dated heading in `CHANGELOG.md`
   and update the compare link at the bottom of the file.

3. If the `versionCode` changed, add a matching
   `fastlane/metadata/android/en-US/changelogs/<versionCode>.txt` file
   with short F-Droid-friendly notes (one sentence per line).

4. Commit, tag, push:

        git add app/build.gradle.kts CHANGELOG.md fastlane/metadata
        git commit -m "Release 0.2.0"
        git tag -a v0.2.0 -m "Release 0.2.0"
        git push && git push origin v0.2.0

5. Watch the run at `/actions`. Usually 6 to 9 minutes including
   NDK setup. The release appears at `/releases/tag/v0.2.0` when it
   finishes.

## Signing for production

The default workflow produces whatever APK the gradle build picks first,
falling back to debug if release signing is not configured. Debug-signed
APKs sideload fine on Fire TV but cannot be published on Play or F-Droid.

To switch to production signing, add these secrets to the repo
(`Settings -> Secrets and variables -> Actions`):

- `SIGNING_KEYSTORE_BASE64` — base64 of your keystore file
- `SIGNING_KEY_ALIAS` — alias name inside the keystore
- `SIGNING_KEY_PASSWORD` — alias password
- `SIGNING_STORE_PASSWORD` — keystore password

Then extend `app/build.gradle.kts` with a `signingConfigs.release` block
that reads from env vars, and update `release.yml` to set those env vars
before the gradle step. A template for this is left as a TODO comment in
the workflow.

## F-Droid

`metadata/org.fireplay.yml` is the F-Droid recipe. F-Droid's buildserver
scans the repo on every tag push and builds from source in its own
sandbox, so nothing else is required from this side besides keeping
`versionName`, `versionCode`, and the `commit:` field in sync with the
tag. Their inclusion queue is slow. Expect a pull request from
`fdroiddata` within a few weeks of the first tagged release.
