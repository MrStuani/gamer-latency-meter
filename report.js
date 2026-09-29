/* Medidor de Latência RP2350 — geração de relatórios legíveis (sem dependências). */

"use strict";

/* ============================ rótulos ============================ */
/* en-US até as chaves rep.* chegarem ao i18n.js (fallback automático). */
const REP_DEFAULTS = {
  "rep.title": "MOUSE LATENCY REPORT",
  "rep.axis.click": "CLICK (T1_CLICK)",
  "rep.axis.motion": "MOTION (T1_MOTION)",
  "rep.noData": "no samples in this axis",
  "rep.date": "Date",
  "rep.firmware": "Firmware",
  "rep.config": "Config",
  "rep.clock": "Clock",
  "rep.resolution": "Resolution",
  "rep.elapsed": "Elapsed",
  "rep.samples": "Samples",
  "rep.discarded": "Discarded",
  "rep.summary": "Summary",
  "rep.distribution": "Distribution",
  "rep.worst": "Worst {n} samples",
  "rep.amp": "{n} samples",
  "rep.unit": "Unit",
  "rep.min": "minimum",
  "rep.p50": "median",
  "rep.mean": "mean",
  "rep.p95": "p95",
  "rep.p99": "p99",
  "rep.max": "maximum",
  "rep.std": "std deviation",
  "rep.seq": "seq",
  "rep.latency": "latency",
  "rep.timeouts": "timeouts",
  "rep.drops": "drops",
  "rep.order": "order",
  "rep.stray": "stray",
  "rep.unknown": "unknown",
  "rep.col.axis": "axis",

  "cfg.t0pull.label": "T0 pull / physical",
  "cfg.t0pull.f": "float (no pull)",
  "cfg.t0pull.up": "pull-up (action drives T0 low)",
  "cfg.t0pull.down": "pull-down (action drives T0 high)",
  "cfg.edge.label": "T0 edge (\"action\")",
  "cfg.edge.r": "rising",
  "cfg.edge.f": "falling",
  "cfg.mode.label": "Axis under test",
  "cfg.mode.c": "Click (T1_CLICK)",
  "cfg.mode.m": "Motion (T1_MOTION)",
  "cfg.holdoff.label": "Holdoff (ms)",
  "cfg.timeout.label": "Timeout / watchdog (\u00b5s)"
};

