## Release Android 0.1.0 — 2026-10-21

- Identidade pública definida: nome `nextQuake`, application ID `org.nextquake.android`; namespace Java e pasta `Documents/ezQuake` preservados.
- Chave RSA 4096 de produção criada fora do Git e Gradle configurado por propriedades locais. Nunca publicar ou perder a chave/credenciais.
- Build Release limpo, arm64/Vulkan, versão 0.1.0 e assinatura v2 passaram; `tests/touch-keyboard.ps1` também passou.
- Corrigidas declarações ausentes da integração ImGui e coberto o estado de teclado do ImGui no teste.
- Alterações locais experimentais em `src/in_touch.c` e `src/vid_sdl2.c`, além dos patches soltos, ficaram fora da versão.
- Tag/release `v0.1.0` publicada: https://github.com/tibazera/nextQuake/releases/tag/v0.1.0
- APK: 17.732.274 bytes; SHA-256 `2358BB15047C2C0C10963B2C11D1B4091384CE50F72DBA2FDC0F8A7BEE681156`.
- Pendente: teste manual no celular.

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
# Continue.md — Android game-data downloader + Vulkan (desktop, worktree irmão)

## Estado atual para Claude/Codex — 2026-10-09 (leia primeiro)

Este resumo substitui as experiências intermediárias descritas no histórico abaixo. Fonte de verdade: E:\Projetos Linux\ezquake-source, branch feature/android-pocket-vulkan. Não trabalhar em paralelo no mesmo checkout: uma sessão do Claude se iniciou sem querer e sobrepôs alterações, chegando a duplicar vk_outline_scale. A definição extra/callback indesejado não estão mais presentes; estado final restaurado e validado com uma definição CVAR_LATCH_GFX, padrão 0.5.

**Controles finais:** esquerdo = frente/trás/strafe, direito = mira. Não acoplar novamente movimento e pitch/yaw. Um toque/arrasto livre só mira; dois toques curtos próximos (250ms de toque, segundo até300ms) iniciam tiro no segundo DOWN. Segurar o segundo permite mirar atirando; UP/CANCEL solta. FIRE também aceita arrasto durante tiro. Fontes simultâneas de ataque são contadas para evitar ataque preso/liberado cedo.

**Console/teclado:** KBD sempre disponível quando touch está ativo; abre console e solicita IME. Seta de retorno recolhe console. IN_TouchConsoleActive reconhece o console forçado inicial e os caminhos SDL_TEXTINPUT/Key_Event_TextInput aceitam suas letras. in_touch 2 segue o último evento físico/touch; não força off só por SDL_HasKeyboard/HasMouse. Abertura automática confirmada no Xiaomi após startup de aproximadamente9s: mInputShown=true, SDLDummyEdit/SDLInputConnection e ImeTracker onShown. Não há IME_DEBUG/IME_TRACE no código final.

**Imagens:** prioridade aos arquivos Quake já existentes (fundos ez_button, runa q, balão chaticons e crosshair do config), Kenney CC0 só para lacunas. Não adicionar PK3/PAK/dados do usuário ao Git.

**Armas:** L/R = weapon 12/10 (mesmo menu nativo). Config do Tiago contém cl_weaponpreselect 4; a troca pode ser pré-seleção até disparar em deathmatch. Não alterar a cfg para contornar isso.

**Vulkan:** preservar port dos outlines e otimizações anteriores do Claude (static index, sky/bind cache, aposentadoria de descriptors, resolução Android). Outline é opcional, custo de FPS confirmado; desligar devolve performance. vk_outline_scale 0.5 usa 25% dos pixels no normals pass; exige vid_restart. Redesenho do mundo e composição fullscreen continuam, sem ganho de FPS medido para a escala.

**Release para amigo com S24 Ultra:** assembleRelease usa CMake Release/-O3, APK não-debuggable; EZQUAKE_ANDROID_RELEASE compila fora profiling, diagnóstico de entrada e validation layers. Sem seções .debug na lib nativa empacotada. Assinatura v2 válida, mesma chave de testes interna (não chave de publicação). APK em app/build/outputs/apk/release/app-release.apk, 15.316.834 bytes, SHA256 634742F5DCC7EF998881677791AC7009491370423BA699306D5B7BC2B91AC41C. Android 10+ / arm64. Nada enviado a terceiros pelo agente: Tiago compartilha o arquivo.

**Validação:** testes touch/IME/double-tap e builds Android Debug/Release passaram. Release também exigiu eliminar stubs sem uso e lista de extensões sem uso; mantida guarda Vulkan em R_Shutdown para builds GL. Nenhum teste em S24 Ultra e nenhum benchmark novo de CPU/GPU/termal realizado. iPhone ainda é port futuro, não iniciado.

**Git:** Tiago autorizou nesta sessão commit local do estado recuperado para protegê-lo. Não autorizou push/PR. Stage deve incluir código, shaders, assets CC0/licença, teste e handoff; não logs/APKs/scripts locais com caminhos específicos. Depois do commit, continue a partir dele e não reaplique patches antigos.


Última atualização: 2026-07-22 (sessão de commits + investigação de performance)

## Regra permanente (Tiago pediu explicitamente nesta sessão)

