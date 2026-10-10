# WhyCremisi: piano condiviso di upgrade

Data: 10 ottobre 2026. Stato: preparazione completata, implementazione non iniziata. Lavoro ufficiale: E:\Lavori, voce 559. Orizzonte richiesto: almeno 2 o 3 giorni diversi prima del 14 ottobre. Le fasi non sono una garanzia di consegna dell'intera visione entro quella data.

## Decisione tecnica

Conservare il plugin JUCE, il DSP, l'interfaccia React e i provider esistenti. Completare il controllo attraverso API della DAW. Per Ableton partire dal bridge AbletonOSC gia presente nel sorgente; hosting VST interno non e' un prerequisito per controllare i parametri esposti da Live. Resta una possibile funzione successiva, non una nuova architettura da costruire prima di poter lavorare.

Un processo locale separato gestisce harness, autenticazione, rete e sessioni AI. Claude Agent SDK e Codex app-server sono adapter aggiuntivi, non sostituti dell'audio engine. Tutti invocano lo stesso contratto di strumenti WhyCremisi; nessun modello dispone di un secondo percorso privilegiato verso la DAW. La UI rimane il punto di ingresso. Identita' utente condivisa fra progetti, stato audio e mapping specifici del progetto.

## Base verificata e difetti confermati

HEAD 512a5c9, master avanti di 6 commit anche rispetto alla verifica remota. VST installato e build VST identici tramite SHA-256. Suite binaria esistente: 2094 controlli, 0 falliti, exit 0. Nessuna build fresca, prova visiva o sessione DAW nuova svolta nella revisione.

| Percorso | Prova nel codice | Lacuna |
|---|---|---|
| AI verso DAW | OscBridge.cpp:2797 chiama dispatchDawCommand | Non basta per provare la modifica |
| Volume AI | :2860 manda track/volume; :1050 attende trackId/valueDb | Payload non accettato |
| Pan AI | :2868 manda track/pan; :1066 attende trackId/value | Payload non accettato |
| Mute AI | :2878 manda track/mute; :1081 attende trackId/muted | Payload non accettato |
| Solo AI | :2886 manda track/solo; :1096 attende trackId/soloed | Payload non accettato |
| Marker AI | :2910 manda marker; :1386 legge index | Indice ricade su 0; nome non risolto |
| Esiti | :2803 dispatch void; :2814 ritorna true; :2826 verifica traffico recente | Successo non correlato all'azione |
| Informazioni traccia | :1550 risposta trackInfo costruita con nome e valori fissi | Non usarla come misura reale |
| Parametri esterni | AbletonDawHandler::setFxParam e comando fxParam esistono | Mancano inventario, risoluzione semantica, lettura e percorso AI |
| Memoria | PluginProcessor.cpp:736/:792 salva/carica workspace nel progetto | Separare profilo globale e dati del Set; memory.json esiste gia |
| Audio real-time | processBlock copia spettro; updateAnalyzer/updateScope usano spectrumLock | Allocazioni e lock confermati; impatto su dropout da misurare |

La suite OscBridgeTest copre costruzione, porte e stato iniziale: non copre i payload mixer o la conferma delle azioni. Il suo successo non smentisce questi difetti.

## Fonti e riuso

AbletonOSC, licenza MIT: https://github.com/ideoforms/AbletonOSC
Il codice device.py offre nomi, valori, range, quantizzazione e listener dei parametri della DAW. Endpoint di lettura e scrittura /live/device/get/parameter/value e /live/device/set/parameter/value. Lettura dopo scrittura o listener sono necessari: il setter non restituisce un ACK. song.py espone undo/redo, che agiscono sullo stack globale di Live. clip_slot.py crea clip e clip.py inserisce note. Il bridge attuale indirizza device delle tracce, non tutte le chain annidate. Preservare gli avvisi MIT se si copia codice.

Ableton MCP, licenza MIT: https://github.com/ahujasid/ableton-mcp
server.py e il Remote Script mostrano strumenti per clip, note, parametri e risposte strutturate. E' un riferimento per i contratti AI; non va installato automaticamente come un secondo bridge concorrente. Alcune funzioni richiedono Live 12, mentre il PC usava Live 11. Il progetto include telemetria: se si adotta una sua parte, verificare e disabilitare invii non richiesti.

Limite del controllo VST: solo parametri pubblicati dal plugin ed esposti/configurati in Live. Non promettere ogni controllo interno di ogni VST. Fonte: https://www.ableton.com/en/live-manual/11/working-with-instruments-and-effects/#plug-in-configure-mode

Le unita' di pan e volume devono essere lette dal parametro Live (min, max, value e str_for_value), non inferite dal commento locale o dalla formula decibelsToGain. Il clamp01 del pan e' incoerente con il contratto ToolRegistry -1..1, ma la mappatura corretta va misurata su Live 11.

Fonti auth attuali:
- https://support.claude.com/en/articles/15036540-use-the-claude-agent-sdk-with-your-claude-plan
- https://developers.openai.com/siwc/token-sharing-open-source/codex-app-server
Usare i flussi ufficiali per l'app, senza prelevare o riutilizzare file di credenziali di altre applicazioni. Catalogo modelli non equivale a disponibilita': serve inference completata. L'attuale setup-token seguito da HTTP diretto non e' gia un adapter Agent SDK.

## Fase 1: azioni affidabili, primo giorno di implementazione