const Report = (() => {
  const US_DIV = 1000, MS_DIV = 1e6, NO = "\u2014", RULE = 60, BINS = 20, WORST = 20, BAR = 30;
  const EXT = { txt: "txt", md: "md", csv: "csv", json: "json" };
  const MIME = {
    txt: "text/plain;charset=utf-8",
    md: "text/markdown;charset=utf-8",
    csv: "text/csv;charset=utf-8",
    json: "application/json;charset=utf-8"
  };

  /* ============================ rótulos ============================ */
  function label(key, vars) {
    let s;
    const table = (typeof I18N_STRINGS !== "undefined" && typeof I18N !== "undefined")
      ? I18N_STRINGS[I18N.lang] : null;
    const en = (typeof I18N_STRINGS !== "undefined") ? I18N_STRINGS.en : null;
    if (table && table[key] !== undefined) s = table[key];
    else if (en && en[key] !== undefined) s = en[key];
    else s = REP_DEFAULTS[key];
    if (s === undefined) return key;
    if (vars) for (const k of Object.keys(vars)) s = s.split("{" + k + "}").join(String(vars[k]));
    return s;
  }

  /* ============================ números ============================ */
  /* A exibição segue o idioma da UI (vírgula em pt/de/es/fr/it/ru). O dado cru
     — coluna do CSV e números do JSON — mantém o ponto: é o que esses formatos
     exigem para continuar legível por máquina. useGrouping:false é de propósito:
     latência nunca passa de 1000, e "10.000 amostras" colado num fórum
     internacional se lê como dez vírgula zero. */
  const NUM_LOCALE = {
    en: "en", pt: "pt-BR", es: "es", fr: "fr", de: "de",
    it: "it", zh: "zh-CN", ja: "ja", ko: "ko", ru: "ru"
  };
  const NF = new Map();

  function numLocale() {
    const code = (typeof I18N !== "undefined" && I18N.lang) ? I18N.lang : "en";
    return NUM_LOCALE[code] || "en";
  }

  function num(v, digits) {
    if (typeof Intl === "undefined" || !Intl.NumberFormat) return v.toFixed(digits);
    const key = numLocale() + ":" + digits;
    let f = NF.get(key);
    if (!f) {
      f = new Intl.NumberFormat(numLocale(), {
        minimumFractionDigits: digits, maximumFractionDigits: digits, useGrouping: false
      });
      NF.set(key, f);
    }
    return f.format(v);
  }

  const numRaw = (v, digits) => v.toFixed(digits);

  /* ============================ latência ============================ */
  function latency(ns) {
    if (ns === null || ns === undefined || !isFinite(ns)) return NO;
    if (ns < MS_DIV) { const v = ns / US_DIV; return num(v, v < 100 ? 2 : 1) + " \u00b5s"; }
    return num(ns / MS_DIV, 2) + " ms";
  }

  function unitOf(refNs) {
    return (refNs !== null && refNs !== undefined && isFinite(refNs) && refNs < MS_DIV)
      ? { suffix: "us", div: US_DIV, label: "\u00b5s" }
      : { suffix: "ms", div: MS_DIV, label: "ms" };
  }

  /* cru: coluna de dados do CSV e campos numéricos do JSON. */
  const raw = (ns, u) => (ns / u.div).toFixed(2);
  /* exibição: rótulo de faixa do histograma em .txt/.md. */
  const rawView = (ns, u) => num(ns / u.div, 2);

  /* ============================ estatísticas ============================ */
  function quantile(sorted, q) {
    const n = sorted.length;
    if (!n) return null;
    let i = Math.ceil(q * n) - 1;
    if (i < 0) i = 0;
    if (i > n - 1) i = n - 1;
    return sorted[i];
  }

  function statsOf(arr) {
    const n = arr.length;
    if (!n) return null;
    const v = new Array(n);
    let min = Infinity, max = -Infinity, sum = 0;
    for (let i = 0; i < n; i++) {
      const x = arr[i].lat;
      v[i] = x;
      if (x < min) min = x;
      if (x > max) max = x;
      sum += x;
    }
    v.sort((a, b) => a - b);
    const mean = sum / n;
    let s2 = 0;
    for (let i = 0; i < n; i++) s2 += (v[i] - mean) ** 2;
    return {
      n, min, max, mean,
      p50: quantile(v, 0.5), p95: quantile(v, 0.95), p99: quantile(v, 0.99),
      std: Math.sqrt(s2 / n),
      first: arr[0].lat, last: arr[n - 1].lat
    };
  }

  function histogram(arr, bins, topNs) {
    const n = arr.length;
    if (!n) return [];
    let min = Infinity, max = -Infinity;
    for (const s of arr) { if (s.lat < min) min = s.lat; if (s.lat > max) max = s.lat; }
    const top = (topNs === undefined || topNs === null || topNs < min) ? max : topNs;
    const span = top - min;
    if (span <= 0) return [{ lo: min, hi: top, count: n }];
    const out = [];
    for (let i = 0; i < bins; i++) {
      const lo = min + (span * i) / bins, hi = min + (span * (i + 1)) / bins;
      let count = 0;
      for (const s of arr) if (s.lat >= lo && (i === bins - 1 ? true : s.lat < hi)) count++;
      out.push({ lo, hi, count });
    }
    return out;
  }

  /* ============================ contexto ============================ */
  function stamp(d) {
    const p = (x) => String(x).padStart(2, "0");
    return d.getFullYear() + "-" + p(d.getMonth() + 1) + "-" + p(d.getDate()) +
           " " + p(d.getHours()) + ":" + p(d.getMinutes()) + ":" + p(d.getSeconds());
  }

  function clock(ms) {
    if (!ms || ms < 0) return NO;
    const s = ms / 1000;
    return s < 90 ? num(s, 1) + " s" : num(s / 60, 1) + " min";
  }

  const shortAxis = (l) => l.replace(/\s*\(.*\)/, "");

  function configLines(cfg) {
    const c = cfg || {};
    const pick = (map, v) => (map[v] !== undefined ? map[v] : (v === undefined || v === null ? NO : v));
    return [
      [label("cfg.t0pull.label"), pick({
        f: label("cfg.t0pull.f"), up: label("cfg.t0pull.up"), down: label("cfg.t0pull.down")
      }, c.t0pull)],
      [label("cfg.edge.label"), pick({ r: label("cfg.edge.r"), f: label("cfg.edge.f") }, c.edge)],
      [label("cfg.mode.label"), pick({ c: label("cfg.mode.c"), m: label("cfg.mode.m") }, c.mode)],
      [label("cfg.holdoff.label"), (c.holdoff_ms === undefined ? NO : c.holdoff_ms)],
      [label("cfg.timeout.label"), (c.timeout_us === undefined ? NO : c.timeout_us)]
    ];
  }

  function analyze(ctx) {
    const dev = ctx.dev || {}, cnt = ctx.counters || {}, c = ctx.config || {};
    const axes = [
      { key: "click", label: label("rep.axis.click"), data: (ctx.data && ctx.data.click) || [] },
      { key: "motion", label: label("rep.axis.motion"), data: (ctx.data && ctx.data.motion) || [] }
    ];
    for (const x of axes) {
      x.stats = statsOf(x.data);
      x.unit = unitOf(x.stats ? x.stats.p50 : 0);
      x.bins = histogram(x.data, BINS, x.stats ? x.stats.p99 : null);
      x.tailed = x.stats ? x.stats.max > x.stats.p99 : false;
      x.worst = x.data.slice().sort((p, q) => q.lat - p.lat).slice(0, WORST);
      x.peakCount = x.bins.reduce((m, b) => (b.count > m ? b.count : m), 0);
    }
    const counters = {
      total: cnt.total || 0,
      timeouts: cnt.timeouts || 0,
      drops: cnt.drops || 0,
      order: dev.order || 0,
      stray: dev.stray || 0
    };
    const discarded = [
      [label("rep.timeouts"), counters.timeouts],
      [label("rep.drops"), counters.drops],
      [label("rep.order"), counters.order],
      [label("rep.stray"), counters.stray]
    ];
    return {
      axes, counters,
      when: (ctx.now instanceof Date) ? ctx.now : new Date(),
      lang: (typeof I18N !== "undefined" && I18N.lang) ? I18N.lang : "en",
      firmware: dev.ver || label("rep.unknown"),
      hz: dev.hz || null,
      resolution: dev.hz ? 2e9 / dev.hz : null,
      elapsed: ctx.elapsedMs || 0,
      cfg: c,
      cfgLines: configLines(c),
      discarded,
      discardLine: discarded.map(([k, v]) => k + ": " + v).join("  \u00b7  "),
      sampleLine: axes.map((x) => shortAxis(x.label) + " " + (x.stats ? x.stats.n : 0)).join("  \u00b7  ")
    };
  }

  /* ============================ .txt ============================ */
  const pad = (k, v, w) => "  " + String(k).padEnd(w) + " " + String(v);
  const range = (b, u) => rawView(b.lo, u) + "\u2013" + rawView(b.hi, u);
  const binLabel = (b, x) => (x.tailed && b === x.bins[x.bins.length - 1]
    ? "\u2265 " + rawView(b.lo, x.unit) : range(b, x.unit));
  const widthOf = (rows) => Math.min(28, rows.reduce((m, [k]) => Math.max(m, String(k).length), 0));
  const ROWS = (s) => [[label("rep.min"), s.min], [label("rep.p50"), s.p50], [label("rep.mean"), s.mean],
                       [label("rep.p95"), s.p95], [label("rep.p99"), s.p99],
                       [label("rep.max"), s.max], [label("rep.std"), s.std]];

  function txtBuild(a) {
    const L = [];
    const eq = "\u2550".repeat(RULE), dash = "\u2500";
    L.push(eq, " " + label("rep.title"), eq);
    const head = [
      [label("rep.date"), stamp(a.when)],
      [label("rep.firmware"), a.firmware],
      [label("rep.clock"), a.hz ? (a.hz / 1e6) + " MHz" : NO],
      [label("rep.resolution"), a.resolution ? "~" + num(a.resolution, 1) + " ns" : NO],
      [label("rep.elapsed"), clock(a.elapsed)]
    ];
    const w = widthOf(head.concat(a.cfgLines));
    for (const [k, v] of head) L.push(pad(k, v, w));
    L.push("");
    for (const [k, v] of a.cfgLines) L.push(pad(k, v, w));
    L.push("");
    L.push(pad(label("rep.samples"), a.sampleLine, w));
    L.push(pad(label("rep.discarded"), a.discardLine, w));

    for (const x of a.axes) {
      L.push("");
      const title = " " + x.label + " ";
      L.push(title + dash.repeat(Math.max(3, RULE - title.length)));
      if (!x.stats) { L.push("  " + label("rep.noData")); continue; }
      const s = x.stats;
      const rows = ROWS(s);
      const sw = widthOf(rows);
      L.push("  " + label("rep.summary") + "  (" + label("rep.amp", { n: s.n }) + ")");
      for (const [k, v] of rows) L.push(pad(k, latency(v), sw + 2));
      L.push("", "  " + label("rep.distribution") + "  (" + x.unit.label + ")");
      for (const b of x.bins) {
        if (!b.count) continue;
        const fill = Math.max(1, Math.round((b.count / x.peakCount) * BAR));
        L.push("    " + binLabel(b, x).padEnd(20) + "\u2588".repeat(fill).padEnd(BAR + 2) + b.count);
      }
      L.push("", "  " + label("rep.worst", { n: x.worst.length }));
      for (const r of x.worst) L.push("    #" + String(r.seq).padEnd(9) + latency(r.lat));
    }
    return L.join("\n") + "\n";
  }

  /* ============================ .md ============================ */
  function mdBuild(a) {
    const L = ["# " + label("rep.title"), ""];
    for (const [k, v] of [
      [label("rep.date"), stamp(a.when)],
      [label("rep.firmware"), a.firmware],
      [label("rep.elapsed"), clock(a.elapsed)]
    ]) L.push("- **" + k + ":** " + v);
    if (a.hz) {
      L.push("- **" + label("rep.clock") + ":** " + (a.hz / 1e6) + " MHz",
             "- **" + label("rep.resolution") + ":** ~" + num(a.resolution, 1) + " ns");
    }
    L.push("");
    for (const [k, v] of a.cfgLines) L.push("- **" + k + ":** " + v);
    L.push("");
    L.push("- **" + label("rep.samples") + ":** " + a.sampleLine);
    L.push("- **" + label("rep.discarded") + ":** " + a.discardLine);

    for (const x of a.axes) {
      L.push("", "## " + x.label, "");
      if (!x.stats) { L.push("_" + label("rep.noData") + "_"); continue; }
      const s = x.stats;
      L.push("_" + label("rep.amp", { n: s.n }) + "_", "");
      L.push("| " + label("rep.summary") + " | " + label("rep.latency") + " |", "| :--- | ---: |");
      for (const [k, v] of ROWS(s)) L.push("| " + k + " | " + latency(v) + " |");
      L.push("", "### " + label("rep.distribution"), "",
        "| " + x.unit.label + " | " + label("rep.samples") + " |", "| :--- | ---: |");
      for (const b of x.bins) if (b.count) L.push("| " + binLabel(b, x) + " | " + b.count + " |");
      L.push("", "### " + label("rep.worst", { n: x.worst.length }), "",
        "| " + label("rep.seq") + " | " + label("rep.latency") + " |", "| ---: | ---: |");
      for (const r of x.worst) L.push("| " + r.seq + " | " + latency(r.lat) + " |");
    }
    return L.join("\n") + "\n";
  }

  /* ============================ .csv ============================ */
  function csvBuild(a) {
    const L = [];
    const quote = (v) => '"' + String(v).replace(/"/g, '""') + '"';
    const note = (k, v) => ("# " + k + (v === "" ? "" : ": " + v)).replace(/"/g, "'");
    L.push(note(label("rep.title"), ""));
    L.push(note(label("rep.date"), stamp(a.when)));
    L.push(note(label("rep.firmware"), a.firmware));
    if (a.hz) L.push(note(label("rep.clock"), (a.hz / 1e6) + " MHz"));
    L.push(note(label("rep.config"), a.cfgLines.map(([k, v]) => k + "=" + v).join(" | ")));
    L.push(note(label("rep.elapsed"), clock(a.elapsed)));
    L.push(note(label("rep.discarded"), a.discardLine.replace(/\s*·\s*/g, " · ")));
    for (const x of a.axes) {
      L.push("", note(label("rep.col.axis"), x.label + " (" + label("rep.unit") + ": " + x.unit.label + ")"));
      const s = x.stats;
      if (!s) { L.push("# " + label("rep.noData")); continue; }
      L.push(note(label("rep.summary"), [label("rep.amp", { n: s.n })]
        .concat(ROWS(s).map(([k, v]) => k + "=" + latency(v)))
        .join(" · ")));
      /* Coluna de dados em inglês fixo: dois usuários de idiomas diferentes geram
         o mesmo schema. Os rótulos humanos ficam nas linhas # acima. */
      L.push(quote("sample") + "," + quote("seq") + "," + quote("latency_" + x.unit.suffix));
      for (const r of x.data) L.push(r.seq + "," + raw(r.lat, x.unit));
    }
    return "\ufeff" + L.join("\n") + "\n";
  }

  /* ============================ .json ============================ */
  function jsonBuild(a) {
    const c = a.cfg, k = a.counters;
    const obj = {
      schema: "gamer-latency-meter/report@1",
      generated_at: a.when.toISOString(),
      language: a.lang,
      firmware: a.firmware,
      clock_hz: a.hz,
      resolution_ns: (a.resolution === null ? null : Number(a.resolution.toFixed(2))),
      elapsed_ms: a.elapsed,
      config: {
        t0pull: c.t0pull || null, edge: c.edge || null, mode: c.mode || null,
        holdoff_ms: c.holdoff_ms ?? null, holdoff_us: c.holdoff_us ?? null,
        timeout_us: c.timeout_us ?? null
      },
      counters: { total: k.total, timeouts: k.timeouts, drops: k.drops, order: k.order, stray: k.stray },
      axes: {}
    };
    for (const x of a.axes) {
      const s = x.stats;
      obj.axes[x.key] = {
        label: x.label,
        samples: s ? s.n : 0,
        unit: "ns",
        stats: s ? {
          n: s.n, min: s.min, p50: s.p50, mean: Math.round(s.mean), p95: s.p95, p99: s.p99,
          max: s.max, std: Number(s.std.toFixed(1)), first: s.first, last: s.last
        } : null,
        display: s ? {
          min: latency(s.min), p50: latency(s.p50), mean: latency(s.mean), p95: latency(s.p95),
          p99: latency(s.p99), max: latency(s.max), std: latency(s.std)
        } : null,
        readings: x.data.map((r) => ({ seq: r.seq, latency_ns: r.lat }))
      };
    }
    return JSON.stringify(obj, null, 2) + "\n";
  }

  /* ============================ build ============================ */
  function build(ctx) {
    const a = analyze(ctx || {});
    return {
      txt: txtBuild(a), md: mdBuild(a), csv: csvBuild(a), json: jsonBuild(a),
      ext: EXT, mime: MIME,
      axes: a.axes.map((x) => ({ key: x.key, label: x.label, unit: x.unit, stats: x.stats }))
    };
  }

  return { build, label, num, numRaw, latency, unitOf, raw, statsOf, histogram, quantile, stamp, clock, ext: EXT, mime: MIME };
})();

if (typeof module === "object" && module.exports) module.exports = Report;
