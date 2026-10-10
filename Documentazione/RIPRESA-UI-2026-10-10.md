# WhyCremisi: dove siamo arrivati (10 ottobre 2026, sera)

## Codex (fermo fino al 14/10 17:40, limite settimanale)
- Ramo fix/azioni-daw-20261010, commit bca8ba0: comandi DAW validati, esiti "inviato" e "confermato" separati.
- Build nuova in build-20261010. Suite: 2544 controlli, 0 falliti (riverificato da Claude il 10/10).
- VST3 e standalone compilati, standalone si apre. Plugin installato NON sostituito.
- Aperti: scala volume e pan di Ableton, rilettura del valore, collaudo vero dentro Live 11.

## Claude (fermo per limite di sessione)
- Ramo ui/riprogetto-20261010 (parte da fix/azioni-daw). Commit f99a0ae: documenti di Codex.
- Commit WIP del riprogetto UI: token di design in webview-ui/src/index.css (+88 righe)
  e tre file base in webview-ui/src/components/onboarding/: bridge.js, catalog.js, i18n.js.
- NON ancora collegati ad App.jsx. Build UI passa. SetupScreen vecchio ancora in uso.

## Prossimo passo (in ordine)
1. Finire il primo avvio in components/onboarding/: Benvenuto e lingua, tre strade
   (Gratis: Ollama o OpenRouter :free o Gemini; Abbonamento: Claude con ai.detectSubscription
   e ai.authToken; Chiave API: provider veri in src/), Collegamento, Prova reale del modello,
   DAW, Pronto. ChatGPT/Codex con abbonamento solo "in arrivo", non e' nel C++.
2. Collegarlo ad App.jsx al posto di SetupScreen, build, lint, screenshot 1200x800 e 900x600.
3. Rifare la schermata principale con gli stessi token (App.jsx 132KB, va divisa).
4. Build VST3 con la UI nuova, collaudo in Ableton (volume, pan, mute, solo, rilettura).
5. Fase 2 del piano: adapter Claude Agent SDK e Codex app-server (vedi PIANO-UPGRADE-2026-10.md).
