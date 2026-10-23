# nextQuake — Android e iOS: diferenças e convivência

Atualizado em 2026-10-23. Este documento registra tudo o que foi feito para
o port iOS e, para cada assunto, deixa explícito o que é Android, o que é
iOS e o que é compartilhado. Leia antes de mexer em código de plataforma.

## Situação

| | Android | iOS |
|---|---|---|
| Status | Estável, publicado (v0.1.1) | Experimental, nunca rodou em iPhone real |
| Branch | `main` | `ios` |
| Distribuição | APK no GitHub Releases | IPA sem assinatura (artifact do CI), sideload com Apple ID grátis (7 dias) |
| Identificador | `org.nextquake.android` | `org.nextquake.ios` |
| Pasta de dados | `/storage/emulated/0/Documents/ezQuake` | `Documents` do app (visível no app Arquivos) |
| Build | Gradle + NDK no PC do Tiago | CMake/Xcode só no CI (`.github/workflows/ios.yml`, macos-15) |
| Assinatura | Chave `nextquake-release.jks` (preservar!) | Nenhuma; a ferramenta de sideload assina na instalação |
| Teste | Celular via adb, Tiago valida visualmente | Simulador no CI; iPhone real só por testadores externos |

Regra: o Android é o produto. Nada do iOS pode mudar o comportamento do
Android. Nenhum merge de `ios` em `main` sem pedido explícito do Tiago.

## Código compartilhado e como cada plataforma é separada

O motor é um só (`src/`). A separação é feita por macros:

| Macro | Vale para | Onde é definida |
|---|---|---|
| `__ANDROID__` | só Android | compilador (NDK) |
| `TARGET_OS_IOS` / `TARGET_OS_IPHONE` / `TARGET_OS_SIMULATOR` | só iOS (e simulador) | `<TargetConditionals.h>` (incluir antes de testar!) |
| `EZ_TOUCH` | Android **e** iOS | `src/in_touch.h` |
| `EZ_MACOS` | só macOS desktop (antes era `__APPLE__`, que também pegava iOS) | CMake |
| `EZQUAKE_IOS` | só iOS | variável do CMake |

Armadilha conhecida: `TARGET_OS_SIMULATOR` sem incluir `<TargetConditionals.h>`
vira 0 sem erro nenhum, e o `#if` some em silêncio.

Compilação: o Android usa `-std=gnu89` com `-Werror` para variável não usada.
Código novo compartilhado deve declarar variáveis no topo do bloco e não
deixar variável só usada num `#ifdef`. Sempre rodar
`.\gradlew.bat :app:assembleRelease` depois de mexer em arquivo compartilhado.

## Assunto por assunto

### Renderização (Vulkan)
- Android: Vulkan nativo do aparelho, com otimizações próprias (resolução
  escolhível, recriação de surface). Intocado.
- iOS: Vulkan sobre Metal via MoltenVK v1.3.0 estático (`MOLTENVK_LIBRARY`
  no CMake, baixado pelo CI). Só no simulador: argument buffers do Metal
  desligados por `VK_EXT_layer_settings` em `src/vk_instance.c` (a GPU virtual
  aborta com samplers neles; variável de ambiente não funciona porque o
  MoltenVK lê a config antes). O simulador também não tem arrays de
  texturas, então o 3D fica preto lá: só um iPhone real valida o jogo.

### Controles de toque e teclado na tela
- Compartilhado (`EZ_TOUCH`): `src/in_touch.c`, eventos de dedo, teclado
  virtual, menu ImGui com toque, pausa ao ir para segundo plano
  (`src/vid_sdl2.c`, `src/menu_imgui.cpp`). Regras de controle (esquerdo
  move, direito mira, duplo toque atira, FIRE mira arrastando, KBD) valem
  para os dois.
- Só Android: vibração via JNI (`SDLActivity.performHapticClick`),
  diagnósticos `in_android_diagnostics`, botão Voltar.
