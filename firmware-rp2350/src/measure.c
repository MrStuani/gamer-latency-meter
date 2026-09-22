#include "measure.h"

#include <string.h>
#include <limits.h>

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

#include "timestamp.pio.h"

/* The SDK's pio_install_program is fine, but these we add manually for both
 * variants (rising/falling) at init so switching polarity is a cheap jmp.  */
#define BASE_INIT    0x80000000u   /* pass de até ~2^31 iterações (~28 s)   */

typedef enum {
    M_IDLE,
    M_ARMED,       /* SMs contando, espera borda de T0                       */
    M_T0DET,       /* T0 capturado, espera T1 da eixo em teste               */
    M_HOLDOFF,     /* debounce negativo pós medição / timeout / remodo       */
} fsm_state_t;

/* ------------------------------------------------------------------ state */
static measure_channel_t ch_t0;
static measure_channel_t ch_t1_click;
static measure_channel_t ch_t1_motion;

static measure_config_t  cfg;
static fsm_state_t       st = M_IDLE;
static bool              running = false;

static uint32_t t0_pending_ts;
static uint32_t ev_at;            /* time_us_32 de entrada no estado atual   */
static uint32_t holdoff_end;

/* stats */
static uint32_t stat_seq_total;
static uint32_t stat_seq_mode[2];
static uint32_t stat_timeouts;
static uint32_t stat_dropped;
static uint32_t stat_stray_t1;
static uint32_t stat_order;
static uint32_t ring_peak;

static uint r_offset_rising, r_offset_falling;

/* ------------------------------------------------------------------ rings */
static inline uint32_t ring_avail(measure_channel_t *ch)
{
    uint32_t wa = dma_hw->ch[ch->dma_ch].write_addr;
    uint32_t off = (wa - (uint32_t)ch->ring.data) / 4u;
    return (off - ch->ring.head) & RING_ENTRIES_MASK;
}

static inline uint32_t ring_pop(measure_channel_t *ch)
{
    uint32_t v = ch->ring.data[ch->ring.head & RING_ENTRIES_MASK];
    ch->ring.head = (ch->ring.head + 1u) & RING_ENTRIES_MASK;
    return v;
}

static void ring_reset(measure_channel_t *ch)
{
    ch->ring.head = 0;
}

/* ------------------------------------------------------------------ DSP   */
static uint32_t now_us(void)
{
    return time_us_32();
}

/* ------------------------------------------------------------------ acting opinião
 * Converte delta de contagens PIO em ns. Cada iteração do loop = 2 ciclos.   */
static uint32_t counts_to_ns(uint32_t delta_counts)
{
    uint64_t cycles = (uint64_t)delta_counts * PIO_LOOP_CYCLES;
    uint64_t ns = cycles * 1000000000ull / clock_get_hz(clk_sys);
    return (uint32_t)ns;
}

/* ------------------------------------------------------------------ PIO   */

static void ch_ring_dma_start(measure_channel_t *ch)
{
    int chno = ch->dma_ch;
    dma_channel_config c = dma_channel_get_default_config(chno);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    /* wrap do endereço de escrita dentro do ring (4 KiB = 1024 x u32)        */
    channel_config_set_ring(&c, true, DMA_RING_LOG2 + 2u);
    channel_config_set_dreq(&c, pio_get_dreq(ch->pio, ch->sm, false));

    /* auto-reload infinito (chain -> ele mesmo)                              */
    channel_config_set_chain_to(&c, chno);

    dma_channel_configure(
        chno, &c,
        ch->ring.data,                       /* write */
        &ch->pio->rxf[ch->sm],               /* read (RX FIFO do SM)  */
        0,                                    /* count = 0 -> chain reload */
        true);
}

/* Arma uma SM: joga ela para `pull block` (início do programa) e injeta a
 * base. Retorna quando o valor foi consumido pelo pull. */
static void ch_arm(measure_channel_t *ch, uint32_t base)
{
    uint prog = (ch == &ch_t0) ? cfg.t0_edge : M_EDGE_RISING;
    uint off  = (prog == M_EDGE_RISING) ? r_offset_rising : r_offset_falling;
    pio_sm_exec(ch->pio, ch->sm, pio_encode_jmp(off));

    /* Nota: pio_sm_put_blocking garant o pull ser consumido; com o SM
     * estacionado no pull block isso acontece imediatamente. */
    pio_sm_put_blocking(ch->pio, ch->sm, base);
}

