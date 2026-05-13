# Paper 14 — Guida per l'Utente
## Quick Start, Interfaccia e Flussi di Lavoro

```
────────────────────────────────────────────────────────────────
  WHYCREMISI RESEARCH PAPERS — N.14
  Guida per l'Utente — Quick Start
  
  "L'AI non si configura. Si usa."
────────────────────────────────────────────────────────────────
```

**Versione:** 1.0  
**Categoria:** Manuale Utente  
**Piattaforme:** macOS 12+ · Windows 10+ · Linux (Wine/Ubuntu Studio)

---

## 1. Introduzione

### 1.1 Cos'è WhyCremisi

WhyCremisi è un **ponte VST3/AU potenziato dall'intelligenza artificiale** che collega la tua DAW a un agente AI in grado di ascoltare, analizzare e controllare in tempo reale ogni parametro di ogni plugin nella tua sessione.

| Cosa FA | Cosa NON fa |
|---------|-------------|
| Ascolta il segnale audio in tempo reale | Non sostituisce il produttore |
| Controlla parametri di qualsiasi plugin VST3/AU | Non genera musica autonoma |
| Analizza spettro, fase, dinamiche e LUFS | Non carica plugin inesistenti |
| Propone e applica catene di effetti | Non modifica il routing audio |
| Ricorda preferenze e sessioni passate | Non registra l'audio della sessione |
| Comunica via testo e comandi vocali | Non bypassa la sicurezza della DAW |

### 1.2 Requisiti di Sistema

```
┌──────────────────────────┬────────────────────────────────────┐
│ Componente               │ Requisito                          │
├──────────────────────────┼────────────────────────────────────┤
│ Sistema Operativo        │ macOS 12+ / Windows 10+ / Linux    │
│                          │ (Wine 8+ o Ubuntu Studio 22.04+)   │
│ DAW                      │ Ableton Live 11+ / Reaper 6+ /    │
│                          │ Logic Pro / FL Studio 21+ /        │
│                          │ Pro Tools 2023+                    │
│ Formati Plugin           │ VST3 / AU (macOS)                  │
│ Connessione              │ Internet (necessaria per AI)       │
│ RAM                      │ 8 GB consigliati (16 GB per        │
│                          │ sessioni grandi)                   │
│ Spazio Disco             │ 500 MB per l'installazione         │
└──────────────────────────┴────────────────────────────────────┘
```

**[NOTE]** WhyCremisi funziona offline per il controllo plugin e l'analisi audio, ma richiede connessione internet per le funzionalità AI. In assenza di rete, l'interfaccia passa in modalità "solo analisi".

---

## 2. Installazione

### 2.1 Procedura di Installazione

```
Passo 1: Scarica l'ultima release da whycremisi.com/download
         └── File: WhyCremisi_x.x.x.dmg (macOS) / .exe (Windows)

Passo 2: Monta il DMG (macOS) o esegui l'installer (Windows)
         └── Trascina WhyCremisi.app nella cartella Applicazioni

Passo 3: Avvia WhyCremisi (comparirà nell'icona di sistema)
         └── Verifica che il servizio WebSocket sia in ascolto
             (porta 9800 predefinita)

Passo 4: Configura la DAW per rilevare il plugin
         └── Vedi §2.2 per ogni DAW specifica
```

### 2.2 Attivazione nella DAW

**Ableton Live 11+**
```
1. Apri Ableton → Preferences → Plug-Ins
2. In "Plugin Sources" aggiungi: /Library/Audio/Plug-Ins/VST3
3. Clicca "Rescan" (o "Rescan All" per pulire la cache)
4. Trascina "WhyCremisi" dal Browser Plugin su una traccia
```

**Reaper 6+**
```
1. Apri Reaper → Preferences → VST
2. In "VST plugin paths" aggiungi: /Library/Audio/Plug-Ins/VST3
3. Clicca "Clear cache / rescan"
4. FX → "WhyCremisi" → Add
```

**Logic Pro**
```
1. Apri Logic → Preferences → Audio → Plug-In Manager
2. Verifica che WhyCremisi sia listato sotto "AU Instruments"
3. Clicca "Reset & Rescan Selection" se non compare
4. Inserisci su una traccia strumentale: Audio FX → WhyCremisi
```

### 2.3 Configurazione WebSocket e OSC

