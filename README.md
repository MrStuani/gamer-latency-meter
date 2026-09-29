# Gamer Peripheral Latency Meter

**Measure peripheral latency on your own bench** — from the physical action
(`T0`) to the translator's output edge (`T1`), with ~13.3&nbsp;ns timer
resolution (@&nbsp;150&nbsp;MHz on the RP2350), using two cheap boards and any
browser tab.

A USB-HID "translator" (CH32V307, 8&nbsp;kHz polling) converts mouse/keyboard
reports into clean GPIO edges; a Raspberry Pi Pico 2 (RP2350) timestamps those
edges with PIO state machines; a zero-dependency WebSerial web app
(**10 languages**) drives and plots everything over USB CDC.

> 🌐 **Live app** — https://mrstuani.github.io/gamer-latency-meter/
> (WebSerial needs HTTPS — GitHub Pages already provides it.)
>
> 📦 **Prebuilt firmware** — [Releases](https://github.com/MrStuani/gamer-latency-meter/releases):
> `pico2-medidor-latency-rp2350.uf2` (Pico 2, drag & drop) and
> `ch32v307-usbhid-latency.hex`/`.bin` (v1.0.1+, flash with WCH-LinkE).

<p align="center">
  <img src="https://i.imgur.com/9iGAsh1.jpeg" alt="Full setup: CH32V307 + Pico 2 measuring the latency of a gaming mouse" width="700">
  <br>
  <sub>Measurement rig: CH32V307 (translator) + Pico 2 (timestamper) + mouse under test</sub>
</p>

---

## What this measures — and what it doesn't

The rig timestamps two edges and subtracts them. What sits between those edges
is the part that matters when you compare numbers with other tools:

| | Detail |
|---|---|
| ✅ **Inside the measurement** | `T0` → `T1` edge delta, quantized to 2 PIO cycles (**~13.3&nbsp;ns @ 150&nbsp;MHz**) |
| ⚠️ **Partly inside** | The translator's own latency: ~10&nbsp;µs systematic (the `PA1` pulse rises 10 timer ticks after the ISR writes the compare) plus **≤ ~125&nbsp;µs** of poll quantization at 8&nbsp;kHz (mean ~62&nbsp;µs) |
| ❌ **Outside** | The OS/game input stack, USB enumeration, whatever the app does after it sees the report |

So a number from this tool is *"action to translator GPIO"*, not *"action to
pixel"*. Treat the ~13.3&nbsp;ns as the **resolution of the timestamp**, never
as an error budget: the input tolerance is dominated by the 8&nbsp;kHz polling
above, so a single sample is only meaningful to tens–hundreds of&nbsp;µs.
Presenting a result as "8&nbsp;ns accurate" would not be defensible, and neither
is presenting it as pixel latency.

**Calibrating the translator's offset** — the ~10&nbsp;µs design offset is not a
bug, it is the systematic latency of the motion pulse by design. To remove it
from your own numbers:

1. Probe `PA2` (the firmware's debug toggle) and the `T1` pin on a scope, and
   measure how long after the `PA2` edge `PA1` rises — that is your translator
   offset, per board.
2. Subtract it from the motion-axis results, and add back the poll
   quantization (±~125&nbsp;µs) as an uncertainty term.
3. A "click to pulse" measurement on the scope should match what the app reports
   within one poll period; if it does not, suspect the `T0` wiring first
   (see [Measuring the motion axis](#measuring-the-motion-axis-t1_motion) for
   which physical edge you are actually capturing).

The firmware **never overclocks** the board: it reads the real clock with
`clock_get_hz(clk_sys)` and reports it in the `STAT` line (`hz=...`) so you can
confirm it on your own unit. Full details in
[`docs/protocol.md`](docs/protocol.md).

## Why this exists

Mouse/keyboard latency is normally quoted as "switch debounce + report rate",
but the numbers that actually matter are the ones you can *verify on your own
bench*. This rig closes the loop:

| Block | Board | Job |
|-------|-------|-----|
| ① "translator" | CH32V307 | USB-HS host, polls the peripheral at 8&nbsp;kHz, emits `T1_CLICK` (level) / `T1_MOTION` (pulse) on GPIO |
| ② timing tool | Pico 2 (RP2350) | PIO ×3 SMs timestamp the edges on the same timebase, computes **T1 − T0** |
| ③ ground truth | your bench | `T0` rewired by you — switch, photodiode+comparator, your robotic finger |

```
┌─────────────┐   T0   ┌──────────────────┐  USB-CDC  ┌──────────┐
│ physical/user│ ─────►│  Pico 2 (RP2350) │ ◄───────► │   web    │
│  "translator"│        │  PIO 3×SM (FIFO) │  WebSerial│   app    │
│  CH32V307    │ T1 ───►│  0xAA 20-byte    │ ────────► │ (this    │
│  USB-HID 8kHz│        │  frames          │           │  repo)   │
└─────────────┘        └──────────────────┘           └──────────┘
```

**Resolution** — all three SMs count down from the same armed value (lockstep):
2 PIO-clock-cycles per tick → **~13.3&nbsp;ns @ 150&nbsp;MHz**, the RP2350's
default system clock on the Pico&nbsp;2. The firmware **never overclocks** the
board: it reads the real clock with `clock_get_hz(clk_sys)` and reports it in
the `STAT` line (`hz=...`) so you can confirm it on your own unit. Full details
in [`docs/protocol.md`](docs/protocol.md).

## Repo layout

```
.
├── index.html / app.js / i18n.js / style.css   → WebSerial app (root = GitHub Pages)
├── report.js                                  → report generator (txt / md / csv / json)
├── tests/                                     → node tests, no dependencies
├── firmware-ch32v307/                          → USB-HID 8kHz host → GPIO "translator"
├── firmware-rp2350/                            → Pico 2 edge-timestamper + protocol
├── LICENSE / NOTICE                            → MIT (first-party) + WCH carve-out
└── docs/
    └── protocol.md                             → protocol & architecture spec
```

## Web app

- Vanilla JS, **no dependencies, no build step** — works from `file://`, `localhost`, or any static host.
- **WebSerial** to speak CDC at 115200&nbsp;8-N-1 with the meter.
- **Multilingual UI (10 languages):** English · Português (BR) · Español · Français · Deutsch · Italiano · 中文 · 日本語 · 한국어 · Русский — auto-detected from your browser or the header selector.
- Live scatter + histogram, real-time stats (avg / min-max / **median · p95 · p99** / std / timeouts / drops).
- **Human-readable reports** in `.txt`, `.md`, `.csv` and `.json` — see below.

> ⚠️ WebSerial requires **Chrome or Edge (desktop)** and a secure context
> (HTTPS or `localhost`). Mobile browsers won't work. A warning is shown in-app otherwise.

## Latency report

The export answers one question in a form you can read or paste: **how long did
the mouse take?** Both axes (CLICK and MOTION) always get their own section, and
the numbers are in **µs below 1&nbsp;ms, ms above** — no raw PIO counters, no
nanoseconds.

Each report carries the run context (date, firmware string from `#VER`, clock and
the resulting resolution, the active config, elapsed time, and how many
timeouts/drops/order/stray events were discarded), the statistics **min · median ·
mean · p95 · p99 · max · std deviation**, a **distribution histogram**, and the
**20 worst samples** with their sequence numbers.

| Format | What it is for |
|---|---|
| `latency_*.txt` | Read it, paste it into an issue/forum. ASCII histogram, boxed sections. |
| `latency_*.md` | Same numbers as Markdown tables, ready for a GitHub issue. |
| `latency_*.csv` | One block per axis: metadata on `#` lines, then a data block with `"sample","seq","latency_us"` (or `latency_ms` — the unit is chosen per axis from its median and stated in the metadata). UTF-8 BOM so Excel reads it. |
| `latency_*.json` | Machine-readable: `schema: gamer-latency-meter/report@1`, `stats` in exact nanoseconds, plus a `display` object with the same values pre-formatted. |

The filename prefix follows the UI language (`latency_…` / `latencia_…`).

Notes on the choices:

- The distribution is binned up to **p99** so a single 3&nbsp;ms outlier can't
  flatten the rest of the histogram — everything above p99 lands in the last
  bin, labelled `≥ x`.
- **The CSV data schema is fixed English** (`sample`, `seq`, `latency_us`), so
  two people exporting from different UI languages still produce identical
  columns. Only the human-readable parts vary: the `#` metadata lines, `.txt`
  and `.md` are fully localized. The JSON keys are English for the same reason.
- **Numbers follow the UI language in prose, not in data.** `Intl.NumberFormat`
  gives `1,08 ms` to a pt-BR reader and `1.08 ms` to everyone else, but the CSV
  data column and the JSON numeric fields always use a dot, and no thousands
  separator is ever added — `10.000 amostras` pasted into an international forum
  reads as ten point zero.
- The raw `t0_ts`/`t1_ts` counters are gone from the exports: `lat_ns` already
  carries the result, and the clock (`hz=`) is in the header if you want to
  re-derive anything.

The report generator is pure and lives in [`report.js`](report.js) (`Report.build`);
the tests run in Node with no dependencies:

```bash
node tests/report.test.mjs    # 110 checks: units, percentiles, locale, 4 formats
node tests/i18n.test.mjs      # 180 checks: key parity across the 10 languages
node tests/app.smoke.mjs      # 56 checks: app.js in a fake DOM, frames -> export
node tests/report.test.mjs --write   # writes tests/out.{txt,md,csv,json} to eyeball
```

## Wiring (Pico 2)

<p align="center">
  <img src="https://i.imgur.com/8KiXe5A.jpeg" alt="Perfboard build: Pico 2 and CH32V307 connected with jumper wires" width="600">
  <br>
  <sub>Perfboard build: Pico 2 (red) and CH32V307 (black)</sub>
</p>
<p align="center">
  <img src="https://i.imgur.com/8Y5WN7X.png" alt="Wiring diagram: CH32V307 translator to Pico 2 meter" width="800">
</p>
| Pico 2 pin | Signal | Connects to |
|---|---|---|
| `GP2` (pin 4) | `T0` — physical action (switch / photodiode+comparator) | your ground truth |
| `GP3` (pin 5) | `T1_CLICK` — click level from CH32V307 `PA0` | translator |
| `GP4` (pin 6) | `T1_MOTION` — motion pulse from CH32V307 `PA1` (TIM2 one-shot) | translator |
| GND (pin 3 or 8) | common ground | both boards |

Signals are **3.3&nbsp;V** — never apply 5&nbsp;V to the pins. The in-app pinout
diagram (all languages) shows every step.

### Measuring the motion axis (T1_MOTION)

The translator's `PA1` motion pulse is generated from the **USB-HID data the PC
receives**, so this path works with **any** mouse. If you instead want to
capture the mouse **optical sensor's own motion output** (the pulse the sensor
raises when it detects displacement), that works **only on mice whose sensor
exposes that pin** — most don't. To find it you need either the **sensor
datasheet** (the pin is often called `MOT` / `MO` / `MOTION` — exact name varies
by sensor family) or an **oscilloscope**: probe the sensor's pads while moving
the mouse until you see the pulse train.

## Building the firmware

### CH32V307 translator (`firmware-ch32v307/`)

RISC-V `rv32imac`, bare-metal USB-HS host (no RTOS). Requires the MounRiver
RISC-V GCC toolchain (path set in `Makefile` / `build.ps1`):

```powershell
.\build.ps1          # or: make
```

Outputs `ch32v307-usbhid-latency.hex` / `.bin`. Flash with the WCH-LinkE.
`PA0` = click, `PA1` = motion pulse (100% hardware via TIM2, 1 µs timer
prescaler with the compare preload rising ~10&nbsp;µs after the ISR write).
HID polling runs at **8&nbsp;kHz** (`bInterval=1` over microframes).

> **Which chip exactly?** The firmware targets the **CH32V30x_D8C** line at
> **144&nbsp;MHz** (`-DSYSCLK_FREQ=144000000`) — the same EVT support chain,
> startup and peripheral set (USB-HS host, TIM2/TIM3, GPIO) is shared by the
> **CH32V307** and the **CH32V305**, so either part works and the prebuilt
> `.hex`/`.bin` runs on both. Variants only differ in flash/RAM size and
> package/pin count (handled by the linker script), not in the blocks this
> firmware uses. Confirm your exact part by the chip marking (`CH32V307…` vs
> `CH32V305…`). The GPIO of this line is **5&nbsp;V-tolerant** on most I/O
> (the `FT` marks in the WCH datasheet) — handy if your `T0` source is 5&nbsp;V
> logic — but the outputs this firmware drives (`PA0`/`PA1`/`PA2`) are
> **3.3&nbsp;V** levels, so keep everything ≤ 3.3&nbsp;V when wiring the
> Pico&nbsp;2.

### Measured latency & the ~10&nbsp;µs offset

The motion (T1_MOTION) rising edge fires ~10&nbsp;µs **after** the firmware
actually sees movement: the ISR writes the timer compare and the output rises
10 timer ticks later, with an additional one-poll-period quantization
(≤ ~125&nbsp;µs at 8&nbsp;kHz). This ~10&nbsp;µs is **not** a bug in the
firmware, it is the systematic latency of the motion pulse by design — see
[What this measures](#what-this-measures--and-what-it-doesnt) at the top for the
full accounting and the calibration procedure.

> ⚠️ **The CH32 side is not a constant, either.** Running the CH32 at a fixed
> 144&nbsp;MHz does **not** guarantee a fixed interrupt-to-GPIO latency: the full
> path (report → USB → ISR → TIM2 → GPIO → Pico PIO) has to be verified
> physically on your bench (oscilloscope; the translator's `PA2` debug toggle
> shows the ISR budget). Expect the spread you see, not the nominal 10&nbsp;µs.

### HID decode behavior

The translator mirrors what a PC actually receives, so latency measured on
the scope matches what software on a desktop would see:

- **Report format wins over Boot.** The peripheral's HID report descriptor is
  read (with up to 4 retries) and bit-level button/X/Y offsets are decoded from
  it. Mice are **never** switched to Boot protocol (`SET_PROTOCOL(Boot)`):
  some devices "fake-ACK" it and degrade to a garbage boot stream the host
  cannot recover. A few devices with a failing descriptor read fall back to an
  empirical report layout and the Report ID is then confirmed live from the
  frames (`byte0` constant across frames).
- **Primary report only.** Auxiliary reports (wheel/vendor, different Report ID)
  are ignored so they cannot corrupt click/motion state — same intent as a PC
  merging them, but only the meaningful report drives the GPIO.
- **Click arming.** The CLICK line only goes active after the first clean
  "buttons=0" frame, so a plug-in burst or a device that reports junk while the
  host enumerates can never produce a spurious click.
- **Plug-in window.** For the first ~50&nbsp;ms after enumeration, both click
  and motion are suppressed during the device's own "settle/handshake" burst;
  genuine movement reported by the device afterwards is passed through.

### RP2350 meter (`firmware-rp2350/`)

Standard Pico SDK (C11, `PICO_BOARD=pico2`). Build with the SDK:

```bash
cmake -B build -DPICO_BOARD=pico2 -DPICO_PLATFORM=rp2350-arm-s -DPICO_SDK_PATH=/path/to/pico-sdk
cmake --build build
```

Console is USB CDC (TinyUSB), UART disabled. Drop the resulting `.uf2` on the
Pico 2 and it shows up as a serial port for the web app.

## Serial protocol

Lines from host: `CONF <t0pull> <edge> <mode> <holdoff_us> <timeout_us>`,
`START`, `STOP`, `STAT`, `VERSION`, `RESET`. Device replies with `#`-prefixed
text and fixed **20-byte binary frames** starting `0xAA` (measure / timeout /
order / dropped). Spec: [`docs/protocol.md`](docs/protocol.md).

## Cost vs a commercial USB analyzer

A purpose-built USB protocol analyzer (e.g. the Beagle&nbsp;USB&nbsp;480) answers
the same class of question — USB traffic, timing and end-to-end latency of a
peripheral — for literally **hundreds of US dollars**. This rig (CH32V307 +
Pico&nbsp;2 + WCH-LinkE + a few jumper wires) covers that same measurement for
the price of **a few dollars**, and unlike the single-purpose analyzer it also
timestamps the *physical* side (`T0`), which a USB-only analyzer cannot see. It
is not a replacement for deep protocol-decode tools; it is a far cheaper way to
get trusted click/XY → report latency numbers on your own bench.

## License

This project's own code — web app, RP2350 firmware, tests and docs — is
[MIT](LICENSE) © 2026 MrStuani.

The `firmware-ch32v307/EVT_Support/`, `USB_Host/` and `USB_Device/` trees are
WCH's, redistributed unmodified from the official `CH32V307EVT.ZIP` so the
firmware builds out of the box. **They are not covered by the MIT grant and
carry no license from WCH** — see [NOTICE](NOTICE) for the details and for how
to swap in your own copy.
