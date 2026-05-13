# 05-REQUISITI - Funzionali e Non-Funzionali

**Documento:** Specifica dettagliata dei requisiti  
**Target:** Team di sviluppo, QA, Edo  
**Stato:** Draft - richiede review

---

## 📋 Requisiti Funzionali

### RF1: Plugin VST3/AU/AAX

**RF1.1** Il plugin deve caricarsi in Ableton Live 12+  
**RF1.2** Il plugin deve caricarsi in Reaper 7+  
**RF1.3** Il plugin deve caricarsi in Logic Pro 11+  
**RF1.4** Il plugin deve caricarsi in FL Studio 21+  
**RF1.5** Il plugin non deve causare crash del DAW  
**RF1.6** Il plugin deve supportare hot-reload preset DAW  
**RF1.7** Il plugin deve supportare automation DAW  
**RF1.8** Il plugin deve essere disponibile in formato VST3  
**RF1.9** Il plugin deve essere disponibile in formato AU (macOS)  
**RF1.10** Il plugin deve essere disponibile in formato AAX (Pro Tools)

**Test:** Caricare plugin in ogni DAW target, verificare stabilità

---

### RF2: Interfaccia Utente (UI)

**RF2.1** UI deve essere responsive (resize window)  
**RF2.2** UI deve includere 8 knob parametri visibili  
**RF2.3** Ogni knob deve mostrare nome parametro  
**RF2.4** Ogni knob deve mostrare valore numerico  
**RF2.5** Ogni knob deve mostrare unità (dB, Hz, %, etc.)  
**RF2.6** Knob deve supportare drag per modifica valore  
**RF2.7** Knob deve supportare double-click per reset valore default  
**RF2.8** UI deve includere status bar (OSC status, AI status)  
**RF2.9** UI deve includere button per aprire AI panel  
**RF2.10** UI deve supportare color scheme dark/light  
**RF2.11** UI deve essere fluida (60fps)  
**RF2.12** UI deve scalare correttamente su display HiDPI

**Test:** Manuale UI testing, performance profiling

---

### RF3: Comunicazione OSC

