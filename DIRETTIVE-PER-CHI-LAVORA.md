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
- Build e test Windows: `verifica.cmd` (cmake + ninja + vcvars64, target `WhyCremisiTests`, cartella `build-v`). Ultimo risultato: 3513 controlli, 0 falliti.
- Commit con "Co-Authored-By" e link sessione. Solo file specifici.
- Fine lavoro: aggiorna E:\Lavori\STATO.md. Cose nuove dette da lui: `node E:\Lavori\segna.js nuovo "titolo" "nota" dopo`.

## Per DeepSeek e OpenCode (importante)
- Provider `OpenCode`: il server rifiuta i modelli gratuiti (403 FreeTierError) a chi non è il programma ufficiale. Quindi il plugin lancia `opencode run --format json --pure -m opencode/<modello>` con il prompt da stdin, come fa con Codex. Serve OpenCode installato. Primo modello: `deepseek-v4.1-flash-free`; elenco in `OpenCodeProvider::freeModels()`.
- Non provato dal vivo dentro il plugin (solo il CLI da shell). Il CLI aggiunge un suo prompt grosso (~30k token): è lento.
- Elenco preso da opencode.ai/docs/zen il 22/10/2026, "per tempo limitato": ricontrolla.
- Per aggiungere un provider: enum in AiEngine.h, providerToId/providerFromId/createProvider/getProviderName in AiEngine.cpp, lista parametro e switch in PluginProcessor.cpp (aggiungi in FONDO, mai in mezzo), normalizzazione modello e URL chiavi in OscBridge.cpp, test in AiEngineTest.cpp.
- Modelli locali piccoli: contesto 8-12K, thinking spento per i comandi, una tool call per volta, argomenti validati. Vedi SINTESI-MODELLI-LOCALI-22-10.md.
- Gemini: default `gemini-flash-latest` (alias Google, sempre il Flash più recente). Riserve in `GeminiProvider::fallbackModels()`: gemini-3.8-flash, 3.6-flash, 3.5-flash-lite, 3.1-flash-lite, 2.5-flash. Serie 1.x e 2.0 spente. Più recente su pagina: gemini-4-argon. NON fatto: il passaggio automatico alla riserva quando la quota finisce (la lista c'è, il collegamento a ChainProvider no). Alias pro/lite-latest non confermati. Non provato con chiave vera. Controlla ai.google.dev/gemini-api/docs/models.
- OpenAI (gpt-6-astra, gpt-6.1-sol, gpt-6-luna) e Groq (gpt-oss-120b, llama-3.3-70b-versatile): visti sulle pagine ufficiali, NON ancora messi nelle liste né provati. Con la chiave il plugin chiede l'elenco al servizio.

## WhyMixy (E:\Dev\WhyMixy)
Server MCP con 64 tool why_*: analisi, mastering, voce, campioni, web, shell, file. Si aggiunge un tool mettendo un file in `moduli\` con un dizionario `TOOLS`. Collegato da `%APPDATA%\WhyCremisi\mcp.json` (collegamento NON provato dal vivo).
Limiti: BPM e tonalità sono stime; why_stems, why_master, why_trascrivi, why_copyright, why_separa_beat non provati; pedalboard e matchering (GPL) vanno rifatti nostri (WhyCatena, WhyMatch) prima dell'installer; Python e librosa vanno impacchettati nell'installer.

## Cosa resta (in ordine)
0. Collegare le riserve Gemini a ChainProvider; aggiornare liste OpenAI/Groq. 1. Provare Gemini e OpenCode dal vivo. 2. Provare tutto in Ableton e Reaper veri. 3. Collegare WhyMixy dal vivo. 4. Impacchettare Python nell'installer. 5. Rifare i due tool GPL. 6. Reel Instagram di @smylzmusic rimasti (Dee3FJwhIwe, DeeCUU9NVaP, DeXDwviovWM, DeUi_IzI9Q2). 7. Punti UTF-8 residui (WhyAccount.cpp:233, AIProvider.cpp ~2196, log, file sessione). 8. Chiave di firma aggiornamenti (la genera lui).
