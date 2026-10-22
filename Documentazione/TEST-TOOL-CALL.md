# Test delle tool call — cosa verificare e come

**Ultimo aggiornamento:** 22 ottobre 2026
**Ramo:** `feat/conferma-reale-comandi-daw`

Questo file dice **cosa provare** dopo la riscrittura del percorso delle tool
call. Non è un elenco di cose "già funzionanti": è la lista dei controlli da
fare, provider per provider, perché non tutti si possono provare sulla stessa
macchina.

---

## Cosa è cambiato (in una pagina)

Prima: i provider convertivano le tool call native in **testo**
(`[TOOL_CALL:nome {json}]`), il motore non le rileggeva, e la chat si fermava
dopo ogni chiamata. In più gli argomenti di Claude andavano persi
(`content_block_start.input` è sempre `{}`).

Ora:

| Cosa | Dove |
|---|---|
| Le tool call viaggiano come **dati** (`getStreamToolCalls`), accumulate per `index` | `src/ai/AIProvider.cpp` |
| Il marcatore `[TOOL_CALL:…]` in chat è solo **vista**: lo genera il bridge con gli argomenti già completi | `src/bridge/OscBridge.cpp` |
| Anthropic: accumulo di `input_json_delta`, parse solo a `content_block_stop` | `AnthropicToolAccumulator` |
| Gemini: tool native (`functionDeclarations`), `functionCall` in streaming, argomenti a pezzi | `GeminiToolAccumulator` |
| Ollama: passa dall'endpoint **compatibile OpenAI** (`/v1/chat/completions`), quindi con tool native | `OllamaProvider` |
| Storico **canonico**: assistant con `tool_calls`, poi messaggi `tool` con `tool_call_id` | `ContextManager`, `buildContextMessagesJson` |
| Ogni provider traduce lo storico nel proprio dialetto | `anthropicMessagesFromNeutral`, `geminiContentsFromNeutral` |
| Id mancanti: generati da WY (`why-<n>-<indice>`) | `AiEngine::resolveToolCalls` |
| Più call nello stesso turno: eseguite **tutte, in ordine**, esito per ciascuna | `AiEngine::resolveToolCalls` |

Test automatici che coprono questa parte (senza rete):

```bash
cmake -B build-test -DBUILD_UNIT_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-test --target WhyCremisiTests --parallel 8
./build-test/WhyCremisiTests_artefacts/Release/WhyCremisiTests
```

Devono risultare **3350 controlli, 0 falliti**, di cui 41 nella suite
`ToolCallParsing` (accumulatori, marcatori, storico Anthropic e Gemini).

---

## Come si guarda cosa succede

```bash
tail -f ~/.config/WhyCremisi/whycremisi.log
```

Le righe che contano:

| Riga | Significato |
|---|---|
| `[ai] strumento <nome> <argomenti> -> OK: …` | lo strumento è stato **eseguito** |
| `[ai] strumento <nome> <argomenti> -> ERRORE: …` | eseguito ma fallito: il motivo è nel testo |
| `[ai] strumento <nome> argomenti non validi` | il modello ha mandato JSON rotto: non eseguito |
| `[AI] Risposta in <ms>ms: …` | turno chiuso, con l'anteprima del testo |
| `config.response` con `key: ai.testConnection` | esito della prova di connessione |

**Regola d'oro:** se in chat compare `[TOOL_CALL:…]` e nel log **non** compare
`[ai] strumento`, la chiamata non è stata eseguita: è un fallimento, non una
risposta.

---

## Matrice dei test

### 1. llama.cpp (locale) — l'unico provato senza credenziali

```bash
llama-server -m /percorso/modello.gguf --port 9931 --jinja
```

Nel plugin: `SUL TUO PC` → chip **llama.cpp** → indirizzo
`http://127.0.0.1:9931` → **Inizia**.

| Prova | Frase | Esito atteso |
|---|---|---|
| connessione | — | Inizia diventa verde |
| tool di lettura | "che stato ha il DSP?" | compare `[TOOL_CALL:dsp.getState …]`, poi **una risposta in prosa**; nel log `[ai] strumento dsp.getState … OK` |
| tool di trasporto | "fai partire" | Reaper parte; log `[ai] strumento …` |
| due call in un turno | "analizza il master e dimmi se clipa" | due righe `[ai] strumento`, poi una sola risposta |
| niente 400 | qualunque frase dopo la prima | nessun errore `All non-assistant messages must contain 'content'` |

