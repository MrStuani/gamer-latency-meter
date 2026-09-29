/* Medidor de Latência — teste de integração do app (node tests/app.smoke.mjs).
   Roda i18n.js + report.js + app.js num DOM falso, injeta frames binários como
   o firmware faria e exercita o caminho serial -> DATA -> exportReport -> Blob. */

import vm from "node:vm";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";

const root = join(dirname(fileURLToPath(import.meta.url)), "..");
const src = (f) => readFileSync(join(root, f), "utf8");
const downloads = [];
const cache = new Map();
const nullIds = new Set();
const VALUES = {
  "cfg-mode": "c", "cfg-t0pull": "up", "cfg-edge": "f",
  "cfg-holdoff": "200", "cfg-timeout": "100000"
};

const ctx2d = new Proxy({}, { get: (t, k) => (k in t ? t[k] : (() => {})), set: () => true });
const el = (id) => ({
  id, textContent: "", value: "", disabled: false, innerHTML: "", className: "",
  dataset: {}, style: {}, children: [], width: 920, height: 320,
  classList: {
    _s: new Set(),
    add(...a) { a.forEach((x) => this._s.add(x)); },
    remove(...a) { a.forEach((x) => this._s.delete(x)); },
    toggle(c, f) { f ? this._s.add(c) : this._s.delete(c); },
    contains(c) { return this._s.has(c); }
  },
  addEventListener() {}, appendChild(c) { this.children.push(c); }, prepend(c) { this.children.unshift(c); },
  removeChild() {}, remove() {}, click() {}, getContext: () => ctx2d, setAttribute() {},
  get firstChild() { return this.children[0]; },
  get lastChild() { return this.children[this.children.length - 1]; }
});

const document = {
  readyState: "complete",
  documentElement: { dataset: {}, lang: "en" },
  getElementById(id) {
    if (nullIds.has(id)) return null;
    if (!cache.has(id)) cache.set(id, el(id));
    if (VALUES[id] !== undefined) cache.get(id).value = VALUES[id];
    return cache.get(id);
  },
  querySelectorAll: () => [],
  createElement: (t) => el(t),
  addEventListener() {}
};

const store = new Map();
const sandbox = {
  console, document, setTimeout, setInterval, clearInterval, clearTimeout,
  TextEncoder, TextDecoder, DataView, Blob, CustomEvent,
  navigator: { language: "pt-BR", serial: {} },
  localStorage: {
    getItem: (k) => (store.has(k) ? store.get(k) : null),
    setItem: (k, v) => store.set(k, String(v))
  },
  getComputedStyle: () => ({ getPropertyValue: () => "#000" }),
  URL: { createObjectURL: (b) => { downloads.push(b); return "blob:stub/" + downloads.length; },
         revokeObjectURL() {} },
  window: { addEventListener() {}, dispatchEvent() {} }
};
sandbox.globalThis = sandbox;
vm.createContext(sandbox);
for (const f of ["i18n.js", "report.js", "app.js"]) vm.runInContext(src(f), sandbox, { filename: f });

const get = (expr) => vm.runInContext(expr, sandbox);
let pass = 0, fail = 0;
const ok = (name, cond, got) => {
  if (cond) { pass++; return; }
  fail++;
  console.error("FAIL " + name + (got === undefined ? "" : "\n      " + JSON.stringify(got)));
};
const throws = (name, fn) => {
  try { fn(); ok(name, true); } catch (e) { ok(name, false, String(e && e.message)); }
};

/* ---------- app carregou e traduziu ---------- */
ok("i18n aplicou pt-BR", get("document.documentElement.lang") === "pt", get("document.documentElement.lang"));

