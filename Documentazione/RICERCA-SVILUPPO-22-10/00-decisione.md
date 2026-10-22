# Decisione sviluppo, 22/10/2026

Base: le quattro ricerche in questa cartella (01 DAW, 02 skill e tool, 03 browser e box, 04 sogni, analisi, abbonamenti).
Regola: ogni scelta si misura su "lo usa chiunque senza installare niente" e "funziona in ogni DAW".

## 1. Controllo di ogni DAW
- Un solo nucleo di comandi (play, tempo, tracce, volume, pan, mute, solo, parametri). Sotto, adattatori.
- Adattatori per DAW, installati dal plugin con un clic dell'utente: Reaper, Ableton (AbletonOSC, MIT), FL Studio, Cubase, Bitwig.
- Rete di sicurezza per tutte le altre: Mackie Control / HUI su porta MIDI virtuale. Limite: 8 fader, niente clip.
- Logic e GarageBand: solo Mac. Pro Tools: solo HUI. Studio One e Reason: da verificare.
- Solo licenze permissive. Niente DrivenByMoss, Ardour o codice GPL dentro il plugin.
- Ordine di lavoro: Reaper, poi Ableton (esiste già), poi MCU, poi FL, Cubase, Bitwig.

## 2. Skill e tool (host)
- Formato unico SKILL.md. FATTO: `src/skills/SkillLibrary` (143c7c5).
- Radici da leggere: .claude, .codex, .agents, OpenCode, OpenClaw, Hermes. Le nostre sono fidate, le altre marcate "terzi".
- Client MCP minimale in C++ (initialize, tools/list, tools/call) per i tool esterni. Nessun SDK ufficiale C++: lo scriviamo noi.
- Testo di una skill di terzi = materiale da leggere, mai ordini. Quarantena prima dell'uso.

## 3. Browser interno, box, pensiero
- Browser: WebView2 con porta di debug e Playwright via CDP su Windows. Sessione dedicata visibile, oppure in background.
- YouTube: solo il lettore ufficiale (IFrame API). Niente estrazione audio e niente analisi dell'audio: i termini lo vietano. Da dire a Whyed.
- Box: il modello sceglie tra testo e box; formato dichiarativo (stile AG-UI / A2UI), disegnati dalla sessione design.
- Pensiero visibile con un tasto: il testo del ragionamento viaggia a parte dalla risposta.

## 4. Anima, memoria, sogni
- Cinque file per progetto: SOUL.md, IDENTITY.md, USER.md, MEMORY.md, DREAMS.md (come OpenClaw, ma con ricordi annullabili e mai cancellati).
- Sogno = passo in background che riordina i ricordi e promuove a MEMORY.md solo quelli qualificati, con diario in DREAMS.md.
- Si addormenta in modo stocastico: p = 1 - exp(-lambda * minuti fermi), azzerato a ogni input, frenato se ci sono ricordi nuovi. Modello locale piccolo.

## 5. Analisi audio
- Licenze permissive: librosa, Whisper, Demucs, basic-pitch, audio-separator (MIT / Apache).
- Già sul PC: faster-whisper, librosa, modelli Whisper. Demucs e basic-pitch da aggiungere in un venv dedicato.
- Essentia (AGPL) e Matchering (GPL): non dentro il plugin chiuso.

## 6. Abbonamenti
- Anthropic vieta l'uso dei token OAuth del piano in strumenti di terzi (da febbraio 2026). Restano: API key, oppure il programma ufficiale lanciato dall'utente. Zona grigia: da decidere con Whyed.
- OpenAI, Google, OpenRouter: da verificare sui termini ufficiali prima di promettere.

## Ordine del codice
1. SkillLibrary (fatto).
2. Anima del progetto: SOUL/IDENTITY/USER/MEMORY/DREAMS + caricamento nel prompt.
3. Sogno stocastico.
4. Tool `skill.search` / `skill.load`.
5. Client MCP.
6. Strato DAW: nucleo + Reaper + MCU.
7. Browser interno e box (con la sessione design).

## Da dire a Whyed
YouTube, passo manuale in Live, BM25 invece di FTS5, login Claude, licenze da verificare (OpenClaw, Hermes, mcpp, assistant-ui, browser-use, SWS), mail JUCE/openDAW non inviate, merge non fatto.
