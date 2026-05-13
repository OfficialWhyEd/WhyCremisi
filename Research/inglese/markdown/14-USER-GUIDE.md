# Paper 14 — User Guide
## Quick Start, Interface and Workflows

```
────────────────────────────────────────────────────────────────
  WHYCREMISI RESEARCH PAPERS — N.14
  User Guide — Quick Start
  
  "AI doesn't get configured. It gets used."
────────────────────────────────────────────────────────────────
```

**Version:** 1.0  
**Category:** User Manual  
**Platforms:** macOS 12+ · Windows 10+ · Linux (Wine/Ubuntu Studio)

---

## 1. Introduction

### 1.1 What WhyCremisi Is

WhyCremisi is an **AI-powered VST3/AU plugin bridge** that connects your DAW to an AI agent capable of listening, analyzing, and controlling every parameter of every plugin in your session in real time.

| What It DOES | What It DOES NOT |
|-------------|------------------|
| Listens to audio signals in real time | Does not replace the producer |
| Controls any VST3/AU plugin parameter | Does not generate standalone music |
| Analyzes spectrum, phase, dynamics, LUFS | Does not load non-existent plugins |
| Proposes and applies effect chains | Does not alter audio routing |
| Remembers preferences and past sessions | Does not record session audio |
| Communicates via text and voice commands | Does not bypass DAW security |

### 1.2 System Requirements

```
┌──────────────────────────┬────────────────────────────────────┐
│ Component                │ Requirement                        │
├──────────────────────────┼────────────────────────────────────┤
│ Operating System         │ macOS 12+ / Windows 10+ / Linux    │
│                          │ (Wine 8+ or Ubuntu Studio 22.04+)  │
│ DAW                      │ Ableton Live 11+ / Reaper 6+ /    │
│                          │ Logic Pro / FL Studio 21+ /        │
│                          │ Pro Tools 2023+                    │
│ Plugin Formats           │ VST3 / AU (macOS)                  │
│ Internet                 │ Required for AI features           │
│ RAM                      │ 8 GB recommended (16 GB for        │
│                          │ large sessions)                    │
│ Disk Space               │ 500 MB for installation            │
└──────────────────────────┴────────────────────────────────────┘
```

**[NOTE]** WhyCremisi works offline for plugin control and audio analysis, but requires an internet connection for AI features. When offline, the interface switches to "analysis-only" mode.

---

## 2. Installation

### 2.1 Installation Procedure

```
Step 1: Download the latest release from whycremisi.com/download
        └── File: WhyCremisi_x.x.x.dmg (macOS) / .exe (Windows)

Step 2: Mount the DMG (macOS) or run the installer (Windows)
        └── Drag WhyCremisi.app into the Applications folder

Step 3: Launch WhyCremisi (it will appear in the system tray)
        └── Verify the WebSocket service is listening
            (default port 9800)

Step 4: Configure your DAW to detect the plugin
        └── See §2.2 for each specific DAW
```

### 2.2 DAW Activation

**Ableton Live 11+**
```
1. Open Ableton → Preferences → Plug-Ins
2. Under "Plugin Sources" add: /Library/Audio/Plug-Ins/VST3
3. Click "Rescan" (or "Rescan All" to clear cache)
4. Drag "WhyCremisi" from the Plugin Browser onto a track
```

**Reaper 6+**
```
1. Open Reaper → Preferences → VST
2. Under "VST plugin paths" add: /Library/Audio/Plug-Ins/VST3
3. Click "Clear cache / rescan"
4. FX → "WhyCremisi" → Add
```

**Logic Pro**
```
1. Open Logic → Preferences → Audio → Plug-In Manager
2. Verify WhyCremisi is listed under "AU Instruments"
3. Click "Reset & Rescan Selection" if missing
4. Insert on an instrument track: Audio FX → WhyCremisi
```

### 2.3 WebSocket and OSC Configuration

