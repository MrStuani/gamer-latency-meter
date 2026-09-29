/* Medidor de Latência — testes do gerador de relatórios (node tests/report.test.mjs). */

import { createRequire } from "node:module";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import { writeFileSync } from "node:fs";

const root = join(dirname(fileURLToPath(import.meta.url)), "..");
const Report = createRequire(import.meta.url)(join(root, "report.js"));

let pass = 0, fail = 0;
function ok(name, cond, got) {
  if (cond) { pass++; return; }
  fail++;
  console.error("FAIL " + name + (got === undefined ? "" : "\n      got: " + JSON.stringify(got)));
}
const eq = (name, got, want) => ok(name, got === want, got);

const mk = (lats, mode) => lats.map((lat, i) => ({ seq: i + 1, t0: 0, t1: 0, lat, mode }));

/* ---------- latency() ---------- */
eq("lat 0",            Report.latency(0),        "0.00 \u00b5s");
eq("lat 500ns",        Report.latency(500),      "0.50 \u00b5s");
eq("lat 50us",         Report.latency(50000),    "50.00 \u00b5s");
eq("lat 250us -> 1dec",Report.latency(250000),   "250.0 \u00b5s");
eq("lat 999999ns->us", Report.latency(999999),   "1000.0 \u00b5s");
eq("lat 1e6 -> ms",    Report.latency(1000000),  "1.00 ms");
eq("lat 12345678ns",   Report.latency(12345678), "12.35 ms");
eq("lat null",         Report.latency(null),     "\u2014");
eq("lat undefined",    Report.latency(undefined),"\u2014");

/* ---------- unitOf() ---------- */
eq("unit below 1ms",   Report.unitOf(999999).suffix, "us");
eq("unit at 1ms",      Report.unitOf(1000000).suffix, "ms");
eq("unit far above",   Report.unitOf(5e7).suffix,     "ms");
eq("unit empty data",  Report.unitOf(null).suffix,    "ms");
eq("unit div us",      Report.unitOf(1).div,          1000);
eq("unit div ms",      Report.unitOf(2e6).div,        1e6);

/* ---------- quantile() ---------- */
eq("q50 of 1..10",  Report.quantile([1,2,3,4,5,6,7,8,9,10], 0.5),  5);
eq("q95 of 1..10",  Report.quantile([1,2,3,4,5,6,7,8,9,10], 0.95), 10);
eq("q99 of 1..10",  Report.quantile([1,2,3,4,5,6,7,8,9,10], 0.99), 10);
eq("q50 of [7]",    Report.quantile([7], 0.5), 7);
eq("q50 empty",     Report.quantile([], 0.5), null);

/* ---------- statsOf() ---------- */
const s10 = Report.statsOf(mk([10,1,5,2,9,3,8,4,7,6]));
ok("stats n",        s10.n === 10, s10.n);
eq("stats min",      s10.min, 1);
eq("stats max",      s10.max, 10);
eq("stats p50",      s10.p50, 5);
eq("stats p95",      s10.p95, 10);
eq("stats first",    s10.first, 10);
eq("stats last",     s10.last, 6);
ok("stats mean",     Math.abs(s10.mean - 5.5) < 1e-9, s10.mean);
ok("stats std",      Math.abs(s10.std - Math.sqrt(8.25)) < 1e-9, s10.std);
eq("stats empty",    Report.statsOf([]), null);

/* ---------- histogram() ---------- */
const flat = Report.histogram(mk([5,5,5,5]), 20);
ok("hist flat single bin", flat.length === 1 && flat[0].count === 4, flat);
const hb = Report.histogram(mk([0,1,2,3,4,5,6,7,8,9]), 5);
eq("hist bins", hb.length, 5);
eq("hist total", hb.reduce((m, b) => m + b.count, 0), 10);
eq("hist first bin lo", hb[0].lo, 0);
ok("hist contiguous", hb.every((b, i) => i === 0 || Math.abs(b.lo - hb[i-1].hi) < 1e-9), hb.map(b => b.lo));
eq("hist empty", Report.histogram([], 20).length, 0);

