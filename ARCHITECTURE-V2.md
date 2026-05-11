# WhyCremisi V2 — Architettura per il Controllo Universale

## Visione

WhyCremisi è un **AI assistant mixing engineer** che vive dentro la DAW come VST3/AU.
L'AI non solo risponde a domande: **ascolta, analizza, decide e agisce** su TUTTO il mix —
compresi gli altri plugin (Waves, FabFilter, SoundToys, UAD, Valhalla, ecc.).

## Il Problema Fondamentale

I plugin VST3 sono **sandboxati**. Un plugin NON può:

- Leggere lo stato di un altro plugin nella catena
- Modificare i parametri di un altro plugin direttamente
- Sapere quali plugin ci sono nel progetto

**Soluzioni possibili per il controllo:**

| Metodo | DAW Support | Plugin Support | Latenza | Bidirezionale |
|--------|-------------|----------------|---------|---------------|
| **MIDI CC Out** | ✅ Tutte | ✅ Tutte (MIDI Learn) | Istantanea | No (solo send) |
| **MIDI CC In (feedback)** | ✅ Tutte | ⚠️ Waves/FabFilter no | Istantanea | Solo send |
| **OSC → DAW → Plugin** | ⚠️ REAPER/Cubase | ⚠️ Pochi | Media | Parziale |
| **VST3 Param Modulation** | ⚠️ REAPER/StudioOne | ❌ Non standard | Media | No |
| **AAX (Pro Tools)** | Solo PT | Limitato | Media | Parziale |
| **Inter-Plugin MIDI** | ⚠️ REAPER, Bitwig | ✅ Se routing manuale | Bassa | Sì |

**Scelta architetturale:** **MIDI CC Out** è il metodo più universale. Funziona con
tutti i DAW e tutti i plugin degni di nota. L'utente fa MIDI Learn una volta, poi
l'AI controlla tutto via CC.

---

## Architettura a Strati

```
┌─────────────────────────────────────────────────────────┐
│                    REACT UI (WebView)                    │
│  Chat AI  │  Mastering Rack  │  MIDI Learn  │  Mapper   │
│  BotFace  │  Telemetry       │  Widget Grid  │  Preset   │
└──────────────────────┬──────────────────────────────────┘
                       │ WebSocket (JSON)
┌──────────────────────▼──────────────────────────────────┐
│                   OscBridge (C++)                        │
│  ┌──────────┐  ┌──────────┐  ┌──────────────────────┐  │
│  │ AI Chat  │  │ DAW Ctrl │  │ Plugin Mapper        │  │
│  │ Engine   │  │ OSC/MIDI │  │ widget→param mapping │  │
│  └──────────┘  └──────────┘  └──────────────────────┘  │
└──────────────────────┬──────────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────────┐
│            Audio Processor (PluginProcessor)             │
│  ┌──────────┐  ┌──────────┐  ┌──────────────────────┐  │
│  │ Audio In │  │ MIDI Out │  │ VST3 Parameters      │  │
│  │/Out      │  │ (CC Send)│  │ (DAW automatable)    │  │
│  └──────────┘  └──────────┘  └──────────────────────┘  │
│  ┌──────────────────────────────────────────────────┐  │
│  │ Parameter Mapping Engine                          │  │
│  │ {widgetId → {target, param, min, max, curve}}    │  │
│  └──────────────────────────────────────────────────┘  │
└──────────────────────┬──────────────────────────────────┘
                       │
          ┌────────────┼────────────┐
          ▼            ▼            ▼
     DAW MIDI      DAW OSC     VST3 Host
     Routing       Routing     Automation
          │            │            │
          ▼            ▼            ▼
    FabFilter Q3   REAPER       DAW Track
    Waves SSL      Ableton      Volume/Pan
    SoundToys      Logic        etc.
    Valhalla       etc.
    (MIDI Learn)
```

---

## Componenti Nuovi (da implementare)

### 1. MidiHandler (`src/midi/MidiHandler.h/.cpp`)