**Manter este arquivo atualizado em AMBOS os repositórios sempre** — este (`E:\Projetos Linux\ezquake-source\continue.md`) e `E:\Projetos Linux\ezquake-sdl3-vulkan-pr\continue.md` — toda vez que uma sessão avançar ou pausar, não só quando pedido. Objetivo: sempre saber onde paramos, em qualquer um dos dois worktrees, mesmo entre sessões/janelas de contexto diferentes.

## Duas frentes de trabalho em paralelo

### 1. Download automático de dados do Quake (Android) — este worktree, branch `feature/android-pocket-vulkan`

**Estado**: working tree limpo (só este `continue.md` modificado). Tudo já commitado/enviado (`b3c586fb`, `acb47046`).

**Pendente, ainda não retomado nesta sessão** (Tiago decidiu não trabalhar nisso agora, focar no sdl3-vulkan-pr):
- Celular novo (vili_global, `2107113SG`, serial ADB `8caabf25`): estávamos prestes a iniciar `adb -s 8caabf25 logcat -v time` em background e então abrir o app pela primeira vez, pra validar o fluxo completo de download igual já validado no rothko (2407FPN8EG). Ver histórico detalhado de comandos MIUI-friendly no git log deste arquivo se precisar recuperar os comandos exatos.
- Tablet original do Tiago: nunca conseguimos conectar via ADB, "baixa os arquivos mas depois fecha a tela" — não diagnosticado, possivelmente abandonado a favor do celular novo. Perguntar ao Tiago se ainda quer retomar.

### 2. Vulkan desktop (`vk_*.c`, compartilhado com este branch) — trabalho ativo agora em `E:\Projetos Linux\ezquake-sdl3-vulkan-pr`, branch `feature/sdl3-vulkan-pr`

**Este é o foco atual da sessão.** Estado completo, detalhado e atualizado está em `E:\Projetos Linux\ezquake-sdl3-vulkan-pr\continue.md` — ler esse arquivo lá para retomar. Resumo:

- **5 commits feitos localmente nesse worktree, testados visualmente e aprovados pelo Tiago ("funcionou ok sem bugs"), mas AINDA NÃO enviados (push pendente, aguardando autorização):**
  1. `b3ada541` — cache de last-bound-pipeline/descriptor-set no loop do mundo (`vk_world.c`), achado numa investigação de performance dedicada com o Fable 5 — elimina rebinds redundantes de pipeline/descriptor set entre draws consecutivos do mesmo material.
  2. `00e4ff14` — fix do drawflat Vulkan (bug do racat): `r_drawflat_mode` não deveria gatear ativação, só estilo de cor.
  3. `a389be91` — fix de CMake: `REGEX REPLACE` sem match retorna string original, causava `FILEVERSION` inválido em clones sem tags.
  4. `0493ea03` — fix de build: preset `msvc-x64` não fixava `RENDERER_VULKAN=ON`, causava builds OpenGL-only silenciosos ("Invalid vid_renderer value detected"). Esse fix é específico do preset CMake do worktree desktop — Android usa `_configure_vulkan.bat` próprio, não precisa dessa mudança.
  5. `8c801c33` — buffers estáticos do mundo (`vk_buffers.c`) passados para memória `DEVICE_LOCAL` com staging upload.
- **Também investigado e descartado nesta sessão** (não aplicar): uma proposta de arquitetura de terceiros (RHI, migração por sistema, instancing, command buffers secundários, HUD SDF) — a maior parte já existe no projeto com outros nomes; os itens genuinamente ausentes (instancing de verdade, command buffers secundários, HUD SDF) foram avaliados e não valem o risco/esforço agora, sem gargalo medido.
- **Próximo passo**: perguntar ao Tiago se quer (a) push desses 5 commits pra `origin feature/sdl3-vulkan-pr`, (b) portar os commits 1, 2 e 5 pra este branch Android (`vk_world.c`/`vk_buffers.c` são compartilhados, então bind cache/drawflat/DEVICE_LOCAL provavelmente aplicam aqui também — o commit 4 não se aplica ao Android).
- Nunca commitar/push em nenhum dos dois worktrees sem autorização explícita do Tiago no momento da conversa atual.

## Regras do projeto (lembrar sempre)

