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
| Streaming AI responses | ✅ Chunk-by-chunk | ❌ | — |
| Run offline | ✅ Ollama support | ❌ | — |
| Works with any DAW | ✅ JUCE/VST3/AU | ⚠️ Limited | ✅ |

---

## How it works

WhyCremisi doesn't need to know what a plugin is. VST3 exposes every parameter as a numbered index. WhyCremisi reads them all. The entire communication stack runs inside the DAW process — no external app, no network roundtrip.

```
┌─────────────────────────────────────────────────────────┐
│                     Your DAW session                    │
│                                                         │
│  [Serum]  [FabFilter Q3]  [Valhalla]  [OTT]  ...      │
│     │           │              │         │              │
│     └───────────┴──────────────┴─────────┘              │
│                         │   VST3 parameter graph        │
│              ┌──────────▼──────────┐                    │
│              │    WhyCremisi       │  ← master channel  │
│              │                     │                    │
│              │  AiEngine           │  Ollama / Groq /   │
│              │  SessionManager     │  Gemini / Claude   │
│              │  OscBridge          │  OpenAI / OpenRouter│
│              └──────┬──────────────┘                    │
└─────────────────────┼─────────────────────────────────────┘
                      │
        ┌─────────────┴──────────────┐
        │                            │
   OSC UDP :9000               WebSocket :8080
   (DAW → plugin)         (plugin ↔ React UI)
        │                            │
   OSC UDP :9001            WhyCremisiBridge.js
   (plugin → DAW)          (singleton, auto-reconnect)
                                     │
                              React UI (WebView)
                           transport · tracks · widgets
                           useWhyCremisi() hook
```

**The 33ms broadcast loop** — a JUCE `Timer` fires every 33ms (~30fps), pushing DAW state (transport, meter L/R/peak) to every connected WebSocket client. React stays in sync with the DAW in real time without polling.

**The Flight Recorder** — every event in the session is appended to a JSONL file with millisecond timestamps. Parameter changes, transport events, AI prompts and responses, OSC messages, errors — all of it. The AI sees not just the current state but everything that happened since you pressed play.

---

## Three phases of intelligence

| Phase | What it does | Status |
|-------|-------------|--------|
| **① Universal Bridge** | Scans and maps all VST3 parameters by index across every loaded plugin. Read + write in real time. | ✅ Live |
| **② Plugin Dictionary** | Semantic layer that maps index numbers to human-readable names for the 10 most-used plugins in the world. "Filter cutoff" instead of "param_0047". | 🔧 Building |
| **③ Auto-Discovery** | AI infers what unknown parameters do by observing their effect on audio. Learns any plugin without a dictionary entry. | 🔮 Roadmap |

Phase ① alone is already useful. You can ask the AI to move a parameter and it will find it. Phases ② and ③ make it fluent.

---

## The wire protocol

Every message on the WebSocket is a JSON object:

```json
{ "type": "ai.prompt", "id": "uuid-v4", "timestamp": 1718123456789, "payload": { ... } }
```

**DAW → UI** events the plugin broadcasts:

| Message type | What it carries |
|---|---|
| `daw.transport` | isPlaying, isRecording, BPM, positionSeconds |
| `daw.track` | trackId, name, volumeDb, pan, muted, soloed |
| `daw.meter` | trackId, leftDb, rightDb, peakLeftDb, peakRightDb |
| `ai.response` | requestId, content, provider, isComplete |
| `ai.stream` | requestId, chunk, isDone — for streaming responses |
| `ui.widget.create` | widgetId, widgetType, title, config |
| `ui.widget.update` | widgetId + updated values |
| `plugin.error` | code, message, severity |

**UI → DAW** commands the React side can send:

| Message type | Effect |
|---|---|
| `daw.command` | play, stop, record, setVolume, etc. |
| `daw.request` | request track list, session state |
| `ai.prompt` | dispatch a prompt to the AI provider |
| `ui.widget.create/remove` | manage dynamic UI widgets |
| `osc.send` | forward a raw OSC message to the DAW |
| `config.get / config.set` | read/write plugin config at runtime |

---

## Session memory — how nothing gets lost

