# 04-SPECIFICHE TECNICHE - Architettura e Protocolli

**Documento:** Scelte tecnologiche, architettura, dettagli protocolli  
**Target:** Team di sviluppo (tecnico)  
**Stato:** Draft - richiede review Edo/Carlo

---

## 🎯 Decisioni Architetturali Critiche

### 1. Linguaggio di Programmazione: C++ (JUCE)

**Opzioni Valutate:**

| Linguaggio | Pro | Contro | Verdetto |
|------------|-----|--------|----------|
| **C++ + JUCE** | Standard industria, 1000+ plugin, performance max | Steep learning curve, memory unsafe | ✅ **SCELTO** |
| **Rust + baseplug** | Memory safe, modern, no crashes | Ecosistema VST immaturo, poca documentazione | ❌ Rischioso per MVP |
| **C# + NAudio** | Semplice, .NET tooling | Solo Windows/macOS, performance audio non ottimale | ❌ Non cross-platform audio |
| **JavaScript (Node.js)** | Veloce sviluppo, familiarità | Latenza audio inaccettabile, non è plugin VST | ❌ Non fattibile |

**Motivazione C++:**
- JUCE è lo standard de-facto (Ableton, Arturia, FabFilter lo usano)
- Performance audio garantita (< 1ms processing latency)
- Supporto completo VST3/AU/AAX
- Ecosistema maturo (forum, training, codebase esistenti)
- Può caricare WebView per UI (React + Tauri)

**Setup JUCE:**
```cpp
// PluginProcessor.h
class WhyCremisiAudioProcessor : public juce::AudioProcessor
{
public:
    WhyCremisiAudioProcessor();
    ~WhyCremisiAudioProcessor() override;
    
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    
private:
    // OSC handler
    std::unique_ptr<OscHandler> oscHandler;
    
    // AI engine
    std::unique_ptr<AiEngine> aiEngine;
    
    // Parameters
    juce::AudioProcessorValueTreeState parameters;
};
```

---

### 2. UI Framework: React + Tauri (via WebView)

**Stack UI:**
```
┌─────────────────────────────────────┐
│  Plugin VST3 (C++/JUCE)             │
│  └─ WebView (Chromium)              │
│     └─ React App                    │
│        ├─ State: Zustand            │
│        ├─ UI: Tailwind CSS          │
│        └─ Animations: Framer Motion │
└─────────────────────────────────────┘
```

**Perché non JUCE nativo:**
- JUCE UI è C++ → sviluppo lento, complesso
- React → sviluppo rapido, hot reload, componenti riutilizzabili
- Team può lavorare su UI senza conoscere C++

**Build System:**
```bash
# Build C++ core
juce::build --target vst3 --config Release

# Build React UI
cd gui && npm run build

# Bundle together
./scripts/bundle.sh
```

**Comunicazione C++ ↔ React:**
```cpp
// C++ side
void sendToWebView(const juce::var& data)
{
    if (auto* editor = dynamic_cast<WhyCremisiEditor*>(getActiveEditor()))
    {
        editor->sendMessageToWebView(data);
    }
}

// JS side (React)
window.addEventListener('message', (event) => {
    const data = JSON.parse(event.data);
    // Update React state
});
```

---

### 3. AI Engine: Multi-Provider con API Key

**Architettura:**
```
User input
    ↓
[AI Router - Provider Selection]
    ↓
┌──────────────┬──────────────┬──────────────┬──────────────┐
│ Ollama Local │  Gemini API  │  Anthropic   │   OpenAI     │
│ (default)    │  (API key)   │  (API key)   │  (API key)   │
└──────────────┴──────────────┴──────────────┴──────────────┘
    ↓
┌──────────────┬──────────────┬──────────────┐
│ OpenRouter   │    Groq      │   Altri      │
│ (API key)    │  (API key)   │  (API key)   │
└──────────────┴──────────────┴──────────────┘
    ↓
[Memory Layer]
    ↓
Response to user
```

**Provider Supportati:**