/* ---------- todo id que app.js pede existe no index.html ---------- */
const htmlIds = new Set([...src("index.html").matchAll(/\bid="([^"]+)"/g)].map((m) => m[1]));
const wanted = new Set([...src("app.js").matchAll(/\$\("([^"]+)"\)/g)].map((m) => m[1]));
const ghost = [...wanted].filter((id) => !htmlIds.has(id));
ok("todo $() existe no index.html", ghost.length === 0, ghost.join(", "));

/* ---------- frames do firmware -> DATA ---------- */
const frame = (type, mode, seq, lat) => {
  const b = new Uint8Array(20);
  b[0] = 0xaa; b[1] = type; b[2] = mode; b[3] = 0;
  const dv = new DataView(b.buffer);
  dv.setUint32(4, seq, true); dv.setUint32(8, 0xffffff00, true);
  dv.setUint32(12, 0xfffffe00, true); dv.setUint32(16, lat, true);
  return b;
};
for (let i = 0; i < 50; i++) get("pump")(frame(0, 0, i + 1, 800000 + i * 1000));
get("pump")(frame(1, 0, 999, 0));
get("pump")(frame(3, 0, 1000, 0));
ok("50 amostras no eixo click", get("DATA.click.length") === 50, get("DATA.click.length"));
ok("eixo motion vazio", get("DATA.motion.length") === 0);
const cnt = get("stCounters");
ok("contadores total/timeouts/drops", cnt.total === 50 && cnt.timeouts === 1 && cnt.drops === 1, cnt);

/* ---------- texto ASCII: #VER e #STAT alimentam devInfo ---------- */
get("pump")(Buffer.from("#VER medidor-rp2350 v1.0.0\n", "utf8"));
get("pump")(Buffer.from("#STAT run=1 mode=c ok=50 timeouts=1 drops=1 order=2 stray=4 hz=150000000\n", "utf8"));
const dev = get("devInfo");
ok("devInfo.ver", dev.ver === "medidor-rp2350 v1.0.0", dev.ver);
ok("devInfo.hz", dev.hz === 150000000, dev.hz);
ok("devInfo order/stray", dev.order === 2 && dev.stray === 4, dev);

/* ---------- tiles de estatistica (numero segue o idioma) ---------- */
get("refreshStats")();
const tile = (id) => String(cache.get(id).textContent);
ok("tile p50 = mediana", tile("st-p50") === "824,0 \u00b5s", tile("st-p50"));
ok("tile p95", /^\d+,\d+ (ms|\u00b5s)$/.test(tile("st-p95")), tile("st-p95"));
ok("tile count", tile("st-count") === "50", tile("st-count"));

/* ---------- botoes de export ligam e baixam ---------- */
get("bindUI")();
for (const kind of ["txt", "csv", "md", "json"]) {
  const b = cache.get("btn-export-" + kind);
  ok("btn " + kind + " ligado", typeof b.onclick === "function");
}
for (const kind of ["txt", "csv", "md", "json"]) cache.get("btn-export-" + kind).onclick();
ok("4 downloads", downloads.length === 4, downloads.length);
ok("1 blob por formato", new Set(downloads.map((b) => b.type)).size === 4,
   downloads.map((b) => b.type));
const texts = await Promise.all(downloads.map((b) => b.text()));
const [txt, csv, md, json] = texts;
const bytes = new Uint8Array(await downloads[1].arrayBuffer());
const enc = new TextDecoder("utf-8").decode(bytes);

ok("txt localizado", txt.includes("RELAT\u00d3RIO DE LAT\u00caNCIA DO MOUSE") ||
                      txt.includes("RELAT"), txt.split("\n")[1]);
ok("txt sem chave crua", ![txt, csv, md].some((s) => /rep\.|exp\.|cfg\.|stat\./.test(s)));
ok("txt tem config traduzida", txt.includes("Eixo em teste") && txt.includes("Clique (T1_CLICK)"),
   txt.match(/Eixo.*/)?.[0]);
ok("txt tem clock e resolucao", txt.includes("150 MHz") && txt.includes("~13,3 ns"),
   txt.match(/Resolu.*/)?.[0]);
ok("txt tem descartados", /timeouts: 1.*drops: 1.*ordem: 2.*edges \u00f3rf\u00e3s: 4/.test(txt),
   txt.match(/Descartados.*/)?.[0]);
ok("txt tem 2 eixos", txt.includes("CLIQUE (T1_CLICK)") && txt.includes("MOVIMENTO (T1_MOTION)"));
ok("txt eixo vazio localizado", txt.includes("sem amostras neste eixo"));
ok("txt tem histograma", txt.includes("\u2588"));
ok("txt faixa do histograma com virgula", /\d+,\d\d\u2013\d+,\d\d/.test(txt), txt.match(/ +[\d,]+\u2013[\d,]+/)?.[0]);
ok("txt tem piores", /Piores \d+ amostras/.test(txt), txt.match(/Piores.*/)?.[0]);
ok("txt sem nan/undefined", !/NaN|undefined|\[object/.test(txt));

ok("csv BOM (EF BB BF)", bytes[0] === 0xef && bytes[1] === 0xbb && bytes[2] === 0xbf,
   [bytes[0], bytes[1], bytes[2]]);
ok("csv cabecalho de dados em ingles fixo", enc.includes('"sample","seq","latency_us"'),
   enc.match(/".*","seq".*/g));
ok("csv metadados localizados", /# eixo: CLIQUE \(T1_CLICK\)/.test(enc), enc.match(/# eixo.*/)?.[0]);
ok("csv rotulo de resumo", /# Resumo: 50 amostras/.test(enc), enc.match(/# :.*/)?.[0]);
ok("csv bloco do eixo vazio sem header",
   (enc.match(/"sample","seq"/g) || []).length === 1 && enc.includes("# eixo: MOVIMENTO"));
/* A vírgula decimal legitimately aparece nas linhas # em pt/de/es/fr/it/ru.
   O que não pode sobrar é a VÍRGULA COMO SEPARADOR de item: o " · " mantem a
   linha em uma coluna só, para o Excel não deslocar o cabecalho de 3 colunas. */
ok("csv sem virgula como separador", !/^# .*, /m.test(enc), enc.match(/^# .*, /m)?.[0]);
ok("csv dado cru com ponto", /^1,800\.00$/m.test(enc), enc.match(/^\d+,\d+\.\d\d$/m)?.[0]);
ok("csv sem t0/t1/ns", !csv.includes("t0_ts") && !csv.includes("lat_ns"));
ok("csv sem aspas em comentario", !enc.split("\n").filter((l) => l.startsWith("#")).some((l) => l.includes('"')));

ok("md tem tabelas", md.includes("| Resumo | lat\u00eancia |") && md.includes("### Distribui\u00e7\u00e3o"),
   md.match(/\| .* \| .* \|/)?.[0]);
ok("md 2 eixos", md.includes("## CLIQUE (T1_CLICK)") && md.includes("## MOVIMENTO (T1_MOTION)"));
ok("md eixo vazio", md.includes("sem amostras neste eixo"));

const j = JSON.parse(json);
ok("json schema", j.schema === "gamer-latency-meter/report@1", j.schema);
ok("json language", j.language === "pt", j.language);
ok("json firmware real", j.firmware === "medidor-rp2350 v1.0.0");
ok("json click n/p50", j.axes.click.samples === 50 && j.axes.click.stats.p50 === 824000, j.axes.click.stats);
ok("json motion vazio", j.axes.motion.samples === 0 && j.axes.motion.stats === null);
ok("json readings sem t0/t1", !("t0" in j.axes.click.readings[0]));
ok("json display formatado (locale)", j.axes.click.display.p50 === "824,0 \u00b5s", j.axes.click.display.p50);
ok("json numero cru com ponto", j.axes.click.stats.p50 === 824000 && j.axes.click.stats.min === 800000,
   j.axes.click.stats);

/* ---------- guard: index.html sem os botoes txt/md nao quebra ---------- */
nullIds.add("btn-export-txt"); nullIds.add("btn-export-md");
throws("bindUI sem txt/md", () => get("bindUI")());
throws("setUi sem txt/md", () => get("setUi")(true));
ok("btn csv continua ligado", typeof cache.get("btn-export-csv").onclick === "function");
nullIds.delete("btn-export-txt"); nullIds.delete("btn-export-md");

/* ---------- limpar dados zera tudo, preserva a identidade do firmware ---------- */
get("DATA.click").push({ seq: 1, lat: 1 });
cache.get("btn-clear").onclick();
ok("clear zera click", get("DATA.click.length") === 0);
ok("clear zera contadores", get("stCounters").total === 0 && get("stCounters").timeouts === 0);
ok("clear preserva firmware", get("devInfo.ver") === "medidor-rp2350 v1.0.0");
ok("tiles vazios viram em dash",
   [get("refreshStats")(), ...["st-p50", "st-p95", "st-p99", "st-avg", "st-std"].map(tile)]
     .slice(1).every((v) => v === "\u2014"));

/* ---------- contexto de export sem dados nao quebra ---------- */
throws("Report.build sem dados", () => get("Report.build")(get("reportContext")()));

console.log((fail ? "FALHOU " : "ok ") + pass + " passed, " + fail + " failed");
process.exit(fail ? 1 : 0);
