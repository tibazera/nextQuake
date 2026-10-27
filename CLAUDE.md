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

## Regras de trabalho

Repositorio https://github.com/tibazera/nextQuake (privado), branch main; iOS em branch separada. Antes de analisar ou alterar, leia AGENTS.md, continue.md e docs/android-pocket-vulkan-handoff.md. Inspecione status e diff locais; nao apague alteracoes locais. Commit/push/PR exigem pedido atual de Tiago. O projeto anterior (ezquake-source) nao deve ser alterado.

Escopo: somente Android e iOS. Desktop herdado nao e alvo, mas nao remova mecanicamente codigo compartilhado. CI Android: .github/workflows/android.yml (Debug em todo push/PR). Tiago valida visualmente; nao tire screenshots automaticamente.

Nao mudar novamente o nome exibido, application ID ou pasta de dados sem combinar a migracao. A pasta de dados CONFIRMADA por Tiago e /storage/emulated/0/Documents/ezQuake (Armazenamento interno > Documents > ezQuake), contendo id1, qw e ezquake diretamente. APK sozinho nao inclui esses dados. O launcher tenta baixar quando falta a base, mas seu check e incompleto.

Preserve touch/IME: esquerdo move, direito mira; um toque mira, duplo toque dispara; FIRE permite mirar arrastando; KBD abre teclado/console. Nao restaurar as experiencias rejeitadas que misturavam movimento e pitch/yaw. Preserve otimizacoes Vulkan. Outline e opcional e reduz FPS quando ligado.
