# Ponti con Ableton Live fatti da altri

Data ricerca: 22/10/2026. Solo web e GitHub, nessun test locale.

## Risposta corta

- Due strade. Remote Script (Python, dentro Live, serve Control Surface) e Extensions SDK (JavaScript, ufficiale Ableton, Live 12.4.5+).
- Installazione SENZA click: NON TROVATO. Nessuna prova che il Control Surface si attivi da Preferences.cfg o Options.txt.
- Lo SDK nuovo non usa Remote Script. Quindi niente Control Surface. Ma l'estensione va comunque installata da Live (drag di un file .ablx).

## Il punto critico: Control Surface

- Ableton: i Remote Script di terze parti vanno in User Library/Remote Scripts e si scelgono dal menu Control Surface. Fonte: [help Ableton 209072009](https://help.ableton.com/hc/en-us/articles/209072009-Installing-third-party-remote-scripts)
- Ableton: Control Surface "None" blocca il rilevamento automatico. Fonte: [help Ableton 209774285](https://help.ableton.com/hc/en-us/articles/209774285)
- Options.txt: esiste, serve per opzioni sperimentali (es. -EnableArmOnSelection). Non risulta un'opzione per il Control Surface. Fonte: [help Ableton Options.txt](https://help.ableton.com/hc/en-us/articles/6003224107292)
- Preferences.cfg come via per attivare Control Surface: NON TROVATO.
- Modo headless o senza GUI per attivare lo script: NON TROVATO.
- Issue con prove su installazione automatica: NON TROVATO. Esiste una issue su ahujasid, ma riguarda altro (vedi sotto).
- Fonte di tutto questo: [ricerca web 22/10](https://help.ableton.com/hc/en-us/articles/209072009)

## Progetti

### 1. Extensions SDK (ufficiale Ableton)

- Cos'è: SDK JavaScript su Node.js. Le estensioni si lanciano con clic destro su un elemento del Set e girano una volta sola. Fonte: [Ableton blog Extensions SDK](https://www.ableton.com/en/blog/introducing-extensions-sdk/)
- Requisiti: Live 12 Suite Beta 12.4.5+. Non in Standard, Intro, Lite. Stessa fonte.
- Licenza SDK: download gratuito. Termini esatti NON TROVATI.
- Controlla: Set, tracce, clip, parametri, MIDI, tempo. Browser: NON TROVATO nella pagina.
- Stato: beta pubblica, serve iscriversi al programma beta. Stessa fonte.
- Installazione: da Live, Impostazioni > Extensions. Automatica: NON TROVATO.

### 2. ableton-live-mcp (ulm0)

- Link: [github.com/ulm0/ableton-live-mcp](https://github.com/ulm0/ableton-live-mcp)
- Licenza: MIT per il codice. La cartella vendor/ ha licenza propria di Ableton.
- Basato su Extensions SDK. Richiede Live 12.4.5+. Nessun MIDI Remote Script (quindi niente Control Surface).
- Installazione: npm install, npm run package, trascinare il file .ablx in Live. Serve un clic dell'utente. Server MCP su http://127.0.0.1:8722/mcp.
- Controlla: clip (create, get, set, delete, copy), note MIDI (get, set, edit con trasposizione, quantizzazione, swing, humanize). Pattern a step.
- Stato: 9 commit, 0 stelle. Data ultimo commit: NON TROVATO sulla pagina.
- Fonte: [ricerca Glama](https://glama.ai/mcp/servers/ulm0/ableton-live-mcp)

### 3. giuliobracci/ableton-mcp-server

- Basato su Extensions SDK, beta. Porta 39001 indicata nel README (da ricerca web).
- Pagina GitHub: 404 al fetch. Licenza e ultimo commit: NON VERIFICATO.
- Fonte: [ricerca web](https://glama.ai/mcp/servers/p7h2v739hv)

### 4. ahujasid/ableton-mcp (il piu noto, Remote Script)

- Link: [github.com/ahujasid/ableton-mcp](https://github.com/ahujasid/ableton-mcp)
- Licenza: MIT. Circa 3,2k stelle, 443 fork, 73 commit su main. Data ultimo commit: NON TROVATO.
- Live 12: NON confermato nella pagina.
- Controlla: tracce (MIDI e audio), device e strumenti dal browser, clip (crea, modifica, avvia), note MIDI, transport, tempo.
- Installazione: uvx --from ableton-mcp ableton-mcp-install-script copia lo script. Poi Control Surface "AbletonMCP" a mano. Il server NON installa lo script all'avvio. Fonte: stessa pagina.
- Issue utili (github.com/ahujasid/ableton-mcp/issues):
  - #127 "Live 12: support loading effects to the master track" (27/08/2026). Prova che Live 12 da problemi sul master.
  - #49 "not available on Smithery" (24/10/2025). Installazione.
  - #124 consenso dataset (18/08/2026). Non chiaro, da verificare.
- Fork con Live 12: ci sono ma non verificati. Es. "ableton-mcp-loopback" (nome da ricerca web, 127.0.0.1:9877).

### 5. AbletonOSC (ideoforms)

- Link: [github.com/ideoforms/AbletonOSC](https://github.com/ideoforms/AbletonOSC)
- Licenza: MIT. 292 commit. Data ultimo commit: NON TROVATA.
- Requisiti: Live 11 o superiore. Live 12: solo una segnalazione in un forum (Open Stage Control), non confermata dal maintainer. Fonte: [forum](https://openstagecontrol.discourse.group/t/ableton-osc-send-receive-osc-direct-to-and-from-ableton-remote-script-not-midi/3829)
- Controlla: tracce (volume, pan, mute, solo, arm, send, routing), device (parametri, anche in bulk), clip (avvio, note, nome, colore, loop, warp), browser: NON nella pagina.
- Installazione: copia in User Library/Remote Scripts, riavvia, Control Surface "AbletonOSC" a mano. Porte 11000 e 11001. Fonte: [github](https://github.com/ideoforms/AbletonOSC)

### 6. AbletonJS (leolabs/ableton-js)

- Link: [github.com/leolabs/ableton-js](https://github.com/leolabs/ableton-js)
- Licenza: MIT (dal fetch). Circa 599 commit. Data ultimo commit: NON TROVATA.
- Remote Script Python in midi-script/, client JS/TS. WebSocket su 127.0.0.1:39031. Password opzionale in Config.py.
- Live 12: indizio nel README (comando ableton12:start), non prova diretta.
- Installazione: copia midi-script in User Library/Remote Scripts come AbletonJS, riavvia, scelta a mano.

### 7. LiveMCP (PyPI, alaarab)

- Link: [pypi.org/project/livemcp](https://pypi.org/project/livemcp/)
- Licenza: MIT. Versione 1.2.8, ultima release 11/03/2026.
- Live 12: dichiarato nella guida di installazione, senza prova esplicita.
- Controlla: 205 tool (transport, tracce, clip, device, mixer, arrangement, groove). Bridge TCP locale.
- Installazione: uvx livemcp --install copia lo script. Poi Control Surface "LiveMCP" a mano. Automatico: NO, dichiarato nella pagina.

### 8. Altri trovati, non verificati

- Ableton MCP Extended (uisato, kevinzhang03): NON VERIFICATO. Usa SDK o Remote Script? NON TROVATO.
- ABLE-MCP (bassDaddyDevices): dentro Live testato su 12.4.5b3, ma il server MCP legge solo file .als. Non crea clip in un Set aperto. Fonte: [Glama](https://glama.ai/mcp/servers/bassDaddyDevices/ABLE-MCP)
- Max for Live bridge: NON TROVATO in questa ricerca.
- pylive, Remote Script custom generico: NON VERIFICATO in dettaglio. Esempio: [Generic-Python-Remote-Script](https://github.com/j74/Generic-Python-Remote-Script).

## Come si crea musica da zero con questi ponti

- Extensions SDK / ulm0: clip_create crea una clip MIDI in uno slot. midi_clip_set_notes scrive le note. Pattern a step con patterns e repeat. Fonte: [Glama ulm0](https://glama.ai/mcp/servers/ulm0/ableton-live-mcp)
- AbletonOSC: clip, note via OSC (aggiunta, lettura, rimozione). Fonte: [github](https://github.com/ideoforms/AbletonOSC)
- ahujasid: crea tracce MIDI, carica strumenti dal browser, crea clip e note. Fonte: [github](https://github.com/ahujasid/ableton-mcp)
- LiveMCP: clip e device, dettagli NON VERIFICATI.

## Per WhyCremisi: cosa ne segue

- Il percorso senza click non esiste nelle prove trovate. Per l'installazione automatica:
  - Opzione A: Extensions SDK. Niente Control Surface. Resta un passo in Live (trascinare .ablx). Richiede Live 12.4.5 Suite Beta.
  - Opzione B: Remote Script. Serve un clic per scegliere il Control Surface. Non esiste via trovata.
- Per il plugin VST3 la domanda resta aperta: un VST non puo' attivare un Remote Script di Live. Da verificare in test reale.
- Da fare: test su PC di Whyed con Live 12.4.5 e l'estensione ulm0, misurando se il primo avvio richiede clic.

## Sintesi (10 righe)

1. Due strade: Remote Script (Python, Control Surface) e Extensions SDK (JS, Live 12.4.5+).
2. Remote Script: serve scegliere il Control Surface a mano in Preferences. Nessuna prova di attivazione automatica.
3. Preferences.cfg e Options.txt: NON TROVATO come via per attivare Control Surface.
4. Extensions SDK: niente Control Surface. Resta un drag di .ablx in Live.
5. ulm0/ableton-live-mcp (MIT, Live 12.4.5+): clip e note MIDI complete. Il progetto piu utile da studiare.
6. ahujasid/ableton-mcp (MIT, 3,2k stelle): Live 12 non confermato, issue #127 segnala problemi.
7. AbletonOSC (MIT): Live 11+, Live 12 non confermato.
8. LiveMCP (MIT): Live 12 dichiarato, Control Surface a mano, release 11/03/2026.
9. Musica da zero: clip_create + midi_clip_set_notes (ulm0), clip e note via OSC (AbletonOSC).
10. Da provare su PC: Extensions SDK con clic misurato, prima di scegliere la strada.
