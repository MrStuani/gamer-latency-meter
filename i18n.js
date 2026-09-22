/* i18n.js — camada de tradução leve (sem dependências). */

"use strict";

const LANGS = [
  { code: "en", flag: "\u{1F1FA}\u{1F1F8}", name: "English" },
  { code: "pt", flag: "\u{1F1E7}\u{1F1F7}", name: "Português (BR)" },
  { code: "es", flag: "\u{1F1EA}\u{1F1F8}", name: "Español" },
  { code: "fr", flag: "\u{1F1EB}\u{1F1F7}", name: "Français" },
  { code: "de", flag: "\u{1F1E9}\u{1F1EA}", name: "Deutsch" },
  { code: "it", flag: "\u{1F1EE}\u{1F1F9}", name: "Italiano" },
  { code: "zh", flag: "\u{1F1E8}\u{1F1F3}", name: "中文 (简体)" },
  { code: "ja", flag: "\u{1F1EF}\u{1F1F5}", name: "日本語" },
  { code: "ko", flag: "\u{1F1F0}\u{1F1F7}", name: "한국어" },
  { code: "ru", flag: "\u{1F1F7}\u{1F1FA}", name: "Русский" }
];

const I18N_STRINGS = {};

I18N_STRINGS.en = {
  "meta.title": "Gamer Peripheral Latency Meter",
  "header.title": "Gamer Peripheral Latency Meter",
  "header.sub": "RP2350 · PIO timestamp · WebSerial · ",
  "header.hint": "brute-offline: open via file:// or GitHub Pages",
  "lang.label": "Language",

  "conn.title": "Connection",
  "conn.connectBtn": "Connect to meter",
  "conn.disconnectBtn": "Disconnect",
  "conn.stateIdle": "disconnected",
  "conn.stateOk": "connected",
  "ws.warn": "WebSerial is not available in this browser. Use Chrome/Edge (desktop) with HTTPS, localhost or GitHub Pages.",
  "ws.notAvail": "WebSerial not available in this browser",

  "wiring.title": "Wiring (Pico 2 pinout)",
  "wiring.svgAria": "Raspberry Pi Pico 2 pinout diagram",
  "wiring.svgTitle": "Pico 2 pinout — T0, T1_CLICK, T1_MOTION and GND",
  "wiring.legend": "T0 = physical action · T1_CLICK = click response · T1_MOTION = motion response · GND = common ground",

  "pin.step1": "<b>T0</b> (physical action: switch / photodiode+comparator) &rarr; <code>GP2</code> = pin <b>4</b>&nbsp;<span class=\"sw t0\">■</span>",
  "pin.step2": "<b>T1_CLICK</b> (CH32V307 click output) &rarr; <code>GP3</code> = pin <b>5</b>&nbsp;<span class=\"sw clk\">■</span>",
  "pin.step3": "<b>T1_MOTION</b> (CH32V307 motion output) &rarr; <code>GP4</code> = pin <b>6</b>&nbsp;<span class=\"sw mot\">■</span>",
  "pin.step4": "<b>GND</b> common between the boards &rarr; pin <b>3</b> (near GP2) or <b>8</b> (near GP3/GP4).",
  "pin.step5": "<b>USB</b> &rarr; the Pico 2&rsquo;s own USB port (WebSerial/CDC).",
  "pin.step6": "<b>LED GP25</b> (on-board): blinks every 500&nbsp;ms while the firmware is alive.",
  "pin.step7": "Signals are 3.3&nbsp;V &mdash; <b>do NOT apply 5&nbsp;V</b> to the pins.",

  "cfg.title": "Measurement configuration",
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
  "cfg.holdoff.label": "Holdoff (µs)",
  "cfg.timeout.label": "Timeout / watchdog (µs)",
  "cfg.apply": "Apply config",
  "cfg.noteDefault": "Free combinations: any T0 pull × edge × axis. A warning will appear here when the combination is unusual.",

  "stats.title": "Statistics",
  "stat.last": "last latency",
  "stat.avg": "average (active)",
  "stat.minmax": "min / max",
  "stat.std": "std deviation",
  "stat.count": "samples (active)",
  "stat.elapsed": "elapsed time",
  "stat.seq": "total sequence",
  "stat.timouts": "timeouts / drops",

  "chart.title": "Latency profile (active mode)",
  "chart.help": "scatter: sequence × latencies · line = moving average (64)",
  "hist.title": "Histogram (last 256)",

  "exp.title": "Events & export",
  "exp.csv": "Export CSV (active)",
  "exp.json": "Export JSON (all)",
  "exp.clear": "Clear data",
  "exp.csvName": "latency",

  "log.connected": "connected",
  "log.disconnected": "disconnected",
  "log.connectError": "connection error: ",
  "log.readFailed": "read failed: ",
  "log.bufOverflow": "serial buffer overflowed; zeroing",
  "log.timeout": "#{seq} TIMEOUT (T1 did not arrive in {us} µs)",
  "log.order": "#{seq} ORDER (t1 before t0)",
  "log.dropped": "#{seq} DROPPED (T0 overwritten)",

  "note.upFall": "T0 with pull-up and edge = falling: the physical action switches off — classic contact-switch combination.",
  "note.fRise": "T0 floating, rising edge: simple convention (motion pulse).",
  "note.fFloat": "Attention: T0 in float mode — noise risk; validate the polarity on an oscilloscope.",
  "note.ok": "Free combination ok.",

  "unit.s": "s",
  "unit.min": "min",
  footer: "Side project: medidor-rp2350 — firmware + static page."
};

