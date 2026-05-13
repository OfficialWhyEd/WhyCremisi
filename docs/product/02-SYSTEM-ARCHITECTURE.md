# Paper 02 — System Architecture
## Tech Stack, Components and Data Flows

```
────────────────────────────────────────────────────────────────
  WHYCREMISI RESEARCH PAPERS — N.02
  Complete System Architecture
────────────────────────────────────────────────────────────────
```

---

## Architecture Overview

```
╔══════════════════════════════════════════════════════════════╗
║                  WHYCREMISI ARCHITECTURE                     ║
╠══════════════════════════════════════════════════════════════╣
║                                                              ║
║  ┌──────────────────────────────────────────────────────┐   ║
║  │            UI LAYER  (React 18 + WebView)            │   ║
║  │  Chat · BoxChat · BotFace · Telemetry · Transport    │   ║
║  └───────────────────────┬──────────────────────────────┘   ║
║                          │  WebSocket JSON (port 8080)       ║
║  ┌───────────────────────▼──────────────────────────────┐   ║
║  │            BRIDGE LAYER  (C++ / JUCE)                │   ║
║  │    OscBridge · WebSocketServer · WebViewBridge       │   ║
║  └──────────┬────────────┬─────────────┬───────────────┘   ║
║             │            │             │                     ║
║  ┌──────────▼──┐  ┌──────▼──────┐  ┌──▼─────────────┐     ║
║  │  AiEngine   │  │  DSP Engine │  │ AgentWorkspace  │     ║
║  │  (multi-    │  │  Analyzer   │  │ PersonalityCore │     ║
║  │  provider)  │  │  EQ/Comp/   │  │ Memory System   │     ║
║  └──────────┬──┘  │  Limiter    │  └────────┬────────┘     ║
║             │     └──────┬──────┘           │              ║
║  ┌──────────▼────────────▼─────────────────▼──────────┐   ║
║  │              DAW LAYER  (JUCE AudioProcessor)       │   ║
║  │         PluginProcessor · ParameterMapper           │   ║
║  └───────────────────────────┬────────────────────────┘   ║
║                              │  VST3 Automation / OSC       ║
║  ┌───────────────────────────▼────────────────────────┐    ║
║  │          DAW HOST + THIRD-PARTY PLUGINS             │    ║
║  │  Ableton · Logic · Reaper · FabFilter · iZotope    │    ║
║  └────────────────────────────────────────────────────┘    ║
╚══════════════════════════════════════════════════════════════╝
```

---

## Tech Stack

| Layer | Technology | Rationale |
|-------|-----------|-----------|
| UI | React 18 + Framer Motion | Reactive components, fluid animations |
| Styling | Tailwind CSS 4 | Utility-first, zero overhead |
| Build | Vite 8 | Fast HMR, optimised bundle |
| Host | JUCE 8 | Industry standard for audio plugins |
| Bridge | WebSocket + JSON | Universal, low latency |
| AI | Multi-provider | Flexibility, no vendor lock-in |
| DSP | JUCE DSP + custom | Real-time, thread-safe |
| Serialisation | nlohmann/json | Header-only, fast, readable |
| OSC | oscpack | Standard DAW protocol |

---

## Complete Data Flow Example

**Scenario:** User writes "analyze the mix and tell me what's wrong with the kick"

```
  1. UI sends: { type:'ai.prompt', payload:{ prompt:'analyze...' } }
  2. OscBridge receives, dispatches to AiEngine
  3. AgentWorkspace enriches prompt:
     — BPM: 128, position: 02:34
     — Kick volume: -3dB, FFT peak at 220Hz (+2dB over ideal)
     — User memory: "prefers dry kick, narrow EQ cuts"
  4. AiEngine streams response via Gemini/Claude/Ollama
  5. UI renders BoxChat type 'eq' (detects "220Hz" in response)
  6. User: "yes apply it"
  7. ParameterMapper → FabFilter Pro-Q3: Band1 freq=220Hz gain=-3dB Q=2.1
  8. AgentSoul logs: "narrow EQ cut accepted — reinforce pattern"
```

---

*→ Continue: [Paper 03 — Third-Party Plugin Control](03-THIRD-PARTY-PLUGIN-CONTROL.md)*
