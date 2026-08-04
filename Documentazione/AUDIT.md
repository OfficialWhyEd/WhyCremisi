# Audit completo — WhyCremisi

**Data:** 4 agosto 2026
**Metodo:** lettura delle 16 specifiche in `Research/`, confronto riga per riga con i 14.423 righe di `src/`, verifica di ogni comando e di ogni strumento AI fino al punto in cui arriva davvero.

Questo documento esiste perché il progetto ha una specifica completa e ambiziosa, e un'implementazione che in alcuni punti chiave **dichiara di fare cose che non fa**. Serve a sapere, senza illusioni, dove si è.

---

## 1. Il difetto strutturale: 12 strumenti AI su 17 non fanno niente

È il risultato più importante di questo audit.

L'esecutore degli strumenti in `OscBridge.cpp` definisce una funzione interna:

```cpp
auto sendDawCmd = [&](const std::string& action, const nlohmann::json& params = {}) {
    nlohmann::json msg;
    msg["type"] = "daw.command";
    msg["payload"]["command"] = action;
    ...
    wsServer->broadcast(msg);      // ← verso i client WebSocket
};
```

`wsServer->broadcast` manda il messaggio **ai client collegati**, cioè alla UI React. Ma la UI ascolta 22 tipi di messaggio e `daw.command` **non è fra questi**: la UI quel tipo lo *invia* soltanto, non lo riceve mai.

Il gestore vero esiste e funziona — `dispatchDawCommand()`, che traduce i comandi in OSC verso la DAW — ed è quello che serve la UI quando l'utente preme un pulsante. L'esecutore degli strumenti AI semplicemente **non lo chiama**.

Ogni strumento poi imposta `result.success = true` e restituisce una frase di conferma.

| Strumento | Stato | Cosa risponde all'AI |
|---|---|---|
| `daw.transport.play` | **morto** | "Playback started" |
| `daw.transport.stop` | **morto** | "Playback stopped" |
| `daw.transport.record` | **morto** | "Recording started" |
| `daw.transport.setTempo` | **morto** | "Tempo set to N" |
| `daw.track.setVolume` | **morto** | "Volume set" |
| `daw.track.setPan` | **morto** | "Pan set" |
| `daw.track.mute` | **morto** | "Track muted" |
| `daw.track.solo` | **morto** | "Track soloed" |
| `daw.plugin.setParam` | **morto** | "Pro-Q3 Band2_Gain set to -3.5" |
| `daw.plugin.bypass` | **morto** | "Plugin bypassed" |
| `daw.marker.goto` | **morto** | "Navigated to marker" |
| `daw.marker.set` | **morto** | "Marker set" |
| `dsp.compressor.set` | vivo | agisce sul DSP interno |
| `dsp.limiter.set` | vivo | agisce sul DSP interno |
| `dsp.bypass` | vivo | agisce sul DSP interno |
| `dsp.getState` | vivo | legge il DSP interno |
| `mix.analyze` | vivo | misure vere dall'analizzatore |

**Conseguenza.** Il controllo della DAW da parte dell'AI — che è *la* promessa del prodotto — è al momento inesistente. L'AI non può premere play. E siccome ogni chiamata risponde "fatto", il modello costruisce il ragionamento successivo su un presupposto falso: dice all'utente di aver fatto una cosa che non è avvenuta.

**Riparazione:** far chiamare a `sendDawCmd` direttamente `dispatchDawCommand(payload)`. È un intervento piccolo su un difetto grande. Va accompagnato dalla verifica che il comando sia poi effettivamente accettato dalla DAW, invece di dichiarare successo a prescindere.

---

## 2. Controllo dei plugin di terze parti: non esiste

Il Paper 03 è marcato ★★★★★ e s'intitola *"Ogni manopola di ogni plugin, accessibile dall'AI"*. Prevede scoperta dei parametri, naming semantico, scalatura, validazione, undo, e un database di 100+ plugin con preset AI.

Nel codice:

