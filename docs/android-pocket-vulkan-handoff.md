## Release Android 0.1.0 — 2026-10-21

Primeira versão pública preparada com nome `nextQuake`, application ID
`org.nextquake.android`, arm64/Vulkan e assinatura de produção. A pasta de dados
continua `/storage/emulated/0/Documents/ezQuake`. Build Release limpo e
`tests/touch-keyboard.ps1` passaram. Experimentos locais não commitados de touch
e os patches soltos não fazem parte da versão. Teste manual no aparelho ainda é
necessário.

Release publicada: `v0.1.0`, APK Android arm64 com 17.732.274 bytes e SHA-256
`2358BB15047C2C0C10963B2C11D1B4091384CE50F72DBA2FDC0F8A7BEE681156`.
Instalação e abertura no celular USB foram confirmadas por Tiago em 2026-10-21;
a versão funcionou corretamente e o pacote ezQuake anterior permaneceu instalado.

## Retomada para Claude — 2026-10-10

Pasta nova independente: E:\Projetos Linux\nextQuake. Leia CLAUDE.md primeiro para o resumo da transicao, arquivos locais de splash/icones e proximos passos. main/origin main publicados ate e05182f7; branding local ainda nao commitado. Submodulos desta copia ainda nao inicializados. Nenhum APK novo desta pasta foi gerado.

App icon update (2026-10-10): user-provided metallic nextQuake logo is now referenced by Android icon/roundIcon, with mdpi through xxxhdpi sizes. ios/Assets.xcassets/AppIcon.appiconset contains a 1024px opaque icon ready for the future iOS target; it is not integrated into a running iOS app. Android resource compilation passed; no full APK build, launcher visual test, or Xcode validation. Changes remain local, uncommitted.

Startup artwork update (2026-10-10): replaced the inherited QuakeWorld wallpaper with the user-provided 'Logo nextQuake em arena infernal.png' as drawable-nodpi/nextquake_splash.png. Shared window/startup/download background and Android 12 splash icon reference the new artwork. aapt2 resource compilation passed; no new APK build or device visual test. Changes remain local, uncommitted.

Installation documentation update (2026-10-10): README now includes an English Android installation guide, the confirmed /storage/emulated/0/Documents/ezQuake directory, full id1/qw/ezquake copying instructions, storage permissions, and the launcher's incomplete base-data check. Documentation only; no new device installation test.

# nextQuake — retomada em 2026-10-10

Tiago decidiu concentrar este projeto exclusivamente em Android e iOS. Fonte: E:\Projetos Linux\nextQuake, repositorio tibazera/nextQuake, main. O projeto original permanece preservado.

Base importada integralmente de 52fab4df, incluindo touch/IME e Release Android. Nenhuma mudanca no motor nesta criacao. iOS ainda nao iniciado; requer integracao SDL3/MoltenVK, bootstrap/armazenamento, audio SDL, habilitacao dos controles e build macOS/Xcode.

Identidade Android (org.ezquake.android/nome exibido) e workflows desktop ainda sao herdados. Proximas tarefas: definir identidade mobile e adaptar build/CI ao foco Android/iOS, preservando a versao Android funcional. Nao remover codigo desktop compartilhado mecanicamente.

A validacao Android registrada abaixo refere-se a base original, nao a um build novo desta copia. Historico abaixo tem caminhos e prioridades antigos; esta secao e AGENTS.md prevalecem.

---
# Android Pocket Vulkan handoff

## Atualização — 2026-10-26

- Aplicadas localmente nas branches `main` e `ios` as correções do Galaxy Tab S7: seek correto em PK3, push constants de world/sprite dentro do mínimo Vulkan de 128 bytes, composite alpha negociado com a surface, crash signals preservados no Android e `cl_onload menu` na configuração distribuída.
- Android Debug compilou na branch `ios`; os arquivos compartilhados foram comparados e são idênticos no worktree de `main`. Validação em aparelho ainda pendente.
- Os dois avisos restantes da validation layer também foram corrigidos: texturas pendentes não são amostradas em layout indefinido e o semaphore render-finished agora pertence à imagem da swapchain.
- Falta repetir a validação em aparelho; o Android Debug recompilou com sucesso.

Branch: `feature/android-pocket-vulkan`

## Latest handoff — 2026-10-09, Codex

This section supersedes older touch/performance status notes. Read the current summary at the top of `continue.md` before editing. Never edit this checkout concurrently with another assistant.