```
┌─────────────────────────────────────────────────────────────┐
│  Impostazioni di Connessione (Impostazioni → Rete)          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  WebSocket Server    ws://localhost:9800  [●] Attivo        │
│  WebSocket Password  ************         [Modifica]        │
│  OSC Server          osc.udp://localhost:9000  [●] Attivo   │
│  OSC Porta Input     9000                                   │
│  OSC Porta Output    9001                                   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**[NOTE]** Le porte WebSocket (9800) e OSC (9000-9001) sono predefinite. Se in conflitto con altri servizi, modificale in Impostazioni → Rete.

---

## 3. Prima Connessione

### 3.1 Avvio del Plugin

```
  1. Inserisci WhyCremisi su una traccia nella tua DAW
  2. L'interfaccia principale si apre automaticamente
  3. Osserva gli indicatori di stato nella barra superiore
```

### 3.2 Stato della Connessione

La barra di stato mostra tre indicatori colorati:

```
┌──────────────────────────────────────────────────────────────┐
│  Stato Collegamenti                   ┌─────┐ ┌─────┐ ┌────┐ │
│                                       │ DAW │ │  WS │ │ AI │ │
│   ● Tutti i sistemi operativi         │  ●  │ │  ●  │ │ ●  │ │
│                                        └─────┘ └─────┘ └────┘ │
└──────────────────────────────────────────────────────────────┘
```

| Indicatore | Colore | Significato |
|------------|--------|-------------|
| DAW | ● Verde | Plugin rilevato e sincronizzato con l'host |
| DAW | ● Giallo | Plugin caricato ma non sincronizzato |
| DAW | ● Rosso | Plugin non rilevato — verifica installazione |
| WS (WebSocket) | ● Verde | Connessione WebSocket attiva |
| WS (WebSocket) | ● Rosso | WebSocket disconnesso — riavvia il servizio |
| AI | ● Verde | Agente AI connesso e operativo |
| AI | ● Giallo | AI in elaborazione |
| AI | ● Rosso | AI non disponibile — verifica connessione internet |

### 3.3 Primo Comando

```
  —Nella chat AI, digita o pronuncia un comando semplice—
  
  Utente > "Ciao, analizza la traccia corrente"
  
  WhyCremisi > Analisi completata. Ecco un riepilogo:
              
              ● LUFS Integrato:  -18.3  (ideale per mixing)
              ● True Peak:       -2.1 dBTP
              ● Range Dinamico:  12.4 dB
              ● LRA (Loudness Range):  8.7 dB
              
              La traccia ha buona dinamica. 
              Vuoi che suggerisca dei miglioramenti?
