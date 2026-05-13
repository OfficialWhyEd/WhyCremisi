# 07-CHANGELOG - Version History

**Documento:** Storico modifiche e versioni  
**Formato:** Keep a Changelog v1.0.0  
**URL:** https://keepachangelog.com/en/1.0.0/

---

## [Unreleased] - 2026-04-14

### Added
- Initial project setup
- Documentation structure (12 files)
- Competitive analysis (TouchOSC, MIDI Agent, Ableton MCP, ReaLearn)
- Technical specifications (C++ + JUCE, React + WebSocket)
- Protocol specifications (OSC, WebSocket, JSON v1.0)
- Requirements document (functional + non-functional)
- Roadmap with 6 phases (21 weeks)
- **VST3 plugin core** with JUCE (compiles on Linux)
- **Standalone build** format
- **OscHandler** — bidirectional OSC UDP (RX :9000, TX :9001)
- **WebSocketServer** — RFC 6455 completo con SHA1 inline (no OpenSSL dep)
- **OscBridge** — dispatcher bidirezionale OSC↔WebSocket
- **whycremisi-bridge.js** — client React con useWhyCremisi hook, auto-reconnect
- **AiEngine** — supporto Ollama locale (HTTP)
- **WebViewBridge** — bridge C++↔JS (fallback per WebView interna)
- **Fallback UI** — JUCE nativa quando WebView non disponibile (Linux)
- **Protocollo JSON v1.0** — specifica completa C++↔JavaScript
- **Architettura Ponte OSC-Web** — UI fuori processo via WebSocket
- **Test suite Python** — 11/11 test protocollo passati
- **AiEngine v2** — implementazione HTTP POST completa per tutti i provider
- **AiEngine multi-provider** — Ollama, Gemini, Anthropic, OpenAI, OpenRouter, Groq
- **OscBridge config AI** — configurazione provider/model/API key via WebSocket
- **Ollama cloud support** — URL configurabile (non solo localhost)
- **Session checkpoint** — documento ripresa per sessione successiva (14/04/2026)

### Changed
- AiEngine: ora usa `juce::URL` con `withPOSTData()` per request body reali
- AiEngine: JSON escape manuale per prompt (no external deps)
- AiEngine multi-provider: tutti i provider cloud richiedono API key (check esplicito)

### In Progress (Non committato)
- AiEngine ↔ OscBridge integration in PluginProcessor.cpp (80% — manca collegamento callback finale)
- Test end-to-end DAW ↔ Browser (bloccato da decisione prossima sessione)

### Notes
- File modificati localmente ma non committati: AiEngine.cpp, AiEngine.h, OscBridge.cpp/.h
- Checkpoint sessione: `STATUS.md` sezione "Sessione 14/04 - Riepilogo Chiusura"
- Prossima azione: completare integrazione AI o test end-to-end (da decidere)

---

## [1.0.0] - 2026-07-01 (Planned)
- Pivot from external bridge to VST3 plugin
- Primary DAW target: Reaper (tested) + Ableton (planned)
- UI framework: React + WebSocket (was WebView integrata, crashava su Linux)
- AI engine: Ollama local + fallback cloud
- **WebView → fallback**: WebKit/GTK crasha in VST, usato architettura ponte
- **Standalone build format** aggiunto a CMakeLists
- **OscBridge** usa callback invece di ereditarietà OscCallback

### Deprecated
- Node.js bridge architecture (old version in /progetti/WhyCremisi-VST-Bridge)
- WebView integrata (crasha su Linux, mantenuta come fallback)

### Removed
- VST2 support (obsolete)
- 32-bit support
- Electron UI option (too heavy)
- **Duplicato COLLABORAZIONE_HB.md** (consolidato in 08-COLLABORAZIONE-HB.md)

### Fixed
- WebView crash su Linux (architettura ponte)
- Build order JUCE modules
- Plugin product name senza spazi
- OscBridge: callback pattern invece di operator() ereditato

### Security
- OSC bind only to localhost
- WebSocket bind only to localhost
- API keys encrypted in config
- AI cannot execute system commands

---

## [1.0.0] - 2026-07-01 (Planned)

### Added
- VST3 plugin core
- GUI with 8 knobs
- OSC bidirectional communication
- AI chat panel (Ollama)
- Auto-mapping with learn mode
- Control history (1000 entries)
- Session save/load (.claw format)
- 10 preset prompts
- Basic animations (200ms ramp)
- Tutorial (5 steps)
- Dark/light theme

### Platform Support
- Windows 10+ (x64)
- macOS 11+ (Intel & Apple Silicon)
- Linux Ubuntu 22.04+ (x64)

### DAW Support
- Ableton Live 12+ (primary)
- Reaper 7+ (tested)
- Logic Pro 11+ (planned)
- FL Studio 21+ (planned)