/* ---------- histograma limitado ao p99 ---------- */
const tailed = Array.from({ length: 100 }, () => 1000000).concat([10000000]);
const th = Report.histogram(tailed, 20, Report.statsOf(mk(tailed)).p99);
ok("p99 corta a cauda", th[th.length - 1].hi <= 1000000, th[th.length - 1]);
eq("cauda some na ultima faixa", th.reduce((m, b) => m + b.count, 0), 101);
ok("sem faixa vazia com p99", th.every((b) => b.count > 0), th.filter((b) => !b.count).length);

/* ---------- build() ---------- */
const ctx = {
  data: { click: mk([412000, 845000, 1120000, 1588000, 881000], "click"),
          motion: mk([2500000, 3100000], "motion") },
  counters: { total: 7, timeouts: 3, drops: 1 },
  dev: { ver: "medidor-rp2350 v1.0.0", hz: 150000000, order: 2, stray: 4 },
  config: { t0pull: "up", edge: "f", mode: "c", holdoff_ms: 200, holdoff_us: 200000, timeout_us: 100000 },
  elapsedMs: 204000,
  now: new Date(2026, 8, 27, 14, 3, 22)
};
const out = Report.build(ctx);

ok("build txt", typeof out.txt === "string" && out.txt.length > 200);
ok("build md",  typeof out.md  === "string" && out.md.length  > 200);
ok("build csv", typeof out.csv === "string" && out.csv.length > 100);
ok("build json",typeof out.json === "string");

ok("txt tem firmware",  out.txt.includes("medidor-rp2350 v1.0.0"), out.txt.slice(0, 300));
ok("txt tem data",      out.txt.includes("2026-09-27 14:03:22"));
ok("txt tem resolucao", out.txt.includes("~13.3 ns"), out.txt.match(/Resolution.*/)?.[0]);
ok("txt tem clock",     out.txt.includes("150 MHz"));
ok("txt tem 2 secoes",  out.txt.includes("CLICK (T1_CLICK)") && out.txt.includes("MOTION (T1_MOTION)"));
ok("txt tem discarded", /timeouts: 3.*drops: 1.*order: 2.*stray: 4/.test(out.txt),
   out.txt.match(/Discarded.*/)?.[0]);
ok("txt tem histograma",out.txt.includes("\u2588"));
ok("txt tem piores",    /Worst \d+ samples/.test(out.txt));
ok("txt click em us",   out.txt.includes("412.0 \u00b5s"), out.txt.match(/.*412.*/)?.[0]);
ok("txt motion em ms",  out.txt.includes("2.50 ms"));
ok("txt elapsed",       out.txt.includes("3.4 min"), out.txt.match(/Elapsed.*/)?.[0]);

ok("md tem tabela",  out.md.includes("| Summary | latency |"), out.md.match(/\| .* \| .* \|/)?.[0]);
ok("md tem 2 secoes",out.md.includes("## CLICK (T1_CLICK)") && out.md.includes("## MOTION (T1_MOTION)"));
ok("md tem hist",    out.md.includes("### Distribution"));
ok("md tem piores",  out.md.includes("### Worst"));

ok("csv tem BOM",    out.csv.charCodeAt(0) === 0xfeff, out.csv.charCodeAt(0));
ok("csv 2 unidades", out.csv.includes('"sample","seq","latency_us"') &&
                      out.csv.includes('"sample","seq","latency_ms"'),
   out.csv.match(/"sample","seq".*/g));