```

**[NOTE]** Se l'AI non risponde entro 5 secondi, verifica la connessione internet in Impostazioni → Rete → Test Connessione.

---

## 4. Interfaccia Principale

```
┌──────────────────────────────────────────────────────────────────────┐
│  WHYCREMISI — Interfaccia Principale               [—] [□] [×]      │
├──────────────────────────────────────────────────────────────────────┤
│  ┌──────────────────────────────────────────────────────────────────┐│
│  │  ▶  ■  ●  ⏸  ⏮  ⏭    |  BPM: 128  |  Time: 01:23:45          ││
│  │  TRASPORT BAR                                        [PREFS]   ││
│  └──────────────────────────────────────────────────────────────────┘│
├────────────────────────────────┬──────────────────────────────────┬──┤
│ ┌──────────────────────────┐  │ ┌──────────────────────────────┐ │  │
│ │  LEVEL METERS            │  │ │  SPECTROGRAM (FFT)           │ │  │
│ │  ┌─┐ ┌─┐                │  │ │  ┌────────────────────────┐  │ │  │
│ │  │█│ │█│  L: -18.3 LUFS │  │ │  │  ░░▒▒▓▓██▓▓▒▒░░       │  │ │  │
│ │  │█│ │█│  R: -18.1 LUFS │  │ │  │  ░░▒▒▓▓██▓▓▒▒░░       │  │ │  │
│ │  │█│ │█│  Peak L: -2.1  │  │ │  │  20Hz ─── 20kHz       │  │ │  │
│ │  └─┘ └─┘  Peak R: -2.4  │  │ │  └────────────────────────┘  │ │  │
│ ├──────────────────────────┤  │ ├──────────────────────────────┤ │  │
│ │  CORRELATION METER       │  │ │  OSCILLOSCOPE                │ │  │
│ │     ╱‾‾‾╲               │  │ │  ┌────────────────────────┐  │ │  │
│ │    ╱  +1  ╲              │  │ │  │   ~~~~~~~~~~          │  │ │  │
│ │   ╱   ●    ╲      +0.92  │  │ │  │  ~~        ~~        │  │ │  │
│ │   ╲        ╱             │  │ │  │ ~~          ~~       │  │ │  │
│ │    ╲  -1  ╱              │  │ │  └────────────────────────┘  │ │  │
│ │     ╲___╱                │  │ │                              │ │  │
│ └──────────────────────────┘  │ └──────────────────────────────┘ │  │
├────────────────────────────────┴──────────────────────────────────┴──┤
│  ┌──────────────────────────────────────────────────────────────────┐│
│  │  AI CHAT                                  [Send] [🎤] [⚙]    ││
│  │  ┌────────────────────────────────────────────────────────────┐││
│  │  │ Utente: "Compress the vocals"                            │││
│  │  │ WhyCremisi: Ho applicato un compressore con ratio 3:1,   │││
│  │  │ attack 10ms, release 50ms, threshold -18dB. Ascolta.     │││
│  │  └────────────────────────────────────────────────────────────┘││
│  └──────────────────────────────────────────────────────────────────┘│
├──────────────────────────────────────────────────────────────────────┤
│  ┌──────────────────────┐ ┌──────────────────────────────────────┐  │
│  │  PLUGIN BROWSER      │ │  PRESET MANAGER                      │  │
│  │  ○ FabFilter Pro-Q3  │ │  ┌────────────────────────────────┐  │  │
│  │  ○ iZotope Ozone 11  │ │  │ Vocal Mix — Current            │  │  │
│  │  ○ Waves SSL-G       │ │  │ Drum Bus — Loaded              │  │  │
│  │  ○ > show all (23)   │ │  │ [Save Current] [Browse...]     │  │  │
│  └──────────────────────┘ └──────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────────┘
```

### 4.1 Trasport Bar

| Comando | Icona | Descrizione |
|---------|-------|-------------|
| Play | ▶ | Avvia la riproduzione |
| Stop | ■ | Ferma la riproduzione |
| Record | ● | Avvia la registrazione |
| Pause | ⏸ | Mette in pausa |
| Rewind | ⏮ | Torna all'inizio |
| Fast Forward | ⏭ | Va alla fine |

La transport bar mostra anche BPM corrente e timecode della sessione.

### 4.2 Level Meters

Indicatori stereo con doppia lettura:

```
  L [████████████░░░░░░░░] -18.3 LUFS  Peak: -2.1 dBTP
  R [█████████████░░░░░░░] -18.1 LUFS  Peak: -2.4 dBTP
```

- **Barra verde:** livello sicuro (sotto -6 dBTP)
- **Barra gialla:** livello caldo (-6 a -1 dBTP)
- **Barra rossa:** clipping (sopra -1 dBTP)

### 4.3 Correlation Meter

Misura la correlazione di fase tra canale L e R:

```
  +1  ─── ● ───  Fase perfetta (mono compatibile)
   0  ────────   Nessuna correlazione
  -1  ────────   Fuori fase (problemi)
```

Valori ideali: +0.5 a +1.0 per mix bilanciati.

### 4.4 Oscilloscopio

Visualizzazione della forma d'onda in tempo reale. Utile per:
- Identificare transienti (batteria, percussioni)
- Visualizzare saturazione e clipping
- Confrontare dinamiche pre/post processing

### 4.5 Spettrogramma (FFT)

Analisi frequenziale con risoluzione selezionabile (512-8192 bins):

| Banda Frequenziale | Range | Uso Principale |
|--------------------|-------|----------------|
| Sub-bass | 20-60 Hz | Kick, subwoofer |
| Bass | 60-250 Hz | Basso, fondamentali |
| Low-mid | 250-500 Hz | Corpo strumenti, mud |
| Mid | 500-2000 Hz | Voce, chitarra, presenza |
| Upper-mid | 2000-4000 Hz | Presenza, sibilanti |
| High | 4000-20000 Hz | Aria, brillantezza, hi-hat |

### 4.6 AI Chat Window

```
┌─────────────────────────────────────────────────────────────┐
│  AI CHAT WINDOW                      [Send] [🎤] [⚙]      │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  [Cronologia]                                               │
│  >>> "Analyze the master bus"                              │
│  <<< Master: LUFS -14.2, TP -1.3dB. Il True Peak è alto.  │
│      Suggerisco di abbassare il threshold del limiter.     │
│                                                             │
│  [Suggerimento rapido]                                     │
│  ● Add sidechain compression to the kick and bass          │
│  ● Apply a gentle high-pass on the reverb send             │
│  ● Compare mix with reference track                        │
│                                                             │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ Type a command or ask a question...                  │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

