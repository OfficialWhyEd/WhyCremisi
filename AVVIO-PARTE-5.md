# WhyCremisi sviluppo, parte 5 (avvio)

Prompt di una riga: "continua whycremisi parte 5 sviluppo" (leggi questo file e E:\Lavori\STATO.md).

## Stato al 22/10/2026
- Ramo feat/conferma-reale-comandi-daw, ultimo commit c82d2c9 (pushato). Non unito a master.
- 3324 test, 0 falliti. Installer Windows 0.2.0, pacchetto Linux, updater con flag, banner UI, script Reaper, guida TEST-CARLO-REAPER-LINUX.md.
- Aggiornamenti firmati Ed25519 (TweetNaCl). Senza chiave pubblica compilata gli aggiornamenti sono spenti.

## Da fare subito
1. Whyed lancia: `! node E:\Dev\WhyCremisi\Tools\firma\genera-chiave.mjs` (io non posso scrivere in E:\Chiavi).
2. Ricompilare plugin (bp.cmd) e installer (Tools\pubblica-beta.ps1, senza -Pubblica), mandare l'exe a Whyed.
3. Pubblicare la release beta con `Tools\pubblica-beta.ps1 -Pubblica` (firma latest.json e carica anche .sig).
4. Whyed deve mettere Carlo (ACarloGitHub) in Write su GitHub: Settings > Access. Ora e solo Read.
5. Chiave Gemini per Carlo: la prende Whyed da E:\Chiavi o ne crea una nuova su aistudio.google.com/apikey.

## Non provato
Reaper dal vivo, Linux dal vivo, aggiornamento contro release vera, firma codice Windows (SmartScreen), Mac.

## Rimasti
Primo avvio con box dinamici (col design), WhyLocalLM/WhyCode/WhyObliter, ragionamento visibile, Suno e free llm api, MCP, browser interno, preview sample, menu preset Reaper, Supabase vero (serve Whyed), leggere i log dopo i test di Carlo.

## Limiti di sicurezza
Il sistema blocca a me: E:\Chiavi, permessi GitHub, scrittura chiavi. Li fa Whyed. Merge e invii solo con suo OK. AbletonOSC solo da casella spuntata.
