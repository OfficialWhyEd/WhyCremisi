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

## Cosa **non** è coperto da questa modifica

1. **Validazione degli argomenti**: `ToolRegistry::executeTool` esegue senza
   controllare tipi, parametri obbligatori o range. Un modello che manda
   `{"track":"due"}` arriva comunque all'esecuzione.
2. **Conferma per le scritture**: nessun passaggio di approvazione prima di
   muovere fader o mutare tracce.
3. **Il giro dopo i tool è in streaming?** No: la chiamata successiva è
   bloccante e non-streaming, con timeout 30 s. Con un modello locale lento
   si vede una pausa lunga (non è un blocco, ma sembra).
4. **Endpoint compatibile OpenAI in uscita** (fase 4) e **server MCP** (fase 5):
   deliberatamente fuori da questo lavoro.

---

## Se qualcosa non torna

```bash
ss -ltn | grep -E ":17373|:9931|:11434"      # chi ascolta
curl -s http://127.0.0.1:9931/v1/models      # llama.cpp risponde?
curl -s http://127.0.0.1:11434/v1/models     # Ollama risponde?
```

Da allegare in caso di segnalazione: le righe del log dal `[AI] Prompt received`
fino a `[AI] Risposta`, più il nome del provider e del modello.
