# WhyCremisi VST Bridge AI - Stato Progetto

**Ultimo aggiornamento:** 2026-05-09 — Claude (sessione setup macOS + bug fixes)

---

## ✅ Completato — Sessione 09/05 (Claude — macOS setup + bug fixes)

| Fix | File | Dettaglio |
|-----|------|-----------|
| Build macOS JUCE 8 | `CMakeLists.txt` | Aggiunto path `/Users/whyed/` — build [100%] pulita su Monterey + AppleClang 14 |
| Anthropic headers | `src/ai/AiEngine.cpp` | Era `Authorization: Bearer` → ora `x-api-key` + `anthropic-version: 2023-06-01` come da spec API |
| JSON parser | `src/ai/AiEngine.cpp` | Rimosso parser manuale fragile, tutti i provider usano nlohmann (già in third_party) |
| Deduplicazione AI | `src/ai/AiEngine.cpp` | OpenAI/OpenRouter/Groq unificati in `callOpenAICompatible()` |
| OSC type tag bug | `src/osc/OscHandler.cpp` | Null terminator del type tag non veniva saltato se già allineato → args puntava al null invece dei dati payload |
| Race condition OSC | `src/bridge/OscBridge.cpp` | `isRunning()` chiamato prima che il thread OSC completasse il bind — aggiunto sleep(50ms) |
| npm install | `webview-ui/` | Dipendenze React installate localmente |

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
| 1 | Rimuovere log di debug da `/tmp/whycremisi-debug.log` | Media |
| 2 | Fix parsing OSC in OscHandler.cpp (pointer arithmetic linee 122-136) | Alta |
| 3 | Mappare tutti i parametri OSC Reaper (volume, pan, mute, solo, tempo, position) | Alta |
| 4 | Verificare OSC Reaper → Plugin (feedback bidirezionale) | Alta |
| 5 | Preferences configurabili (IP/porte OSC e WebSocket senza editare codice) | Media |
| 6 | Documentazione parametri OSC/WS per Edo | Media |
| 7 | gain1/gain2 non collegati | Bassa |
| 8 | WebSocket reconnect logic cleanup | Bassa |
| 9 | Integrare Mastering Rack knobs (da prototipo di Edo) | Media |
| 10 | Vector Scope con dati audio reali | Bassa |
| 11 | Supporto OSC Ableton | Media |

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