```
┌─────────────────────────────────────────────────────────────┐
│  Connection Settings (Settings → Network)                   │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  WebSocket Server    ws://localhost:9800  [●] Active         │
│  WebSocket Password  ************         [Change]           │
│  OSC Server          osc.udp://localhost:9000  [●] Active   │
│  OSC Input Port      9000                                   │
│  OSC Output Port     9001                                   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**[NOTE]** The default WebSocket (9800) and OSC (9000-9001) ports are predefined. If they conflict with other services, change them in Settings → Network.

---

## 3. First Connection

### 3.1 Launching the Plugin

```
  1. Insert WhyCremisi on a track in your DAW
  2. The main interface opens automatically
  3. Observe the status indicators in the top bar
```

### 3.2 Connection Status

The status bar displays three color-coded indicators:

```
┌──────────────────────────────────────────────────────────────┐
│  Connection Status                     ┌─────┐ ┌─────┐ ┌───┐ │
│                                        │ DAW │ │ WS  │ │ AI│ │
│   ● All systems operational            │  ●  │ │  ●  │ │ ● │ │
│                                        └─────┘ └─────┘ └───┘ │
└──────────────────────────────────────────────────────────────┘
```

| Indicator | Color | Meaning |
|-----------|-------|---------|
| DAW | ● Green | Plugin detected and synced with host |
| DAW | ● Yellow | Plugin loaded but not synced |
| DAW | ● Red | Plugin not detected — check installation |
| WS (WebSocket) | ● Green | WebSocket connection active |
| WS (WebSocket) | ● Red | WebSocket disconnected — restart service |
| AI | ● Green | AI agent connected and operational |
| AI | ● Yellow | AI processing |
| AI | ● Red | AI unavailable — check internet connection |

### 3.3 First Command

```
  —In the AI chat, type or speak a simple command—
  
  User > "Hello, analyze the current track"
  
  WhyCremisi > Analysis complete. Here is a summary:
              
              ● Integrated LUFS:  -18.3  (ideal for mixing)
              ● True Peak:        -2.1 dBTP
              ● Dynamic Range:    12.4 dB
              ● LRA (Loudness Range):  8.7 dB
              
              The track has good dynamics. 
              Would you like me to suggest improvements?
