# 03-ROADMAP-FASI - Piano di Sviluppo Strutturato

**Documento:** Piano di sviluppo a fasi con milestone  
**Approccio:** Critico - smonto le richieste originali e le ricostruisco in modo fattibile  
**Timeline:** 6 mesi per MVP, 12 mesi per v1.0

---

## ⚠️ Analisi Critica delle Richieste Originali

### Richieste Originali (come ricevute da Edo)

1. Controllo remoto da pagina HTML con auto-mapping
2. Monitorare VST/plugin e farli comparire nella pagina
3. AI agent con: chat, interazione controlli, cronologia, animazioni, prompt library, apprendimento stile
4. Demo, tutorial, istruzioni
5. Salvataggio sessioni

### Problemi Identificati (Onesti)

#### ❌ Problema #1: Ambiguità "VST o Plugin o Bridge"
**Richiesta originale:** "un vst o plugin o bridge"  
**Problema:** Sono tre cose diverse con architetture differenti.  
**Soluzione:** Decidere **UNA** cosa:
- Se è plugin VST → si carica nel DAW, controlla solo i parametri esposti
- Se è bridge esterno → controlla tutto via OSC ma non è installabile come plugin
- **Decisione:** Plugin VST3 con comunicazione OSC secondaria

#### ❌ Problema #2: "Monitorare VST e farli comparire nella pagina"
**Richiesta:** Monitorare tutti i VST del progetto  
**Problema:** Un plugin VST non può vedere altri plugin nel DAW. Violerebbe le specifiche VST3.  
**Soluzione:** 
- Come VST: può solo vedere i propri parametri + parametri esposti dal DAW (track volume, pan, send levels)
- Come bridge esterno: può monitorare tutto via OSC ma richiede setup manuale
- **Decisione:** Focus su controllo parametri del plugin stesso + track controls via OSC

#### ❌ Problema #3: "AI che muove i controlli"
**Richiesta:** AI che interagisce con i controlli come una persona  
**Problema:** Se l'AI muove un fader da -∞ a 0dB in un frame, è un "salto", non un movimento. Richiede animazione smooth.  
**Soluzione:** Implementare **ramping** automatico:
```
Parametro: da valore A a valore B in T secondi con curva E
```

#### ❌ Problema #4: Troppi obiettivi per Fase 1
**Richiesta:** Tutte e 5 le funzionalità da subito  
**Problema:** Scope creep. 12+ mesi di sviluppo. Rischio di non finire mai.  
**Soluzione:** Prioritizzazione aggressiva. Fase 1 = controllo base + AI chat. Features avanzate dopo.

---

## 🎯 Piano Ristrutturato a Fasi

### Principio Guida
**"Ship early, ship often. Better to release 20% of features 100% working than 100% of features 20% working."**

---

## FASE 0: Pre-Development Setup (2 settimane)

### Obiettivi
- Setup tooling e ambiente
- Decisioni architetturali definitive
- Design base UI/UX

### Tasks
- [x] Setup repo GitHub (monorepo con submodules)
- [ ] Setup CI/CD (GitHub Actions per build Windows/macOS/Linux)
- [x] Scelta linguaggio definitiva: C++/JUCE + React
- [x] Setup JUCE o equivalente framework
- [ ] Wireframe UI in Figma (minimo 3 pagine: main, settings, AI panel)
- [x] Setup Ableton Live per testing (build Windows completata da Edo)
- [ ] Documentare OSC mapping Ableton via Max4Live
- [x] Decisione architettura: Ponte OSC-Web (UI React in browser separato)
- [x] Protocollo JSON v1.0 definitivo e testato (11/11)

### Deliverable
- Repository configurato con branch protection
- CI che compila per tutte le piattaforme
- Design UI approvato da Edo
- Ambiente di test funzionante

### KPI
- Build time < 5 minuti
- Tutte le piattaforme compilano
- Design approvato

---

## FASE 1: Core Plugin Foundation (6 settimane)

### Obiettivi
- Plugin VST3 che si carica in Ableton
- GUI base funzionante
- Comunicazione OSC bidirezionale

### Tasks

#### Week 1-2: Plugin Skeleton
- [x] Setup progetto JUCE con template VST3
- [x] GUI window base (fallback UI JUCE funzionante)
- [x] Plugin si carica in Reaper senza crash (Linux)
- [x] Build Linux VST3 completata
- [x] Build Windows completata (Edo)
- [ ] Test in Ableton (Edo)

#### Week 3-4: OSC Communication
- [x] OscHandler implementato (bidirezionale UDP :9000→:9001)
- [x] WebSocketServer RFC 6455 (TCP :8080, SHA1 inline)
- [x] OscBridge bidirezionale OSC↔WebSocket
- [x] whycremisi-bridge.js client React con useWhyCremisi hook
- [x] Protocollo JSON v1.0 implementato in C++ e JS
- [ ] Mappatura parametri plugin → OSC address
- [ ] Test con TouchOSC o osc-send tool
- [x] WebViewBridge C++ implementato (fallback per WebView interna)

