# Paper 03 — Controllo Plugin di Terze Parti
## FabFilter, iZotope, Waves e l'Ecosistema Plugin Completo

```
────────────────────────────────────────────────────────────────
  WHYCREMISI RESEARCH PAPERS — N.03
  Controllo Universale dei Plugin di Terze Parti
  
  "Ogni manopola di ogni plugin — accessibile dall'AI."
────────────────────────────────────────────────────────────────
```

**Categoria:** Integrazione Plugin  
**Importanza:** ★★★★★ — Core differenziante del progetto

---

## 1. Il Problema del Silos Digitale

I plugin audio professionali sono isole isolate. FabFilter Pro-Q3 non sa cosa sta facendo iZotope Ozone. Waves SSL non conosce il livello della traccia. L'ingegnere del suono umano tiene tutto questo in testa simultaneamente.

WhyCremisi è il primo sistema che **rompe questi silos** — creando un piano di controllo unificato su cui l'AI può operare l'intero ecosistema.

```
  PRIMA (stato attuale del settore):
  
  [Produttore] → apre FabFilter manualmente → gira manopola EQ
  [Produttore] → apre Ozone manualmente → regola limiter
  [Produttore] → apre Waves manualmente → comprime
  
  Ogni plugin: isolato, nessuna comunicazione, nessuna memoria
  
  ─────────────────────────────────────────────────────────────
  
  DOPO (WhyCremisi):
  
  [Produttore] → "la kick ha troppo mud, sistemala"
  [WhyCremisi] → analizza FFT → identifica 200-400Hz → 
                 apre FabFilter Pro-Q3 sulla traccia kick →
                 imposta banda narrow -3.5dB a 220Hz →
                 verifica risultato → conferma → registra in memoria
```

---

## 2. I Meccanismi di Controllo

Esistono quattro meccanismi attraverso cui WhyCremisi controlla i plugin:

### 2.1 VST3 Parameter Automation (Metodo Primario)

Lo standard VST3 espone ogni parametro di un plugin come un valore numerico normalizzato [0.0 – 1.0] accessibile dall'host.

```
  Host (DAW/JUCE)
      │
      ├── getParameter(paramId) → float [0,1]
      └── setParameter(paramId, value)  ← WhyCremisi agisce qui
```

**Come funziona:**

```cpp
// ParameterMapper.cpp — esempio concettuale
void ParameterMapper::setPluginParameter(
    int trackId, 
    const std::string& pluginName,
    const std::string& paramName, 
    float value)
{
    auto* processor = getProcessorOnTrack(trackId, pluginName);
    int paramIdx = lookupParamIndex(pluginName, paramName);
    processor->setParameterNotifyingHost(paramIdx, value);
}
```

**Database parametri (esempio FabFilter Pro-Q3):**

```
  Plugin: FabFilter Pro-Q3
  ─────────────────────────────────────────────────────────
  Parametro           ID     Range        Descrizione
  ─────────────────────────────────────────────────────────
  Band1_Frequency     0      20-20000Hz   Frequenza banda 1
  Band1_Gain          1      -30 / +30dB  Guadagno banda 1
  Band1_Q             2      0.025 - 40   Risonanza banda 1
  Band1_Type          3      enum         Peak/Shelf/HP/LP/...
  Band1_Enabled       4      0/1          Attiva/disattiva
  Output_Gain         5      -30 / +30dB  Guadagno uscita
  Phase_Mode          6      enum         Linear/Minimum/Zero
  ...                 ...    ...          ...
  ─────────────────────────────────────────────────────────
  Totale: ~200 parametri mappati
```

### 2.2 MIDI CC (Metodo Universale)

Qualunque plugin con MIDI Learn può essere controllato via MIDI Control Change. WhyCremisi genera messaggi MIDI CC internamente.

```
  WhyCremisi → MIDI CC 74 (Brightness) value=64 → Plugin target
```

Utile per plugin che non espongono parametri VST3 in modo standard, o per hardware esterno (sintetizzatori, controller).

### 2.3 OSC (Open Sound Control)

Alcuni DAW e plugin supportano OSC nativo (es. Reaper). WhyCremisi usa un socket OSC per comunicare direttamente.

```
  WhyCremisi → /track/1/fx/1/param/3/value 0.75 → Reaper/plugin
```

### 2.4 DAW SDK / ReaScript / Max for Live

Per integrazioni più profonde:
- **Reaper:** ReaScript API (Lua/Python/C)
- **Ableton:** Max for Live device che riceve messaggi WhyCremisi
- **Logic:** Control Surface Protocol

---

## 3. Plugin Supportati — Roadmap Dettagliata

