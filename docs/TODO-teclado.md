# TODO — Teclado: duração do pressionamento (hold)

Status: planejamento fechado, implementação pendente.

## Escopo

- Repositório: `gamer-latency-meter`.
- **Firmware WCH/CH32V307 não será alterado.**
- Mudam: firmware RP2350, app web, relatórios, i18n e documentação.
- Métrica principal: duração do pressionamento reportado (`hold_t1`).
- Métricas secundárias: hold físico (`hold_t0`), latência de press e latência de release.
- Sinais: `T0` e `T1_CLICK`, ambos com as duas bordas capturadas.
- Apresentação: gráfico + estatísticas, com exportação nos 4 formatos existentes.
- Semântica: **“qualquer tecla”**. O hold é do sinal `T1_CLICK`, não de uma tecla específica.

## Desenho da medição

```
T0 press ─────────────────────────────┐
   T1 rise ────────────────────────┐   │
                                   │   │
T0 release ────────────────────────┼───┘
   T1 fall ────────────────────────┘

hold_t0  = t0_fall - t0_rise
hold_t1  = t1_fall - t1_rise
press    = t1_rise - t0_rise
release  = t1_fall - t0_fall
```

- `hold_t1` é o dado primário.
- `hold_t0` serve para comparar o físico com o reportado.
- `release` pode ser negativo se o `T1` cair antes do `T0`; nesse caso, marcar a flag de ordem.
- As duas descidas (`t0_fall` e `t1_fall`) podem chegar em qualquer ordem.
- O sample só é válido com as quatro bordas (`t0_rise`, `t0_fall`, `t1_rise`, `t1_fall`).
- Se qualquer borda não chegar dentro do timeout, descartar como `TIMEOUT`.

## Contrato de dados

Frame novo de **40 bytes**, mantendo o magic `0xAA` e usando `type=4` para diferenciar do frame atual de 20 bytes.

```
offset  size  field
0       1     magic 0xAA
1       1     type 4 (KEY_HOLD)
2       1     mode 2 (hold)
3       1     flags        bit0 = release antes do T0
4       4     seq          u32
8       4     t0_rise      u32 (contagem PIO)
12      4     t0_fall      u32
16      4     t1_rise      u32
20      4     t1_fall      u32
24      4     hold_t1_ns   u32
28      4     hold_t0_ns   u32
32      4     press_ns     u32
36      4     release_ns   i32 (pode ser negativo)
```

O parser do app lê o `type` no offset 1 e escolhe o tamanho do frame (20 ou 40 bytes).

## Fase 1 — RP2350 + PIO

- [ ] Criar `MODE_HOLD=2`; `CONF` aceita modo `h`.
- [ ] Em hold, usar **4 SMs no PIO0**: `T0 rise`, `T0 fall`, `T1 rise`, `T1 fall`, com `pio_enable_sm_mask_in_sync(pio, 0xF)`.
- [ ] Preservar o modo mouse atual (3 SMs: T0, T1_CLICK, T1_MOTION).
- [ ] Usar `cfg.t0_rising` para definir qual borda do T0 é o “press” (T0 pode ser ativo alto ou baixo).
- [ ] Ler e intercalar as 4 FIFOs por timestamp, com comparação wrap-aware de 32 bits.
- [ ] FSM: `T0 press → T1 rise → T1 fall + T0 fall`; aceitar as duas descidas em qualquer ordem e marcar `release_before_t0`.
- [ ] Debounce do T0 nas duas bordas; reutilizar `holdoff_us` (preset de teclado com 5–10 ms).
- [ ] Timeout de press/T0 release usando `timeout_us`; novo `hold_timeout_us` opcional (6º argumento do `CONF`, default ~5 s).
- [ ] Descartar como `TIMEOUT` se qualquer uma das quatro bordas não chegar.
- [ ] Calcular `hold_t1`, `hold_t0`, `press` e `release`; tratar wrap e limite de ~57 s.
- [ ] Emitir `type=4` de 40 bytes; ajustar ring/drain para frames de 20 e 40 bytes.
- [ ] `STAT` passa a imprimir `mode=h` e os contadores de hold.
- [ ] Validar no osciloscópio que as 4 SMs continuam em lockstep.