```

**[NOTE]** If the AI does not respond within 5 seconds, check your internet connection in Settings → Network → Test Connection.

---

## 4. Main Interface

```
┌──────────────────────────────────────────────────────────────────────┐
│  WHYCREMISI — Main Interface                       [—] [□] [×]      │
├──────────────────────────────────────────────────────────────────────┤
│  ┌──────────────────────────────────────────────────────────────────┐│
│  │  ▶  ■  ●  ⏸  ⏮  ⏭    |  BPM: 128  |  Time: 01:23:45          ││
│  │  TRANSPORT BAR                                         [PREFS] ││
│  └──────────────────────────────────────────────────────────────────┘│
├────────────────────────────────┬──────────────────────────────────┬──┤
│ ┌──────────────────────────┐  │ ┌──────────────────────────────┐ │  │
│ │  LEVEL METERS            │  │ │  SPECTROGRAM (FFT)           │ │  │
│ │  ┌─┐ ┌─┐                │  │ │  ┌────────────────────────┐  │ │  │
│ │  │█│ │█│  L: -18.3 LUFS │  │ │  │  ░░▒▒▓▓██▓▓▒▒░░       │  │ │  │
│ │  │█│ │█│  R: -18.1 LUFS │  │ │  │  ░░▒▒▓▓██▓▓▒▒░░       │  │ │  │
│ │  │█│ │█│  Peak L: -2.1  │  │ │  │  20Hz ─── 20kHz       │  │ │  │
│ │  └─┘ └─┘  Peak R: -2.4  │  │ │  └────────────────────────┘  │ │  │
│ ├──────────────────────────┤  │ ├──────────────────────────────┤ │  │
│ │  CORRELATION METER       │  │ │  OSCILLOSCOPE                │ │  │
│ │     ╱‾‾‾╲               │  │ │  ┌────────────────────────┐  │ │  │
│ │    ╱  +1  ╲              │  │ │  │   ~~~~~~~~~~          │  │ │  │
│ │   ╱   ●    ╲      +0.92  │  │ │  │  ~~        ~~        │  │ │  │
│ │   ╲        ╱             │  │ │  │ ~~          ~~       │  │ │  │
│ │    ╲  -1  ╱              │  │ │  └────────────────────────┘  │ │  │
│ │     ╲___╱                │  │ │                              │ │  │
│ └──────────────────────────┘  │ └──────────────────────────────┘ │  │
├────────────────────────────────┴──────────────────────────────────┴──┤
│  ┌──────────────────────────────────────────────────────────────────┐│
│  │  AI CHAT                                  [Send] [🎤] [⚙]    ││
│  │  ┌────────────────────────────────────────────────────────────┐││
│  │  │ User: "Compress the vocals"                              │││
│  │  │ WhyCremisi: Applied compressor with ratio 3:1, attack    │││
│  │  │ 10ms, release 50ms, threshold -18dB. Listen.             │││
│  │  └────────────────────────────────────────────────────────────┘││
│  └──────────────────────────────────────────────────────────────────┘│
├──────────────────────────────────────────────────────────────────────┤
│  ┌──────────────────────┐ ┌──────────────────────────────────────┐  │
│  │  PLUGIN BROWSER      │ │  PRESET MANAGER                      │  │
│  │  ○ FabFilter Pro-Q3  │ │  ┌────────────────────────────────┐  │  │
│  │  ○ iZotope Ozone 11  │ │  │ Vocal Mix — Current            │  │  │
│  │  ○ Waves SSL-G       │ │  │ Drum Bus — Loaded              │  │  │
│  │  ○ > show all (23)   │ │  │ [Save Current] [Browse...]     │  │  │
│  └──────────────────────┘ └──────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────────┘
```

### 4.1 Transport Bar

| Command | Icon | Description |
|---------|------|-------------|
| Play | ▶ | Starts playback |
| Stop | ■ | Stops playback |
| Record | ● | Starts recording |
| Pause | ⏸ | Pauses playback |
| Rewind | ⏮ | Goes to start |
| Fast Forward | ⏭ | Goes to end |

The transport bar also displays the current BPM and session timecode.

### 4.2 Level Meters

Stereo indicators with dual readout:

```
  L [████████████░░░░░░░░] -18.3 LUFS  Peak: -2.1 dBTP
  R [█████████████░░░░░░░] -18.1 LUFS  Peak: -2.4 dBTP
```

- **Green bar:** safe level (below -6 dBTP)
- **Yellow bar:** warm level (-6 to -1 dBTP)
- **Red bar:** clipping (above -1 dBTP)

### 4.3 Correlation Meter

Measures phase correlation between L and R channels:

```
  +1  ─── ● ───  Perfect phase (mono compatible)
   0  ────────   No correlation
  -1  ────────   Out of phase (problematic)
```

Ideal values: +0.5 to +1.0 for balanced mixes.

### 4.4 Oscilloscope

Real-time waveform display. Useful for:
- Identifying transients (drums, percussion)
- Visualizing saturation and clipping
- Comparing pre/post processing dynamics

### 4.5 Spectrogram (FFT)

Frequency analysis with selectable resolution (512-8192 bins):

| Frequency Band | Range | Primary Use |
|----------------|-------|-------------|
| Sub-bass | 20-60 Hz | Kick, subwoofer |
| Bass | 60-250 Hz | Bass, fundamentals |
| Low-mid | 250-500 Hz | Instrument body, mud |
| Mid | 500-2000 Hz | Vocals, guitar, presence |
| Upper-mid | 2000-4000 Hz | Presence, sibilance |
| High | 4000-20000 Hz | Air, brilliance, hi-hat |

### 4.6 AI Chat Window

```
┌─────────────────────────────────────────────────────────────┐
│  AI CHAT WINDOW                      [Send] [🎤] [⚙]      │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  [History]                                                  │
│  >>> "Analyze the master bus"                              │
│  <<< Master: LUFS -14.2, TP -1.3dB. True Peak is high.    │
│      I suggest lowering the limiter threshold.              │
│                                                             │
│  [Quick Suggestion]                                        │
│  ● Add sidechain compression to the kick and bass          │
│  ● Apply a gentle high-pass on the reverb send             │
│  ● Compare mix with reference track                        │
│                                                             │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ Type a command or ask a question...                  │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