| Provider | Tipo | API Key | Modelli Consigliati |
|----------|------|---------|-------------------|
| **Ollama** | Locale | N/A | `llama3.1:8b`, `llama3.2`, `mistral` |
| **Gemini** | Cloud | `GEMINI_API_KEY` | `gemini-1.5-flash`, `gemini-1.5-pro` |
| **Anthropic** | Cloud | `ANTHROPIC_API_KEY` | `claude-3-5-sonnet`, `claude-3-haiku` |
| **OpenAI** | Cloud | `OPENAI_API_KEY` | `gpt-4o-mini`, `gpt-4o`, `gpt-4-turbo` |
| **OpenRouter** | Cloud | `OPENROUTER_API_KEY` | Accesso a 100+ modelli |
| **Groq** | Cloud | `GROQ_API_KEY` | `llama-3.1-70b`, `mixtral-8x7b` |

**Configurazione API Key:**
```json
{
  "ai": {
    "defaultProvider": "ollama",
    "fallbackOrder": ["ollama", "gemini", "anthropic", "openai"],
    "providers": {
      "ollama": {
        "enabled": true,
        "baseUrl": "http://localhost:11434",
        "model": "llama3.1:8b"
      },
      "gemini": {
        "enabled": true,
        "apiKey": "${GEMINI_API_KEY}",
        "model": "gemini-1.5-flash"
      },
      "anthropic": {
        "enabled": true,
        "apiKey": "${ANTHROPIC_API_KEY}",
        "model": "claude-3-5-sonnet"
      },
      "openai": {
        "enabled": true,
        "apiKey": "${OPENAI_API_KEY}",
        "model": "gpt-4o-mini"
      },
      "openrouter": {
        "enabled": true,
        "apiKey": "${OPENROUTER_API_KEY}",
        "model": "anthropic/claude-3.5-sonnet"
      },
      "groq": {
        "enabled": true,
        "apiKey": "${GROQ_API_KEY}",
        "model": "llama-3.1-70b-versatile"
      }
    }
  }
}
```

**Implementazione C++:**
```cpp
class AiEngine
{
public:
    enum class Provider { Ollama, Gemini, Anthropic, OpenAI, OpenRouter, Groq };
    
    struct Config {
        Provider provider;
        juce::String apiKey;
        juce::String model;
        juce::String baseUrl;
        int timeoutMs = 10000;
    };
    
    AiEngine(const Config& config);
    
    // Send prompt to AI
    juce::String sendPrompt(const juce::String& prompt);
    
    // Set active provider
    void setProvider(Provider provider);
    
    // Get available providers
    juce::StringArray getAvailableProviders() const;
    
private:
    Config config;
    std::unique_ptr<juce::URLSession> session;
    
    // Provider-specific implementations
    juce::String callOllama(const juce::String& prompt);
    juce::String callGemini(const juce::String& prompt);
    juce::String callAnthropic(const juce::String& prompt);
    juce::String callOpenAI(const juce::String& prompt);
    juce::String callOpenRouter(const juce::String& prompt);
    juce::String callGroq(const juce::String& prompt);
};
```

**UI per Gestione API Key:**
- Settings panel con campi per ogni provider
- API key salvate in config sicuro (non in chiaro)
- Test button per verificare connessione
- Fallback automatico se provider non disponibile

**Modelli Consigliati:**
- **Primario:** `llama3.1:8b` (Ollama locale) - 4GB RAM, 200ms response
- **Fallback:** `gemini-1.5-flash` (Gemini) - cloud, sub-second
- **Backup:** `gpt-4o-mini` (OpenAI) - cloud, fallback

**System Prompt:**
```
You are an AI audio assistant. The user is using Ableton Live.
Current parameters:
- Parameter 1: {name}, value: {value}, range: {min}-{max}
- Parameter 2: ...

Rules:
1. Be concise (max 2 sentences)
2. Never suggest dangerous values (gain > +6dB, ratio > 10:1)
3. Ask for confirmation before applying changes
4. Explain your reasoning

User question: {user_input}
```

