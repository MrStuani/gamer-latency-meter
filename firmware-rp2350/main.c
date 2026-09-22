/*
 * medidor-rp2350 v1.0.0
 * Firmware do medidor de latencia — fala com a pagina WebSerial (app.js).
 *
 * Pinos (troque nos #defines):
 *   GP2 = T0         (acao fisica: chave / fotodiodo + comparador)
 *   GP3 = T1_CLICK   (saida de clique do CH32V307)
 *   GP4 = T1_MOTION  (saida de movimento do CH32V307)
 *   GND comum com o CH32V307.
 *
 * Arquitetura:
 *   PIO0: 3 SMs (uma por canal) iniciadas em sincronia, cada uma com contador
 *         decrescente livre -> timestamps na mesma base de tempo (ver edge_ts.pio).
 *   Core 0: le os FIFOs, pareia T0->T1, detecta TIMEOUT / ORDER / DROPPED.
 *   Core 1: USB CDC (stdio_usb), parser de comandos, envia frames binarios e linhas '#'.
 *
 * Frame binario (20 bytes, little-endian) — o que o app.js espera:
 *   [0]=0xAA [1]=tipo (0 amostra, 1 timeout, 2 order, 3 dropped) [2]=modo (0 click, 1 motion)
 *   [3]=0 [4..7]=seq [8..11]=t0 [12..15]=t1 [16..19]=lat (ns)
 *   t0/t1 em contagens crescentes (~Xpio), 1 contagem = 2 ciclos de PIO.
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/stdio_usb.h"
#include "pico/stdio/driver.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "hardware/sync.h"
#include "edge_ts.pio.h"

#define FW_VERSION      "medidor-rp2350 v1.0.0"

#define PIN_T0          2
#define PIN_T1_CLICK    3
#define PIN_T1_MOTION   4
#define T1_RISING       1      /* 1 = T1 marca na subida, 0 = na descida */

#define FRAME_LEN       20
#define RING_N          512    /* frames; potencia de 2 */

enum { SM_T0 = 0, SM_CLK = 1, SM_MOT = 2 };
enum { PULL_FLOAT, PULL_UP, PULL_DOWN };
enum { CMD_CONF = 1, CMD_START = 2, CMD_STOP = 4, CMD_RESET = 8 };
enum { FT_SAMPLE = 0, FT_TIMEOUT = 1, FT_ORDER = 2, FT_DROPPED = 3 };

typedef struct {
    uint8_t  t0pull;
    bool     t0_rising;
    uint8_t  mode;             /* 0 = click, 1 = motion */
    uint32_t holdoff_us;
    uint32_t timeout_us;
} cfg_t;

static const cfg_t CFG_DEFAULT = { PULL_FLOAT, true, 0, 2000, 100000 };

/* ---------------- estado (core 0 dono; core 1 so le stats) ---------------- */
static PIO  pio = pio0;
static uint off_rise, off_fall;

static cfg_t cfg = { PULL_FLOAT, true, 0, 2000, 100000 };
static cfg_t cfg_new;                          /* escrito pelo core 1 */
static volatile uint32_t g_cmd;
static volatile bool     g_running;

static uint32_t holdoff_ticks, hz_sys;

static bool     pend, have_last;
static uint32_t pend_t0, pend_seq, pend_us, last_t0;

static volatile uint32_t st_seq, st_ok, st_to, st_drop, st_order, st_stray, st_fifo_full, st_ring_ovf;

/* ---------------- ring core0 -> core1 ---------------- */
static uint8_t ring[RING_N][FRAME_LEN];
static volatile uint32_t ring_wr, ring_rd;

static void frame_put(uint8_t type, uint8_t mode, uint32_t seq, uint32_t t0, uint32_t t1, uint32_t lat) {
    uint32_t w = ring_wr;
    if (w - ring_rd >= RING_N) { st_ring_ovf++; return; }
    uint8_t *f = ring[w % RING_N];
    f[0] = 0xAA; f[1] = type; f[2] = mode; f[3] = 0;
    memcpy(f + 4,  &seq, 4);
    memcpy(f + 8,  &t0,  4);
    memcpy(f + 12, &t1,  4);
    memcpy(f + 16, &lat, 4);
    __dmb();
    ring_wr = w + 1;
}

/* ---------------- hardware ---------------- */
static const uint sm_pin[3] = { PIN_T0, PIN_T1_CLICK, PIN_T1_MOTION };

static void update_timing(void) {
    hz_sys = clock_get_hz(clk_sys);
    holdoff_ticks = (uint32_t)((uint64_t)cfg.holdoff_us * hz_sys / 2000000ull);   /* 1 tick = 2 ciclos */
}

static inline uint32_t ticks_to_ns(uint32_t dt) {
    uint64_t ns = (uint64_t)dt * 2000000000ull / hz_sys;
    return ns > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)ns;
}