## Fase 2 — Web app

- [ ] Seletor “Dispositivo: mouse/teclado”, persistido em `localStorage`.
- [ ] Modo “Key hold (T1_CLICK)” em teclado; esconder/ignorar motion.
- [ ] Parser de frame variável (20/40 bytes) e novo `DATA.hold`.
- [ ] Guardar `{seq, holdT1, holdT0, press, release, flags, timestamps}`.
- [ ] Seletor de métrica para gráfico/estatísticas: `hold T1` (default), `hold T0`, `press`, `release`.
- [ ] Gráfico de dispersão `seq × métrica` e histograma da métrica ativa.
- [ ] Cards de estatísticas para as 4 métricas: min, mediana, média, p95, p99, max, std.
- [ ] Config: enviar `CONF ... h ... <hold_timeout_us>`; preset de teclado (contato/chave, holdoff maior).
- [ ] Adaptar textos, legendas e estados vazios para teclado/hold.
- [ ] Manter o fluxo de mouse sem alterações visuais.

## Fase 3 — Relatórios + i18n

- [ ] Título próprio: “KEYBOARD HOLD REPORT”.
- [ ] Eixo aditivo `hold` com 4 métricas: `hold_t1`, `hold_t0`, `press`, `release`.
- [ ] `.txt`/`.md`: sumário por métrica; histograma e “piores samples” para `hold_t1`.
- [ ] `.csv`: um bloco por métrica, mantendo `"sample","seq","latency_us|ms"`; unidade escolhida pela mediana de cada métrica.
- [ ] `.json`: `axes.hold.metrics.*` com stats, display e readings; manter `schema@1` (aditivo) e incluir `device`.
- [ ] Metadados: `device=keyboard`, `mode=h`, `hold_timeout_us`.
- [ ] Novas chaves i18n nos 10 idiomas e paridade no `tests/i18n.test.mjs`.
- [ ] Exportar hold nos 4 formatos; mouse continua exportando click/motion como hoje.

## Fase 4 — Testes

- [ ] Node: parser de frame de 40 bytes, 4 métricas, frames 20/40 misturados, i18n.
- [ ] Relatório: fixtures de hold para txt/md/csv/json, incluindo `release_ns` negativo.
- [ ] App smoke: seleção de teclado, modo hold, injeção de frames e exportação.
- [ ] Hardware: hold curto/longo, bounce, acorde, modificador, repetição, release + novo press, timeout, mouse sem regressão.
- [ ] Critérios: 1 sample por press-release; repetição não gera sample; T0_fall ausente vira timeout; mouse continua passando 110/181/56.

## Fase 5 — Documentação

- [ ] README: seção “Keyboard hold duration”, wiring do T0, o que é `hold_t1` vs `hold_t0`.
- [ ] `docs/protocol.md`: `mode=h`, frame `type=4` de 40 bytes, 6º argumento do `CONF`, `STAT mode=h`.
- [ ] Explicar que `press`/`release` são derivados e que `release` pode ser negativo.
- [ ] Ressalvas: “qualquer tecla”; scan/debounce do teclado está dentro da medição; a resolução de 13 ns não é a resolução do hold; não mede polling rate; não distingue tecla.
- [ ] Registrar que o firmware WCH não foi alterado e que teclados não suportados por ele continuam fora do escopo.

## Limitações que a documentação precisa assumir

- O hold é do **sinal any-key** (`T1_CLICK`), não de uma tecla específica.
- Segurar uma tecla gera **um** hold; repetição automática não gera novos samples.
- Em acorde, o hold vai até a **última** tecla soltar.
- A taxa de report do teclado domina a incerteza; os 13 ns são só do timestamp.
- Teclados composite/NKRO/Report ID problemáticos continuam limitados pelo firmware WCH atual.

## Critérios de aceite

- [ ] Modo teclado/hold mostra gráfico e estatísticas das 4 métricas.
- [ ] Relatório hold funciona em txt, md, csv e json.
- [ ] Um press-release gera exatamente um sample válido.
- [ ] T0_fall ausente não gera sample.
- [ ] Mouse e suíte atual sem regressão.
- [ ] Documentação explica claramente o que está dentro e fora da medição.