ok("csv 2 blocos",   (out.csv.match(/"sample","seq"/g) || []).length === 2);
ok("csv sem ns cru", !out.csv.includes("lat_ns") && !out.csv.includes("t0_ts"));
const dataRows = out.csv.split("\n").filter((l) => /^\d+,\d+\.\d\d$/.test(l));
ok("csv linhas de dados", dataRows.length === 7, dataRows.length);
ok("csv config",     out.csv.includes("# Config:"), out.csv.split("\n").slice(0, 8));
ok("csv sem rotulo vazio", !/^# :/m.test(out.csv), out.csv.match(/^# :.*/m)?.[0]);
ok("csv comentario de resumo por eixo",
   out.csv.includes("# Summary: 5 samples") && out.csv.includes("# Summary: 2 samples"),
   out.csv.match(/# Summary.*/g));

const j = JSON.parse(out.json);
eq("json schema",    j.schema, "gamer-latency-meter/report@1");
eq("json lang",      j.language, "en");
eq("json firmware",  j.firmware, "medidor-rp2350 v1.0.0");
eq("json clock",     j.clock_hz, 150000000);
eq("json res",       j.resolution_ns, 13.33);
eq("json elapsed",   j.elapsed_ms, 204000);
eq("json cfg pull",  j.config.t0pull, "up");
eq("json cfg holdoff", j.config.holdoff_ms, 200);
eq("json cnt total", j.counters.total, 7);
eq("json cnt strays",j.counters.stray, 4);
ok("json eixos",     j.axes.click && j.axes.motion);
eq("json axis n",    j.axes.click.samples, 5);
eq("json axis unit", j.axes.click.unit, "ns");
eq("json axis min",  j.axes.click.stats.min, 412000);
eq("json axis mean", j.axes.click.stats.mean, 969200);
ok("json display",    j.axes.click.display.min === "412.0 \u00b5s" &&
                      j.axes.motion.display.max === "3.10 ms",
   [j.axes.click.display.min, j.axes.motion.display.max]);
eq("json readings n",j.axes.motion.readings.length, 2);
eq("json reading",   j.axes.motion.readings[0].latency_ns, 2500000);
ok("json sem t0/t1", !("t0" in j.axes.click.readings[0]));

/* ---------- eixos vazios ---------- */
const one = Report.build({ data: { click: mk([1000, 2000]), motion: [] }, counters: {}, dev: {}, config: {} });
ok("1 eixo no txt",   one.txt.includes("CLICK (T1_CLICK)") && one.txt.includes("no samples in this axis"),
   one.txt.match(/MOTION[\s\S]{0,80}/)?.[0]);
ok("1 eixo no json",  JSON.parse(one.json).axes.motion.samples === 0);
eq("1 eixo stats",    JSON.parse(one.json).axes.motion.stats, null);
ok("sub-ms usa us",   one.csv.includes('"sample","seq","latency_us"') && one.csv.includes("1,1.00"),
   one.csv.match(/".*","seq".*/)?.[0]);
ok("1 eixo sem seq",  !one.txt.includes("1,0.00"));

const none = Report.build({ data: {}, counters: {}, dev: {}, config: {} });
ok("sem dados nao quebra", none.txt.length > 100 && JSON.parse(none.json).axes.click.samples === 0);
ok("sem dados cfg vazio", none.txt.includes("\u2014"), none.txt.split("\n").slice(0, 12));

/* ---------- configs estranhos ---------- */
const weird = Report.build({ data: { click: mk([1]) }, dev: { hz: 0, ver: "" }, config: { t0pull: "?" } });
ok("hz 0 sem crash", weird.txt.includes("unknown"), weird.txt.match(/Firmware.*/)?.[0]);

/* ---------- escapes de CSV ---------- */
const quoted = Report.build({ data: { click: mk([1000]) }, config: { t0pull: 'a"b' } });
const notes = quoted.csv.split("\n").filter((l) => l.startsWith("#"));
ok("csv sem aspas cruas em comentario", !notes.some((l) => l.includes('"')), notes);
ok("csv sanitiza valor",                quoted.csv.includes("a'b"), notes.find((l) => l.includes("Config:")));

/* ---------- amostra de 4000 ---------- */
const big = Array.from({ length: 4000 }, (_, i) => 400000 + (i % 900) * 1000);
const t0 = Date.now();
const bigOut = Report.build({ data: { click: mk(big), motion: [] }, counters: { total: 4000 }, dev: {}, config: {} });
const ms = Date.now() - t0;
ok("4000 amostras < 2s", ms < 2000, ms + "ms");
ok("4000 amostras p50",  JSON.parse(bigOut.json).axes.click.stats.p50 >= 400000);
eq("4000 samples count", JSON.parse(bigOut.json).axes.click.samples, 4000);

/* ---------- o schema do CSV nao depende do idioma ---------- */
/* Só o schema de dados é fixo em inglês; os rótulos humanos ficam nas linhas #.
   Uma tabela pt sintética basta para provar a separação — a paridade real das
   chaves é garantida por tests/i18n.test.mjs. */
const dataLines = (csv) => csv.split("\n").filter((l) => /^"sample"/.test(l) || /^\d+,\d+\.\d\d$/.test(l));
globalThis.I18N_STRINGS = {
  en: {},
  pt: {
    "rep.title": "RELATÓRIO DE LATÊNCIA DO MOUSE",
    "rep.axis.click": "CLIQUE (T1_CLICK)",
    "rep.axis.motion": "MOVIMENTO (T1_MOTION)",
    "rep.summary": "Resumo", "rep.col.axis": "eixo", "rep.unit": "Unidade",
    "rep.min": "mínimo", "rep.p50": "mediana", "rep.mean": "média",
    "rep.p95": "p95", "rep.p99": "p99", "rep.max": "máximo", "rep.std": "desvio padrão",
    "rep.amp": "{n} amostras"
  }
};
globalThis.I18N = { lang: "pt" };
const outPt = Report.build(ctx);
ok("csv: schema de dados identico en vs pt",
   dataLines(out.csv).join("\n") === dataLines(outPt.csv).join("\n"),
   [dataLines(out.csv)[0], dataLines(outPt.csv)[0]]);
ok("csv: metadados diferem por idioma",
   out.csv.includes("# Summary:") && outPt.csv.includes("# Resumo:") &&
   out.csv.includes("# axis: CLICK") && outPt.csv.includes("# eixo: CLIQUE"),
   [out.csv.match(/^# (Summary|axis).*/m)?.[0], outPt.csv.match(/^# (Resumo|eixo).*/m)?.[0]]);
ok("csv: numero cru mantem ponto em pt", outPt.csv.includes("412,412.00") || outPt.csv.includes(",412.00"),
   outPt.csv.match(/^\d+,\d+\.\d\d$/m)?.[0]);
ok("txt: separador decimal segue o idioma", out.txt.includes("412.0 µs") &&
   outPt.txt.includes("412,0 µs"), [out.txt.match(/.*412\.0.*/)?.[0], outPt.txt.match(/.*412,0.*/)?.[0]]);
ok("json: display localizado, numero cru em ns", JSON.parse(outPt.json).axes.click.display.min === "412,0 µs" &&
   JSON.parse(outPt.json).axes.click.stats.min === 412000,
   JSON.parse(outPt.json).axes.click.display.min);

/* ---------- num() segue o locale; numRaw() nunca ---------- */
globalThis.I18N = { lang: "en" };
eq("num en", Report.num(824, 1), "824.0");
globalThis.I18N = { lang: "pt" };
eq("num pt", Report.num(824, 1), "824,0");
globalThis.I18N = { lang: "de" };
eq("num de", Report.num(1234.5, 2), "1234,50");
globalThis.I18N = { lang: "ja" };
eq("num ja", Report.num(824, 1), "824.0");
/* Empate exato: o Intl arredonda a meio-para-cima, o toFixed trunca o binário
   (1.005 vale 1,0049999… em ponto flutuante). Os dois divergem em 0,01 e o Intl
   está certo — por isso a exibição e o dado cru não são o mesmo caminho. */
ok("num arredonda empate para cima", Report.num(1.005, 2) === "1.01" &&
   Report.numRaw(1.005, 2) === "1.00", [Report.num(1.005, 2), Report.numRaw(1.005, 2)]);
ok("num: sem separador de milhar", Report.num(10000, 1) === "10000.0", Report.num(10000, 1));
ok("numRaw: ponto sempre", Report.numRaw(824, 1) === "824.0" && Report.numRaw(1234.5, 2) === "1234.50");
delete globalThis.I18N;
delete globalThis.I18N_STRINGS;

/* ---------- saida de inspecao ---------- */
if (process.argv.includes("--write")) {
  for (const k of ["txt", "md", "csv", "json"]) {
    writeFileSync(join(root, "tests", "out." + k), out[k]);
  }
  console.log("escrito em tests/out.{txt,md,csv,json}");
}

console.log((fail ? "FALHOU " : "ok ") + pass + " passed, " + fail + " failed");
process.exit(fail ? 1 : 0);
