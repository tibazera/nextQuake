# nextQuake

A QuakeWorld client focused exclusively on **Android and iOS**, based on [QW-Group's ezQuake](https://github.com/QW-Group/ezquake-source). nextQuake is an independent project, not an official QW-Group release.

## Current status

- Android arm64: SDL3, Vulkan, Oboe audio, touch controls, console, and Android keyboard integration.
- Android Debug and Release builds were validated in the imported base at commit `52fab4df`. This checkout has not received a new build yet.
- iOS: the port has not been implemented yet. The planned approach uses SDL3, Vulkan through MoltenVK/Metal, SDL audio, and iOS storage and keyboard integration.
- Inherited desktop code remains as infrastructure; desktop is not a target platform for this project.

## Development

Read `AGENTS.md`, `continue.md`, and `docs/android-pocket-vulkan-handoff.md` before changing the controls or renderer. The original history has been preserved to track authorship and technical decisions.

To build for Android, configure JDK 17, the Android SDK, and the NDK as described in the handoff, then run `gradlew.bat :app:assembleDebug` or `gradlew.bat :app:assembleRelease`.

Building for iOS requires macOS and Xcode, locally or remotely. No IPA is available, and the client has not been validated on an iPhone.

## License and game data

Credit goes to QW-Group, the ezQuake authors, and the contributors to the original code. The GNU GPL v2 license inherited from ezQuake is preserved in [LICENSE](LICENSE). Keep the original copyright notices and author attribution. Proprietary Quake game data (PAK/PK3 files and maps) is not included in this repository.

The application's display name and Android package identifier are still inherited from ezQuake. Updating the application branding will be a separate task.