```
~/Library/Application Support/WhyCremisi/
  sessions/
    20240615_142301/
      header.json      ← session metadata, written once at start
      events.jsonl     ← one JSON object per line, append-only
      summary.json     ← event counts per type, written at end
  current.json         ← always-fresh live snapshot of active session
  memory.json          ← long-term knowledge base, updated across sessions
```

`events.jsonl` is the core: append-only, zero-overhead, reconstructable. Every `logOscEvent`, `logTransport`, `logParameter`, `logAiPrompt`, `logAiResponse`, `logError` call adds one line. The rate-limiter keeps meter ticks to 1 entry per 500ms and position ticks to 1 per second — so the log stays usable at high sample rates.

`memory.json` accumulates knowledge across sessions. The AI doesn't start from zero each time.

---

## AI providers

| Provider | Default | Notes |
|---|---|---|
| **Ollama** | ✅ yes (llama3.2) | Runs at `localhost:11434`, fully offline |
| **Groq** | — | Fast inference, free tier available |
| **Gemini** | — | Google, flash and pro models |
| **Anthropic** | — | Claude 3 family |
| **OpenAI** | — | GPT-4o and variants |
| **OpenRouter** | — | Single key, any model |

Config: `temperature 0.7`, `maxTokens 2048`, `timeout 30s`. All providers share the same `sendPromptAsync()` interface — swap with one config change.

---

## BotFace — the mascot that reads the room

The BotFace SVG mascot changes state automatically based on what's happening on the wire:

| Bridge event | BotFace state |
|---|---|
| `ai.prompt` sent | `thinking` |
| `ai.stream` chunk received | `typing` |
| `ai.response` complete | `success` → `idle` after 2s |
| `plugin.error` | `error` → `idle` after 3s |
| Idle | `idle` |

9 emotional states total, animated with framer-motion. The mascot is not decorative — it's the real-time status indicator of everything happening between the plugin, the AI provider, and the DAW.

---

## Features

- **Universal parameter bridge** — reads and writes any VST3 parameter by index, across all plugins simultaneously
- **Flight Recorder** — append-only JSONL session log + cross-session `memory.json`, injected as context into every AI prompt
- **Streaming AI responses** — chunk-by-chunk `ai.stream` messages, BotFace animates during generation
- **BotFace mascot** — animated SVG mascot with 9 emotional states, state machine driven by WebSocket message types
- **React WebView UI** — full React app rendered inside a JUCE WebView, `useWhyCremisi()` hook for clean integration
- **WhyCremisiBridge.js** — WebSocket singleton with auto-reconnect (10 attempts, 2s interval), pending request map, typed event emitter
- **33ms broadcast loop** — JUCE Timer pushes transport + meter to React at ~30fps
- **Dynamic widget system** — C++ broadcasts `ui.widget.create/update/remove`, React renders them live
- **Multi-provider AI** — Ollama (offline default) + Groq + Gemini + Claude + OpenAI + OpenRouter
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

Load WhyCremisi on the **master channel** in your DAW. Open the plugin UI. The bridge starts automatically — OSC on `:9000`, WebSocket on `:8080`, React connects and sends `plugin.init`.

For **offline use**: install [Ollama](https://ollama.ai), run `ollama pull llama3.2`. No API key needed.

---

## Project structure

```
WhyCremisi/
├── src/
│   ├── core/
│   │   ├── PluginProcessor.cpp    # VST3 host + universal parameter scanner
│   │   ├── PluginEditor.cpp       # JUCE WebView host
│   │   ├── SessionManager.cpp     # JSONL event log + cross-session memory.json
│   │   └── tests/                 # 14 automated tests
│   ├── ai/
│   │   └── AiEngine.cpp           # 6 providers, sync + async, streaming
│   └── bridge/
│       ├── OscBridge.cpp          # 33ms timer, widget broadcasts, message dispatch
│       ├── OscHandler.cpp         # UDP OSC receiver (:9000 in, :9001 out)
│       └── WebSocketServer.cpp    # TCP WebSocket server (:8080)
├── webview-ui/                    # React app (UI, BotFace, panels)
│   ├── src/
│   │   ├── whycremisi-bridge.js   # WebSocket singleton + useWhyCremisi() hook
│   │   ├── BotFace.tsx            # Animated mascot — 9 states, framer-motion
│   │   ├── SessionPanel.tsx       # Flight recorder view
│   │   └── WidgetSystem.tsx       # Dynamic plugin parameter widgets
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