#### Week 5-6: GUI Base
- [ ] 8 knob parametri con label
- [ ] Valore display per ogni knob
- [ ] Resizable window
- [ ] Settings panel minimo (porta OSC, enable/disable)
- [x] UI web esterna: architettura ponte implementata (React + WebSocket)
- [ ] Heartbroken: setup React + server dev
- [ ] Heartbroken: connessione React a ws://localhost:8080

### Deliverable
- File .vst3 che si carica in Ableton Live
- GUI con 8 knob funzionanti
- OSC bidirezionale verificato

### KPI
- CPU overhead < 2% quando idle
- Latenza OSC < 5ms (loopback test)
- Zero crash in 1 ora di uso continuativo

---

## FASE 2: AI Integration (4 settimane)

### Obiettivi
- AI chat panel laterale
- Comunicazione con Ollama local
- Risposte contestuali ai parametri

### Tasks

#### Week 7-8: AI Backend
- [x] Integrazione Ollama API (localhost:11434) ✅ **14/04/2026**
- [x] AiEngine multi-provider: Ollama, Gemini, Anthropic, OpenAI, OpenRouter, Groq ✅ **14/04/2026**
- [x] Ollama cloud support (URL configurabile) ✅ **14/04/2026**
- [ ] System prompt per contesto audio
- [ ] Memoria conversazione (ultimi 10 messaggi)
- [ ] Error handling (AI non disponibile, timeout)

> **📍 CHECKPOINT 14/04/2026:** AiEngine v2 completo ma non ancora collegato a OscBridge. Manca integrazione finale in PluginProcessor.cpp per dispatch ai.prompt → risposta AI via WebSocket.

#### Week 9-10: AI UI
- [ ] Chat panel laterale (collassabile)
- [ ] Input box con submit button
- [ ] Scrollable message history
- [ ] Typing indicator
- [ ] Clear history button

#### Feature: Comandi AI Base
- [ ] "Qual è il valore del knob 1?" → risposta testuale
- [ ] "Imposta knob 1 a 0.75" → AI invia OSC message
- [ ] "Analizza i parametri attuali" → AI riassume

### Deliverable
- AI chat funzionante nel plugin
- Comandi base per leggere/modificare parametri
- Storia conversazione visibile

### KPI
- Tempo risposta AI < 3 secondi (con Ollama local)
- Accuratezza comandi > 90%
- Zero memory leak in 1 ora di uso

---

## FASE 3: Auto-Mapping & Control History (4 settimane)

### Obiettivi
- Auto-mapping parametri DAW track
- Cronologia modifiche in tempo reale
- UI per visualizzare history

### Tasks

#### Week 11-12: Auto-Mapping
- [ ] Detect OSC messages in ingresso (learn mode)
- [ ] Dialog: "Nuovo controllo rilevato: /track/1/volume. Aggiungere?"
- [ ] Assegnazione automatica a knob disponibile
- [ ] Save mapping in preset

#### Week 13-14: Control History
- [ ] Data structure per ogni modifica (timestamp, parameter, value, source)
- [ ] Real-time log display (scrollable table)
- [ ] Filter by: parameter, source (user/AI), time range
- [ ] Export history to JSON/CSV

### Deliverable
- Auto-mapping funzionante con dialog
- History visibile in UI
- Export funzionante

### KPI
- Latenza detection < 50ms
- History accurate al 100%
- Export completo senza perdite dati

---

## FASE 4: AI Animation & Prompt Library (3 settimane)

### Obiettivi
- Animazioni quando AI muove controlli
- Libreria prompt pre-configurati
- Feedback visivo AI actions

### Tasks

#### Week 15-16: Animations
- [ ] Implementare smooth ramping (valore A → B in T secondi)
- [ ] Knob animation CSS/WebGL
- [ ] AI indicator (pulsing glow quando AI sta agendo)
- [ ] Undo button per azioni AI

#### Week 17: Prompt Library
- [ ] 10 prompt pre-configurati per mixing base:
  - "Boost vocal presence" (high shelf +3dB)
  - "Tighten bass" (low cut + compression)
  - "Add air to mix" (high shelf +2dB)
  - etc.
- [ ] UI per selezionare prompt
- [ ] Edit/Add custom prompts

### Deliverable
- Animazioni fluide sui knob
- Prompt library funzionante
- Feedback visivo AI

### KPI
- Animazione 60fps
- Prompt execution < 500ms
- User satisfaction > 4/5 (internal testing)

---

## FASE 5: Session Management & Export (2 settimane)

### Obiettivi
- Salvataggio/caricamento sessioni
- Preset management
- Export configuration

### Tasks

#### Week 18-19: Session Management
- [ ] Save session file (.claw format - JSON)
- [ ] Load session
- [ ] Recent sessions list
- [ ] Auto-save every 5 minuti

#### Session Content
- Mapping corrente (knob → OSC address)
- AI conversation history
- Control modification history
- Plugin settings

### Deliverable
- File .claw funzionante
- Auto-save implementato
- Load rapido (< 2 secondi)

