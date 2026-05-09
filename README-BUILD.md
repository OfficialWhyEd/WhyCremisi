# WhyCremisi VST Bridge AI - Build Instructions

**Cross-platform build guide for Linux, Windows, and macOS.**

---

## 📋 Prerequisites (All Platforms)

### Required Tools
- **CMake** >= 3.20
- **JUCE** >= 7.0.12 (download from [juce.com](https://juce.com/get-juce))
- **Git**

### Platform-Specific Prerequisites

#### Linux (Ubuntu/Debian)
```bash
# JUCE dependencies
sudo apt update
sudo apt install libasound2-dev libx11-dev libxrandr-dev libxinerama-dev \
    libxcursor-dev libgl-dev libfreetype6-dev libxcomposite-dev \
    mesa-common-dev libgl1-mesa-dev

# NEW: cURL for AI HTTP requests
sudo apt install libcurl4-openssl-dev

# GTK and WebKit (for WebView support)
sudo apt install libgtk-3-dev libwebkit2gtk-4.1-dev
```

#### Windows (Visual Studio)
1. **Visual Studio 2022** with "Desktop development with C++" workload
2. **vcpkg** (Microsoft's C++ package manager)
   ```cmd
   # Install vcpkg (one time)
   git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
   C:\vcpkg\bootstrap-vcpkg.bat
   
   # Install cURL
   C:\vcpkg\vcpkg.exe install curl:x64-windows-static
   ```
3. Set environment variable:
   ```cmd
   setx VCPKG_ROOT "C:\vcpkg"
   ```

#### macOS
```bash
# Using Homebrew
brew install cmake curl

# JUCE dependencies usually already present on macOS
```

---

## 🔧 Setup

### 1. Clone Repository
```bash
git clone https://github.com/OfficialWhyEd/VST-PlugIn-Ai.git
cd VST-PlugIn-Ai
```

### 2. Download and Setup JUCE

Download JUCE from [juce.com/get-juce](https://juce.com/get-juce) and extract to:

| Platform | Recommended Path |
|----------|------------------|
| Linux | `/home/carlo/SDKs/JUCE` |
| Windows | `C:\SDKs\JUCE` |
| macOS | `/Users/edo/SDKs/JUCE` |

Set environment variable:
```bash
# Linux/macOS
export JUCE_ROOT="/home/carlo/SDKs/JUCE"

# Windows (in Command Prompt)
setx JUCE_ROOT "C:\SDKs\JUCE"
```

---

## 🏗️ Build

### Linux
```bash
export JUCE_ROOT=/home/carlo/SDKs/JUCE
cmake -B build -DJUCE_ROOT=$JUCE_ROOT
cmake --build build --config Release

# Install VST3 to user directory
cp build/WhyCremisiVSTPlugin_artefacts/Release/VST3/WhyCremisiVSTPlugin.vst3 ~/.vst3/
```

### Windows (with vcpkg)
```cmd
# Open "Developer Command Prompt for VS 2022"

# Configure with vcpkg toolchain
cmake -B build -G "Visual Studio 17 2022" -A x64 ^
    -DJUCE_ROOT=C:\SDKs\JUCE ^
    -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake

# Build
cmake --build build --config Release

# VST3 location (copy to your DAW's VST3 folder)
build\WhyCremisiVSTPlugin_artefacts\Release\VST3\WhyCremisiVSTPlugin.vst3
```

### macOS
```bash
export JUCE_ROOT=/Users/edo/SDKs/JUCE
cmake -B build -DJUCE_ROOT=$JUCE_ROOT -G "Xcode"
cmake --build build --config Release

# Install VST3
cp -R build/WhyCremisiVSTPlugin_artefacts/Release/VST3/WhyCremisiVSTPlugin.vst3 ~/Library/Audio/Plug-Ins/VST3/
```

---

## 🧪 Test

### Verify Build
```bash
# Linux/macOS - list built files
ls -la build/WhyCremisiVSTPlugin_artefacts/Release/

# Windows - check VST3 was created
dir build\WhyCremisiVSTPlugin_artefacts\Release\VST3\
```

### Load in DAW
1. Open Ableton Live, Reaper, or other VST3-compatible DAW
2. Scan/rescan VST3 plugins
3. Load "WhyCremisi VST Bridge AI"
4. Check that the GUI appears (800x600 window with knobs)

---

## 🐛 Troubleshooting

### "JUCE not found"
- Verify `JUCE_ROOT` environment variable
- Check that path exists: `ls $JUCE_ROOT` (Linux/macOS) or `dir %JUCE_ROOT%` (Windows)

### "CURL not found" (Linux)
```bash
sudo apt install libcurl4-openssl-dev
# Verify: pkg-config --exists curl && echo "OK"
```

### "CURL not found" (Windows)
- Ensure vcpkg is installed and `VCPKG_ROOT` is set
- Re-run: `vcpkg install curl:x64-windows-static`
- Use full path to vcpkg.cmake in CMake command

### Build Errors
```bash
# Clean rebuild
rm -rf build
cmake -B build ...
cmake --build build
```

---

## 📝 For Heartbroken (Edo's AI Assistant)

### What Changed Recently (Aura Branch)
1. **cURL Added**: Now required for AI HTTP requests (Ollama, Gemini, etc.)
2. **OscHandler**: Now bidirectional - can send AND receive OSC messages
3. **AiEngine**: HTTP infrastructure implemented (cURL-based)

### Your Tasks (UI Development)
1. Set up Windows build environment (see above)
2. Test that plugin loads in Ableton Live
3. Continue React/WebView UI development in `src/ui/`
4. Communicate any build issues to Carlo/Aura

### Communication Protocol
- **Git commits**: Start with "Heartbroken: " (e.g., "Heartbroken: Added GainSlider component")
- **Before push**: Check WORKFLOW.md rules
- **Issues**: Comment on GitHub or notify Carlo

---

## 🔄 Continuous Integration (Future)

GitHub Actions will be set up to:
- Build on Linux (Ubuntu), Windows (VS2022), macOS (Xcode)
- Run unit tests
- Package VST3 releases

---

## 📞 Support

- **Carlo (Linux)**: Build issues, backend C++, OSC
- **Edo (Windows)**: Build issues, UI testing, Ableton integration
- **Aura**: Documentation, code review, architecture

---

*Last updated: 11 April 2026*
*Branch: aura (Carlo/Aura development)*
