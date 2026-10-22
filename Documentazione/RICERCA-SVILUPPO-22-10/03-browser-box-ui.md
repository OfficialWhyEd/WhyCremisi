# Ricerca 03: browser interno dell agente e box interattivi (22/10)

Solo ricerca web. Nessuna installazione. Le licenze segnate "da verificare" non sono state confermate sul repo ufficiale.

## 1. Browser interno controllabile dall agente

### Come funziona dentro una WebView
- WebView2 (Windows) si puo' aprire con il porto di debug remoto. Metodo: variabile `WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS=--remote-debugging-port=9222`, oppure argomento `--remote-debugging-port=9222` in `EnsureCoreWebView2Async`. Poi Playwright si collega con `connectOverCDP`.
  - Fonte: https://playwright.dev/docs/webview2
  - Attenzione: TestComplete dice che il flag da solo puo' non bastare e consiglia `--edge-webview-switches=--remote-debugging-port=9222`. Fonte: https://support.smartbear.com/testcomplete/docs/app-testing/web/webview-two/about.html
- Per eventi CDP dentro l app si usa `GetDevToolsProtocolEventReceiver`. Serve a leggere eventi, non ad aprire la porta. Fonte: https://learn.microsoft.com/ja-jp/microsoft-edge/webview2/reference/win32/icorewebview2devtoolsprotocoleventreceiver
- WKWebView (Mac) non ha CDP. Serve un altro percorso: Web Inspector o un bridge JS proprio. Non trovato un approccio standard nelle ricerche.
- Iframe: il sito YouTube non si legge via JS dal plugin (cross-origin). Si controlla solo tramite l API ufficiale del player.

### Repo open source con licenza
| Progetto | Ruolo | Licenza |
|---|---|---|
| Playwright https://github.com/microsoft/playwright | automazione via CDP, funziona con WebView2 | Apache-2.0 (noto, da verificare sul repo) |
| Stagehand https://github.com/browserbase/stagehand | SDK agente su CDP, usa Playwright come base | MIT (da fonti secondarie) |
| browser-use https://github.com/browser-use/browser-use | agente LLM su browser, Python | da verificare (dato noto MIT) |
| Playwright MCP https://github.com/microsoft/playwright-mcp | server MCP per l agente | da verificare |

### Limiti YouTube
- I Termini di servizio di YouTube vietano di scaricare o modificare il servizio senza permesso. Fonte secondaria: https://ecommercetimes.com/?p=173113 (caso 2013). Le ricerche non hanno trovato il testo ufficiale aggiornato.
- Estrarre l audio da YouTube e analizzarlo nel plugin e' il punto piu' a rischio. Rischio legale e di account. Non sono un parere legale.
- Strada consigliata: YouTube IFrame Player API ufficiale (https://developers.google.com/youtube/iframe_api_reference). Il plugin controlla riproduzione e ricerca. Non legge l audio.

## 2. Generative UI e box interattivi nella chat

| Progetto | Cosa e' | Licenza |
|---|---|---|
| A2UI https://github.com/google/A2UI | protocollo dichiarativo JSON per UI generate dall agente | Apache-2.0 (fonte: https://www.getapp.com/all-software/a/a2ui/, da verificare sul repo) |
| AG-UI https://github.com/ag-ui-protocol/ag-ui | protocollo di connessione agente-app, streaming | MIT (da fonti secondarie) |
| MCP-UI / MCP Apps | UI interattive via iframe dentro MCP | da verificare. Lavoro confluito in MCP Apps. Fonte: https://developers.googleblog.com/en/introducing-a2ui-an-open-project-for-agent-driven-interfaces/ |
| CopilotKit https://github.com/CopilotKit/CopilotKit | React: chat, tool call, componenti generati a runtime | MIT (da mirror, verificare repo ufficiale) |
| assistant-ui https://github.com/assistant-ui/assistant-ui | libreria React per chat, headless, shadcn/Tailwind | da verificare (dato noto MIT) |
| Vercel AI SDK generative UI https://sdk.vercel.ai | streamUI: tool che streamano componenti React | Apache-2.0 (da directory). streamUI e' sperimentale, Vercel consiglia AI SDK UI |

### Mostrare il ragionamento con un tasto
Nessun progetto specifico trovato nelle ricerche. Pattern semplice: blocco "thinking" collassato in chat, con tasto per aprire e chiudere. Si puo' copiare dal pattern di assistant-ui o CopilotKit, dopo verifica della licenza.

## 3. Componenti open source per visualizzatori audio

| Componente | Cosa fa | Licenza |
|---|---|---|
| react-audio-spectrum https://www.npmjs.com/package/react-audio-spectrum | spettro in React | MIT (da fonte secondaria) |
| DavidLazic/react-audio-visualizer https://github.com/DavidLazic/react-audio-visualizer | componente React di visualizzazione | MIT (nel repo, da verificare) |
| tkhdev/react-audio-visualizer https://github.com/tkhdev/react-audio-visualizer | hook React, 38 modalita' | da verificare |
| polarity/app.vectorscope https://github.com/polarity/app.vectorscope | vectorscope su canvas, VU per L R M S. JS puro, da portare in React | MIT (da verificare) |
| mantine-audio https://awesome.ecosyste.ms/projects/gfazioli.github.io%2Fmantine-audio%2F | player con waveform e spettro, Mantine | MIT (da fonte secondaria) |

EQ interattivo e sample browser con anteprima: non trovato un componente pronto con licenza confermata. Da costruire con Web Audio API (BiquadFilterNode, AnalyserNode). Non sono stati trovati componenti verificati per questi due.

## Raccomandazione (breve)
1. Browser: WebView2 con `--remote-debugging-port` e Playwright via `connectOverCDP` su Windows. Su Mac serve un percorso diverso, da studiare.
2. YouTube: solo IFrame API ufficiale. Niente estrazione audio.
3. Chat: CopilotKit o assistant-ui per i box, A2UI come formato dei componenti. Verificare le licenze sui repo ufficiali prima di usarle.
4. Audio: react-audio-spectrum e polarity vectorscope come base. EQ e sample browser da scrivere con Web Audio API.