| Previsto | Presente |
|---|---|
| `ParameterMapper` con discovery / naming / scaling / validation / undo | 96 righe che mappano i widget della **propria** UI. Nessuna delle cinque funzioni |
| Hosting VST3 di plugin esterni | **Zero.** Nessun `AudioPluginFormatManager`, `KnownPluginList`, `AudioPluginInstance` in tutto `src/` |
| `plugin-database.json`, 100+ plugin | **Non esiste.** Solo lo schema nel Paper 13 |
| `PluginChain` | 117 righe: una lista di nomi e slot, senza alcun plugin dietro |

**Nota sui due piani che non concordano.** Il Paper 06 mette il controllo dei plugin nella **Fase Beta**, subito dopo l'Alpha. La `ROADMAP.md` mette *"Plugin hosting (load VST3 within WhyCremisi)"* allo **step 97 su 100**, in fondo alla Fase 7. È lo stesso lavoro collocato all'inizio in un piano e alla fine nell'altro. Va deciso quale vale.

---

## 3. Memoria: due livelli su cinque, e nel posto sbagliato

Il Paper 04 (*"Non si azzera. Ricorda. Cresce."*) prevede cinque livelli e un ciclo di apprendimento.

| Livello | Previsto | Presente |
|---|---|---|
| L4 Identità | permanente, immutabile | `AgentIdentity`, 48 righe — c'è |
| L3 Personalità | evolve lentamente | `PersonalityCore`, 221 righe — c'è, ma non evolve da sola |
| L2 **Profilo utente** | generi, range BPM, **plugin preferiti per ruolo**, target LUFS, **tassi di accettazione** | `AgentUser`, 46 righe: nome, pronomi, fuso orario, note. **Niente del resto** |
| L1 Sessione | azioni e risultati del giorno | `SessionManager` → `memory.json` — conta prompt ed eventi, non distilla nulla |
| L0 Contesto real-time | BPM, meter, plugin attivi | c'è e funziona |

**Ciclo di apprendimento** (accetti un EQ → sale il tasso; rifiuti → cala la profondità): **assente**.

**E c'è un problema di collocazione.** Il Paper 04 dice che tutto sta in `~/.whycremisi/`. Nel codice `AgentWorkspace` viene salvato dentro `getStateInformation()`, cioè **dentro il file di progetto della DAW**. Significa che la memoria è per-progetto: apri un progetto nuovo e l'agente non ricorda niente. È l'opposto di "ricorda ogni sessione".

---

## 4. Altri comandi che finiscono nel vuoto

Oltre ai 12 strumenti AI:

| Comando | Origine | Problema |
|---|---|---|
| `setParameter` | `WidgetKnob.jsx`, `WidgetSlider.jsx` | La UI lo manda come `daw.command`; il plugin ascolta `widget.valueChange`. **Il sistema widget è scollegato da entrambi i lati:** giri una manopola e non arriva niente |
| `softClip` | `ClippingBox.jsx` | Nessun gestore |
| `hardClip` | `ClippingBox.jsx` | Nessun gestore |
| `midSide`, `phaseInvert`, `mono`, `narrow`, `widen` | `VectorscopeBox.jsx` | Mandavano OSC a `/whycremisi/*`, indirizzi inventati che nessuna DAW ascolta — **in riparazione**, vedi §7 |

---

## 5. Dati non misurati ancora presenti

Il grosso è stato ripulito il 4 agosto (vedi `git log`), ma resta:

- **`App.jsx:1939`** — la striscia dello spettro, quando `spectrum` è vuoto, disegna
  `0.1 + Math.sin(i * 0.2 + Date.now() * 0.0005) * 0.05`: un'onda animata finta che sembra audio. È lo stesso difetto che il `TODO.md` dava per chiuso il 01/08 — era stato corretto nei pannelli, non qui.

La regola che ne discende, e che vale per tutto il progetto:
**se un numero non viene da una misura, o un pulsante da un'azione reale, non deve esistere.** Un pannello vuoto è informazione; un pannello che inventa è un danno.

---

## 6. Cosa invece funziona davvero

