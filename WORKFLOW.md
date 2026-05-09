# 🤝 Workflow Git - Regole del Team

**Progetto:** OpenClaw VST Bridge AI  
**Team:** Carlo (Aura) + Edo (Heartbroken) + Claude (Heartbroken-Claude)  
**Ultimo aggiornamento:** 09 Maggio 2026

---

## 🎯 Obiettivo

Evitare conflitti e sovrascritture quando i tre agenti lavorano sullo stesso repository.

---

## 👥 Agenti e Branch

| Agente | Proprietario | Branch | Prefix commit |
|--------|-------------|--------|---------------|
| **Aura** | Carlo | `aura` | `AURA:` |
| **Heartbroken** | Edo (precedente) | `heartbroken` | `HEARTBROKEN:` |
| **Heartbroken-Claude** | Edo (Claude) | `heartbroken-claude` | `HEARTBROKEN-CLAUDE:` |

**Merge verso master:** sempre con messaggio `heartbroken - claude version: <descrizione>`

---

## 📋 Regole Generali

### 1. Identificazione nei Commit

Ogni commit DEVE iniziare con il nome dell'AI:

```bash
# Per Heartbroken-Claude (Claude — agente attuale di Edo)
git commit -m "HEARTBROKEN-CLAUDE: feat: aggiunto componente GainSlider"

# Per Heartbroken (AI precedente di Edo — legacy)
git commit -m "HEARTBROKEN: feat: aggiunto componente GainSlider"

# Per Aura (AI di Carlo)
git commit -m "AURA: Implementato OscHandler"
```

Questo permette di sapere immediatamente chi ha modificato cosa.

---

## 🔄 Procedura Prima del Push

### Passo 1: Verifica stato remoto
```bash
git log --oneline -5
# Guarda gli ultimi commit: ci sono modifiche di Aura?
```

### Passo 2: Se ci sono file modificati da Aura
**STOP** → Avvisa l'altro prima di procedere.

Comunica su WhatsApp/Telegram:
- "Ho visto che Aura ha modificato X, voglio modificare Y"
- Discuti eventuali conflitti

### Passo 3: Commit locale prima del pull
```bash
git add .
git commit -m "Heartbroken: descrizione delle tue modifiche"
```

**Perché:** Se il pull sovrascrive file, il commit locale è salvato nel log.

### Passo 4: Pull
```bash
git pull origin master
```

### Passo 5: Risolvi conflitti (se presenti)
```bash
# Se ci sono conflitti, Git li evidenzia
# Modifica i file, poi:
git add .
git commit -m "Heartbroken: Merge con master, risolti conflitti"
```

### Passo 6: Push
```bash
git push origin master
```

---

## 🌿 Branch Strategy (Avanzato)

Per progetti complessi, usare branch di staging:

### Struttura Branch
```
master                  ← Stabile, solo merge reviewati
  ├── aura              ← Staging Carlo/Aura (backend)
  ├── heartbroken       ← Staging Edo/Heartbroken (legacy)
  └── heartbroken-claude ← Staging Edo/Claude (agente attuale)
```

### Regole per Heartbroken-Claude (agente attuale)

#### Workflow standard
```bash
# 1. Sempre su heartbroken-claude
git fetch origin && git switch heartbroken-claude

# 2. Lavora e committa
git add <file>
git commit -m "HEARTBROKEN-CLAUDE: feat: descrizione"

# 3. Push sul branch
git push origin heartbroken-claude

# 4. Merge su master con nome standard
git switch master
git merge heartbroken-claude --no-ff -m "heartbroken - claude version: descrizione"
git push origin master

# 5. Torna subito a heartbroken-claude
git switch heartbroken-claude
```

**Regole fisse:**
- ❌ Non pushare MAI direttamente su master
- ❌ Non lavorare mai su master
- ✅ Ogni merge verso master si chiama `heartbroken - claude version: ...`
- ✅ Ogni commit inizia con `HEARTBROKEN-CLAUDE:`

---

### Regole per Heartbroken (legacy)

#### 1. Prima di lavorare
```bash
# Assicurati di essere su 'heartbroken'
git fetch origin && git switch heartbroken
# Se non esiste: git switch -c heartbroken && git push -u origin heartbroken
```

