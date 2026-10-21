# Changelog

Release versions follow `MAJOR.MINOR.PATCH` and live in the `VERSION` file.
Only Android and iOS are release targets.

## 0.1.0

First public nextQuake release, based on ezQuake 3.7.0-dev by the QW-Group
and its contributors. The inherited GNU GPL v2 license and original authorship
are preserved.

### Download

- APK: `nextQuake-0.1.0-android-arm64.apk`
- Size: 17,732,274 bytes
- SHA-256: `2358BB15047C2C0C10963B2C11D1B4091384CE50F72DBA2FDC0F8A7BEE681156`
- Android application ID: `org.nextquake.android`
- Signed with the dedicated nextQuake release key for upgrade-compatible future releases.

### Technical baseline

- Android 10 or newer (minimum API 29; target and compile API 36).
- 64-bit ARM only (`arm64-v8a`). There is no 32-bit or x86 APK.
- SDL 3.4.0 for the Android window, lifecycle, keyboard, mouse and controller integration.
- Vulkan 1.0 renderer only. The desktop OpenGL renderers are not included in the APK.
- Oboe audio output using AAudio or OpenSL ES as selected by the Android device.
- Android NDK 28.2.13676358; native Release code is optimized and stripped of debug symbols.
- Android Debug and Release builds, APK signing checks and the touch/keyboard regression test passed.
- The release was installed and launched successfully on a Xiaomi 14T Pro running Android 16.

### What is different from traditional desktop ezQuake

- **Mobile-first target.** This release is an Android application rather than a desktop executable.
  It includes Android lifecycle handling, storage access, a launcher and a mobile rendering surface.
- **Vulkan-only Android renderer.** Rendering uses Vulkan 1.0 and SPIR-V shaders. OpenGL is not
  packaged, which avoids shipping a second renderer and keeps the APK smaller.
- **Touch controls.** The left side moves forward/back and strafes; the right side aims. A single
  touch or drag aims, a nearby double tap fires, and the FIRE button can be held while dragging to aim.
  JUMP, weapon previous/next, scoreboard, chat, console, keyboard and menu controls are available on screen.
- **Android keyboard and console integration.** The KBD button opens the software keyboard. Text input
  works in the startup console, console, chat and Dear ImGui text fields. The console becomes shorter while
  the keyboard is visible.
- **Physical input support.** A real mouse and keyboard can be used through USB or Bluetooth. Touch mode
  follows actual physical/touch input instead of trusting Android's sometimes misleading attached-device flags.
- **Dear ImGui mobile menu.** The Android Vulkan build has a touch-friendly home screen, Quick Play,
  server browser, demo browser, settings, console and quit confirmation, plus a rendered background map.
  Set `menu_classic 1` before restarting to use the inherited classic menus.
- **Android launcher and game-data handling.** The launcher checks for Quake data, can download its
  configured base package, requests the required storage access and detects an existing HD texture pack.
- **Safe default configuration.** A mobile-oriented `config.cfg` is copied only when no configuration
  exists. It never overwrites the user's existing config and excludes identity, passwords, team/colour,
  spectator and desktop-window settings.
- **Mobile resolution control.** Auto mode caps the render surface inside a 1920x1080 envelope while
  preserving aspect ratio and never enlarging a smaller display. Options also offers device-appropriate
  render heights from 360p through the available native/capped resolutions; the compositor upscales them.
- **Vulkan frame-path changes.** The renderer supports two frames in flight, batches dynamic lightmap
  uploads through per-frame staging buffers, builds a static world index buffer, caches repeated bindings
  and retires texture descriptors without a full device wait.
- **Optional world outlines.** Vulkan supports the world-outline path. `vk_outline_scale 0.5` renders
  its normals pass at half width and height; `vk_opt_world_outline 0` disables that pass. Outlines cost
  frame rate and remain off unless enabled by the user's settings/ruleset.
- **Android pre-rotation fixes.** Rendering, mouse/touch coordinates, HUD projection, player-name labels
  and ImGui viewport/scissor transforms account for rotated Android swapchains.
- **Android audio behavior.** Playback uses Oboe. Voice capture is disabled in this Android release.
- **Mobile identity and version reporting.** The app is named nextQuake and `f_version` reports
  `nextQuake 0.1.0 (ezQuake 3.7.0-dev) Android <version>:Vulkan`. Update checks use this repository's releases.
- **Android-specific safety fixes.** SDL3 boolean return handling, Vulkan frame/surface synchronization,
  infostring bounds, larger server-info buffers and NUL termination in `cvaredit` were corrected.
- **No bundled Quake game data.** Proprietary PAK/PK3 files, maps and user configs are not part of the APK.
- **No iOS build yet.** The iOS/MoltenVK port remains future work.

### Installation

1. Download `nextQuake-0.1.0-android-arm64.apk` from the GitHub release.
2. Open the APK on an Android 10+ arm64 device with Vulkan 1.0 support. If Android asks, allow APK
   installation for the browser or file manager used to open it.
3. Install the app. It appears as **nextQuake**. The package is separate from older test builds using
   `org.ezquake.android`, so both applications may appear until the older one is removed manually.
4. Obtain a working Quake/nQuake/ezQuake installation. The APK does not contain the game data.
5. In the phone's internal storage, create this exact directory:

   ```text
   Documents/ezQuake
   ```

   Its full Android path is `/storage/emulated/0/Documents/ezQuake`.
6. Copy the complete `id1`, `qw` and `ezquake` directories into `Documents/ezQuake`, preserving their
   contents and directory structure. The result should resemble:

   ```text
   Documents/ezQuake/
   ├── id1/
   │   ├── pak0.pak
   │   └── pak1.pak
   ├── qw/
   │   └── ... maps, packages, skins, sounds and configs ...
   └── ezquake/
       ├── ezquake.pk3
       └── ... other ezQuake assets ...
   ```

   Do not create an extra folder such as `Documents/ezQuake/nQuake/id1`. Keep `.pak` and `.pk3` files
   intact; do not extract them. Windows executables are not needed on Android.
7. Start nextQuake and grant storage access. On Android 11 or newer, enable **Allow access to manage all
   files** when Android opens that settings page, then return to nextQuake. Android 10 uses the regular
   storage permission.
8. Wait for startup to complete. The first launch can open at the console; use the on-screen **KBD** button
   to enter commands. Touch controls use automatic mode by default (`in_touch 2`).
9. If the launcher offers an automatic data download, keep the app open and connected until it finishes.
   Its base-data check is intentionally simple; if files are still reported missing, copy all three folders
   manually as described above.

Existing game data and configurations are not deleted by installing or upgrading the APK. Back up personal
configs before manually replacing or merging files. See the README and `docs/COMPATIBILITY.md` for monitor,
device and troubleshooting notes.
