/* Medidor de Latência — chaves de tradução (node tests/i18n.test.mjs). */

import vm from "node:vm";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";

const root = join(dirname(fileURLToPath(import.meta.url)), "..");
const src = (f) => readFileSync(join(root, f), "utf8");

const sandbox = {
  console, setTimeout,
  localStorage: { getItem: () => "en", setItem: () => {} },
  navigator: { language: "en" },
  document: {
    readyState: "complete", documentElement: { dataset: {} },
    getElementById: () => null, querySelectorAll: () => [], addEventListener() {}
  },
  CustomEvent: class { constructor(t) { this.type = t; } }
};
sandbox.window = { addEventListener() {}, dispatchEvent() {} };
sandbox.globalThis = sandbox;
vm.createContext(sandbox);
vm.runInContext(src("i18n.js"), sandbox, { filename: "i18n.js" });

const get = (e) => vm.runInContext(e, sandbox);
const T = get("I18N_STRINGS");
const I18N = get("I18N");
const LANGS = get("LANGS").map((l) => l.code);

let pass = 0, fail = 0;
const ok = (name, cond, got) => {
  if (cond) { pass++; return; }
  fail++;
  console.error("FAIL " + name + (got === undefined ? "" : "\n      " + got));
};

/* ---------- todas as linguas tem as mesmas chaves ---------- */
const base = Object.keys(T.en).sort();
ok("10 linguas", LANGS.length === 10, String(LANGS.length));
for (const lang of LANGS) {
  const keys = Object.keys(T[lang]).sort();
  const missing = base.filter((k) => !keys.includes(k));
  const extra = keys.filter((k) => !base.includes(k));
  ok("paridade " + lang, missing.length === 0 && extra.length === 0,
     "faltando: " + missing.join(", ") + (extra.length ? " | sobrando: " + extra.join(", ") : ""));
}

/* ---------- toda label() usada em report.js existe em todas as linguas ---------- */
const used = new Set();
for (const m of src("report.js").matchAll(/label\("([^"]+)"/g)) used.add(m[1]);
ok("report.js usa >= 30 chaves", used.size >= 30, String(used.size));
for (const key of [...used].sort()) {
  const bad = LANGS.filter((l) => T[l][key] === undefined);
  ok("report.js " + key, bad.length === 0, "faltando em " + bad.join(", "));
}

/* ---------- todo data-i18n do index.html existe ---------- */
const dom = new Set();
for (const m of src("index.html").matchAll(/data-i18n="([^"]+)"/g)) dom.add(m[1]);
for (const m of src("index.html").matchAll(/data-i18n-html="([^"]+)"/g)) dom.add(m[1]);
for (const m of src("index.html").matchAll(/data-i18n-attr="([^"]+)"/g)) {
  for (const pair of m[1].split(";")) {
    const i = pair.indexOf(":");
    if (i > 0) dom.add(pair.slice(i + 1).trim());
  }
}
ok("index.html usa >= 40 chaves", dom.size >= 40, String(dom.size));
for (const key of [...dom].sort()) {
  const bad = LANGS.filter((l) => T[l][key] === undefined);
  ok("index.html " + key, bad.length === 0, "faltando em " + bad.join(", "));
}

/* ---------- placeholders {n} preservados em todas as linguas ---------- */
for (const key of ["rep.worst", "rep.amp", "log.timeout", "log.dropped"]) {
  const want = (T.en[key].match(/\{[a-z0-9]+\}/gi) || []).sort().join(",");
  for (const lang of LANGS) {
    const got = (T[lang][key].match(/\{[a-z0-9]+\}/gi) || []).sort().join(",");
    ok("placeholders " + key + "/" + lang, got === want, got + " != " + want);
  }
}

/* ---------- report.js nao deve precisar mais de REP_DEFAULTS ---------- */
const defaults = [...src("report.js").matchAll(/^\s*"(rep\.[^"]+)":/gm)].map((m) => m[1]);
const unusedFallback = defaults.filter((k) => !T.en[k]);
ok("sem fallback morto em REP_DEFAULTS", unusedFallback.length === 0, unusedFallback.join(", "));
const missingInEn = [...used].filter((k) => !T.en[k]);
ok("nenhuma chave rep.* sem en", missingInEn.length === 0, missingInEn.join(", "));

/* ---------- nenhum rep.* orfao no i18n ---------- */
/* Chave que saiu do report.js e ficou esquecida na tabela não é visível na
   paridade (10 idiomas com a mesma sobra), só comparando com o uso real. */
const orphans = base.filter((k) => k.startsWith("rep.") && !used.has(k));
ok("nenhum rep.* orfao", orphans.length === 0, orphans.join(", "));

/* ---------- nomes de arquivo nao podem ter separadores de caminho ---------- */
for (const lang of LANGS) {
  ok("csvName seguro " + lang, /^[A-Za-z0-9._-]+$/.test(T[lang]["exp.csvName"]), T[lang]["exp.csvName"]);
}

/* ---------- fallback: chave faltando cai no ingles, nunca na chave crua ---------- */
const probe = "rep.amp";
ok("probe existe", T.en[probe] !== undefined && T.ko[probe] !== undefined);
get(`delete I18N_STRINGS.ko[${JSON.stringify(probe)}]`);
I18N.set("ko");
ok("t() usa ingles quando falta no ko", I18N.t(probe) === T.en[probe], I18N.t(probe));
/* Último recurso: chave ausente em todo idioma volta crua. É o único caminho em
   que uma chave aparece na tela, e as verificações de paridade acima garantem
   que nenhuma chave realmente usada chegue nesse caminho. */
ok("t() so devolve a chave crua no ultimo recurso",
   I18N.t("rep.inexistente") === "rep.inexistente", I18N.t("rep.inexistente"));
get(`I18N_STRINGS.ko[${JSON.stringify(probe)}] = ${JSON.stringify("ko-temp")}`);
ok("t() volta a usar ko depois de restaurar", I18N.t(probe) === "ko-temp", I18N.t(probe));

/* ---------- o texto estatico do index.html bate com a tabela en ---------- */
/* O apply() sobrescreve tudo no load, então essa divergência não aparece na
   tela — mas fica no HTML cru, no view-source e no crawler. A paridade acima
   não pega isso, porque compara chaves, não valores. */
const norm = (s) => s.replace(/\s+/g, " ").trim();
const stale = [];
for (const m of src("index.html").matchAll(/data-i18n="([A-Za-z0-9_.]+)"[^>]*>([\s\S]*?)<\//g)) {
  const key = m[1];
  const text = norm(m[2].replace(/<[^>]*>/g, ""));
  if (!text || T.en[key] === undefined) continue;
  if (text !== norm(T.en[key])) stale.push(key + ": " + text.slice(0, 60) + " != " + norm(T.en[key]).slice(0, 60));
}
ok("nenhum fallback estatico divergente no index.html", stale.length === 0, stale.join("\n      "));

console.log((fail ? "FALHOU " : "ok ") + pass + " passed, " + fail + " failed");
process.exit(fail ? 1 : 0);
