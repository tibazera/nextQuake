# Agente do projeto ezQuake Pocket Vulkan

## Retomada mais recente — 2026-10-09, Codex

Antes de retomar, leia a seção **Estado atual para Claude/Codex** no início de `continue.md` e a atualização mais recente de `docs/android-pocket-vulkan-handoff.md`. Elas descrevem o controle touch final e substituem as experiências intermediárias de movimento/mira e as notas antigas de funcionalidades não testadas.

Não edite este worktree simultaneamente com outro agente/assistente. Nesta sessão houve sobreposição de edições do Claude que duplicou `vk_outline_scale`; o estado final tem uma definição latched, com padrão 0.5 e aplicação após `vid_restart`. Preserve o touch e as correções de IME já validados; não restaure patches antigos por cima do worktree.

Tiago autorizou nesta sessão um commit local do estado recuperado. Essa autorização não inclui push/PR nem commits automáticos em sessões futuras.

Este arquivo é a memória operacional compartilhada entre Codex, Claude e futuros colaboradores. Leia-o por inteiro antes de alterar qualquer arquivo. Leia também `docs/android-pocket-vulkan-handoff.md`.

## Regra mais importante

O diretório local `E:\Projetos Linux\ezquake-source` é a fonte de verdade mais fiel do projeto. O worktree pode conter avanços ainda não commitados e, nesse caso, ele é mais atual que o branch remoto, o histórico Git ou qualquer PR.

Antes de trabalhar:

1. Confirme que o diretório atual é `E:\Projetos Linux\ezquake-source`.
2. Execute `git status --short`, `git branch --show-current` e examine os diffs locais.
3. Preserve toda mudança local que não pertença claramente à tarefa atual.
4. Nunca use `git reset --hard`, `git checkout --`, `git clean` ou equivalentes para apagar o worktree.
5. Não faça commit, push, force-push nem abra/atualize PR automaticamente. Faça isso somente quando Tiago pedir naquele momento.

`E:\Projetos Linux\unezquakepocket` não é o código-fonte. Essa pasta é usada apenas para receber APKs de teste quando solicitado. Não implemente mudanças nela.

## Objetivo e limites atuais

O projeto usa o `qw-group/ezquake-source` canônico como base (via o fork `tibazera/ezquake-source`) e busca um cliente QuakeWorld Android arm64 moderno com SDL3, Vulkan, controles utilizáveis e desempenho adequado em smartphones topo de linha. Não é baseado na linhagem `dusty-qw/unezquake` — essa comparação foi descartada cedo no projeto (ver decisão registrada em commits/handoff antigos); `unezquakepocket`, citado em outros pontos deste arquivo, é só o nome da pasta local de entrega de APK, não uma base de código.

O branch de desenvolvimento é `feature/android-pocket-vulkan` no repositório `tibazera/ezquake-source`. Os commits servem para preparar, mais tarde, um PR completo e limpo para o grupo `qw-group`. Esse PR upstream ainda não é o foco. Não ofereça, publique ou envie mudanças ao upstream sem autorização explícita.

Não substitua a base por FTEQW. FTEQW e vkQuake são referências para soluções pontuais de Android, Vulkan e desempenho. Importe ideias apenas depois de medir o gargalo e adaptar a solução à arquitetura do ezQuake.

## Estado técnico alcançado

