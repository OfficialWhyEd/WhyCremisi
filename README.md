<h1 align="center">
  <img src="assets/logo.png" height="60" alt="WhyCremisi" valign="middle"/>
  &nbsp;WhyCremisi
</h1>

<p align="center">
  <strong>The AI that lives inside your DAW and controls every knob of every plugin — without knowing them in advance.</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/JUCE-7-FF0000?style=flat-square" />
  <img src="https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat-square&logo=cplusplus&logoColor=white" />
  <img src="https://img.shields.io/badge/React-WebView_UI-61DAFB?style=flat-square&logo=react&logoColor=black" />
  <img src="https://img.shields.io/badge/VST3-AU-Standalone-7B00D4?style=flat-square" />
  <img src="https://img.shields.io/badge/AI-Groq_%7C_Gemini_%7C_Claude_%7C_Ollama-D97706?style=flat-square" />
  <img src="https://img.shields.io/badge/tests-14_passing-brightgreen?style=flat-square" />
  <img src="https://img.shields.io/badge/macOS-Monterey+-000000?style=flat-square&logo=apple&logoColor=white" />
</p>

<br/>

<p align="center">
  <img src="assets/banner.png" width="100%" alt="WhyCremisi"/>
</p>

<br/>

> **WhyCremisi installs on your master channel.** From there, it scans every parameter of every plugin loaded in the session — Serum, FabFilter, Valhalla, anything — and exposes them all to an AI that can read, write and automate them in real time. No preset mapping. No plugin SDK. No prior knowledge of the plugin. Just index-based VST3 parameter scanning, and an AI that learns what each knob does on the fly.

---

<p align="center">
  <img src="assets/screenshot.png" alt="WhyCremisi inside the DAW" width="100%"/>
</p>

---

## Why this is different

Every other "AI for music production" tool is either a standalone app disconnected from your session, or a chatbot that generates MIDI. WhyCremisi is a **plugin** — it loads in the same DAW process as your other plugins, sits on the master channel, and has native access to the VST3 parameter graph of the entire session.

| | WhyCremisi | Standalone AI tools | Manual workflow |
|---|---|---|---|
| Lives inside the DAW | ✅ VST3/AU plugin | ❌ External app | — |
| Accesses any plugin's parameters | ✅ Universal bridge | ❌ Hardcoded presets | ❌ One by one |
| No preset mapping needed | ✅ Auto-discovery | ❌ Plugin-specific | — |
| React UI inside the plugin | ✅ WebView | ❌ | — |
| Knows the session history | ✅ Flight Recorder | ❌ | ❌ |
| Works with any DAW | ✅ JUCE/VST3/AU | ⚠️ Limited | ✅ |
| Run offline | ✅ Ollama support | ❌ | — |

---

## How it works

WhyCremisi doesn't need to know what a plugin is. VST3 exposes every parameter as a numbered index. WhyCremisi reads them all.

```
┌─────────────────────────────────────────────────────┐
│                   Your DAW session                  │
│                                                     │
│  [Serum]  [FabFilter Q3]  [Valhalla]  [OTT]  ...  │
│     │           │              │         │          │
│     └───────────┴──────────────┴─────────┘          │
│                         │                           │
│              VST3 parameter graph                   │
│                         │                           │
│              ┌──────────▼──────────┐                │
│              │    WhyCremisi       │  ← master ch.  │
│              │  Universal Bridge   │                │
│              │  Flight Recorder    │                │
│              │  React WebView UI   │                │
│              └──────────┬──────────┘                │
└─────────────────────────┼───────────────────────────┘
                          │
              ┌───────────▼───────────┐
              │   AI Provider         │
              │  Groq · Gemini        │
              │  Claude · OpenAI      │
              │  OpenRouter · Ollama  │
              └───────────────────────┘
```

**The Flight Recorder** logs every event in the session — parameter changes, transport events, plugin state — and feeds them as context to the AI. The AI doesn't just see the current state: it sees what you've been doing for the last N minutes.

---

## Three phases of intelligence