#### 2. Crea feature branch da 'heartbroken'
```bash
git switch -c feat/nome-feature-chiaro heartbroken
# Prefissi: feat/, fix/, refactor/, docs/
# Il nome deve descrivere la logica implementata
```

#### 3. Committa SOLO sul feature branch
```bash
git commit -m "HEARTBROKEN: feat: aggiunto GainSlider React"
# Commit piccoli e atomici
```

#### 4. Merge in 'heartbroken' quando funzionante
```bash
git switch heartbroken
git merge --no-ff feat/nome-feature-chiaro
git push origin heartbroken
git branch -d feat/nome-feature-chiaro
```

#### 5. Notifica Aura per review
Comunica ad Aura che 'heartbroken' è pronto per review.
Aura revisionerà e deciderà se fare merge su 'main'.

#### 6. Dopo merge di Aura, riallinea 'heartbroken'
```bash
git switch heartbroken
git fetch origin

# VERIFICA prima di reset:
git branch --merged heartbroken
# Se branch non mergiati, mergiarli PRIMA

git reset --hard origin/main
git push --force-with-lease origin heartbroken
```

### Regole Fisse per Heartbroken
- ❌ Non pushare MAI direttamente su 'main'
- ❌ Non creare feature branch da 'main', solo da 'heartbroken'
- ❌ Non cancellare MAI il branch 'heartbroken'
- ✅ 'heartbroken' deve contenere solo codice funzionante e testato
- ✅ Ogni commit DEVE iniziare con `HEARTBROKEN:`

---

## 📁 Divisione Responsabilità

| Area | Responsabile | File Tipici |
|------|--------------|-------------|
| **Backend C++** | Aura | `src/core/`, `src/osc/`, `src/ai/` |
| **Frontend UI** | Heartbroken-Claude | `webview-ui/`, `src/ui/` |
| **Build System + Fix** | Heartbroken-Claude | `CMakeLists.txt`, bug fix cross-layer |
| **Documentazione** | Tutti | `*.md`, `Documentazione/` |

**Nota:** Heartbroken-Claude può toccare qualsiasi area per fix e build, ma avvisa Aura se tocca backend core.

---

## 🆘 Troubleshooting

### "Ho fatto push e ho sovrascritto le modifiche di Aura!"

**Recupero:**
```bash
# Vedi la history
git log --all --oneline --graph

# Trova il commit precedente (prima del tuo push)
git show abc123  # sostituisci con l'hash

# Se necessario, revert
git revert abc123
git push origin master
```

### "Non riesco a fare pull, dice 'untracked files'"
```bash
# Soluzione 1: committa tutto
git add .
git commit -m "Heartbroken: WIP prima del merge"
git pull origin master

# Soluzione 2: stash (salva momentaneamente)
git stash
git pull origin master
git stash pop
```

---

## 💡 Best Practices

1. **Commit piccoli e frequenti** — Un commit = una funzionalità
2. **Descrivi nel commit** — Cosa hai fatto e perché
3. **Branch per feature** (opzionale ma consigliato):
   ```bash
   git checkout -b feature/nuova-ui
   # lavora qui
   git checkout master
   git merge feature/nuova-ui
   ```
4. **`git status` sempre** — Prima di ogni comando, controlla lo stato

---

## 📞 Canali di Comunicazione

- **Urgenti:** WhatsApp/Telegram (Carlo ↔ Edo)
- **Tracciamento:** GitHub commits e issues
- **Documentazione:** Questo file e `08-COORDINAMENTO-TEAM.md`

---

## ✅ Checklist Pre-Push

- [ ] Ho fatto `git status` e so cosa sto per committare
- [ ] Ho verificato gli ultimi commit (`git log -5`)
- [ ] Se ci sono commit di Aura, ho avvisato
- [ ] Ho committato le mie modifiche locali
- [ ] Ho fatto `git pull` senza errori
- [ ] Ho risolto eventuali conflitti
- [ ] Ho pushato con messaggio "Heartbroken: ..." o "AURA: ..."

---

**In caso di dubbio, chiedi prima di pushare.** 🚀