# Projetos bitDogLab — Fase 1 (EmbarcaTech)

Repositório com os projetos desenvolvidos na Fase 1 do programa EmbarcaTech pelo bitDogLab.

## Conteúdo
- `estacionamento_http_bitdoglab/` — protótipo de controle de estacionamento com interface HTTP (Raspberry Pi Pico W, lwIP).
- `menu_display_bitdoglab/` — projeto de menu / display (Pico).
- `semaforo_bitdoglab/` — semáforo físico implementado com Pico (GPIO / timers / PWM).
- `semaforo_wokwi/` — versão/simulação para Wokwi (simulador online).

## Como preparar o repo para publicar
1. Remover pastas de `build/` e arquivos binários gerados (ex.: `.o`, `.elf`, `.bin`, `build/`) antes de subir.
2. Adicionar `.gitignore` para projetos C / Pico (ex.: `build/`, `.vscode/`, `*.o`, `*.elf`, `*.bin`, `*.uf2`).
3. Adicionar um `LICENSE` (recomendo MIT se quiser abrir livremente).
4. Criar instruções de compilação por projeto (ex.: dependências do Pico SDK, comandos `cmake`/`make`).

## Licença
MIT (ou escolha outra que prefira)

