/* Medidor de Latência RP2350 — lado web (WebSerial, sem dependências). */

"use strict";

/* ============================ estado global ============================ */
const MAX_SAMPLES = 4000;          // amostras mantidas por modo
const DATA = { click: [], motion: [] };   // {seq, t0, t1, lat}

let port = null;
let reader = null;
let writer = null;
let rxBuf = new Uint8Array(0);
let running = false;
let startedAt = 0;
let stCounters = { total: 0, timeouts: 0, drops: 0 };
let autoStatTimer = null;

const $ = (id) => document.getElementById(id);
const fmtUs = (ns) => (ns === undefined || ns === null) ? "—"
                      : (ns / 1000).toFixed(ns < 100000 ? 2 : 1) + " µs";

/* ============================ log ============================ */
function logLine(cls, text) {
  const ul = $("event-log");
  const li = document.createElement("li");
  li.className = cls;
  li.textContent = text;
  ul.prepend(li);
  while (ul.children.length > 80) ul.lastChild.remove();
}

const log = { m: (t) => logLine("m", t), t: (t) => logLine("t", t), e: (t) => logLine("e", t) };

/* ============================ estatísticas ============================ */
function statsOf(arr) {
  const n = arr.length;
  if (!n) return null;
  let min = Infinity, max = -Infinity, sum = 0;
  for (const s of arr) {
    if (s.lat < min) min = s.lat;
    if (s.lat > max) max = s.lat;
    sum += s.lat;
  }
  const mean = sum / n;
  let s2 = 0;
  for (const s of arr) s2 += (s.lat - mean) ** 2;
  return { n, min, max, mean, std: Math.sqrt(s2 / n) };
}

function fmtClock(ms) {
  const s = ms / 1000;
  return s < 90 ? s.toFixed(1) + " " + I18N.t("unit.s") : (s / 60).toFixed(1) + " " + I18N.t("unit.min");
}

function refreshStats() {
  const modeArr = DATA[currentMode()];
  const st = statsOf(modeArr);
  $("st-last").textContent  = modeArr.length ? fmtUs(modeArr[modeArr.length - 1].lat) : "—";
  $("st-avg").textContent   = st ? fmtUs(st.mean) : "—";
  $("st-minmax").textContent = st ? fmtUs(st.min) + " / " + fmtUs(st.max) : "—";
  $("st-std").textContent   = st ? fmtUs(st.std) : "—";
  $("st-count").textContent = st ? st.n : 0;
  $("st-elapsed").textContent = running && startedAt ? fmtClock(Date.now() - startedAt) : "0 s";
  $("st-seq").textContent   = stCounters.total;
  $("st-timouts").textContent = stCounters.timeouts + " / " + stCounters.drops;
}

/* ============================ gráficos ============================ */
function drawScatter() {
  const cvs = $("chart-main");
  const ctx = cvs.getContext("2d");
  const W = cvs.width, H = cvs.height;
  ctx.clearRect(0, 0, W, H);

  const arr = DATA[currentMode()];
  const n = arr.length;
  if (!n) return;

  const use = arr.slice(-MAX_SAMPLES);
  let min = Infinity, max = -Infinity;
  for (const s of use) { if (s.lat < min) min = s.lat; if (s.lat > max) max = s.lat; }
  const span = (max - min) || 1;
  const padT = 8, padB = 22, padL = 48;

  // grid
  ctx.strokeStyle = "#1c2433";
  ctx.fillStyle = "#7c8aa5";
  ctx.font = "10px monospace";
  for (let i = 0; i <= 4; i++) {
    const y = padT + (i / 4) * (H - padT - padB);
    ctx.beginPath(); ctx.moveTo(padL, y); ctx.lineTo(W - 6, y); ctx.stroke();
    const v = max - (i / 4) * span;
    ctx.fillText((v / 1000).toFixed(v / 1000 >= 10 ? 1 : 2) + "ms", 2, y + 3);
  }

  const xFor = (i) => padL + (i / Math.max(1, use.length - 1)) * (W - padL - 6);
  const yFor = (v) => padT + (1 - (v - min) / span) * (H - padT - padB);

  ctx.fillStyle = "#39d0a0";
  ctx.beginPath();
  for (let i = 0; i < use.length; i++) {
    ctx.moveTo(xFor(i) + 1, yFor(use[i].lat));
    ctx.arc(xFor(i), yFor(use[i].lat), 1.6, 0, 6.2832);
  }
  ctx.fill();

  // média móvel
  ctx.strokeStyle = "#4da3ff";
  ctx.lineWidth = 1.4;
  ctx.beginPath();
  const WINDOW = 64;
  for (let i = 0; i < use.length; i++) {
    let s = 0, c = 0;
    for (let j = Math.max(0, i - WINDOW); j <= i; j++) { s += use[j].lat; c++; }
    const mx = xFor(i), my = yFor(s / c);
    i === 0 ? ctx.moveTo(mx, my) : ctx.lineTo(mx, my);
  }
  ctx.stroke();
}