**Memory Context:**
```json
{
  "session_id": "uuid",
  "conversation": [...],
  "parameters_history": [
    {"param": "gain", "from": -12, "to": -6, "by": "user", "at": 1234567890}
  ],
  "user_patterns": [
    {"pattern": "gain_boost_before_eq", "frequency": 0.7}
  ]
}
```

---

## 🔌 Protocollo OSC (Open Sound Control)

### Overview
OSC è protocollo UDP-based per controllo audio. Ableton Live supporta OSC via Max4Live (non nativo).

### Setup Ableton Live
1. Install Max4Live (incluso in Live Suite)
2. Caricare device "OSC Send" e "OSC Receive"
3. Configurare porta: 9000 (default)

### OSC Address Pattern Standard

#### Track Controls
```
/track/1/volume f 0.75      # Track 1 volume (0.0 to 1.0)
/track/1/pan i 64            # Track 1 pan (0-127)
/track/1/mute b 1            # Track 1 mute (0/1)
/track/1/solo b 0            # Track 1 solo
/track/1/arm b 1             # Track 1 record arm
```

#### Device Parameters
```
/track/1/device/1/param/1 f 0.5   # Device 1, param 1
/track/1/device/1/bypass b 0      # Bypass device
```

#### Transport
```
/transport/play t              # Play
/transport/stop t              # Stop
/transport/record t            # Record
/transport/position f 120.5    # Position in seconds
/tempo f 128.0                 # BPM
```

#### Custom Messages (per WhyCremisi)
```
/whycremisi/plugin/name s "FabFilter Pro-Q 3"
/whycremisi/plugin/parameters i 24
/whycremisi/plugin/param/1/name s "Frequency"
/whycremisi/plugin/param/1/value f 1000.0
/whycremisi/plugin/param/1/min f 20.0
/whycremisi/plugin/param/1/max f 20000.0
```

### Implementazione C++

```cpp
class OscHandler
{
public:
    OscHandler(int port = 9000)
    {
        socket.bindTo(port);
        socket.addListener(this);
    }
    
    void sendMessage(const juce::String& address, const juce::var& value)
    {
        osc::OutboundPacketStream p(buffer, 1024);
        p << osc::BeginMessage(address.toRawUTF8());
        
        if (value.isFloat())
            p << value.toString().getFloatValue();
        else if (value.isInt())
            p << (int)value;
        else if (value.isString())
            p << value.toString().toRawUTF8();
        
        p << osc::EndMessage;
        socket.send(p.Data(), p.Size());
    }
    
    void oscMessageReceived(const osc::ReceivedMessage& message) override
    {
        juce::String address = message.AddressPattern();
        
        // Parse parameters
        for (auto it = message.ArgumentsBegin(); it != message.ArgumentsEnd(); ++it)
        {
            if (it->IsFloat())
                handleParamChange(address, it->AsFloat());
            else if (it->IsInt32())
                handleParamChange(address, (float)it->AsInt32());
            else if (it->IsString())
                handleParamName(address, it->AsString());
        }
    }
    
private:
    UdpTransmitSocket socket;
    char buffer[1024];
};
```

---

## 🎵 Protocollo MIDI (Fallback)

### Why MIDI?
- Supporto universale in tutti i DAW
- Fallback se OSC non disponibile
- Compatibilità con hardware controllers

### Limitazioni
- Solo 128 CC (Control Change) messages
- Risoluzione 7-bit (0-127) vs OSC float
- No string messages
- No custom addressing

### MIDI CC Standard

| CC # | Name | Use Case |
|------|------|----------|
| 1 | Modulation Wheel | General purpose |
| 7 | Volume | Track volume |
| 10 | Pan | Stereo position |
| 11 | Expression | Dynamics |
| 64 | Sustain Pedal | On/off |
| 74 | Brightness | Filter cutoff |

### Mappatura WhyCremisi MIDI

