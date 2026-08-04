# TODO — WhyCremisi

**Ultimo aggiornamento:** 1 agosto 2026

Questo file elenca le cose aperte **fuori** dalla roadmap dei 100 step (`Documentazione/ROADMAP.md`), che resta il piano di sviluppo vero e proprio.

---

## Alta priorità

- [ ] **Interfaccia multilingua** (chiesto il 04/08/2026) — la lingua si sceglie dalle impostazioni, ma al primo avvio va indovinata: prima la lingua di sistema, in subordine quella di Ableton. Oggi le stringhe stanno scritte a mano dentro i componenti — il 04/08 la parte principale è stata portata in italiano, il rack è ancora in inglese — quindi serve prima un meccanismo di traduzione, non altre sostituzioni a mano.

- [ ] **Verificare il plugin dentro un DAW su Windows** — lo standalone si apre con la UI React funzionante e il VST3 passa i test automatici, ma dentro Reaper e Ableton non è ancora stato provato: caricamento, apertura UI, comandi OSC. Serve copiare il `.vst3` in `C:\Program Files\Common Files\VST3\`, cosa che richiede permessi da amministratore.
- [ ] **Ricompilare su Mac dopo il merge** — il ramo unificato è stato verificato solo su Windows. Su macOS vanno riprovati il blocco `if(APPLE)` del `CMakeLists.txt` e il formato AU.
- [ ] **Allineare la versione di JUCE** — su Mac è stato usato JUCE 8.0.12, su Windows c'è 8.0.4. Le API della WebView differiscono fra le due.
- [ ] **Testare il feedback OSC dal DAW al plugin** — la direzione plugin → DAW funziona, l'inversa non è mai stata verificata davvero.
- [ ] **Provare i pannelli con audio che scorre** — le misure ora sono vere e le firme mid/side sono state verificate su segnali noti, ma il percorso completo fino allo schermo dentro una DAW non è ancora stato visto.
- [ ] **Immagine stereo (`StereoscopeBox`)** — è l'ultimo pannello ancora ricavato dai soli meter L/R; ora che i punti del vectorscope arrivano, può usare quelli.
- [ ] **Login Claude con un click** — oggi il token di un abbonamento si rileva da `ANTHROPIC_AUTH_TOKEN` o si incolla a mano. Il flusso OAuth completo dentro il plugin (browser + PKCE) è un lavoro a sé.
- [ ] **Provare in Ableton il canale di comando verso il DSP** — il cablaggio c'è e il percorso è verificato staticamente, ma il comportamento a runtime non è confermato: `pedalboard` crea più istanze del plugin e non permette di raggiungere quella che processa l'audio. Vedi `ABLETON.md`.
- [x] ~~**Collegare la catena DSP alla UI e all'AI**~~ — EQ, compressore e limiter esistono e funzionano, ma non è raggiungibile nulla: `DSPEngine::setBypass()` non viene chiamato da nessuna parte e nel bridge OSC non c'è un solo comando che imposti soglia, ratio o bande. Il `CompressorBox` della UI manda comandi che non arrivano al DSP interno. Finché non sono cablati, i moduli restano bypassati.

## Media priorità

- [ ] **Preferenze configurabili** — IP e porte OSC/WebSocket si impostano ancora modificando il codice.
- [ ] **Mappare tutti i parametri OSC di Reaper** — volume, pan, mute, solo, tempo, posizione.
- [ ] **Supporto OSC per Ableton** — `AbletonDawHandler` esiste ma è poco più di uno scheletro.
- [ ] **CI su GitHub Actions** — build automatica per Windows e macOS a ogni push (step 80 della roadmap). Avrebbe intercettato subito il fatto che `master` non compilava.

## Bassa priorità

- [ ] `getCurrentPosition` è deprecato in JUCE 8 → usare `getPosition` in `PluginProcessor`.
- [ ] Pulire la logica di riconnessione del WebSocket.
- [ ] Vector Scope con dati audio reali (l'analyzer ormai li fornisce).
- [ ] Decidere che fare dei branch storici `heartbroken`, `heartbroken-claude`, `aura` una volta riunificati.

---

## Fatto

- [x] **Build Windows sbloccata** (01/08/2026) — guard su `SIGPIPE`, WebView2 attivato nel `CMakeLists.txt`, percorso della UI reso cross-platform, copia della dist React anche su Windows.
- [x] **Regole del progetto riscritte** (01/08/2026) — rimosso tutto l'impianto a tre persone; vedi `Documentazione/WORKFLOW.md`.
- [x] **Rami riunificati** (01/08/2026) — `master` e `heartbroken-claude` riuniti; nel merge sono emersi il `getName()` spezzato che impediva a `master` di compilare, l'escaping JavaScript mai eseguito e il flag `/utf-8` mancante per MSVC.
- [x] **Documentazione separata dal sito** (01/08/2026) — `docs/` è solo la landing page GitHub Pages, la documentazione sta in `Documentazione/`.
- [x] **Riferimenti al repository corretti** (01/08/2026) — il repo è stato rinominato in `OfficialWhyEd/WhyCremisi`: aggiornati i comandi di clone e rimosso il remote `carlo`, che puntava a un repository non più esistente.
- [x] **Sezione Claude rifatta** (01/08/2026) — l'elenco modelli offriva solo modelli ritirati, quindi Claude restituiva 404. Ora Opus 5, Sonnet 5 e Haiku 4.5, accesso con abbonamento oltre che con chiave API, controllo dell'effort e adaptive thinking. Tolti `temperature`/`top_p`, che sui modelli attuali fanno fallire la richiesta.
- [x] **Accesso senza abbonamento via OpenRouter** (01/08/2026) — modelli gratuiti contrassegnati nella schermata di setup, per chi non ha né abbonamento né voglia di pagare a consumo.
- [x] **Misure vere nei pannelli di analisi** (01/08/2026) — il vectorscope disegnava una figura animata con `Math.sin(Date.now())`, l'RMS era `LUFS + 2.1`, il true peak `picco + 0.3`, la riduzione del compressore veniva dalla correlazione di fase e lo spettro aveva un fallback `Math.random()`. Ora arrivano tutte dall'analyzer, che diverse di esse già calcolava senza che nessuno le inviasse.
- [x] **Consumo di token visibile** (01/08/2026) — token di sessione nel footer, con dettaglio nel tooltip; funziona per Claude e per i provider compatibili OpenAI.
- [x] **Il plugin non colora più il suono da solo** (01/08/2026) — misurando l'uscita con `pedalboard` è saltato fuori che con i default di fabbrica comprimeva 4:1 sopra −24 dB e limitava a −6 dB, attenuando di 12,7 dB un segnale a −9 dBFS RMS. Su un plugin che si carica sul master e che nasce per analizzare, è un comportamento sbagliato. Ora i moduli partono bypassati.
