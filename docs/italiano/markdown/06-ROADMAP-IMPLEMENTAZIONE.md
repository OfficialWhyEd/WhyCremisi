# Paper 06 — Roadmap di Implementazione
## Fasi, Milestone e Obiettivi Tecnici

```
────────────────────────────────────────────────────────────────
  WHYCREMISI RESEARCH PAPERS — N.06
  Roadmap di Implementazione Completa
  
  "Ogni fase costruisce la successiva."
────────────────────────────────────────────────────────────────
```

**Categoria:** Pianificazione Strategica

---

## Fase Alpha — Fondamenta (Completata ✓)

```
  ✓ Plugin VST3 + Standalone funzionante su macOS
  ✓ Bridge WebSocket React ↔ C++ (porta 8080)
  ✓ AI streaming multi-provider (Ollama, Gemini, OpenAI)
  ✓ BotFace animata con 9 stati emotivi
  ✓ BoxChat contestuale (8 tipi)
  ✓ SessionPanel + Flight Recorder
  ✓ AgentWorkspace + PersonalityCore + OpenClaw Memory
  ✓ DSP Engine (Analyzer, Compressor, EQ, Limiter)
  ✓ SetupScreen configurazione AI provider
  ✓ Advisory card con Execute/Dismiss
  ✓ Test suite integrazione WebSocket
```

---

## Fase Beta — Controllo Plugin (In corso)

```
  ○ ParameterMapper completato con database FabFilter
  ○ Database iZotope Ozone 11 + Neutron 4
  ○ UI browser parametri plugin attivi
  ○ Undo stack per ogni azione plugin
  ○ Learning loop da feedback utente
  ○ Pattern detection multi-sessione
  ○ Build Windows (VST3 + Standalone)
  ○ Build Linux (VST3)
```

---

## Fase 1.0 — Prodotto Completo

```
  ○ 100+ plugin mappati nel database
  ○ Auto-discovery parametri (scan VST3)
  ○ A/B comparison automatico
  ○ Reference track analysis
  ○ Multi-DAW: Ableton, Logic, Reaper, FL Studio
  ○ INSTALLER PULITO win/mac/linux (tutto incluso, zero dipendenze)
  ○ Sito web + documentazione pubblica
  ○ Versione trial 30 giorni + licenza commerciale
```

---

## Fase 2.0 — Ecosistema

```
  ○ Plugin API pubblica
  ○ Community database mapping
  ○ Collaborazione multi-agente
  ○ Mobile companion app (monitoring)
  ○ Cloud sync profilo utente (opt-in)
```

---

## Installer Zero-Dipendenze — Specifica Completa

Chiunque riceva WhyCremisi deve avere **tutto** funzionante al primo avvio. Nessun download aggiuntivo, nessuna dipendenza esterna, nessuna configurazione manuale.

### Cosa deve essere incluso nell'installer

```
  BUNDLE WHYCREMISI — CONTENUTO COMPLETO
  
  ┌─────────────────────────────────────────────────────────┐
  │  CORE PLUGIN                                            │
  │  ✓ WhyCremisi.vst3      (macOS / Windows / Linux)      │
  │  ✓ WhyCremisi.component (AU — macOS/Logic only)        │
  │  ✓ WhyCremisi.app       (Standalone macOS)             │
  │  ✓ WhyCremisi.exe       (Standalone Windows)           │
  │  ✓ WhyCremisi           (Standalone Linux)             │
  ├─────────────────────────────────────────────────────────┤
  │  AI ENGINE LOCALE (incluso, non richiede internet)     │
  │  ✓ Ollama runtime       (binario per ogni OS)          │
  │  ✓ Modello AI base      (llama3.2:3b, ~2GB)            │
  │  ✓ Modello AI avanzato  (llama3.1:8b, ~5GB) [opt-in]  │
  ├─────────────────────────────────────────────────────────┤
  │  RUNTIME INCLUSI                                        │
  │  ✓ WebView2 runtime     (Windows — embedded)           │
  │  ✓ VC++ Redistributable (Windows — embedded)           │
  │  ✓ React UI bundle      (già compilato, offline)       │
  │  ✓ WebSocket server     (già in binario JUCE)          │
  ├─────────────────────────────────────────────────────────┤
  │  DATABASE PLUGIN                                        │
  │  ✓ plugin-database.json (100+ plugin mappati)          │
  │  ✓ ai-presets.json      (preset intelligenti)          │
  ├─────────────────────────────────────────────────────────┤
  │  ASSETS E CONFIGURAZIONE                                │
  │  ✓ Icone app (tutti i formati — vedi Paper 07)         │
  │  ✓ Config default       (pronta all'uso)               │
  │  ✓ License.txt + EULA   (inclusa)                      │
  └─────────────────────────────────────────────────────────┘
```

### Installer per piattaforma

```
  macOS
  ─────
  Formato:    WhyCremisi-v1.0.pkg  (installer nativo)
  Installa:   /Library/Audio/Plug-Ins/VST3/WhyCremisi.vst3
              /Library/Audio/Plug-Ins/Components/WhyCremisi.component
              /Applications/WhyCremisi.app
              ~/Library/Application Support/WhyCremisi/ (dati utente)
  Firma:      Apple Developer ID firmato + notarizzato
  Requisiti:  macOS 12 Monterey o superiore, Apple Silicon o Intel
  
  Windows
  ───────
  Formato:    WhyCremisi-v1.0-setup.exe  (NSIS o Inno Setup)
  Installa:   C:\Program Files\Common Files\VST3\WhyCremisi.vst3
              C:\Program Files\WhyCremisi\WhyCremisi.exe
              %APPDATA%\WhyCremisi\ (dati utente)
  Firma:      Certificato EV code signing
  Requisiti:  Windows 10 64-bit o superiore
  
  Linux
  ─────
  Formato:    whycremisi-v1.0.tar.gz + script install.sh
              whycremisi-v1.0.deb  (Debian/Ubuntu)
              whycremisi-v1.0.rpm  (Fedora/RHEL)
  Installa:   ~/.vst3/WhyCremisi.vst3
              ~/Applications/WhyCremisi
  Requisiti:  Ubuntu 22.04+ / Fedora 38+, X11 o Wayland
```

### Comportamento al primo avvio

```
  1. Plugin caricato nella DAW (o Standalone aperto)
  2. SetupScreen appare automaticamente
  3. Scelta: AI locale (Ollama incluso) o API cloud
  4. Se locale: Ollama si avvia automaticamente (già incluso)
  5. Test connessione automatico
  6. Pronto all'uso in < 30 secondi
  
  NESSUN download aggiuntivo
  NESSUNA registrazione obbligatoria
  NESSUNA dipendenza esterna
  FUNZIONA offline (con AI locale)
```

---

*Fine della Roadmap. Vedere Paper 07 per asset e brand.*

*→ Continua in: [Paper 07 — Asset e Brand Identity](07-ASSET-BRAND.md)*