### 4.7 Plugin Browser

Lists all VST3/AU plugins detected in the current session. Each plugin shows:
- Name and manufacturer
- Installed version
- Status (loaded/inactive)
- Exposed parameters (click to expand)

### 4.8 Preset Manager

Save and load complete configurations:

```
  Preset: "Vocal_Mix_v2"
  Includes:
    ✓ EQ settings (FabFilter Pro-Q3)
    ✓ Compression (Pro-C2)
    ✓ Reverb (Pro-R)
    ✓ AI chat context
    ✓ LUFS target: -14
```

**[NOTE]** Presets save the state of plugins controlled through WhyCremisi, not the entire DAW session.

---

## 5. Workflows

### 5.1 Assisted Mixing: "Compress the Vocals"

```
  User: "Compress the vocals"

  1. WhyCremisi analyzes the vocal track
     → LUFS: -21.3, Dynamic Range: 14.2 dB
     → Vocal transients: 8.2 dB average peak

  2. WhyCremisi proposes:
     ┌──────────────────────────────────────────────────────┐
     │  Proposal: Compressor on Vocal Track                  │
     │                                                      │
     │  Plugin: FabFilter Pro-C 2 (Clean mode)              │
     │  ● Threshold:   -18.5 dB                             │
     │  ● Ratio:        3.0 : 1                             │
     │  ● Attack:       8 ms                                │
     │  ● Release:      45 ms                               │
     │  ● Makeup:       +2.5 dB                             │
     │  ● Output:       -1.5 dB (headroom)                  │
     │                                                      │
     │  [PREVIEW]  [APPLY]  [MODIFY]                        │
     └──────────────────────────────────────────────────────┘

  3. User clicks [PREVIEW] → A/B listening
  4. User clicks [APPLY]
  5. WhyCremisi logs to history: "Vocals compressed — 3:1 ratio"
```

### 5.2 Mastering: "Apply a Master Chain"

```
  User: "Apply a master chain for Spotify"

  1. WhyCremisi analyzes the master bus
     → LUFS: -18.3, TP: -2.1 dBTP
     → Spotify target: Integrated LUFS -14

  2. Proposed chain:
     ┌──────────────────────────────────────────────────────┐
     │  MASTER CHAIN — Spotify Target                        │
     │                                                      │
     │  Slot 1: FabFilter Pro-Q 3    — Corrective EQ       │
     │  Slot 2: iZotope Ozone 11     — Exciter + Imager    │
     │  Slot 3: FabFilter Pro-C 2    — Stereo compression   │
     │  Slot 4: FabFilter Pro-L 2    — Limiter              │
     │                                                      │
     │  Target: Integrated LUFS -14, TP -1.0 dBTP           │
     │                                                      │
     │  [PREVIEW]  [APPLY]  [MODIFY]                        │
     └──────────────────────────────────────────────────────┘
```

### 5.3 Creative Sound: "70s Analog Synth"

