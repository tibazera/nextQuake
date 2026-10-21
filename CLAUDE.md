# Instruções para Claude — nextQuake

## Estado atual — 2026-10-21

A primeira versão pública Android `v0.1.0` foi publicada no GitHub Releases. O nome exibido é `nextQuake` e o
application ID é `org.nextquake.android`; a pasta de dados continua sendo
`/storage/emulated/0/Documents/ezQuake`. Releases usam a chave de produção local
`%USERPROFILE%\.android\nextquake-release.jks`, configurada fora do Git em
`%USERPROFILE%\.gradle\gradle.properties`. Preserve e faça backup desses dois
arquivos: perder a chave impede atualizações sobre instalações existentes.

Este estado substitui as notas antigas abaixo sobre branding não commitado,
submódulos vazios e ausência de build neste checkout.

## Retomada em 2026-10-10

Trabalhe em **E:\Projetos Linux\nextQuake**, repositorio https://github.com/tibazera/nextQuake (privado), branch main. Esta e uma copia Git independente, com historico preservado. E:\Projetos Linux\ezquake-source e o projeto anterior; nao altere essa pasta nem o worktree desktop para tarefas do nextQuake.

Antes de analisar ou alterar, leia AGENTS.md, continue.md e docs/android-pocket-vulkan-handoff.md. Inspecione status e diff locais. Nao edite simultaneamente com outro assistente; nao apague alteracoes locais. Commit/push/PR exigem pedido atual de Tiago.

## O que ja esta publicado

Base Android funcional importada de 52fab4df. README em ingles, creditando ezQuake/QW-Group e GPL v2, logo no topo e guia de instalacao. Ultimo commit publicado: e05182f7.

Escopo decidido por Tiago: somente Android e iOS. Desktop herdado nao e alvo, mas nao remova mecanicamente codigo compartilhado. iOS ainda nao foi implementado.

## Alteracoes locais a preservar

- Splash: app/src/main/res/drawable-nodpi/nextquake_splash.png, arte 'Logo nextQuake em arena infernal.png'. Substitui ezquake_splash.jpg, cuja exclusao e intencional. Drawable de fundo compartilhado e tema Android 12 apontam para a nova arte.
- Icone: logo 'Logo metálico “nextQuake” com símbolo Quake.png', em mipmap-mdpi/hdpi/xhdpi/xxhdpi/xxxhdpi/nextquake_icon.png. Manifesto usa nextquake_icon em icon e roundIcon.
- iOS: ios/Assets.xcassets/AppIcon.appiconset com PNG opaco 1024px e Contents.json; ios/README.md explica a integracao futura. Ainda nao existe projeto Xcode nem IPA.
- Notas de retomada em continue.md e handoff.

Estas mudancas NAO foram commitadas nem enviadas. aapt2 compilou os recursos Android com sucesso; nao houve novo APK, validacao em launcher, build Xcode ou teste em aparelho.

## Proximo passo tecnico

Inicializar submodulos nesta copia (vcpkg e src/qwprot estao vazios): git submodule update --init --recursive. Configurar JDK 17, Android SDK C:\Android\Sdk, NDK 28.2.13676358 e ferramentas host de shader conforme handoff. Os scripts locais _build_android*.bat do projeto antigo fixam o caminho antigo: nao execute esses scripts sem adaptar o diretorio. Fazer build Android Debug e Release no nextQuake e verificar que o APK empacota a nova arte. Tiago valida visualmente; nao tire screenshots automaticamente.

Nao mudar novamente o nome exibido, application ID ou pasta de dados sem combinar a migracao. A pasta de dados CONFIRMADA por Tiago e /storage/emulated/0/Documents/ezQuake (Armazenamento interno > Documents > ezQuake), contendo id1, qw e ezquake diretamente. APK sozinho nao inclui esses dados. O launcher tenta baixar quando falta a base, mas seu check e incompleto.

Preserve touch/IME: esquerdo move, direito mira; um toque mira, duplo toque dispara; FIRE permite mirar arrastando; KBD abre teclado/console. Nao restaurar as experiencias rejeitadas que misturavam movimento e pitch/yaw. Preserve otimizacoes Vulkan. Outline e opcional e reduz FPS quando ligado.
