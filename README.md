![nextQuake logo](docs/assets/nextquake-logo.png)

# nextQuake

A QuakeWorld client focused exclusively on **Android and iOS**, based on [QW-Group's ezQuake](https://github.com/QW-Group/ezquake-source). nextQuake is an independent project, not an official QW-Group release.

## Current status

- Android arm64: SDL3, Vulkan, Oboe audio, touch controls, console, and Android keyboard integration.
- Android Debug and Release builds were validated in the imported base at commit `52fab4df`. This checkout has not received a new build yet.
- iOS: the port has not been implemented yet. The planned approach uses SDL3, Vulkan through MoltenVK/Metal, SDL audio, and iOS storage and keyboard integration.
- Inherited desktop code remains as infrastructure; desktop is not a target platform for this project.

## Supported Android versions

Requirements: **Android 10 (API 29) or newer**, an **arm64-v8a** device (any phone or tablet from the last several years), and a GPU with **Vulkan 1.0** support. Devices without Vulkan, 32-bit-only devices and Android 9 or older are not supported.

| Android | API | Status |
|---|---|---|
| 16 | 36 | **Tested** (Xiaomi 14T Pro, Mali-G720) |
| 15 | 35 | Supported by the build, not tested yet |
| 14 | 34 | Supported by the build, not tested yet |
| 13 | 33 | Supported by the build, not tested yet |
| 12 / 12L | 31 / 32 | Supported by the build, not tested yet |
| 11 | 30 | Supported by the build, not tested yet. Needs "Allow access to manage all files" |
| 10 | 29 | Minimum version, not tested yet. Uses the regular storage permission |
| 9 and older | 28 or lower | Not supported |

"Supported by the build" means the app is built for that API level and uses no newer API; it does not mean it has been run there. The list of devices that have actually been tested is in [docs/COMPATIBILITY.md](docs/COMPATIBILITY.md); reports of other devices are welcome.

## Playing on a monitor with keyboard and mouse

nextQuake is meant to run on any Android phone or tablet that meets the requirements above (Android 10+, arm64, Vulkan 1.0). It is not limited to flagships, but it has only been tested on the devices listed in [docs/COMPATIBILITY.md](docs/COMPATIBILITY.md).

The game is fully playable with a mouse and keyboard on a monitor: connect a **powered USB-C hub** to the phone with the mouse, the keyboard, the monitor and a charger. The touch controls hide themselves while a real mouse or keyboard is in use.

**Video output over USB-C.** The phone must be able to send video through its USB-C port. On current phones this is normally **DisplayPort Alt Mode** (the older MHL standard is rarely used on USB-C phones). Many phones have no video output at all; others can only mirror at 60 Hz. A refresh rate above 60 Hz on the monitor needs all of these to support it: the phone's video output, the hub or adapter, the cable and the monitor. Phone makers rarely publish the external-display refresh rate, so check it on your own setup.

The **LD39 USB-C cable** is also a reported option for connecting some mid-range phones to a monitor, but its output is limited to **60 Hz**. Phone compatibility still depends on the device, so verify support before buying the cable.

Phones that are reported to have DisplayPort Alt Mode (video output over USB-C), from third-party lists that are not authoritative:

- Samsung Galaxy S24 Ultra and S25 Ultra (also Samsung DeX, see [known issues](#known-issues)).
- Google Pixel 8 series and newer, including the Pixel 9 Pro and Pixel 10 Pro XL. Android 16 desktop mode for external displays needs a recent Android build.
- Motorola Edge 60 Pro.

This list does **not** say which of them output 90, 120 or 144 Hz to a monitor: that was not verified. Confirm "DisplayPort Alt Mode" (and the maximum external refresh rate) in the phone's official specifications before buying anything for this purpose, and tell us what works so it can be added to [docs/COMPATIBILITY.md](docs/COMPATIBILITY.md). Phones without video output can only use software solutions (for example DisplayLink adapters), which are not tested with nextQuake.

Sources: [List of phones with external monitor support](https://www.faceofit.com/list-of-phones-with-external-monitor/), [List of smartphones with DisplayPort Alt Mode](https://uperfect.com/blogs/wikimonitor/list-of-smartphones-with-displayport-alt-mode), [Plugable: Android 16 desktop mode with a Pixel](https://kb.plugable.com/how-to-use-android-16-desktop-mode-with-a-pixel-phone-and-usb-c-display-or-hub).

## Known issues

- **Game data is not included.** The APK alone does not start a match: the Quake data must be in `Documents/ezQuake`. The launcher's check for missing data is incomplete, so a partial copy may not be detected.
- **One older tablet closed the app right after downloading the game data.** Seen on a single tablet; the cause was not found.
- **The world outline costs frame rate.** `gl_outline 2` or `3` renders the world a second time. Lower `vk_outline_scale` (default 0.5, applied after `vid_restart`) or leave the outline off on slower phones.
- **Heavy `BLASTBufferQueue ... NO_BUFFER_AVAILABLE` logging** appears in `adb logcat` while the frame rate is uncapped (Vulkan MAILBOX/IMMEDIATE presentation). Enabling vertical sync or setting `cl_maxfps` avoids it.
- **Samsung DeX: the frame rate is capped in fullscreen.** Reported on a Samsung device in DeX mode: the game reaches a high frame rate when it runs in a window, but is limited when it is made fullscreen. The cause is not confirmed; it looks like the frame-rate management of DeX or Samsung's game mode, not the renderer. Until it is understood, keep the game in a window under DeX.
- **Some Android 10 to 15 devices, Adreno GPUs and tablets have not been tested at all.** See [docs/COMPATIBILITY.md](docs/COMPATIBILITY.md).
- **iOS is not available.**

## Android installation

**Installing the APK alone is not enough: the client also needs Quake/nQuake/ezQuake game data in the correct folder.** The current Android build requires Android 10 or newer, an arm64 device, and Vulkan support.

1. Install the provided release APK. If Android asks, allow APK installation for the browser or file manager you are using. The application appears as **nextQuake**.
2. Obtain a working nQuake/ezQuake installation on your computer, including its game data. Keep a backup of your existing configuration before copying or merging files.
3. Connect the phone to your computer using USB file transfer, or use a file manager on the phone. In **internal storage**, create this exact folder:

   ```text
   Documents/ezQuake
   ```

   Its full Android path is:

   ```text
   /storage/emulated/0/Documents/ezQuake
   ```

   Keep the spelling and capitalization shown above. The folder is still named `ezQuake`, even though this project is named nextQuake.

4. Copy the `id1`, `qw`, and `ezquake` folders from your nQuake/ezQuake installation directly into that folder, preserving their contents and directory structure. The result should look like this:

   ```text
   Documents/
   └── ezQuake/
       ├── id1/
       │   ├── pak0.pak
       │   └── pak1.pak          (if included in your registered Quake installation)
       ├── qw/
       │   └── ...              (maps, configs, skins, sounds, and packages)
       └── ezquake/
           ├── ezquake.pk3
           └── ...              (other ezQuake assets)
   ```

   Copy the folders' full contents, not just the example files. Do not create an extra nested folder such as `Documents/ezQuake/nQuake/id1` or `Documents/ezQuake/ezQuake/id1`. Keep `.pak` and `.pk3` files intact; do not extract them. Windows executables are not needed on Android.

5. Open the app and grant the requested storage access. On Android 11 or newer, enable **Allow access to manage all files** for the app when prompted, then return to it. On Android 10, allow the requested storage permission.
6. Wait for startup to finish. The game may initially open at the console; use the on-screen **KBD** button to enter commands. Touch controls are enabled automatically by default (`in_touch 2`).

### Automatic download and missing data

The current launcher attempts to download its configured game data when it cannot find base data. If you use this option, allow the download to finish and keep an internet connection available. Manual copying is useful when downloads fail or when you want to use your existing installation.

The launcher's base-data check is not a complete installation check: finding `id1/pak0.pak` or another recognized base file can skip the download even if other assets are missing. If the app reports missing files, verify the folder structure and copy the complete `id1`, `qw`, and `ezquake` directories rather than only `pak0.pak`.

If some maps display unexpected simple textures, check your data packages: an inherited `id1/gpl_maps.pk3` can contain replacement maps that override the originals. Preserve a backup before changing any game data.

## iOS installation

An iOS build is not available yet. The Android APK and Android storage instructions do not apply to iPhone.

## Development

Read `AGENTS.md`, `continue.md`, and `docs/android-pocket-vulkan-handoff.md` before changing the controls or renderer. The original history has been preserved to track authorship and technical decisions.

To build for Android, configure JDK 17, the Android SDK, and the NDK as described in the handoff, then run `gradlew.bat :app:assembleDebug` or `gradlew.bat :app:assembleRelease`.

Building for iOS requires macOS and Xcode, locally or remotely. No IPA is available, and the client has not been validated on an iPhone.

## License and game data

Credit goes to QW-Group, the ezQuake authors, and the contributors to the original code. The GNU GPL v2 license inherited from ezQuake is preserved in [LICENSE](LICENSE). Keep the original copyright notices and author attribution. Proprietary Quake game data (PAK/PK3 files and maps) is not included in this repository.

Third-party assets: the on-screen control bases are Kenney "Mobile Controls" (CC0), the touch icons are from [game-icons.net](https://game-icons.net) by Lorc and Delapouite (CC BY 3.0), and the menu font is Oxanium by Severin Meyer (SIL OFL 1.1). See `app/src/main/assets/touch/` and `src/fonts/` for the license texts.

The Android application ID is `org.nextquake.android`. The Java namespace and the `Documents/ezQuake` data folder remain compatible with the ezQuake base.
