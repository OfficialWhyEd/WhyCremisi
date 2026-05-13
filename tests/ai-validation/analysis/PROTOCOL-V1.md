# 📡 Protocol Analysis: OSC-to-JSON Mapping

This document details the communication protocol used by the `OscBridge`.

## 1. DAW → UI (Downstream)

| OSC Address | JSON Type | Payload Example |
| :--- | :--- | :--- |
| `/transport/play` | `daw.transport` | `{"isPlaying": true, "bpm": 120.0}` |
| `/track/[id]/volume` | `daw.track` | `{"trackId": 1, "volume": 0.75}` |
| `/meter/[id]` | `daw.meter` | `{"trackId": -1, "leftDb": -12.4, "rightDb": -12.2}` |

## 2. UI → DAW (Upstream)

| Action | OSC Address | Value Type |
| :--- | :--- | :--- |
| `play` | `/transport/play` | (None) |
| `stop` | `/transport/stop` | (None) |
| `setGain` | `/track/master/volume` | `float` (0.0 - 1.0) |
| `setDrive` | `/param/drive` | `float` (0.0 - 1.0) |

## 3. AI Communication (Internal)

### `ai.prompt`
- **Direction**: UI → Bridge → AiEngine
- **Payload**: `{"prompt": "Analyze low end", "requestId": "..."}`

### `ai.stream`
- **Direction**: AiEngine → Bridge → UI
- **Payload**: `{"chunk": "I detected...", "isDone": false, "requestId": "..."}`

---

## Observations on Protocol v1
- **Efficiency**: Use of `nlohmann::json` ensures rapid serialization.
- **Robustness**: The bridge uses a `juce::Timer` (30ms) to batch high-frequency data (like meters) avoiding WebSocket congestion.
