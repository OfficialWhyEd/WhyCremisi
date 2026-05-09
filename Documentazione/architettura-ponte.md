# Architettura Ponte - WhyCremisi VST Bridge AI

**Data creazione:** 2026-04-12  
**Ultimo aggiornamento:** 2026-04-14  
**Stato:** ✅ Implementata

---

## Problema Identificato

WebView (WebKit/GTK) nel plugin VST **crasha** su Linux.

**Evidenzie:**
- Forum JUCE: "WebBrowserComponent very poor linux experience"
- GitHub JUCE: Issue #1557 GUI crashes on Ubuntu 24.04
- Test reale: Reaper crasha all'apertura del plugin con WebView

**Soluzione:** Architettura "Ponte OSC-Web" — UI fuori dal plugin.

---

## Architettura Finale

```
┌─────────────────────────────────────────────────────────────────────────┐
│                              PLUGIN VST (JUCE)                          │
│                                                                         │
│   ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────────┐  │
│   │   OscHandler    │───►│   OscBridge     │───►│  WebSocketServer   │  │
│   │   (UDP :9000)   │    │   (Dispatcher)  │    │    (TCP :8080)     │  │
│   │   RX/TX OSC     │◄───│   JSON↔OSC      │    │   RFC 6455         │  │
│   └─────────────────┘    └─────────────────┘    └──────────┬──────────┘  │
│            ▲                                                │            │
│            │                                                ▼            │
│   ┌────────┴─────────┐                           (Browser connections)  │
│   │   DAW (Reaper/   │                                                          │
│   │   Ableton)       │    UDP 9000 ◄──────► UDP 9001                          │
│   └──────────────────┘                                                          │
│                                                                         │
│   Ports:                                                                     │
│   - OSC RX (from DAW): 9000                                                  │
│   - OSC TX (to DAW):  9001                                                  │
│   - WebSocket (UI):   8080                                                   │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    │ WebSocket (ws://localhost:8080)
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                         REACT UI (Browser)                               │
│                                                                         │
│   ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────────┐  │
│   │  whycremisi-bridge │───►│   React App     │───►│   Widgets           │  │
│   │  (WebSocket UI)  │◄───│   (State)       │◄───│   (Sliders/Knobs)  │  │
│   └─────────────────┘    └─────────────────┘    └─────────────────────┘  │
│                                                                         │
│   Dev:  http://localhost:5173                                            │
│   Prod: Built bundle served by plugin or standalone                      │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Componenti Implementati

### 1. WebSocketServer (`src/bridge/WebSocketServer.h/cpp`)

Server WebSocket RFC 6455 integrato nel plugin.

| Proprietà | Valore |
|-----------|--------|
| Protocollo | RFC 6455 (WebSocket) |
| Porta default | 8080 |
| Connessioni | Multiple (broadcast a tutti i client) |
| Frame supportati | Text, Close, Ping, Pong |

**Funzionalità:**
- Accetta connessioni multiple
- Broadcast JSON a tutti i client connessi
- Gestisce handshake, ping/pong keepalive
- Frame text con payload JSON

### 2. OscBridge (`src/bridge/OscBridge.h/cpp`)

Dispatcher bidirezionale tra OSC e WebSocket.

| Direzione | Flusso |
|-----------|--------|
| DAW → UI | OSC (UDP :9000) → OscHandler → OscBridge → WebSocket → React |
| UI → DAW | React → WebSocket → OscBridge → OscHandler → OSC (UDP :9001) |

**Traduzioni implementate:**
| OSC Address | JSON Type | Note |
|-------------|-----------|------|
| `/transport/play` | `daw.transport` | isPlaying |
| `/transport/stop` | `daw.transport` | isPlaying |
| `/transport/record` | `daw.transport` | isRecording |
| `/transport/tempo` | `daw.transport` | bpm |
| `/track/{id}/volume` | `osc.message` | forwarded raw |
| `/track/{id}/pan` | `osc.message` | forwarded raw |

### 3. OscHandler (`src/osc/OscHandler.h/cpp`)

Handler OSC UDP esistente — mantenuto per compatibilità MIDI/parametri.

| Proprietà | Valore |
|-----------|--------|
| Porta RX | 9000 (default) |
| Porta TX | 9001 (default, configurabile) |
| Protocollo | OSC binary (RFC) |

---

## Stack Comunicazione

```
DAW ──UDP/OSC──► Plugin ──Internal──► WebSocketServer ──TCP/WS──► Browser
                      │
                      └──► whycremisi-bridge.js (client WebSocket)
