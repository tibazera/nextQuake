# Agentes do nextQuake

## Fonte de verdade e escopo

- Fonte local: E:\Projetos Linux\nextQuake.
- Repositorio: tibazera/nextQuake; branch principal: main.
- Plataformas alvo: somente Android e iOS. Desktop nao e foco de desenvolvimento.
- Base importada: ezQuake, commit 52fab4df. Preserve historico, licenca e autoria.
- O projeto original E:\Projetos Linux\ezquake-source permanece independente. Nao altere seus remotes nem o worktree desktop para tarefas do nextQuake.

## Antes de editar

Leia este arquivo, continue.md e docs/android-pocket-vulkan-handoff.md. Inspecione git status, branch e diffs. O worktree local pode conter avances ainda nao commitados; preserve-os. Nao trabalhe simultaneamente com outro assistente no mesmo checkout.

Nao use reset --hard, checkout --, clean ou equivalentes. Nao faca commit/push/PR sem pedido atual de Tiago. Nao envie nada ao upstream sem autorizacao explicita.

## Estado a preservar

Android SDL3/Vulkan/Oboe e touch sao a base funcional. Esquerdo move, direito mira; toque simples mira, duplo toque dispara; FIRE permite arrastar mirando. Preserve console inicial, KBD e IME, L/R e as correcoes de sincronizacao Vulkan. Outline e opcional e tem custo de performance.

iOS ainda nao foi implementado. Nao afirme suporte ou funcionamento antes de compilar e validar. Reaproveite SDL e o renderer via MoltenVK; nao substitua o motor.

## Validacao e dados

Depois de alterar codigo nativo, Java, CMake ou dependencias, execute ao menos o build Android Debug. Remova instrumentacao temporaria. Diferencie resultados comprovados de hipoteses; nao tire screenshots automaticamente. Dados Quake e configs do usuario nao devem ser apagados nem versionados.

Atualize continue.md e o handoff quando houver avancos relevantes. O historico operacional anterior esta em docs/ezquake-origin-agent-history.md; seus caminhos e prioridades antigos nao substituem estas regras.