function drawHistogram() {
  const cvs = $("chart-hist");
  const ctx = cvs.getContext("2d");
  const W = cvs.width, H = cvs.height;
  ctx.clearRect(0, 0, W, H);

  const arr = DATA[currentMode()].slice(-256);
  const n = arr.length;
  if (!n) return;
  let min = Infinity, max = -Infinity;
  for (const s of arr) { if (s.lat < min) min = s.lat; if (s.lat > max) max = s.lat; }
  const span = (max - min) || 1;
  const BINS = 40;
  const counts = new Array(BINS).fill(0);
  for (const s of arr) counts[Math.min(BINS - 1, Math.floor(((s.lat - min) / span) * BINS))]++;
  const peak = Math.max(...counts);
  const bw = W / BINS;

  ctx.fillStyle = "#39d0a0";
  for (let i = 0; i < BINS; i++) {
    const h = (counts[i] / peak) * (H - 24);
    ctx.fillRect(i * bw + 1, H - h, bw - 2, h);
  }
}

/* ============================ parser do stream ============================ */
function currentMode() { return $("cfg-mode").value === "m" ? "motion" : "click"; }

function handleFrame(f) {
  const mode = f.mode === 1 ? "motion" : "click";
  const seq  = f.seq;
  switch (f.type) {
    case 0: {
      DATA[mode].push({ seq, t0: f.t0, t1: f.t1, lat: f.lat });
      if (DATA[mode].length > MAX_SAMPLES) DATA[mode].splice(0, DATA[mode].length - MAX_SAMPLES);
      stCounters.total++;
      if (!running) { running = true; startedAt = Date.now(); }
      break;
    }
    case 1: stCounters.timeouts++; log.t(I18N.t("log.timeout", { seq, us: timeoutUs() })); break;
    case 2: log.e(I18N.t("log.order", { seq: seq })); break;
    case 3: stCounters.drops++;  log.t(I18N.t("log.dropped", { seq })); break;
  }
}

function timeoutUs() { return Number($("cfg-timeout").value) || 100000; }

function decodeText(chunk) {
  try { return new TextDecoder().decode(chunk).trim(); }
  catch { return ""; }
}

function handleText(t) {
  if (!t) return;
  if (t.startsWith("#")) {
    if (t.includes("STAT")) log.m(t);         // linha de status crua
    else if (t.startsWith("#ACK")) log.m(t);
    else if (t.startsWith("#ERR")) log.e(t);
    else if (t.startsWith("#VER")) log.m(t);
    else log.m(t);
  }
}