- Só iOS: vibração por `UIImpactFeedbackGenerator` (`src/ios_haptic.m`);
  controles afastados do notch/Dynamic Island por `VID_SafeAreaInsets`
  (no Android o layout continua igual ao validado).

### Arte dos controles
- Fonte única: `app/src/main/assets/touch/` (Kenney CC0 + game-icons CC BY 3.0).
- Android: o launcher Java copia para `<dados>/ezquake/gfx/touch/`.
- iOS: o CMake copia a pasta para dentro do app e `IN_TouchInit` copia para
  `Documents/ezquake/gfx/touch/` no primeiro uso.

### Download dos dados no primeiro uso
- Fonte única da lista: `GameDataManifest.java` (método `essential()`),
  arquivos do repositório nQuake/distfiles.
- Android: `EzQuakeLauncherActivity` + `GameDataInstaller.java` (Java,
  HttpURLConnection), com oferta do pacote HD.
- iOS: `src/ios_gamedata.c` (C, libcurl, tela simples do SDL), chamado no
  `main` antes do `Host_Init`. O CMake gera `gamedata.txt` lendo o arquivo
  Java, então mudar o manifesto no Android muda o iOS também. Pacote HD
  ainda não existe no iOS.
- HTTPS: no iOS o curl usa mbedtls, que não enxerga os certificados do
  sistema; por isso o app leva o `cacert.pem` do curl.se. A checagem de
  versão (`src/version.c`) também usa curl e ainda não recebeu o CA no iOS.

### Arquivos e entrada do programa
- Android: pasta de dados fixa e combinada com o Tiago; não mudar sem migração.
- iOS: `fs.c` usa `SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS)`; `sys_posix.c`
  inclui `SDL_main.h` porque o SDL controla o `main` real; `system()` não
  existe e devolve -1 (`src/sys.h`).

### Som
- Android: Oboe. iOS: backend SDL. Speex (voz) desligado no iOS.

### Ícone e identidade
- Android: mipmaps `nextquake_icon`, splash próprio.
- iOS: `ios/Assets.xcassets/AppIcon` compilado no app; Info.plist gerado
  pelo CMake (landscape, compartilhamento de arquivos ligado, nome nextQuake).
- Os dois se identificam como "nextQuake 0.1.1" (`VERSION`).

## CI

- `.github/workflows/main.yml`: só manual (`workflow_dispatch`); builds
  desktop desligados.
- `.github/workflows/ios.yml` (roda em push na branch `ios`):
  - `simulator`: build Debug, instala no simulador **sem** dados, exige
    que o app baixe os dados (>= 50 arquivos) e continue aberto; guarda
    logs, screenshots e crash reports.
  - `device`: build Release `iphoneos`, empacota `nextQuake-unsigned.ipa`.

## O que é preciso para os dois conviverem no `main`

1. Manter toda diferença atrás de macro, nunca com `if` em tempo de
   execução, e sem mudar o caminho do Android.
2. Antes de qualquer merge: build Release Android + instalar no celular +
   Tiago validar; e o CI iOS verde.
3. Trazer `ios.yml` junto, disparando também em `main` (ou manual), para o
   iOS não quebrar em silêncio com mudanças do Android.
4. Versão única (`VERSION`) para os dois; releases podem anexar APK e IPA,
   marcando o IPA como experimental.
5. Manifesto de dados único (já é); qualquer novo arquivo de dados entra
   no `GameDataManifest.java`.
6. Arquivos só do iOS (`src/ios_*.c/.m`, `ios/`) e só do Android (`app/`)
   não precisam de macro, porque cada build só compila os seus.

## Pendências do iOS

- Resultado do primeiro teste em iPhone real (um amigo do Tiago, iPhone 18,
  pacote em `%USERPROFILE%\Desktop\nextQuake-iOS-teste`).
- 3D, desempenho e toque só são validáveis em aparelho.
- CA para a checagem de versão; pacote HD opcional.
- Publicação oficial exige conta Apple Developer (US$ 99/ano); App Store tem
  conflito com a GPL. Preferir TestFlight, sideload ou lojas alternativas da UE.