I18N_STRINGS.pt = {
  "meta.title": "Medidor de Latência de Periférico Gamer",
  "header.title": "Medidor de Latência de Periférico Gamer",
  "header.sub": "RP2350 · PIO timestamp · WebSerial · ",
  "header.hint": "brute-offline: abra por file:// ou GitHub Pages",
  "lang.label": "Idioma",

  "conn.title": "Conexão",
  "conn.connectBtn": "Conectar ao medidor",
  "conn.disconnectBtn": "Desconectar",
  "conn.stateIdle": "desconectado",
  "conn.stateOk": "conectado",
  "ws.warn": "WebSerial não disponível neste navegador. Use Chrome/Edge (desktop) com HTTPS, localhost ou GitHub Pages.",
  "ws.notAvail": "WebSerial não disponível neste navegador",

  "wiring.title": "Como ligar (pinagem do Pico 2)",
  "wiring.svgAria": "Diagrama dos pinos do Raspberry Pi Pico 2",
  "wiring.svgTitle": "Pinagem do Pico 2 — T0, T1_CLICK, T1_MOTION e GND",
  "wiring.legend": "T0 = ação física · T1_CLICK = resposta de clique · T1_MOTION = resposta de movimento · GND = terra comum",

  "pin.step1": "<b>T0</b> (ação física: chave / fotodiodo+comparador) &rarr; <code>GP2</code> = pino <b>4</b>&nbsp;<span class=\"sw t0\">■</span>",
  "pin.step2": "<b>T1_CLICK</b> (saída de clique do CH32V307) &rarr; <code>GP3</code> = pino <b>5</b>&nbsp;<span class=\"sw clk\">■</span>",
  "pin.step3": "<b>T1_MOTION</b> (saída de movimento do CH32V307) &rarr; <code>GP4</code> = pino <b>6</b>&nbsp;<span class=\"sw mot\">■</span>",
  "pin.step4": "<b>GND</b> comum entre as placas &rarr; pino <b>3</b> (perto do GP2) ou <b>8</b> (perto do GP3/GP4).",
  "pin.step5": "<b>USB</b> &rarr; porta USB do próprio Pico 2 (WebSerial/CDC).",
  "pin.step6": "<b>LED GP25</b> (on-board): pisca a cada 500&nbsp;ms quando o firmware está vivo.",
  "pin.step7": "Os sinais são em 3,3&nbsp;V &mdash; <b>não aplique 5&nbsp;V</b> nos pinos.",

  "cfg.title": "Configuração da medição",
  "cfg.t0pull.label": "T0 pull / física",
  "cfg.t0pull.f": "float (sem pull)",
  "cfg.t0pull.up": "pull-up (ação desce T0)",
  "cfg.t0pull.down": "pull-down (ação sobe T0)",
  "cfg.edge.label": "Borda do T0 (\"ação\")",
  "cfg.edge.r": "subida (rising)",
  "cfg.edge.f": "descida (falling)",
  "cfg.mode.label": "Eixo em teste",
  "cfg.mode.c": "Clique (T1_CLICK)",
  "cfg.mode.m": "Movimento (T1_MOTION)",
  "cfg.holdoff.label": "Holdoff (µs)",
  "cfg.timeout.label": "Timeout / watchdog (µs)",
  "cfg.apply": "Aplicar config",
  "cfg.noteDefault": "Combinações livres: qualquer T0 pull × borda × eixo. Aviso quando a combinação for incomum será mostrado aqui.",

  "stats.title": "Estatísticas",
  "stat.last": "última latência",
  "stat.avg": "média (ativo)",
  "stat.minmax": "mín / máx",
  "stat.std": "desvio padrão",
  "stat.count": "amostras (ativo)",
  "stat.elapsed": "tempo decorrido",
  "stat.seq": "sequência total",
  "stat.timouts": "timeouts / drops",

  "chart.title": "Perfil da latência (modo ativo)",
  "chart.help": "scatter: sequência × latências · linha = média móvel (64)",
  "hist.title": "Histograma (últimas 256)",

  "exp.title": "Eventos & exportação",
  "exp.csv": "Exportar CSV (ativo)",
  "exp.json": "Exportar JSON (todos)",
  "exp.clear": "Limpar dados",
  "exp.csvName": "latencia",

  "log.connected": "conectado",
  "log.disconnected": "desconectado",
  "log.connectError": "erro de conexão: ",
  "log.readFailed": "leitura falhou: ",
  "log.bufOverflow": "buffer serial estourou; zerando",
  "log.timeout": "#{seq} TIMEOUT (T1 não veio em {us} µs)",
  "log.order": "#{seq} ORDER (t1 antes de t0)",
  "log.dropped": "#{seq} DROPPED (T0 sobrescrito)",

  "note.upFall": "T0 com pull-up e borda = descida: ação física desliga — combinação clássica de chave de contato.",
  "note.fRise": "T0 flutuante, borda de subida: convenção simples (pulso de motion).",
  "note.fFloat": "Atenção: T0 em modo float — risco de ruído; valide a polaridade no osciloscópio.",
  "note.ok": "Combinação livre ok.",

  "unit.s": "s",
  "unit.min": "min",
  footer: "Projeto side: medidor-rp2350 — firmware + página estática."
};

I18N_STRINGS.es = {
  "meta.title": "Medidor de Latencia de Periférico Gamer",
  "header.title": "Medidor de Latencia de Periférico Gamer",
  "header.sub": "RP2350 · PIO timestamp · WebSerial · ",
  "header.hint": "brute-offline: abre mediante file:// o GitHub Pages",
  "lang.label": "Idioma",

  "conn.title": "Conexión",
  "conn.connectBtn": "Conectar al medidor",
  "conn.disconnectBtn": "Desconectar",
  "conn.stateIdle": "desconectado",
  "conn.stateOk": "conectado",
  "ws.warn": "WebSerial no está disponible en este navegador. Usa Chrome/Edge (escritorio) con HTTPS, localhost o GitHub Pages.",
  "ws.notAvail": "WebSerial no disponible en este navegador",

  "wiring.title": "Cómo conectar (pinout del Pico 2)",
  "wiring.svgAria": "Diagrama del pinout del Raspberry Pi Pico 2",
  "wiring.svgTitle": "Pinout del Pico 2 — T0, T1_CLICK, T1_MOTION y GND",
  "wiring.legend": "T0 = acción física · T1_CLICK = respuesta de clic · T1_MOTION = respuesta de movimiento · GND = tierra común",

  "pin.step1": "<b>T0</b> (acción física: interruptor / fotodiodo+comparador) &rarr; <code>GP2</code> = pin <b>4</b>&nbsp;<span class=\"sw t0\">■</span>",
  "pin.step2": "<b>T1_CLICK</b> (salida de clic del CH32V307) &rarr; <code>GP3</code> = pin <b>5</b>&nbsp;<span class=\"sw clk\">■</span>",
  "pin.step3": "<b>T1_MOTION</b> (salida de movimiento del CH32V307) &rarr; <code>GP4</code> = pin <b>6</b>&nbsp;<span class=\"sw mot\">■</span>",
  "pin.step4": "<b>GND</b> común entre las placas &rarr; pin <b>3</b> (junto a GP2) o <b>8</b> (junto a GP3/GP4).",
  "pin.step5": "<b>USB</b> &rarr; puerto USB del propio Pico 2 (WebSerial/CDC).",
  "pin.step6": "<b>LED GP25</b> (on-board): parpadea cada 500&nbsp;ms cuando el firmware está vivo.",
  "pin.step7": "Las señales son de 3,3&nbsp;V &mdash; <b>no apliques 5&nbsp;V</b> a los pines.",

  "cfg.title": "Configuración de la medición",
  "cfg.t0pull.label": "T0 pull / física",
  "cfg.t0pull.f": "float (sin pull)",
  "cfg.t0pull.up": "pull-up (la acción baja T0)",
  "cfg.t0pull.down": "pull-down (la acción sube T0)",
  "cfg.edge.label": "Borde del T0 (\"acción\")",
  "cfg.edge.r": "subida (rising)",
  "cfg.edge.f": "bajada (falling)",
  "cfg.mode.label": "Eje en prueba",
  "cfg.mode.c": "Clic (T1_CLICK)",
  "cfg.mode.m": "Movimiento (T1_MOTION)",
  "cfg.holdoff.label": "Holdoff (µs)",
  "cfg.timeout.label": "Timeout / watchdog (µs)",
  "cfg.apply": "Aplicar config",
  "cfg.noteDefault": "Combinaciones libres: cualquier pull de T0 × borde × eje. Aquí se avisará si la combinación es inusual.",

  "stats.title": "Estadísticas",
  "stat.last": "última latencia",
  "stat.avg": "media (activo)",
  "stat.minmax": "mín / máx",
  "stat.std": "desviación estándar",
  "stat.count": "muestras (activo)",
  "stat.elapsed": "tiempo transcurrido",
  "stat.seq": "secuencia total",
  "stat.timouts": "timeouts / drops",

  "chart.title": "Perfil de latencia (modo activo)",
  "chart.help": "scatter: secuencia × latencias · línea = media móvil (64)",
  "hist.title": "Histograma (últimas 256)",

  "exp.title": "Eventos y exportación",
  "exp.csv": "Exportar CSV (activo)",
  "exp.json": "Exportar JSON (todos)",
  "exp.clear": "Limpiar datos",
  "exp.csvName": "latencia",

  "log.connected": "conectado",
  "log.disconnected": "desconectado",
  "log.connectError": "error de conexión: ",
  "log.readFailed": "fallo de lectura: ",
  "log.bufOverflow": "buffer serial desbordado; reiniciando",
  "log.timeout": "#{seq} TIMEOUT (T1 no llegó en {us} µs)",
  "log.order": "#{seq} ORDER (t1 antes de t0)",
  "log.dropped": "#{seq} DROPPED (T0 sobrescrito)",

  "note.upFall": "T0 con pull-up y borde = bajada: la acción física se apaga — combinación clásica de interruptor de contacto.",
  "note.fRise": "T0 flotante, borde de subida: convención simple (pulso de motion).",
  "note.fFloat": "Atención: T0 en modo float — riesgo de ruido; valida la polaridad con un osciloscopio.",
  "note.ok": "Combinación libre correcta.",

  "unit.s": "s",
  "unit.min": "min",
  footer: "Proyecto paralelo: medidor-rp2350 — firmware + página estática."
};

