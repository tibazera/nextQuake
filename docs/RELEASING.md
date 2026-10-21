# Releasing nextQuake

Only Android (and later iOS) are release targets. The release version is
`MAJOR.MINOR.PATCH` in the `VERSION` file at the repo root, which is the single
source for:

- the APK `versionName` and `versionCode` (`app/build.gradle.kts`,
  `versionCode = MAJOR*10000 + MINOR*100 + PATCH`, so 0.1.0 -> 100);
- `NEXTQUAKE_VERSION` in the native code (`src/version.h`), shown by `f_version`,
  the version banner and the update check;
- `scripts/release.ps1`, `CHANGELOG.md` and the git tag `v<version>`.

## What `f_version` shows

`nextQuake 0.1.0 (ezQuake 3.7.0-dev) Android 14:Vulkan`

- `nextQuake` (orange) and the release version (green) come from `VERSION`.
- The ezQuake base version stays in the text on purpose: servers and mods detect
  the client through it (`VERSION_NUMBER` in `src/version.h` is unchanged).
- The OS comes from `QW_PlatformDetail()` (`src/version.c`): the Android release
  version is read at run time from `ro.build.version.release`. iOS is stubbed to
  `iOS` until the iOS port exists. `QW_PLATFORM` is `Android` / `iOS` (it used to
  be `Linux32` on Android because `__linux__` is also defined there).
- The renderer is `Vulkan` on mobile.

## Steps for a release

1. Decide the version and add a `## <version>` section to `CHANGELOG.md`.
2. Commit everything (the script refuses a dirty tree unless `-AllowDirty`,
   which is for test builds only).
3. `.\scripts\release.ps1 -Version <version>` (or edit `VERSION` yourself).
   It builds `:app:assembleRelease`, verifies the signature and that the APK
   `versionName` matches, then writes to `release/` (git-ignored):
   `nextQuake-<version>-android-arm64.apk`, its `.sha256` and
   `RELEASE_NOTES-<version>.md` (the CHANGELOG section).
4. Install the APK on a device and run the checklist below.
5. Run the commands the script prints: commit `VERSION`/`CHANGELOG.md`, create the
   annotated tag `v<version>`, push the branch and tag, then `gh release create`.
   The script never commits, tags or pushes by itself.

## Signing

Public releases require these Gradle properties, normally stored in the user's
`~/.gradle/gradle.properties` file so neither the key nor its passwords enter Git:

- `NEXTQUAKE_STORE_FILE`
- `NEXTQUAKE_STORE_PASSWORD`
- `NEXTQUAKE_KEY_ALIAS`
- `NEXTQUAKE_KEY_PASSWORD`

Keep a secure backup of both the keystore and its passwords. Changing or losing
the key prevents in-place updates for installed users. Without these properties,
Gradle produces an unsigned Release APK and `scripts/release.ps1` rejects it.

## Pre-release checklist (on a device)

- App opens, game data present, console/KBD, touch controls, menu.
- `f_version` in a server (or `say` the reply) shows nextQuake, the green version,
  the real Android version and `Vulkan`.
- `gl_outline 3` works and `vk_outline_scale` behaves; `vid_restart` is stable.
- Fresh install and upgrade over the previous version both start.

## Update check

`sys_update_check` queries
`https://api.github.com/repos/tibazera/nextQuake/releases/latest` and compares its
tag (`v0.1.0` -> `0.1.0`) with `NEXTQUAKE_VERSION`. While the repository is private
the request is anonymous and fails silently, so no "outdated" banner is shown.