```cpp
// MIDI CC → OSC Address translation
const std::map<int, juce::String> midiToOsc = {
    {1, "/track/1/volume"},
    {2, "/track/1/pan"},
    {3, "/track/1/device/1/param/1"},
    // ... mapping configurabile
};

void handleMidiCC(int ccNumber, int value)
{
    float normalizedValue = value / 127.0f;
    juce::String oscAddress = midiToOsc[ccNumber];
    
    // Forward to OSC handler
    oscHandler.sendMessage(oscAddress, normalizedValue);
    
    // Update UI
    sendToWebView({
        {"type", "param_change"},
        {"address", oscAddress},
        {"value", normalizedValue},
        {"source", "midi"}
    });
}
```

---

## 🎛️ VST3 Parameters (Primario)

### Vantaggio VST3
- **Native integration** - DAW vede i parametri automaticamente
- **No setup OSC/MIDI** - funziona out-of-the-box
- **Automation** - DAW può automatizzare i parametri
- **Preset management** - DAW gestisce preset

### Implementazione

```cpp
class WhyCremisiAudioProcessor : public juce::AudioProcessor
{
public:
    WhyCremisiAudioProcessor()
    {
        // Define parameters
        parameters.createAndAddParameter(
            std::make_unique<juce::AudioParameterFloat>(
                "gain",           // parameter ID
                "Gain",           // name
                -60.0f, 12.0f, 0.0f, // range
                "dB"              // unit
            )
        );
        
        parameters.createAndAddParameter(
            std::make_unique<juce::AudioParameterFloat>(
                "frequency",
                "Frequency",
                20.0f, 20000.0f, 1000.0f,
                "Hz"
            )
        );
        
        parameters.createAndAddParameter(
            std::make_unique<juce::AudioParameterBool>(
                "bypass",
                "Bypass",
                false
            )
        );
    }
    
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override
    {
        auto gainParam = parameters.getRawParameterValue("gain");
        float gain = Decibels::decibelsToGain(gainParam->load());
        
        // Apply gain
        buffer.applyGain(gain);
    }
};
```

### Comunicazione VST3 → OSC

Quando DAW modifica parametro VST3, invia OSC per sync:

```cpp
void parameterChanged(const juce::String& parameterID, float newValue)
{
    juce::String oscAddress = "/whycremisi/plugin/" + parameterID;
    oscHandler.sendMessage(oscAddress, newValue);
    
    // Update React UI
    sendToWebView({
        {"type", "param_change"},
        {"address", oscAddress},
        {"value", newValue},
        {"source", "vst3"}
    });
}
```

---

## 🏗️ Struttura Progetto (C++)

```
src/
├── core/
│   ├── PluginProcessor.cpp/h       # Main VST3 plugin
│   ├── PluginEditor.cpp/h          # GUI wrapper
│   └── WhyCremisiEngine.cpp/h        # Main engine
├── osc/
│   ├── OscHandler.cpp/h            # OSC send/receive
│   ├── OscMessage.cpp/h            # Message parsing
│   └── OscAddress.cpp/h            # Address pattern matching
├── midi/
│   ├── MidiHandler.cpp/h           # MIDI CC handling
│   └── MidiMapping.cpp/h           # CC → OSC mapping
├── ai/
│   ├── AiEngine.cpp/h              # AI orchestration
│   ├── OllamaClient.cpp/h          # Ollama API
│   ├── GeminiClient.cpp/h          # Gemini API
│   └── MemoryStore.cpp/h           # Conversation memory
├── ui/
│   └── WebViewBridge.cpp/h         # C++ ↔ React bridge
└── utils/
    ├── Logger.cpp/h
    └── Config.cpp/h
```

---

## 📦 Build System

### CMake (Primary)
```cmake
cmake_minimum_required(VERSION 3.20)
project(WhyCremisi-VST-Bridge-AI)

# JUCE
add_subdirectory(JUCE)

# Plugin
juce_add_plugin(WhyCremisiVSTBridgeAI
    COMPANY_NAME "WhyCremisi"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    PLUGIN_MANUFACTURER_CODE OpCl
    PLUGIN_CODE OcAI
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "WhyCremisi VST Bridge AI"
)

target_sources(WhyCremisiVSTBridgeAI
    PRIVATE
        src/core/PluginProcessor.cpp
        src/core/PluginEditor.cpp
        src/osc/OscHandler.cpp
        src/ai/AiEngine.cpp
)

target_link_libraries(WhyCremisiVSTBridgeAI
    PRIVATE
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        oscpack  # OSC library
        nlohmann_json  # JSON for AI comms
)
```