I18N_STRINGS.fr = {
  "meta.title": "Mesureur de latence de périphérique gamer",
  "header.title": "Mesureur de latence de périphérique gamer",
  "header.sub": "RP2350 · PIO timestamp · WebSerial · ",
  "header.hint": "brute-offline : ouvrez via file:// ou GitHub Pages",
  "lang.label": "Langue",

  "conn.title": "Connexion",
  "conn.connectBtn": "Connecter au mesureur",
  "conn.disconnectBtn": "Déconnecter",
  "conn.stateIdle": "déconnecté",
  "conn.stateOk": "connecté",
  "ws.warn": "WebSerial n'est pas disponible dans ce navigateur. Utilisez Chrome/Edge (desktop) avec HTTPS, localhost ou GitHub Pages.",
  "ws.notAvail": "WebSerial indisponible dans ce navigateur",

  "wiring.title": "Câblage (pinout du Pico 2)",
  "wiring.svgAria": "Diagramme du pinout du Raspberry Pi Pico 2",
  "wiring.svgTitle": "Pinout du Pico 2 — T0, T1_CLICK, T1_MOTION et GND",
  "wiring.legend": "T0 = action physique · T1_CLICK = réponse au clic · T1_MOTION = réponse au mouvement · GND = masse commune",

  "pin.step1": "<b>T0</b> (action physique : interrupteur / photodiode+comparateur) &rarr; <code>GP2</code> = broche <b>4</b>&nbsp;<span class=\"sw t0\">■</span>",
  "pin.step2": "<b>T1_CLICK</b> (sortie de clic du CH32V307) &rarr; <code>GP3</code> = broche <b>5</b>&nbsp;<span class=\"sw clk\">■</span>",
  "pin.step3": "<b>T1_MOTION</b> (sortie de mouvement du CH32V307) &rarr; <code>GP4</code> = broche <b>6</b>&nbsp;<span class=\"sw mot\">■</span>",
  "pin.step4": "<b>GND</b> commun entre les cartes &rarr; broche <b>3</b> (près de GP2) ou <b>8</b> (près de GP3/GP4).",
  "pin.step5": "<b>USB</b> &rarr; port USB du Pico 2 lui-même (WebSerial/CDC).",
  "pin.step6": "<b>LED GP25</b> (on-board) : clignote toutes les 500&nbsp;ms quand le firmware est actif.",
  "pin.step7": "Les signaux sont en 3,3&nbsp;V &mdash; <b>n'appliquez pas 5&nbsp;V</b> sur les broches.",

  "cfg.title": "Configuration de la mesure",
  "cfg.t0pull.label": "T0 pull / physique",
  "cfg.t0pull.f": "float (sans pull)",
  "cfg.t0pull.up": "pull-up (l'action fait descendre T0)",
  "cfg.t0pull.down": "pull-down (l'action fait monter T0)",
  "cfg.edge.label": "Front du T0 (\"action\")",
  "cfg.edge.r": "montant (rising)",
  "cfg.edge.f": "descendant (falling)",
  "cfg.mode.label": "Axe testé",
  "cfg.mode.c": "Clic (T1_CLICK)",
  "cfg.mode.m": "Mouvement (T1_MOTION)",
  "cfg.holdoff.label": "Holdoff (µs)",
  "cfg.timeout.label": "Timeout / watchdog (µs)",
  "cfg.apply": "Appliquer la config",
  "cfg.noteDefault": "Combinaisons libres : tout pull de T0 × front × axe. Un avertissement s'affichera ici si la combinaison est inhabituelle.",

  "stats.title": "Statistiques",
  "stat.last": "dernière latence",
  "stat.avg": "moyenne (actif)",
  "stat.minmax": "min / max",
  "stat.std": "écart type",
  "stat.count": "échantillons (actif)",
  "stat.elapsed": "temps écoulé",
  "stat.seq": "séquence totale",
  "stat.timouts": "timeouts / drops",

  "chart.title": "Profil de latence (mode actif)",
  "chart.help": "scatter : séquence × latences · ligne = moyenne mobile (64)",
  "hist.title": "Histogramme (256 dernières)",

  "exp.title": "Événements & export",
  "exp.csv": "Exporter CSV (actif)",
  "exp.json": "Exporter JSON (tout)",
  "exp.clear": "Effacer les données",
  "exp.csvName": "latence",

  "log.connected": "connecté",
  "log.disconnected": "déconnecté",
  "log.connectError": "erreur de connexion : ",
  "log.readFailed": "échec de lecture : ",
  "log.bufOverflow": "buffer série saturé ; remise à zéro",
  "log.timeout": "#{seq} TIMEOUT (T1 pas arrivé en {us} µs)",
  "log.order": "#{seq} ORDER (t1 avant t0)",
  "log.dropped": "#{seq} DROPPED (T0 écrasé)",

  "note.upFall": "T0 en pull-up et front = descente : l'action physique coupe — combinaison classique d'interrupteur de contact.",
  "note.fRise": "T0 flottant, front montant : convention simple (pulse de motion).",
  "note.fFloat": "Attention : T0 en mode float — risque de bruit ; validez la polarité à l'oscilloscope.",
  "note.ok": "Combinaison libre correcte.",

  "unit.s": "s",
  "unit.min": "min",
  footer: "Projet parallèle : medidor-rp2350 — firmware + page statique."
};