Nota: `--jinja` è **obbligatorio**, senza non ci sono tool call native.
La porta 8080 è occupata dal WebSocket del plugin (ora su 17373): usane
un'altra, 9931 è il default di `llama-server` in questa build.

### 2. Ollama

```bash
ollama serve            # endpoint compatibile: /v1/chat/completions
ollama pull llama3.2
```

| Prova | Esito atteso |
|---|---|
| "fai partire" | tool eseguito, non testo |
| modello senza supporto tool (es. un `codellama` vecchio) | nessun crash: o tool call native, o fallback testuale; in chat si vede il marcatore, nel log l'esecuzione |

Da verificare: la versione di Ollama deve esporre `/v1/chat/completions`
(le versioni recenti lo fanno; `curl http://127.0.0.1:11434/v1/models` per
controllare).

### 3. Google Gemini (API nativa) — **non provato: serve una chiave**

Chiave da `aistudio.google.com`. Provider `GEMINI` → incolla la chiave → modello
`gemini-2.0-flash` o superiore (i modelli senza function calling non servono).

| Prova | Esito atteso |
|---|---|
| "che stato ha il DSP?" | `functionCall` in streaming, argomenti completi, tool eseguito |
| due call in un turno | entrambe eseguite in ordine |
| turno successivo | lo storico contiene `functionCall`/`functionResponse`: il modello **ricorda** cosa ha già chiesto |

### 4. OpenAI / OpenRouter / Groq

Stesso percorso (dialetto OpenAI). Con OpenRouter basta una chiave gratuita.

| Prova | Esito atteso |
|---|---|
| "spegni il limiter" | tool eseguito, risposta in prosa |
| "e poi rimettilo" | il secondo turno vede il risultato del primo (storico con `tool_call_id`) |

### 5. Anthropic (Claude) — il caso che perdeva gli argomenti

Chiave API oppure abbonamento (pulsante "Collega il mio account Claude").

| Prova | Esito atteso |
|---|---|
| "metti la traccia 2 a -6 dB" | il marcatore mostra `{"track":2,"db":-6}`, **non** `{}` |
| tool eseguito | `[ai] strumento daw.setVolume … OK` |

Se il marcatore mostra `{}`, la regressione è tornata: gli argomenti non
vengono accumulati da `input_json_delta`.

### 6. Claude Code / ChatGPT Codex (accesso da abbonamento)

Questi due **non** ricevono strumenti nativi (per scelta: il programma esterno
non li espone). Restano sul protocollo testuale JSON.

| Prova | Esito atteso |
|---|---|
| "fai partire" | la risposta contiene il JSON `{"calls":[…]}` e il tool viene eseguito dal percorso testuale |
| log | `[ai] strumento …` presente |

---

## L'alea che resta: gli argomenti delle call non vengono validati

Questa e' la parte da decidere **prima** di aggiungere altro codice.

Oggi `ToolRegistry::executeTool` (`src/ai/ToolRegistry.cpp`) inoltra la call
all'executor senza controllare niente. **Attenzione**: i tool `daw.*` validano
già nel loro ramo (`readInt`/`readNumber` con range, vedi `OscBridge.cpp`); la
mancanza riguarda gli altri rami (gruppo "sospesi" sotto) e il livello del
registro. L'executor e' una catena di
`if/else` sul nome (`src/bridge/OscBridge.cpp:4659`) che legge gli argomenti
con `value()` o `get<>()`. Le conseguenze, per classe di errore:

| Errore del modello | Cosa succede oggi | Visibile? |
|---|---|---|
| Nome strumento inesistente | `success = false`, `"Unknown tool: …"` | **Sì**: il modello lo legge e puo' correggere |
| Parametro obbligatorio mancante | il tool usa un default implicito o legge un valore assente | **No**: il log dice `OK` |
| Tipo sbagliato (`"due"` dove serve un numero) | `nlohmann` lancia un'eccezione, catturata dal bridge: **l'intero turno fallisce** con "Errore interno durante la richiesta" e il modello non riceve nulla su cui correggere | Parzialmente: si vede l'errore, non la causa |
| Valore fuori scala (es. volume `0..1` invece dei dB) | viene eseguito: il fader si muove in un punto sbagliato e lo strumento riporta `OK` | **No**: l'azione sbagliata e' indistinguibile da una giusta |
| Parametro sconosciuto (nome inventato) | ignorato in silenzio: il modello crede di aver chiesto altro | **No** |

