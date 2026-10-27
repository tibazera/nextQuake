# Session Notes — Android/Vulkan port checkpoint

## Objetivo atual
Portar e validar o renderer Vulkan de ezQuake na branch `feature/android-pocket-vulkan`,
com paridade funcional frente a GLC/GLM (gamma, contraste, FXAA, MSAA, timerefresh),
e auditar o menu System (`settsystem_arr` em `src/menu_options.c`) quanto a itens
ainda não verificados sob Vulkan.

## Resumo do que foi feito nesta sessão
- Corrigido bug de tela branca real: `VK_BrightenScreen` usava blendfunc errado
  (`r_blendfunc_additive_blending`) em vez do equivalente GLC/GLM
  (`r_blendfunc_src_dst_color_dest_one`, `dst*(1+src)`). Com `gl_contrast >= 2`
  o blend additivo saturava a tela inteira para branco. Corrigido e confirmado
  pelo usuário ("arrumou.").
- Corrigido gating de pós-processamento: `VK_PostProcessActive()` e
  `VK_PostProcessComposite()` agora só aplicam a curva real de gamma/contraste via
  shader quando `vid_software_palette` está ativo, igual ao
  `GLM_CompilePostProcessProgram()` (`POST_PROCESS_PALETTE`). Sem isso, gamma/contraste
  do usuário (pensados para a gamma ramp de hardware) eram aplicados como curva de
  shader, podendo lavar a imagem para branco mesmo sem o bug do BrightenScreen.
- Corrigido default de `vid_renderer`: builds com `EZ_MULTIPLE_RENDERERS` +
  `RENDERER_OPTION_VULKAN` priorizavam OpenGL moderno (1) no `#if/#elif`. Reordenado
  para Vulkan (2) ter prioridade. Não era a causa do bug do usuário (cfg já fixava
  `vid_renderer 2`), mas é correto para configs novas/limpas.
- Commit `65d5557d` feito na branch atual com as mudanças relacionadas (post-process
  real, timerefresh, fix do contraste). 10 arquivos, 826 inserções, 11 remoções.

## Arquivos alterados (commit 65d5557d)
- `AGENTS.md` — histórico/roadmap atualizado
- `CMakeLists.txt`
- `src/vid_sdl2.c` — default de `vid_renderer`
- `src/vk_draw.c` — `VK_PostProcessComposite`, fix do blendfunc em `VK_BrightenScreen`
- `src/vk_local.h`
- `src/vk_main.c` — wiring do post-process em `VK_BeginFrame`
- `src/vk_renderpass.c`
- `src/vk_swapchain.c` — `VK_PostProcessActive`, criação/destruição de recursos
- `src/vulkan_shaders/vk_post_process.frag` / `.vert`

## Arquivos importantes para continuar
- `src/menu_options.c` (linhas ~1298-1384, `setting settsystem_arr[]`) — lista
  definitiva de todos os itens da aba System do menu, base da auditoria pendente.
- `src/vk_draw.c` — `VK_BrightenScreen`, `VK_PostProcessComposite`.
- `src/vk_swapchain.c` — `VK_PostProcessActive`, `VK_CreatePostProcessResources`.
- `src/glc_misc.c` / `src/r_states.c` — referência GLC para `r_state_brighten_screen`
  (ground truth usado para achar o bug do blendfunc).
- `src/glm_framebuffer.c` — referência do gating `POST_PROCESS_PALETTE`.
- `AGENTS.md` — memória operacional compartilhada com Codex; item 6 (post-process,
  já atualizado) e item 8 (itens do System tab ainda não auditados sob Vulkan).

## Decisões técnicas tomadas
- Gating de gamma/contraste via shader deve espelhar exatamente o flag
  `vid_software_palette` do caminho GLM, não os valores brutos de `v_gamma`/`v_contrast`.
- `VK_BrightenScreen` é independente do pipeline de post-process novo (já existia,
  commit `95d24f48`, mesmo dia) — blendfunc precisa bater com `r_state_brighten_screen`
  do GLC (`dst*(1+src)`), não additive flat.
- Diagnóstico temporário (`Com_Printf` em `VK_BeginFrame`) foi usado e depois
  removido, conforme regra do projeto de não deixar instrumentação temporária.

## Comandos que funcionaram
- `git log --oneline -5`, `git diff --stat HEAD~1 HEAD`, `git status --short`
- Build/teste manual do `.exe` em `C:\ezquake` com a `.cfg` real do usuário e
  `-condebug` para inspecionar `qconsole.log`.

## Comandos que falharam / não executados ainda
- Build Android debug (`_build_android_debug.bat`) — **ainda não executado nesta
  sessão**, pendente conforme regra do AGENTS.md de rodar ao menos o build Android
  debug após mudanças nativas/CMake.

## Erro atual, se ainda existir
Nenhum bug aberto confirmado. Pendente apenas auditoria (não há erro conhecido):
itens do menu System ainda não verificados especificamente sob Vulkan:
- Resolução/Bit Depth/Fullscreen/Vsync (handlers customizados em `menu_options.c`)
- Connection (bandwidth, early packets, packetloss, QTV buffer) — provavelmente
  agnóstico de renderer, não confirmado lendo código Vulkan
- Sound & Volume, Font — provavelmente agnósticos de renderer

## Próximo passo exato
1. Responder à pergunta do usuário sobre quais itens da aba System foram
   confirmados/corrigidos nesta sessão (gamma, contraste, FXAA, BrightenScreen) vs.
   quais seguem não auditados (resolução, bit depth, fullscreen, vsync, conexão).
2. Rodar `_build_android_debug.bat` para validar as mudanças nativas no target
   Android, conforme regra pendente do AGENTS.md.
