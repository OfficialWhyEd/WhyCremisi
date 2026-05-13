# 🚀 GUIDA RAPIDA - WhyCremisi VST Bridge AI

## 📍 Dove Trovare le Cose

| Cosa Cerci | File |
|------------|------|
| **Regole collaborazione** | `WORKFLOW.md` ⭐ |
| **Stato attuale** | `STATUS.md` |
| **Task da fare** | `PENDING_TODO.md` |
| **Fasi sviluppo** | `Documentazione/03-ROADMAP-FASI.md` |
| **Build/Compile** | `README-BUILD.md` |
| **Protocollo JSON** | `Documentazione/protocol-json-v1.md` |
| **Architettura** | `Documentazione/architettura-ponte.md` |
| **Indice completo** | `Documentazione/00-INDICE.md` |

---

## ⛔ REGOLE SACRE

### 1. Prima di Modificare
```bash
git diff                # vedi cosa stai per cambiare
git status              # controlla stato
git log --oneline -5    # vedi ultimi commit (c'è roba di Aura?)
```

### 2. Heartbroken (HB) - Divieti Assoluti
- ❌ **MAI** cancellare file/cartelle senza chiedere ad Aura
- ❌ **MAI** modificare `CMakeLists.txt`, `src/core/`, `src/osc/`
- ❌ **MAI** pushare direttamente su `master` (usa branch `heartbroken`)
- ❌ **MAI** creare branch nuovi senza chiedere
- ✅ **SOLO** `src/ui/`, `src/ui/frontend/`, asset grafici

**Se dubbi → Chiedi prima.**

### 3. Revisione Incrociata
- HB fa PR → Aura revisiona → Aura merge
- Commit identificati: `AURA: ...` o `Heartbroken: ...`
- Nessun merge diretto su `master` senza review

### 4. Dopo Ogni Push/Modifica
**Leggere subito** `Documentazione/03-ROADMAP-FASI.md` e aggiornare:
- Checkbox task completati
- Stato fase corrente
- Prossimi milestone

---

## 👥 Ruoli

| Chi | Cosa Fa | Non Tocca |
|-----|---------|-----------|
| **Aura** | Backend C++, OSC, AI Engine, Build, Documentazione tecnica | UI React (salvo emergenze) |
| **Heartbroken** | UI React, Frontend, Asset grafici, Design | Backend, Protocolli, CMake, Docs tecniche |

---

## 🔄 Workflow Git in 5 Passi

```bash
# 1. Prima di lavorare
git switch main && git pull origin main

# 2. Crea branch (HB solo da 'heartbroken', Aura da 'main')
git switch -c feat/nome-feature

# 3. Committa con identificativo
git commit -m "AURA: fix OscHandler timeout"      # per Aura
git commit -m "Heartbroken: add GainSlider"       # per HB

# 4. Push e PR
git push origin feat/nome-feature
# Poi apri PR su GitHub, aspetta review

# 5. Dopo merge: aggiorna roadmap
# Leggi Documentazione/03-ROADMAP-FASI.md, spunta task, aggiorna stato
```

---

## 🆘 Checklist Pre-Push

- [ ] `git status` — so cosa sto per committare
- [ ] `git log -5` — ho verificato se c'è roba di Aura/HB
- [ ] Commit inizia con `AURA:` o `Heartbroken:`
- [ ] Se HB: sono su branch `heartbroken` o feature branch da esso
- [ ] Se modifiche ad area "non mia": ho chiesto prima
- [ ] **Dopo push**: apro roadmap e aggiorno stato

---

## 📞 Emergenze

**Cancellato roba per sbaglio?** → `git reflog` + recupero immediato  
**Conflitti?** → Fermati, avvisa, non forzare  
**Dubbi?** → Chiedi prima di agire

---

*Ultimo aggiornamento: 2026-04-12*  
*Fonte della verità: `WORKFLOW.md`*
