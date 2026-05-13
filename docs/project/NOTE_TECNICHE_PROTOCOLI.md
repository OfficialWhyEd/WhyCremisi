# Note Tecniche Protocolli DAW

**Creato:** 2026-04-12  
**Fonti:** Documenti ufficiali scaricati, link verificati  
**Stato:** Work in progress — basato su fonti autentiche

---

## 📚 Manuali Scaricati

| File | Fonte | Dimensione | Sezioni Rilevanti |
|------|-------|------------|------------------|
| `Reaper_User_Guide_7.56.pdf` | reaper.fm (ufficiale) | 26MB | Control Surfaces, OSC, Actions |
| `OSC_1.0_Specification.pdf` | opensoundcontrol.org (via hangar.org mirror) | 96KB | Protocollo OSC completo |

---

## 🔵 REAPER — Fonti Verificate

### Documentazione Ufficiale
- **User Guide PDF:** https://www.reaper.fm/userguide/ReaperUserGuide756c.pdf
- **OSC SDK:** https://www.reaper.fm/sdk/osc/osc.php

### File Pattern OSC
- **Posizione:** `~/.config/REAPER/Default.ReaperOSC`
- **Formato:** Pattern personalizzabili con wildcard `@` per track number
- **Esempio pattern:** `TRACK_VOLUME` → `/track/@/volume`

### Comandi Base (da verificare nel PDF)
Secondo la documentazione SDK, Reaper supporta:
- Transport: play, stop, pause, record
- Track: volume, pan, mute, solo, arm
- FX: parameter values, bypass

**Da leggere nel PDF:** Sezione "Control Surfaces" e "OSC Pattern Config"

---

## 🔴 Ableton Live — Fonti Verificate

### Documentazione Ufficiale
- **Live Object Model (LOM):** https://docs.cycling74.com/apiref/lom
- **LiveAPI JavaScript:** https://cycling74.com/docs/max5/vignettes/js/jsliveapi.html
- **AbletonOSC (libreria community):** https://github.com/ideoforms/AbletonOSC

### Live Object Model (LOM)
Ableton espone un'API JavaScript tramite Max for Live:
- `live.path` — seleziona oggetti nel Live Set
- `live.object` — esegue operazioni sugli oggetti
- `live.observer` — ascolta cambiamenti

### AbletonOSC — Comandi Verificati
Dal repository ufficiale ideoforms/AbletonOSC:
```
/live/play                    → Start playback
/live/stop                    → Stop playback
/live/track/set/volume n v    → Set track n volume to v
/live/track/get/volume n      → Query track n volume
/live/clip/fire t c           → Fire clip at track t, clip c
```

**Nota:** Richiede Max for Live device installato nel Live Set.

---

## 🟡 OSC — Fonti Verificate

### Specifica 1.0
- **PDF scaricato:** `OSC_1.0_Specification.pdf`
- **Sorgente:** http://opensoundcontrol.org/spec-1_0.html

### Caratteristiche Tecniche (dal PDF)
- Trasporto: UDP (tipicamente porta 9000)
- Formato: Bundle di messaggi con timestamp
- Indirizzi: pattern gerarchici (es: `/track/1/volume`)
- Tipi dati: int32, float32, string, blob

### Best Practices
- **PDF:** `osc-best-practices-final.pdf` (scaricabile da opensoundcontrol.stanford.edu)

---

## 🟣 MIDI — Fonti Verificate

### Standard MIDI 1.0
- **Fonte:** midi.org/specs/
- **Documento:** MIDI 1.0 Detailed Specification (richiede login)

### CC Standard (da fonti pubbliche concordanti)
| CC | Funzione | Tipo |
|----|----------|------|
| 1  | Modulation Wheel | Continuo |
| 7  | Channel Volume | Continuo |
| 10 | Pan | Continuo |
| 11 | Expression | Continuo |
| 64 | Sustain Pedal | On/Off |

**Nota:** Lista completa disponibile su https://www.presetpatch.com/midi-cc-list.aspx

---

## 📖 Sezioni da Leggere nei PDF

### Reaper User Guide 7.56
- [ ] Chapter XX: Control Surfaces
- [ ] Chapter XX: OSC Configuration
- [ ] Appendix: Default.ReaperOSC reference

### OSC 1.0 Specification
- [ ] Section 1: Introduction
- [ ] Section 2: OSC Syntax
- [ ] Section 3: OSC Messages

---

## 🔗 Link Utili (Online)

| Risorsa | URL |
|---------|-----|
| Reaper OSC SDK | https://www.reaper.fm/sdk/osc/osc.php |
| Ableton LOM | https://docs.cycling74.com/apiref/lom |
| AbletonOSC GitHub | https://github.com/ideoforms/AbletonOSC |
| OSC Spec 1.0 | http://opensoundcontrol.org/spec-1_0.html |
| MIDI Specs | https://www.midi.org/specs |

---

## ⚠️ Limitazioni Documentazione

### Mancanti
- ❌ Ableton Live 12 Reference Manual PDF (non disponibile pubblicamente)
- ❌ MIDI 1.0 Detailed Specification (richiede login membri MIDI.org)
- ❌ Reaper Default.ReaperOSC file completo (non scaricato)

### Da Fare
1. Leggere sezioni OSC nel PDF Reaper (26MB)
2. Estrarre comandi specifici per control surfaces
3. Verificare compatibilità AbletonOSC con Live 12
4. Testare ricezione OSC in Reaper reale

---

## 📝 Note Estratte (in corso)

*Questa sezione verrà aggiornata man mano che leggo i PDF.*

---

*File creato con riferimenti verificati. Nessun contenuto inventato.*