**RF3.1** Plugin deve avviare OSC server su porta configurabile  
**RF3.2** Porta default deve essere 9000  
**RF3.3** Plugin deve ricevere messaggi OSC su porta configurata  
**RF4.4** Plugin deve inviare messaggi OSC su destinazione configurata  
**RF3.5** Plugin deve supportare indirizzo destinazione default 127.0.0.1:9001  
**RF3.6** Plugin deve parsare messaggi OSC con argomenti float  
**RF3.7** Plugin deve parsare messaggi OSC con argomenti int  
**RF3.8** Plugin deve parsare messaggi OSC con argomenti string  
**RF3.9** Plugin deve parsare messaggi OSC con argomenti bool  
**RF3.10** Plugin deve gestire pattern OSC con wildcard (es. /track/*/volume)  
**RF3.11** Plugin deve inviare notifica UI quando parametro cambia via OSC  
**RF3.12** Plugin deve aggiornare valore parametro quando riceve OSC  
**RF3.13** Plugin deve loggare tutti i messaggi OSC in console (debug mode)  
**RF3.14** Plugin deve gestire errori OSC (porta occupata, network unreachable)  
**RF3.15** Plugin deve permettere enable/disable OSC communication  
**RF3.16** Plugin deve supportare OSC bundle (multi-message)

**Test:** Unit test con osc-send tool, integration test con Max4Live

---

### RF4: Protocollo MIDI (Fallback)

**RF4.1** Plugin deve ricevere MIDI CC input  
**RF4.2** Plugin deve mappare CC #1-8 a parametri 1-8 (default)  
**RF4.3** Plugin deve permettere configurazione mapping CC → parametro  
**RF4.4** Plugin deve convertire valore MIDI (0-127) a float (0.0-1.0)  
**RF4.5** Plugin deve inviare MIDI CC output quando parametro cambia  
**RF4.6** Plugin deve supportare MIDI learn (right-click su knob, muovi controller)

**Test:** Test con controller MIDI fisico, test con loopback MIDI

---

### RF5: AI Engine

**RF5.1** Plugin deve connettersi a Ollama su localhost:11434  
**RF5.2** Plugin deve rilevare se Ollama non è in esecuzione  
**RF5.3** Plugin deve mostrare errore user-friendly se Ollama non disponibile  
**RF5.4** Plugin deve inviare prompt a Ollama con context parametri  
**RF5.5** Plugin deve ricevere risposta da Ollama  
**RF5.6** Plugin deve mostrare risposta AI in UI  
**RF5.7** Plugin deve mantenere history conversazione (ultimi 10 messaggi)  
**RF5.8** Plugin deve includere system prompt per contesto audio  
**RF5.9** Plugin deve supportare fallback a Gemini API se Ollama non disponibile  
**RF5.10** Plugin deve supportare fallback a OpenAI API se Gemini non disponibile  
**RF5.11** Plugin deve gestire timeout (10 secondi max)  
**RF5.12** Plugin deve gestire errori API (rate limit, network)  
**RF5.13** Plugin deve loggare tutte le chiamate AI (debug mode)  
**RF5.14** AI deve poter leggere valori parametri correnti  
**RF5.15** AI deve poter modificare valori parametri via comandi testuali  
**RF5.16** AI deve chiedere conferma prima di applicare modifiche > ±3dB  
**RF5.17** AI deve spiegare ragionamento dietro suggerimenti  
**RF5.18** AI deve supportare comandi multi-parametro ("set gain -6dB and freq 1000Hz")

**Test:** Unit test con mock AI, integration test con Ollama real

---

### RF6: Auto-Mapping

**RF6.1** Plugin deve avere modalità "Learn"  
**RF6.2** In modalità Learn, plugin ascolta messaggi OSC in ingresso  
**RF6.3** Quando riceve messaggio OSC, plugin mostra dialog  
**RF6.4** Dialog chiede: "Nuovo controllo rilevato: {address}. Aggiungere?"  
**RF6.5** Dialog mostra knob disponibile per mapping  
**RF6.6** Utente può confermare o ignorare  
**RF6.7** Se confermato, plugin mappa indirizzo OSC a knob  
**RF6.8** Plugin salva mapping in preset  
**RF6.9** Plugin supporta "Auto-map all" (mappa primi 8 parametri rilevati)  
**RF6.10** Plugin supporta "Clear all mappings"  
**RF6.11** Plugin visualizza mappatura corrente in UI  
**RF6.12** Plugin permette edit manuale mapping

**Test:** Manual test con Max4Live, test con TouchOSC

---

### RF7: Control History

**RF7.1** Plugin registra ogni modifica parametro  
**RF7.2** Ogni record include: timestamp, parametro, valore vecchio, valore nuovo, source (user/AI/OSC/MIDI)  
**RF7.3** Plugin mantiene history in memoria (ultimi 1000 cambi)  
**RF7.4** Plugin visualizza history in tabella scrollable  
**RF7.5** Plugin permette filtro per parametro  
**RF7.6** Plugin permette filtro per source  
**RF7.7** Plugin permette filtro per intervallo temporale  
**RF7.8** Plugin permette esportazione history in JSON  
**RF7.9** Plugin permette esportazione history in CSV  
**RF7.10** Plugin permette clear history  
**RF7.11** Plugin mostra statistiche: numero cambi per parametro, media valori  
**RF7.12** Plugin permette replay history (per debugging)

**Test:** Unit test su data structure, integration test su export

---

### RF8: Animazioni

**RF8.1** Quando parametro cambia, knob anima verso nuovo valore  
**RF8.2** Animazione durata: 200ms (configurabile)  
**RF8.3** Animazione usa easing curve (cubic-bezier)  
**RF8.4** Knob visualizza indicatore quando AI sta agendo (pulsing glow)  
**RF8.5** Animazione è fluida (60fps)  
**RF8.6** Utente può disabilitare animazioni  
**RF8.7** Animazioni funzionano anche quando cambio è da OSC/MIDI  
**RF8.8** Animazioni supportano interruzione (se utente inizia drag)

**Test:** Visual test, performance profiling

---

### RF9: Prompt Library

**RF9.1** Plugin include 10 prompt pre-configurati  
**RF9.2** Prompts coprono casi d'uso comuni:
- Boost vocal presence
- Tighten bass
- Add air to mix
- Glue drums
- Widen stereo
- De-ess vocals
- Compress dynamics
- Add saturation
- Clean low-end
- Balance levels

**RF9.3** UI mostra lista prompt  
**RF9.4** Utente può cliccare prompt per eseguire  
**RF9.5** Plugin invia prompt + context a AI  
**RF9.6** AI esegue azioni multiple se necessario  
**RF9.7** Utente può aggiungere custom prompt  
**RF9.8** Utente può editare prompt esistenti  
**RF9.9** Utente può cancellare custom prompt  
**RF9.10** Prompt library è salvata in preset  
**RF9.11** Plugin permette export/import prompt library  
**RF9.12** Prompt includono descrizione tooltip

**Test:** Manual test di ogni prompt, test export/import

---

### RF10: Session Management

**RF10.1** Plugin permette salvataggio sessione (.claw file)  
**RF10.2** File include: mapping, history, AI conversation, settings  
**RF10.3** Plugin permette caricamento sessione  
**RF10.4** Plugin mostra lista recent sessions  
**RF10.5** Plugin supporta auto-save ogni 5 minuti  
**RF10.6** Auto-save non blocca UI  
**RF10.7** Plugin permette ripristino da auto-save al riavvio  
**RF10.8** Plugin mostra dialog se file non salvato all'uscita  
**RF10.9** Plugin supporta versioni file (per backward compatibility)  
**RF10.10** Plugin gestisce gracefulmente file corrotti  
**RF10.11** Plugin supporta export preset singolo  
**RF10.12** Plugin supporta import preset singolo

**Test:** Unit test su serialization, integration test su save/load

---

### RF11: Demo & Tutorial

**RF11.1** Plugin include modalità demo (preset caricato, simula interazioni)  
**RF11.2** Plugin include tutorial interattivo (5 step)  
**RF11.3** Tutorial copre: setup, mapping, AI chat, history, save  
**RF11.4** Tutorial è skippable  
**RF11.5** Tutorial può essere riaperto da menu Help  
**RF11.6** Plugin include link a video demo (YouTube)  
**RF11.7** Plugin include link a user manual (PDF)  
**RF11.8** Plugin include link a community forum  
**RF11.9** Plugin include link a support email  
**RF11.10** Plugin mostra versione e changelog

**Test:** Manual test tutorial, verifica tutti i link

---

### RF12: Settings & Configuration

**RF12.1** Plugin ha pannello Settings  
**RF12.2** Settings includono: porta OSC, porta Ollama, API keys  
**RF12.3** Settings includono: enable/disable OSC, MIDI, AI  
**RF12.4** Settings includono: animation speed, theme, language  
**RF12.5** Settings sono salvate in file config  
**RF12.6** Settings includono reset to defaults  
**RF12.7** Settings includono export/import config  
**RF12.8** Settings includono opzioni avanzate (debug mode, logging)  
**RF12.9** Settings includono check for updates  
**RF12.10** Settings includono privacy options (data collection opt-out)

**Test:** Test export/import, test reset

---

## 📊 Requisiti Non-Funzionali

### RNF1: Performance

**RNF1.1** CPU overhead < 2% quando idle  
**RNF1.2** CPU overhead < 5% durante processing  
**RNF1.3** Latenza OSC < 5ms (loopback)  
**RNF1.4** Latenza AI < 3 secondi (Ollama local)  
**RNF1.5** Load time plugin < 500ms  
**RNF1.6** UI rendering > 60fps  
**RNF1.7** Memory usage < 100MB  
**RNF1.8** Disk I/O non blocca UI  
**RNF1.9** Network I/O non blocca UI  
**RNF1.10** Garbage collection non causa glitch audio

**Test:** Profiling con PluginDoctor, performance monitoring

---

### RNF2: Affidabilità

**RNF2.1** Zero crash in 100 ore di uso continuativo  
**RNF2.2** Mean time between failures (MTBF) > 1000 ore  
**RNF2.3** Recovery time < 10 secondi (restart plugin)  
**RNF2.4** Data loss rate < 0.1%  
**RNF2.5** Auto-save reliability 100%  
**RNF2.6** OSC message delivery > 99.9%  
**RNF2.7** AI API uptime > 99% (cloud)  
**RNF2.8** Session file corruption rate < 0.01%

**Test:** Stress testing, long-running tests

---

### RNF3: Usabilità

**RNF3.1** Onboarding completato da > 80% utenti nuovi  
**RNF3.2** Task base completabile in < 3 click  
**RNF3.3** Discoverability features > 70% (utenti trovano features senza doc)  
**RNF3.4** Error rate utente < 5% (azioni non intenzionali)  
**RNF3.5** Satisfaction score > 4/5  
**RNF3.6** Net Promoter Score (NPS) > 50  
**RNF3.7** Support ticket rate < 10% utenti  
**RNF3.8** Time to first success < 10 minuti

**Test:** User testing con 10+ beta testers

---

### RNF4: Manutenibilità

**RNF4.1** Code coverage test > 80%  
**RNF4.2** Cyclomatic complexity < 10 per funzione  
**RNF4.3** Documentazione codice > 50% funzioni  
**RNF4.4** Build time < 5 minuti  
**RNF4.5** Zero warnings in build  
**RNF4.6** Dependency count < 20  
**RNF4.7** Update frequency: minor release ogni 4 settimane  
**RNF4.8** Bug fix time < 48 ore (critical), < 1 settimana (major)

**Test:** Code analysis tools (SonarQube, clang-tidy)

---

### RNF5: Portabilità

**RNF5.1** Supporto Windows 10+  
**RNF5.2** Supporto macOS 11+ (Intel e Apple Silicon)  
**RNF5.3** Supporto Linux Ubuntu 22.04+  
**RNF5.4** UI consistente cross-platform  
**RNF5.5** Performance equivalente ±10% cross-platform  
**RNF5.6** File format compatibile cross-platform  
**RNF5.7** OSC/MIDI compatibile cross-platform  
**RNF5.8** AI engine compatibile cross-platform

**Test:** CI/CD su tutte le piattaforme

---

### RNF6: Sicurezza

**RNF6.1** OSC bind solo a localhost  
**RNF6.2** No esecuzione codice arbitrario da AI  
**RNF6.3** API keys criptate in config file  
**RNF6.4** No logging di dati sensibili  
**RNF6.5** Auto-save non sovrascrive senza backup  
**RNF6.6** History non include dati audio (solo parametri)  
**RNF6.7** AI non può eseguire comandi di sistema  
**RNF6.8** Network traffic criptato (HTTPS per cloud APIs)

**Test:** Security audit, penetration testing

---

## 🚫 Requisiti Esplicitamente Fuori Scope (v1.0)

- ❌ Supporto VST2 (obsoleto)
- ❌ Supporto plugin 32-bit
- ❌ Mobile companion app
- ❌ Cloud sync (richiede backend)
- ❌ Real-time audio analysis (FFT)
- ❌ Plugin hosting interno (caricare altri VST dentro)
- ❌ Multi-user collaboration
- ❌ AI training custom (richiede ML pipeline)
- ❌ Hardware controller dedicated (usiamo OSC/MIDI)
- ❌ Surround sound (> 2 canali)

---

## ✅ Prioritizzazione Requisiti (MoSCoW)

### MUST HAVE (senza questi, prodotto non funziona)
- RF1.1, RF1.5, RF1.8 (plugin base)
- RF2.2, RF2.6 (knob base)
- RF3.1, RF3.3, RF3.12 (OSC base)
- RF5.1, RF5.5, RF5.14 (AI base)
- RF10.1, RF10.3 (save/load)

### SHOULD HAVE (importanti ma non blocker)
- RF6.1, RF6.7 (auto-mapping)
- RF7.1, RF7.4 (history)
- RF8.1, RF8.5 (animations)
- RF9.1, RF9.4 (prompt library)
- RF11.2, RF11.3 (tutorial)

### COULD HAVE (nice to have)
- RF1.10 (AAX support)
- RF4.6 (MIDI learn)
- RF12.4 (themes)
- RF12.10 (privacy options)

### WON'T HAVE (v1.0)
- Tutti i requisiti in "Fuori Scope"

---

*Questo documento è la fonte di verità per i requisiti. Ogni modifica richiede review e versionamento.*
