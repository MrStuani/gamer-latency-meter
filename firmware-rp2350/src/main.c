#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "hardware/clocks.h"

#include "measure.h"
#include "config.h"

/* =========================================================================
 * Medidor de latência de periférico gamer — RP2350
 *
 * Console = CDC USB (stdio_usb, 0x1209/0x0001 no Pico SDK, serial simples).
 *
 * Procolo (docs/protocol.md):
 *   host -> device : linhas ASCII terminadas em '\n'
 *       CONF <t0pull> <edge> <mode> <holdoff_us> <timeout_us>          ex: CONF f r c 2000 100000
 *       START / STOP
 *       STAT | VERSION | RESET
 *   device -> host :
 *       - textos iniciados por '#' (ACK '#' prefixa) terminados em '\n'
 *       - frames binários fixos de 20 bytes começando com 0xAA:
 *         [0]0xAA [1]type [2]mode [3]flags [4..7]seq u32 [8..11]t0_ts u32
 *         [12..15]t1_ts u32 [16..19]lat_ns u32
 * ========================================================================= */

#define FRAME_LEN      20
#define FRAME_MAGIC    0xAAu

typedef enum {
    FT_TYPE_MEASURE  = 0x00,
    FT_TYPE_TIMEOUT  = 0x01,
    FT_TYPE_ORDER    = 0x02,
    FT_TYPE_DROPPED  = 0x03,
} frame_type_t;

static void emit_frame(const measure_event_t *ev)
{
    uint8_t f[FRAME_LEN] = {0};
    frame_type_t t =
        ev->type == EV_MEASURE ? FT_TYPE_MEASURE :
        ev->type == EV_TIMEOUT ? FT_TYPE_TIMEOUT :
        ev->type == EV_ORDER   ? FT_TYPE_ORDER   : FT_TYPE_DROPPED;

    f[0] = FRAME_MAGIC;
    f[1] = (uint8_t)t;
    f[2] = (uint8_t)ev->mode;
    f[3] = (uint8_t)ev->flags;
    memcpy(&f[4],  &ev->seq, 4);
    memcpy(&f[8],  &ev->t0_ts, 4);
    memcpy(&f[12], &ev->t1_ts, 4);
    memcpy(&f[16], &ev->lat_ns, 4);

    for (int i = 0; i < FRAME_LEN; i++) stdio_putchar((char)f[i]);
}

static void reply(const char *s)
{
    /* '#' prefixo separa texto de frame binário (0xAA) no mesmo stream.  */
    stdio_putchar('#');
    while (*s) stdio_putchar(*s++);
    stdio_putchar('\n');
}

static const char *pull_name(t0_pull_t p)
{
    return p == M_T0_PULL_UP ? "up" : p == M_T0_PULL_DOWN ? "down" : "float";
}
static const char *edge_name(edge_t e) { return e == M_EDGE_RISING ? "rise" : "fall"; }
static const char *mode_name(mode_t m) { return m == M_MODE_CLICK ? "click" : "motion"; }

static void stat_reply(void)
{
    measure_config_t cfg;
    measure_get_config(&cfg);

    uint32_t dropped = 0, stray = 0, order = 0; /* contadores dedicados p/ futuras versões */
    (void)dropped; (void)stray; (void)order;

    char b[160];
    int n = snprintf(b, sizeof(b),
        "STAT mode=%s running=%d seq_total=%lu click=%lu motion=%lu "
        "timeouts=%lu ring_peak=%d "
        "holdoff=%lu timeout=%lu t0edge=%s t0pull=%s clock=%lu",
        mode_name(cfg.mode), measure_running(),
        (unsigned long)measure_seq_total(),
        (unsigned long)measure_seq_per_mode(M_MODE_CLICK),
        (unsigned long)measure_seq_per_mode(M_MODE_MOTION),
        (unsigned long)measure_timeouts(),
        measure_ring_used_max(),
        (unsigned long)cfg.holdoff_us, (unsigned long)cfg.timeout_us,
        edge_name(cfg.t0_edge), pull_name(cfg.t0_pull),
        (unsigned long)clock_get_hz(clk_sys));
    (void)n;
    reply(b);
}