### 4.7 Plugin Browser

Elenco di tutti i plugin VST3/AU rilevati nella sessione corrente. Per ogni plugin mostra:
- Nome e produttore
- Versione installata
- Stato (caricato/inattivo)
- Parametri esposti (click per espandere)

### 4.8 Preset Manager

Salva e carica configurazioni complete:

```
  Preset: "Vocal_Mix_v2"
  Include:
    ✓ EQ settings (FabFilter Pro-Q3)
    ✓ Compression (Pro-C2)
    ✓ Reverb (Pro-R)
    ✓ AI chat context
    ✓ LUFS target: -14
```

**[NOTE]** I preset salvano lo stato dei plugin controllati via WhyCremisi, non l'intera sessione DAW.

---

## 5. Flussi di Lavoro

### 5.1 Mixing Assistito: "Compress the Vocals"

```
  Utente: "Compress the vocals"

  1. WhyCremisi analizza la traccia vocale
     → LUFS: -21.3, Dynamic Range: 14.2 dB
     → Transienti vocali: 8.2 dB di picco medio

  2. WhyCremisi propone:
     ┌──────────────────────────────────────────────────────┐
     │  Proposta: Compressore su Traccia Vocale             │
     │                                                      │
     │  Plugin: FabFilter Pro-C 2 (Clean mode)              │
     │  ● Threshold:   -18.5 dB                             │
     │  ● Ratio:        3.0 : 1                             │
     │  ● Attack:       8 ms                                │
     │  ● Release:      45 ms                               │
     │  ● Makeup:       +2.5 dB                             │
     │  ● Output:       -1.5 dB (headroom)                  │
     │                                                      │
     │  [ANTEPRIMA]  [APPLICA]  [MODIFICA]                  │
     └──────────────────────────────────────────────────────┘

  3. Utente clicca [ANTEPRIMA] → ascolta A/B
  4. Utente clicca [APPLICA]
  5. WhyCremisi registra nell'history: "Vocals compressed — 3:1 ratio"
```

### 5.2 Masterizzazione: "Apply a Master Chain"

```
  Utente: "Apply a master chain for Spotify"

  1. WhyCremisi analizza il master bus
     → LUFS: -18.3, TP: -2.1 dBTP
     → Target Spotify: LUFS -14 integrato

  2. Catena proposta:
     ┌──────────────────────────────────────────────────────┐
     │  MASTER CHAIN - Spotify Target                        │
     │                                                      │
     │  Pos 1: FabFilter Pro-Q 3    — EQ correttivo        │
     │  Pos 2: iZotope Ozone 11     — Exciter + Imager     │
     │  Pos 3: FabFilter Pro-C 2    — Compressione stereo   │
     │  Pos 4: FabFilter Pro-L 2    — Limiter               │
     │                                                      │
     │  Target: LUFS -14 integrato, TP -1.0 dBTP            │
     │                                                      │
     │  [ANTEPRIMA]  [APPLICA]  [MODIFICA]                  │
     └──────────────────────────────────────────────────────┘
```

### 5.3 Suono Creativo: "70s Analog Synth"

```
  Utente: "Make this sound like a 70s analog synth"

  1. WhyCremisi analizza timbro corrente
  2. Applica catena di effetti:
     ┌──────────────────────────────────────────────────────┐
     │  CHAIN: "70s Analog Synth"                            │
     │                                                      │
     │  ● Saturn 2          — Tape saturation, warm drive   │
     │  ● Pro-Q 3           — Low-pass filter (-3dB @ 8kHz) │
     │  ● RC-20 Retro Color — Wow & Flutter + Noise         │
     │  ● Pro-R             — Spring reverb (decay: 2.5s)   │
     │                                                      │
     │  [ANTEPRIMA]  [APPLICA]  [SALVA COME PRESET]         │
     └──────────────────────────────────────────────────────┘
```

