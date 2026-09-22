# Protocolo e arquitetura — Medidor de Latência RP2350

## Arquitetura em camadas

```
┌─────────────┐   T0   ┌────────────────────────────┐  USB-CDC   ┌──────────┐
│ física/user │ ──────▶│  medidor RP2350 (Pico 2)  │ ◀─────────▶│  página  │
│ "tradutor"  │        │  PIO 3×SM + DMA + FSM     │   WebSerial│   web    │
│             │T1_CLICK│  0xAA <16-byte frames>    │ ─────────▶│          │
│             │T1_MOTN┘└────────────────────────────┘           └──────────┘
```

- **T0** — ground truth: sinal físico (chave/contato mecânico) rewireado pelo
  usuário conforme o teste.
- **T1_CLICK / T1_MOTION** — saídas do circuito "tradutor" (ex: firmware
  CH32V307 que emite pulso/level a partir do relatório HID).
- O **RP2350** timestamps as bordas de **T0, T1_CLICK, T1_MOTION** via 3 SMs
  PIO (captura de borda; nunca GPIO IRQ — ver `edge_ts.pio`) e calcula
  a latência **T1 − T0**.

## Timebase (precisão)

Cada SM PIO mantém um contador X de 32 bits (down counter). Todas as SMs
recebem a **mesma base** a cada arme (`pull block`), o programa é idêntico em
estrutura (2 ciclos/iteração) nas variantes rising/falling → os contadores
permanecem em *lockstep*. Latência em contagens:

```
lat_contagens = t0_ts − t1_ts          (t1 chegou depois → X menor)
lat_ciclos    = lat_contagens × 2      (loop PIO = 2 ciclos)
lat_ns        = lat_ciclos × 1e9 / f_clk
```

| Parâmetro                    | Valor                     |
|------------------------------|---------------------------|
| Relógio do sistema (default) | 250 MHz (RP2350)          |
| Resolução                    | 2 ciclos PIO              |
| Resolução @250 MHz           | ~8 ns                     |
| Pulso mínimo detectável      | ~2 ciclos (~16 ns @250 MHz)|
| Skew de arme entre SMs       | < ~200 ns (ver README: scope) |

> ⚠️ Pulso de motion do sensor é estimado em **~10 ns**. Muito perto do
> limiar de ~26 ns a 150 MHz. **Antes de confiar em números**, alimente o
> T1_MOTION com um gerador de pulso de referência no osciloscópio e ajuste
> o clock do sistema (200–250 MHz) se precisar.

## Protocolo serial (CDC 115200-8N1, Teensy-style binary + texto)

### host → device (linhas ASCII, terminadas em `\n`)

```
CONF <t0pull> <edge> <mode> <holdoff_us> <timeout_us>
    t0pull     f | up | down
    edge       r | f            (qual borda do T0 conta como "ação")
    mode       c | m            (eixo em teste: c=CLICK  m=MOTION)
    holdoff_us µs (debounce negativo pós-medição, default 2000)
    timeout_us µs (watchdog T0→T1,        default 100000)

START          (rearma os SMs e inicia medição)
STOP           (congela, limpa filas locais)
STAT           (status/contadores em texto, prefixo '#')
VERSION        (versão do firmware)
RESET          (volta à config default)
```

Exemplos:
```
CONF f r c 2000 100000
CONF up f m 2000 100000
START
```

### device → host

- Texto (respostas a comandos) começa com `#` e termina em `\n`
  (`#ACK ...`, `#STAT ...`, `#ERR ...`, `#VER ...`).
- **Frames binários fixos de 20 bytes**, primeiro byte `0xAA` (little-endian):

```
offset tamanho campo
0      1      magic 0xAA
1      1      type   0=MEASURE 1=TIMEOUT 2=ORDER 3=DROPPED
2      1      mode   0=click  1=motion
3      1      flags  (reservado)
4      4      seq    (u32, sequencial da medição)
8      4      t0_ts  (u32, contador PIO do T0)
12     4      t1_ts  (u32, contador PIO do T1 ativo)
16     4      lat_ns (u32)
```

- **MEASURE** — latência válida. `lat_ns = (t0_ts − t1_ts)×2 ciclos`.
- **TIMEOUT** — T0 foi detectado e o T1 ativo não veio dentro de `timeout_us`
  (watchdog); medição abortada, rearme após holdoff.
- **ORDER** — T1 chegou *antes* do T0 correspondente (evento espúrio /
  lockstep quebrado). Ignorar/logar.
- **DROPPED** — novo T0 chegou antes do T1 do pass vigente; o par anterior foi
  descartado (mantém o T0 mais recente). Sinal de eventos muito rápidos.

## Comportamento do par (neutraliza crosstalk)

Em cada modo **apenas o T1 do eixo em teste** casa com o T0:

| Mode   | casa T0 com | ignora (descarta) |
|--------|-------------|-------------------|
| CLICK  | T1_CLICK    | T1_MOTION (crosstalk de sensor) |
| MOTION | T1_MOTION   | T1_CLICK          |

Eventos do eixo inativo são drenados e não geram medição nem travam a FSM.

## FSM

```
IDLE ─START─▶ HOLDOFF(expira) ─▶ (repouso T0?) ─▶ ARMED ─T0─▶ T0DET
T0DET ─▶ T1_ativo ─▶ MEASURE ─▶ HOLDOFF ─▶ ...
T0DET ─▶ timeout ─▶ TIMEOUT ─▶ HOLDOFF ─▶ ...
T0DET ─▶ novo T0 ─▶ DROPPED (mantém T0 recente)
```

- **HOLDOFF**: debounce negativo; nenhum evento é aceito durante este período
  (pulso de retorno do switch não vira nova medição).
- **Repouso T0**: pós-holdoff, só rearma se o pino estiver no nível de repouso
  (LOW para `edge=r`; HIGH para `edge=f`).

## Estatísticas expostas (`STAT`)

`seq_total`, `click`, `motion`, `timeouts`, `ring_peak`, config ativa e clock.

## Pinos (Pico 2)

| Pino GPIO | Função          |
|-----------|-----------------|
| GP2       | T0 (ground truth, rewireável) |
| GP3       | T1_CLICK (saída do tradutor)  |
| GP4       | T1_MOTION (saída do tradutor) |
| USB       | CDC (config/dados)            |

Alterações de pino/especificações nos `#define` de `main.c`.