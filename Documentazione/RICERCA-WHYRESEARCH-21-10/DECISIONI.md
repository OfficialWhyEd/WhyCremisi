# DECISIONI sviluppo WhyCremisi (22/10/2026)

Fonte: 12 ricerche in questa cartella, lette da 3 lettori Haiku (riassunti) e controllate col codice.
Regola: licenze dei pesi quasi mai lette. Ogni punto marcato VERIFICARE va chiuso prima di distribuire.
Il plugin e' chiuso: niente GPL/AGPL/CC-NC linkati dentro.

## Obiettivo guida (Whyed 21/10)
Funziona anche se non c'e' installato NULLA. Il plugin scarica da solo AbletonOSC, modelli, helper.
L'utente sceglie solo account/abbonamento e modello.

## 1. Ponte con la DAW
- Ableton: AbletonOSC (MIT) gia' usato. Installazione automatica = codice di terzi, DIRLO a Whyed prima di eseguire.
- Il plugin sul master vede solo transport: tracce e device passano dal ponte (device.list/params/setParam gia' fatti).
- Auto-riparazione: parametri cercati per NOME, non per indice (indici cambiano tra Live 12.x).
- Server MCP: da costruire (nessun repo), solo 127.0.0.1, controllo Origin, token.
- Altre DAW: Reaper (python-reapy MIT, gia' ReaperDawHandler), Pro Tools SDK Avid piu' avanti. Logic, Studio One, FL, Cubase: niente API, solo transport/MIDI.
- VERIFICARE: AbletonOSC su Live 12 reale, caricamento device non risolto (workaround Simpler).

## 2. Modelli e login
- Catena: DeepSeek via OpenRouter, poi :free, poi Gemini free, Groq free, locale.
- OpenRouter PKCE: fatto. DeepSeek V4.1 Flash potrebbe non essere :free: VERIFICARE esistenza e prezzo.
- Gemini free: i prompt possono servire a Google. Dirlo all'utente nella scelta.
- ChatGPT col programma Codex: fatto, ma la ricerca dice che e' pensato per Codex CLI e partner. Non salviamo token. Zona grigia da tenere d'occhio.
- LOGIN CLAUDE con `claude -p`: i termini Anthropic limitano l'abbonamento a Claude Code e app native. ZONA GRIGIA. Decisione tecnica: resta come opzione esplicita, spenta di default, con avviso chiaro; la via piena e' la API key dell'utente. DECISIONE FINALE A WHYED + parere legale.
- Race Groq+Gemini: codice nuovo (~40 righe), non esiste in nessun repo. Riuso decisore.mjs di System One come router.
- Locale: llama.cpp server (porta 9931) / Ollama, 4-8B quantizzati. RAM da misurare, stop sotto 3 GB liberi.
- Senza censura (heretic): AGPL non verificata, derivato eredita licenza del base. Non distribuire, solo uso locale suo.

## 3. Audio e analisi
- C++ nel plugin: libebur128 (MIT) per LUFS e true peak per traccia. Key/spettro/stereo/dinamica: codice nostro.
- Helper Python scaricato a richiesta: Beat This! (MIT, 8 MB), basic-pitch (Apache), CLAP (CC0). Misurare su 1000 sample.
- SCARTATI: Essentia (AGPL), aubio (GPL), MuQ e Music2Latent (NC), madmom (modelli NC).
- Spettrogramma come immagine: aiuto, non fonte (i VLM lo leggono male).
- Visione: Qwen3-VL via Ollama. VERIFICARE licenza sulla card ufficiale e tool calling con immagini.

## 4. Plugin Dictionary e mix
- Metodo: introspezione parametri + sweep misurato, dizionario JSON per plugin, LLM etichetta solo i dati.
- clap-info (MIT) per leggere i CLAP. pluginval (GPL) solo come test in sviluppo, mai distribuito.
- Auto-mix: solo idee (brevetto Dolby su Steinmetz). automix-toolkit (Apache) solo codice.
- Un numero si mostra solo se misurato.

## 5. Musica e MIDI
- ACE-Step 1.5 (MIT codice) audio, MIDI-GPT (MIT), Anticipatory Music Transformer (Apache) per continuare MIDI, Text2MIDI.
- SCARTATO: MusicGen (pesi NC).
- VERIFICARE licenza pesi, VRAM reale. PC ha 2 GB GPU: generazione audio non qui, MIDI si.
- Policy: niente nomi di artista nel prompt, niente LoRA su artisti reali.

## 6. Memoria
- File markdown per progetto (fonte) + indice SQLite FTS5. Ogni ricordo: data, fonte, stato (attivo/annullato/sostituito).
- Scritta su file prima di compattare. Niente server (Mem0, Letta, Zep scartati). basic-memory AGPL: zero codice.
- Embedding dopo: EmbeddingGemma (licenza Gemma, VERIFICARE).

## 7. Licenze e mercato (non bloccano lo sviluppo, bloccano la vendita)
- JUCE: licenza da chiarire prima di vendere. Mail NON inviata, serve OK di Whyed.
- openDAW AGPL: sito chiuso richiede licenza commerciale. Mail NON inviata.
- Spazio libero nel mercato: agente chat indipendente dalla DAW con memoria e ascolto del mix.

## Ordine di lavoro (parte 4 in poi)
1. Plugin Dictionary + Auto-Discovery (clap-info, sweep, JSON) e rimuovere lo stub daw.plugin.* in OscBridge.cpp
2. LUFS per traccia con libebur128
3. Router + race provider, Gemini verificato
4. Helper sample scaricabile e misurato
5. Installer automatico AbletonOSC (dopo avviso a Whyed) + collaudo in Ableton 11
6. Memoria markdown + FTS5
7. Server MCP, poi Live 12, poi altre DAW
Nessun merge senza OK di Whyed.