function pump(append) {
  // acumula
  let buf = new Uint8Array(rxBuf.length + append.length);
  buf.set(rxBuf, 0);
  buf.set(append, rxBuf.length);
  let i = 0;

  while (i < buf.length) {
    if (buf[i] === 0xAA) {
      if (buf.length - i < 20) break;          // espera frame inteiro
      const f = { type: buf[i + 1], mode: buf[i + 2] };
      const dv = new DataView(buf.buffer, buf.byteOffset + i);
      f.seq = dv.getUint32(4, true);
      f.t0  = dv.getUint32(8, true);
      f.t1  = dv.getUint32(12, true);
      f.lat = dv.getUint32(16, true);
      handleFrame(f);
      i += 20;
    } else {
      let nl = -1;
      for (let j = i; j < buf.length; j++) if (buf[j] === 0x0A) { nl = j; break; }
      if (nl < 0) break;                        // espera '\n'
      const t = decodeText(buf.subarray(i, nl));
      handleText(t);
      i = nl + 1;
    }
  }

  rxBuf = buf.slice(i);                         // resto pendente
  if (rxBuf.length > 128 * 1024) { log.e(I18N.t("log.bufOverflow")); rxBuf = new Uint8Array(0); }
  redraw();
}

/* ============================ serial ============================ */
async function sendLine(cmd) {
  if (!writer) return;
  await writer.write(new TextEncoder().encode(cmd + "\n"));
}

async function connect() {
  try {
    port = await navigator.serial.requestPort();
    await port.open({ baudRate: 115200 });
    writer = port.writable.getWriter();
    await port.setSignals?.({ dataTerminalReady: true });

    setUi(true);
    log.m(I18N.t("log.connected"));

    const loop = async () => {
      reader = port.readable.getReader();
      while (port) {
        let r;
        try { r = await reader.read(); }
        catch (e) { if (port) log.e(I18N.t("log.readFailed") + e); break; }
        if (r.done) break;
        if (r.value && r.value.length) pump(r.value);
      }
      if (port) disconnect();
    };
    loop();
    autoStatTimer = setInterval(() => { if (port) sendLine("STAT"); }, 2000);
    await sendLine("VERSION");
  } catch (e) {
    log.e(I18N.t("log.connectError") + e);
    if (port) { try { await port.close(); } catch {} port = null; }
    setUi(false);
  }
}

async function disconnect() {
  clearInterval(autoStatTimer);
  if (reader) { try { await reader.releaseLock(); } catch {} reader = null; }
  if (writer) { try { await writer.releaseLock(); } catch {} writer = null; }
  if (port) { try { await port.close(); } catch {} port = null; }
  setUi(false);
  running = false;
  log.m(I18N.t("log.disconnected"));
}

/* ============================ UI ============================ */
function setUi(on) {
  $("btn-disconnect").disabled = !on;
  $("btn-apply").disabled = !on;
  $("btn-start").disabled = !on;
  $("btn-stop").disabled = !on;
  $("btn-stat").disabled = !on;
  $("btn-reset").disabled = !on;
  $("btn-export-csv").disabled = !DATA[currentMode()].length;
  $("btn-export-json").disabled = !DATA.click.length && !DATA.motion.length;
  $("btn-clear").disabled = !DATA.click.length && !DATA.motion.length;
  const st = $("conn-state");
  st.className = "state " + (on ? "ok" : "idle");
  st.textContent = on ? I18N.t("conn.stateOk") : I18N.t("conn.stateIdle");
  $("live-dot").classList.toggle("on", on);
}

function cfgNote() {
  const pull = $("cfg-t0pull").value;
  const edge = $("cfg-edge").value;
  const note = $("cfg-note");
  let key = "note.ok";
  if (pull === "up" && edge === "f") key = "note.upFall";
  else if (pull === "f" && edge === "r") key = "note.fRise";
  else if (pull === "f") key = "note.fFloat";
  note.textContent = I18N.t(key);
  note.classList.toggle("bad", key === "note.fFloat");
}

