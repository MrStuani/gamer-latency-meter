#ifndef MEASURE_H
#define MEASURE_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/pio.h"
#include "pico/stdlib.h"

#include "config.h"

/* =========================================================================
 * Núcleo do medidor de latência.
 *
 * FSM (uma medição por vez):
 *   IDLE -> START(revoca base) -> ARMED(espera borda T0)
 *        -> T0_DETECTED(espera borda do T1 da eixo em teste)
 *        -> MEASURED / TIMEOUT -> HOLDOFF -> (repouso T0?) -> ARMED
 *
 * O pareamento de eventos NEUTRALIZA crosstalk entre eixos:
 *  - modo CLICK  : casa T0 com a borda do T1_CLICK ; ignora T1_MOTION
 *  - modo MOTION : casa T0 com a borda do T1_MOTION; ignora T1_CLICK
 * (bordas do eixo inativo são descartadas, não travam a medição).
 *
 * A latência é calculada com os timestamps PIO (lockstep) dos SMs T0 e T1:
 *   lat_cycles = (t0_ts - t1_ts) * PIO_LOOP_CYCLES   (t1 chegou depois de t0
 *                                                     -> t1_ts menor)
 *   lat_ns     = lat_cycles * 1e9 / SYSTEM_CLOCK
 *
 * Arme só ocorre com o pino T0 em repouso; holdoff e watchdog são
 * configuráveis (ver protocolo em docs/protocol.md).
 * ========================================================================= */

/* --- Configuração ------------------------------------------------------- */
typedef enum { M_T0_FLOAT = 0, M_T0_PULL_UP, M_T0_PULL_DOWN } t0_pull_t;
typedef enum { M_EDGE_RISING = 0, M_EDGE_FALLING } edge_t;
typedef enum { M_MODE_CLICK = 0, M_MODE_MOTION } mode_t;

typedef struct {
    t0_pull_t t0_pull;      /* como o driver físico do T0 é ligado            */
    edge_t    t0_edge;      /* a qual borda do T0 corresponde a "ação"        */
    mode_t    mode;         /* eixo em teste                                  */
    uint32_t  holdoff_us;   /* debounce negativo pós-medição (µs)             */
    uint32_t  timeout_us;   /* watchdog: sem T1 após T0 -> TIMEOUT (µs)       */
} measure_config_t;

/* --- Eventos de saída ---------------------------------------------------- */
typedef enum {
    EV_MEASURE = 0,   /* latência válida                                     */
    EV_TIMEOUT,       /* watchdog estourou (T0 detectado, T1 não veio)       */
    EV_ORDER,         /* t1 chegou antes do t0 (contra-ordem no par)         */
    EV_DROPPED,       /* novo T0 chegou antes do T1 do pass vigente (drop)   */
} measure_event_type_t;

typedef struct {
    measure_event_type_t type;
    mode_t  mode;
    uint32_t seq;      /* sequencial de medições (u32)                        */
    uint32_t t0_ts;    /* timestamp PIO de T0 (contador lockstep)             */
    uint32_t t1_ts;    /* timestamp PIO do T1 da eixo em teste                */
    uint32_t lat_ns;   /* 0 nos eventos não-medida (timeout/order/drop)       */
    uint32_t flags;    /* bit0 ~reservado                                     */
} measure_event_t;

/* --- DMA / rings --------------------------------------------------------- */
#define RING_ENTRIES      DMA_RING_SIZE
#define RING_ENTRIES_MASK (DMA_RING_SIZE - 1u)

typedef struct {
    uint32_t data[RING_ENTRIES] __attribute__((aligned(RING_ENTRIES * 4)));
    unsigned head;              /* índice do próximo a consumir               */
    uint32_t dma_write_off;     /* offset(write_addr - base)/4 (cache)        */
} measure_ring_t;

typedef struct {
    measure_ring_t  ring;
    /* campos internos PIO/DMA preenchidos por measure_init */
    PIO pio;
    uint8_t  sm;
    int      dma_ch;
    uint     prog_offset;
} measure_channel_t;

/* --- API ------------------------------------------------------------------ */
void measure_init(void);
void measure_config_defaults(measure_config_t *cfg);
void measure_get_config(measure_config_t *cfg);
void measure_apply_config(const measure_config_t *cfg);   /* rearma se ativo */
void measure_start(void);
void measure_stop(void);

/* Avança a FSM, drenando os rings. Retorna 1 se preencheu *ev. */
bool measure_poll(measure_event_t *ev);

uint32_t measure_seq_total(void);
uint32_t measure_seq_per_mode(mode_t m);
uint32_t measure_timeouts(void);

/* Acesso para diagnósticos (protocolo STAT). */
bool  measure_running(void);
int   measure_ring_used_max(void);          /* pico de ocupação de ring       */

#endif /* MEASURE_H */