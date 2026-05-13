# Documentazione Progetto Edo - INDICE

**Progetto:** WhyCremisi VST Bridge AI Edition  
**Data Creazione:** 2026-04-09  
**Versione:** 1.0.0  
**Stato:** Analisi Iniziale & Brainstorming Critico

---

## 📚 Documenti Principali

### 📋 Inizio Qui
- **GUIDA-RAPIDA.md** (root) - Regole sacre, workflow git, dove trovare le cose
- **WORKFLOW.md** (root) - Fonte della verità per collaborazione Aura/HB

### 📑 Documentazione di Progetto
1. **01-EXECUTIVE-SUMMARY.md** - Sintesi critica del progetto, rischi e opportunità
2. **02-ANALISI-MERCATO.md** - Prodotti simili esistenti e gap competitivi
3. **03-ROADMAP-FASI.md** - Piano di sviluppo a fasi con milestone ⭐ AGGIORNARE DOPO OGNI PUSH
4. **04-SPECIFICHE-TECNICHE.md** - Architettura, protocolli e scelte tecnologiche
5. **05-REQUISITI.md** - Requisiti funzionali e non-funzionali dettagliati
6. **06-BRAINSTORMING.md** - Idee aggiuntive e feature future
7. **07-CHANGELOG.md** - Versioni e modifiche

### 🤝 Collaborazione
- **08-COLLABORAZIONE-HB.md** - Regole tecniche per Heartbroken (complementa WORKFLOW.md)
- **09-GUIDA-SVILUPPO.md** - Guida sviluppo UI per Edo e team ★ NEW

### 🔧 Tecnico
- **architettura-ponte.md** - Decisione architettura OSC-Web
- **protocol-json-v1.md** - Protocollo JSON C++ ↔ JavaScript v1.0 (definitivo)
- **NOTE_TECNICHE_PROTOCOLI.md** - Note implementazione OSC

### 📊 Stato e Task (root)
- **STATUS.md** - Stato attuale progetto
- **PENDING_TODO.md** - Task da completare (sincronizzare con roadmap)
- **GUIDA-RAPIDA.md** - Regole sacre e navigazione documentazione
- **WORKFLOW.md** - Fonte della verità per collaborazione git
- **Comunicazioni.md** - Messaggi tra team members
- **README-BUILD.md** - Istruzioni build cross-platform
- **TEST-GUIDE.md** - Guida test per Edo/HB

---

## 🎯 Obiettivo del Progetto

Creare una linea di plugin VST/AU/AAX con **firma AI integrata** - il primo plugin DAW al mondo con agente AI che non solo assiste ma **interagisce visivamente** con i controlli in tempo reale.

**Mission:** Rendere l'AI un membro del team di produzione, non uno strumento passivo.

---

## ⚠️ REGOLE FERREE DEL PROGETTO

1. **Mai toccare `/home/carlo/.whycremisi/` senza permesso esplicito di Carlo**
2. **Mai dire bugie** - Se non si sa, si dice "non lo so, verifico"
3. **Testare sempre prima di affermare** - "Funziona" solo dopo aver testato
4. **Verificare documentazione/protocolli** prima di implementare
5. **Essere critici e onesti** - Se un'idea non funziona, dirlo chiaramente

---

## 📁 Struttura Repository

```
WhyCremisi-VST-Bridge-AI/
├── bridge_core/                    # Core engine (C++/Rust)
├── gui/                            # Frontend (React + Tauri)
├── ai_engine/                      # AI agent system
├── protocols/                      # OSC/MIDI/VST3 handlers
├── documentation/                  # Questa cartella
├── tests/                          # Test suite
├── assets/                         # Icons, fonts, media
└── build/                          # Build scripts
```

---

## 👥 Team

- **Edo** - Founder, Product Vision, Audio Engineering
- **Carlo** - Advisor, Project Management, QA
- **Aura** - AI Assistant, Documentation, Research

---

*Questa documentazione è in continuo aggiornamento. Ultima modifica: 2026-04-12*
