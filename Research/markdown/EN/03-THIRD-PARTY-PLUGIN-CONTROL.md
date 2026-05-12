# Paper 03 — Third-Party Plugin Control
## FabFilter, iZotope, Waves and the Complete Plugin Ecosystem

```
────────────────────────────────────────────────────────────────
  WHYCREMISI RESEARCH PAPERS — N.03
  Universal Third-Party Plugin Control

  "Every knob of every plugin — accessible by AI."
────────────────────────────────────────────────────────────────
```

---

## 1. The Digital Silo Problem

Audio plugins are isolated islands. FabFilter Pro-Q3 does not know what iZotope Ozone is doing. Waves SSL does not know the track level. WhyCremisi is the first system that **breaks these silos** — creating a unified control plane on which AI can operate the entire ecosystem.

---

## 2. Control Mechanisms

### 2.1 VST3 Parameter Automation (Primary)

VST3 exposes every plugin parameter as a normalised float [0.0–1.0] accessible from the host.

```cpp
// ParameterMapper — conceptual example
void ParameterMapper::setPluginParameter(
    int trackId, const std::string& pluginName,
    const std::string& paramName, float value)
{
    auto* proc = getProcessorOnTrack(trackId, pluginName);
    int idx = lookupParamIndex(pluginName, paramName);
    proc->setParameterNotifyingHost(idx, value);
}
```

### 2.2 MIDI CC — Universal fallback for any MIDI-Learn capable plugin
### 2.3 OSC — Direct communication with Reaper and OSC-native DAWs
### 2.4 DAW SDK — ReaScript (Reaper), Max for Live (Ableton), Control Surface Protocol

---

## 3. Supported Plugins — Priority Roadmap

### FabFilter — HIGH PRIORITY

| Plugin | Key Parameters Mapped |
|--------|----------------------|
| Pro-Q 3 | 24 bands: freq, gain, Q, type, phase mode |
| Pro-C 2 | Threshold, ratio, attack, release, knee, style |
| Pro-L 2 | Threshold, gain, lookahead, true peak, oversampling |
| Pro-MB | 6 bands: range, threshold, ratio, attack, release |
| Pro-R | Decay, room size, character, brightness |
| Saturn 2 | 6 saturation bands: drive, tone, level, mix |

### iZotope — HIGH PRIORITY

| Plugin | Capabilities |
|--------|-------------|
| Ozone 11 | Maximizer threshold, IRC mode, EQ, imager width, LUFS target |
| Neutron 4 | Per-track EQ, compressor, transient shaper, exciter |
| RX 10 | Noise reduction, de-click, dialogue isolation |

### Waves — MEDIUM PRIORITY

SSL G-Master Buss · API 2500 · CLA-76 · H-Reverb · Renaissance EQ · L3-16 · Kramer HLS

### Universal Audio — MEDIUM PRIORITY

1176 · LA-2A · Neve 1073 · API Vision · Ocean Way Studios

### Native Instruments — MEDIUM PRIORITY

Massive X · Kontakt 7 · Guitar Rig 7 · Supercharger GT

---

## 4. ParameterMapper — Core Integration Engine

```
  Responsibilities:
  
  DISCOVERY   — Scans all exposed parameters at plugin load
  NAMING      — Maps numeric IDs to semantic names (47 → "Band3_Gain")
  SCALING     — Converts physical values to normalised (-3dB → 0.42)
  VALIDATION  — Checks range and type before applying
  UNDO        — Saves prior state for instant rollback
```

---

## 5. Plugin Database Format

```json
{
  "plugin_id": "fabfilter_proq3",
  "display_name": "FabFilter Pro-Q 3",
  "vst3_guid": "E8E38A93-21B0-4B8D-B7EC-B54B8B9CF48C",
  "parameters": [
    {
      "id": 0, "name": "Band1_Frequency",
      "alias": ["freq1", "eq band 1 freq"],
      "min": 20.0, "max": 20000.0,
      "scale": "logarithmic", "unit": "Hz"
    }
  ],
  "ai_presets": {
    "remove_mud": { "Band2_Freq": 220, "Band2_Gain": -3.5, "Band2_Q": 2.1 },
    "add_presence": { "Band4_Freq": 3500, "Band4_Gain": 2.0 }
  }
}
```

---

## 6. Safety Constraints

- No action applied without explicit user confirmation (unless auto-apply enabled)
- Full undo stack maintained for every action
- No gain changes > 12dB in a single command
- No modifications during recording (unless override set)
- No audio routing changes (feedback risk)

---

*→ Continue: [Paper 04 — Agent Memory System](04-AGENT-MEMORY-SYSTEM.md)*
