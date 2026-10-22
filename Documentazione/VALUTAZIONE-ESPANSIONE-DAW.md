# Valutazione: comunicazione VST↔DAW, tool call e espansione ad altre DAW

**Data:** 22 ottobre 2026
**Contesto:** sessione di collaudo su REAPER (Linux) di `feat/conferma-reale-comandi-daw`.

Scritto per essere condiviso con lo sviluppatore. Distingue **fatti verificati**
da **valutazioni**.

---

## 1. «I test sono stati fatti solo su Ableton» — è vero, ed è documentato

Fatti verificabili:

- `Documentazione/COLLAUDO.md` lo dichiara: *"Matrice di host: oggi si prova
  solo su Ableton"*.
- La conferma dei comandi è cablata su Live: `verificaConLettura` interroga
  `/live/...`, e la curva del fader usata per la verifica è quella di Live 11
  (misurata il 22/10).
- Copertura: **49 indirizzi `/live/...`** usati dal plugin; **264** esposti da
  AbletonOSC. Lato REAPER: una decina di verbi, senza lettura di ritorno.
- REAPER era **teorico**, non collaudato. Tre difetti trovati il 22/10:
  1. destinazione OSC di default `127.0.0.1:9001` (nessuno ascolta): i comandi
     uscivano nel vuoto mentre il log diceva `SENT`;
  2. scrittura del volume su `/track/N/volume/db`, che è **solo feedback**
     (le scritture passano da `/track/N/volume`, posizione normalizzata);
  3. buffer del feedback con `std::atomic` non inizializzate: la conferma era
     impossibile anche con il feedback in arrivo.
- La DAW non veniva riconosciuta dall'host (`detectFromHost` era uno stub che
  rispondeva sempre `Unknown`): in Ableton i tool esclusivi di Live non
  venivano offerti, e l'agente rispondeva correttamente «non posso» — sulla
  lista che aveva ricevuto.

Tutti e quattro sono stati corretti; la suite è a 3435 controlli, 0 falliti.

---

## 2. Espandere alle altre DAW: la leva è il **trasporto**, non la singola DAW

Ogni DAW richiede quattro strati, non una tabella di indirizzi:

| Strato | Contenuto |
|---|---|
| **Trasporto** | OSC · MCU/MIDI · script nativi (ReaScript, Scripter) · API proprietarie |
| **Mapping** | verbi → messaggi, unità, indici (0-based nel contratto) |
| **Prova** | cosa si può rileggere, e quindi quando dire `Confirmed` |
| **Capacità** | cosa quella DAW non sa fare (il tool non viene offerto) |

**Punto chiave:** Logic Pro **non ha OSC**. Si pilota via **MCU (MIDI)**, e WY
ha già `src/daw/MackieDawHandler.cpp`. Quindi il lavoro non è «aggiungere
Logic»: è **aggiungere il trasporto MIDI/MCU** e poi mappare su quello Logic,
Cubase, Studio One e Pro Tools (che condividono lo stesso dialetto).

Raggruppare per trasporto, non per DAW, evita di riscrivere lo stesso lavoro
cinque volte.

| Trasporto | DAW |
|---|---|
| OSC + Remote Script | Ableton (AbletonOSC) |
| OSC nativo | REAPER, e altri con OSC documentato |
| MCU / MIDI | Logic, Cubase, Studio One, Pro Tools, FL Studio (parziale) |
| Script nativi | REAPER (ReaScript), Logic (Scripter) — per ciò che OSC non copre |

---

## 3. Cosa manca perché una DAW sia «supportata»

1. **Adattatore** con i verbi del contratto (traccia 0-based, volume in dB).
2. **Lettura di ritorno** ove possibile; altrimenti `Unconfirmed` dichiarato.
3. **Dichiarazione di capacità** (matrice) così il modello non promette ciò che
   non esiste.
4. **Test sul filo** (deterministici, senza DAW) + smoke test su host reale.

Casi attuali da chiudere:
- **REAPER**: creare una traccia si può (`/action/40001`, fatto). Clip, item e
  note MIDI **no**: la OSC di REAPER non ha pattern per crearli — serve
  ReaScript. `mix.levels`/`mix.snapshot.*` sarebbero possibili sfruttando il
  feedback di volume/pan/mute, ma non sono implementati: per ora sono nel
  gruppo Live.