### 3.1 FabFilter Suite — Priorità ALTA

```
  ┌─────────────────────────────────────────────────────────┐
  │  FABFILTER — Integrazione Completa                      │
  ├─────────────────────┬───────────────────────────────────┤
  │  Plugin             │  Parametri Chiave Mappati         │
  ├─────────────────────┼───────────────────────────────────┤
  │  Pro-Q 3            │  24 bande: freq, gain, Q, type    │
  │                     │  Phase mode, output gain          │
  │                     │  Analyzer display mode            │
  ├─────────────────────┼───────────────────────────────────┤
  │  Pro-C 2            │  Threshold, ratio, attack,        │
  │                     │  release, knee, make-up gain      │
  │                     │  Style (Clean/Classic/Opto/...)   │
  │                     │  Sidechain freq, gain             │
  ├─────────────────────┼───────────────────────────────────┤
  │  Pro-L 2            │  Threshold, gain, attack,         │
  │                     │  release, lookahead               │
  │                     │  True peak limit, oversampling    │
  ├─────────────────────┼───────────────────────────────────┤
  │  Pro-MB             │  6 bande: range, threshold,       │
  │                     │  ratio, attack, release per banda │
  ├─────────────────────┼───────────────────────────────────┤
  │  Pro-R              │  Decay, room size, character,     │
  │                     │  space, brightness, distance      │
  ├─────────────────────┼───────────────────────────────────┤
  │  Saturn 2           │  6 bande saturazione: drive,      │
  │                     │  tone, level, mix per banda       │
  └─────────────────────┴───────────────────────────────────┘
```

**Esempio di utilizzo AI con FabFilter Pro-Q3:**
```
  Utente: "togli il mud dai bassi del piano"
  AI analizza FFT traccia piano: picco a 320Hz, buildup 180-280Hz
  
  WhyCremisi esegue:
  → Pro-Q3 banda 1: tipo=Peak, freq=230Hz, gain=-4.5dB, Q=1.8
  → Pro-Q3 banda 2: tipo=HighPass, freq=45Hz (pulisce subsonic)
  → Verifica: LUFS rimasto stabile, fase OK
  → Proposta all'utente con anteprima A/B
```

### 3.2 iZotope Suite — Priorità ALTA

```
  ┌─────────────────────────────────────────────────────────┐
  │  IZOTOPE — Integrazione Intelligente                    │
  ├─────────────────────┬───────────────────────────────────┤
  │  Plugin             │  Capacità WhyCremisi              │
  ├─────────────────────┼───────────────────────────────────┤
  │  Ozone 11           │  Maximizer threshold, IRC mode    │
  │                     │  EQ intelligente, imager width    │
  │                     │  Vintage Tape saturation drive    │
  │                     │  Target loudness LUFS             │
  ├─────────────────────┼───────────────────────────────────┤
  │  Neutron 4          │  Per-traccia: EQ, compressore     │
  │                     │  Transient shaper, exciter        │
  │                     │  Visual Mixer balance             │
  ├─────────────────────┼───────────────────────────────────┤
  │  RX 10              │  Noise reduction amount           │
  │                     │  Dialogue isolation sensitivity   │
  │                     │  De-click threshold               │
  ├─────────────────────┼───────────────────────────────────┤
  │  Insight 2          │  Lettura metadati loudness        │
  │                     │  (read-only, usato per analisi)   │
  └─────────────────────┴───────────────────────────────────┘
```

### 3.3 Waves — Priorità MEDIA

```
  Plugin principali mappati:
  
  ● SSL G-Master Buss    — Threshold, ratio, attack, release,
                           makeup, mix
  ● API 2500             — Threshold, ratio, attack, release,
                           headroom, tone
  ● CLA-76               — Input, output, ratio, attack, release
  ● H-Reverb             — Pre-delay, decay, size, early/late
  ● Renaissance EQ       — 6 bande paragrafiche
  ● L3-16                — Threshold, gain per banda (16-band)
  ● Kramer HLS           — Drive, bass, treble, mix
```

### 3.4 Universal Audio — Priorità MEDIA

UAD plugin operano su hardware DSP dedicato ma espongono parametri VST3 standard dall'host:

```
  ● 1176 Classic Limiter — Input, output, ratio, attack, release
  ● LA-2A Leveling Amp   — Peak reduction, gain
  ● Neve 1073 Preamp EQ  — Gain stages, EQ bands
  ● API Vision Channel    — Parametric EQ, compressor, gate
  ● Ocean Way Studios     — Room selection, mic distance
```

### 3.5 Native Instruments — Priorità MEDIA