Va detto, perché è parecchio e la base è solida:

- **DSP**: analizzatore FFT completo — LUFS momentary/short/integrated, true peak, correlazione, RMS, loudness range, centroide, rolloff, energia su 10 bande logaritmiche, BPM, tonalità con relativa, nuvola di punti del vectorscope. EQ, compressore e limiter funzionanti, e da agosto raggiungibili da UI e AI.
- **Motore AI**: sei provider (Claude, OpenAI, Gemini, OpenRouter, Groq, Ollama), streaming, function calling, gestione contesto, consumo token visibile. Dal 4 agosto ogni fallimento riporta un motivo leggibile.
- **Bridge**: WebSocket RFC 6455 scritto a mano con handshake e riconnessione; OSC verso Reaper verificato; dialetto AbletonOSC implementato.
- **UI React**: 13 pannelli modulari, BotFace, SessionPanel, temi, export/import sessione. Vectorscope e stereoscope su dati veri.
- **Build**: VST3 + Standalone su Windows, VST3 + AU + Standalone su macOS. 2094 test automatici, 0 falliti.

---

## 7. Lavoro in corso non ancora concluso

- **Stadio stereo** (`src/dsp/StereoStage.{h,cpp}`): scritto, inserito nella catena e nel CMake, i comandi sono cablati al DSP. **Non ancora compilato né provato.** Manca il collegamento dello stato alla UI (`stereoState` in `App.jsx` e la prop verso `VectorscopeBox`).

---

## 8. Storia del progetto, e la questione macOS

215 commit dal 9 aprile 2026. Aprile 93, maggio 58, giugno 35, agosto 29.

Il progetto è nato su Windows, è proseguito su **macOS da maggio a giugno** — ed è lì che la Fase Alpha è stata completata e provata — poi è tornato su Windows a fine luglio, con il team ridotto a una persona.

**Nulla del lavoro su Mac è andato perso.** Verificato: `heartbroken-claude`, `heartbroken` e `aura` hanno zero commit non presenti in `master`. Resta fuori un solo commit del 15 aprile in `archivio/heartbroken-aprile-2026` ("Update WebView bridge and dependencies"), superato dal lavoro successivo.

La sensazione che "su Mac ci fosse di più" è fondata, ma non perché manchi codice: è che **su macOS l'Alpha era completa e collaudata**, mentre su Windows fino al 1º agosto il plugin non compilava nemmeno, e la verifica dentro un DAW reale su Windows non è ancora avvenuta.

---

## 9. Stato rispetto alla roadmap dei 100 step

`ROADMAP.md`, 7 fasi. Dichiarati completi: 1–49, 52, 54, 55, 57, 58, 60, 63–65, 75–77. Aperti: 50, 51, 53, 56, 59, 61, 62, 66–74, 78–100.

Il blocco **78–100** è quello che pesa: test React, end-to-end, CI, documentazione API e l'intera Fase 7.

Va però notato che diversi step marcati completi lo sono solo a livello di codice scritto, non di funzione raggiungibile — gli strumenti AI del §1 ne sono l'esempio: gli step ci sono, il codice c'è, il comando non arriva.

---

## 10. Ordine di priorità che ne deriva

1. **Far arrivare i comandi AI alla DAW** (§1). Intervento piccolo, sblocca la promessa centrale. Senza questo tutto il resto è cosmetica.
2. **Rendere onesto ogni esito**: nessun `success = true` senza una conferma reale. Vale per gli strumenti, per i comandi e per i pannelli.
3. **Collegare il sistema widget** (§4): allineare `setParameter` e `widget.valueChange`.
4. **Spostare la memoria fuori dal file di progetto** (§3), in `%APPDATA%`, e ampliare il profilo utente ai campi della specifica.
5. **Decidere quando fare il controllo dei plugin esterni** (§2) — Fase Beta come dice il Paper 06, o step 97 come dice la ROADMAP — e allineare i due piani.
6. **Provare il plugin dentro un DAW su Windows**, che non è mai stato fatto.