/* Arma os três SMs quase simultaneamente (base única, lockstep).           */
static void arm_all(uint32_t base)
{
    uint32_t saved = save_and_disable_interrupts();
    ch_arm(&ch_t0, base);
    ch_arm(&ch_t1_click, base);
    ch_arm(&ch_t1_motion, base);
    restore_interrupts(saved);
}

static void sm_start(measure_channel_t *ch, uint pin, uint prog_off)
{
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_in_pins(&c, pin);
    sm_config_set_clkdiv(&c, PIO_CLKDIV);
    pio_sm_init(ch->pio, ch->sm, prog_off, &c);
    pio_sm_set_enabled(ch->pio, ch->sm, true);
}

/* ------------------------------------------------------------------ public */
static measure_channel_t *active_t1_ch(void)
{
    return (cfg.mode == M_MODE_CLICK) ? &ch_t1_click : &ch_t1_motion;
}

static measure_channel_t *idle_t1_ch(void)
{
    return (cfg.mode == M_MODE_CLICK) ? &ch_t1_motion : &ch_t1_click;
}

static bool t0_at_rest(void)
{
    int lvl = gpio_get(PIN_T0);
    return (cfg.t0_edge == M_EDGE_RISING) ? (lvl == 0) : (lvl == 1);
}

void measure_config_defaults(measure_config_t *out)
{
    out->t0_pull    = M_T0_FLOAT;
    out->t0_edge    = M_EDGE_RISING;
    out->mode       = M_MODE_CLICK;
    out->holdoff_us = DEFAULT_HOLDOFF_US;
    out->timeout_us = DEFAULT_TIMEOUT_US;
}

void measure_get_config(measure_config_t *out) { *out = cfg; }

void measure_init(void)
{
    cfg = (measure_config_t){0};
    measure_config_defaults(&cfg);

    PIO pio = PIO_MEASURE;

    ch_t0           = (measure_channel_t){ .pio = pio, .sm = SM_T0 };
    ch_t1_click     = (measure_channel_t){ .pio = pio, .sm = SM_T1_CLICK };
    ch_t1_motion    = (measure_channel_t){ .pio = pio, .sm = SM_T1_MOTION };

    r_offset_rising  = pio_add_program(pio, &timestamp_rising_program);
    r_offset_falling = pio_add_program(pio, &timestamp_falling_program);

    ch_t0.dma_ch = dma_claim_unused_channel(true);
    ch_t1_click.dma_ch = dma_claim_unused_channel(true);
    ch_t1_motion.dma_ch = dma_claim_unused_channel(true);

    gpio_init(PIN_T0);
    gpio_init(PIN_T1_CLICK);
    gpio_init(PIN_T1_MOTION);

    sm_start(&ch_t0,        PIN_T0,        r_offset_rising);
    sm_start(&ch_t1_click,  PIN_T1_CLICK,  r_offset_rising);
    sm_start(&ch_t1_motion, PIN_T1_MOTION, r_offset_rising);

    ch_ring_dma_start(&ch_t0);
    ch_ring_dma_start(&ch_t1_click);
    ch_ring_dma_start(&ch_t1_motion);

    measure_apply_config(&cfg);
}

void measure_apply_config(const measure_config_t *newcfg)
{
    /* metade pública: reseta estado ativo e rings (dados de borda antigos
     * se tornam inválidos quando a polaridade muda)                        */
    cfg = *newcfg;

    /* pull elétrico do pino T0 */
    switch (cfg.t0_pull) {
        case M_T0_PULL_UP:   gpio_pull_up(PIN_T0);   break;
        case M_T0_PULL_DOWN: gpio_pull_down(PIN_T0); break;
        default:             gpio_disable_pulls(PIN_T0); break;
    }

    /* pizza: caso T0 já esteja armado, reposiciona o SM do T0 no programa
     * (rising/falling) e reentra no ciclo de rearme via HOLDOFF           */
    if (running) {
        st = M_HOLDOFF;
        holdoff_end = 0;          /* primeiro poll dispara a rearme          */
    } else {
        st = M_IDLE;
    }
    ring_reset(&ch_t0);
    ring_reset(&ch_t1_click);
    ring_reset(&ch_t1_motion);
}

void measure_start(void)
{
    running = true;
    st = M_HOLDOFF;
    holdoff_end = 0;               /* rearma no primeiro poll                 */
}

void measure_stop(void)
{
    running = false;
    st = M_IDLE;
    ring_reset(&ch_t0);
    ring_reset(&ch_t1_click);
    ring_reset(&ch_t1_motion);
}