### 5.4 Risoluzione Problemi: "Fix the Bass Muddiness"

```
  Utente: "Fix the bass muddiness"

  1. WhyCremisi analizza FFT della traccia basso
     → Identifica accumulo a 180-350Hz
     → Rapporto segnale/rumore degradato nella fascia low-mid

  2. WhyCremisi propone:
     ┌──────────────────────────────────────────────────────┐
     │  Analisi: Muddiness nel basso                        │
     │                                                      │
     │  Causa: Accumulo di frequenze 180-350Hz              │
     │                                                      │
     │  Soluzione Suggerita:                                │
     │  ● FabFilter Pro-Q 3 — Banda Peak centrata a 240Hz  │
     │  ● Gain: -4.2 dB                                     │
     │  ● Q: 2.5 (stretto)                                  │
     │                                                      │
     │  [ANTEPRIMA A/B]  [APPLICA]  [AFFINA]               │
     └──────────────────────────────────────────────────────┘
```

---

## 6. Comandi Vocali/Testo

### 6.1 Categorie di Comandi

| Categoria | Esempio | Azione |
|-----------|---------|--------|
| Volume/Gain | "Set reverb send to -12 dB" | Regola parametro |
| Effetti | "Add sidechain compression to the kick and bass" | Crea catena |
| Routing | "Create a parallel compression chain on the drums" | Routing + FX |
| Analisi | "Analyze the master bus for clipping" | Report dettagliato |
| Confronto | "Compare this mix with the reference track" | A/B analysis |
| Preset | "Load the 'Warm Vocal' preset" | Carica preset |
| Trasporto | "Play from bar 24" | Controllo DAW |
| EQ | "High-pass the guitars at 120Hz" | Filtro specifico |

### 6.2 Esempi Dettagliati

```
Comando: "Set reverb to 25%"
───────────────────────────────────────
Risultato: Wet/Dry del riverbero portato da 40% a 25% sulla traccia corrente.

Comando: "Add sidechain compression to the kick and bass"
───────────────────────────────────────
Risultato: WhyCremisi:
  1. Crea un compressore sulla traccia basso
  2. Imposta sidechain input dalla traccia kick
  3. Configura: ratio 4:1, attack 1ms, release 50ms
  4. Ratio: 4:1, threshold automatico basato sul livello kick

Comando: "Create a parallel compression chain on the drums"
───────────────────────────────────────
Risultato: WhyCremisi:
  1. Crea traccia return "Drums Parallel"
  2. Invio dalla traccia drum a -10dB
  3. Aggiunge compressore con attack 30ms, high ratio 8:1
  4. Mix al 50%

Comando: "Analyze the master bus for clipping"
───────────────────────────────────────
Risultato: 
  ● True Peak: -0.3 dBTP — Clipping rilevato!
  ● Sample di clipping: 1,247 campioni (0.03% del totale)
  ● Suggerimento: Riduci il gain del limiter di 1.5dB
                    o applica un soft clipper prima del limiter.

Comando: "Compare this mix with the reference track"
───────────────────────────────────────
Risultato:
  ┌─────────────────────────────────────────────────────────┐
  │  CONFRONTO: Mix Corrente vs Reference                    │
  │                                                         │
  │  Metrica           Mix         Reference    Δ           │
  │  ────────────────────────────────────────────────────   │
  │  LUFS Integrato    -14.2       -14.0        -0.2 OK     │
  │  True Peak         -1.3 dB     -0.8 dB      -0.5 dB     │
  │  LRA               6.2 dB      7.1 dB       -0.9 dB     │
  │  EQ Shape          250Hz +3dB  250Hz flat    → EQ       │
  │  Stereo Width      68%         72%           -4%        │
  └─────────────────────────────────────────────────────────┘
```

---

## 7. Preferenze e Impostazioni

### 7.1 Interfaccia