I18N_STRINGS.de = {
  "meta.title": "Gaming-Peripherie-Latenz-Messgerät",
  "header.title": "Gaming-Peripherie-Latenz-Messgerät",
  "header.sub": "RP2350 · PIO timestamp · WebSerial · ",
  "header.hint": "brute-offline: über file:// oder GitHub Pages öffnen",
  "lang.label": "Sprache",

  "conn.title": "Verbindung",
  "conn.connectBtn": "Mit Messgerät verbinden",
  "conn.disconnectBtn": "Trennen",
  "conn.stateIdle": "getrennt",
  "conn.stateOk": "verbunden",
  "ws.warn": "WebSerial ist in diesem Browser nicht verfügbar. Verwende Chrome/Edge (Desktop) mit HTTPS, localhost oder GitHub Pages.",
  "ws.notAvail": "WebSerial in diesem Browser nicht verfügbar",

  "wiring.title": "Verdrahtung (Pico 2 Pinout)",
  "wiring.svgAria": "Raspberry Pi Pico 2 Pinout-Diagramm",
  "wiring.svgTitle": "Pico 2 Pinout — T0, T1_CLICK, T1_MOTION und GND",
  "wiring.legend": "T0 = physikalische Aktion · T1_CLICK = Klick-Antwort · T1_MOTION = Bewegungs-Antwort · GND = gemeinsame Masse",

  "pin.step1": "<b>T0</b> (physikalische Aktion: Schalter / Fotodiode+Komparator) &rarr; <code>GP2</code> = Pin <b>4</b>&nbsp;<span class=\"sw t0\">■</span>",
  "pin.step2": "<b>T1_CLICK</b> (Klick-Ausgang des CH32V307) &rarr; <code>GP3</code> = Pin <b>5</b>&nbsp;<span class=\"sw clk\">■</span>",
  "pin.step3": "<b>T1_MOTION</b> (Bewegungs-Ausgang des CH32V307) &rarr; <code>GP4</code> = Pin <b>6</b>&nbsp;<span class=\"sw mot\">■</span>",
  "pin.step4": "<b>GND</b> gemeinsam zwischen den Platinen &rarr; Pin <b>3</b> (bei GP2) oder <b>8</b> (bei GP3/GP4).",
  "pin.step5": "<b>USB</b> &rarr; den eigenen USB-Anschluss des Pico 2 nutzen (WebSerial/CDC).",
  "pin.step6": "<b>LED GP25</b> (on-board): blinkt alle 500&nbsp;ms, solange die Firmware läuft.",
  "pin.step7": "Die Signale sind 3,3&nbsp;V &mdash; <b>kein 5&nbsp;V auf die Pins</b> anlegen.",

  "cfg.title": "Messungskonfiguration",
  "cfg.t0pull.label": "T0-Pull / physikalisch",
  "cfg.t0pull.f": "float (ohne Pull)",
  "cfg.t0pull.up": "pull-up (Aktion zieht T0 niedrig)",
  "cfg.t0pull.down": "pull-down (Aktion zieht T0 hoch)",
  "cfg.edge.label": "T0-Flanke (\"Aktion\")",
  "cfg.edge.r": "steigend (rising)",
  "cfg.edge.f": "fallend (falling)",
  "cfg.mode.label": "Zu testende Achse",
  "cfg.mode.c": "Klick (T1_CLICK)",
  "cfg.mode.m": "Bewegung (T1_MOTION)",
  "cfg.holdoff.label": "Holdoff (µs)",
  "cfg.timeout.label": "Timeout / Watchdog (µs)",
  "cfg.apply": "Konfig anwenden",
  "cfg.noteDefault": "Freie Kombinationen: beliebiger T0-Pull × Flanke × Achse. Eine Warnung erscheint hier bei ungewöhnlichen Kombinationen.",

  "stats.title": "Statistiken",
  "stat.last": "letzte Latenz",
  "stat.avg": "Mittelwert (aktiv)",
  "stat.minmax": "min / max",
  "stat.std": "Standardabweichung",
  "stat.count": "Proben (aktiv)",
  "stat.elapsed": "verstrichene Zeit",
  "stat.seq": "Gesamtsequenz",
  "stat.timouts": "Timeouts / Drops",

  "chart.title": "Latenzprofil (aktiver Modus)",
  "chart.help": "scatter: Sequenz × Latenzen · Linie = gleitender Mittelwert (64)",
  "hist.title": "Histogramm (letzte 256)",

  "exp.title": "Ereignisse & Export",
  "exp.csv": "CSV exportieren (aktiv)",
  "exp.json": "JSON exportieren (alle)",
  "exp.clear": "Daten löschen",
  "exp.csvName": "latency",

  "log.connected": "verbunden",
  "log.disconnected": "getrennt",
  "log.connectError": "Verbindungsfehler: ",
  "log.readFailed": "Lesefehler: ",
  "log.bufOverflow": "Serieller Puffer übergelaufen; wird zurückgesetzt",
  "log.timeout": "#{seq} TIMEOUT (T1 kam nicht in {us} µs)",
  "log.order": "#{seq} ORDER (t1 vor t0)",
  "log.dropped": "#{seq} DROPPED (T0 überschrieben)",

  "note.upFall": "T0 mit Pull-up und fallender Flanke: die physische Aktion schaltet ab — klassische Taster-Kombination.",
  "note.fRise": "T0 float, steigende Flanke: einfache Konvention (Motion-Puls).",
  "note.fFloat": "Achtung: T0 im Float-Modus — Rauschrisiko; Polarität am Oszilloskop prüfen.",
  "note.ok": "Freie Kombination ok.",

  "unit.s": "s",
  "unit.min": "min",
  footer: "Nebenprojekt: medidor-rp2350 — Firmware + statische Seite."
};