```
  User: "Make this sound like a 70s analog synth"

  1. WhyCremisi analyzes current timbre
  2. Applies effect chain:
     ┌──────────────────────────────────────────────────────┐
     │  CHAIN: "70s Analog Synth"                            │
     │                                                      │
     │  ● Saturn 2          — Tape saturation, warm drive   │
     │  ● Pro-Q 3           — Low-pass filter (-3dB @ 8kHz) │
     │  ● RC-20 Retro Color — Wow & Flutter + Noise         │
     │  ● Pro-R             — Spring reverb (decay: 2.5s)   │
     │                                                      │
     │  [PREVIEW]  [APPLY]  [SAVE AS PRESET]                │
     └──────────────────────────────────────────────────────┘
```

### 5.4 Problem Solving: "Fix the Bass Muddiness"

```
  User: "Fix the bass muddiness"

  1. WhyCremisi analyzes bass track FFT
     → Identifies buildup at 180-350Hz
     → Degraded signal-to-noise ratio in low-mid range

  2. WhyCremisi proposes:
     ┌──────────────────────────────────────────────────────┐
     │  Analysis: Muddiness in the bass                      │
     │                                                      │
     │  Cause: Buildup of frequencies 180-350Hz              │
     │                                                      │
     │  Suggested Solution:                                 │
     │  ● FabFilter Pro-Q 3 — Peak band centered at 240Hz  │
     │  ● Gain: -4.2 dB                                     │
     │  ● Q: 2.5 (narrow)                                   │
     │                                                      │
     │  [A/B PREVIEW]  [APPLY]  [FINE-TUNE]                │
     └──────────────────────────────────────────────────────┘
```

---

## 6. Voice/Text Commands

### 6.1 Command Categories

| Category | Example | Action |
|----------|---------|--------|
| Volume/Gain | "Set reverb send to -12 dB" | Adjust parameter |
| Effects | "Add sidechain compression to the kick and bass" | Create chain |
| Routing | "Create a parallel compression chain on the drums" | Routing + FX |
| Analysis | "Analyze the master bus for clipping" | Detailed report |
| Comparison | "Compare this mix with the reference track" | A/B analysis |
| Preset | "Load the 'Warm Vocal' preset" | Load preset |
| Transport | "Play from bar 24" | DAW control |
| EQ | "High-pass the guitars at 120Hz" | Specific filter |

### 6.2 Detailed Examples

```
Command: "Set reverb to 25%"
───────────────────────────────────────
Result: Reverb Wet/Dry brought from 40% to 25% on the current track.

Command: "Add sidechain compression to the kick and bass"
───────────────────────────────────────
Result: WhyCremisi:
  1. Creates a compressor on the bass track
  2. Sets sidechain input from kick track
  3. Configures: ratio 4:1, attack 1ms, release 50ms
  4. Auto-threshold based on kick level

Command: "Create a parallel compression chain on the drums"
───────────────────────────────────────
Result: WhyCremisi:
  1. Creates "Drums Parallel" return track
  2. Sends from drum track at -10dB
  3. Adds compressor with 30ms attack, high ratio 8:1
  4. Mix at 50%

Command: "Analyze the master bus for clipping"
───────────────────────────────────────
Result: 
  ● True Peak: -0.3 dBTP — Clipping detected!
  ● Clipping samples: 1,247 samples (0.03% of total)
  ● Suggestion: Reduce limiter gain by 1.5dB
                or apply a soft clipper before the limiter.

Command: "Compare this mix with the reference track"
───────────────────────────────────────
Result:
  ┌─────────────────────────────────────────────────────────┐
  │  COMPARISON: Current Mix vs Reference                    │
  │                                                         │
  │  Metric            Mix         Reference    Δ           │
  │  ────────────────────────────────────────────────────   │
  │  Integrated LUFS   -14.2       -14.0        -0.2 OK     │
  │  True Peak         -1.3 dB     -0.8 dB      -0.5 dB     │
  │  LRA               6.2 dB      7.1 dB       -0.9 dB     │
  │  EQ Shape          250Hz +3dB  250Hz flat    → EQ       │
  │  Stereo Width      68%         72%           -4%        │
  └─────────────────────────────────────────────────────────┘
```

---

## 7. Preferences and Settings

### 7.1 Interface