```
  ● Massive X            — Wavetable position, filter cutoff/res
  ● Kontakt 7            — Volume, pan, transpose per strumento
  ● Guitar Rig 7         — Amp gain, cabinet type, effects chain
  ● Supercharger GT      — Drive, character, compress amount
```

---

## 4. Il ParameterMapper — Cuore dell'Integrazione

```
  src/core/ParameterMapper.cpp
  
  Responsabilità:
  
  1. DISCOVERY — Alla connessione del plugin, scansiona tutti
                 i parametri esposti e li cataloga
  
  2. NAMING    — Mappa i parametri numerici a nomi semantici
                 (paramId=47 → "Band3_Gain" in Pro-Q3)
  
  3. SCALING   — Converte valori fisici in valori normalizzati
                 (-3dB → 0.42 in scala Pro-Q3)
  
  4. VALIDATION — Controlla range e tipo prima di applicare
  
  5. UNDO      — Salva stato precedente per rollback immediato
```

**Struttura dati interna:**

```
  PluginParameterMap {
    pluginName: "FabFilter Pro-Q3"
    pluginId: "E8E38A93-..."  (VST3 GUID)
    parameters: [
      { id: 0, name: "Band1_Frequency", 
        min: 20.0, max: 20000.0, unit: "Hz",
        defaultValue: 1000.0, currentValue: 220.0 },
      { id: 1, name: "Band1_Gain",
        min: -30.0, max: 30.0, unit: "dB",
        defaultValue: 0.0, currentValue: -4.5 },
      ...
    ]
  }
```

---

## 5. Sicurezza e Controllo

### 5.1 Consenso obbligatorio
Nessuna modifica viene applicata senza conferma esplicita dell'utente, salvo configurazione "auto-apply" attivata manualmente.

### 5.2 Undo istantaneo
```
  WhyCremisi mantiene uno stack di undo per ogni azione:
  
  [stato pre-azione] → [azione applicata] → [stato post-azione]
                                    ↓
                          "ANNULLA" → ripristina stato pre-azione
```

### 5.3 Limiti di sicurezza
- Nessuna modifica al routing audio (rischio feedback)
- Nessuna variazione di gain > 12dB in un singolo comando
- Nessuna modifica durante la registrazione (a meno di override)

---

## 6. Database dei Plugin — Formato

Il database viene mantenuto in file JSON e aggiornato con ogni release:

```json
{
  "plugin_id": "fabfilter_proq3",
  "display_name": "FabFilter Pro-Q 3",
  "vendor": "FabFilter",
  "vst3_guid": "E8E38A93-21B0-4B8D-B7EC-B54B8B9CF48C",
  "version_tested": "3.22",
  "parameters": [
    {
      "id": 0,
      "name": "Band1_Frequency",
      "alias": ["freq1", "frequenza banda 1", "eq band 1 freq"],
      "min": 20.0, "max": 20000.0,
      "scale": "logarithmic",
      "unit": "Hz"
    }
  ],
  "ai_presets": {
    "remove_mud": { "Band2_Freq": 220, "Band2_Gain": -3.5, "Band2_Q": 2.1 },
    "add_presence": { "Band4_Freq": 3500, "Band4_Gain": 2.0, "Band4_Q": 1.4 },
    "high_pass_clean": { "Band1_Type": "HP", "Band1_Freq": 40, "Band1_Q": 0.7 }
  }
}
```

---

## 7. Roadmap Implementazione Plugin Control

```
  FASE ALPHA (ora)
  ─────────────────
  ✓ ParameterMapper base
  ✓ Controllo parametri VST3 via JUCE
  ✓ OscBridge riceve comandi plugin.control
  ○ Database parametri FabFilter Pro-Q3

  FASE BETA
  ─────────────────
  ○ Database completo FabFilter (Pro-Q3, Pro-C2, Pro-L2)
  ○ Database completo iZotope (Ozone 11, Neutron 4)
  ○ Auto-discovery parametri (scan automatico)
  ○ UI per browsing parametri plugin attivi
  ○ Undo stack completo

  FASE 1.0
  ─────────────────
  ○ Database 100+ plugin mappati
  ○ MIDI Learn automatico
  ○ Preset AI per ogni plugin
  ○ A/B comparison automatico

  FASE 2.0
  ─────────────────
  ○ Plugin API pubblica per sviluppatori terzi
  ○ Community database (mapping crowd-sourced)
  ○ Machine learning da preferenze utente
```

---

*→ Continua in: [Paper 04 — Sistema di Memoria dell'Agente](04-SISTEMA-MEMORIA-AGENTE.md)*