- **Ableton**: `music.compose`, `beat.create`, `device.*`, marker — implementati.
  Mancano creazione/duplicazione/eliminazione tracce come *tool* (i comandi
  AbletonOSC esistono: `/live/song/create_midi_track`, `duplicate_track`,
  `delete_track`).

---

## 4. Macchine virtuali per i test

- **Vincolo legale/pratico:** Logic è macOS-only. VM macOS su host non-Apple
  violano la licenza Apple. Realistico: **un Mac** (mini o VM su hardware
  Apple) per Logic/Ableton, **una VM Windows** per Cubase/FL/Studio One/REAPER.
- **Costo/beneficio:** i test GUI sono lenti e fragili. La parte più efficace
  è già disponibile: i **test sul filo** (`src/daw/tests/`), che asseriscono
  indirizzo e valori senza aprire una DAW. Le VM servono per **pochi smoke
  test reali**, non come strategia principale.
- **Proposta:** CI con `pluginval` + unit test sul filo; matrice ridotta di
  host reali (2 ambienti: Windows e macOS) con uno script che guida la DAW via
  OSC/MIDI e verifica il feedback.

---

## 5. Agenti, tool call e modelli locali: due equivoci da sciogliere

**a) Il WebSocket non è una tool call.** È il trasporto tra interfaccia e
plugin: il modello non lo vede. Le tool call passano dalle API dei provider; il
plugin le esegue.

**b) Le funzioni agentiche non sono limitate a MCP.** WY usa **function calling
nativo** per 7 provider su 9 (OpenAI, OpenRouter, Groq, Anthropic, Gemini,
llama.cpp, Ollama via `/v1/chat/completions`). MCP serve solo per i server
esterni che l'utente collega.

**Cosa serve davvero a un modello locale:**
- un **template di chat che esponga i tool** (`llama-server --jinja` nel caso di
  llama.cpp): senza, il modello non emetterà mai `tool_calls` native;
- addestramento all'uso degli strumenti (Qwen, Llama 3.x, Mistral lo hanno);
- argomenti validi più che «intelligenza»: un 27B va bene, un modello senza
  template no, indipendentemente dalla dimensione;
- a 11 tok/s le tool call sono lente ma funzionanti: il rischio è
  l'affidabilità degli argomenti, non la velocità.

**Verifica proposta:** batteria di ~20 casi (strumento giusto? argomenti
corretti? rifiuta strumenti inesistenti? riconosce la DAW?) contro il modello
locale, con il fallback testuale come rete. Da aggiungere come
**qualificazione dei modelli supportati**.

---

## 6. Priorità consigliate

1. **Nessun «fatto» senza prova**: `Confirmed` solo con lettura di ritorno;
   `Unconfirmed` dichiarato; nessun successo silenzioso.
2. **Trasporto prima delle DAW**: completare OSC, aggiungere **MCU/MIDI**, poi
   mappare Logic → Cubase → Studio One → Pro Tools.
3. **Registro unico per DAW**: trasporto, porta, adattatore, capacità, tipo di
   prova. Un solo punto da leggere e verificare.
4. **Test**: sul filo per ogni adattatore in CI + pluginval + matrice ridotta di
   host reali.
5. **Qualificazione modelli**: requisiti minimi documentati e batteria di casi.

---

## 7. Cosa è già stato fatto in questa sessione

- Tool call native in streaming per OpenAI/Anthropic/llama.cpp/Ollama/Gemini,
  con accumulatori provati da test.
- Storico canonico per dialetto (OpenAI / Anthropic / Gemini).
- Contratto dell'esito (`Done/Unsupported/Rejected/Failed/Unconfirmed` +
  `Confirmed/Unconfirmed`) e resa unica del testo per il modello.
- Matrice delle capacità per DAW; l'agente sa cosa non esiste dove.
- Adattatore REAPER corretto su indirizzi, unità e curva del fader (misurata).
- Riconoscimento della DAW dall'host (`juce::PluginHostType`).
- `daw.track.create` (REAPER `/action/40001`, Ableton `create_midi_track`).
- Test: 3435 controlli, 0 falliti (`src/daw/tests`, `src/ai/tests`,
  `src/osc/tests`).
- Documentazione operativa: `TEST-TOOL-CALL.md`.
