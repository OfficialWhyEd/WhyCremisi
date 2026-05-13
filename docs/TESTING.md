# 🧪 TEST GUIDE - WhyCremisi VST Bridge AI

**Per:** Edo e Heartbroken  
**Ultimo aggiornamento:** 10 Aprile 2026  
**Commit testato:** `dd21959`

---

## 📋 Prerequisiti Windows

### 1. Software richiesto
- **Visual Studio 2022** con "Desktop development with C++"
- **CMake** >= 3.20 (https://cmake.org/download/)
- **Git** (https://git-scm.com)
- **JUCE 7.0.12+** (https://juce.com/get-juce)

### 2. Setup JUCE
```
1. Scarica JUCE da https://juce.com/get-juce
2. Estrai in C:\SDKs\JUCE
3. Imposta variabile ambiente:
   - Windows: JUCE_ROOT=C:\SDKs\JUCE
```

### 3. Setup variabile ambiente
```cmd
setx JUCE_ROOT "C:\SDKs\JUCE"
```

---

## 🔨 Build Windows

### Step 1: Clone repository
```cmd
git clone https://github.com/OfficialWhyEd/VST-PlugIn-Ai.git
cd VST-PlugIn-Ai
```

### Step 2: Pull ultime modifiche
```cmd
git pull origin master
```

### Step 3: Configura CMake
```cmd
mkdir build
cd build
cmake .. -DJUCE_ROOT=C:\SDKs\JUCE
```

### Step 4: Build
```cmd
cmake --build . --config Release
```

### Step 5: Trova i file compilati
```
build\WhyCremisiVSTPlugin_artefacts\Release\VST3\WhyCremisi VST Bridge AI.vst3\
build\WhyCremisiVSTPlugin_artefacts\Release\Standalone\WhyCremisi VST Bridge AI.exe
```

---

## 🎹 Installa in DAW

### Ableton Live
1. Copia `WhyCremisi VST Bridge AI.vst3` in:
   - `C:\Program Files\Common Files\VST3\`
   - O in una cartella personalizzata
2. Apri Ableton
3. Vai su **Options → Plug-ins → VST Plug-in System**
4. Clicca **Rescan** o aggiungi la cartella VST3
5. Il plugin appare sotto **VST3 → WhyCremisi**

### Reaper
1. Copia `WhyCremisi VST Bridge AI.vst3` in `C:\Program Files\Common Files\VST3\`
2. Apri Reaper
3. Vai su **Options → Preferences → Plug-ins → VST**
4. Clicca **Re-scan** o **Clear cache/re-scan**
5. Crea una traccia, clicca **FX**, cerca "WhyCremisi"

---

## 🧪 Test Standalone

### Lancio
```cmd
cd build\WhyCremisiVSTPlugin_artefacts\Release\Standalone
WhyCremisi VST Bridge AI.exe
```

### Cosa dovresti vedere
- Finestra con titolo "WhyCremisi VST Bridge AI"
- Due slider: **Gain 1** e **Gain 2**
- Campo di testo "Ask the AI about your mix..."
- Bottone "Ask AI"

### Test base
1. Muovi lo slider **Gain 1** → il valore cambia (dB)
2. Clicca "Ask AI" → risposta placeholder: `[Phase 1] AI response placeholder`

---

## 🎚️ Test OSC con DAW

### Step 1: Avvia il plugin nel DAW
- Ableton o Reaper, carica il plugin su una traccia

### Step 2: Configura OSC in Reaper

**Reaper → Options → Preferences → Control/OSC/Web**

| Opzione | Valore |
|---------|--------|
| Mode | `Configure device IP + local port` |
| Device name | `WhyCremisi` |
| Device IP | `127.0.0.1` |
| Local port | `9000` |
| Allow binding messages | ✅ ON |

Clicca **OK**.

### Step 3: Apri Control Surface Browser

In Reaper:
1. **Options → Preferences → Control/OSC/Web**
2. Doppio-click sulla riga "WhyCremisi"
3. Clicca **"Open browser..."** o vai su **Actions → Control Surface Browser**

### Step 4: Fai Learn OSC

1. Nel **Control Surface Browser**, trova la colonna "Action"
2. Clicca su una riga vuota
3. Muovi un fader del volume in Reaper
4. Reaper associa automaticamente `/track/1/volume`
5. Il plugin dovrebbe ricevere il messaggio OSC

### Step 5: Verifica ricezione OSC

**Attualmente il plugin logga in console**, ma in questa versione Phase 1 non c'è un display visibile.

Per vedere i log:
- **Linux:** Avvia Reaper da terminale, i log appaiono in console
- **Windows:** I log vanno in Debug console (non visibile senza debugger)

**Per Heartbroken:** Nella prossima versione, i messaggi OSC appariranno nella UI React.

---

## 🐛 Troubleshooting Windows

### Build error: "JUCE_ROOT not found"
```cmd
set JUCE_ROOT=C:\SDKs\JUCE
cmake .. -DJUCE_ROOT=C:\SDKs\JUCE
```

### Build error: "Visual Studio not found"
Installa Visual Studio 2022 con "Desktop development with C++".

### Plugin non appare in Ableton/Reaper
1. Verifica che il file `.vst3` sia in `C:\Program Files\Common Files\VST3\`
2. Fai **Rescan** nelle preferenze DAW
3. Se non appare, riavvia il DAW

### OSC non funziona
1. Verifica che la porta **9000** non sia occupata da altri programmi
2. Verifica che il firewall permetta connessioni localhost
3. Il plugin deve essere caricato in una traccia attiva

---

## ✅ Checklist Test Completo

| Test | Stato | Note |
|------|-------|------|
| Build Windows | ⬜ | Con Visual Studio 2022 |
| Standalone si avvia | ⬜ | Doppio click su .exe |
| GUI visibile | ⬜ | Slider, bottoni |
| VST3 carica in DAW | ⬜ | Ableton o Reaper |
| OSC riceve messaggi | ⬜ | Phase 1: log in console |

---

## 📞 Contatti

**Se ci sono problemi:**
- Aura (AI di Carlo): chiedi a Carlo
- Documentazione: `Documentazione/` nel repo
- GitHub Issues: https://github.com/OfficialWhyEd/VST-PlugIn-Ai/issues

---

**Buon testing!** 🚀