I18N_STRINGS.it = {
  "meta.title": "Misuratore di latenza per periferiche gamer",
  "header.title": "Misuratore di latenza per periferiche gamer",
  "header.sub": "RP2350 · PIO timestamp · WebSerial · ",
  "header.hint": "brute-offline: apri con file:// o GitHub Pages",
  "lang.label": "Lingua",

  "conn.title": "Connessione",
  "conn.connectBtn": "Connetti al misuratore",
  "conn.disconnectBtn": "Disconnetti",
  "conn.stateIdle": "disconnesso",
  "conn.stateOk": "connesso",
  "ws.warn": "WebSerial non è disponibile in questo browser. Usa Chrome/Edge (desktop) con HTTPS, localhost o GitHub Pages.",
  "ws.notAvail": "WebSerial non disponibile in questo browser",

  "wiring.title": "Come collegare (pinout del Pico 2)",
  "wiring.svgAria": "Diagramma del pinout del Raspberry Pi Pico 2",
  "wiring.svgTitle": "Pinout del Pico 2 — T0, T1_CLICK, T1_MOTION e GND",
  "wiring.legend": "T0 = azione fisica · T1_CLICK = risposta al click · T1_MOTION = risposta al movimento · GND = terra comune",

  "pin.step1": "<b>T0</b> (azione fisica: interruttore / fotodiodo+comparatore) &rarr; <code>GP2</code> = pin <b>4</b>&nbsp;<span class=\"sw t0\">■</span>",
  "pin.step2": "<b>T1_CLICK</b> (uscita di click del CH32V307) &rarr; <code>GP3</code> = pin <b>5</b>&nbsp;<span class=\"sw clk\">■</span>",
  "pin.step3": "<b>T1_MOTION</b> (uscita di movimento del CH32V307) &rarr; <code>GP4</code> = pin <b>6</b>&nbsp;<span class=\"sw mot\">■</span>",
  "pin.step4": "<b>GND</b> comune tra le schede &rarr; pin <b>3</b> (vicino a GP2) o <b>8</b> (vicino a GP3/GP4).",
  "pin.step5": "<b>USB</b> &rarr; porta USB dello stesso Pico 2 (WebSerial/CDC).",
  "pin.step6": "<b>LED GP25</b> (on-board): lampeggia ogni 500&nbsp;ms quando il firmware è vivo.",
  "pin.step7": "I segnali sono a 3,3&nbsp;V &mdash; <b>non applicare 5&nbsp;V</b> ai pin.",

  "cfg.title": "Configurazione della misura",
  "cfg.t0pull.label": "T0 pull / fisico",
  "cfg.t0pull.f": "float (senza pull)",
  "cfg.t0pull.up": "pull-up (l'azione porta T0 basso)",
  "cfg.t0pull.down": "pull-down (l'azione porta T0 alto)",
  "cfg.edge.label": "Fronte del T0 (\"azione\")",
  "cfg.edge.r": "salita (rising)",
  "cfg.edge.f": "discesa (falling)",
  "cfg.mode.label": "Asse in prova",
  "cfg.mode.c": "Click (T1_CLICK)",
  "cfg.mode.m": "Movimento (T1_MOTION)",
  "cfg.holdoff.label": "Holdoff (µs)",
  "cfg.timeout.label": "Timeout / watchdog (µs)",
  "cfg.apply": "Applica config",
  "cfg.noteDefault": "Combinazioni libere: qualsiasi pull di T0 × fronte × asse. Un avviso apparirà qui se la combinazione è inusuale.",

  "stats.title": "Statistiche",
  "stat.last": "ultima latenza",
  "stat.avg": "media (attivo)",
  "stat.minmax": "min / max",
  "stat.std": "deviazione standard",
  "stat.count": "campioni (attivo)",
  "stat.elapsed": "tempo trascorso",
  "stat.seq": "sequenza totale",
  "stat.timouts": "timeout / drop",

  "chart.title": "Profilo di latenza (modalità attiva)",
  "chart.help": "scatter: sequenza × latenze · linea = media mobile (64)",
  "hist.title": "Istogramma (ultimi 256)",

  "exp.title": "Eventi ed esportazione",
  "exp.csv": "Esporta CSV (attivo)",
  "exp.json": "Esporta JSON (tutti)",
  "exp.clear": "Cancella dati",
  "exp.csvName": "latency",

  "log.connected": "connesso",
  "log.disconnected": "disconnesso",
  "log.connectError": "errore di connessione: ",
  "log.readFailed": "lettura fallita: ",
  "log.bufOverflow": "buffer seriale esaurito; azzeramento",
  "log.timeout": "#{seq} TIMEOUT (T1 non arrivato in {us} µs)",
  "log.order": "#{seq} ORDER (t1 prima di t0)",
  "log.dropped": "#{seq} DROPPED (T0 sovrascritto)",

  "note.upFall": "T0 con pull-up e fronte = discesa: l'azione fisica si spegne — combinazione classica da interruttore di contatto.",
  "note.fRise": "T0 flottante, fronte di salita: convenzione semplice (impulso di motion).",
  "note.fFloat": "Attenzione: T0 in modalità float — rischio di rumore; valida la polarità con un oscilloscopio.",
  "note.ok": "Combinazione libera ok.",

  "unit.s": "s",
  "unit.min": "min",
  footer: "Progetto collaterale: medidor-rp2350 — firmware + pagina statica."
};

I18N_STRINGS.zh = {
  "meta.title": "游戏外设延迟测量仪",
  "header.title": "游戏外设延迟测量仪",
  "header.sub": "RP2350 · PIO timestamp · WebSerial · ",
  "header.hint": "brute-offline：通过 file:// 或 GitHub Pages 打开",
  "lang.label": "语言",

  "conn.title": "连接",
  "conn.connectBtn": "连接到测量仪",
  "conn.disconnectBtn": "断开连接",
  "conn.stateIdle": "未连接",
  "conn.stateOk": "已连接",
  "ws.warn": "当前浏览器不支持 WebSerial。请使用 Chrome/Edge（桌面版），并通过 HTTPS、localhost 或 GitHub Pages 访问。",
  "ws.notAvail": "当前浏览器不支持 WebSerial",

  "wiring.title": "接线方式（Pico 2 引脚）",
  "wiring.svgAria": "Raspberry Pi Pico 2 引脚图",
  "wiring.svgTitle": "Pico 2 引脚 — T0、T1_CLICK、T1_MOTION 与 GND",
  "wiring.legend": "T0 = 物理动作 · T1_CLICK = 点击响应 · T1_MOTION = 移动响应 · GND = 公共接地",

  "pin.step1": "<b>T0</b>（物理动作：开关 / 光电二极管+比较器）&rarr; <code>GP2</code> = 引脚 <b>4</b>&nbsp;<span class=\"sw t0\">■</span>",
  "pin.step2": "<b>T1_CLICK</b>（CH32V307 的点击输出）&rarr; <code>GP3</code> = 引脚 <b>5</b>&nbsp;<span class=\"sw clk\">■</span>",
  "pin.step3": "<b>T1_MOTION</b>（CH32V307 的移动输出）&rarr; <code>GP4</code> = 引脚 <b>6</b>&nbsp;<span class=\"sw mot\">■</span>",
  "pin.step4": "<b>GND</b> 两块板共地 &rarr; 引脚 <b>3</b>（靠近 GP2）或 <b>8</b>（靠近 GP3/GP4）。",
  "pin.step5": "<b>USB</b> &rarr; Pico 2 自带的 USB 端口（WebSerial/CDC）。",
  "pin.step6": "<b>LED GP25</b>（板上）：固件运行时会每 500&nbsp;ms 闪烁一次。",
  "pin.step7": "信号为 3.3&nbsp;V &mdash; <b>切勿在引脚上施加 5&nbsp;V</b>。",

  "cfg.title": "测量配置",
  "cfg.t0pull.label": "T0 上/下拉 / 物理",
  "cfg.t0pull.f": "浮空（无上/下拉）",
  "cfg.t0pull.up": "上拉（动作拉低 T0）",
  "cfg.t0pull.down": "下拉（动作拉高 T0）",
  "cfg.edge.label": "T0 边沿（\"动作\"）",
  "cfg.edge.r": "上升沿",
  "cfg.edge.f": "下降沿",
  "cfg.mode.label": "待测轴",
  "cfg.mode.c": "点击（T1_CLICK）",
  "cfg.mode.m": "移动（T1_MOTION）",
  "cfg.holdoff.label": "保持时间（µs）",
  "cfg.timeout.label": "超时 / 看门狗（µs）",
  "cfg.apply": "应用配置",
  "cfg.noteDefault": "自由组合：任意 T0 上/下拉 × 边沿 × 轴。若组合特殊，此处会显示提示。",

  "stats.title": "统计信息",
  "stat.last": "最近延迟",
  "stat.avg": "平均值（活动）",
  "stat.minmax": "最小 / 最大",
  "stat.std": "标准差",
  "stat.count": "样本数（活动）",
  "stat.elapsed": "已用时间",
  "stat.seq": "总序号",
  "stat.timouts": "超时 / 丢弃",

  "chart.title": "延迟分布（活动模式）",
  "chart.help": "散点：序号 × 延迟 · 曲线 = 移动平均（64）",
  "hist.title": "直方图（最近 256 个）",

  "exp.title": "事件与导出",
  "exp.csv": "导出 CSV（活动）",
  "exp.json": "导出 JSON（全部）",
  "exp.clear": "清空数据",
  "exp.csvName": "latencia",

  "log.connected": "已连接",
  "log.disconnected": "已断开",
  "log.connectError": "连接错误：",
  "log.readFailed": "读取失败：",
  "log.bufOverflow": "串行缓冲区溢出；已清零",
  "log.timeout": "#{seq} 超时（T1 在 {us} µs 内未到达）",
  "log.order": "#{seq} 顺序异常（t1 早于 t0）",
  "log.dropped": "#{seq} 已丢弃（T0 被覆盖）",

  "note.upFall": "T0 上拉且边沿为下降沿：物理动作断开——接触开关的经典组合。",
  "note.fRise": "T0 浮空、上升沿：简单约定（移动脉冲）。",
  "note.fFloat": "注意：T0 为浮空模式——有噪声风险；请用示波器验证极性。",
  "note.ok": "自由组合正常。",

  "unit.s": "秒",
  "unit.min": "分",
  footer: "个人项目：medidor-rp2350 — 固件 + 静态页面。"
};

