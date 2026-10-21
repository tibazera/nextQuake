# Changelog

Release versions follow `MAJOR.MINOR.PATCH` and live in the `VERSION` file.
Only Android and iOS are release targets.

## 0.1.0

First nextQuake release, based on ezQuake 3.7.0-dev (QW-Group, GPL v2).

- Android arm64 client (Android 10+, API 29) with SDL3, Vulkan and Oboe audio.
- Touch controls and on-screen keyboard/console: left side moves, right side aims,
  a tap aims and a double tap fires; FIRE can be held while dragging to aim.
- Vulkan performance work: static world index buffer, bind caching, lighter
  outline pass (`vk_outline_scale`), per-device resolution list in Options.
- nextQuake splash screen and app icon.
- Public Android identity (`nextQuake`, application ID `org.nextquake.android`)
  and a dedicated release-signing key for upgrade-compatible APKs.
- `f_version` now reports `nextQuake <version> (ezQuake <base>) <OS>:<renderer>`,
  e.g. `nextQuake 0.1.0 (ezQuake 3.7.0-dev) Android 14:Vulkan`,
  instead of the inherited Linux platform string. The update check now looks at
  the nextQuake releases.
- A default `config.cfg` is bundled and installed on first run only, when the game folder has none
  (an existing config is never touched). It carries the HUD, movement/interpolation, prediction and
  bind settings; name, team, colours, spectator mode and desktop video settings are left out.
- The launcher detects an existing HD texture pack instead of asking to download it again, and remembers
  the answer next to the game data so it survives a reinstall.
- iOS is not available yet.
