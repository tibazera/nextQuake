# Device compatibility

Record of devices nextQuake was actually run on. Add a row after every real
test; do not list a device or Android version here unless it was tested.

| Device | Android | GPU | nextQuake | Result | Notes |
|---|---|---|---|---|---|
| Xiaomi 14T Pro (2407FPN8EG, "rothko") | 16 (API 36) | Mali-G720 Immortalis MC12 | pre-0.1 dev builds | Runs: Vulkan renderer, touch controls, console/keyboard | Main test device; MIUI blocks `adb input`, use scrcpy UHID |

Not yet tested: Android 10 to 15 devices, Adreno GPUs, tablets, gamepads over
Bluetooth, other Mali generations.

Known reports, not diagnosed:

- Samsung DeX (device and One UI version not recorded): high frame rate in a
  window, limited when fullscreen. Probably DeX / Samsung game-mode frame-rate
  management. Not reproduced or measured in this repository. Related: `vk_main.c`
  already calls `ANativeWindow_setFrameRate(240)` because without a declared target
  the compositor paced the game to about 65-85 fps with the CPU and GPU idle.
- An older tablet (model not recorded) downloaded the game data and then closed
  the app. The cause was never found (no ADB access to it).
