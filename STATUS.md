# WhyCremisi VST Bridge AI - Stato Progetto

**Ultimo aggiornamento:** 2026-05-11 — Claude (sessione OSC fix + review work-in-progress)

---

## ✅ Completato — Sessione 11/05 (Claude — OSC fix + review WIP)

| Fix | File | Dettaglio |
|-----|------|-----------|
| OSC parsing double null skip | `src/osc/OscHandler.cpp` | Rimosso `ptr++` erroneo dopo padding (linea 133) — causava misalignment e type tag saltato |
| Verified build [100%] | `CMakeLists.txt` | VST3 + AU + Standalone compilano con nuovi moduli (MidiHandler, ParameterMapper, PluginChain, DSPEngine) |
| Nuovi moduli C++ | `src/core/`, `src/midi/`, `src/dsp/` | MidiHandler, ParameterMapper, PluginChain, DSPEngine (Analyzer, Compressor, Limiter, EQBand) |
| Refactor OscBridge | `src/bridge/OscBridge.cpp` | `sendConfigResponse()` per risposte configurabili, dispatch MIDI learn, chain management, AI action execution |
| 8 gain parameters | `src/core/PluginProcessor.cpp` | Da 2 a 8 parametri gain, midi learn routing, DSP processing, analyzer data push |
| MIDI Learn UI | `webview-ui/src/App.jsx` | Pulsante LEARN sui widget, pannello Parameter Mapping, pannello Plugin Chain |
| AI Action log | `webview-ui/src/App.jsx` | Undo/redo azioni AI, action log timeline |
| Bridge API | `webview-ui/src/whycremisi-bridge.js` | `midiLearnStart()`, `midiLearnStop()`, `getPluginChain()`, `setPluginChain()` |
| AiEngine refactor | `src/ai/AiEngine.cpp/h` | Action callbacks, widget context, provider deduplicazione estesa |

**Build verificata:** VST3 + AU + Standalone compilati senza errori su macOS, JUCE 8.0.12.

**Build verificata:** VST3 + Standalone compilati senza errori su macOS 12.6.3, JUCE 8.0.12.

---

## ✅ Completato (Sessione 2 — 14/04 pomeriggio)

### Bug fixes critici
| Fix | Dettaglio |
|-----|-----------|
| OscHandler duplicato | Rimosso da PluginProcessor, solo OscBridge gestisce OSC |
| SHA1 handshake WebSocket | Endianness e padding corretti |
| Key extraction WebSocket | `substring(16)` invece di `indexOfChar(':')` |
| isRunning() OscHandler | Ora controlla `connected` oltre a `running` |
| OSC addresses Reaper | `/play`, `/stop`, `/record` (formato breve) |
| DAW target IP | `192.168.1.12:8000` (IP locale Reaper) |

### Integrazione UI WhyCremisi
| Componente | File | Stato |
|------------|------|-------|
| BotFace avatar animato | `webview-ui/src/components/BotFace.jsx` | ✅ Copiato da prototipo |
| Design system WhyCremisi | `webview-ui/src/index.css` | ✅ Tailwind + custom CSS |
| App principale | `webview-ui/src/App.jsx` | ✅ Refactoring completo |
| Toolbox con transport | `webview-ui/src/components/Toolbox.jsx` | ✅ Bridge WebSocket |
| Bridge compatibilità | `webview-ui/src/whycremisi-bridge.js` | ✅ +`window.receiveFromPlugin`, `__whycremisiBridge` |
| Dipendenze | `package.json` | ✅ framer-motion, tailwindcss |
| Vite config | `vite.config.js` | ✅ +tailwindcss plugin |
| HTML | `index.html` | ✅ Space Grotesk, Material Symbols |
| Documentazione team | `Documentazione/09-GUIDA-SVILUPPO.md` | ✅ Nuova guida |

### Build
- ✅ Frontend production build (Vite + Tailwind) — successo
- ✅ VST3 build C++ — successo
- ✅ Standalone build C++ — successo
- ✅ VST3 installato in `~/.vst3/`

---

## 🔧 Cosa funziona

- WebSocket handshake — browser si connette e rimane connesso
- OSC Plugin → Reaper: Play, Stop, Rec funzionano (`/play`, `/stop`, `/record`)
- OSC ricezione: il plugin ascolta su porta 9000, invia a 192.168.1.12:8000
- UI WhyCremisi: Header, Side Module, AI Chat Console con BotFace, Toolbox, Vector Scope placeholder, Footer
- Bridge: `whycremisi.sendDAWCommand()`, `whycremisi.sendAIPrompt()`, auto-reconnect, request/response con timeout
- Compatibilità C++ WebView: `window.receiveFromPlugin()` e `window.__whycremisiBridge`

---

## ⚠️ Da fare

| # | Task | Priorità |
|---|------|----------|
| 1 | Mappare tutti i parametri OSC Reaper (volume, pan, mute, solo, tempo, position) | Alta |
| 2 | Verificare OSC Reaper → Plugin (feedback bidirezionale) | Alta |
| 3 | Preferences configurabili (IP/porte OSC e WebSocket senza editare codice) | Media |
| 4 | Documentazione parametri OSC/WS per Edo | Media |
| 5 | WebSocket reconnect logic cleanup | Bassa |
| 6 | Vector Scope con dati audio reali (ora riceve dall'analyzer!) | Bassa |
| 7 | Supporto OSC Ableton | Media |
| 8 | `getCurrentPosition` deprecato → usare `getPosition` in PluginProcessor | Bassa |

---

## 📊 Impostazioni OSC Reaper

- Device port: 9000 (invio)
- Device IP: 127.0.0.1
- Local listen port: 8000 (ricezione)
- Local IP: 192.168.1.12 (non modificabile)

---

## 📁 Struttura Frontend Produzione

```
webview-ui/
├── src/
│   ├── App.jsx               # UI WhyCremisi completa
│   ├── whycremisi-bridge.js     # Bridge WebSocket RFC 6455
│   ├── index.css              # Tailwind + stili WhyCremisi
│   ├── main.jsx
│   └── components/
│       ├── BotFace.jsx        # Avatar SVG morphing animato
│       ├── Toolbox.jsx        # Widget system + transport
│       ├── WidgetSlider.jsx
│       ├── WidgetKnob.jsx
│       ├── WidgetButton.jsx
│       └── index.js
├── index.html
├── package.json               # +framer-motion, +tailwindcss
└── vite.config.js             # +@tailwindcss/vite plugin
```

---

*Ultimo aggiornamento: 2026-04-14 — Aura*