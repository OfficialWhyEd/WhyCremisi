# WhyCremisi VST Bridge AI

**⚠️ IMPORTANTE: Leggi prima `WORKFLOW.md` per le regole di collaborazione tra Aura e Heartbroken.**

AI-powered VST3 plugin for DAW control via OSC and MIDI.

## Quick Start

### Prerequisites (Linux)
```bash
sudo apt install libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libgl-dev
```

### Build
```bash
cd /home/carlo/progetti/Vst-plugin-AI
export JUCE_ROOT=/home/carlo/SDKs/JUCE
cmake -B build -DJUCE_ROOT=$JUCE_ROOT
cmake --build build --config Release
```

### Install Plugin
```bash
# VST3 location
cp build/WhyCremisiVSTPlugin_artefacts/VST3/WhyCremisiVSTPlugin.vst3 ~/.vst3/
```

## Project Structure

```
├── CMakeLists.txt          # CMake configuration
├── src/
│   ├── core/               # PluginProcessor, PluginEditor
│   ├── osc/                # OSC communication handler
│   ├── ai/                 # AI Engine (multi-provider)
│   ├── ui/                 # UI components (WebView bridge)
│   └── utils/              # Utility functions
└── Documentazione/         # Full documentation
```

## Features

- **VST3/AU Plugin** for Ableton Live, Reaper, etc.
- **OSC Control** for parameter automation
- **Multi-Provider AI**: Ollama, Gemini, Anthropic, OpenAI, OpenRouter, Groq
- **WebView UI** with React frontend
- **MIDI CC Mapping** for hardware controllers

## License

MIT License - See LICENSE file