/* ============================ eventos UI ============================ */
function bindUI() {
  $("btn-connect").onclick = connect;
  $("btn-disconnect").onclick = disconnect;

  $("btn-apply").onclick = async () => {
    await sendLine(`CONF ${$("cfg-t0pull").value} ${$("cfg-edge").value} ${$("cfg-mode").value} ${$("cfg-holdoff").value} ${$("cfg-timeout").value}`);
  };
  $("btn-start").onclick = async () => { running = true; startedAt = Date.now(); await sendLine("START"); };
  $("btn-stop").onclick  = async () => { running = false; await sendLine("STOP"); };
  $("btn-stat").onclick  = async () => { await sendLine("STAT"); };
  $("btn-reset").onclick = async () => {
    $("cfg-holdoff").value = 2000;
    $("cfg-timeout").value = 100000;
    await sendLine("RESET");
  };

  ["cfg-t0pull", "cfg-edge", "cfg-mode", "cfg-holdoff", "cfg-timeout"].forEach(
    (id) => $(id).addEventListener("change", cfgNote));
  cfgNote();

  $("btn-export-csv").onclick = exportCsv;
  $("btn-export-json").onclick = exportJson;
  $("btn-clear").onclick = () => {
    DATA.click.length = 0; DATA.motion.length = 0;
    stCounters = { total: 0, timeouts: 0, drops: 0 };
    setUi(false); setUi(true); redraw();
  };

  // redraw em intervalos curtos para acompanhar tráfego alto
  setInterval(redraw, 250);

  // re-traduz conteúdo dinâmico ao trocar o idioma
  window.addEventListener("i18n:change", () => {
    cfgNote();
    setUi(Boolean(port));
    if (!("serial" in navigator)) $("conn-hint").textContent = I18N.t("ws.notAvail");
  });
}

/* ============================ export ============================ */
function exportCsv() {
  const arr = DATA[currentMode()];
  const lines = ["seq,mode,t0_ts,t1_ts,lat_ns", ...arr.map(s =>
    `${s.seq},${currentMode()},${s.t0},${s.t1},${s.lat}`)];
  download(blob(lines.join("\n")), `${I18N.t("exp.csvName")}_${currentMode()}_${ts()}.csv`, "text/csv");
}

function exportJson() {
  const obj = {
    firmware: "medidor-rp2350 v1.0.0",
    exported: new Date().toISOString(),
    config: readConfig(),
    stats: {
      total: stCounters.total, timeouts: stCounters.timeouts, drops: stCounters.drops,
      click: statsOf(DATA.click), motion: statsOf(DATA.motion)
    },
    click: DATA.click, motion: DATA.motion
  };
  download(blob(JSON.stringify(obj, null, 1)), `${I18N.t("exp.csvName")}_all_${ts()}.json`, "application/json");
}

function readConfig() {
  return {
    t0pull: $("cfg-t0pull").value, edge: $("cfg-edge").value,
    mode: $("cfg-mode").value, holdoff_us: Number($("cfg-holdoff").value),
    timeout_us: Number($("cfg-timeout").value)
  };
}

function blob(s, type) { return new Blob([s], { type }); }
function ts() { return new Date().toISOString().replace(/[:.]/g, "-"); }
function download(b, name, type) {
  const a = document.createElement("a");
  a.href = URL.createObjectURL(b); a.download = name; a.click();
  URL.revokeObjectURL(a.href);
}

/* ============================ redraw ============================ */
let lastRedraw = 0;
function redraw() {
  const now = Date.now();
  if (now - lastRedraw < 120) return;
  lastRedraw = now;
  refreshStats();
  drawScatter();
  drawHistogram();
  setUi(Boolean(port));
}

/* ============================ init ============================ */
if (!("serial" in navigator)) {
  $("ws-warn").classList.remove("hidden");
  $("btn-connect").disabled = true;
  $("conn-hint").textContent = I18N.t("ws.notAvail");
}
bindUI();
redraw();