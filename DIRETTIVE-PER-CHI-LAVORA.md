# WhyCremisi: direttive per chi lavora da ora in poi
Scritto il 22/10/2026. Vale per Claude, DeepSeek, OpenCode, Codex e persone.
Prima di tutto leggi questo file, poi AVVIO-PARTE-5.md, poi E:\Lavori\STATO.md.

## Cos'è e cosa vuole Whyed
Plugin VST3 sul master di una DAW. Chat AI che muove transport, tracce e parametri, crea pezzi da zero e si ricorda tutto. Parla il linguaggio della musica, non i menu.
Deve funzionare per tutti. L'utente finale non installa nulla a mano. Deve poter fare tutto.
Whyed è dislessico e ADHD: frasi corte, poche righe, niente trattini lunghi, risposte in italiano con gli accenti giusti. Una domanda sola, a bottoni, solo se serve.

## Regole che non si rompono
1. Un numero si mostra solo se viene da una misura.
2. Un pulsante fa solo un'azione reale.
3. "Riuscito" solo con conferma dalla DAW. Esiti: Done, Unsupported, Rejected, Failed, Unconfirmed.
4. Non dire "fatto" senza averlo provato. Di' sempre cosa NON è provato.
5. Mai "hai ragione" né sinonimi. Se sbagli: correggi, di' cosa hai fatto, vai avanti.
6. Fai ESATTAMENTE quello che chiede, né meno né altro. Rileggi i suoi messaggi veri prima di agire.
7. Mai proporre di chiudere o ricominciare la sessione.
8. Nessun invio mail, merge o pubblicazione senza il suo OK. Il merge in master dei rami `feat/conferma-reale-comandi-daw` e `fix/utf8-e-toolcall-22-10` non si fa senza conferma.
9. Chiavi: stanno in E:\Chiavi. Mai chiederle a lui, mai stamparle. Se l'accesso è negato non aggirarlo: lo fa lui.
10. AbletonOSC è codice di terzi: si installa solo da azione esplicita dell'utente (casella non spuntata di default).
11. Licenze: plugin chiuso. Niente GPL, AGPL, CC-NC linkati o copiati. MIT, Apache, BSD, ISC vanno bene con riconoscimento.
12. Prefisso o suffisso Why su ogni cosa nostra.
13. Mai `git add` di intere cartelle. `catalog.js` e `i18n.js` del design non si toccano. Il design non tocca C++ né bridge.
14. Ogni nuovo .cpp va in CMake. Errori sempre con il motivo. IT e EN.
15. Se non trovi un tool: cerca almeno 3 strade (GitHub, open source, adattare) prima di dire "non si può".
16. Non saturare la RAM: stop sotto 3 GB liberi. Valorant aperto = RAM libera.

## Come si lavora
- Prima cerca cosa esiste (repo, STATO.md, memorie). Mai da zero, si implementa sopra.
- Build e test Windows: `verifica.cmd` (cmake + ninja + vcvars64, target `WhyCremisiTests`, cartella `build-v`). Ultimo risultato: 3508 controlli, 0 falliti.
- Commit con "Co-Authored-By" e link sessione. Solo file specifici.
- Fine lavoro: aggiorna E:\Lavori\STATO.md. Cose nuove dette da lui: `node E:\Lavori\segna.js nuovo "titolo" "nota" dopo`.

## Per DeepSeek e OpenCode (importante)
- Provider `OpenCode` nel plugin: endpoint `https://opencode.ai/inference/openai/v1/chat/completions`, modelli gratuiti in `OpenCodeProvider::freeModels()` (src/ai/AIProvider.cpp). Il primo è `deepseek-v4.1-flash-free`.
- Elenco preso da opencode.ai/docs/zen il 22/10/2026. È "per tempo limitato": ricontrolla la pagina prima di fidarti.
- Non provato: se serve la chiave anche per i modelli gratuiti e se esiste l'indirizzo `/models` usato da testConnection. Provalo e correggi.
- Per aggiungere un provider: enum in AiEngine.h, providerToId/providerFromId/createProvider/getProviderName in AiEngine.cpp, lista parametro e switch in PluginProcessor.cpp (aggiungi in FONDO, mai in mezzo), normalizzazione modello e URL chiavi in OscBridge.cpp, test in AiEngineTest.cpp.
- Modelli locali piccoli: contesto 8-12K, thinking spento per i comandi, una tool call per volta, argomenti validati. Vedi SINTESI-MODELLI-LOCALI-22-10.md.
- Gemini: i modelli 1.x e 2.0 sono spenti. Default `gemini-3.5-flash-lite`. Controlla ai.google.dev/gemini-api/docs/models.

## WhyMixy (E:\Dev\WhyMixy)
Server MCP con 64 tool why_*: analisi, mastering, voce, campioni, web, shell, file. Si aggiunge un tool mettendo un file in `moduli\` con un dizionario `TOOLS`. Collegato da `%APPDATA%\WhyCremisi\mcp.json` (collegamento NON provato dal vivo).
Limiti: BPM e tonalità sono stime; why_stems, why_master, why_trascrivi, why_copyright, why_separa_beat non provati; pedalboard e matchering (GPL) vanno rifatti nostri (WhyCatena, WhyMatch) prima dell'installer; Python e librosa vanno impacchettati nell'installer.

## Cosa resta (in ordine)
1. Provare Gemini e OpenCode con chiave vera. 2. Provare tutto in Ableton e Reaper veri. 3. Collegare WhyMixy dal vivo. 4. Impacchettare Python nell'installer. 5. Rifare i due tool GPL. 6. Reel Instagram di @smylzmusic rimasti (Dee3FJwhIwe, DeeCUU9NVaP, DeXDwviovWM, DeUi_IzI9Q2). 7. Punti UTF-8 residui (WhyAccount.cpp:233, AIProvider.cpp ~2196, log, file sessione). 8. Chiave di firma aggiornamenti (la genera lui).