bool measure_poll(measure_event_t *ev)
{
    uint32_t now = now_us();
    measure_channel_t *act = active_t1_ch();
    measure_channel_t *idl = idle_t1_ch();

    /* --- watchdog --------------------------------------------------------- */
    if (running && st == M_T0DET && (now - ev_at) > cfg.timeout_us) {
        stat_timeouts++;
        ev->type  = EV_TIMEOUT;
        ev->mode  = cfg.mode;
        ev->seq   = stat_seq_total;
        ev->t0_ts = t0_pending_ts;
        ev->t1_ts = 0;
        ev->lat_ns = 0;
        ev->flags = 0;
        st = M_HOLDOFF;
        holdoff_end = now + cfg.holdoff_us;
        return true;
    }

    /* --- eventos T0 ------------------------------------------------------- */
    for (uint32_t n = ring_avail(&ch_t0); n; n--) {
        uint32_t ts = ring_pop(&ch_t0);
        if (st == M_ARMED) {
            t0_pending_ts = ts;
            st = M_T0DET;
            ev_at = now;
        } else if (st == M_T0DET) {
            /* novo T0 chegou antes do T1 do pass vigente: perde o anterior  */
            stat_dropped++;
            ev->type   = EV_DROPPED;
            ev->mode   = cfg.mode;
            ev->seq    = stat_seq_total;
            ev->t0_ts  = t0_pending_ts;
            ev->t1_ts  = 0;
            ev->lat_ns = 0;
            ev->flags  = 0;
            t0_pending_ts = ts;           /* mantém o mais recente           */
            ev_at = now;
            return true;
        }
        /* ARMED fora (HOLDOFF/IDLE): descarta silencioso                    */
    }

    /* --- T1 da eixo em teste ---------------------------------------------- */
    uint32_t n_act = ring_avail(act);
    if (n_act > ring_peak) ring_peak = n_act;
    for (uint32_t n = n_act; n; n--) {
        uint32_t ts = ring_pop(act);
        if (st != M_T0DET) {
            stat_stray_t1++;
            continue;
        }
        if (ts > t0_pending_ts || (t0_pending_ts - ts) > (uint32_t)(INT32_MAX)) {
            /* t1 menor => depois no tempo; ordem invertida só se t1 for
             * absolutamente posterior? ver guarda acima. Ordem inválida acontece
             * se os contadores divergirem (falha de lockstep) -> ev_ORDER    */
            stat_order++;
            ev->type   = EV_ORDER;
            ev->mode   = cfg.mode;
            ev->seq    = stat_seq_total;
            ev->t0_ts  = t0_pending_ts;
            ev->t1_ts  = ts;
            ev->lat_ns = 0;
            ev->flags  = 0;
            st = M_HOLDOFF;
            holdoff_end = now + cfg.holdoff_us;
            return true;
        }
        uint32_t d  = t0_pending_ts - ts;
        uint32_t ns = counts_to_ns(d);

        stat_seq_total++;
        stat_seq_mode[cfg.mode]++;

        ev->type   = EV_MEASURE;
        ev->mode   = cfg.mode;
        ev->seq    = stat_seq_total;
        ev->t0_ts  = t0_pending_ts;
        ev->t1_ts  = ts;
        ev->lat_ns = ns;
        ev->flags  = 0;

        st = M_HOLDOFF;
        holdoff_end = now + cfg.holdoff_us;
        return true;
    }

    /* --- T1 do eixo inativo (crosstalk) ------------------------------------ */
    for (uint32_t n = ring_avail(idl); n; n--) {
        (void)ring_pop(idl);
        /* descartado de propósito: não perturba a medição do eixo ativo     */
    }

    /* --- saída do HOLDOFF (rearme) ---------------------------------------- */
    if (running && st == M_HOLDOFF && holdoff_end <= now) {
        if (!t0_at_rest()) return false;   /* espera repouso físico do T0     */
        st = M_ARMED;
        holdoff_end = 0;
        arm_all(BASE_INIT);
    }

    return false;
}

uint32_t measure_seq_total(void)   { return stat_seq_total; }
uint32_t measure_seq_per_mode(mode_t m) { return stat_seq_mode[m]; }
uint32_t measure_timeouts(void)    { return stat_timeouts; }
bool measure_running(void)     { return running; }

int measure_ring_used_max(void)
{
    return (int)ring_peak;
}