| Phase | What it does | Status |
|-------|-------------|--------|
| **① Universal Bridge** | Scans and maps all VST3 parameters by index across every loaded plugin. Read + write in real time. | ✅ Live |
| **② Plugin Dictionary** | Semantic layer that maps index numbers to human-readable names for the 10 most-used plugins in the world. "Filter cutoff" instead of "param_0047". | 🔧 Building |
| **③ Auto-Discovery** | AI infers what unknown parameters do by observing their effect on audio. Learns any plugin without a dictionary entry. | 🔮 Roadmap |

Phase ① alone is already useful. You can ask the AI to move a parameter and it will find it. Phases ② and ③ make it fluent.

---

## Features

- **Universal parameter bridge** — reads and writes any VST3 parameter by index, across all plugins simultaneously
- **Flight Recorder** — circular buffer of session events, injected as context into every AI prompt
- **BotFace mascot** — animated SVG mascot with 9 emotional states, lives inside the plugin UI
- **React WebView UI** — full React app rendered inside a JUCE WebView, hot-reload during development
- **OSC bridge** — bidirectional real-time communication with the DAW
- **Multi-provider AI** — swap between Groq, Gemini, Anthropic, OpenAI, OpenRouter, or local Ollama with one config change
- **14 automated tests** — CI-ready, build is stable
- **VST3 + AU + Standalone** — one codebase, three build targets

---

## Plugin Dictionary target list

The 10 plugins that cover ~90% of real-world sessions:

| Plugin | Category | Status |
|--------|----------|--------|
| **Serum** | Synthesizer | 🔧 Mapping |
| **Vital** | Synthesizer | 🔧 Mapping |
| **Massive X** | Synthesizer | 📋 Queued |
| **FabFilter Pro-Q3** | EQ | 🔧 Mapping |
| **FabFilter Pro-C2** | Compressor | 📋 Queued |
| **OTT** | Multiband | ✅ Simple |
| **Valhalla VintageVerb** | Reverb | 📋 Queued |
| **Valhalla Delay** | Delay | 📋 Queued |
| **Waves SSL E-Channel** | Channel strip | 📋 Queued |
| **API 2500** | Bus compressor | 📋 Queued |

---

## Quick start

**Prerequisites:** CMake ≥ 3.22 · Xcode Command Line Tools · [JUCE 7](https://juce.com/get-juce/)

```bash
git clone https://github.com/OfficialWhyEd/WhyCremisi
cd WhyCremisi

# Build
cmake -B build -DJUCE_ROOT=/path/to/JUCE
cmake --build build --config Release

# Install VST3
cp -r build/WhyCremisi_artefacts/VST3/WhyCremisi.vst3 \
      ~/Library/Audio/Plug-Ins/VST3/

# Configure AI provider
cp config.example.json config.json
# → set your preferred provider + key in config.json
```

Load WhyCremisi on the **master channel** in your DAW. Open the plugin UI. The bridge starts automatically.

---

## Project structure

```
WhyCremisi/
├── src/
│   ├── core/
│   │   ├── PluginProcessor.cpp    # VST3 host + universal parameter scanner
│   │   ├── PluginEditor.cpp       # JUCE WebView host
│   │   └── tests/                 # 14 automated tests
│   └── bridge/
│       └── WebSocketServer.cpp    # OSC + WebView IPC layer
├── webview-ui/                    # React app (UI, BotFace, panels)
│   ├── src/
│   │   ├── BotFace.tsx            # Animated mascot — 9 states
│   │   ├── SessionPanel.tsx       # Flight recorder view
│   │   └── WidgetSystem.tsx       # Plugin parameter widgets
└── Research/                      # Logo, design system, visual docs
```

---

## Roadmap

- [ ] Plugin dictionary for top 10 plugins
- [ ] Auto-discovery via audio analysis
- [ ] Parameter automation curves generated by AI
- [ ] Session summary export (what the AI did, and why)
- [ ] Windows support (JUCE is cross-platform, bridge needs porting)
- [ ] Plugin state presets saved by AI ("my warm master", "my hard-clipped drums")

---

<p align="center">
  <br/>
  Built by <a href="https://github.com/OfficialWhyEd">@whyed</a>
  &nbsp;·&nbsp; macOS · JUCE 7 · MIT License
  <br/><br/>
  <em>If you think this is insane, you're right. Star it anyway.</em>
</p>