static void sm_setup(uint sm, uint pin, bool rising) {
    uint off = rising ? off_rise : off_fall;
    pio_sm_config c = rising ? edge_rise_program_get_default_config(off)
                             : edge_fall_program_get_default_config(off);
    sm_config_set_jmp_pin(&c, pin);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);      /* RX FIFO de 8 palavras */
    sm_config_set_clkdiv(&c, 1.0f);

    bool hi = gpio_get(pin);                            /* comeca no estado certo: sem evento falso */
    uint rel = rising ? (hi ? edge_rise_offset_hi : edge_rise_offset_lo)
                      : (hi ? edge_fall_offset_hi : edge_fall_offset_lo);
    pio_sm_init(pio, sm, off + rel, &c);                /* limpa FIFOs, seta PC */
    pio_sm_exec(pio, sm, pio_encode_mov_not(pio_x, pio_null));   /* X = 0xFFFFFFFF */
}

static void hw_restart(bool enable) {
    pio_set_sm_mask_enabled(pio, 0x7, false);

    for (int i = 0; i < 3; i++) {
        pio_gpio_init(pio, sm_pin[i]);
        pio_sm_set_consecutive_pindirs(pio, i, sm_pin[i], 1, false);
        gpio_disable_pulls(sm_pin[i]);
    }
    if (cfg.t0pull == PULL_UP)   gpio_pull_up(PIN_T0);
    if (cfg.t0pull == PULL_DOWN) gpio_pull_down(PIN_T0);
    busy_wait_us(200);                                   /* deixa o nivel assentar antes de ler */

    sm_setup(SM_T0,  PIN_T0,        cfg.t0_rising);
    sm_setup(SM_CLK, PIN_T1_CLICK,  T1_RISING);
    sm_setup(SM_MOT, PIN_T1_MOTION, T1_RISING);

    pend = false; have_last = false;
    if (enable) pio_enable_sm_mask_in_sync(pio, 0x7);    /* mesma base de tempo nas 3 SMs */
}

/* ---------------- pareamento (core 0) ---------------- */
static void on_t0(uint32_t t) {
    if (have_last && (uint32_t)(t - last_t0) < holdoff_ticks) return;   /* debounce */
    if (pend) { frame_put(FT_DROPPED, cfg.mode, pend_seq, pend_t0, 0, 0); st_drop++; }
    last_t0 = t; have_last = true;
    pend = true; pend_t0 = t; pend_seq = ++st_seq; pend_us = time_us_32();
}

static void on_t1(uint32_t t) {
    if (!pend) { st_stray++; return; }
    uint32_t dt = t - pend_t0;
    pend = false;
    if (dt >= 0x80000000u) {                                            /* T1 antes de T0 */
        frame_put(FT_ORDER, cfg.mode, pend_seq, pend_t0, t, 0); st_order++; return;
    }
    frame_put(FT_SAMPLE, cfg.mode, pend_seq, pend_t0, t, ticks_to_ns(dt));
    st_ok++;
}

static void clear_stats(void) {
    st_seq = st_ok = st_to = st_drop = st_order = st_stray = st_fifo_full = st_ring_ovf = 0;
}

static void handle_cmds(uint32_t c) {
    if (c & CMD_RESET) {
        cfg = CFG_DEFAULT; g_running = false; clear_stats();
        update_timing(); hw_restart(false);
    }
    if (c & CMD_CONF) {
        cfg = cfg_new; update_timing(); hw_restart(g_running);
    }
    if (c & CMD_STOP) {
        g_running = false; pio_set_sm_mask_enabled(pio, 0x7, false); pend = false;
    }
    if (c & CMD_START) {
        clear_stats(); update_timing(); hw_restart(true); g_running = true;
    }
}

static void core0_loop(void) {
    gpio_init(PICO_DEFAULT_LED_PIN); gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    uint32_t next_blink = 0;
    for (;;) {
        uint32_t now = time_us_32();
        if ((int32_t)(now - next_blink) >= 0) { gpio_put(PICO_DEFAULT_LED_PIN, !gpio_get(PICO_DEFAULT_LED_PIN)); next_blink = now + 500000; }

        uint32_t cmd = __atomic_exchange_n(&g_cmd, 0, __ATOMIC_ACQ_REL);
        if (cmd) handle_cmds(cmd);
        if (!g_running) { tight_loop_contents(); continue; }

        uint t1sm = cfg.mode ? SM_MOT : SM_CLK;
        uint off  = cfg.mode ? SM_CLK : SM_MOT;

        if (pio_sm_is_rx_fifo_full(pio, SM_T0)) st_fifo_full++;
        while (!pio_sm_is_rx_fifo_empty(pio, SM_T0))  on_t0(~pio_sm_get(pio, SM_T0));   /* T0 primeiro */
        while (!pio_sm_is_rx_fifo_empty(pio, t1sm))   on_t1(~pio_sm_get(pio, t1sm));    /* ~X = contagem crescente */
        while (!pio_sm_is_rx_fifo_empty(pio, off))    (void)pio_sm_get(pio, off);       /* eixo nao testado */

        if (pend && (uint32_t)(time_us_32() - pend_us) > cfg.timeout_us) {
            frame_put(FT_TIMEOUT, cfg.mode, pend_seq, pend_t0, 0, 0);
            st_to++; pend = false;
        }
    }
}

