# 🤝 Collaborazione Heartbroken (HB)

**File complementare a:** `WORKFLOW.md` (fonte della verità)  
**Scopo:** Linee guida specifiche per Heartbroken (AI di Edo)

---

## 🎯 Il Tuo Campo

| ✅ Fai Questo | ❌ Non Toccare |
|--------------|----------------|
| `src/ui/frontend/` - React, Vue, etc. | `src/core/` - PluginProcessor, Audio |
| `src/ui/Prototipi/` - HTML/CSS prototipi | `src/osc/` - OSC Handler |
| Asset grafici (icone, font, CSS) | `src/ai/` - AI Engine |
| `src/ui/components/` - Componenti React | `CMakeLists.txt` - Build system |
| Documentazione UI/UX | Protocolli, JSON schema |

---

## ⚠️ Regola d'Oro

**NON CANCELLARE MAI FILE O CARTELLE SENZA CHIEDERE AD AURA**

Quando cancelli roba:
1. Aura deve analizzare cosa hai cancellato vs master
2. Cherry-pick manuale file per file
3. Perdita di tempo per entrambi

---

## 🔄 Branch Workflow

### Per Heartbroken:

```bash
# 1. Assicurati di essere su 'heartbroken'
git fetch origin && git switch heartbroken

# 2. Crea feature branch DA 'heartbroken' (non da main!)
git switch -c feat/nome-feature-chiaro heartbroken

# 3. Committa solo sul feature branch
git commit -m "Heartbroken: feat: aggiunto GainSlider"

# 4. Merge in 'heartbroken' quando funziona
git switch heartbroken
git merge --no-ff feat/nome-feature

# 5. Push e notifica Aura
git push origin heartbroken
# Poi: apri PR verso main, Aura revisiona
```

### Regole Fisse
- ❌ **Mai** pushare direttamente su `main`
- ❌ **Mai** creare feature branch da `main`, solo da `heartbroken`
- ❌ **Mai** cancellare branch `heartbroken`
- ✅ `heartbroken` contiene solo codice funzionante e testato
- ✅ Ogni commit inizia con `Heartbroken:`

---

## 📋 Checklist Pre-Push

- [ ] `git status` — so cosa sto per committare
- [ ] `git log --oneline -5` — verificato ultimi commit (c'è roba di Aura?)
- [ ] Se commit di Aura: **STOP**, avvisato su WhatsApp/Telegram
- [ ] Committato modifiche locali
- [ ] `git pull` fatto senza errori
- [ ] Conflitti risolti (se presenti)
- [ ] Commit message inizia con `Heartbroken:`
- [ ] **Dopo push**: apro roadmap e aggiorno stato task

---

## 🆘 Se Hai Dubbi

| Situazione | Cosa Fare |
|------------|-----------|
| Non sai se cancellare? | **NON CANCELLARE**, chiedi ad Aura |
| Hai già fatto casino? | Avvisa subito, prima è meglio |
| Vuoi sperimentare? | Crea branch `hb-test/nome-feature` |
| Dubbi su un file? | "Aura, posso modificare X?" — risposta in 30s |

---

## 💬 Comunicazione

- **Urgenti:** WhatsApp/Telegram (Carlo ↔ Edo)
- **Tecniche:** GitHub PR e issues
- **Documentazione:** Questo file e `WORKFLOW.md`

---

*Rispettiamo queste regole → lavoriamo felici*  
*Contraddizioni? `WORKFLOW.md` vince sempre.*