```
┌─────────────────────────────────────────────────────────────┐
│  PREFERENZE — Interfaccia                                    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ● Tema             ○ Scuro  ○ Chiaro  ○ Sistema            │
│                                                             │
│  ● Lingua           ○ Italiano  ● English                    │
│                                                             │
│  ● Dimensione       ○ 100%  ○ 125%  ○ 150%  ● 200%         │
│    Finestra                                                   │
│                                                             │
│  ● Dimensione       ● Normale  ○ Compatta                   │
│    Chat                                                     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 7.2 Shortcut da Tastiera

| Shortcut | Azione |
|----------|--------|
| `⌘+Enter` | Invia comando chat |
| `⌘+K` | Focus sulla chat |
| `⌘+L` | Pulisci cronologia chat |
| `⌘+M` | Mute/Unmute output |
| `⌘+,` | Apri preferenze |
| `Space` | Play/Pause DAW |
| `⌘+Z` | Annulla ultima azione AI |
| `⌘+Shift+Z` | Ripristina ultima azione |
| `Esc` | Chiudi modale |

### 7.3 Comportamento AI

```
┌─────────────────────────────────────────────────────────────┐
│  PREFERENZE — Comportamento AI                               │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ● Creatività         ○ Bassa  ● Media  ○ Alta              │
│    (bassa = segue strettamente le regole di mixing)          │
│    (alta = sperimenta con catene creative)                   │
│                                                             │
│  ● Verbosità          ○ Sintetico  ● Dettaglio  ○ Esteso    │
│                                                             │
│  ● Auto-apply         ○ Mai  ○ Dopo conferma  ● Always     │
│                                                             │
│  ● Volume Alert       ● Attivo  ○ Disattivo                 │
│    (soglia: -1 dBTP)                                        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**[NOTE]** La modalità "Auto-apply • Always" è sconsigliata per utenti inesperti. WhyCremisi suggerisce di iniziare con "Dopo conferma".

---

## 8. Risoluzione dei Problemi

### 8.1 FAQ Rapida

```
PROBLEMA: Il plugin non compare nella DAW
─────────────────────────────────────────
  Cause possibili:
  ● Plugin non installato correttamente
    → Verifica: /Library/Audio/Plug-Ins/VST3/WhyCremisi.vst3
  ● Cache DAW non aggiornata
    → Ableton: Rescan All; Reaper: Clear cache/rescan
  ● Formato non supportato dalla DAW
    → Logic richiede AU (non VST3)
    → FL Studio richiede VST3 (non AU)

PROBLEMA: Nessun suono dopo l'inserimento
─────────────────────────────────────────
  Cause possibili:
  ● WhyCremisi in bypass
    → Disattiva bypass dal plugin header
  ● Routing audio errato
    → Verifica che il segnale passi attraverso il plugin
  ● Plugin in modalità "solo analisi"
    → Controlla impostazioni modalità operativa

PROBLEMA: L'AI non risponde
─────────────────────────────
  1. Verifica connessione internet
  2. Controlla lo stato AI (● Rosso?)
  3. Riavvia il servizio WhyCremisi
  4. Controlla firewall (porta 443 per API AI)
  5. Riavvia la DAW

PROBLEMA: OSC non funziona
─────────────────────────────
  1. Verifica che la porta OSC (9000) non sia occupata
  2. Controlla che il target IP sia corretto
  3. Reaper: Abilita "Control surface OSC" in Preferences
  4. Prova: `nc -u localhost 9000` da terminale

PROBLEMA: WebSocket disconnesso
─────────────────────────────────
  1. Riavvia il servizio WebSocket di WhyCremisi
  2. Verifica che la porta 9800 sia libera
  3. Controlla se VPN/proxy interferisce con WebSocket
  4. Log: ~/Library/Logs/WhyCremisi/websocket.log
```

### 8.2 Log Viewer

```
  Menu → Aiuto → Log Viewer
  
  ┌────────────────────────────────────────────────────────────┐
  │  LOG VIEWER                            [Clear] [Export]   │
  ├────────────────────────────────────────────────────────────┤
  │  [12:34:01] INF Plugin loaded: WhyCremisi v1.0.0          │
  │  [12:34:02] INF WebSocket connected on port 9800           │
  │  [12:34:03] INF AI agent connected (model: gpt-4o)        │
  │  [12:34:05] DBG Track analysis: LUFS=-21.3, DR=14.2      │
  │  [12:34:10] INF AI suggestion: "Compress the vocals"     │
  │  [12:34:12] ACT Applied: Pro-C2 threshold=-18.5, ratio=3 │
  │  [12:34:15] WRN True Peak approaching limit: -1.3 dBTP   │
  │  [12:34:20] ERR WebSocket connection lost — reconnecting  │
  │  [12:34:22] INF WebSocket reconnected successfully        │
  └────────────────────────────────────────────────────────────┘
  
  Livelli di log: INF (info), DBG (debug), WRN (warning), ERR (error), ACT (azione)
```