- Left stick controls forward/backward/strafe only. Right-side drag controls yaw/pitch. Walking does not tilt the camera. The combined movement/look experiments were rejected by Tiago.
- A single right-side touch/drag only aims. Two short nearby taps trigger attack on the second DOWN; holding that second touch permits aiming while firing, and UP/CANCEL stops firing. FIRE still supports press-and-drag aiming. All touch attack sources are released correctly on cancellation/mode change.
- L/R use `weapon 12`/`weapon 10`, matching the existing menu binds. The user's `cl_weaponpreselect 4` can delay selection until firing in deathmatch; do not overwrite his config.
- Existing Quake button backgrounds, menu rune, chat balloon and configured crosshair take priority; Kenney CC0 artwork fills gaps. No Quake data is versioned.
- KBD remains visible with touch enabled and opens console/IME. The return-arrow button folds the console. The forced startup console accepts IME text even with `key_dest == key_game`.
- `in_touch 2` follows actual physical/touch input, rather than disabling itself solely because SDL reports attached keyboard/mouse. Initial-console keyboard opening was confirmed on Xiaomi with `mInputShown=true`/`ImeTracker onShown` after the approximately nine-second engine startup. All temporary IME logs were removed.
- World outlines were visually confirmed by Tiago; they are optional and reduce FPS when enabled. `vk_outline_scale 0.5` uses a half-size normals target; changing it requires `vid_restart`. Exactly one cvar definition/registration exists.
- Android Release uses `-O3`, is non-debuggable, and compiles out Vulkan profiling, input diagnostics and Vulkan validation layers. Packaged native debug sections are stripped. Debug keeps the optional profiler for future diagnosis.
- Both Android debug and release builds, and `tests/touch-keyboard.ps1`, passed. Release APK: `app/build/outputs/apk/release/app-release.apk` (arm64, Android 10+, test signing key). S24 Ultra testing and performance measurements remain pending.
- Preserve all work listed here. Tiago requested a local commit to protect it; no push/PR is authorized. Local build scripts/logs and APKs are not source deliverables to commit.

## Current state

- Android arm64 client builds with SDL 3.4, Vulkan and the Oboe backend.
- The SDL3 Java bootstrap is synced with SDL 3.4 and starts the exported native `main` entry point.
- SDL3 boolean return contracts are applied to initialization, mutexes, semaphores, events and Vulkan surface creation.
- Dynamic lightmap sub-image updates are queued and copied through a persistent mapped staging buffer in the next frame command buffer. This removes the immediate-command-buffer and queue-idle path from normal frame updates.
- The renderer now permits two frames in flight instead of waiting for the previous frame at every `VK_BeginFrame`. Each frame owns its synchronization objects and lightmap staging buffer, while each swapchain image tracks the fence that last used it.
- Android rendering is capped to a 1920x1080 envelope while preserving device aspect ratio and never upscaling smaller displays. The 2712x1220 test device renders at 1920x864; touch normalization continues to use the physical View dimensions.
- `SDLActivity` now adds the `SurfaceView` to its `RelativeLayout` with explicit `MATCH_PARENT` layout params. Without this, the surface-size cap above exposed a latent bug: the layout defaulted to `WRAP_CONTENT`, and once the surface had a fixed buffer size, `SurfaceView.onMeasure()` used that fixed size as its preferred view size instead of filling the screen, leaving most of the display black. Confirmed visually and via `dumpsys SurfaceFlinger` layer bounds before and after the fix.
- Voice capture is disabled on Android; playback uses Oboe.

## Build

Required local versions used for validation:

- JDK 17
- Android SDK at `C:\Android\Sdk`
- NDK `28.2.13676358`
- glslang from the vcpkg host tools directory on `PATH`

Build with `gradlew.bat :app:assembleDebug`. The APK is generated at `app/build/outputs/apk/debug/app-debug.apk`.

## Device-specific pitfalls

- Do not implement a code-side fix, automatic archive rewrite or special-case map loading for the texture issue described below. Quake data must not be packaged or committed in this repository. The intended solution is to distribute a separate, corrected PK3 and let the normal filesystem precedence rules load it.
- Root cause observed on the test device: `id1/gpl_maps.pk3` contained replacement simple-texture BSPs at `maps/dm3.bsp`, `maps/dm4.bsp` and `maps/dm6.bsp`. Because that archive had higher filesystem precedence, those BSPs shadowed the original maps from `pak1.pak`. Animated lava, portals and item textures still appeared, but most wall and geometry textures were missing, which initially looked like a Vulkan texture-upload failure.
- Diagnostic confirmation: removing only those three BSP entries from the active archive immediately restored the original textured maps. This was a device-side experiment, not a project fix. A copy named `gpl_maps.pk3.simpletextures-backup` was retained on the test device.
- Follow-up data task: build or obtain a clean replacement PK3 that does not override dm3, dm4 and dm6 with the simple-texture BSP variants. Validate its contents and load order against `pak1.pak` before distributing it. Renderer changes should only be reconsidered if the problem reproduces with known-good BSP and texture data.
- scrcpy captures Android audio by default and routes it to `AUDIO_DEVICE_OUT_REMOTE_SUBMIX`, making the phone speaker appear silent. Use `scrcpy --keyboard=uhid --mouse=uhid --no-audio` when sound must remain on the phone.
- Xiaomi blocks normal ADB input injection unless its additional USB debugging security option is enabled. UHID keyboard and mouse work without that permission.

## Audio diagnosis

The SDL2 expression `SDL_TryLockMutex(mutex) == 0` was invalid after migration because SDL3 returns `bool`. It acquired the mixer mutex, treated success as failure and left it locked, producing the SDL mutex assertion and silence. `S_TryLockMixer()` now uses the SDL3 boolean result directly. Device logs confirmed that the Oboe callback produces non-zero PCM and AAudio plays it; remaining apparent silence during desktop mirroring was scrcpy audio routing, not the mixer.

## Next performance work

- Measure dm3/dm4/dm6 frame timings after the lightmap batching and two-frames-in-flight changes. Startup/stability has been validated on the Xiaomi test device, but the in-map FPS gain is not yet measured.
- Profile world draw-call count, pipeline switches and CPU/GPU frame split before importing broader renderer changes from vkQuake or FTEQW.
- Compare frametimes at the capped 1920x864 surface against the former native 2712x1220 path, then decide whether a user-selectable render-scale setting is still necessary.