Un solo scrittore: Claude quando incaricato. Codex verifica diff, contratto e risultato. Prima dell'incarico, quote fresche, branch di lavoro, stato e date dei file. Non riavviare le sessioni Claude esistenti.

Lavoro: schema canonico per command/action/result; mapping esplicito da ToolRegistry al dispatcher; errori per campi/indici invalidi; esiti requested/sent/confirmed/failed/unknown con requestId; read-back correlato e tolleranza; nessun stato UI anticipato presentato come prova di esecuzione. Prima correggere mixer e marker, poi inventario di tracce/device/parametri. Conservare il bridge OSC invece di riscriverlo. Separare copie della telemetria dal callback audio tramite buffer preallocato senza attesa sul lock del thread UI.

Prove: test che prima riproducono i payload ignorati; cattura OSC su destinatario controllato; timeout e destinatario assente restituiscono unknown/failed. Build fresca UI/VST/test. In un Set di prova Ableton: scrivi volume/pan/mute/solo, rileggi, verifica e ripristina il valore precedente. Parametro VST configurato: inventario, modifica e ripristino. Null test e stress a 48 kHz con buffer 64/128/256; nessun dropout attribuibile al bridge, nessuna allocazione/lock bloccante nel callback. Pluginval almeno strictness 5 per questa build; livelli maggiori per rilascio.

Non chiudere la fase solo per test/build verdi: servono log e schermate del percorso reale in Live. Allegare le immagini anche in chat.

## Fase 2: harness e memoria, secondo giorno distinto

Lavoro: aggiungere il processo agente fuori dall'audio engine, mantenendo i sei provider. Adapter Claude SDK e Codex app-server con streaming, cancel, errori strutturati, scelta modello da fonte reale e conteggio consumo distinguendo token locali dalle quote dell'account. Connessione degli altri harness mediante lo stesso contratto di strumenti/capability, non un nuovo adapter manuale per ogni tool. Memoria globale utente locale con scritture atomiche; il Set conserva parametri e mapping, non credenziali. Riutilizzare SessionManager e AgentWorkspace, non creare un secondo sistema di memoria.

Prove: completare un turno reale per ciascun adapter che viene dichiarato supportato; stesso tool richiesto da Claude e Codex, stesso esito verificato. Abort durante richiesta, altra richiesta successiva funzionante, chiusura della DAW senza processo orfano. Preferenza ricordata nel Set A e nel Set B dopo riavvio. Forget/reset verificati, nessuna chiave in export, log o file DAW. Nessun bypass della conferma tramite un harness.

## Fase 3: produzione musicale reale, terzo giorno se necessario

Lavoro: strumenti per tracce, clip MIDI, note, caricamento device e sezioni arrangiate, basati sulle API effettivamente disponibili in Live 11. Azioni multiple rappresentate come piano leggibile con stato e annullamento. Read-back dopo ogni cambiamento importante. Analisi audio per guidare il mix con misure vere; evitare di trasformare una regola di genere in una diagnosi del segnale.

Prova finale: da richiesta di Whyed, creare una sezione di 8 o 16 battute in un Set nuovo di prova; verificare tracce, note, strumenti e arrangiamento dentro Live. Suonarla, salvarla, chiudere e riaprire. Applicare una modifica di mix motivata da misure, rileggere il parametro e annullarla. Render/export, quando supportato e verificato, WAV 24 bit 48 kHz. Prima/dopo con loudness, true peak e file realmente aperto/ascoltato. Video/schermate e log devono corrispondere al Set e alla build provati. Il test verifica funzionamento; la qualita' musicale resta un giudizio distinto.

## Revisione delle specifiche

Claude ha letto integralmente i 16 file Research/inglese/markdown/00..15, ARCHITECTURE.md e ROADMAP.md in tre richieste concluse. Rapporti integrali in MATERIALE/2026-10-10/REVISIONE-CLAUDE.md. I paper divergono su hosting, memoria, porte, schema database, provider, piattaforme e distribuzione. Gli esempi nei paper non sono API approvate. Le affermazioni dei revisori non verificate nel sorgente sono ipotesi documentali da controllare, non nuovi difetti dichiarati certi.

Per questo ciclo: Windows e Ableton sul PC reale; nessuna promessa di AAX, Linux, accesso a qualsiasi VST, installer universale o profilo AI che apprende automaticamente prima delle rispettive prove. Multi-DAW e hosting restano nella visione, con implementazione e collaudo separati.

## Confronto pubblico e primato

Ableton MCP documenta creazione di clip e arrangiamenti. Monody documenta una DAW con MCP, analisi mix e operazioni annullabili: https://monody.ai/mcp . Isorek documenta assistenti che controllano i suoi tre plugin: https://isorek.com/mcp/ . Sono fonti dei rispettivi autori, non prove indipendenti eseguite su questo PC. Quindi composizione e integrazione AI non sono esclusive di WhyCremisi. Il confronto definitivo richiede il video di Whyed e lo stesso brief musicale provato sui sistemi confrontati. L'ampiezza della nostra specifica e' una direzione, non una prova di superiorita'.

## Stato e ripristino

Revisione e piano completati; codice, installazione e credenziali invariati. Upgrade ancora aperto nella voce 559. Nessun push dei sei commit preesistenti. Per annullare questa sola preparazione, rimuovere PIANO-UPGRADE-2026-10.md e ripristinare RIPRESA-2026-10-10.md dalla copia in MATERIALE/2026-10-10; le prove possono restare archiviate. Le future modifiche di codice avranno branch e commit separati, reversibili.