I18N_STRINGS.ja = {
  "meta.title": "ゲーミング周辺機器レイテンシメーター",
  "header.title": "ゲーミング周辺機器レイテンシメーター",
  "header.sub": "RP2350 · PIO timestamp · WebSerial · ",
  "header.hint": "brute-offline: file:// または GitHub Pages から開く",
  "lang.label": "言語",

  "conn.title": "接続",
  "conn.connectBtn": "メーターに接続",
  "conn.disconnectBtn": "切断",
  "conn.stateIdle": "未接続",
  "conn.stateOk": "接続済み",
  "ws.warn": "このブラウザでは WebSerial を利用できません。Chrome/Edge（デスクトップ）で HTTPS、localhost、GitHub Pages を使用してください。",
  "ws.notAvail": "このブラウザで WebSerial を利用できません",

  "wiring.title": "配線方法（Pico 2 ピン配置）",
  "wiring.svgAria": "Raspberry Pi Pico 2 のピン配置図",
  "wiring.svgTitle": "Pico 2 のピン配置 — T0、T1_CLICK、T1_MOTION、GND",
  "wiring.legend": "T0 = 物理的アクション · T1_CLICK = クリック応答 · T1_MOTION = 移動応答 · GND = 共通グランド",

  "pin.step1": "<b>T0</b>（物理的アクション：スイッチ / フォトダイオード+コンパレータ）&rarr; <code>GP2</code> = ピン <b>4</b>&nbsp;<span class=\"sw t0\">■</span>",
  "pin.step2": "<b>T1_CLICK</b>（CH32V307 のクリック出力）&rarr; <code>GP3</code> = ピン <b>5</b>&nbsp;<span class=\"sw clk\">■</span>",
  "pin.step3": "<b>T1_MOTION</b>（CH32V307 の移動出力）&rarr; <code>GP4</code> = ピン <b>6</b>&nbsp;<span class=\"sw mot\">■</span>",
  "pin.step4": "<b>GND</b> を基板間で共通にする &rarr; ピン <b>3</b>（GP2 の近く）または <b>8</b>（GP3/GP4 の近く）。",
  "pin.step5": "<b>USB</b> &rarr; Pico 2 自身の USB ポート（WebSerial/CDC）。",
  "pin.step6": "<b>LED GP25</b>（オンボード）：ファームウェア動作中は 500&nbsp;ms ごとに点滅します。",
  "pin.step7": "信号は 3.3&nbsp;V です &mdash; ピンに <b>5&nbsp;V を印加しないでください</b>。",

  "cfg.title": "測定設定",
  "cfg.t0pull.label": "T0 プル / 物理",
  "cfg.t0pull.f": "フロート（プルなし）",
  "cfg.t0pull.up": "プルアップ（アクションで T0 が LOW）",
  "cfg.t0pull.down": "プルダウン（アクションで T0 が HIGH）",
  "cfg.edge.label": "T0 エッジ（\"アクション\"）",
  "cfg.edge.r": "立ち上がり",
  "cfg.edge.f": "立ち下がり",
  "cfg.mode.label": "測定軸",
  "cfg.mode.c": "クリック（T1_CLICK）",
  "cfg.mode.m": "移動（T1_MOTION）",
  "cfg.holdoff.label": "ホールドオフ（µs）",
  "cfg.timeout.label": "タイムアウト / ウォッチドッグ（µs）",
  "cfg.apply": "設定を適用",
  "cfg.noteDefault": "自由な組み合わせ：任意の T0 プル × エッジ × 軸。特殊な組み合わせの場合はここに警告が表示されます。",

  "stats.title": "統計",
  "stat.last": "最新レイテンシ",
  "stat.avg": "平均（アクティブ）",
  "stat.minmax": "最小 / 最大",
  "stat.std": "標準偏差",
  "stat.count": "サンプル数（アクティブ）",
  "stat.elapsed": "経過時間",
  "stat.seq": "総シーケンス",
  "stat.timouts": "タイムアウト / ドロップ",

  "chart.title": "レイテンシ分布（アクティブモード）",
  "chart.help": "scatter：シーケンス × レイテンシ · 線 = 移動平均（64）",
  "hist.title": "ヒストグラム（最新 256）",

  "exp.title": "イベントとエクスポート",
  "exp.csv": "CSV をエクスポート（アクティブ）",
  "exp.json": "JSON をエクスポート（すべて）",
  "exp.clear": "データをクリア",
  "exp.csvName": "latency",

  "log.connected": "接続しました",
  "log.disconnected": "切断しました",
  "log.connectError": "接続エラー：",
  "log.readFailed": "読み取り失敗：",
  "log.bufOverflow": "シリアルバッファオーバーフロー；リセット",
  "log.timeout": "#{seq} タイムアウト（T1 が {us} µs 以内に届きませんでした）",
  "log.order": "#{seq} 順序異常（t1 が t0 より前）",
  "log.dropped": "#{seq} ドロップ（T0 が上書きされました）",

  "note.upFall": "T0 プルアップ + 立ち下がりエッジ：物理的アクションで遮断——接触スイッチの定番構成。",
  "note.fRise": "T0 フロート、立ち上がりエッジ：シンプルな方式（モーションのパルス）。",
  "note.fFloat": "注意：T0 がフロートモード——ノイズの恐れがあります。オシロスコープで極性を確認してください。",
  "note.ok": "自由な組み合わせです。",

  "unit.s": "秒",
  "unit.min": "分",
  footer: "サイドプロジェクト：medidor-rp2350 — ファームウェア + 静的ページ。"
};

