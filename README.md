# Gamer Peripheral Latency Meter

**Measure real end-to-end latency of a gaming peripheral** — from the physical
action to the electrical response your software sees — with ~8&nbsp;ns
resolution, using two cheap boards and any browser tab.

A USB-HID "translator" (CH32V307, 8&nbsp;kHz polling) converts mouse/keyboard
reports into clean GPIO edges; a Raspberry Pi Pico 2 (RP2350) timestamps those
edges with PIO state machines; a zero-dependency WebSerial web app
(**10 languages**) drives and plots everything over USB CDC.

> 🌐 **Live app** — https://mrstuani.github.io/gamer-latency-meter/
> (WebSerial needs HTTPS — GitHub Pages already provides it.)

---

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
│  "translator"│        │  PIO 3×SM + DMA  │  WebSerial│   app    │
│  CH32V307    │ T1 ───►│  0xAA 20-byte    │ ────────► │ (this    │
│  USB-HID 8kHz│        │  frames          │           │  repo)   │
└─────────────┘        └──────────────────┘           └──────────┘
```

**Resolution** — all three SMs count down from the same armed value (lockstep):
2 PIO-clock-cycles per tick → **~8&nbsp;ns @ 250&nbsp;MHz**. Full details in
[`docs/protocol.md`](docs/protocol.md).

## Repo layout

```
.
├── index.html / app.js / i18n.js / style.css   → WebSerial app (root = GitHub Pages)
├── firmware-ch32v307/                          → USB-HID 8kHz host → GPIO "translator"
├── firmware-rp2350/                            → Pico 2 edge-timestamper + protocol
└── docs/
    └── protocol.md                             → protocol & architecture spec
```

## Web app

- Vanilla JS, **no dependencies, no build step** — works from `file://`, `localhost`, or any static host.
- **WebSerial** to speak CDC at 115200&nbsp;8-N-1 with the meter.
- **Multilingual UI (10 languages):** English · Português (BR) · Español · Français · Deutsch · Italiano · 中文 · 日本語 · 한국어 · Русский — auto-detected from your browser or the header selector.
- Live scatter + histogram, real-time stats (avg / min-max / std / timeouts / drops), CSV & JSON export.

> ⚠️ WebSerial requires **Chrome or Edge (desktop)** and a secure context
> (HTTPS or `localhost`). Mobile browsers won't work. A warning is shown in-app otherwise.

## Wiring (Pico 2)

| Pico 2 pin | Signal | Connects to |
|---|---|---|
| `GP2` (pin 4) | `T0` — physical action (switch / photodiode+comparator) | your ground truth |
| `GP3` (pin 5) | `T1_CLICK` — click level from CH32V307 `PA0` | translator |
| `GP4` (pin 6) | `T1_MOTION` — motion pulse from CH32V307 `PA1` (TIM2 one-shot) | translator |
| GND (pin 3 or 8) | common ground | both boards |

Signals are **3.3&nbsp;V** — never apply 5&nbsp;V to the pins. The in-app pinout
diagram (all languages) shows every step.

## Building the firmware

### CH32V307 translator (`firmware-ch32v307/`)

RISC-V `rv32imac`, bare-metal USB-HS host (no RTOS). Requires the MounRiver
RISC-V GCC toolchain (path set in `Makefile` / `build.ps1`):

```powershell
.\build.ps1          # or: make
```

Outputs `ch32v307-usbhid-latency.hex` / `.bin`. Flash with the WCH-LinkE.
`PA0` = click, `PA1` = motion pulse (~10&nbsp;µs, 100% hardware via TIM2).
HID polling runs at **8&nbsp;kHz** (`bInterval=1` over microframes).

### RP2350 meter (`firmware-rp2350/`)

Standard Pico SDK (C11, `PICO_BOARD=pico2`, `PICO_PLATFORM=rp2350-arm-s`). Build
with the SDK, or `idf.py`-style:

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

## License

This project's own sources are provided as-is for experimentation.
The WCH EVT support files (`EVT_Support/`, `USB_Host/`, `USB_Device/`) are the
property of WCH and are redistributed unmodified under their terms.
Third parties, see `openwch/ch32v307`.