Due di questi casi sono la vera alea: **l'azione sbagliata viene eseguita e
registrata come riuscita**. Non e' un errore che si nota: e' un errore che
produce un log pulito.

### Vantaggi della validazione, per classe di call

1. **Call con parametri mancanti** → oggi esegue con un default che nessuno ha
   scelto; con la validazione torna al modello l'elenco di cosa manca, e il
   turno successivo e' corretto invece di essere plausibile.
2. **Call con tipi sbagliati** → oggi costa il turno intero (eccezione); con la
   coercizione (`"-6"` → `-6`) la call passa, altrimenti l'errore arriva al
   modello come testo, che puo' riprovare.
3. **Call con valori fuori scala** → oggi muovono la DAW e risultano `OK`; con
   i range dichiarati vengono rifiutate **prima** di toccare la DAW, e il
   modello riceve il range ammesso.
4. **Call con parametri sconosciuti** → oggi l'intenzione si perde in silenzio;
   con un warning nel log l'errore e' almeno **registrato**.
5. **Diagnosi** → ogni rifiuto e' una riga nel log (`[ai] strumento … rifiutato:
   motivo`). Oggi, per un'azione sbagliata, non c'e' nessuna riga da cercare.

### Cosa serve per farla senza inventare vincoli

Su **39 tool** registrati in `ToolRegistry::registerBuiltinTools`, solo **2**
dichiarano `min/max` (`daw.track.setVolume`: −96…12 dB, `daw.track.setPan`:
−1…1) e 2 dichiarano un `enum`. Quei range li ha scritti chi conosce la DAW:
non sono deducibili dal codice.

Quindi la regola: **si valida solo cio' che e' dichiarato; nessun vincolo
inventato.** I range mancanti vanno compilati da chi conosce i tool (una
riga per parametro), poi la validazione li applica senza altre modifiche.

### Cosa la validazione **non** risolve

Il modello che sceglie lo strumento giusto con un valore **valido ma non
desiderato** ("muta la traccia 1" quando l'utente intendeva la 2). Per quello
serve una conferma prima delle scritture: e' un passo separato, con una
modifica alla UI.

---

## Cosa **non** è coperto da questa modifica

1. **Validazione degli argomenti**: `ToolRegistry::executeTool` esegue senza
   controllare tipi, parametri obbligatori o range. Dettagli e conseguenze
   nella sezione precedente: e' il punto da cui ripartire.
2. **Conferma per le scritture**: nessun passaggio di approvazione prima di
   muovere fader o mutare tracce.
3. **Il giro dopo i tool è in streaming?** No: la chiamata successiva è
   bloccante e non-streaming, con timeout 30 s. Con un modello locale lento
   si vede una pausa lunga (non è un blocco, ma sembra).
4. **Endpoint compatibile OpenAI in uscita** (fase 4) e **server MCP** (fase 5):
   deliberatamente fuori da questo lavoro.

---

## Appunti per lo sviluppatore

### L'interfaccia deve scalare con il ridimensionamento della finestra

**Stato attuale (verificato nel codice):**
- `src/core/PluginEditor.cpp:129` → `setSize (1100, 820)`; nessuna chiamata a
  `setResizable`/`setResizeLimits` nel file, quindi la finestra si apre a
  misura fissa.
- Il layout React è scritto in pixel/Tailwind per il desktop (la schermata di
  setup usa `max-w-lg`, i pannelli usano griglie a 12 colonne): anche
  abilitando il ridimensionamento, il contenuto non si adatterebbe da solo.

**Richiesta:** l'interfaccia deve adattarsi alla dimensione della finestra,
sia ingrandendo sia rimpicciolendo, senza tagliare controlli.

**Due strade, con i loro costi:**

| Strada | Come | Pro | Contro |
|---|---|---|---|
| A. Scala globale | `transform: scale()` sull'intero contenuto, calcolato dal rapporto fra dimensione finestra e 1100×820 | Pochissimo codice, identico a quello che si vede oggi | Testo sfocato a fattori non interi; aree di click che non corrispondono più ai pixel; accessibilità |
| B. Layout fluido | `setResizable(true)` + `setResizeLimits(...)`, unità relative, griglie con frazioni, `clamp()` per i font | Nitido a ogni dimensione, niente pixel fuori posto | Più lavoro e più punti da rivedere: ogni pannello e ogni schermata |

**Consiglio:** B, con limiti espliciti (es. minimo 900×620, massimo 1920×1200)
e una sola scala tipografica condivisa, così i pannelli non vanno rifatti uno
per uno.

**Da verificare quando si farà:**
- comportamento nei DAW che ripristinano la dimensione salvata del plugin
  (lo stato dell'editor deve essere persistito);
- che i limiti dichiarati all'host corrispondano a quelli della UI;
- che la schermata di setup resti utilizzabile alla dimensione minima
  (ha già `max-h-[96vh]` con scroll: va provata).

---

## Se qualcosa non torna

```bash
ss -ltn | grep -E ":17373|:9931|:11434"      # chi ascolta
curl -s http://127.0.0.1:9931/v1/models      # llama.cpp risponde?
curl -s http://127.0.0.1:11434/v1/models     # Ollama risponde?
```

Da allegare in caso di segnalazione: le righe del log dal `[AI] Prompt received`
fino a `[AI] Risposta`, più il nome del provider e del modello.

---

# Censimento dei tool: cosa è condiviso fra REAPER e Ableton

**Fonti:** `src/ai/ToolRegistry.cpp` (37 tool) · `~/.config/REAPER/OSC/Default.ReaperOSC`
(la configurazione reale) · README di AbletonOSC (`ideoforms/AbletonOSC`).

Tre gruppi. Solo il gruppo B può avere **una call davvero condivisa**: cambia
l'indirizzo, non il significato.

## A — Interni al plugin: identici su ogni DAW (15)

`dsp.compressor.set` · `dsp.limiter.set` · `dsp.bypass` · `dsp.getState` ·
`mix.analyze` · `ui.showPanels` · `samples.scan` · `samples.search` ·
`memory.remember` · `memory.search` · `memory.forget` · `skill.search` ·
`skill.load` · `mcp.list` · `mcp.call`

Nessuna dipendenza dalla DAW: non c'è niente da astrarre.

## B — Comuni: stessa intenzione, indirizzi diversi (10 + 3 parziali)

| Tool | REAPER (config reale) | Ableton (AbletonOSC) | Note |
|---|---|---|---|
| `daw.transport.play` | `t/play` | `/live/song/start_playing` | ok entrambi |
| `daw.transport.stop` | `t/stop` | `/live/song/stop_playing` | ok entrambi |
| `daw.transport.record` | `t/record` = **toggle** | `/live/song/set/session_record`, `trigger_session_record` | **parziale**: Reaper non ha "record on/off", solo toggle; Ableton distingue sessione/arrangiamento |
| `daw.transport.setTempo` | `f/tempo/raw` | `/live/song/set/tempo` | ok entrambi (mai `/tempo`, che è normalizzato) |
| `daw.track.setVolume` | `f/track/@/volume/db` | `/live/track/set/volume` (lineare) | **convenzione**: nel contratto il valore è dB; l'adattatore converte |
| `daw.track.setPan` | `n/track/@/pan` | `/live/track/set/pan` | entrambi −1…1 |
| `daw.track.mute` | `b/track/@/mute` (0/1) | `/live/track/set/mute` | ok entrambi |
| `daw.track.solo` | `b/track/@/solo` (0/1) | `/live/track/set/solo` | ok entrambi |
| `daw.plugin.setParam` | `n/track/@/fx/@/fxparam/@/value` | `/live/device/set/parameter/value` | **convenzione**: come si identifica il plugin (slot/indice vs nome) |
| `daw.plugin.bypass` | `b/track/@/fx/@/bypass` | non esiste un bypass: si passa dal parametro "Device On" | Reaper pieno, Ableton **parziale** |
| `mix.levels` | feedback su `track/@/volume`, `…/mute` | `/live/track/get/volume` … | Ableton pieno; Reaper **parziale** (solo ciò che la config rimanda) |
| `mix.snapshot.save` / `recall` | possibile: scrive su `…/volume/db` + mute; **rileggere** è il problema | pieno (get + set) | **parziale**: su Reaper manca la lettura affidabile |
| `daw.marker.goto` / `daw.marker.set` | solo **lettura** in questa config (`s/marker/@/name`, `f/marker/@/time`): non si creano né si saltano | **sì**, come cue point: `/live/song/cue_point/jump`, `/live/song/set_or_delete_cue` | su Live sì, su REAPER **no** |

## C — Solo Ableton (7)

| Tool | Perché | Su Reaper |
|---|---|---|
| `ableton.setup` | installa AbletonOSC | non ha senso |
| `music.compose` | scrive note in una clip: `/live/clip/add/notes` | OSC Reaper non scrive MIDI |
| `beat.create` | come sopra, dai sample | idem |
| `device.list` | enumerazione dei device di Live | Reaper non espone l'elenco FX via OSC |
| `device.params` | parametri con nome/valore di Live | parziale: solo se Reaper li manda (`s/fx/@/name`) |
| `device.setParam` | impostazione + rilettura su Live | coperto da `daw.plugin.setParam` |
| `device.dictionary` | dizionario dei parametri di Live | solo da feedback, non enumerabile |

## Contratto del risultato (implementato)

`ToolResult` ora porta, oltre a `output`:
`outcome` (`Done | Unsupported | Rejected | Failed | Unconfirmed`),
`confirmation` (`NotApplicable | Unconfirmed | Confirmed`), `data`, `warnings`.
Il testo che il modello legge nasce da **un solo** punto, `renderToolResult`:
la forma è identica per ogni DAW, cambia solo l'evidenza (confermato o no).

- un comando `daw.*`/`mix.*` che non è stato riletto è **Unconfirmed** per
  default: mai un "OK" finto;
- un tool che la DAW non ha → **Unsupported** con il motivo, e non viene
  eseguito;
- argomenti non interpretabili → **Rejected**.

## Capacità per DAW (implementato)

`src/daw/DawCapabilities.cpp` tiene tre elenchi: universali (15), comuni (11),
solo Ableton (7) + marker (2), più i **sospesi** (`daw.plugin.setParam`,
`daw.plugin.bypass`: l'executor li rifiuta sempre, quindi non si offrono più).

`AiEngine` filtra i tool offerti in base alla DAW attiva e aggiunge al system
prompt una riga che dice quale DAW è aperta e **cosa non esiste lì**. Così
l'agente sa, per esempio, che su REAPER `device.*` e `music.compose` non ci
sono, invece di chiamarli e ricevere "AbletonOSC non risponde".

## Le tre convenzioni da fissare nel contratto

1. **Indice traccia: 0-based sempre.** REAPER parte da 1 → l'adattatore somma 1
   (`/track/@`), Ableton parte da 0. Oggi il valore passa crudo: su Reaper è
   off-by-one.
2. **Unità nel contratto, non nel provider.** `setVolume` riceve **dB**;
   l'adattatore sceglie `…/volume/db` (Reaper) o converte per Live. Oggi il
   valore cambia scala a seconda del percorso (handler = lineare, fallback = dB).
3. **Stato, non toggle.** `record(on/off)` e `mute(on/off)`: dove la DAW ha solo
   un toggle (Reaper `/record`), l'adattatore deve conoscere lo stato corrente
   (dal feedback) e agire solo se serve.

## Conseguenza per il contratto dei tool

- Gruppo **A**: nessuna modifica.
- Gruppo **B**: il tool resta uno solo; cambia l'adattatore. È qui che si
  guadagna l'astrazione.
- Gruppo **C**: il tool **non va offerto** al modello quando la DAW non lo
  supporta, e se chiamato deve rispondere *"non disponibile su REAPER"*, non
  *"AbletonOSC non risponde"*.

Senza questo censimento, ogni tool del gruppo C è un turno sprecato e un
messaggio d'errore fuorviante. Con il censimento, la matrice diventa una tabella
di capacità per adattatore, e il modello vede solo ciò che esiste davvero.