- Ler `AGENTS.md` (raiz deste worktree) e `docs/android-pocket-vulkan-handoff.md` antes de mexer em qualquer coisa — este worktree local é fonte de verdade, não confiar em branch remoto/PR/histórico de commits.
- **Nunca commitar/push sem autorização explícita do Tiago no momento da conversa atual.**
- Dois worktrees distintos, não confundir: `E:\Projetos Linux\ezquake-source` (este, branch Android `feature/android-pocket-vulkan`) e `E:\Projetos Linux\ezquake-sdl3-vulkan-pr` (branch desktop primário `feature/sdl3-vulkan-pr`, head real da PR #1145 upstream). Trabalho de Vulkan validado no desktop deve, quando não quebrar o Android, ser portado para os dois branches na mesma sessão.
- Build Android: variáveis de ambiente documentadas no `AGENTS.md`, seção "Build validado".
- Build Windows do worktree do PR: usar o junction `C:\eqspr` (não o path com espaço `E:\Projetos Linux\...`, que quebra o build de libs autotools como speex/speexdsp) — script `_build_wip.bat` já atualizado pra isso.
- Ao usar `adb shell` com paths absolutos tipo `/storage/...` a partir do Git Bash/MSYS no Windows, usar `//storage/...` (barra dupla) pra evitar que o MSYS reinterprete o path como caminho de host.
- MIUI pode exigir autorizar "Instalar via USB" nas Opções do desenvolvedor (separado da autorização de Depuração USB) antes de `adb install` funcionar — se der `INSTALL_FAILED_USER_RESTRICTED`, pedir pro usuário verificar essa opção e/ou aceitar um popup de confirmação que aparece na tela do aparelho no momento da instalação.

## Touch controls Android (2026-10-09) — implementado, NÃO testado em aparelho

Build Android debug OK. Nada commitado. Arquivos: `src/in_touch.{c,h}` (novo, só `__ANDROID__`), ganchos em `vid_sdl2.c` (eventos `SDL_EVENT_FINGER_*`, `IN_TouchFrame`, `IN_UpdateTextInputState` agora abre/fecha IME só quando touch ativo e `key_dest` é console/chat), `in_sdl2.c` (`IN_TouchMove`, `IN_TouchInit`), `cl_screen.c` (`IN_TouchDraw` antes do cursor), `keys.c` (texto do IME sem keydown anterior), `menu_options.c` (seção Touch Controls na página de mouse), `CMakeLists.txt`.

Cvars: `in_touch` 0 off / 1 on / 2 auto (padrão; liga se NÃO houver teclado E mouse via `SDL_HasKeyboard/HasMouse`, reavaliado todo frame, então hub USB faz hotplug), `in_touch_scale`, `in_touch_alpha`, `in_touch_look_sens` (negativo inverte). Desligado = eventos de dedo descartados, zero impacto em mouse/teclado. Ao mudar de estado, `IN_TouchReleaseAll` solta tudo. Console imprime `touch: mode=.. keyboard=.. mouse=.. -> on/off` a cada mudança.

Controles: jogo = stick flutuante esquerda (<42% da tela), look no resto, FIRE/JUMP/W+/W- à direita, TAB/SAY/CON/ESC no topo. Menu/console/chat = d-pad esquerda, OK/BACK direita, KBD (reabre IME) no topo; toque no menu move cursor e clica (clique atrasado 2 frames).

**Testar quando Tiago chegar:**
1. Sem hub: log mostra keyboard/mouse; confirmar que auto liga o overlay. RISCO: Android pode reportar teclado/mouse virtual sempre => auto nunca liga. Se acontecer, usar `in_touch 1` e trocar a detecção por último evento físico.
2. Com hub teclado+mouse: overlay some, IME nunca abre, latência igual a antes. Desplugar/replugar.
3. Menu: toque move cursor e clica? (pode precisar de `IN_MouseTrackingRequired`/cursor visível); d-pad/OK/BACK.
4. Console/chat: IME abre, digitar, Enter envia; KBD reabre. Se Key_Console(0, ch) falhar, ver `keys.c Key_Event_TextInput`.
5. Jogo: stick, look, fire, jump, trocar arma, scoreboard, multitouch simultâneo; sentido do look (ajustar sinal/sens).
6. Tamanho/posição dos botões (`Layout()` em in_touch.c). Gamepad (perfil Android) ainda NÃO feito.

## Otimizações de performance mobile (2026-10-09) — implementadas, NÃO testadas em aparelho

Build Android debug OK, nada commitado. **Backups**: `E:\tmp\eqvk-backup\` (`pre-perf-*.patch` = antes das otimizações, `post-perf-*.patch` = depois, cópias de `in_touch.*` e `vk_profile.*`). Cada otimização tem **cvar de fallback** (0 = caminho original):

| cvar | padrão | o que faz |
|---|---|---|
| `vk_opt_static_index` | 1 | índices do mundo montados 1x por mapa num buffer imutável (`vk_world.c` `VK_WorldStaticIndexBuild`) em vez de reconstruir+reenviar todo frame. Superfície com VBO alterado ou fora do worldmodel cai no caminho dinâmico sozinha. |
| `vk_opt_sky_cache` | 1 | descriptor set do céu reescrito 1x por frame, não a cada draw flat/sky |
| `vk_opt_bind_cache` | 1 | pula `vkCmdBindPipeline`/`BindDescriptorSets` repetidos em alias models e sprites 3D |
| `vk_opt_texture_nowait` | 1 | mudar filtro/anisotropia/clamp aposenta o descriptor antigo (livre após 3 frames) em vez de `vkDeviceWaitIdle` |
| `vk_profile` | 0 | `vk_profile 1` imprime a cada 2s: fps, CPU ms, GPU ms (timestamps), esperas fence/acquire/image, submit/present, draws do mundo (estáticos), índices enviados. `src/vk_profile.{c,h}` |
| `vid_android_resolution` | 0 | altura do buffer em px (0 = Auto, teto 1080p). A linha **Resolution** do menu (Options > Screen Settings) cicla Auto + lista montada a cada abertura a partir da tela detectada (`VID_AndroidBuildResolutionList`: nativa + 1440/1200/1080/900/720/600/540/480/360 abaixo dela, mesma proporção). Compositor faz upscale. JNI `EzQuakeActivity.setRenderHeight` -> `SDLSurface.applyRenderHeightCap`. |

Não feito (decisão): Swappy/display timing (vsync `r_swapInterval` + `cl_maxfps` cobrem pacing básico), ASTC/ETC2, bindless, merge de draws, loop duplo do mundo.

**Testar quando Tiago chegar** (baseline com tudo em 0, depois ligar um por vez, `vk_profile 1`, dm3/dm4/dm6, mesma posição, 60 s):
1. Visual igual com `vk_opt_static_index` 1 vs 0 (paredes, portas/plats, água, céu, luma/fullbright, mapa novo, `vid_restart`). `idx up` do log deve cair para ~0.
2. Trocar anisotropia/filtro no menu com mapa carregado: sem travada, sem textura errada (`vk_opt_texture_nowait`).
3. Modelos/sprites/partículas corretos com `vk_opt_bind_cache 1`.
4. Menu Resolution: cada opção preenche a tela, toque OK, sem tela preta/crash; Auto volta ao normal; escolha persiste após reiniciar o app.
5. Log `android video: display WxH, N resolutions offered`: confirma que `SDL_GetDesktopDisplayMode` devolve a resolução FÍSICA e não a do buffer capado (se vier 1920x864 a lista sai errada). Linhas Bit Depth/Fullscreen do menu seguem visíveis no Android, sem efeito.
6. Se algo quebrar: cvar correspondente = 0 e anotar o sintoma.

## Outline de mundo (gl_outline & 2) portado para o Android Vulkan (2026-10-09) — compila e instala, NÃO verificado visualmente

Portado de `c05de798` + `ac53d594` (branch desktop) só a parte do outline, sem cáusticas/fxaa/vsync/gamma/screenshot:
- Shaders novos: `vk_world_normals.{vert,frag}`, `vk_world_outline.frag` (registrados no `CMakeLists.txt`).
- `vk_renderpass.c`/`vk_swapchain.c`: render pass de normais (RGBA16F + depth, sempre 1 amostra) e framebuffers por imagem do swapchain.
- `vk_draw.c`: pipeline/composição do outline + teardown em `VK_HudResourcesShutdown` (fix do hang no vid_restart).
- `vk_world.c`: `VK_DrawWorldNormalsPass` (mundo opaco só com posição; respeita index buffer estático/dinâmico), `VK_WorldReopenMainRenderPass`, composição entre o mundo opaco e alias/sprites. Neste branch o main pass não tem variante LOAD: reabre-se o mesmo pass (re-clear) o que é sem perda porque nada foi desenhado antes.
- `r_brushmodel_surfaces.c`: `R_DrawWorldOutlines` usa `RuleSets_AllowEdgeOutline()` (antes bloqueava gl_outline 2/3 fora de cheats/demo).
- Fallback: `vk_opt_world_outline` (padrão 1; 0 desliga o pass inteiro). Custo zero com `gl_outline` sem bit 2.
- Backup pré-port: `E:\tmp\eqvk-backup\pre-outline-*.patch`.

**Testar**: `gl_outline 2` ou `3` num mapa (dm3), ver contornos nas arestas de paredes; `vid_restart` (não pode travar); MSAA e post-process (gamma/FXAA) ligados e desligados; alternar `vk_opt_world_outline 0/1`; conferir erros no logcat.

## Retomada Codex — 2026-10-09

GitHub confirmado via gh: origin/feature/android-pocket-vulkan = db779096af939b25aacfbc0dd7ef0548a5e7c5f7, igual ao HEAD local. Touch, assets Kenney, otimizações e outlines de hoje continuam NÃO commitados. PR desktop #1145 continua OPEN. CI Android-branch run 29971114257 falhou no job Linux ao linkar VK_AbandonActiveFrame em build sem Vulkan; corrigida localmente a guarda RENDERER_OPTION_VULKAN em R_Shutdown. CI remoto não foi reexecutado (sem push).

Histórico enviado por Tiago: outline confirmado visualmente por ele, com custo alto de FPS. Kenney já compilado pelo Claude, mas instalação estava pendente. Não confundir com as seções antigas acima que ainda dizem NÃO testado.

Concluída a última autorização pendente: vk_outline_scale (padrão 0.5, CVAR_LATCH_GFX, efetivo limitado a 0.25..1). Normais/depth/framebuffer/viewport/scissor usam tamanho reduzido; composição usa o tamanho real da textura para offsets. 0.5 reduz a área desse passe a 25%, mas mantém redraw da geometria e composição fullscreen. Mudar requer vid_restart; vk_outline_scale 1 volta à resolução cheia. Não há FPS medido nem validação visual deste novo tamanho ainda.

Build Android debug final SUCCESSFUL. APK com assets Kenney e alteração de outline instalado via adb install -r no Xiaomi LJN7LFRKXG79P7J7, launcher relançado. Não alterada cfg do usuário, não tiradas screenshots. Sem commit/push/PR. Próximos testes: gl_outline 2/3 num mapa, comparar vk_outline_scale 0.5/1 + vid_restart e medir vk_profile 1; validar ícones no touch e MSAA/post-process com outline.

## Touch/chat e arte original — retomada 2026-10-09

Tiago esclareceu: priorizar imagens do próprio Quake encontradas em C:\unezquake; Kenney só preenche lacunas, nunca gerar arte do zero. src/in_touch.c agora prefere gfx/ui/ez_button_normal/pressed, textures/tracker/q no MENU, recorte do balão de textures/chaticons no CHAT e crosshairs/<crosshairimage> no FIRE. Fallback aos arquivos Kenney já existentes. Nenhum dado Quake adicionado ao repositório. PK3 e crosshairs/7.png existem no aparelho; PK3 do PC e celular têm tamanhos diferentes, portanto disponibilidade de cada imagem no celular ainda precisa de confirmação visual.

Chat: Tiago confirmou que tocar fisicamente no celular abre say: mas não o IME. Encontrada proteção de 10s após tecla física bloqueando também solicitações explícitas. CHAT/CONSOLE/KBD agora ignoram essa espera e pedem reabertura. Teste tests/touch-keyboard.ps1 compila as funções C reais em harness MSVC e verifica bloqueio físico, desbloqueio explícito, consumo único do nudge e ausência de IME fora de console/chat. Passou; build Android debug final passou; APK instalado e launcher relançado. Abertura real do IME permanece a confirmar por Tiago. Android show_ime_with_hard_keyboard observado como 0; não mudado globalmente.

Joystick combinado: Tiago perguntou sobre mover e olhar com um dedo. Apresentadas duas opções (vertical anda/horizontal gira, ou borda do stick gira); escolha ainda pendente, nada implementado.

iPhone: explicado que requer build/port iOS, Mac/Xcode/assinatura e MoltenVK; modelo de iPhone e acesso a Mac ainda desconhecidos. Não iniciado port iOS.

## Direcional de um dedo — 2026-10-09

Tiago confirmou que quer andar e olhar pelo direcional esquerdo. IN_TouchMove agora mantém frente/trás no eixo vertical e usa eixo horizontal para girar yaw, com cl_yawspeed e o mesmo passo de tempo de CL_AdjustAngles (cls.trueframetime ou physframetime conforme cl_independentPhysics). Este joystick não faz mais strafe; olhar vertical permanece pela área de look. Teste tests/touch-keyboard.ps1 ampliado para giro em 60/120Hz e física independente, avanço preservado e pausa: falhou com código antigo e passou com correção. Build Android debug passou; APK instalado e launcher relançado. Sensação real de controle ainda depende de validação pelo usuário.

L/R: usuário relatou possível falha, depois explicou que entrou em servidor sem armas. Mantidos impulse 10/12; não alegar bug confirmado nem troca validada nesse servidor. Nenhum commit/push.

## Direcional vertical também olha — 2026-10-09

Tiago pediu explicitamente olhar cima/baixo pelo mesmo direcional. Adicionado pitch ao eixo vertical em IN_TouchMove, junto do avanço/recuo existente, usando cl_pitchspeed e passo da física. Respeita minpitch/maxpitch e pausa; desativa pitch drift ao usar esse eixo. Empurrar para cima agora anda para frente E olha para cima simultaneamente; para baixo recua E olha para baixo. Teste vertical falhou antes e passou após correção; build Android debug passou. APK instalado e launcher relançado. Validação visual e sensação do acoplamento ainda pendentes. Sem commit/push.

## Sensibilidade do direcional — 2026-10-09

Tiago achou o giro impossível de controlar. Reduzido yaw/pitch do stick para 20% do anterior, multiplicado pelo in_touch_look_sens existente (ajustável no menu). Movimento frente/trás não mudou. No padrão sens=1 e cl_yawspeed=140/cl_pitchspeed=150: 28/30 graus/s em deflexão máxima. Testes verificam taxas em 60/120Hz, limites pitch, física independente e sens=0.5; passaram após falha comprovada no código anterior. Build Android debug passou, APK instalado e launcher relançado. Sensação no aparelho ainda a validar. Sem commit/push.

## Direcional: câmera por arrasto — 2026-10-09, estado atual

Tiago continuou achando impossível mesmo com 20% da velocidade. Removido giro contínuo por deflexão (yaw e pitch) e seus externs de passo de física. Movimento de ROLE_STICK agora acumula look_dx/look_dy como ROLE_LOOK; câmera só acompanha deslocamento do dedo usando caminho existente e in_touch_look_sens. Dedo parado não gira/inclina mais, mas mantém avanço/recuo pelo stick_y; strafe continua ausente no stick. Este comportamento substitui as duas seções anteriores de velocidade contínua. Teste reproduziu giro involuntário com eixo segurado no código anterior e passou após remoção; também verifica consumo único de arrasto, sensibilidade, pausa e limites de pitch. Build Android debug passou, APK instalado/relançado. Ergonomia com um dedo ainda depende de Tiago. Sem commit/push.

## Botões visuais console/teclado — 2026-10-09

B_KBD fica visível em todos os destinos quando touch está ativo. No jogo/menu abre toggleconsole e solicita IME; no console/chat solicita reabertura sem fechar a entrada. Durante gameplay fica no topo à esquerda de TAB; fora dele no topo direito. B_CONSOLE também aparece quando key_console, ao lado de KBD, com icon_arrow_curved existente para recolher console via toggleconsole. Nenhuma arte criada. Testes de comando, visibilidade e solicitação de texto falharam antes e passaram depois; build Android debug passou; APK instalado/relançado. Abertura real do IME e usabilidade visual ainda dependem do aparelho/usuário. Sem commit/push.

## Controle touch final separado + FIRE arrastável — 2026-10-09

Depois de testar o controle combinado, Tiago apontou conflito entre andar e olhar verticalmente e perguntou como fazer ficar fácil como WASD+mouse. Recomendado/restaurado stick esquerdo só movimento (frente/trás e strafe); área direita só mira, sem look acumulado no ROLE_STICK. Tiago perguntou como mirar e apertar tiro com o mesmo polegar: ROLE_BUTTON/B_FIRE agora acumula look_dx/dy ao arrastar mantendo +attack desde DOWN até UP/CANCEL, mesmo ao sair da área do botão. Outros botões não movimentam mira. Nenhuma arte/novo modo/cvar adicionado.

L/R corrigidos de impulse 10/12 para weapon 10/12, os mesmos comandos do menu de binds do cliente. Config do PC tem cl_weaponpreselect 4, portanto em deathmatch a escolha pode só aplicar ao atirar; preservado.

Tests/touch-keyboard.ps1 executa funções C reais também do finger loop, com stubs de plataforma/seleção de botão: verifica FIRE down/motion/up, mirar durante tiro, soltar ataque, JUMP não mirar, stick não mirar e soltar movimento. Também verifica os comandos weapon 10/12. Todos passaram; build Android debug passou; APK instalado e relançado. Uso real e ergonomia ainda dependem de teste do usuário. Este estado substitui todas as experiências de stick combinado anteriores. Sem commit/push.

## Tiro ao tocar a área de mira — 2026-10-09

Pedido de Tiago: não precisar acertar FIRE, tocar tela para atirar. ROLE_LOOK (toque na área livre direita, x>=0.42; botões têm prioridade) inicia +attack no DOWN, arrasto mira pelo caminho existente e UP/CANCEL libera ataque. ROLE_STICK/esquerda não atira. IN_TouchReleaseAll também libera ataque de ROLE_LOOK. Contagem attack_fingers evita soltar ataque prematuramente se FIRE e área livre estiverem pressionados simultaneamente. Teste de toque livre falhou antes e passou depois; testa mira, UP, limpeza geral e dois dedos com cancelamento. Build Android debug passou; APK instalado/relançado. Agora tocar a área direita para apenas mirar também atira: comportamento solicitado, confirmar ergonomia no celular. Sem commit/push.

## Console inicial/IME e toque duplo — 2026-10-09

IME investigado com logs temporários C/Java e ImeTracker. Em solicitações explícitas, registrado want=1/dest=2/touch=1, SDL_StartTextInput=true, editor focus/window/accepting=true e ImeTracker onShown. Falha inicial encontrada: console full-screen de SCR_NEED_CONSOLE_BACKGROUND pode estar visível com key_dest=key_game. Novo IN_TouchConsoleActive compartilha essa detecção com wants-IME, GameDest, filtro de SDL_TEXTINPUT e Key_Event_TextInput, para solicitar teclado E aceitar letras nesse console. last_physical_key começa em -11 para não suprimir IME nos primeiros 10s sem tecla física. Caminho desktop preservado por macro que retorna key_dest==key_console. Teste console inicial falhou antes e passou depois. Todos os IME_DEBUG removidos, SDLActivity.java restaurado após diagnóstico.

Tiago pediu distinguir mira de tiro: toque simples/arrasto livre só mira. Primeiro toque curto (<=250ms, movimento dentro de UnitSize) registra candidato; segundo toque em até300ms na mesma região inicia tiro no DOWN e permite arrastar mirando, até UP/CANCEL. Limpeza de touch apaga candidato e libera ataque. FIRE continua pressionar/arrastar para tiro direto. Testes verificam toque simples, arrasto, toque duplo, atraso>300ms, região distante, cancelamento e limpeza. Build Android debug passou; APK instalado/relançado. Ergonomia e teclado inicial real ainda a validar no aparelho. Sem commit/push.

## IME em in_touch 2 confirmado no aparelho — 2026-10-09

Tiago reiterou teclado ausente no console inicial em modo auto. Removida desativação incondicional por SDL_HasKeyboard && SDL_HasMouse em IN_TouchFrame: auto passa a seguir eventos físicos/touch, conforme intenção já documentada. Teste com ambos periféricos reportados reproduziu active=false antes e passa após mudança.

Investigação temporária adicional mostrou delay de startup: antes do primeiro HandleEvents touch permanece false; primeiro frame útil aproximadamente9s após início. Depois de completar inicialização, rastreado mode=2/auto=1/want=1/key_dest=console e Android ImeTracker onShown. dumpsys input_method confirmou mInputShown=true, SDLDummyEdit focado com SDLInputConnection. As verificações anteriores de teclado oculto foram feitas cedo demais, ainda durante startup; não eram prova de falha depois do carregamento. Logs IME_TRACE removidos; build/testes finais passaram; APK final reinstalado/relançado. Sem alterar cfg, sem commit/push.

## Release 0.1 / f_version (2026-10-10) — código pronto, NÃO compilado ainda

- `VERSION` (0.1.0) é a fonte única: Gradle (versionName/versionCode), CMake (`NEXTQUAKE_VERSION`), `scripts/release.ps1`, `CHANGELOG.md`. Fluxo em `docs/RELEASING.md`.
- `f_version` agora: `nextQuake 0.1.0 (ezQuake 3.7.0-dev) Android 14:Vulkan` — nextQuake laranja (`&cf80`), versão verde (`&c0f0`), plataforma azul claro no Android. `QW_PLATFORM` = Android/iOS (antes `Linux32` porque `__linux__` também é definido no Android), `QW_RENDERER` = Vulkan no mobile, `QW_PlatformDetail()` lê `ro.build.version.release`. iOS: stub "iOS".
- Update check aponta para `tibazera/nextQuake` releases (tag `v0.1.0`). Repo privado => falha em silêncio, sem banner.
- Arquivos: `src/version.h`, `src/version.c`, `src/fchecks.c`, `src/menu_ingame.c`, `CMakeLists.txt`, `app/build.gradle.kts`, `scripts/release.ps1`, `CHANGELOG.md`, `docs/RELEASING.md`, `.gitignore` (+`/release/`).
- Falta: `git submodule update --init --recursive`, build Debug/Release no nextQuake, conferir `f_version` num servidor, keystore de release real, commit/tag (só com pedido do Tiago).

## unezQuake 2.1 → nextQuake (2026-10-10)

Upstream 2.1 (dusty-qw/unezquake, publicado 2026-10-09; 39 commits desde 2.0.5). O que NÃO se aplica direto: o nextQuake deriva do ezQuake (QW-Group), não do unezQuake — não tem `cl_ezcsqc.c`, `cl_session.c` nem o stack de antilag/prediction (as correções de EZCSQC/LG/sound prediction ficam de fora).

**Portado (sem build ainda):** (1) correção de **buffer overflow remoto no parse de infostring** (`src/common.c`: `Info_ValueForKey`, `Info_RemoveKey`, `Info_Print`; também `Info_RemovePrefixedKeys`, que tem o mesmo padrão e o upstream não corrigiu); (2) serverinfo de 1024 bytes (`MAX_SERVERINFO_STRING` 512→1024 em `common.h`, `argv_buf` 1024→2048 em `cmd.h`); (3) `cvaredit` copiando o NUL final (`cvar.c`).

**Pendente / decisão:** novo menu (Quick Play, servidores, demos, busca de settings) é **Dear ImGui com backends OpenGL2/3 + SDL2** (`menu_imgui.cpp` 2294 linhas, `menu_ui_bridge.c`, `menu_scene.c`, ~69k linhas de ImGui no total). Não roda no renderer Vulkan: exigiria backends ImGui Vulkan+SDL3, escala/touch e mapa de fundo; é um projeto, não um cherry-pick. Sessions (multi-servidor, janela/processo desktop) não faz sentido no mobile agora. `cl_delay_packet` → CL_PING e Shift+Esc abrem console: não portados (CL_PING mexe em net.c, sem benefício mobile claro; Shift+Esc é trivial se quiser).

## Config padrão embutida + correções do dia (2026-10-10)

- **Config padrão**: `app/src/main/assets/defaults/config.cfg` (gerada da config do Tiago sem `name/team/skin/cores/spectator`, sem `vid_*`/janela/mouse de desktop e sem senhas/tokens; 4032 linhas, 1708 `hud_*`, 62 binds). O launcher (`installDefaultConfigIfMissing`) copia para `Documents/ezQuake/ezquake/configs/config.cfg` **só se não existir**. Motivo: com a config padrão do cliente o modo observar travava; com a config do Tiago ficou normal. Build OK; cópia em instalação limpa **ainda não testada** (o celular já tem a config).
- **Nomes acima da cabeça (autoid)** deslocados: `R_Project3DCoordinates` usava a projeção pré-rotacionada do Android; corrigido com `R_RemoveAndroidPreRotation` em `src/r_matrix.c`. Confirmado pelo Tiago.
- **Console com teclado**: `in_touch_console_size` (0.4) encolhe o console enquanto o IME está aberto.
- **Launcher**: detecta pacote HD já instalado e grava marcador `ezquake/.hd_textures_prompted`.
- **Ports do Codex revertidos** (screenshot/formato swapchain/guards cvar/sky/mouse subpixel/mip clamp): `E:\tmp\nq-codex-ports.patch` guarda o patch. Não eram a causa do travamento. Investigação do Codex sobre pose2 do alias (`nq-spectator-fix.patch`, não aplicado): Vulkan descarta `pose2`, possível melhoria futura, não confirmada como causa.
- **Do unezQuake 2.1 já aplicado**: overflow de infostring (`common.c`), serverinfo 1024/argv_buf 2048, cvaredit. Menu novo (ImGui GL/SDL2) = projeto futuro.
- Para builds do nextQuake usar o junction `C:\nq` (caminho sem espaço; `speex` quebra com espaço). Linha de base antiga: `C:\nqbase` (worktree detached de db779096, `-DGIT_SUBMODULE=OFF`).

## Menu Overhaul (Dear ImGui do unezQuake 2.1) portado para Vulkan/SDL3 (2026-10-10) — instalado, NÃO validado visualmente

- Fonte upstream de referência (somente leitura, fora do git): `_upstream_unezquake/` (tag 2.1, em `.git/info/exclude`). ImGui v1.92.9b: núcleo igual ao vendored do unezQuake + backends oficiais SDL3/Vulkan (`src/imgui/backends`), com patch próprio em `imgui_impl_vulkan.cpp` (`ImGui_ImplVulkan_SetPreRotation`: shader/push constants/viewport/scissor girados para a pré-rotação do swapchain Android; vertex shader `src/vulkan_shaders/vk_imgui.vert`).
- Arquivos novos: `src/menu_imgui.cpp` (portado de GL/SDL2 para Vulkan/SDL3; "Quit unezQuake?" -> nextQuake; copyright da equipe unezQuake preservado), `menu_ui_bridge.{c,h}` (adaptado pelo Codex ao nosso motor: settings pages reimplementadas, sem sessões, textura de preview via `VK_ImGui_TextureID`), `menu_scene.{c,h}` (STUB: mapa de fundo não portado), `vk_imgui.cpp` + `vk_imgui_bridge.h` (backend), `vk_imgui_glue.c` (handles Vulkan, cache de texturas), `vk_texture.c: VK_TextureImageView`.
- Ganchos: `menu.h/menu.c` (estado `m_imgui`, `menu_quickplay`, M_ImGui_*), `cl_screen.c` (`M_ImGui_Frame` depois do pós-processamento), `cl_main.c` (`M_ClassicMenus_Init` antes de `VID_Init`), `vid_sdl2.c` (`MenuUI_ProcessEvent` em todo evento, toque -> `MUIVK_TouchPointer`, VidInit/VidShutdown junto do Vulkan), `in_touch.c` (overlay antigo some com o menu aberto; IME abre em campo de texto via `MenuUI_WantsTextInput`).
- cvars: `menu_classic 1` volta ao menu clássico; `menu_scale`; `menu_background` (sem efeito, cena é stub).
- Lacunas conhecidas: fundo (mapa) não portado; botão QTV desligado (`qtvlist_has_stream` não existe aqui); rolagem por arrasto e Back do Android no menu ainda não verificados; nada validado no aparelho.

## Fundo do menu com mapa portado + APKs (2026-10-10, 06:05)

- `src/menu_scene.c` agora e o real (unezQuake 2.1), nao mais stub. Ganchos: `screen.h` (SCR_NEED_CONSOLE_BACKGROUND), `cl_view.c` (V_PreRenderView), `cl_screen.c` (MenuScene_Frame / MenuScene_Draw2D), `cl_main.c` (MenuScene_Invalidate em CL_ClearState e CL_Disconnect), `r_model.c` (Mod_ReloadModelsTextures).
- Build via `C:\nq` com JDK17, ANDROID_HOME C:\Android\Sdk, glslangValidator de C:\VulkanSDK\1.4.350.0\Bin no PATH: Debug e Release BUILD SUCCESSFUL. Debug app-debug.apk 30.683.741 B; Release app-release.apk 17.725.278 B, SHA256 4841208DE3554038156A26352A730BDB04DB27F77819F438C6B2455374C3F094 (chave de teste).
- NAO validado em aparelho (nenhum conectado por adb). Revisao Codex falhou (sem acesso ao diretorio); revisao manual de vk_world/r_rmain nao achou gate por cls.state.
- Patches soltos: nq-menu-bridge.patch ja aplicado (pode apagar); q-spectator-fix.patch nao aplica e foi rejeitado.
- Sem commit/push.

Correcoes da revisao Codex (menu_scene.c): estaticas (portas/itens) agora sao enfileiradas e ligadas DEPOIS de R_NewMap (que zera efrags); cena sem camera valida falha em vez de ficar ativa sem desenhar. Debug/Release recompilados: app-debug.apk 30.684.013 B; app-release.apk 17.725.502 B, SHA256 7FE960C89EC875ECD88EB5F8F30263BEAA6307A17B37BC1E757A226FEA91F6DE. Ainda nao validado em aparelho.

## Console ImGui (2026-10-10) — compila, NAO instalado/validado
- Nova pagina `MUI_PAGE_CONSOLE` no menu ImGui (botao CONSOLE da barra): scrollback lido de `con.text/clr` (monoespacado, cores, 500 linhas, arrasto rola), entrada com IME, Send/Tab/Clear/Close, historico (setas) e Tab completion via `Key_Console`, entrada passa por `Key_Console(K_ENTER)` (mesma logica de chat/comando do console classico). Encolhe para a metade superior quando o teclado esta aberto.
- Console classico (`toggleconsole`, backtick, botoes CON/KBD do overlay) NAO foi trocado; decidir com o Tiago.
- Quick Play: toque seleciona card (Play/Spectate fixos). Browser: timeouts curl, CAPATH Android, limite MAX_SERVERS, logs `nq-sb`.