```
┌─────────────────────────────────────────────────────────────┐
│  PREFERENCES — Interface                                     │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ● Theme            ○ Dark  ○ Light  ○ System               │
│                                                             │
│  ● Language         ○ Italiano  ● English                    │
│                                                             │
│  ● Window Size      ○ 100%  ○ 125%  ○ 150%  ● 200%         │
│                                                             │
│  ● Chat Size        ● Normal  ○ Compact                     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 7.2 Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `⌘+Enter` | Send chat command |
| `⌘+K` | Focus chat |
| `⌘+L` | Clear chat history |
| `⌘+M` | Mute/Unmute output |
| `⌘+,` | Open preferences |
| `Space` | Play/Pause DAW |
| `⌘+Z` | Undo last AI action |
| `⌘+Shift+Z` | Redo last action |
| `Esc` | Close modal |

### 7.3 AI Behavior

```
┌─────────────────────────────────────────────────────────────┐
│  PREFERENCES — AI Behavior                                   │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ● Creativity        ○ Low  ● Medium  ○ High                │
│    (low = strictly follows mixing rules)                     │
│    (high = experiments with creative chains)                 │
│                                                             │
│  ● Verbosity         ○ Concise  ● Detailed  ○ Extended      │
│                                                             │
│  ● Auto-apply        ○ Never  ○ After confirm  ● Always    │
│                                                             │
│  ● Volume Alert      ● Active  ○ Disabled                   │
│    (threshold: -1 dBTP)                                     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**[NOTE]** "Auto-apply • Always" mode is not recommended for inexperienced users. WhyCremisi suggests starting with "After confirm".

---

## 8. Troubleshooting

### 8.1 Quick FAQ

```
PROBLEM: Plugin does not appear in the DAW
───────────────────────────────────────────
  Possible causes:
  ● Plugin not properly installed
    → Verify: /Library/Audio/Plug-Ins/VST3/WhyCremisi.vst3
  ● DAW cache not updated
    → Ableton: Rescan All; Reaper: Clear cache/rescan
  ● Format not supported by DAW
    → Logic requires AU (not VST3)
    → FL Studio requires VST3 (not AU)

PROBLEM: No sound after inserting the plugin
─────────────────────────────────────────────
  Possible causes:
  ● WhyCremisi in bypass
    → Disable bypass from plugin header
  ● Incorrect audio routing
    → Verify signal passes through the plugin
  ● Plugin in "analysis-only" mode
    → Check operating mode in settings

PROBLEM: AI is not responding
───────────────────────────────
  1. Check internet connection
  2. Check AI status (● Red?)
  3. Restart the WhyCremisi service
  4. Check firewall (port 443 for AI API)
  5. Restart the DAW

PROBLEM: OSC not working
───────────────────────────
  1. Verify OSC port (9000) is not occupied
  2. Check that the target IP is correct
  3. Reaper: Enable "Control surface OSC" in Preferences
  4. Test: `nc -u localhost 9000` from terminal

PROBLEM: WebSocket disconnected
─────────────────────────────────
  1. Restart the WhyCremisi WebSocket service
  2. Verify port 9800 is free
  3. Check if VPN/proxy interferes with WebSocket
  4. Log: ~/Library/Logs/WhyCremisi/websocket.log
```

### 8.2 Log Viewer

```
  Menu → Help → Log Viewer
  
  ┌────────────────────────────────────────────────────────────┐
  │  LOG VIEWER                            [Clear] [Export]   │
  ├────────────────────────────────────────────────────────────┤
  │  [12:34:01] INF Plugin loaded: WhyCremisi v1.0.0          │
  │  [12:34:02] INF WebSocket connected on port 9800           │
  │  [12:34:03] INF AI agent connected (model: gpt-4o)        │
  │  [12:34:05] DBG Track analysis: LUFS=-21.3, DR=14.2      │
  │  [12:34:10] INF AI suggestion: "Compress the vocals"     │
  │  [12:34:12] ACT Applied: Pro-C2 threshold=-18.5, ratio=3 │
  │  [12:34:15] WRN True Peak approaching limit: -1.3 dBTP   │
  │  [12:34:20] ERR WebSocket connection lost — reconnecting  │
  │  [12:34:22] INF WebSocket reconnected successfully        │
  └────────────────────────────────────────────────────────────┘
  
  Log levels: INF (info), DBG (debug), WRN (warning), ERR (error), ACT (action)
```

