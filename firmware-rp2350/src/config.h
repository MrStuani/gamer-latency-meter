#ifndef CONFIG_H
#define CONFIG_H

/* =========================================================================
 * Medidor de latência de periférico gamer — RP2350 (Raspberry Pi Pico 2)
 *
 * Pino único T0 (ground truth / ação física), rewireado manualmente pelo
 * usuário conforme o teste (ver README). Os pinos T1_CLICK/T1_MOTION vêm do
 * circuito "tradutor" (fora de escopo, já existe).
 *
 * IMPORTANTE (validação por osciloscópio antes de travar números):
 *  A resolução de timestamp aqui é de ~2 ciclos PIO. Com PIO a 250 MHz isso
 *  dá ~8 ns por passo e um pulso de motion mínimo detectável de ~16 ns —
 *  suficiente para o pulso de ~10 ns estimado no sensor. Meça no osciloscópio
 *  e ajuste SYSTEM_CLOCK_KHZ se precisar.
 * ========================================================================= */

/* --- Indentificação / build ------------------------------------------- */
#define FW_VERSION_MAJOR     1
#define FW_VERSION_MINOR     0
#define FW_VERSION_PATCH     0

/* --- Relógio do sistema ------------------------------------------------ */
/* RP2350: 0 = deixa o bootrom em 150 MHz (default). Para tentar capturar
 * pulsos de ~10 ns, aumente (ex: 200000 ou 250000). PIO roda no system clock
 * (CLKDIV = 1), então a resolução acompanha. Meça no osciloscópio. */
/* NOTA RP2350: 0 = deixa o bootrom em 150 MHz (default). Para capturar
 * pulsos de ~10 ns, aumente (ex: 200000 ou 250000). PIO roda no system clock
 * (CLKDIV = 1), então a resolução acompanha. Meça no osciloscópio. */
#define SYSTEM_CLOCK_KHZ    250000u
#define PIO_CLKDIV           1.0f

/* --- Pinagem (GPIOs livres no Pico 2; evite 26-29 = ADC, 2/3 USB) -------- */
#define PIN_T0_MODERADO     2   /* se mudar, mude também PIN_T0_* abaixo   */
#define PIN_T0              2   /* ground truth (rewireado pelo usuário)   */
#define PIN_T1_CLICK        3   /* saída CLICK do tradutor                 */
#define PIN_T1_MOTION       4   /* saída MOTION do tradutor                */

/* --- PIO / SMs ---------------------------------------------------------- */
#define PIO_MEASURE         pio0
#define SM_T0               0
#define SM_T1_CLICK         1
#define SM_T1_MOTION        2
#define SM_COUNT            3

/* --- DMA: ring buffer por SM (alta taxa de eventos) ---------------------- */
#define DMA_RING_SIZE       1024u   /* entradas (u32) por SM, potência de 2 */
#define DMA_RING_LOG2       10u
/* Canais DMA usados (0-11 no RP2350) */
#define DMA_CH_T0           0
#define DMA_CH_T1_CLICK     1
#define DMA_CH_T1_MOTION    2

/* --- Protocolo serial / tempos ------------------------------------------ */
/* Unidades para holdoff e watchdog (em µs). Valores default sensatos,
 * configuráveis em runtime via comando CONFIG. */
#define DEFAULT_HOLDOFF_US  2000u
#define DEFAULT_TIMEOUT_US  100000u   /* 100 ms é muito tempo p/ mouse */

/* Nop deste loop PIO: 2 ciclos por iteração (ver pio/timestamp.pio).
 * Fator de conversão contagem->ciclos aplicado no cálculo de latência. */
#define PIO_LOOP_CYCLES     2u
#define PIO_TIMESTAMP_PERIOD_CYCLES PIO_LOOP_CYCLES

#endif /* CONFIG_H */