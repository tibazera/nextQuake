# continue.md — nextQuake

Notas de retomada vivas. Leia AGENTS.md antes. Histórico antigo do ezquake-source:
docs/continue-history-ezquake.md e docs/ezquake-origin-agent-history.md.

## Organização e CI — 2026-10-27

- iOS vive em branch separada (`ios`); esta linha trata só Android e infraestrutura comum.
- Novo workflow `.github/workflows/android.yml`: build `:app:assembleDebug` (arm64, Vulkan) em todo push/PR,
  com submódulos, JDK 17, NDK 28.2.13676358, glslang do apt e cache binário do vcpkg. APK sai como artefato.
  Validado: run 2 (81efeb7) passou em ~8 min. A run 1 falhou só no passo android-actions/setup-android, trocado pelo SDK do runner.
- Escala de renderização já existe: cvar `vid_android_resolution` com seletor no menu (Auto = envelope 1920x1080).
- Launcher: dados de base vazios (0 bytes) não contam mais como presentes, e um install copiado à mão sem
  `ezquake/ezquake.pk3` baixa só esse arquivo, sem misturar outros pacotes (ex.: gpl_maps.pk3).
  Não compilado nem testado em aparelho nesta sessão (ambiente sem Android SDK).
- `SESSION_NOTES.md` movido para docs/session-notes-vulkan-port.md.
- Atenção: o próprio manifesto de download (`GameDataManifest.essential()`) baixa `id1/gpl_maps.pk3`, o arquivo
  que sobrepõe dm3/dm4/dm6 com BSPs de textura simples (ver handoff). Pela regra do handoff a correção é de
  dados (PK3 limpo distribuído à parte), não de código; decisão pendente com Tiago.

## Compatibilidade Android Adreno / PK3 — 2026-10-26

- Correções locais, ainda sem commit, aplicadas igualmente em `main` e `ios`: seek de arquivos ZIP sem underflow, push constants Vulkan limitados a 128 bytes, composite alpha suportado pela surface, sinais fatais Android sem conversão para `exit(0)` e início padrão no menu.
- O Android Debug da branch `ios` compilou com sucesso. Os oito arquivos compartilhados são idênticos ao worktree isolado de `main`; o build nesse worktree não iniciou o CMake porque submódulos não são materializados automaticamente nele.
- Também corrigidos os dois erros restantes da validation layer: texturas só ficam disponíveis para desenho após chegarem a `SHADER_READ_ONLY_OPTIMAL`, e cada imagem da swapchain possui seu próprio semáforo render-finished.
- Ainda falta validação visual e com validation layer no Xiaomi 14T Pro e no Galaxy Tab S7.

## Release Android 0.1.0 — 2026-10-21

- Identidade pública definida: nome `nextQuake`, application ID `org.nextquake.android`; namespace Java e pasta `Documents/ezQuake` preservados.
- Chave RSA 4096 de produção criada fora do Git e Gradle configurado por propriedades locais. Nunca publicar ou perder a chave/credenciais.
- Build Release limpo, arm64/Vulkan, versão 0.1.0 e assinatura v2 passaram; `tests/touch-keyboard.ps1` também passou.
- Corrigidas declarações ausentes da integração ImGui e coberto o estado de teclado do ImGui no teste.
- Alterações locais experimentais em `src/in_touch.c` e `src/vid_sdl2.c`, além dos patches soltos, ficaram fora da versão.
- Tag/release `v0.1.0` publicada: https://github.com/tibazera/nextQuake/releases/tag/v0.1.0
- APK: 17.732.274 bytes; SHA-256 `2358BB15047C2C0C10963B2C11D1B4091384CE50F72DBA2FDC0F8A7BEE681156`.
- Validado no celular USB em 2026-10-21: APK instalado, abriu e Tiago confirmou que funcionou corretamente. O pacote antigo `org.ezquake.android` foi preservado em paralelo.

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

