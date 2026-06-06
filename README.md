<h1 align="center">
  <img src="assets/logo.png" height="64" alt="WhyCremisi" valign="middle"/>
  &nbsp;WhyCremisi
</h1>

<p align="center">
  <img src="https://img.shields.io/badge/JUCE-7-FF0000?style=flat-square" />
  <img src="https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat-square&logo=cplusplus&logoColor=white" />
  <img src="https://img.shields.io/badge/React-WebView_UI-61DAFB?style=flat-square&logo=react&logoColor=black" />
  <img src="https://img.shields.io/badge/VST3-AU-7B00D4?style=flat-square" />
  <img src="https://img.shields.io/badge/status-active-brightgreen?style=flat-square" />
</p>

<br/>

> Un layer di intelligenza che si installa sul master channel e da lì controlla l'intera sessione: il DAW, il transport, e tutti gli altri plugin caricati. Un co-pilota AI per la produzione musicale.

---

<p align="center">
  <img src="assets/screenshot.png" alt="WhyCremisi in Ableton" width="100%"/>
</p>

---

## Cosa fa

**WhyCremisi non è un semplice plugin** — è un universal parameter bridge. Espone automaticamente tutti i parametri di ogni plugin caricato nel DAW e li rende accessibili all'AI, senza dover conoscere il plugin in anticipo.

**Tre fasi di intelligenza:**

| Fase | Descrizione | Stato |
|------|-------------|-------|
| **Universal bridge** | Legge e scrive qualsiasi parametro VST per indice | ✅ Attivo |
| **Plugin dictionary** | Mappa semantica dei 10 plugin più usati al mondo | 🔧 In sviluppo |
| **Auto-discovery** | AI interpreta parametri di plugin sconosciuti | 🔮 Roadmap |

---

## Features

- **VST3 + AU + Standalone** — build completa per Ableton, REAPER e DAW compatibili
- **OSC Bridge** — comunicazione bidirezionale con il DAW in tempo reale
- **Flight Recorder** — log degli ultimi N eventi di sessione come contesto AI
- **React WebView UI** — interfaccia moderna con mascotte BotFace (9 stati)
- **Multi-provider AI** — Groq, Gemini, Anthropic, OpenAI, OpenRouter, Ollama
- **14 test automatizzati** — CI-ready, build stabile

---

## Build (macOS)

```bash
git clone https://github.com/OfficialWhyEd/WhyCremisi
cd WhyCremisi

# Prerequisiti: CMake, Xcode Command Line Tools, JUCE 7
cmake -B build -DJUCE_ROOT=/path/to/JUCE
cmake --build build --config Release

# Installa
cp -r build/WhyCremisi_artefacts/VST3/WhyCremisi.vst3 ~/Library/Audio/Plug-Ins/VST3/
```

---

## Struttura

```
WhyCremisi/
├── src/
│   ├── core/          # PluginProcessor — universal parameter bridge
│   ├── bridge/        # WebSocketServer — OSC + WebView IPC
│   └── core/tests/    # 14 test automatizzati
├── webview-ui/        # React UI — BotFace, SessionPanel, Widget system
└── Research/          # Design, loghi, documentazione visiva
```

---

## Plugin Dictionary (roadmap)

I 10 plugin target che coprono il 90% delle sessioni reali:

`Serum` · `Vital` · `Massive X` · `FabFilter Pro-Q3` · `Pro-C2` · `OTT` · `Valhalla VintageVerb` · `Valhalla Delay` · `Waves SSL` · `API 2500`

---

<p align="center">Built by <a href="https://github.com/OfficialWhyEd">@whyed</a> · macOS · JUCE 7 · MIT License</p>