### Build Script
```bash
#!/bin/bash
# build.sh

# Build C++ core
cmake -B build
cmake --build build --config Release

# Build React UI
cd gui && npm run build && cd ..

# Bundle
./scripts/bundle_vst.sh
```

---

## 🛠️ Setup Sviluppo Ambiente

### Prerequisiti Cross-Platform
- CMake >= 3.20
- JUCE 7.0.12+ (download da juce.com)
- Compilatore:
  - Linux: `gcc` (>= 11) o `clang` (>= 14)
  - macOS: Xcode Command Line Tools
  - Windows: Visual Studio 2022

### Prerequisiti Specifici Linux (Ubuntu/Debian)

⚠️ **Nota:** Questi prerequisiti sono per sistemi Linux (come il sistema di sviluppo attuale). Per Windows o macOS, vedere le sezioni successive.

**⚠️ IMPORTANTE - VST2 SDK Rimosso:**
Steinberg ha rimosso VST2 SDK dalle distribuzioni recenti. Il plugin supporta solo **VST3** (standard moderno). Ableton Live, Reaper e tutti i DAW moderni supportano VST3.

**Pacchetti APT necessari per JUCE:**
```bash
sudo apt update
sudo apt install libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libgl-dev \
    libfreetype6-dev libxcomposite-dev mesa-common-dev libgl1-mesa-dev libcurl4-openssl-dev
```

**Verifica installazione:**
```bash
# Dovrebbero mostrare versione installata
dpkg -l | grep libasound2-dev
dpkg -l | grep libx11-dev
dpkg -l | grep libgl-dev
```

**Note:**
- `libasound2-dev` → ALSA audio backend (necessario per audio I/O)
- `libx11-dev` → X11 windowing system (base per GUI)
- `libxrandr-dev` → X11 RandR extension (per monitor/resolution handling)
- `libxinerama-dev` → X11 Xinerama extension (per multi-monitor)
- `libxcursor-dev` → X11 cursor extension (per custom cursor in UI)
- `libgl-dev` → OpenGL (per rendering GUI accelerato)
- `libfreetype6-dev` → Font rendering per JUCE GUI
- `libxcomposite-dev` → X11 composite extension (per trasparenza)
- `mesa-common-dev` → Mesa headers per OpenGL
- `libgl1-mesa-dev` → Mesa OpenGL development
- `libcurl4-openssl-dev` → HTTPS per download e API calls

**Se mancanti:** Il build fallirà con errori tipo "cannot find -lasound" o "X11/Xlib.h: No such file or directory"

### Prerequisiti macOS
- Xcode Command Line Tools:
  ```bash
  xcode-select --install
  ```

### Prerequisiti Windows (Sistema di Edo)

⚠️ **Nota:** Questo è l'ambiente di sviluppo principale per Edo.

**Prerequisiti necessari:**
1. **Visual Studio 2022** con "Desktop development with C++"
2. **Windows SDK** (incluso in Visual Studio)
3. **CMake** >= 3.20 (download da cmake.org)
4. **JUCE 7.0.12+** (download da juce.com)

**Installazione Visual Studio:**
- Download da: https://visualstudio.microsoft.com/
- Selezionare "Desktop development with C++" workload
- Includere Windows SDK

**Setup JUCE:**
1. Estrai in `C:\SDKs\JUCE` o percorso preferito
2. Imposta variabile ambiente: `JUCE_ROOT=C:\SDKs\JUCE`

**Verifica installazione:**
```cmd
cmake --version
gcl --version
```

---

### Step 1: Download e Setup JUCE (1 ora)