---

## 9. Tips and Best Practices

### 9.1 Getting Started

```
  ╔══════════════════════════════════════════════════════════════╗
  ║             5 Tips for New Users                             ║
  ╠══════════════════════════════════════════════════════════════╣
  ║                                                              ║
  ║  1. START WITH A SIMPLE SONG                                 ║
  ║     → 4-8 tracks, few plugins, familiar genre                ║
  ║                                                              ║
  ║  2. ALWAYS USE PREVIEW                                       ║
  ║     → Listen before applying any changes                     ║
  ║     → The [PREVIEW] button changes nothing                   ║
  ║                                                              ║
  ║  3. VERIFY PARAMETERS BEFORE CONFIRMING                     ║
  ║     → Read the values proposed by the AI                     ║
  ║     → Manual tweak if necessary                              ║
  ║                                                              ║
  ║  4. USE PRESETS AS A STARTING POINT                          ║
  ║     → Presets give a solid foundation                        ║
  ║     → Always customize for your specific track               ║
  ║                                                              ║
  ║  5. PROVIDE CONSTANT FEEDBACK TO THE AI                     ║
  ║     → "Too much compression" → AI adjusts                    ║
  ║     → "More presence" → AI adds EQ                          ║
  ║     → More feedback = better results                         ║
  ║                                                              ║
  ╚══════════════════════════════════════════════════════════════╝
```

### 9.2 Ideal Workflow

```
  PHASE 1 — ANALYSIS
  ───────────────────
  Load the song → "Analyze the session"
  WhyCremisi analyzes every track and proposes optimizations

  PHASE 2 — ROUGH MIX
  ─────────────────────
  Apply EQ corrections → "Remove mud from bass"
  Set base levels → "Set all track levels to -18 LUFS"
  Stereo balance → "Widen the pad stereo image"

  PHASE 3 — FINAL MIX
  ─────────────────────
  Track compression → "Compress the drum bus 4:1"
  Effects and reverb → "Add room reverb to the vocals"
  Automations → "Automate the filter cutoff in the bridge"

  PHASE 4 — MASTERING
  ────────────────────
  "Apply a master chain for Spotify"
  Final verification → "Analyze the master bus"
  Export → "Export to WAV 48kHz 24-bit"
```

### 9.3 Effective Feedback

```
  ❌ "I don't like it"              → Too vague
  ✅ "Compression is too much,      → Specific, action-oriented
     reduce ratio to 2:1"

  ❌ "Add effects"                  → Too generic
  ✅ "Add a long-tail reverb        → Specific: type, position
     to the vocals"

  ❌ "Fix the bass"                 → Doesn't say what's wrong
  ✅ "The bass has too much         → Identifies cause + range
     mud at 200-300Hz"
```

### 9.4 Common Mistakes to Avoid

| Mistake | Consequence | Solution |
|---------|-------------|----------|
| Applying without listening | Unexpected results | Always use PREVIEW |
| Commands too vague | Imprecise response | Be specific |
| Ignoring AI warnings | Clipping or phase errors | Read AI messages |
| Not saving presets | Configuration loss | Save preset after each session |
| Too many commands in a row | Processing confusion | Wait for AI response |

---

```
────────────────────────────────────────────────────────────────
  "WhyCremisi is not a plugin: it's a producer in a box."
────────────────────────────────────────────────────────────────
```

*→ Continue: [Paper 15 — API and Plugin Development](15-API-PLUGIN-DEVELOPMENT.md)*
