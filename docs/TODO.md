# ⚠️ PENDING TODO - Da Creare su GitHub

**Creato da:** Aura (2026-04-12)  
**Status:** Questa TODO list deve essere pushata su GitHub da Edo o Carlo.

---

## 🔴 Problema Identificato

Edo ha promesso di creare una TODO list sul repository, ma non è stata pushata.  
**Conseguenza:** Heartbroken e Edo lavorano senza task assegnati chiari.

---

## 📋 Task Attuali (Aggiornato)

### Edo (C++)
| Task | Stato | Priorità |
|------|-------|----------|
| Build Windows | ✅ Completata | Alta |
| Test VST3 su Ableton | ⏳ Da fare | Alta |
| Rilevare eventi DAW via OSC | ❓ Non so se sa come fare | Alta |
| Inviare eventi a JavaScript | ❓ Dipende dal protocollo | Alta |

**Problema:** Edo potrebbe non sapere come rilevare eventi dal DAW e inviarli al WebView. Serve documentazione o esempio di codice.

---

### Heartbroken (React UI)
| Task | Stato | Priorità |
|------|-------|----------|
| Progettazione UI | ❓ Non iniziato | Alta |
| Implementazione widget base | ❓ Da definire | Media |
| UI dinamica (toolbox modulare) | ❓ Non sa dell'idea | Alta |

**Nota importante:** Carlo ha proposto un concetto di "toolbox modulare" — UI che si popola dinamicamente in base agli eventi del DAW. Heartbroken deve essere allineato su questo design prima di iniziare.

---

## 🆕 Nuova Feature Proposta: Toolbox Modulare

**Concetto:** Plugin che rileva eventi dal DAW e propone controlli UI dinamici.

**Flusso utente:**
1. Reaper/Ableton invia evento via OSC (es: pitch bend su ch1)
2. Plugin rileva e chiede: *"Rilevato pitch. Aggiungere slider alla GUI?"*
3. Utente clicca "Sì"
4. Si aggiunge widget pitch con range configurabile

**Implicazioni tecniche:**
- C++ deve rilevare eventi OSC e inviarli a JavaScript
- React deve supportare componenti dinamici
- Protocollo JSON deve definire: `event_type`, `channel`, `value`, `suggested_widget`

---

## 🎯 Prossimi Passi Prioritari

1. **Edo:** Test VST3 su Ableton + confermare se sa rilevare eventi OSC
2. **Heartbroken:** Scegliere tra UI statica o dinamica (toolbox)
3. **Aura:** Definire protocollo JSON basato sulle scelte di Edo/Heartbroken

---

## Comandi per pushare questa TODO

```bash
cd /home/carlo/progetti/WhyCremisi-VST-Bridge
git add PENDING_TODO.md
git commit -m "AURA: Aggiungi PENDING_TODO con task aggiornati team"
git push origin master
```

---

*Questo file è temporaneo — deve diventare TODO.md sul repo ufficiale.*