```

**Nota:** Il browser non può ricevere UDP OSC direttamente. Il WebSocketServer fa da ponte TCP.

---

## Protocollo JSON v1.0

Vedi `protocol-json-v1.md` per la specifica completa.

### Messaggi Plugin → UI (via WebSocket)

```json
{"type": "daw.transport", "payload": {"isPlaying": true, "bpm": 120.0, "positionSeconds": 45.5}}
{"type": "daw.track", "payload": {"trackId": 1, "name": "Lead Vocal", "volumeDb": -3.0}}
{"type": "daw.meter", "payload": {"trackId": 1, "leftDb": -12.5, "rightDb": -11.8}}
{"type": "osc.message", "payload": {"address": "/track/1/volume", "value": 0.75}}
{"type": "ai.response", "payload": {"content": "Suggerimento EQ..."}}
{"type": "plugin.error", "payload": {"code": "ERR_CODE", "message": "...", "severity": "error"}}
```

### Messaggi UI → Plugin (via WebSocket)

```json
{"type": "plugin.init", "payload": {"version": "1.0.0", "capabilities": ["widgets", "ai", "osc"]}}
{"type": "daw.command", "payload": {"command": "play"}}
{"type": "daw.request", "payload": {"request": "transport"}}
{"type": "ai.prompt", "payload": {"prompt": "Analizza traccia 1"}}
{"type": "widget.valueChange", "payload": {"widgetId": "eq-high", "value": 3.5}}
{"type": "osc.send", "payload": {"address": "/track/1/volume", "value": 0.8}}
```

---

## Vantaggi vs WebView Integrata

| Aspetto | WebView integrata | Ponte OSC-Web |
|---------|-------------------|---------------|
| Stabilità | ❌ Crash su Linux | ✅ Stabile (UI fuori processo) |
| Libertà UI | Limitata (WebKit) | Totale (any browser, any framework) |
| Debug | Difficile | Facile (DevTools, React DevTools) |
| Latenza | Bassa | ~1-5ms (locale, accettabile per controllo) |
| Professionale | Sì | Sì (pattern usato da synth come Vital, Serum) |
| Multi-client | No | ✅ Sì (broadcast a tutti i browser) |

---

## File Nuovi (14/04/2026)

```
src/bridge/
├── WebSocketServer.h    # Header
├── WebSocketServer.cpp   # Implementazione RFC 6455
├── OscBridge.h          # Header
└── OscBridge.cpp        # Implementazione dispatcher OSC↔WS
```

---

## Build e Test

### Build
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

Il plugin sarà in:
- VST3: `build/WhyCremisiVSTPlugin_artefacts/Release/VST3/WhyCremisiVSTBridgeAI.vst3`
- Standalone: `build/WhyCremisiVSTPlugin_artefacts/Release/Standalone/WhyCremisiVSTBridgeAI`

### Test Rapido

1. Avvia Reaper con plugin caricato
2. Apri browser e connettiti a `ws://localhost:8080` (o usa `whycremisi-bridge.js`)
3. Invia `{"type": "plugin.init", ...}` — il plugin risponderà con `plugin.init`

---

## Prossimi Step

| # | Task | Stato | Note |
|---|------|-------|------|
| 1 | WebSocket server + OscBridge | ✅ Fatto | Questo commit |
| 2 | Aggiornare whycremisi-bridge.js per WebSocket | ⏳ Da fare | Heartbroken |
| 3 | UI React base connessa | ⏳ Da fare | Heartbroken |
| 4 | Test integrazione Reaper ↔ browser | ⏳ Da fare | Carlo |
| 5 | AI integration (Ollama) | ⏳ Da fare | AiEngine |

---

## Riferimenti

- **tomduncalf/WebUISynth** (45⭐): github.com/tomduncalf/WebUISynth
- **JanWilczek/juce-webview-tutorial** (50⭐): github.com/JanWilczek/juce-webview-tutorial
- **JUCE OSC Tutorial**: docs.juce.com/master/tutorial_osc_sender_receiver.html
- **RFC 6455 WebSocket**: tools.ietf.org/html/rfc6455

---

*Documento aggiornato il 2026-04-14 con implementazione completata.*
