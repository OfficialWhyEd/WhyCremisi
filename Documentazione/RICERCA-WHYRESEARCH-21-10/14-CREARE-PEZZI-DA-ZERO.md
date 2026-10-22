# 14 - Creare pezzi musicali da zero dentro la DAW (21/10/2026)

## Sintesi: cosa prenderei
1. Metodo subito, zero installazioni: un LLM remoto (il tuo abbonamento, DeepSeek via OpenRouter) scrive le note come JSON. Il plugin le valida e le crea nella clip.
2. Il plugin ha anche un generatore a regole: ritmi euclidei (Bjorklund, MIT) e accordi, basso, arpeggi. Logica presa da un progetto JS MIT del febbraio 2026.
3. Lo schema JSON delle note ispirato a Magenta NoteSequence. Un progetto Apache-2.0 lo usa gia' per mandare note da LLM a una DAW.
4. Metodo locale opzionale: MIDI-GPT (MIT nel codice) e Text2MIDI (Apache-2.0 sulla pagina HF). Solo dopo aver trovato e letto i pesi.
5. Scartare per ora MuseCoco: il repo Muzic e' stato archiviato il 1 ottobre 2026 e i pesi non li ho trovati.
6. Scartare music-pattern-generator come codice: e' GPL-3.0, il plugin e' chiuso. Tenere solo l'idea.
7. Ableton: il ponte resta AbletonOSC (gia' deciso). ableton-mcp (MIT) e' uno spunto per il protocollo.
8. Il valore vero e' la validazione: l'LLM sbaglia tonalita' e durate. Il plugin deve correggere prima di scrivere.
9. Su 8 GB liberi di RAM: le regole girano ovunque. Il metodo remoto non usa RAM locale. Il locale MIDI non l'ho misurato.
10. Raccomandazione: metodo 1 subito. Metodo 2 come opzione spenta di default, dopo verifica licenza pesi.

## Raccomandazione chiara
- METODO A, SUBITO (nessuna installazione): LLM remoto + JSON note + generatore a regole nel WebView. Costo: solo l'abbonamento che l'utente gia' ha.
- METODO B, LOCALE OPZIONALE: helper scaricato a richiesta con MIDI-GPT o Text2MIDI. Prima di usarlo: pesi, licenza dei pesi, RAM misurata.
- NON USARE ORA: MuseCoco, ableton-mind (alpha v0.1.1 secondo la sua pagina), generatori GPL.

## Tabella: approccio, licenza, requisiti, verdetto
| Approccio / progetto | Licenza codice | Licenza pesi | Dimensione | Gira su CPU con 8 GB liberi? | Qualita' riportata | Link | Verdetto |
|---|---|---|---|---|---|---|---|
| Ritmi euclidei, Bjorklund (odahan/bjorklund) | MIT (letta nel README) | nessun peso | codice minimo | Si, codice Python semplice | algoritmo, nessuna valutazione | https://github.com/odahan/bjorklund | PRENDERE (algoritmo) |
| chordgen (savoysounds, Gitea) | MIT (letta nel README) | nessun peso | non verificata | Si, gira nel browser | accordi, basso, arpeggi, 3 generatori indipendenti. Nessuna batteria | https://gitea.com/savoysounds/chordgen | PRENDERE (logica accordi e basso) |
| isobar (loparcog) | MIT (testo LICENSE letto) | nessun peso | non verificata | Si, Python puro | pattern, PEuclidean, PArpeggiator, PDegree. Output MIDI, OSC | https://github.com/loparcog/isobar | SPUNTO (riferimento di design, 0 stelle) |
| music-pattern-generator (hisschemoller) | GPL-3.0 | nessun peso | non verificata | Si | Euclid FX, MIDI out. Niente accordi o basso | https://github.com/hisschemoller/music-pattern-generator | SCARTARE il codice (GPL). Solo idea |
| Polyrhythmix (dredozubov) | Unlicense | nessun peso | non verificata | Si, Rust CLI | batteria e basso da descrizione, MIDI. Ultimo push luglio 2023 | https://awesome.ecosyste.ms/projects/github.com%2Fdredozubov%2Fpolyrhythmix | SPUNTO (vecchio) |
| MCP MIDI Bridge (tezza1971/mcp-midi) | Apache-2.0 (testo letto) | nessun peso | non verificata | Richiede Node 18+ e Python 3.8+ | LLM manda NoteSequence JSON via HTTP, la DAW la riceve da porta MIDI virtuale. 4 stelle | https://github.com/tezza1971/mcp-midi | PRENDERE (schema JSON). Non usare l'app: e' una app a parte |
| ahujasid/ableton-mcp | MIT (nota 01) | nessun peso | non verificata | Si | Socket JSON verso Live. 3.2k stelle (nota 01) | https://github.com/ahujasid/ableton-mcp | SPUNTO (protocollo) |
| ableton-mind | non verificata | nessun peso | - | - | Espone Object Model di Live, note con pitch/start/durata/velocity. Alpha v0.1.1 | https://images.glama.ai/mcp/servers/Pantani/ableton-mind | SCARTARE per ora |
| MIDI-GPT (Metacreation-Lab) | MIT nel badge README (nota 06) | non trovato | non trovato | non verificato | Infill per traccia o battuta, densita, polifonia, stile | https://github.com/Metacreation-Lab/MIDI-GPT | PRENDERE (metodo locale), dopo verifica pesi |
| Text2MIDI (AMAAI-Lab) | Apache-2.0 sulla pagina HF (nota 06) | Apache-2.0 HF (nota 06) | non trovato | non verificato | Da testo a MIDI (accordi, tonalita', tempo nel testo) | https://huggingface.co/amaai-lab/text2midi | PRENDERE (metodo locale), dopo verifica RAM |
| Anticipatory Music Transformer | Apache 2.0 (nota 06) | non trovato | non dichiarata | non verificato, esempi CUDA | Continuazione e infill di MIDI | https://github.com/jthickstun/anticipation | SPUNTO (per la fase "continua la mia traccia") |
| MuseCoco (dentro Microsoft Muzic) | MIT del repo (pagina letta) | NON TROVATO | 1.2 miliardi di parametri (arXiv 2306.00110) | NON TROVATO | Da testo a MIDI | https://github.com/microsoft/muzic , https://arxiv.org/pdf/2306.00110 | SCARTARE: repo archiviato il 1 ottobre 2026 |
| Magenta (generico) | Apache 2.0 | vedi nota 06 | vedi nota 06 | vedi nota 06 | vedi nota 06 | https://magenta.tensorflow.org/oss-infinite-crate | SPUNTO (schema NoteSequence) |

## Dettagli: agenti LLM che compongono in una DAW
- AbletonMCP (ahujasid): il progetto piu' citato. Le directory dicono che crea clip MIDI e scrive note. Non ho aperto il codice per confermare il formato delle note: https://mcpservers.org/tr/servers/ahujasid/ableton-mcp (aggregatore, non il repo).
- ableton-copilot-mcp (xiaolaa2): lavora sull'Arrangement View, via npm. Non e' focus su note: https://playbooks.com/mcp/xiaolaa2-ableton-copilot (aggregatore).
- reaper-mcp (itsuzef): 60 tool divisi per progetto, tracce, FX, mix, render. MIT (nota 01): https://github.com/itsuzef/reaper-mcp . Lista di tool utile, codice non copiare.
- openDAW MCP (AMEOBIUS): un tool accetta un array JSON di note con pitch, beat di inizio, durata, velocity opzionale. Il formato e' quello che ci serve: https://glama.ai/mcp/servers/AMEOBIUS-team/opendaw-mcp/tools/mcp_opendaw_create_notes_batch (licenza NON VERIFICATA).
- ReaMCP (danielkinahan): legge le note da un item MIDI di REAPER. Utile per la lettura: https://glama.ai/mcp/servers/danielkinahan/ReaMCP/tools/get_midi_notes (licenza NON VERIFICATA).
- LMMS MCP (TypeWolf): controlla tracce, pattern, note, tempo: https://glama.ai/mcp/servers/pavytuerhh (licenza NON VERIFICATA).
- M2TM NOTES: generatore MIDI commerciale per Ableton 12 con testo come notazione. Chiuso: https://midi.org/?p=5168 . SCARTARE (commerciale).
- llmjam (logicmoo): jam in tempo reale, non usa tool calling: https://github.com/logicmoo/llmjam (licenza NON VERIFICATA).

## Dettagli: approccio "LLM scrive note come JSON, poi la DAW le crea"
- Schema: nota = {pitch (0-127), start (in battute o beat), durata, velocity}. Stesso schema di openDAW MCP e di NoteSequence (Magenta).
- Il plugin deve validare prima di scrivere: range di pitch, tonalita', quantizzazione, durate sensate, sovrapposizioni.
- Errore tipico: l'LLM sbaglia le note fuori scala. Soluzione: il generatore a regole fornisce la scala e la progressione. L'LLM sceglie solo tra opzioni valide.
- Gia' noto dal progetto: AbletonOSC (MIT) e' il ponte. Che AbletonOSC scriva note in una clip e' NON VERIFICATO in questa ricerca. Va provato su Live 12 reale.

## Dettagli: generatori a regole (batteria, accordi, basso)
- Euclidiano (Bjorklund): e' la base per ritmi di batteria e percussioni. Un tipo di pattern, poche righe di codice. PRENDERE.
- Accordi e basso: chordgen (JS, MIT, febbraio 2026) mostra la logica: progressioni, linea di basso, arpeggi. Riscrivibile in TypeScript nel WebView React. PRENDERE la logica, non copiare il codice senza leggere la licenza in dettaglio.
- Teoria degli accordi: teoria-chord-progression (MIT, versione 1.0.0, non recente): https://npmjs.org/package/teoria-chord-progression . Utile come riferimento.
- Batteria con regole complete: non trovato un progetto MIT o Apache recente. Polyrhythmix (Unlicense) e' vecchio. Per ora NON TROVATO come progetto da prendere.
- Plugin commerciali (Bassliner): algoritmici ma chiusi. SCARTARE come codice. Solo come riferimento di risultato: https://www.kvraudio.com/product/bassliner-by-dreeemwave

## Dubbi e cose non verificate
- Pesi di MIDI-GPT e Text2MIDI: licenza e dimensione NON TROVATE in questa ricerca. Vanno lette nelle pagine HF.
- MuseCoco: pesi NON TROVATI. Repo archiviato il 1 ottobre 2026, quindi nessuna manutenzione.
- RAM e CPU per MIDI-GPT e Text2MIDI: NON TROVATO. Non dico "gira su 8 GB" senza misura.
- Licenza di chordgen: letta nel README, non nel file LICENSE. Controllare il file.
- Licenza di isobar e music-pattern-generator: lette dalle pagine, non dal codice completo.
- Stelle e date di ahujasid/ableton-mcp: prese dalla nota 01, non riaperte qui.
- Nessun progetto LLM-DAW del 2026 con licenza permissiva e batteria completa trovato.
- Nessun test reale fatto. Nessun repo clonato o eseguito.
