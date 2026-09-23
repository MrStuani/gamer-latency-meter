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
>
> 📦 **Prebuilt firmware** — [Releases](https://github.com/MrStuani/gamer-latency-meter/releases):
> `pico2-medidor-latency-rp2350.uf2` (Pico 2, drag & drop) and
> `ch32v307-usbhid-translator.hex`/`.bin` (flash with WCH-LinkE).

<p align="center">
  <img src="https://i.imgur.com/9iGAsh1.jpeg" alt="Full setup: CH32V307 + Pico 2 measuring the latency of a gaming mouse" width="700">
  <br>
  <sub>Measurement rig: CH32V307 (translator) + Pico 2 (timestamper) + mouse under test</sub>
</p>

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

### Measured latency & the ~10&nbsp;µs offset

The motion (T1_MOTION) rising edge fires ~10&nbsp;µs **after** the firmware
actually sees movement: the ISR writes the timer compare and the output rises
10 timer ticks later, with an additional one-poll-period quantization
(≤ ~125&nbsp;µs at 8&nbsp;kHz). This ~10&nbsp;µs is **not** a bug in the
firmware, it is the systematic latency of the motion pulse by design. When
interpreting scope/the app results, subtract ~10&nbsp;µs (and account for the
±125&nbsp;µs quantization, mean ~62&nbsp;µs) — an oscilloscope measurement of
"click to pulse" should match what the web app reports within one poll period.

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

## License

This project's own sources are provided as-is for experimentation.
The WCH EVT support files (`EVT_Support/`, `USB_Host/`, `USB_Device/`) are the
property of WCH and are redistributed unmodified under their terms.
Third parties, see `openwch/ch32v307`.
