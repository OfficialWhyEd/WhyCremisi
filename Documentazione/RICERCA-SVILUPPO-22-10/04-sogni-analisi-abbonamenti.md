# Ricerca 22/10: sogni dell'agente, analisi audio, abbonamenti

Stato: ricerca web e lettura locale. Niente di installato, niente di modificato.
Legenda: [V] = licenza verificata sulla pagina del repo. [A] = trovata in aggregatori o blog. [M] = da memoria, da controllare.

---

## A. Dreaming: consolidamento della memoria mentre l'agente è fermo

### Cos'è
Un processo in background che rilegge la memoria quando l'utente non scrive.
Riordina, riassume, segnala contraddizioni. Non risponde a nessuno.

### Dove esiste
- **OpenClaw Dreaming**. Opt-in, disattivato di default. Tre fasi: Light, Deep, REM.
  Solo la fase Deep scrive in MEMORY.md. Testo leggibile in DREAMS.md, stato in memory/.dreams/.
  Fonte: [docs OpenClaw, concetto Dreaming](https://docs2.openclaw.ai/concepts/dreaming) [A].
  Un blog riporta soglia di 3 giorni per le note vecchie: [openclawdc](https://www.openclawdc.com/blog/openclaw-dreaming-memory/) [A].
  Licenza del repo OpenClaw: non verificata. Da controllare prima di copiare codice.
- **Letta / MemGPT, sleep-time compute**. Paper di aprile 2025, Letta con UC Berkeley, arXiv:2504.13171.
  Un agente primario parla con l'utente. Un agente "sleep-time" lavora in background sulla memoria.
  Dichiarato circa 5x meno calcolo a parità di accuratezza (un'altra fonte dice 2.5x).
  Blog Letta: [letta.com/blog/sleep-time-compute](https://letta.com/blog/sleep-time-compute) [A].
  Repo: [letta-ai/letta](https://github.com/letta-ai/letta), licenza Apache-2.0 [A].
- **Generative Agents** (Park et al., 2023). Memoria, riflessione periodica, pianificazione.
  Repo: [joonspk-research/generative_agents](https://github.com/joonspk-research/generative_agents), licenza Apache-2.0 [V].
- **Claude Code "auto dream" / /dream**. Descritto in blog di terze parti, non in docs ufficiali. Non usare come fonte primaria [A].

### Come farlo partire in modo stocastico (proposta mia)
Non a orario fisso. La probabilità cresce con l'inattività.

- Ogni minuto fermo: probabilità p = 1 - exp(-λ · minuti_fermi). Con λ piccolo, per esempio 0.05.
  Dopo 10 minuti fermi p circa 0.39. Dopo 30 circa 0.78.
- Tiro un numero casuale ogni minuto. Se è sotto p, parte un ciclo di sogno.
- Reset di minuti_fermi appena l'utente scrive.
- Freno: un sogno solo se dalla fine dell'ultimo sogno sono entrati nuovi ricordi. Niente sogni a vuoto.
- Limite duro: un sogno ogni N ore, e mai durante una sessione di lavoro attiva.

### Cosa produce
1. Riordino: raggruppa idee per tema.
2. Sintesi: riassume gruppi di note vecchie.
3. Contraddizioni: le segnala, non le risolve da sola.
4. Output in file di staging (tipo DREAMS.md), non direttamente in memoria. Si promuove dopo, con regola chiara.

### Raccomandazione A
Schema OpenClaw (tre fasi, staging, promozione) più il trigger stocastico qui sopra.
Esecuzione con modello locale piccolo, per non saturare la RAM (regola: fermarsi sotto 3 GB liberi).

---

## B. Strumenti audio e musica

### Licenze verificate o trovate
| Strumento | Uso | Licenza | Stato |
|---|---|---|---|
| [Demucs](https://github.com/adefossez/demucs) | separazione stem | MIT | [V] repo originale poco mantenuto, valutare fork |
| [audio-separator](https://github.com/karaokenerds/python-audio-separator) | stem con modelli MDX/Demucs | MIT | [A] da PyPI e pyproject |
| [basic-pitch](https://github.com/spotify/basic-pitch) (Spotify) | audio in MIDI, note polifoniche | Apache-2.0 | [A] da PyPI |
| [Whisper](https://github.com/openai/whisper) | trascrizione, testi | MIT | [V] |
| [Essentia](https://github.com/MTG/essentia) | beat, tonalità, accordi, loudness, embedding | AGPL-3.0, esiste licenza commerciale | [V] |
| [Matchering](https://github.com/sergree/matchering) | mastering reference-based | GPL-3.0 | [V] |
| [librosa](https://github.com/librosa/librosa) | analisi, beat, chroma | ISC | [M] |
| [madmom](https://github.com/CPJKU/madmom) | beat, downbeat, tempo | BSD | [M] |
| [pedalboard](https://github.com/spotify/pedalboard) (Spotify) | effetti in Python | GPL-3.0 | [M] |

Attenzione. Essentia (AGPL) e Matchering (GPL) non si possono linkare in un prodotto chiuso senza licenza commerciale.
Per un plugin chiuso: usare librosa, madmom, Whisper, basic-pitch, Demucs/audio-separator. Per il mastering reference-based, scrivere il proprio con librosa e pyloudnorm (licenza da verificare) e lasciare Matchering solo come riferimento.

### Già installato sul PC (solo percorsi e versioni)
Ricerca su E:\ e C:\Users\Whyed.
- Python 3.12.10 (sistema)
- pip: faster-whisper 1.2.1, librosa 1.0.0 (solo Python globale, venv non controllati)
- Modelli Whisper in cache: E:\.cache\huggingface\hub\ con faster-whisper-large-v3, faster-whisper-medium, faster-whisper-small, openai whisper-small
- Skill: C:\Users\Whyed\.claude\skills\audio-mixy, C:\Users\Whyed\.codex\skills\agente-mixy
- Cartella musica: C:\Users\Whyed\Music\remix yng miles
- NON trovati: cartelle Demucs, Essentia, basic-pitch, Matchering, madmom, pedalboard, cartelle Mixy dedicate.

### Raccomandazione B
Base: Whisper (già in cache), librosa (già installato), Demucs o audio-separator per gli stem, basic-pitch per le note.
Per il mastering: scrivere un modulo proprio su librosa. Essentia solo se si accetta AGPL o si compra la licenza commerciale.
Installare in un venv dedicato, non nel Python globale.

---

## C. Collegare l'abbonamento senza incollare chiavi

### Anthropic (Claude)
- Dal 19-20 febbraio 2026 i termini consumer vietano di usare i token OAuth dei piani Free, Pro, Max in tool di terze parti.
  Il Claude Code e Claude.ai sono gli unici usi previsti. L'Agent SDK in terze parti è escluso.
  Blocchi tecnici attivi dal 9 gennaio 2026.
  Fonti: [Gigazine](https://gigazine.net/gsc_news/en/20260220-anthropic-third-party-block) [A],
  [AlternativeTo](https://alternativeto.net/news/2026/2/anthropic-officially-bans-using-subscription-authentication-for-third-party-claude-use) [A].
- Dopo confusione, Anthropic ha detto di voler incoraggiare sviluppo locale con Agent SDK e `claude -p` [A].
- Quindi: un plugin NON deve prendere il token dell'abbonamento. Ammesso: API key ufficiale, oppure il CLI `claude` lanciato dall'utente sul proprio PC per uso personale. Verificare la pagina ufficiale "Legal and compliance".

### OpenAI (ChatGPT / Codex)
- Il login Codex dà un token usato da più client. Alcuni tool lo riusano via proxy locale [A].
- Un articolo riporta che OpenAI fattura le richieste di terze parti dentro il piano, con limiti settimanali per app [A].
- Non trovata una pagina ufficiale che lo permetta espressamente. Usare il CLI Codex ufficiale, mai estrarre token.
- Alcuni plugin della community dichiarano uso personale soltanto [A].

### Google (Gemini)
- Una fonte dice che Google ha cambiato le regole per Gemini CLI a febbraio 2026. Dettagli non trovati. Da verificare.

### OpenRouter
- Non trovato nulla sui termini di login da abbonamento. OpenRouter è a chiave API e pagamento a consumo: via pulita, da confermare sui termini ufficiali.

### Raccomandazione C
1. Un solo adattatore per provider. Il plugin chiede una chiave API una volta, letta da variabile d'ambiente o file in E:\Chiavi. Mai in chat.
2. Per Claude e ChatGPT, l'abbonamento solo tramite il CLI ufficiale che l'utente lancia da sé, uso personale.
3. Per distribuzione o uso commerciale: prima leggere i termini ufficiali di ogni fornitore.
4. Per il dreaming: modello locale. Niente ToS da gestire e niente costi.

---

## Limiti della ricerca
- Molte fonti sono blog e aggregatori [A]. Le licenze [V] sono sulla pagina del repo.
- Non verificati: licenze di OpenClaw, madmom, librosa, pedalboard, pyloudnorm, termini OpenAI/Google/OpenRouter.
- La ricerca locale ha guardato solo il Python globale. Eventuali venv non sono stati controllati.