```cpp
class MidiHandler {
    struct MidiMapping {
        int ccNumber;          // 0-127
        int midiChannel;       // 0-15
        float minValue;        // mapped range
        float maxValue;
        MappingCurve curve;    // linear, log, exponential
        juce::String targetPlugin;  // display name only
        juce::String paramName;     // display name only
        int paramId;           // optional VST3 param ID
    };

    // MIDI Learn
    void startLearn(int widgetId);  // waits for next MIDI input
    void stopLearn();
    void onMidiInput(const juce::MidiMessage& msg);
    
    // Send CC
    void sendCC(int widgetId, float normalizedValue);
    void flush(juce::MidiBuffer& buffer);  // called from processBlock
    
    // Persistence
    juce::ValueTree saveMappings();
    void loadMappings(const juce::ValueTree& tree);
};
```

### 2. ParameterMapper (`src/core/ParameterMapper.h/.cpp`)

Central registry that connects UI widgets → MIDI CC → VST3 params.

```cpp
class ParameterMapper {
    struct WidgetBinding {
        juce::String widgetId;
        BindingType type;       // midi, vst3Param, osc, native
        union {
            MidiConfig midi;
            Vst3Config vst3;
            OscConfig osc;
        };
        float currentValue;
        float min, max;
        float defaultValue;
    };

    std::map<juce::String, WidgetBinding> bindings;
    
    // Set widget value → trigger output
    void setWidgetValue(const juce::String& widgetId, float value);
    
    // Get widget value (for UI)
    float getWidgetValue(const juce::String& widgetId);
    
    // Expose as VST3 parameters for DAW automation
    void syncToVst3Params(juce::AudioProcessorValueTreeState& params);
    
    // MIDI Learn integration
    void bindToMidiLearn(const juce::String& widgetId, int cc, int channel);
};
```

### 3. AI Structured Commands

Non più solo testo libero. L'AI deve emettere comandi strutturati
che il bridge esegue:

```json
{
  "type": "ai.action",
  "actions": [
    {
      "target": "plugin:fabfilter-pro-q-3",
      "band": 1,
      "param": "gain",
      "value": -2.4,
      "unit": "dB"
    },
    {
      "target": "daw:master",
      "param": "volume",
      "value": -1.2,
      "unit": "dB"
    },
    {
      "target": "midi:cc",
      "channel": 1,
      "cc": 22,
      "value": 64
    }
  ],
  "text": "Ho applicato -2.4dB a 200Hz sul master EQ"
}
```

---

## Piano di Implementazione

### FASE 1 — Fondamenta MIDI e Mapping (1-2 settimane)

| Task | File | Descrizione | Tempo |
|------|------|-------------|-------|
| 1.1 | `src/midi/MidiHandler.h/.cpp` | Classe MIDI CC send con flush su MidiBuffer | 4h |
| 1.2 | `src/core/ParameterMapper.h/.cpp` | Sistema di mappatura widget→param | 6h |
| 1.3 | `PluginProcessor.h/.cpp` | Integrare MidiHandler + ParameterMapper nel processBlock | 3h |
| 1.4 | `OscBridge.cpp` | Implementare `widget.valueChange` reale (non stub) | 2h |
| 1.5 | `PluginProcessor.cpp` | Cablare gain3-8 (o rimuoverli) | 30min |
| 1.6 | Build Release + React dist | `npm run build`, `cmake --build --config Release` | 1h |
| 1.7 | `config.get` read path | Implementare lettura config in dispatchConfig | 1h |

**Totale FASE 1: ~18h (2-3 giorni full-time)**

### FASE 2 — MIDI Learn e UI Mapping (1 settimana)

| Task | File | Descrizione | Tempo |
|------|------|-------------|-------|
| 2.1 | `MidiHandler.cpp` | MIDI Learn: cattura messaggio MIDI in entrata | 4h |
| 2.2 | `PluginProcessor.cpp` | Routing MIDI input → MidiHandler | 2h |
| 2.3 | `App.jsx` | Nuova UI: MIDI Learn button su ogni widget | 4h |
| 2.4 | `App.jsx` | Nuova UI: Parameter Mapping Panel (drag & drop) | 6h |
| 2.5 | `OscBridge.cpp` | Messaggi `midi.learn.start`, `midi.learn.stop`, `midi.learn.complete` | 2h |
| 2.6 | `whycremisi-bridge.js` | MIDI Learn bridge methods | 1h |
| 2.7 | Persistenza mappature | `getStateInformation/setStateInformation` per le mappature | 2h |

