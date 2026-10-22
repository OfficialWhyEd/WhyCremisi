# WhyCremisi: lista infinita (per DeepSeek e chiunque lavori)
Scritta il 22/10/2026. Si lavora sempre dall'alto. Quando la lista finisce si aggiungono voci nuove (vedi in fondo). Non si finisce mai.

## IL CICLO (ripetilo senza fermarti)
1. Leggi DIRETTIVE-PER-CHI-LAVORA.md (regole) e MANUALE-OPERATIVO.md (come si fa, quali strumenti).
2. Prendi la PRIMA voce non fatta qui sotto.
3. Cerca cosa esiste già (repo, STATO.md, open source). Mai da zero.
4. Fai la modifica. Solo file specifici, mai cartelle intere.
5. Costruisci e lancia i test (verifica.cmd). Devono restare 0 falliti.
6. Se è provabile dal vivo, provalo. Se non lo è, scrivi "NON provato".
7. Commit con Co-Authored-By e link sessione, push sul ramo di lavoro.
8. Segna la voce: [x] fatto e provato / [~] fatto ma non provato / [!] bloccato (scrivi il motivo).
9. Aggiorna E:\Lavori\STATO.md con una riga.
10. Torna al punto 2. Non chiedere "continuo?". Non proporre di chiudere.

## SI SALTA E SI SEGNA [!] (serve Whyed)
Merge in master, invio mail, pubblicazione beta, chiavi, installazione di codice di terzi. Si passa alla voce dopo.

## LISTA
- [ ] 1. Collegare le riserve Gemini (GeminiProvider::fallbackModels) a ChainProvider: quota finita o errore 429 passa al modello dopo.
- [ ] 2. Aggiornare liste OpenAI (gpt-6-astra, gpt-6.1-sol, gpt-6-luna: verificare che accettino chat/completions) e Groq (gpt-oss-120b, llama-3.3-70b-versatile) in AIProvider.cpp, PluginProcessor.cpp, OscBridge.cpp e nei test.
- [ ] 3. Provare OpenCode dal vivo nel plugin (provider OpenCode, scrivere "ciao"). Correggere ciò che si rompe.
- [ ] 4. Provare Gemini con la chiave in E:\Chiavi (leggerla da codice, mai stamparla).
- [ ] 5. Punti UTF-8 residui: WhyAccount.cpp:233, AIProvider.cpp ~2196, log, file sessione.
- [ ] 6. Collegare WhyMixy dal vivo (mcp.json) e provare why_stems, why_master, why_trascrivi, why_copyright, why_separa_beat.
- [ ] 7. Rifare matchering e pedalboard come WhyMatch e WhyCatena (niente GPL nell'installer).
- [ ] 8. Impacchettare Python + librosa nell'installer.
- [ ] 9. Provare tutto in Ableton vero e in Reaper (Carlo, Linux).
- [ ] 10. Analizzare i reel di @smylzmusic (Dee3FJwhIwe, DeeCUU9NVaP, DeXDwviovWM, DeUi_IzI9Q2): come sono fatti, cosa serve a WhyCremisi.
- [ ] 11. Tradurre in comandi WhyCremisi ogni cosa del README non ancora fatta (il README si fa tutto, extra compresi).
- [ ] 12. Test: per ogni comando DAW, un test di esito (Done/Unsupported/Rejected/Failed/Unconfirmed).
- [!] 13. Chiave di firma aggiornamenti: la genera Whyed.
- [!] 14. Merge dei rami in master: serve OK di Whyed.

## QUANDO LA LISTA FINISCE
Rileggi il README e STATO.md, cerca lacune (numeri senza misura, pulsanti finti, comandi senza conferma dalla DAW, messaggi d'errore senza motivo, testi solo IT o solo EN). Ogni lacuna diventa una voce nuova qui sopra. Poi ricomincia dal ciclo.