#### Linux/macOS
1. Scarica JUCE da https://juce.com/get-juce
2. Estrai in `/home/carlo/SDKs/JUCE` (Linux) o `/Users/edo/SDKs/JUCE` (macOS)
3. Configura environment:
   ```bash
   export JUCE_ROOT="/home/carlo/SDKs/JUCE"  # Linux
   # oppure
   export JUCE_ROOT="/Users/edo/SDKs/JUCE"  # macOS
   ```

#### Windows (Edo)
1. Scarica JUCE da https://juce.com/get-juce
2. Estrai in `C:\SDKs\JUCE` o percorso preferito
3. Imposta variabile ambiente:
   - Apri "System Properties" > "Environment Variables"
   - Aggiungi nuova variabile utente:
     - Nome: `JUCE_ROOT`
     - Valore: `C:\SDKs\JUCE`
   - Oppure da Command Prompt (amministratore):
     ```cmd
     setx JUCE_ROOT "C:\SDKs\JUCE" /M
     ```

---

### Step 2: Setup Progetto CMake (1 giorno)

#### Linux/macOS
Crea `CMakeLists.txt` nella root:

```cmake
cmake_minimum_required(VERSION 3.20)
project(WhyCremisi-VST-Plugin)

# JUCE
add_subdirectory(${JUCE_ROOT} JUCE)

# IMPORTANTE: VST2 SDK non è più disponibile (Steinberg rimosso)
# Usiamo solo VST3 (standard moderno supportato da tutti i DAW)

# Plugin
juce_add_plugin(WhyCremisiVSTPlugin
    COMPANY_NAME "WhyCremisi"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    PLUGIN_MANUFACTURER_CODE OpCl
    PLUGIN_CODE OcAI
    FORMATS VST3 Standalone      # VST3 only - no VST2
    PRODUCT_NAME "WhyCremisi VST Bridge AI"
)

# Disabilita VST2 fallback (richiede SDK non disponibile)
target_compile_definitions(WhyCremisiVSTPlugin PRIVATE
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_sources(WhyCremisiVSTPlugin
    PRIVATE
        src/core/PluginProcessor.cpp
        src/core/PluginEditor.cpp
        src/osc/OscHandler.cpp
        src/ai/AiEngine.cpp
)

target_link_libraries(WhyCremisiVSTPlugin
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        oscpack
        nlohmann_json
)

# Include directories
target_include_directories(WhyCremisiVSTPlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/osc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ai
)
```

#### Windows (Edo)
Lo stesso `CMakeLists.txt` funziona, ma:
- Assicurarsi che `JUCE_ROOT` sia impostato come variabile ambiente
- Usare Visual Studio 2022 per aprire la soluzione generata

---

### Step 3: Creare Struttura Cartelle (30 minuti)

#### Linux/macOS
```bash
mkdir -p src/{core,osc,ai,ui,utils}
```

#### Windows (Edo)
```cmd
mkdir src\core
mkdir src\osc
mkdir src\ai
mkdir src\ui
mkdir src\utils
```

---

### Step 4: Plugin Skeleton (2 giorni)

#### `src/core/PluginProcessor.h`
```cpp
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../osc/OscHandler.h"
#include "../ai/AiEngine.h"

class WhyCremisiAudioProcessor : public juce::AudioProcessor
{
public:
    WhyCremisiAudioProcessor();
    ~WhyCremisiAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "WhyCremisi VST Bridge AI"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

private:
    std::unique_ptr<OscHandler> oscHandler;
    std::unique_ptr<AiEngine> aiEngine;
    juce::AudioProcessorValueTreeState parameters;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WhyCremisiAudioProcessor)
};
```