### KPI
- File size < 500KB per sessione tipica
- Load time < 2 secondi
- Zero data loss

---

## FASE 6: Documentation & Polish (2 settimane)

### Obiettivi
- Tutorial integrato
- Demo video
- Manual

### Tasks

#### Week 20-21: Documentation
- [ ] In-plugin tutorial (onboarding 5 step)
- [ ] Demo video (YouTube, 3 minuti)
- [ ] User manual (PDF, 10 pagine)
- [ ] FAQ document
- [ ] Website landing page

### Deliverable
- Tutorial funzionante nel plugin
- Video pubblico
- Manual disponibile

### KPI
- Onboarding completion rate > 80%
- Support tickets < 10% users (indicatore chiarezza docs)

---

## 🚫 Feature da RIMANDARE (Post v1.0)

### Rimandate a v1.5
- **Pattern learning AI**: Richiede dataset e training. Non fattibile per MVP.
- **Multi-DAW auto-configuration**: Troppo complesso. Focus su Ableton prima.
- **Plugin hosting interno**: Architettura completamente diversa.
- **Cloud sync**: Richiede backend infrastructure.

### Rimandate a v2.0
- **Mobile companion app**: Scope separato.
- **Collaboration features**: Richiede real-time sync architecture.
- **AI training custom**: Richiede ML pipeline.

---

## 📊 Timeline Visuale

```
Week:  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21
       ├───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┤
Fase 0:██
Fase 1: ████████
Fase 2:          ████████
Fase 3:                  ████████
Fase 4:                          ██████
Fase 5:                                  ████
Fase 6:                                      ████
       ├───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┤
       Month 1           Month 2           Month 3           Month 4-5
```

**Total: 21 settimane ≈ 5 mesi**

---

## 🎯 Milestone Critiche

### Milestone 1 (Week 6): "Hello World Plugin"
- Plugin si carica in Ableton
- GUI appare
- OSC funziona

**Checkpoint:** Se questo fallisce, rivedere tecnologia.

### Milestone 2 (Week 10): "AI First Response"
- AI risponde a domanda
- Parametri letti correttamente

**Checkpoint:** Se AI è troppo lenta, valutare modello più piccolo o cloud.

### Milestone 3 (Week 14): "Complete Loop"
- Auto-mapping funziona
- History registrata
- AI modifica parametri

**Checkpoint:** Questo è il MVP core. Se funziona, siamo a buon punto.

### Milestone 4 (Week 21): "Ship Ready"
- Tutti i test passano
- Docs completi
- Demo ready

**Checkpoint:** Ready for beta release.

---

## 📈 Success Metrics per Fase

### Fase 1
- Build success rate: 100%
- Load time: < 500ms
- Zero crashes in 100 loads

### Fase 2
- AI response time: < 3s (p95)
- Command accuracy: > 90%
- User satisfaction: > 4/5

### Fase 3
- Auto-map success: > 95%
- History accuracy: 100%
- Export reliability: 100%

### Fase 4
- Animation smooth: 60fps
- Prompt usefulness: > 80%
- Visual feedback clarity: > 4/5

### Fase 5
- Save/load time: < 2s
- Zero data loss: 100%
- Auto-save reliability: 100%

### Fase 6
- Onboarding completion: > 80%
- Support ticket rate: < 10%

---

## 🔄 Review Points

### Review settimanale (venerdì)
- Progress vs plan
- Blockers
- Adjustments

### Review mensile
- Timeline adjustment
- Scope review
- Risk assessment

### Review milestone
- Go/No-Go decision
- Pivot se necessario
- Next phase planning

---

## 🚨 Risk Register

### Rischio: JUCE learning curve
**Probabilità:** Media  
**Impatto:** Alto (2-4 settimane ritardo)  
**Mitigazione:** Iniziare con template esistenti, usare forum JUCE

### Rischio: Ollama performance
**Probabilità:** Media  
**Impatto:** Medio (user experience degradata)  
**Mitigazione:** Fallback a modello più piccolo (phi-3), cloud backup

### Rischio: Ableton compatibility issues
**Probabilità:** Bassa  
**Impatto:** Alto (blocker totale)  
**Mitigazione:** Test continuativo, beta testers

### Rischio: Scope creep
**Probabilità:** Alta  
**Impatto:** Alto (timeline slippage)  
**Mitigazione:** Strict feature freeze dopo Fase 2

---

## 📝 Note Implementative

### Priorità Sviluppo
1. **Core stability** > Features
2. **User experience** > Technical elegance
3. **Ship MVP** > Perfect everything

### Definition of Done (DoD)
- [ ] Codice reviewato
- [ ] Test unitari passanti
- [ ] Test manuali eseguiti
- [ ] Documentazione aggiornata
- [ ] Build CI verde
- [ ] Zero regression note

---

*Questo piano è living document. Aggiornare dopo ogni push o modifica significativa.*
*Ultimo aggiornamento: 2026-04-12 — Fase 0 completata, Fase 1 in corso (skeleton + OSC parziale)*