I18N_STRINGS.ko = {
  "meta.title": "게이밍 주변기기 지연 측정기",
  "header.title": "게이밍 주변기기 지연 측정기",
  "header.sub": "RP2350 · PIO timestamp · WebSerial · ",
  "header.hint": "brute-offline: file:// 또는 GitHub Pages로 열기",
  "lang.label": "언어",

  "conn.title": "연결",
  "conn.connectBtn": "측정기에 연결",
  "conn.disconnectBtn": "연결 해제",
  "conn.stateIdle": "연결 안 됨",
  "conn.stateOk": "연결됨",
  "ws.warn": "이 브라우저에서는 WebSerial을 사용할 수 없습니다. Chrome/Edge(데스크톱)에서 HTTPS, localhost 또는 GitHub Pages를 사용하세요.",
  "ws.notAvail": "이 브라우저에서는 WebSerial을 사용할 수 없음",

  "wiring.title": "배선 방법 (Pico 2 핀아웃)",
  "wiring.svgAria": "Raspberry Pi Pico 2 핀아웃 다이어그램",
  "wiring.svgTitle": "Pico 2 핀아웃 — T0, T1_CLICK, T1_MOTION 및 GND",
  "wiring.legend": "T0 = 물리적 동작 · T1_CLICK = 클릭 응답 · T1_MOTION = 이동 응답 · GND = 공통 접지",

  "pin.step1": "<b>T0</b> (물리적 동작: 스위치 / 포토다이오드+컴패레이터) &rarr; <code>GP2</code> = 핀 <b>4</b>&nbsp;<span class=\"sw t0\">■</span>",
  "pin.step2": "<b>T1_CLICK</b> (CH32V307의 클릭 출력) &rarr; <code>GP3</code> = 핀 <b>5</b>&nbsp;<span class=\"sw clk\">■</span>",
  "pin.step3": "<b>T1_MOTION</b> (CH32V307의 이동 출력) &rarr; <code>GP4</code> = 핀 <b>6</b>&nbsp;<span class=\"sw mot\">■</span>",
  "pin.step4": "<b>GND</b> 보드 간 공통 접지 &rarr; 핀 <b>3</b> (GP2 근처) 또는 <b>8</b> (GP3/GP4 근처).",
  "pin.step5": "<b>USB</b> &rarr; Pico 2 자체의 USB 포트 (WebSerial/CDC).",
  "pin.step6": "<b>LED GP25</b> (온보드): 펌웨어가 동작 중일 때 500&nbsp;ms마다 깜빡입니다.",
  "pin.step7": "신호는 3.3&nbsp;V입니다 &mdash; 핀에 <b>5&nbsp;V를 인가하지 마세요</b>.",

  "cfg.title": "측정 설정",
  "cfg.t0pull.label": "T0 풀 / 물리",
  "cfg.t0pull.f": "플로팅 (풀 없음)",
  "cfg.t0pull.up": "풀업 (동작이 T0를 LOW로)",
  "cfg.t0pull.down": "풀다운 (동작이 T0를 HIGH로)",
  "cfg.edge.label": "T0 엣지 (\"동작\")",
  "cfg.edge.r": "상승",
  "cfg.edge.f": "하강",
  "cfg.mode.label": "테스트할 축",
  "cfg.mode.c": "클릭 (T1_CLICK)",
  "cfg.mode.m": "이동 (T1_MOTION)",
  "cfg.holdoff.label": "홀드오프 (µs)",
  "cfg.timeout.label": "타임아웃 / 워치독 (µs)",
  "cfg.apply": "설정 적용",
  "cfg.noteDefault": "자유 조합: 임의 T0 풀 × 엣지 × 축. 비정상적인 조합이면 여기에 경고가 표시됩니다.",

  "stats.title": "통계",
  "stat.last": "마지막 지연",
  "stat.avg": "평균 (활성)",
  "stat.minmax": "최소 / 최대",
  "stat.std": "표준 편차",
  "stat.count": "샘플 수 (활성)",
  "stat.elapsed": "경과 시간",
  "stat.seq": "총 시퀀스",
  "stat.timouts": "타임아웃 / 드롭",

  "chart.title": "지연 프로필 (활성 모드)",
  "chart.help": "scatter: 시퀀스 × 지연 · 선 = 이동 평균 (64)",
  "hist.title": "히스토그램 (최근 256개)",

  "exp.title": "이벤트 및 내보내기",
  "exp.csv": "CSV 내보내기 (활성)",
  "exp.json": "JSON 내보내기 (전체)",
  "exp.clear": "데이터 지우기",
  "exp.csvName": "latency",

  "log.connected": "연결됨",
  "log.disconnected": "연결 해제됨",
  "log.connectError": "연결 오류: ",
  "log.readFailed": "읽기 실패: ",
  "log.bufOverflow": "직렬 버퍼 오버플로; 초기화",
  "log.timeout": "#{seq} 타임아웃 (T1이 {us} µs 안에 도착하지 않음)",
  "log.order": "#{seq} 순서 이상 (t1이 t0보다 먼저)",
  "log.dropped": "#{seq} 드롭됨 (T0 덮어씀)",

  "note.upFall": "T0 풀업 + 하강 엣지: 물리적 동작이 차단됨 — 접점 스위치의 전형적인 조합.",
  "note.fRise": "T0 플로팅, 상승 엣지: 간단한 방식 (모션 펄스).",
  "note.fFloat": "주의: T0 플로팅 모드 — 노이즈 위험; 오실로스코프로 극성을 확인하세요.",
  "note.ok": "자유 조합 정상.",

  "unit.s": "초",
  "unit.min": "분",
  footer: "사이드 프로젝트: medidor-rp2350 — 펌웨어 + 정적 페이지."
};