static void handle_conf(char *arg)
{
    measure_config_t cfg;
    measure_config_defaults(&cfg);
    char pull_c = 'f', edge_c = 'r', mode_c = 'c';
    unsigned long hold = DEFAULT_HOLDOFF_US, to = DEFAULT_TIMEOUT_US;

    int n = sscanf(arg, "%c %c %c %lu %lu",
                   &pull_c, &edge_c, &mode_c, &hold, &to);
    if (n < 3) { reply("ERR CONF precisa de: CONF <f|up|down> <r|f> <c|m> [holdoff_us] [timeout_us]"); return; }

    /* Diagnóstico: ecoa até 4 bytes crus recebidos (pode conter caractere
     * invisível que está corrompendo o parse). */
    {
        char dbg[80];
        int k = 0;
        while (arg[k] && arg[k] != '\r' && arg[k] != '\n' && k < 4) { if (arg[k] < 32) arg[k] = '?'; k++; }
        snprintf(dbg, sizeof(dbg), "DBG raw='%.*s' parsed=%d pull=%c edge=%c mode=%c", 12, arg, n, pull_c, edge_c, mode_c);
        reply(dbg);
    }

    cfg.t0_pull    = pull_c == 'u' || pull_c == 'U' ? M_T0_PULL_UP
                   : pull_c == 'd' || pull_c == 'D' ? M_T0_PULL_DOWN : M_T0_FLOAT;
    cfg.t0_edge    = (edge_c == 'f' || edge_c == 'F') ? M_EDGE_FALLING : M_EDGE_RISING;
    cfg.mode       = (mode_c == 'm' || mode_c == 'M') ? M_MODE_MOTION : M_MODE_CLICK;
    cfg.holdoff_us = (uint32_t)hold;
    cfg.timeout_us = (uint32_t)to;

    measure_apply_config(&cfg);
    char ack[112];
    snprintf(ack, sizeof(ack), "ACK CONF %s %s %c %lu %lu",
             pull_name(cfg.t0_pull), edge_name(cfg.t0_edge), mode_c,
             (unsigned long)cfg.holdoff_us, (unsigned long)cfg.timeout_us);
    reply(ack);
}

static void handle_line(char *line)
{
    /* linha sem '\n' já removido */
    if (!strncmp(line, "VERSION", 7)) {
        reply("VER 1.0.0");
    } else if (!strncmp(line, "STAT", 4)) {
        stat_reply();
    } else if (!strncmp(line, "RESET", 5)) {
        measure_config_t cfg;
        measure_config_defaults(&cfg);
        measure_apply_config(&cfg);
        reply("ACK RESET");
    } else if (!strncmp(line, "START", 5)) {
        measure_start();
        reply("ACK START");
    } else if (!strncmp(line, "STOP", 4)) {
        measure_stop();
        reply("ACK STOP");
    } else if (!strncmp(line, "CONF", 4)) {
        handle_conf(line + 4);
    } else if (line[0]) {
        reply("ERR comando desconhecido");
    }
}

static int serial_line_ready(void)
{
    return stdio_usb_connected();   /* só processa comandos com host ligado  */
}

static void mini_line_loop(void)
{
    static char buf[128];
    static uint8_t len = 0;
    int c;
    while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        if (c == '\n' || c == '\r') {
            if (len) { buf[len] = 0; handle_line(buf); len = 0; }
        } else if (len < sizeof(buf) - 1) {
            buf[len++] = (char)c;
        }
    }
}

int main(void)
{
    if (SYSTEM_CLOCK_KHZ) set_sys_clock_khz(SYSTEM_CLOCK_KHZ, true);

    stdio_init_all();
    measure_init();

    measure_event_t ev;
    for (;;) {
        mini_line_loop();
        if (measure_poll(&ev)) emit_frame(&ev);
        tight_loop_contents();
    }
    return 0;
}