- Build Android arm64 com SDL 3.4, Vulkan e Oboe.
- Bootstrap Java atualizado para os arquivos oficiais do SDL 3.4.
- `EzQuakeActivity.getMainFunction()` retorna `main` e o `main` nativo Android possui visibilidade exportada.
- APIs de janela, eventos, entrada e superfície Vulkan foram adaptadas para SDL3.
- Retornos booleanos do SDL3 foram corrigidos em inicialização, mutexes, semáforos e criação de superfície.
- Captura VOIP foi desativada no Android; reprodução continua pelo backend Oboe.
- Atualizações dinâmicas de lightmaps são enfileiradas e copiadas por um staging buffer persistente no command buffer do frame seguinte.
- O caminho normal de atualização de lightmaps não cria vários command buffers imediatos nem executa `vkQueueWaitIdle` por atualização.
- O loop Vulkan permite dois frames em voo. Cada frame possui semáforos, fence e staging de lightmap próprios; cada imagem do swapchain rastreia seu último fence.
- A superfície de render Android é limitada ao envelope 1920x1080, preservando a proporção física e sem fazer upscale. No Xiaomi 2712x1220, o buffer validado é 1920x864 e o SurfaceView é escalado pelo compositor para preencher a tela.
- `SDLActivity.mLayout.addView(mSurface, ...)` agora passa `RelativeLayout.LayoutParams(MATCH_PARENT, MATCH_PARENT)` explícito. Sem isso, o `RelativeLayout` usava `WRAP_CONTENT` por padrão e, depois do `setFixedSize()` do item acima, o `SurfaceView` media seu próprio tamanho preferido como o buffer fixo (1920x864) em vez de preencher a tela — confirmado via `dumpsys SurfaceFlinger` (bounds da layer do SurfaceView = 1920x864 contra os 2712x1220 da Activity) e visualmente (área preta cobrindo o resto da tela). Corrigido; confirmado visualmente no Xiaomi que a tela volta a preencher por completo.
- dm3, dm4 e dm6 voltaram a carregar com texturas depois de isolar um conflito de conteúdo PK3 no aparelho.
- `f_system` agora reporta corretamente o aparelho Android. `SYSINFO_Init()` tinha um branch específico pra `__linux__` que também capturava Android (pois `__ANDROID__` implica `__linux__`), usando `/proc/cpuinfo`/`gl_renderer` que não fazem sentido no port Vulkan. Adicionado um branch `#elif defined(__ANDROID__)` antes do `__linux__` em `src/host.c`, usando `__system_property_get` (`ro.product.manufacturer/model`, `ro.soc.manufacturer/model`, `ro.board.platform`) e `sysconf(_SC_PHYS_PAGES/_SC_PAGE_SIZE)` pra memória. `VK_DescriptiveString()` em `vk_main.c` retornava a string fixa `"Vulkan"` em vez do nome real da GPU; agora retorna `vk_options.physicalDeviceProperties.deviceName`. Confirmado no aparelho que `f_system` mostra o nome do celular corretamente. Pendente: a colorização de `f_system`/`f_version` por fabricante de CPU/GPU (Intel/AMD/NVIDIA) não tem regras pra Qualcomm/ARM/Mali/Adreno — baixa prioridade, deixado para depois do trabalho de performance.
- **Causa raiz do teto de ~30fps, encontrada e corrigida**: `vkQueuePresentKHR` retornando `VK_SUBOPTIMAL_KHR` (esperado e permanente no nosso caso, já que renderizamos de propósito num buffer menor que a tela — ver item do envelope 1920x1080 acima) disparava `VK_RequestSwapChainRecreate()` em `VK_EndFrame()` (`vk_main.c`), e isso destruía e recriava a swapchain inteira — com `vkDeviceWaitIdle` completo — em **todo frame**, não só na troca real de resolução/orientação. Só ficou visível depois de instrumentar `VK_CreateSwapChain` com um log de criação e capturar uma sessão de jogo real: a mensagem repetia a cada ~30ms, no ritmo exato do frame. Corrigido para só recriar em `VK_ERROR_OUT_OF_DATE_KHR` (erro real); `SUBOPTIMAL_KHR` agora só marca o frame como apresentado normalmente. Confirmado no aparelho: FPS dobrou de ~30 para ~65 imediatamente após a correção. Esse era o gargalo real, não os draw calls nem o overhead de CPU por draw que tínhamos cogitado antes de medir.
- Bug relacionado encontrado e corrigido na mesma investigação: em `VK_BeginFrame()`, quando `VK_RecreateSwapChain()`/`VK_RecreateSurfaceAndSwapChain()` falhava, a função retornava sem nunca resetar a flag `vk_recreate_swapchain_requested`/`vk_recreate_surface_requested` — isso podia travar o app de vez (loop infinito de tentativas de recriação, nunca chegando a desenhar um frame). Corrigido resetando a flag antes da tentativa de recriação, não depois do sucesso, então uma falha real ainda permite tentar de novo no frame seguinte sem virar busy-loop permanente.
- Upload de textura e lightmap no carregamento de mapa agora passa pela mesma fila em lote (`pendingTextureUploads[]` em `vk_texture.c`), flushada uma vez por frame dentro de `VK_BeginFrame`. Antes, `VK_UploadTexture` fazia até 3 ciclos `vkQueueWaitIdle` por textura, e o carregamento de mapas grandes (dm3-scale, ~420 texturas + ~490 lightmaps) ficava na casa de milhares de GPU drains síncronos — o stall de ~12s no load de mapa media exatamente isso. `VK_TextureFlushPendingUploads` foi corrigido para ler/atualizar `vktex->layout` dinamicamente (via `VK_TextureRecordTransitionBarrier`) em vez de assumir `SHADER_READ_ONLY_OPTIMAL` fixo, o que também tornou correto enfileirar texturas recém-criadas (layout real `UNDEFINED`), não só lightmaps já existentes.
- Instrumentação temporária de profiling (`VK_PROFILE`, tag de logcat) foi adicionada em `vk_main.c`, `vk_world.c`, `vk_texture.c` e `vk_swapchain.c`: intervalo de frame, espera de fence GPU, `vkAcquireNextImageKHR`, espera de imagem em voo, `vkQueueSubmit`, `vkQueuePresentKHR`, tempo de gravação de comandos, draws do mundo, uploads de lightmap, modo de present escolhido. Foi essa instrumentação que revelou os dois bugs de swapchain acima. Remover antes de considerar o trabalho de performance concluído (regra geral: não deixar log de debug temporário na versão final).
- Repositórios de referência vkQuake e FTEQW foram clonados (shallow, só leitura) em `E:\Projetos Linux\_research\{vkquake,fteqw}`, fora deste repositório, para comparar arquitetura de sincronização/batching/threading na investigação de performance.
- **Build Windows com Vulkan validado e regressão corrigida.** `vcpkg.json` precisa da feature `vulkan` para `sdl3` também em `windows, osx` (antes só Android tinha), senão o SDL3 compilado não tem `SDL_VIDEO_VULKAN` e `SDL_CreateWindow(..., SDL_WINDOW_VULKAN)` falha com "Vulkan support is either not configured...". Corrigido um bug real e separado em `src/vid_sdl2.c`: `VID_RENDERER_MIN`/`VID_RENDERER_MAX` (linha ~99) estavam fixos em `0`/`1`, nunca atualizados quando o Vulkan se tornou `vid_renderer` valor `2` (`r_main.c`'s `renderer_options[]`). Isso fazia `VID_SDL_GL_SetupContextAttributes()` tratar `vid_renderer 2` como inválido e tentar criar contexto OpenGL numa janela `SDL_WINDOW_VULKAN`, falhando sempre com "The specified window isn't an OpenGL window". Corrigido trocando `VID_RENDERER_MAX` para `2`. Confirmado no Windows desktop (AMD Radeon RX 6800 XT): `qconsole.log` mostra "Vulkan initialised successfully! Device 0: AMD Radeon RX 6800 XT" com `+set vid_renderer 2`. Build/run: `cmd.exe /c "_build_vulkan.bat"` (script auxiliar na raiz do repo, fora do CMakePresets.json, porque o preset `msvc-x64` não passa pelo ambiente do Visual Studio automaticamente) seguido de `ezquake.exe -dev -condebug +set vid_renderer 2`. Lembrar: `vid_renderer` é `CVAR_LATCH_GFX`, mas o range-check de sanidade roda antes de qualquer latch ser necessário — o bug nunca era sobre latching.
- **Buffers dinâmicos corrigidos para os dois frames em voo.** Cada `r_buffer_id` agora possui uma cópia por `currentFrame`; update, consulta e binding selecionam o mesmo slot do fence/semaphore do frame. Antes, o CPU podia sobrescrever geometria ainda lida pelo GPU. Resize/recriação continua deliberadamente síncrono com `vkDeviceWaitIdle` até existir aposentadoria diferida de buffers; não remover essa espera isoladamente.
- **Aquisição e sincronização não abandonam mais recursos Vulkan.** Depois de `vkAcquireNextImageKHR` ter sucesso, a espera do fence da imagem não usa mais timeout recuperável: retornar ali deixava a imagem adquirida e o semáforo binário sinalizado para reutilização inválida. Falhas de wait/reset/begin/end/submit agora encerram pelo limite fatal do renderer em vez de continuar com fence não sinalizado. `VK_ERROR_SURFACE_LOST_KHR` solicita recriação da superfície, enquanto `OUT_OF_DATE` continua restrito à swapchain.
- **Crash 100% reproduzível ao trocar `vid_renderer` saindo do Vulkan (ex.: 2→1), corrigido.** `R_Shutdown()` em `src/r_main.c` chamava `renderer.Shutdown(mode)` (que pra Vulkan destrói `VkDevice`/`VkInstance` e zera `vk_options` na hora) **antes** de `renderer.DeleteVAOs()`/`buffers.Shutdown()` — então `VK_DeleteVAOs`/`VK_BufferShutdown` chamavam `vkDestroyBuffer` etc. com o `VkDevice` já nulo, e o loader (`vulkan-1.dll`) crashava com fail-fast (`0xc0000409`) ao tentar resolver a dispatch table de um handle nulo. Confirmado via Visor de Eventos do Windows: mesmo módulo, mesmo offset, repetido em todo `vid_restart` saindo do Vulkan. Corrigido invertendo a ordem: VAOs/buffers agora são destruídos **antes** de `renderer.Shutdown(mode)`, enquanto o device ainda está vivo — sem efeito no GL, já que o contexto GL só morre bem depois, em `VID_Shutdown()`. Também adicionada checagem defensiva em `VK_BufferDestroyCopies` (`vk_buffers.c`) contra `vk_options.logicalDevice == VK_NULL_HANDLE`.
- **PR limpo de SDL3+Vulkan desktop preparado, sem Android/vkQuake/FTEQW, em worktree separado.** A pedido de Tiago, existe um worktree dedicado em `E:\Projetos Linux\ezquake-vulkan-pr` (branch `feature/sdl3-vulkan-pr`, baseado em `upstream/master`) contendo só a migração SDL3 desktop + backend Vulkan completo (sem nada de Android/Oboe/JNI, sem nenhuma menção a vkQuake/FTEQW em código, comentário, commit ou doc — verificado com grep no diff inteiro contra `upstream/master`). O Android fica para um PR separado, posterior. Build dos três renderizadores (Classic GL + Modern GL + Vulkan) validado do zero nesse worktree.
  - **Pegadinha de build descoberta**: o autotools/libtool usado por `speex`/`speexdsp` no vcpkg quebra com espaço no caminho ("Libtool does not cope well with whitespace in `pwd`" → "C compiler cannot create executables", já que o `cl.exe` recebe `-LIBPATH:E:/Projetos\ Linux/...` mal escapado e separa em dois argumentos). Isso afeta qualquer worktree sob `E:\Projetos Linux\...`; nosso build principal só não sofre porque o `vcpkg_installed` dele já estava populado de antes. Contorno usado: criar um *directory junction* sem espaço (`mklink /J C:\ezq-pr "E:\Projetos Linux\ezquake-vulkan-pr"`, não precisa admin) e buildar a partir dele — mesmos arquivos, caminho diferente. Não renomeie as pastas reais do usuário pra resolver isso.
  - Build da PR também precisa do mesmo `VULKAN_SDK=C:\VulkanSDK\1.4.350.0` e do Vulkan SDK `Bin` no `PATH` que `_configure_vulkan.bat` já configura no worktree principal — sem isso, `find_package(Vulkan REQUIRED)` falha mesmo com o SDK instalado.
  - Durante a auditoria desse worktree foram encontradas e corrigidas duas regressões introduzidas pela remoção mecânica de código Android: (1) `src/host.c` tinha voltado pra `if (SDL_Init(0) != 0)` (semântica SDL2; precisa ser `if (!SDL_Init(0))` em SDL3); (2) `src/snd_voip.c` tinha o único bloco `#ifndef __ANDROID__` (condição invertida) do projeto, e a remoção mecânica apagou a declaração de `s_inputdevice` junto com a guarda em vez de só remover a guarda — quebrou a compilação. Ao revisar remoção de código Android no futuro, prestar atenção especial em `#ifndef __ANDROID__` (raro, fácil de inverter por engano) e em qualquer checagem de retorno de API SDL2→SDL3 que esteja perto do código removido.
- **PR validado de verdade nos 3 SOs via CI real do GitHub, antes de abrir o PR.** Em vez de confiar em "Windows compilou, deve compilar igual nos outros", a branch `feature/sdl3-vulkan-pr` foi enviada pro fork pessoal (`tibazera/ezquake-source`, remote `origin` do worktree da PR) — o workflow `.github/workflows/main.yml` do próprio projeto já dispara em `push` para qualquer branch (`branches: ['**']`), não só PR, então isso roda a matriz real de 4 jobs (`windows-2025-vs2026`, `macos-arm64`, `macos-x64`, `ubuntu-latest`) sem precisar abrir PR contra o `qw-group`. Usado também um branch descartável (`test/ci-validation`, deletado depois) só pra setar `fail-fast: false` temporariamente e ver os 4 jobs de uma vez em vez de descobrir um erro por vez. Run final, verde nos 4 jobs + `macos-universal`, na branch real (com `fail-fast` padrão): https://github.com/tibazera/ezquake-source/actions/runs/27922601144.
  - **4 bugs reais encontrados só nessa validação cruzada, nenhum visível no MSVC/Windows** (commits 12-14 do PR, replicados também neste worktree principal no commit `0cb34fe6`): (1) `src/vid_sdl2.c` tinha uma `SDL_DisplayMode display_mode;` morta (sobra da migração) — GCC trata `-Werror=unused-variable` como erro fatal em build Release, MSVC não; (2) o bloco `#ifdef __APPLE__` de deadkey-ignore e o log de debug `developer 2` em `vid_sdl2.c` ainda liam os campos antigos `event->keysym.*`/`event->state` do SDL2 — SDL3 achatou isso direto no evento (`event->key`, `event->down`), só visível em build macOS; (3) `src/sys_osx.m` tinha o único `SDL_CreateWindow` do projeto ainda na assinatura de 6 args do SDL2 (com x/y) — SDL3 só aceita `title/w/h/flags`, macOS-only; (4) `gl_misc.c` passava um índice de display (int) onde o SDL3 já espera um `SDL_DisplayID` opaco — bug silencioso (mostrava "0Hz" no `gfxinfo` em vez de falhar o build), corrigido expondo `VID_SDL_DisplayID()` (antes `static` em `vid_sdl2.c`) via `r_local.h`; aproveitado pra também renomear `SDL_FreeSurface`→`SDL_DestroySurface`.
  - **Lição**: "mesmo arquivo .c" não significa "mesmo resultado" entre plataformas — compilador (GCC vs MSVC vs Clang) e versão da lib externa puxada pelo vcpkg/apt por SO mudam o que é erro fatal e quais símbolos existem. Os 3 bugs macOS só apareceram porque o código real só compila nesse SO (`#ifdef __APPLE__`/arquivo `.m`); nenhuma leitura de diff pegaria isso, só build de verdade.
  - `gh` (GitHub CLI) precisou de `gh auth login` manual do usuário (rodado por ele com prefixo `!` na sessão) pra eu conseguir baixar logs de job de CI — sem isso só dá pra ver status geral via API pública, não o log detalhado de erro.
  - **PR aberto de verdade contra o upstream (2026-06-22), só depois de autorização explícita de Tiago no momento:** https://github.com/QW-Group/ezquake-source/pull/1145 ("Add Vulkan renderer backend and migrate desktop client to SDL3"), de `tibazera/ezquake-source:feature/sdl3-vulkan-pr` pra `QW-Group/ezquake-source:master`, via `gh pr create`. Corpo do PR = `PR_DESCRIPTION.md` (sem o H1, que foi usado como `--title`), revisado e ajustado por Tiago antes de abrir — inclui a seção "Why this works, and why it doesn't break the existing renderers" em 3 categorias de risco (Vulkan isolado/zero-risco, migração SDL2→SDL3 1:1, flag `TEX_PREMUL_ALPHA` em código compartilhado com análise de blend mode já existente no GL). Android continua de fora, PR separado futuro.

Commits de referência no momento em que esta memória foi criada:

- `675fb197` — migração do cliente Android/Vulkan para SDL3.
- `cea4a636` — batching das atualizações dinâmicas de lightmap.
- `691a5959` — dois frames Vulkan em voo com sincronização e staging por frame.
- `53fdaa54` — superfície Android limitada proporcionalmente ao envelope 1920x1080.
- `e33e1597` — handoff Android/Vulkan inicial.
- `eff9b8f2` — estratégia documentada para o conflito de PK3.

Esses hashes são marcos, não substitutos do worktree local.

## Build validado

Ambiente usado:

- JDK 17: `C:\Program Files\Eclipse Adoptium\jdk-17.0.19.10-hotspot`
- Android SDK: `C:\Android\Sdk`
- NDK: `C:\Android\Sdk\ndk\28.2.13676358`
- glslang host tools: `C:\eqs\vcpkg\installed\x64-windows\tools\glslang`

No PowerShell, configure as variáveis e execute:

```powershell
$env:JAVA_HOME='C:\Program Files\Eclipse Adoptium\jdk-17.0.19.10-hotspot'
$env:ANDROID_HOME='C:\Android\Sdk'
$env:ANDROID_SDK_ROOT='C:\Android\Sdk'
$env:ANDROID_NDK_HOME='C:\Android\Sdk\ndk\28.2.13676358'
$env:Path='C:\eqs\vcpkg\installed\x64-windows\tools\glslang;' + $env:Path
.\gradlew.bat :app:assembleDebug
```

APK gerado: `app\build\outputs\apk\debug\app-debug.apk`.

Dispositivo usado nos testes: Xiaomi 2407FPN8EG, codinome `rothko`, Android 16, serial ADB `LJN7LFRKXG79P7J7`. O serial pode mudar ou o aparelho pode estar desconectado; sempre confirme com `C:\Android\Sdk\platform-tools\adb.exe devices`.

## Erros cometidos e o que aprendemos

### Diretório errado

No início, houve trabalho e investigação apontando para `unezquakepocket`. O fonte real passou a ser `E:\Projetos Linux\ezquake-source`; `unezquakepocket` é somente entrega de APK. Sempre confirme o diretório antes de editar ou executar Git.

### Texturas tratadas inicialmente como defeito Vulkan

dm3, dm4 e dm6 carregavam lava, portais e itens, mas paredes e chão apareciam sem as texturas esperadas. e1m2 e aerowalk funcionavam. Isso levou a várias hipóteses no upload Vulkan, BSP e sobreposição de texturas.

A causa confirmada no aparelho foi `id1/gpl_maps.pk3`: ele continha `maps/dm3.bsp`, `maps/dm4.bsp` e `maps/dm6.bsp` em variantes simple-texture e tinha prioridade sobre os mapas originais de `pak1.pak`. Remover somente essas três entradas restaurou imediatamente os mapas texturizados.

Não transforme isso em hack de renderer, regra especial por nome de mapa ou reescrita automática de arquivo. A correção planejada é criar ou obter outro PK3 limpo, sem esses BSPs substitutos, e validar sua prioridade de carregamento. Não versione dados do Quake neste repositório. No aparelho foi mantido um backup diagnóstico chamado `gpl_maps.pk3.simpletextures-backup`; isso não é parte do projeto.

### Migração SDL2 para SDL3 feita além da troca de dependência

Trocar `sdl2` por `sdl3` no vcpkg/CMake não bastou. SDL3 mudou assinaturas, nomes de eventos, estruturas de teclado, APIs Vulkan, retorno de inicialização, mutexes e semáforos. O projeto usa temporariamente `SDL_ENABLE_OLD_NAMES`, mas toda chamada alterada deve respeitar a semântica SDL3 real.

O erro mais grave foi manter a expressão SDL2 `SDL_TryLockMutex(mutex) == 0`. No SDL2, sucesso era zero; no SDL3, a função retorna `bool`. A chamada adquiria o mutex, interpretava sucesso como falha e não o liberava. Isso causou silêncio e a assertion em `SDL_TryLockMutex`/`SDL_sysmutex.c:116`. A forma correta atual é testar diretamente o retorno booleano.

Ao portar outras chamadas, confira a documentação/headers da versão SDL3 instalada; não deduza a semântica apenas pelo nome antigo.

### Oboe acusado quando o scrcpy desviava o áudio

O backend Oboe do APK do Lele já funcionava e é essencialmente o mesmo. Instrumentação temporária confirmou PCM não zero no callback, AAudio iniciado e player sem mute. O silêncio restante ocorria porque o scrcpy captura áudio por padrão e roteava a saída para `AUDIO_DEVICE_OUT_REMOTE_SUBMIX`, retirando-a do alto-falante do telefone.

Para manter áudio no celular:

```powershell
scrcpy --serial LJN7LFRKXG79P7J7 --keyboard=uhid --mouse=uhid --no-audio
```

Não abra instâncias duplicadas do scrcpy. Verifique processos existentes antes de iniciar outra. Se o objetivo for ouvir no computador, remova `--no-audio` e confira o dispositivo de saída do Windows.

Durante o diagnóstico, o callback Oboe foi temporariamente alterado para bloquear no mutex e foram adicionados logs de amostras. Essas experiências foram revertidas. O callback final permanece não bloqueante como no port do Lele; a correção real foi a semântica SDL3 do mutex.

### Tela preta depois de limitar a resolução de render

Depois de introduzir `getHolder().setFixedSize()` em `SDLSurface` para limitar o buffer ao envelope 1920x1080, a tela passou a mostrar o jogo renderizado num retângulo no canto superior esquerdo, com o resto preto. A causa não foi o compositor: `SDLActivity` adicionava o `SurfaceView` ao `RelativeLayout` sem `LayoutParams` explícito (`mLayout.addView(mSurface)`), então o layout usava `WRAP_CONTENT`. Sem tamanho fixo de buffer isso nunca importava; com o buffer fixo, o `onMeasure()` do `SurfaceView` passou a reportar o tamanho fixo como preferido, e a View ficou do tamanho do buffer (1920x864) em vez de preencher a Activity (2712x1220). Confirmado via `dumpsys SurfaceFlinger` comparando `bounds`/`geomLayerBounds` da layer do SurfaceView com a layer da Activity. Corrigido passando `RelativeLayout.LayoutParams(MATCH_PARENT, MATCH_PARENT)` no `addView`. Ao tocar em `setFixedSize`/dimensionamento de buffer em qualquer SurfaceView, confirme sempre que o `LayoutParams` do pai força preenchimento total — não assuma que o compositor escala automaticamente sem isso.

### Entrada remota no Xiaomi

`adb shell input` e a injeção normal do scrcpy falharam com `INJECT_EVENTS`. O modo UHID de teclado e mouse funciona sem essa permissão adicional. Não conclua que o SDL3 perdeu entrada antes de separar restrição do Android/Xiaomi de falha no engine.

### Aplicativo que não iniciava

Após a migração, o APK chegou a não abrir. Foram necessários o bootstrap Java SDL3 correto, `getMainFunction()` retornando `main`, símbolo nativo exportado e correções de ciclo de vida/retornos booleanos. Evite desfazer esses pontos ao sincronizar arquivos Java do SDL.

## Decisões de desempenho e riscos conhecidos

O primeiro gargalo atacado foi a atualização dinâmica de lightmaps, que usava command buffers imediatos e esperas de fila repetidas. O staging atual reduz essas sincronizações e introduz uma latência de até um frame para uploads enfileirados.

O renderer originalmente usava um único fence global e esperava no começo de todo frame. Isso anulava o benefício das várias imagens do swapchain e podia provocar degraus de frame pacing em FIFO. A implementação atual usa dois frames em voo, synchronization objects por frame, fence por imagem e staging de upload separado por frame. Se o número de frames em voo mudar, mantenha todas essas estruturas dimensionadas em conjunto.

No Xiaomi de teste, o surface em paisagem é 2712x1220 e o display físico foi observado a 120 Hz. O build com dois frames em voo iniciou e permaneceu estável, sem fatal ou erro Vulkan no logcat. O ganho real de FPS em dm3/dm4/dm6 ainda não foi medido; não o trate como resultado confirmado.

Para evitar custo de fill-rate desnecessário, `SDLSurface` chama `SurfaceHolder.setFixedSize()` com um tamanho que cabe em 1920x1080 e mantém o aspect ratio do aparelho. Exemplos: 1920x1080 permanece 1920x1080; 2712x1220 vira 1920x864; aparelhos menores mantêm sua resolução nativa. O tamanho físico do `View`, e não o tamanho reduzido do buffer, deve ser usado para normalizar eventos de toque.

`dumpsys gfxinfo` mede principalmente a UI Java e não representa os frames Vulkan do `SurfaceView`. Para observar apresentação real, obtenha o nome exato da layer BLAST no `dumpsys SurfaceFlinger` e use `dumpsys SurfaceFlinger --latency '<nome da layer>'`. A primeira linha é o período do display em nanossegundos e as seguintes contêm timestamps por frame. No menu desconectado foram observados intervalos próximos de 33–40 ms, mas isso não é benchmark de gameplay: `CL_MinFrameTime()` aplica `cl_maxfps_menu` ou a frequência detectada quando `cls.state == ca_disconnected`. O config ativo tinha `cl_maxfps 308`, `cl_maxfps_menu 0` e `vid_vsync 0`.

Não importe grandes blocos de vkQuake/FTEQW por intuição. Primeiro meça:

- tempo de CPU e GPU por frame;
- draw calls do mundo;
- trocas de pipeline e descriptor sets;
- custo de lightmaps dinâmicos;
- resolução real do swapchain e escalonamento;
- pacing, present mode e sincronização;
- comportamento térmico depois de alguns minutos.

Use dm3, dm4 e dm6 com dados conhecidos e limpos para comparação. e1m2 e aerowalk são controles úteis porque nunca apresentaram o mesmo conflito.

## Fluxo obrigatório de trabalho

1. Leia o pedido mais recente e este arquivo.
2. Inspecione worktree, branch e diffs antes de editar.
3. Faça mudanças somente em `E:\Projetos Linux\ezquake-source`, salvo pedido explícito de copiar o APK pronto.
4. Preserve alterações locais, arquivos de usuário e dados PK3/PAK.
5. Para bugs, obtenha evidência com logs, diffs e testes pequenos antes de mudar arquitetura.
6. Remova toda instrumentação temporária antes de considerar a alteração concluída.
7. Execute pelo menos o build Android debug após alterações nativas, Java, CMake ou dependências.
8. Se houver aparelho conectado e o pedido incluir teste, instale com `adb install -r`, relance e confirme que o processo permanece vivo. Tiago navega e valida visualmente; não tire screenshots automaticamente.
9. Relate exatamente o que foi comprovado e o que ainda é hipótese.
10. Não faça commit/push/PR sem autorização atual. Quando autorizado, prefira commits pequenos por assunto e mantenha o PR preparado para revisão futura, não para envio imediato ao `qw-group`.

## Auditoria de menu/cvars Vulkan (2026-06-25)

A pedido de Tiago, foi feita uma varredura dos menus/cvars de vídeo que ainda assumem OpenGL ou são no-op sob Vulkan (`renderer.X = VK_NoOperation*` em `vk_main.c`). Build Windows usado para validar: `cmd.exe /c "_build_vulkan.bat"` (PowerShell, não Bash do Git, que não herda corretamente o ambiente do `vcvars64.bat` por algum motivo — usar a tool de PowerShell para este build). Testado rodando `ezquake.exe -dev -condebug +set vid_renderer 2` e lendo `qw/qconsole.log` (note o subdiretório `qw/`, não a raiz do build).

Confirmados e corrigidos nesta sessão:

- **Anisotropic Filtering (`gl_anisotropy`) era 100% no-op sob Vulkan.** `VK_TextureSetAnisotropy` em `vk_texture.c` era um stub vazio, e o sampler sempre criava com `anisotropyEnable = VK_FALSE`. Corrigido: `VK_CreateLogicalDevice` (`vk_physical_devices.c`) agora habilita `samplerAnisotropy` na criação do device quando o physical device reporta suporte (estava com `VkPhysicalDeviceFeatures` zerado, nada era habilitado); `vk_texture_t` ganhou um campo `anisotropy`; `VK_TextureCreateSampler` usa `min(anisotropy, limits.maxSamplerAnisotropy)` quando suportado; `VK_TextureSetAnisotropy` agora armazena o valor e recria os samplers (mesmo padrão que `VK_TextureWrapModeClamp` já usava pra clamp/repeat).
- **`renderer.PolyBlend` (flash de dano/pickup/quad/pent, tint de underwater) era 100% no-op sob Vulkan** (`VK_PolyBlend = VK_NoOperationFloat4`). `R_PolyBlend()` em `r_rmain.c` já chama isso incondicionalmente sempre que `v_blend[3] != 0` (a guarda de hw-gamma nunca é true, ver próximo item), então esse efeito visual simplesmente não existia em nenhum frame Vulkan. Corrigido implementando `VK_PolyBlend` em `vk_draw.c` reaproveitando `VK_DrawRectangle` (mesmo pipeline 2D de alpha premultiplicado já usado por HUD), igual à técnica do `GLC_PolyBlend`/`GLM_PolyBlend`.
- **`renderer.BrightenScreen` (parte do slider "Gamma"/"Contrast" do menu) era 100% no-op sob Vulkan.** Implementado `VK_BrightenScreen` em `vk_draw.c` com a mesma técnica de `GLC_BrightenScreen`/`GLM_BrightenScreen`: redesenha um quad fullscreen com blend aditivo (`r_blendfunc_additive_blending`, novo pipeline `hudBrightenPipeline` criado em `VK_HudEnsureResources` reaproveitando o shader `vk_hud_color`), indo de `v_contrast` até 1 dobrando o brilho a cada passada. Isso só cobre `v_contrast > 1`; **não implementa a curva real de `v_gamma`** (escurecer com gamma < 1, ou clarear sem ser via contrast) porque isso exigiria uma passagem de post-process lendo o frame já renderizado, que o Vulkan não tem (ver limitação abaixo). `VK_BrightenScreen` foi ligado em `renderer.PostProcessScreen` (`#define VK_PostProcessScreen VK_BrightenScreen` em `vk_main.c`), já que `GL_FramebufferPostProcessScreen()` (o call site GL real) nunca é chamado pro Vulkan — o call site genérico correto é `renderer.PostProcessScreen()` em `cl_screen.c:SCR_UpdateScreenPostPlayerView()`.
- Nova entrada de buffer compartilhada `r_buffer_hud_brighten_vertex_data` em `r_buffers.h` (quad NDC estático) só usada pelo Vulkan, seguindo o mesmo padrão de `r_buffer_hud_circle_vertex_data`.
- `VK_HudCreateColorPipeline` (`vk_draw.c`) ganhou um parâmetro `r_blendfunc_t blendFunc` (antes hardcoded em `r_blendfunc_premultiplied_alpha`) pra poder criar o pipeline aditivo do brighten sem duplicar a função.

Investigado e **decidido não implementar agora** (documentar como limitação, não confundir com bug não percebido):

- **Vulkan não tem post-process pass nenhum** (`VK_RenderFramebuffers`, `VK_PostProcessScreen` antes desta sessão, e `VK_FramebufferCreate` são todos no-op/false). Toda a seção de menu "Framebuffer" (`vid_framebuffer`, HDR, HDR Tonemap, `vid_framebuffer_scale`, **`vid_framebuffer_multisample`** = antialiasing/MSAA, `vid_framebuffer_fxaa`) é puramente GL e não tem efeito nenhum sob Vulkan — não é regressão desta sessão, é arquitetura ausente. Implementar MSAA real exigiria um color attachment multisample + resolve em TODOS os pipelines que desenham na render pass principal (`vk_world.c`, `vk_aliasmodel.c`, `vk_sprite3d.c`, `vk_draw.c`) — mudança grande e arriscada, não cabe em "fix incremental pequeno". Decisão: deixar para uma sessão dedicada só a isso, não tentar meio-caminho.
- ~~Vulkan nunca gera mip levels~~ — **implementado nesta sessão**, ver seção "Roadmap mipmap → MSAA → gamma" abaixo.
- **`v_gamma` real (escurecer/clarear fora do hack de contrast aditivo) permanece sem suporte sob Vulkan.** Nota importante: HW gamma já está morta para TODOS os renderizadores, não só Vulkan — `VID_SetDeviceGammaRampReal()` em `vid_sdl2.c` só faz algo dentro de `#ifdef X11_GAMMA_WORKAROUND`, que nunca é definido (confirmado lendo o código; mesma conclusão a que chegou um commit não-mergeado da branch `pr-1105-nano-sdl3`, ver auditoria de branches abaixo). Pro GL real hoje, gamma funciona via post-process (`vid_software_palette=1`, uniform `r_program_uniform_post_process_glc_gamma`/equivalente GLM) — Vulkan precisaria da mesma passagem de post-process do item acima pra ganhar isso.
- `renderer.DrawSky`, `DrawAliasModelPowerupShell`/`DrawAlias3ModelPowerupShell`, `DrawDisc` são no-op sob Vulkan, mas **`GLM_DrawSky`/`GLM_DrawAliasModelPowerupShell`/`GLM_DrawAlias3ModelPowerupShell`/`GLM_DrawDisc` também são no-op** (`glm_main.c`) — não é lacuna específica do Vulkan, é decisão já tomada no renderer moderno. Não tratar como bug a corrigir.
- `vid_vsync`/present mode e `vid_restart`/recriação de swapchain já funcionam corretamente sob Vulkan (`VK_RefreshPresentationMode`, `VK_PhysicalDeviceBestPresentationMode` em `vk_physical_devices.c`) — confirmado lendo o código, não precisou de correção. Screenshot (`VK_Screenshot`) e depth-bias (`vkCmdSetDepthBias` em `vk_world.c`) também já são implementações reais, não stubs — não confundir com o stub antigo mencionado em commits de outra branch (ver abaixo).

### Pesquisa vkQuake/FTEQW pras 3 lacunas restantes (2026-06-25)

A pedido de Tiago, dois agentes leram o código-fonte de `E:\Projetos Linux\_research\vkquake` e `E:\Projetos Linux\_research\fteqw` pra ver como eles resolvem MSAA, gamma e mipmap em Vulkan. Achado principal: **as duas engines confirmam, de forma independente, que MSAA e gamma real exigem a mesma mudança de arquitetura** (um render target offscreen + segunda passagem de composição), **mas mipmap não exige nada disso** — é só trabalho de upload de textura, pode ser feito isolado.

1. **Mipmap (o mais fácil, sem mudança de arquitetura)**: tanto vkQuake (`gl_texmgr.c`) quanto FTEQW (`engine/client/image.c` + `vk_init.c`) geram os mips **na CPU** (box-filter, ex. `stb_image_resize`/`Image_MipMap*X8`), empacotam todos os níveis num staging buffer só, e fazem **um único `vkCmdCopyBufferToImage`** com uma `VkBufferImageCopy` por nível de mip (`imageSubresource.mipLevel = i`). Nenhuma das duas usa `vkCmdBlitImage` para mipmap de textura estática (vkQuake só usa blit pra mip do render-target de água/warp, que precisa regenerar todo frame). Sampler usa `maxLod = FLT_MAX`/sem teto artificial, anisotropia limitada a `VkPhysicalDeviceLimits.maxSamplerAnisotropy` (igual ao que já implementei nesta sessão). **Pra portar**: se o ezQuake já tem geração de mip em CPU pro caminho OpenGL (bem provável, é a abordagem clássica do Quake), é só alimentar esse mesmo array de níveis pro `VkImageCreateInfo.mipLevels` e fazer upload em lote — sem tocar em render pass nenhuma.

2. **MSAA**: as duas confirmam que **não dá pra multisamplar a imagem do swapchain diretamente** — sempre existe uma imagem de cor offscreen single-sample, e o resolve do MSAA acontece da imagem multisample pra essa offscreen via `pResolveAttachments` da render pass (resolve automático do driver, sem shader manual). vkQuake sempre usa esse caminho; FTEQW só aloca o target offscreen **quando precisa** (`vid_multisample > 0` ou outro post-effect ativo) e renderiza direto pro swapchain nos outros casos — modelo mais parecido com o que o Vulkan do ezQuake já faz hoje (direto pro swapchain), então é o mais fácil de adaptar sem reescrever tudo.
3. **Gamma/contrast/brightness real**: as duas implementam como uma passagem fullscreen depois da cena renderizada, lendo a imagem offscreen (`subpassLoad` no vkQuake, sampler comum no FTEQW) e aplicando `pow(cor, gamma) * contrast + brightness` num fragment shader trivial, escrevendo o resultado na imagem real do swapchain. **Confirma de forma independente** o que já tínhamos concluído sozinhos: Vulkan não tem nenhuma API de gamma ramp de swapchain (diferente do GL com `SDL_SetWindowGammaRamp`), então essa passagem de post-process é a ÚNICA forma de ter gamma real em Vulkan — não tem alternativa mais simples. FTEQW até documenta isso explicitamente nos modos de `vid_hardwaregamma` (modo 4 = "scene-only gamma shader", único caminho disponível pra Vulkan).

**Conclusão prática**: os itens 2 e 3 compartilham a mesma infraestrutura (render target offscreen + segunda render pass de composição) nas duas engines — não vale a pena implementar um sem pensar no outro. O modelo do FTEQW (target offscreen só alocado on-demand, baseado em flags por frame) é o que se encaixa melhor na arquitetura atual do Vulkan do ezQuake (que já renderiza direto pro swapchain e funciona bem assim na maioria dos casos). Recomendação pra sessão futura: implementar primeiro uma abstração mínima de render-target offscreen (cor + depth, framebuffer, render pass com cache por combinação de flags), do tipo `VK_RT_Begin`/`VK_RT_End`, e então pendurar MSAA e gamma real nela como flags opcionais — exatamente como o FTEQW faz com seu pool `postproc[4]` e bitmask de flags (`RP_MULTISAMPLE`, `RP_FP16`, etc). Mipmap pode (e deve) ser feito antes/em paralelo, sem depender disso.

### Roadmap mipmap → MSAA → gamma, e implementação do item 1 (2026-06-25)

A pedido de Tiago ("implementa do mais facil para o mais dificil"), a ordem acima (mipmap, depois MSAA, depois gamma real) foi adotada como roadmap formal e o item 1 foi implementado nesta sessão.

**Mipmap real implementado (item 1, concluído).** Antes, nenhuma textura Vulkan tinha `mipLevels > 1`; agora `VK_UploadTexture` (`vk_texture.c`) gera a cadeia completa de mips na CPU quando `mode & TEX_MIPMAP` (mesma condição que já existia e já era respeitada por `GL_UploadTexture`/`glGenerateMipmap` no caminho OpenGL), via `Image_MipReduce` (`image.c`, já existente, mesmo box-filter usado no picmip — não foi escrita lógica de filtro nova). Mudanças principais:

- `VK_TextureMipLevelCount(width, height)`: conta níveis até 1x1, igual ao padrão vkQuake/FTEQW.
- `VK_TextureQueuePendingMipmapUpload`: gera a cadeia inteira (nível 0 + reduções sucessivas via `Image_MipReduce`) direto dentro do buffer de upload em lote já existente (o mesmo usado por lightmaps/texturas comuns), e a enfileira como **uma entrada só** (`vk_pending_texture_upload_t` ganhou `mipCount`/`extraMipWidth`/`extraMipHeight`/`extraMipOffset[16]`) — importante: contagem de entradas na fila continua proporcional ao número de *texturas*, não texturas×níveis, então o cap de `VK_MAX_PENDING_TEXTURE_UPLOADS` (1024) carregando um mapa dm3-scale (~420 texturas) não precisou mudar.
- `VK_TextureFlushPendingUploads` e a nova `VK_TextureUploadMipChainImmediate` (fallback raro, só se a fila lotar) fazem **uma única `vkCmdCopyBufferToImage`** com um array de `VkBufferImageCopy` (um por nível) por textura, em vez de uma chamada por nível.
- `VK_TextureRecordTransitionBarrier` e `VK_TextureCreateImageView` tinham `levelCount`/`baseMipLevel` hardcoded em `1` — agora usam `vktex->mipLevels` (a barreira de layout cobre a imagem inteira de uma vez, não nível a nível; texturas sem mipmap continuam com `mipLevels=1`, comportamento idêntico a antes).
- `VK_CreateImageResource` (`vk_resources.c`) ganhou um parâmetro `mipLevels` (`vk_swapchain.c` passa `1` pro depth buffer, sem mudança de comportamento ali).
- Sampler: `maxLod` deixou de ser fixo em `0.0f`, agora é `mipLevels - 1`, então a GPU passa a escolher entre níveis de verdade (antes a opção "Quality Mode" `GL_LINEAR_MIPMAP_LINEAR` vs `GL_LINEAR` era indistinguível sob Vulkan; agora tem efeito real).
- Texturas sem `TEX_MIPMAP` (HUD, fontes, lightmaps, sprites 2D) continuam exatamente como antes — `mipLevels=1`, mesmo caminho de upload de nível único, nenhuma mudança de comportamento ou de custo pra elas.

Validado: build limpo (0 erros/warnings) e `ezquake.exe -dev -condebug +set vid_renderer 2 +map dm3` carregou o mapa completo (mensagem "The Abandoned Base" no console = sucesso) sem erro de validação Vulkan nem crash, exercitando o caminho em lote com várias centenas de texturas mipmapadas de uma vez.

**Item 2 (MSAA) implementado em sessão posterior (2026-06-25, parte 3)** — diferente da previsão acima, não precisou da abstração de render-target offscreen completa: bastou um attachment multisample (`VK_CreateSwapChainMSAAColorResources` em `vk_swapchain.c`) resolvido direto pro swapchain via `pResolveAttachments` na render pass (`vk_renderpass.c`), mais `VK_DetermineMSAASampleCount` (`vk_physical_devices.c`, clampa o cvar `vid_framebuffer_multisample` contra os limites reais do device) e os 10 sites de `rasterizationSamples` hardcoded em `vk_aliasmodel.c`/`vk_draw.c`/`vk_sprite3d.c`/`vk_vao.c`/`vk_world.c` passando a ler `vk_options.msaaSamples`. Implementado primeiro em `feature/sdl3-vulkan-pr` (validado lá: 0/4/8/16x, incluindo o clamp 16→8 e uma correção de um bug de validação pré-existente — layout `UNDEFINED` no primeiro uso de uma imagem recém-criada quando a variante "no-clear" do render pass de `gl_clear` era selecionada por engano), depois portado pra este branch (aqui sem a complicação do clear/no-clear variant, que não existe neste branch). Validado em ambos com `-dev -condebug +set vid_renderer 2 +map dm3`, sem crash nem erro de validação. **Item 3 (gamma real) continua não implementado** — ainda exige a passagem de post-process com leitura de volta do frame renderizado, que esse trabalho de MSAA não precisou e não substitui.

### Auditoria das outras branches locais (a pedido de Tiago: "if had something we can use on android, its different branchs")

Branches locais com commits que NÃO estão em `feature/android-pocket-vulkan`: `feature/vulkan-sdl3` (4 commits) e `pr-1105-nano-sdl3` (5 commits). `feature/android-vulkan` e `feature/colorize-f-version-f-system` são idênticas ao branch atual (0 commits de diferença).

- **`feature/vulkan-sdl3`**: os 4 commits únicos (Vulkan backend inicial, migração SDL3, fix de `vid_renderer` bounds/depth-bias/screenshot) já estão **superados** por versões melhores já presentes em `feature/android-pocket-vulkan` — confirmado lendo o código atual: screenshot já é real (`VK_Screenshot` em `vk_main.c`), depth-bias já usa `vkCmdSetDepthBias` de verdade (`vk_world.c`), `vid_renderer` bounds já corrigido. Nada para portar daqui.
- **`pr-1105-nano-sdl3`**: os 5 commits únicos (porta pra SDL3 numa linhagem separada, e principalmente **remove o motor de hardware gamma morto e roteia o view-blend pelo shader de post-process**, autor Daniel Svensson + Claude Opus 4.8, datado 2026-06-19) **não estão** em `feature/android-pocket-vulkan` — o código atual ainda tem o motor de HW gamma morto (`vid_hwgammacontrol`, `V_UpdatePalette`, `applyHWGamma` em `cl_view.c`/`cl_screenshot.c`/`vid_sdl2.c`). Isso é só GL (`gl_framebuffer.c`/`glc_framebuffer.c`/`glm_framebuffer.c`), não toca Vulkan, mas é um candidato relevante pra portar/cherry-pick numa sessão futura: limpa código morto e corrige a semântica de `vid_software_palette`/gamma pro GL real, sem qualquer benefício direto pro Vulkan (que precisaria da sua própria passagem de post-process pra se beneficiar igual).

## Próximas prioridades

1. Confirmar áudio no aparelho com scrcpy em `--no-audio` durante jogo real.
2. Medir FPS e frame time em dm3/dm4/dm6 após o batching de lightmaps e os dois frames em voo.
3. Preparar e validar um PK3 substituto limpo fora do repositório de código.
4. Perfilar CPU/GPU antes da próxima otimização Vulkan.
5. Só depois comparar implementações específicas de vkQuake e FTEQW para o gargalo medido.
6. **Concluído e validado visualmente no Windows (2026-06-25).** Post-process real implementado no Vulkan: render pass adicional (`vk_renderpass_postprocess` em `vk_renderpass.c`) compõe a partir de um color target offscreen por imagem do swapchain (`postProcessColorImages`/`Views`, alocados em `VK_CreatePostProcessResources` em `vk_swapchain.c`, junto dos framebuffers principais — sempre alocados, já que `v_gamma`/`v_contrast`/`vid_framebuffer_fxaa` não são latched e podem mudar sem vid_restart). `VK_PostProcessActive()` decide por frame, em `VK_BeginFrame`, se a passagem principal desenha no offscreen target ou direto no swapchain. **Importante:** assim como GLM (`GLM_CompilePostProcessProgram`/`POST_PROCESS_PALETTE`), essa passagem só aplica a curva real de gamma/contraste quando `vid_software_palette` está ativo — com `vid_software_palette 0` (default no Windows é 1, mas o usuário tinha 0 no cfg dele), gamma/contraste ficam a cargo da gamma ramp do SO e a passagem Vulkan vira no-op nesse aspecto (só roda se FXAA estiver ligado). `VK_EndFrame` roda a passagem de composição (`VK_PostProcessComposite` em `vk_draw.c`). Shaders novos: `src/vulkan_shaders/vk_post_process.{vert,frag}`. `timerefresh` também implementado de verdade (`VK_TimeRefresh` em `vk_main.c`): 128 ciclos completos de submit+present reais com `vkDeviceWaitIdle` no final.
   - **Bug real encontrado e corrigido na mesma sessão:** o sintoma reportado ("tela toda branca, mas o jogo responde a comandos como quit") não veio do código novo de post-process — veio de `VK_BrightenScreen` (`vk_draw.c`, implementado horas antes no commit `95d24f48`, é o único caminho de contraste que o Vulkan já tinha antes deste post-process). Ele usava `r_blendfunc_additive_blending` (src+dst) no `hudBrightenPipeline`, enquanto GLC/GLM usam `r_blendfunc_src_dst_color_dest_one` (`dst*(1+src)`, ver `r_states.c:147`). Com `gl_contrast 2`, o primeiro passo do loop (`f>=2`) desenha branco puro (`1,1,1,1`); em blend aditivo isso satura a tela inteira pra branco independente da cena por baixo — em `dst*(1+src)` (o correto) isso apenas dobra o brilho existente. Corrigido trocando o blendfunc na criação do pipeline (`vk_draw.c` ~linha 790). Confirmado pelo usuário que resolveu (`vid_renderer 2`, `gl_contrast 2`, `gl_gamma 0.4`, `vid_software_palette 0`).
   - Próximo passo: portar essa mesma correção de blendfunc pro `feature/android-pocket-vulkan` (branch Android) e rodar build Android debug, já que `VK_BrightenScreen` é idêntico em ambos os branches e o bug provavelmente também afeta Android com `gl_contrast > 1`.
7. Avaliar portar os commits de remoção de HW-gamma da branch `pr-1105-nano-sdl3` pro `feature/android-pocket-vulkan` (benefício é só GL, mas remove código morto e simplifica antes de decidir o post-process do Vulkan).
8. Continuar a varredura de menu/cvars do pedido original: ainda não verificados nesta sessão — brilho/gamma já coberto acima, mas resolução/refresh rate, bit depth, fullscreen/windowed e os cvars de antilag/prediction/ruleset não foram auditados (suspeita: já funcionam, pois não são específicos de renderer, mas não confirmado lendo o código Vulkan).

## Arquivos centrais

- `CMakeLists.txt` e `vcpkg.json`: SDL3, Oboe e dependências Android.
- `app/src/main/java/org/ezquake/android/EzQuakeActivity.java`: entrada Android específica do app.
- `app/src/main/java/org/libsdl/app/`: bootstrap Java SDL3.
- `app/src/main/java/org/libsdl/app/SDLSurface.java`: limite proporcional da superfície Android e normalização física do toque.
- `src/vid_sdl2.c` e `src/in_sdl2.c`: plataforma SDL; os nomes históricos dos arquivos permanecem apesar do SDL3.
- `src/snd_main.c`: mutex e mixer.
- `src/snd_backend_oboe.cpp`: saída Android Oboe.
- `src/snd_voip.c`: captura de voz, atualmente desativada no Android.
- `src/vk_texture.c`: upload e batching de texturas/lightmaps.
- `src/vk_main.c`: início/fim do frame Vulkan e flush de uploads.
- `docs/android-pocket-vulkan-handoff.md`: resumo técnico curto para handoff.

### Estratégia de branch atualizada (2026-06-25) — leia antes de decidir onde trabalhar

Tiago corrigiu o entendimento anterior deste arquivo: **`feature/sdl3-vulkan-pr`** (worktree separado em `E:\Projetos Linux\ezquake-vulkan-pr`) é o branch **primário** — é o head real da PR #1145 pro `QW-Group/ezquake-source` upstream. Todo trabalho novo de Vulkan deve ser desenvolvido e commitado ali primeiro, validado no Windows, com a PR atualizada. `feature/android-pocket-vulkan` é o branch paralelo Android: só recebe trabalho depois de validado em `sdl3-vulkan-pr`, mas **sempre** que algo validado lá não quebrar o Android, deve ser portado pra aqui também na mesma sessão — não esperar a PR ser aceita. `feature/vulkan-sdl3` (nome parecido, branch diferente) continua obsoleto/abandonado.

Quando o token do Claude acabar e Tiago continuar no Codex (ou vice-versa): se uma ferramenta produzir algo melhor enquanto a outra estava fora, continue a partir do melhor, mas não descarte o trabalho que só a sessão atual tem — compare e funda os dois.

**Sessão de 2026-06-25 (parte 2):** `feature/sdl3-vulkan-pr` já tinha um WIP não commitado superior ao daqui em alguns pontos (sampler cache deduplicado em vez de 2 samplers por textura, mip pyramid via `Image_MipReduce`) — mantido como está, sem reescrever com a versão deste branch. Nesse WIP também havia 2 bugs reais de sincronização (descritos abaixo) escondidos porque o nome do validation layer estava errado (`VK_LAYER_LUNARG_standard_validation`, removido do SDK há anos; corrigido pra `VK_LAYER_KHRONOS_validation`) — sem a layer carregando, nenhum erro de validação aparecia. 5 commits feitos e enviados pra `sdl3-vulkan-pr` (push + PR #1145 atualizada): nome da validation layer, remoção de código morto de device-layer, remoção de claim falso de `R_SUPPORT_FRAMEBUFFERS`, fix de PolyBlend/BrightenScreen portado deste branch, e o sampler-cache/mipmap do próprio WIP. Os bugs de sincronização que a validation layer corrigida revelou — `worldFlatSkyDescriptorSet` compartilhado entre frames-in-flight (deveria ser um set por frame) e padding insuficiente em 2 structs de push-constant pro layout std430 — existiam **independentemente** aqui também (mesma causa raiz, código parecido); portados pra este branch junto com a correção do nome da validation layer, o claim falso de framebuffer, e um guard de `vkDeviceWaitIdle` faltante em `VK_TextureUpdateDescriptor` (mesma classe de bug: atualizar descriptor set que pode estar em uso por command buffer ainda em voo). Tudo validado com `-dev -condebug +set vid_renderer 2 +map dm3` antes e depois — confirmado que o erro de validação desaparece. Ver commit "Vulkan: port validation/correctness fixes from the sdl3-vulkan-pr branch".

O trabalho de MSAA iniciado antes dessa correção de estratégia (campos `msaaSamples`/`msaaColorImage*` em `vk_local.h`, parâmetro `samples` em `VK_CreateImageResource`, `VK_CreateSwapChainMSAAColorResources`/`VK_DestroySwapChainMSAAColorResources` em `vk_swapchain.c`) continua **não commitado e incompleto** (não plugado no framebuffer/render pass/pipelines) — decidir antes da próxima sessão se continua aqui ou se deveria ter sido feito em `sdl3-vulkan-pr` primeiro, seguindo a estratégia acima.

Atualize este arquivo quando uma decisão importante, regressão, correção confirmada ou mudança de fluxo tornar alguma seção obsoleta. Não registre senhas, tokens, chaves de assinatura privadas ou dados proprietários.
