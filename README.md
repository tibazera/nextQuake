# nextQuake

Cliente QuakeWorld focado exclusivamente em **Android e iOS**, baseado no [ezQuake do QW-Group](https://github.com/QW-Group/ezquake-source). nextQuake e um projeto independente; nao e uma versao oficial do QW-Group.

## Estado atual

- Android arm64: SDL3, Vulkan, audio Oboe, controles touch, console e teclado Android.
- Android Debug e Release validados na base importada, commit 52fab4df. Esta copia ainda nao recebeu um novo build.
- iOS: port ainda nao implementado. Caminho previsto: SDL3, Vulkan sobre MoltenVK/Metal, audio SDL e integracao com armazenamento/teclado iOS.
- O codigo desktop herdado permanece como infraestrutura; nao e uma plataforma alvo deste projeto.

## Desenvolvimento

Leia AGENTS.md, continue.md e docs/android-pocket-vulkan-handoff.md antes de alterar os controles ou o renderer. O historico anterior foi preservado para rastrear autoria e decisoes.

Build Android: configure JDK 17, Android SDK e NDK conforme o handoff e execute `gradlew.bat :app:assembleDebug` ou `gradlew.bat :app:assembleRelease`.

O build iOS requer macOS/Xcode local ou remoto. Ainda nao existe IPA nem validacao em iPhone.

## Licenca e dados

Creditos ao QW-Group, aos autores do ezQuake e aos contribuidores do codigo original. A licenca GNU GPL v2 herdada do ezQuake permanece em [LICENSE](LICENSE). Preserve os avisos de copyright e a atribuicao aos autores originais. Dados proprietarios do Quake (PAK/PK3/mapas) nao fazem parte deste repositorio.

O nome exibido pelo aplicativo e o identificador Android ainda sao os herdados do ezQuake; a mudanca de identidade sera uma tarefa separada.