I18N_STRINGS.ru = {
  "meta.title": "Измеритель задержки игровой периферии",
  "header.title": "Измеритель задержки игровой периферии",
  "header.sub": "RP2350 · PIO timestamp · WebSerial · ",
  "header.hint": "brute-offline: откройте через file:// или GitHub Pages",
  "lang.label": "Язык",

  "conn.title": "Подключение",
  "conn.connectBtn": "Подключиться к измерителю",
  "conn.disconnectBtn": "Отключиться",
  "conn.stateIdle": "нет соединения",
  "conn.stateOk": "подключено",
  "ws.warn": "WebSerial недоступен в этом браузере. Используйте Chrome/Edge (desktop) с HTTPS, localhost или GitHub Pages.",
  "ws.notAvail": "WebSerial недоступен в этом браузере",

  "wiring.title": "Подключение (распиновка Pico 2)",
  "wiring.svgAria": "Схема распиновки Raspberry Pi Pico 2",
  "wiring.svgTitle": "Распиновка Pico 2 — T0, T1_CLICK, T1_MOTION и GND",
  "wiring.legend": "T0 = физическое действие · T1_CLICK = ответ клика · T1_MOTION = ответ движения · GND = общая земля",

  "pin.step1": "<b>T0</b> (физическое действие: переключатель / фотодиод+компаратор) &rarr; <code>GP2</code> = контакт <b>4</b>&nbsp;<span class=\"sw t0\">■</span>",
  "pin.step2": "<b>T1_CLICK</b> (выход клика CH32V307) &rarr; <code>GP3</code> = контакт <b>5</b>&nbsp;<span class=\"sw clk\">■</span>",
  "pin.step3": "<b>T1_MOTION</b> (выход движения CH32V307) &rarr; <code>GP4</code> = контакт <b>6</b>&nbsp;<span class=\"sw mot\">■</span>",
  "pin.step4": "<b>GND</b> общий между платами &rarr; контакт <b>3</b> (возле GP2) или <b>8</b> (возле GP3/GP4).",
  "pin.step5": "<b>USB</b> &rarr; собственный USB-порт Pico 2 (WebSerial/CDC).",
  "pin.step6": "<b>LED GP25</b> (на борту): мигает каждые 500&nbsp;мс, пока прошивка работает.",
  "pin.step7": "Сигналы на 3,3&nbsp;В &mdash; <b>не подавайте 5&nbsp;В</b> на контакты.",

  "cfg.title": "Конфигурация измерения",
  "cfg.t0pull.label": "T0 подтяжка / физика",
  "cfg.t0pull.f": "float (без подтяжки)",
  "cfg.t0pull.up": "pull-up (действие ведёт T0 вниз)",
  "cfg.t0pull.down": "pull-down (действие ведёт T0 вверх)",
  "cfg.edge.label": "Фронт T0 (\"действие\")",
  "cfg.edge.r": "нарастающий (rising)",
  "cfg.edge.f": "спадающий (falling)",
  "cfg.mode.label": "Тестируемая ось",
  "cfg.mode.c": "Клик (T1_CLICK)",
  "cfg.mode.m": "Движение (T1_MOTION)",
  "cfg.holdoff.label": "Holdoff (µs)",
  "cfg.timeout.label": "Timeout / watchdog (µs)",
  "cfg.apply": "Применить конфиг",
  "cfg.noteDefault": "Свободные комбинации: любая подтяжка T0 × фронт × ось. Предупреждение о необычной комбинации появится здесь.",

  "stats.title": "Статистика",
  "stat.last": "последняя задержка",
  "stat.avg": "среднее (активно)",
  "stat.minmax": "мин / макс",
  "stat.std": "стандартное отклонение",
  "stat.count": "выборок (активно)",
  "stat.elapsed": "прошло времени",
  "stat.seq": "всего последовательность",
  "stat.timouts": "таймауты / потери",

  "chart.title": "Профиль задержки (активный режим)",
  "chart.help": "scatter: последовательность × задержки · линия = скользящее среднее (64)",
  "hist.title": "Гистограмма (последние 256)",

  "exp.title": "События и экспорт",
  "exp.csv": "Экспорт CSV (активный)",
  "exp.json": "Экспорт JSON (все)",
  "exp.clear": "Очистить данные",
  "exp.csvName": "latency",

  "log.connected": "подключено",
  "log.disconnected": "отключено",
  "log.connectError": "ошибка подключения: ",
  "log.readFailed": "сбой чтения: ",
  "log.bufOverflow": "переполнение последовательного буфера; сброс",
  "log.timeout": "#{seq} ТАЙМАУТ (T1 не пришёл за {us} µs)",
  "log.order": "#{seq} ПОРЯДОК (t1 раньше t0)",
  "log.dropped": "#{seq} ПОТЕРЯНО (T0 перезаписан)",

  "note.upFall": "T0 с pull-up и спадающим фронтом: физическое действие отключается — классическая комбинация контактного переключателя.",
  "note.fRise": "T0 float, нарастающий фронт: простая схема (импульс движения).",
  "note.fFloat": "Внимание: T0 в режиме float — риск помех; проверьте полярность осциллографом.",
  "note.ok": "Свободная комбинация допустима.",

  "unit.s": "с",
  "unit.min": "мин",
  footer: "Побочный проект: medidor-rp2350 — прошивка + статическая страница."
};

const I18N = (() => {
  let lang = "en";

  function t(key, vars) {
    const table = I18N_STRINGS[lang] || I18N_STRINGS.en;
    let s = table[key] !== undefined ? table[key] : I18N_STRINGS.en[key];
    if (s === undefined) s = key;
    if (vars) {
      for (const [k, v] of Object.entries(vars)) {
        s = s.split(`{${k}}`).join(String(v));
      }
    }
    return s;
  }

  function detect() {
    let stored = null;
    try { stored = localStorage.getItem("i18n-lang"); } catch {}
    if (stored && I18N_STRINGS[stored]) return stored;
    const nav = (navigator.language || "en").toLowerCase();
    for (const { code } of LANGS) {
      if (nav === code || nav.startsWith(code + "-")) return code;
    }
    return "en";
  }

  function apply() {
    document.documentElement.lang = lang;
    document.querySelectorAll("[data-i18n]").forEach((el) => {
      el.textContent = t(el.dataset.i18n);
    });
    document.querySelectorAll("[data-i18n-html]").forEach((el) => {
      el.innerHTML = t(el.dataset.i18nHtml);
    });
    document.querySelectorAll("[data-i18n-attr]").forEach((el) => {
      el.dataset.i18nAttr.split(";").forEach((pair) => {
        const idx = pair.indexOf(":");
        if (idx < 0) return;
        const attr = pair.slice(0, idx).trim();
        const key = pair.slice(idx + 1).trim();
        if (attr && key) el.setAttribute(attr, t(key));
      });
    });
    const sel = document.getElementById("lang-sel");
    if (sel) sel.value = lang;
  }

  function set(newLang) {
    if (!I18N_STRINGS[newLang]) return;
    lang = newLang;
    try { localStorage.setItem("i18n-lang", newLang); } catch {}
    apply();
    window.dispatchEvent(new CustomEvent("i18n:change"));
  }

  function init() {
    lang = detect();
    const sel = document.getElementById("lang-sel");
    if (sel) {
      LANGS.forEach(({ code, flag, name }) => {
        const o = document.createElement("option");
        o.value = code;
        o.textContent = `${flag} ${name}`;
        sel.appendChild(o);
      });
      sel.addEventListener("change", () => set(sel.value));
    }
    apply();
  }

  return { t, set, get lang() { return lang; }, init };
})();

if (document.readyState === "loading") {
  document.addEventListener("DOMContentLoaded", I18N.init);
} else {
  I18N.init();
}