### AI Models
- Ollama local: llama3.1:8b (default)
- Gemini: gemini-1.5-flash (fallback)
- OpenAI: gpt-4o-mini (backup)

### Performance
- CPU overhead: < 2% idle, < 5% active
- Latency OSC: < 5ms
- Latency AI: < 3s (local)
- Load time: < 500ms
- Memory: < 100MB

---

## [1.1.0] - 2026-09-01 (Planned)

### Added
- Real-time audio analysis (FFT)
- Pattern learning AI (beta)
- 5 additional preset prompts
- MIDI learn functionality
- Custom skin support
- Export history to CSV

### Improved
- Animation smoothness (60fps guaranteed)
- AI response time (caching)
- OSC message batching

### Fixed
- Bug: Knob animation interrupt
- Bug: OSC message loss under heavy load
- Bug: AI timeout not handled gracefully

---

## [1.5.0] - 2026-12-01 (Planned)

### Added
- VST plugin hosting (experimental)
- Mobile companion app (iOS/Android)
- Cloud sync (AWS S3)
- Prompt marketplace (beta)
- Reference track matching AI

### Changed
- UI framework upgraded to React 19
- JUCE upgraded to v8.0
- Ollama models: llama3.2 (faster)

### Removed
- Support for macOS 10.15 (EOL)

---

## [2.0.0] - 2027-03-01 (Planned)

### Added
- Multi-agent AI system (4 agents)
- Pattern learning AI (stable)
- VR interface (experimental)
- Hardware controller support (Push, Launchpad)
- AI voice control (beta)
- DMX lighting control

### Changed
- Architecture: microservices (core + AI + UI)
- Pricing: subscription model introduced
- Licensing: per-user instead of per-machine

### Deprecated
- Standalone version (use plugin only)

---

## Release Notes Format

Each release includes:
- **Download links** for all platforms
- **SHA256 checksums** for verification
- **Migration guide** from previous version
- **API changes** (if any)
- **Known issues** with workarounds
- **Credits** to contributors

---

## Versioning Scheme

Format: `MAJOR.MINOR.PATCH`

- **MAJOR**: Breaking changes (architecture, API)
- **MINOR**: New features (backwards compatible)
- **PATCH**: Bug fixes only

Examples:
- `2.0.0`: Complete rewrite
- `1.5.0`: New features (cloud sync, mobile app)
- `1.1.2`: Bug fix release

---

## Support Lifecycle

| Version | Release Date | End of Support | End of Life |
|---------|--------------|----------------|-------------|
| 1.0.x | 2026-07-01 | 2027-07-01 | 2028-07-01 |
| 1.1.x | 2026-09-01 | 2027-09-01 | 2028-09-01 |
| 1.5.x | 2026-12-01 | 2027-12-01 | 2028-12-01 |
| 2.0.x | 2027-03-01 | 2028-03-01 | 2029-03-01 |

**Support:** Bug fixes + security updates  
**End of Life:** No updates, download still available

---

## Backwards Compatibility

- Plugin v1.x carica preset v1.y (y <= x)
- Plugin v2.x non carica preset v1.x (breaking change)
- Session files: convert tool provided per major upgrades
- OSC protocol: stable within major version
- API: stable within minor version

---

## Known Issues Tracking

### Issue Template
```
ID: OC-XXX
Title: Brief description
Severity: Critical/Major/Minor
Affected: Version(s)
Workaround: Steps to avoid/mitigate
Fixed in: Version (if applicable)
```

### Example
```
ID: OC-001
Title: AI timeout not handled on Windows
Severity: Major
Affected: 1.0.0-1.0.2
Workaround: Restart plugin
Fixed in: 1.0.3
```

---

## Credits

### Core Team
- Edo (Founder, Product Vision)
- Carlo (Advisor, Project Management)
- Aura (AI Assistant, Documentation)

### Contributors
- [To be added]

### Third-Party
- JUCE (ROLI Ltd.)
- React (Meta)
- Tauri (Tauri Contributors)
- Ollama (Ollama Inc.)
- Tailwind CSS (Tailwind Labs)

---

## Download Archive

### Latest Release
- **Version:** [Unreleased]
- **Date:** TBD
- **Download:** Not yet available

### Previous Releases
- None yet

---

## Update Check

Plugin checks for updates on startup (opt-in).

**Endpoint:** `https://api.whycremisi.io/v1/check-update`  
**Method:** `GET`  
**Response:**
```json
{
  "latest_version": "1.0.0",
  "download_url": "https://download.whycremisi.io/v1.0.0/",
  "changelog_url": "https://docs.whycremisi.io/changelog",
  "urgent": false
}
```

---

*This changelog is maintained automatically. Last updated: 2026-04-14*