#### `src/core/PluginProcessor.cpp`
```cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"

WhyCremisiAudioProcessor::WhyCremisiAudioProcessor()
    : parameters(*this, nullptr, "Parameters", createParameterLayout())
{
    oscHandler = std::make_unique<OscHandler>(9000);
    aiEngine = std::make_unique<AiEngine>();
}

WhyCremisiAudioProcessor::~WhyCremisiAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout WhyCremisiAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "gain1", "Gain 1", -60.0f, 12.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "gain2", "Gain 2", -60.0f, 12.0f, 0.0f));
    // ... altri 6 parametri

    return { params.begin(), params.end() };
}

void WhyCremisiAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    oscHandler->start();
}

void WhyCremisiAudioProcessor::releaseResources()
{
    oscHandler->stop();
}

void WhyCremisiAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Apply gain from parameter 1
    auto* gainParam = parameters.getRawParameterValue("gain1");
    buffer.applyGain(juce::Decibels::decibelsToGain(gainParam->load()));
}

void WhyCremisiAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void WhyCremisiAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr && xmlState->hasTagName(parameters.state.getType()))
    {
        parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}

juce::AudioProcessorEditor* WhyCremisiAudioProcessor::createEditor()
{
    return new WhyCremisiAudioProcessorEditor(*this);
}
```

### Step 5: Build e Test (1 giorno)

#### Linux
```bash
# Configure
cmake -B build -DJUCE_ROOT="/home/carlo/SDKs/JUCE"

# Build
cmake --build build --config Release

# Test
# Carica build/WhyCremisiVSTPlugin_artefacts/VST3/WhyCremisiVSTPlugin.vst3 in Ableton/Reaper
```

#### Windows (Edo)
```cmd
# Configure (da Developer Command Prompt per VS2022)
cmake -B build -DJUCE_ROOT=C:\SDKs\JUCE -G "Visual Studio 17 2022"

# Build
cmake --build build --config Release

# Test
# Carica build\WhyCremisiVSTPlugin_artefacts\VST3\WhyCremisiVSTPlugin.vst3 in Ableton/Reaper
```

#### macOS
```bash
# Configure
cmake -B build -DJUCE_ROOT=/Users/edo/SDKs/JUCE -G "Xcode"

# Build
cmake --build build --config Release

# Test
# Carica build/WhyCremisiVSTPlugin_artefacts/VST3/WhyCremisiVSTPlugin.vst3 in Ableton/Logic Pro
```

---

### Step 6: OSC Handler Stub (3 giorni)
Crea `src/osc/OscHandler.h` e `OscHandler.cpp` con implementazione base.

### Step 7: AI Engine Stub (2 giorni)
Crea `src/ai/AiEngine.h` e `AiEngine.cpp` con connessione Ollama.

---

## 📊 Timeline Setup Completa

| Step | Durata | Deliverable |
|------|--------|-------------|
| Prerequisiti | 1 ora | Toolchain installato |
| Setup CMake | 1 giorno | CMakeLists.txt funzionante |
| Skeleton plugin | 2 giorni | Plugin carica in DAW |
| Build & test | 1 giorno | VST3 funzionante |
| OSC stub | 3 giorni | Comunicazione OSC base |
| AI stub | 2 giorni | Connessione Ollama |
| **Total** | **~10 giorni** | **Plugin base funzionante** |

---

## 🚀 Prossimi Step Dopo Setup

1. **UI React** (1 settimana)
2. **Auto-mapping** (1 settimana)
3. **Control history** (1 settimana)
4. **Animations** (3 giorni)

---

*Questa sezione è da aggiungere a `04-SPECIFICHE-TECNICHE.md`.*

## 📊 Performance Targets

| Metric | Target | Test |
|--------|--------|------|
| CPU overhead (idle) | < 1% | Task Manager |
| CPU overhead (active) | < 5% | PluginDoctor |
| Latency OSC | < 5ms | Loopback test |
| Latency AI response | < 200ms | Local benchmark |
| Memory usage | < 100MB | Process monitor |
| Load time | < 500ms | Stopwatch |

---

## 🔒 Security Considerations

### AI Safety
- Rate limiting: max 1 request/sec per param
- Value clamping: -60dB to +12dB per gain
- Confirmation required: AI non applica > ±3dB senza conferma
- Timeout: 10 secondi max per AI response

### OSC Security
- Bind solo a localhost (127.0.0.1)
- No OSC messages da network esterno
- Validate tutti i messaggi OSC (no buffer overflow)

---

*Questo documento richiede review tecnica da Edo prima di implementazione.*