/* ---------------- USB / protocolo (core 1) ---------------- */
static void usb_write(const void *p, int n) {
    if (n > 0 && stdio_usb_connected()) stdio_usb.out_chars((const char *)p, n);   /* binario, sem CRLF */
}

static void reply(const char *fmt, ...) {
    char b[200];
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    if (n > (int)sizeof b - 1) n = sizeof b - 1;
    usb_write(b, n);
}

static void send_stat(void) {
    reply("#STAT run=%u mode=%c ok=%lu timeouts=%lu drops=%lu order=%lu stray=%lu fifo_full=%lu ring_ovf=%lu hz=%lu\n",
          g_running, cfg.mode ? 'm' : 'c',
          (unsigned long)st_ok, (unsigned long)st_to, (unsigned long)st_drop, (unsigned long)st_order,
          (unsigned long)st_stray, (unsigned long)st_fifo_full, (unsigned long)st_ring_ovf,
          (unsigned long)hz_sys);
}

static void handle_line(char *s) {
    if (!strcmp(s, "VERSION"))      reply("#VER " FW_VERSION "\n");
    else if (!strcmp(s, "STAT"))    send_stat();
    else if (!strcmp(s, "START"))   { __atomic_fetch_or(&g_cmd, CMD_START, __ATOMIC_RELEASE); reply("#ACK START\n"); }
    else if (!strcmp(s, "STOP"))    { __atomic_fetch_or(&g_cmd, CMD_STOP,  __ATOMIC_RELEASE); reply("#ACK STOP\n"); }
    else if (!strcmp(s, "RESET"))   { __atomic_fetch_or(&g_cmd, CMD_RESET, __ATOMIC_RELEASE); reply("#ACK RESET\n"); }
    else if (!strncmp(s, "CONF ", 5)) {
        char pull[8], e, m; unsigned ho, to;
        if (sscanf(s + 5, "%7s %c %c %u %u", pull, &e, &m, &ho, &to) != 5) { reply("#ERR CONF sintaxe\n"); return; }
        uint8_t p;
        if      (!strcmp(pull, "f"))    p = PULL_FLOAT;
        else if (!strcmp(pull, "up"))   p = PULL_UP;
        else if (!strcmp(pull, "down")) p = PULL_DOWN;
        else { reply("#ERR CONF pull\n"); return; }
        if ((e != 'r' && e != 'f') || (m != 'c' && m != 'm') || ho > 1000000u || to < 100u || to > 50000000u) {
            reply("#ERR CONF faixa\n"); return;
        }
        cfg_new = (cfg_t){ p, e == 'r', m == 'm', ho, to };
        __atomic_fetch_or(&g_cmd, CMD_CONF, __ATOMIC_RELEASE);
        reply("#ACK CONF %s %c %c %u %u\n", pull, e, m, ho, to);
    }
    else reply("#ERR comando desconhecido\n");
}

static void drain_frames(void) {
    uint8_t out[FRAME_LEN * 16];
    int n = 0;
    uint32_t w = ring_wr; __dmb();
    while (n < 16 && ring_rd != w) {
        memcpy(out + n * FRAME_LEN, ring[ring_rd % RING_N], FRAME_LEN);
        __dmb();
        ring_rd = ring_rd + 1;
        n++;
    }
    usb_write(out, n * FRAME_LEN);
}

static void core1_main(void) {
    char line[96]; unsigned len = 0;
    for (;;) {
        char rx[64];
        int n = stdio_usb.in_chars(rx, sizeof rx);          /* nao bloqueante */
        for (int i = 0; i < n; i++) {
            char ch = rx[i];
            if (ch == '\n' || ch == '\r') {
                if (len) { line[len] = 0; handle_line(line); len = 0; }
            } else if (len < sizeof line - 1) line[len++] = ch;
        }
        drain_frames();
        if (n <= 0 && ring_rd == ring_wr) sleep_us(100);
    }
}

/* ---------------- main ---------------- */
int main(void) {
    stdio_init_all();                                        /* USB CDC no core 0 (padrao do SDK) */
    off_rise = pio_add_program(pio, &edge_rise_program);
    off_fall = pio_add_program(pio, &edge_fall_program);
    update_timing();
    hw_restart(false);                                       /* pronto, parado, aguardando START */
    multicore_launch_core1(core1_main);
    core0_loop();
}