---

## 9. Consigli e Best Practice

### 9.1 Per Iniziare

```
  ╔══════════════════════════════════════════════════════════════╗
  ║             5 Consigli per Nuovi Utenti                      ║
  ╠══════════════════════════════════════════════════════════════╣
  ║                                                              ║
  ║  1. INIZIA CON UN BRANO SEMPLICE                             ║
  ║     → 4-8 tracce, pochi plugin, genere conosciuto           ║
  ║                                                              ║
  ║  2. USA SEMPRE L'ANTEPRIMA                                   ║
  ║     → Ascolta prima di applicare qualsiasi modifica          ║
  ║     → Il pulsante [ANTEPRIMA] non modifica nulla             ║
  ║                                                              ║
  ║  3. VERIFICA I PARAMETRI PRIMA DI CONFERMARE                ║
  ║     → Leggi i valori proposti dall'AI                        ║
  ║     → Regola manualmente se necessario                       ║
  ║                                                              ║
  ║  4. USA I PRESET COME PUNTO DI PARTENZA                     ║
  ║     → I preset danno una base solida                        ║
  ║     → Personalizza sempre per il tuo brano                   ║
  ║                                                              ║
  ║  5. DAI FEEDBACK COSTANTE ALL'AI                            ║
  ║     → "Too much compression" → l'AI regola                   ║
  ║     → "More presence" → l'AI aggiunge EQ                    ║
  ║     → Più feedback = risultati migliori                      ║
  ║                                                              ║
  ╚══════════════════════════════════════════════════════════════╝
```

### 9.2 Workflow Ideale

```
  FASE 1 — ANALISI
  ─────────────────
  Carica il brano → "Analyze the session"
  WhyCremisi analizza ogni traccia e propone ottimizzazioni

  FASE 2 — MIX GREZZO
  ─────────────────────
  Applica le correzioni EQ → "Remove mud from bass"
  Imposta livelli base → "Set all track levels to -18 LUFS"
  Bilanciamento stereo → "Widen the pad stereo image"

  FASE 3 — MIX FINALE
  ─────────────────────
  Compressione tracce → "Compress the drum bus 4:1"
  Effetti e riverbero → "Add room reverb to the vocals"
  Automazioni → "Automate the filter cutoff in the bridge"

  FASE 4 — MASTERING
  ───────────────────
  "Apply a master chain for Spotify"
  Verifica finale → "Analyze the master bus"
  Export → "Export to WAV 48kHz 24-bit"
```

### 9.3 Feedback Efficace

```
  ❌ "Non mi piace"              → Troppo vago
  ✅ "La compressione è troppa,  → Specifico, orientato
     riduci il ratio a 2:1"        all'azione

  ❌ "Aggiungi effetti"          → Troppo generico
  ✅ "Aggiungi un riverbero      → Specifico: tipo, posizione
     a coda lunga sulla voce"

  ❌ "Sistema il basso"          → Non dice cosa non va
  ✅ "Il basso ha troppo         → Identifica causa + range
     mud a 200-300Hz"
```

### 9.4 Errori Comuni da Evitare

| Errore | Conseguenza | Soluzione |
|--------|-------------|-----------|
| Applicare senza ascoltare | Risultati imprevisti | Usa sempre ANTEPRIMA |
| Comandi troppo vaghi | Risposta imprecisa | Sii specifico |
| Ignorare i warning AI | Clipping o fase errata | Leggi i messaggi AI |
| Non salvare preset | Perdita configurazioni | Salva preset dopo ogni sessione |
| Troppi comandi in fila | Confusione nel processing | Aspetta la risposta AI |

---

```
────────────────────────────────────────────────────────────────
  "WhyCremisi non è un plugin: è un produttore in una scatola."
────────────────────────────────────────────────────────────────
```

*→ Continua in: [Paper 15 — API e Sviluppo Plugin](15-API-SVILUPPO-PLUGIN.md)*