**Totale FASE 2: ~21h (3-4 giorni)**

### FASE 3 — AI Decision Engine (2 settimane)

| Task | File | Descrizione | Tempo |
|------|------|-------------|-------|
| 3.1 | `AiEngine.h/.cpp` | Nuovo prompt system: contesto + azioni strutturate | 6h |
| 3.2 | `OscBridge.cpp` | `ai.action` message handler (esegue comandi AI) | 4h |
| 3.3 | `AiEngine.cpp` | Plugin chain context builder (quali plugin, parametri) | 4h |
| 3.4 | `OscBridge.cpp` | AI-action broadcast (UI mostra cosa fa l'AI) | 2h |
| 3.5 | `App.jsx` | Action log: "AI ha applicato: -2.4dB su Q3 Band 1" | 3h |
| 3.6 | `App.jsx` | Undo/Redo system per azioni AI | 4h |
| 3.7 | `SessionManager.cpp` | Log azioni AI in memoria lunga | 2h |

**Totale FASE 3: ~25h (4-5 giorni)**

### FASE 4 — Plugin State Awareness (2 settimane)

| Task | File | Descrizione | Tempo |
|------|------|-------------|-------|
| 4.1 | Ricerca: auto-detect plugin chain | Come rilevare altri plugin nel chain? (MIDI, OSC, o manuale) | 6h |
| 4.2 | `App.jsx` | Plugin Chain Setup: l'utente configura la catena manualmente | 6h |
| 4.3 | `AiEngine.cpp` | Template prompt con catena plugin conosciuta | 3h |
| 4.4 | `OscBridge.cpp` | MIDI feedback (CC In) per leggere stato plugin | 6h |
| 4.5 | `App.jsx` | Plugin State Display (misure, settaggi attuali) | 4h |
| 4.6 | REAPER script | Script ReaScript per esportare catena effetti | 3h |
| 4.7 | Ableton script | Script M4L per esportare catena effetti | 3h |
| 4.8 | `PluginProcessor.cpp` | MIDI Through Mode (MIDI In → Out per Learn) | 2h |

**Totale FASE 4: ~33h (5-7 giorni)**

### FASE 5 — DSP Modules (1-2 settimane)

| Task | File | Descrizione | Tempo |
|------|------|-------------|-------|
| 5.1 | `src/dsp/EQBand.h/.cpp` | EQ parametrico (IIR biquad) | 6h |
| 5.2 | `src/dsp/Compressor.h/.cpp` | Compressore con attack/release/ratio | 6h |
| 5.3 | `src/dsp/Limiter.h/.cpp` | Limiter a lookahead | 4h |
| 5.4 | `src/dsp/Analyzer.h/.cpp` | FFT real-time + phase correlation | 8h |
| 5.5 | `PluginProcessor.cpp` | Integrazione DSP in processBlock | 3h |
| 5.6 | `App.jsx` | EQ UI (parametric bands) | 6h |
| 5.7 | `App.jsx` | Comp UI (grafico risposta) | 4h |
| 5.8 | `App.jsx` | Vector scope reale (phase correlation) | 4h |
| 5.9 | `OscBridge.cpp` | Broadcast analisi FFT/loudness/phase | 2h |

**Totale FASE 5: ~43h (6-9 giorni)**

### FASE 6 — Polish e Produzione (1-2 settimane)

| Task | File | Descrizione | Tempo |
|------|------|-------------|-------|
| 6.1 | Code signing + notarization | macOS Developer ID + notarize | 4h |
| 6.2 | Windows code signing | Certificato + signtool | 3h |
| 6.3 | Linux AppImage/Flatpak | Packaging per Linux | 4h |
| 6.4 | Test con REAPER | Verifica controllo FabFilter/Waves via MIDI | 4h |
| 6.5 | Test con Ableton | Verifica controllo tramite MIDI | 3h |
| 6.6 | Test con Logic | Verifica AU + MIDI control | 3h |
| 6.7 | Test con FL Studio | Verifica MIDI out | 2h |
| 6.8 | Documentazione utente | Manuale + video dimostrativi | 8h |
| 6.9 | Preset system | Salvare/caricare preset plugin (catene complete) | 4h |
| 6.10 | Performance tuning | Audio thread profiling, ridurre latenza | 4h |

**Totale FASE 6: ~39h (5-8 giorni)**

---

## Stima Totale

| Fase | Ore | Full-time | Part-time (sera) |
|------|-----|-----------|-------------------|
| FASE 1 — Fondamenta MIDI | 18h | 2-3 giorni | 4-5 giorni |
| FASE 2 — MIDI Learn + UI | 21h | 3-4 giorni | 5-6 giorni |
| FASE 3 — AI Decision Engine | 25h | 4-5 giorni | 6-8 giorni |
| FASE 4 — Plugin State | 33h | 5-7 giorni | 8-10 giorni |
| FASE 5 — DSP Modules | 43h | 6-9 giorni | 10-14 giorni |
| FASE 6 — Polish | 39h | 5-8 giorni | 8-12 giorni |
| **TOTALE** | **~180h** | **4-5 settimane** | **6-10 settimane** |

---

## Decisioni Architetturali Chiave

### Perché MIDI CC e non OSC/VST3 direct?
- **MIDI CC** funziona con TUTTI i plugin e TUTTI i DAW
- FabFilter, Waves, SoundToys, Valhalla, UAD — tutti supportano MIDI Learn
- L'utente fa MIDI Learn UNA VOLTA, salva la mappatura, poi l'AI controlla tutto
- MIDI è a bassissima latenza e non richiede threading extra

### Come fa l'AI a sapere cosa c'è nel progetto?
Non può in modo automatico (VST3 sandboxing). Soluzioni:
1. **Setup manuale**: l'utente configura la catena nella UI (facile, universale)
2. **MIDI feedback**: plugin mandano CC di risposta (raro, non standard)
3. **ReaScript/M4L**: script DAW-specifici che esportano la catena
4. **Future**: VST3 inter-plugin communication (non ancora standardizzato)

### Come funziona l'AI action execution?
1. Utente scrive "dammi più caldo sul master"
2. AI analizza contesto (plugin catena conosciuta + meter)
3. AI risponde con azioni strutturate (JSON)
4. Bridge esegue: MIDI CC → FabFilter Q3 Band 1 Gain → -2.4dB
5. UI mostra "AI ha applicato: Master EQ 200Hz -2.4dB"

### MIDI Learn workflow
1. Click "LEARN" sul widget (es. gain knob)
2. Plugin entra in modalità learn (ascolta MIDI In)
3. Utente muove un controller fisico OR un parametro di un altro plugin
4. Plugin cattura il MIDI CC/channel
5. Salva mappatura: `gainKnob → CC#22, Ch.1`
6. Ora il gain knob (e l'AI) controlla quel parametro via MIDI CC

---

## Dipendenze

### MIDI
- Plugin già ha `NEEDS_MIDI_INPUT TRUE` e `NEEDS_MIDI_OUTPUT TRUE`
- `processBlock` già riceve `MidiBuffer& midiMessages`
- Basta aggiungere messaggi CC al buffer in uscita

### Parameter Persistence
- `ParameterMapper` salvato come ValueTree in `getStateInformation`
- MIDI mappings persistono nel progetto DAW
- Widget bindings salvabili come preset

### Nuove dipendenze librerie
- Nessuna. MIDI è nativo JUCE. FFT è nativo JUCE. DSP biquad è algebra base.

---

## Riepilogo per l'Utente

"WhyCremisi oggi è un plugin che chatta con AI e controlla transport DAW.
Dopo V2 sarà un **AI mixing engineer** che:
- Controlla qualsiasi plugin via MIDI CC (Waves, FabFilter, UAD, ecc.)
- Impara i controlli con MIDI Learn
- Analizza il mix con DSP reale (FFT, compressione, EQ)
- Prende decisioni e le esegue sul mix
- Ricorda tutto (session memory)
- Funziona in REAPER, Ableton, Logic, FL Studio, Cubase"
