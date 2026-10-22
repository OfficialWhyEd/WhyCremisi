# Guida Sviluppo UI — WhyCremisi VST Bridge AI Edition

**Versione:** 1.0
**Data:** 2026-04-14
**Autore:** Aura
**Destinatari:** Edo, Heartbroken, Jetty, chiunque contribuisca alla GUI

---

## 1. Architettura UI

### 1.1 Cartelle e ruoli

```
src/ui/
├── frontend/                          # Frontend Vite+React (bozza iniziale, NON attivo)
├── Prototipi/
│   ├── Front End Ui Test 1/           # PROTOTIPO MATURO — qui Edo sviluppa
│   │   ├── src/
│   │   │   ├── App.tsx                # UI completa WhyCremisi
│   │   │   ├── whycremisi-bridge.js     # Bridge AURA Protocol v1.0 (app://message/)
│   │   │   ├── components/
│   │   │   │   └── BotFace.tsx        # Avatar animato SVG morphing
│   │   │   ├── index.css              # Stili WhyCremisi (Tailwind)
│   │   │   └── main.tsx
│   │   ├── temp_rebirth/              # Vecchia versione (ARCHIVIO)
│   │   └── package.json
│   ├── Main Prototipo/               # HTML prototype + MASTER_SPEC.md
│   ├── LOGHI TEST/                   # Asset grafici
│   ├── MarathonTerminalUI/           # Varianti prototipo (riferimento)
│   └── Prototipo_1/                  # ~50 varianti HTML (riferimento)
│
webview-ui/                           # ★ FRONTEND PRODUZIONE ★
├── src/
│   ├── App.jsx                       # App principale (UI WhyCremisi integrata)
│   ├── whycremisi-bridge.js            # Bridge WebSocket RFC 6455 (PRODUZIONE)
│   ├── components/
│   │   ├── BotFace.jsx               # Avatar animato (da Prototipo)
│   │   ├── Toolbox.jsx              # Widget system (proposal/accept)
│   │   ├── WidgetSlider.jsx
│   │   ├── WidgetKnob.jsx
│   │   └── WidgetButton.jsx
│   ├── index.css                     # Stili globali WhyCremisi
│   └── main.jsx
├── index.html
├── package.json
└── vite.config.js
```

### 1.2 Due bridge, due protocolli

| | Prototipo (Edo) | Produzione (webview-ui) |
|---|---|---|
| **File** | `Prototipi/.../whycremisi-bridge.js` | `webview-ui/src/whycremisi-bridge.js` |
| **Protocollo** | `app://message/` URL interception | WebSocket RFC 6455 (`ws://localhost:17373`) |
| **Uso** | Sviluppo in isolamento, Google AI Studio | VST3 in Reaper, Standalone |
| **API** | `bridge.send(type, payload)` | `whycremisi.sendDAWCommand()`, `whycremisi.sendAIPrompt()`, ecc. |

**Il pattern comune:** entrambi espongono `bridge.send(type, payload)` e `bridge.on(type, callback)`.

I componenti React (BotFace, Toolbox, ecc.) chiamano SOLO `bridge.send()` e `bridge.on()`.
Non conoscono il protocollo sottostante. Quando integrate nel VST3, il bridge cambia implementazione, i componenti no.

---

## 2. Come sviluppare la GUI (per Edo)

### 2.1 Ambiente di lavoro

Lavora in: `src/ui/Prototipi/Front End Ui Test 1/`

```bash
cd "src/ui/Prototipi/Front End Ui Test 1"
npm install
npm run dev
```

Apri `http://localhost:5173` nel browser.

### 2.2 Regole per i componenti

1. **Usa sempre `bridge.send()` e `bridge.on()`** per comunicare con il backend
2. **Non usare `window.receiveFromPlugin` direttamente** nei componenti — è responsabilità del bridge
3. **Non importare `whycremisi-bridge.js` direttamente nei componenti** — importa sempre `bridge` dal file bridge
4. **Mantieni il design system WhyCremisi** (palette, font, CRT overlay) come in MASTER_SPEC.md
5. **Ogni nuovo componente React deve essere standalone** — un file, una responsabilità

### 2.3 Messaggi supportati (protocollo JSON v1.0)

Consulta `Documentazione/protocol-json-v1.md` per il formato completo.

Tipi messaggi che la UI può inviare:
- `ai.prompt` — Invia prompt all'AI engine
- `daw.command` — Controlla il DAW (play, stop, record, setVolume, setPan, ecc.)
- `daw.request` — Richiede info al DAW (transport, trackInfo, trackList, meter)
- `osc.send` — Invia messaggio OSC raw
- `ui.widget.create/update/remove` — Gestione widget dinamici
- `config.get/set` — Configurazione plugin
- `plugin.init` — Inizializzazione (inviato automaticamente alla connessione)

Tipi messaggi che la UI riceve:
- `daw.transport` — Stato trasporto (isPlaying, bpm, position, ecc.)
- `daw.track` — Info traccia
- `daw.meter` — Metering audio
- `ai.response` — Risposta AI
- `ai.stream` — Streaming AI
- `osc.message` — Messaggio OSC raw
- `ui.widget.create/update/remove` — Aggiornamenti widget
- `plugin.error` — Errore

### 2.4 Esempio: collegare un componente al bridge

```jsx
import bridge from './whycremisi-bridge';

function MyComponent() {
  const [isPlaying, setIsPlaying] = useState(false);

  useEffect(() => {
    const unsub = bridge.on('daw.transport', (payload) => {
      setIsPlaying(payload.isPlaying);
    });
    return () => unsub();
  }, []);

  const handlePlay = () => {
    bridge.send('daw.command', { command: 'play' });
  };

  return <button onClick={handlePlay}>{isPlaying ? 'Stop' : 'Play'}</button>;
}
```

---

## 3. Come testare la GUI nel VST3

### 3.1 Build del plugin

```bash
cd /home/carlo/HDs/AI-Ubuntu/progetti/WhyCremisi-VST-Bridge/build
cmake .. && make -j4
```

### 3.2 Installazione VST3

```bash
cp -r build/WhyCremisiVSTPlugin_artefacts/Release/VST3/WhyCremisiVSTBridgeAI.vst3 ~/.vst3/
```

### 3.3 Test in Reaper

1. Apri Reaper
2. Inserisci FX → WhyCremisiVSTBridgeAI
3. Il plugin si apre con la GUI WebView
4. La GUI si connette via WebSocket a `ws://localhost:17373`
5. I controlli nella GUI inviano messaggi JSON al plugin C++
6. Il plugin C++ li traduce in OSC verso Reaper (`/play`, `/stop`, `/record`)
7. Reaper risponde e il plugin riceve feedback via OSC (porta 8000)

### 3.4 Configurazione OSC Reaper (OBBLIGATORIA)

 Reaper → Options → Preferences → Control/OSC/web:
- **Device port:** 9000 (il plugin invia qui)
- **Device IP:** 127.0.0.1
- **Local listen port:** 8000 (Reaper ascolta qui)
- **Local IP:** 192.168.1.12

**Importante:** Reaper usa OSC addresses nel formato BREVE:
- `/play` (non `/transport/play`)
- `/stop` (non `/transport/stop`)
- `/record` (non `/transport/record`)

### 3.5 Test standalone (secondario)

```bash
./build/WhyCremisiVSTPlugin_artefacts/Release/Standalone/WhyCremisiVSTBridgeAI
```

Lo standalone NON è ancora autosufficiente — richiede Reaper in esecuzione per il feedback OSC.

---

## 4. Design System WhyCremisi (riferimento)

Da `MASTER_SPEC.md`:

- **Palette:**
  - Primario: `#DC143C` (Rosso Cremisi)
  - Secondario: `#FFB000` (Ambra/Glow)
  - Sfondo: `#0d0d0d` / `#131313`
  - Testo: `#e5e2e1`
  - Muted: `#4d4d4d`
  - Border: `#222222`

- **Font:** Space Grotesk (Google Fonts)

- **Effetti:**
  - CRT Overlay (scanlines, trasparenza 0.02-0.05)
  - LED Glow (box-shadow su stati attivi)
  - Advisory Breathe (pulsazione border)
  - Glitch hover sui bottoni
  - Terminal cursor lampeggiante

- **Regola:** ogni modifica grafica DEVE mantenere il contrasto `#DC143C` vs `#FFB000`.

---

## 5. Workflow Git

### 5.1 Branch

| Branch | Proprietario | Uso |
|--------|-------------|-----|
| `master` | Aura (main) | Codice production |
| `heartbroken` | Edo (Heartbroken) | Sviluppo di Edo |
| `aura` | (deprecato) | Non usare |

### 5.2 Prima di lavorare

```bash
git checkout heartbroken
git pull origin master          # Prende le fix recenti
git merge origin/master         # Risolvi conflitti se necessario
# ... lavora ...
git add . && git commit -m "[HB] Descrizione modifica"
git push origin heartbroken     # NON pushare su master
```

### 5.3 Tag commit

- `[AURA]` — Commit di Aura
- `[HB]` — Commit di Heartbroken
- `[JETTY]` — Commit di Jetty

### 5.4 Regola d'oro

**Non pushare MAI su `master` senza il consenso di Carlo.**

---

## 6. DAW Support

### 6.1 Ableton Live (primario)

Ableton supporta OSC limitatamente. Per il controllo completo serve il protocollo HTML/MIDI.
Stato attuale: NON implementato. TODO.

### 6.2 Reaper (secondario, ma già funzionante)

Supporto OSC completo. Già implementato:
- Play/Stop/Record via `/play`, `/stop`, `/record`
- Volume traccia via `/track/{id}/volume`
- Pan traccia via `/track/{id}/pan`
- Mute/Solo via `/track/{id}/mute`, `/track/{id}/solo`
- Tempo via `/tempo`

### 6.3 Parametri OSC mappati (parziale)

| Comando | Indirizzo OSC | Valore | DAW |
|---------|--------------|--------|-----|
| Play | `/play` | 1.0 | Reaper |
| Stop | `/stop` | 1.0 | Reaper |
| Record | `/record` | 1.0 | Reaper |
| Volume | `/track/{id}/volume` | 0.0-1.0 | Reaper |
| Pan | `/track/{id}/pan` | -1.0-1.0 | Reaper |
| Mute | `/track/{id}/mute` | 0/1 | Reaper |
| Solo | `/track/{id}/solo` | 0/1 | Reaper |
| Tempo | `/tempo` | BPM | Reaper |

---

## 7. Checklist integrazione GUI nel VST3

Quando Edo ha un componente pronto per la produzione:

1. Il componente è in `src/ui/Prototipi/Front End Ui Test 1/src/`
2. Usa solo `bridge.send()` e `bridge.on()` (nessun `window.*` diretto)
3. Passa il lint: nessun console.log di debug, nessun hardcoded value
4. Copiare in `webview-ui/src/components/`
5. Se è un file `.tsx`, convertirlo a `.jsx` o configurare TypeScript nel webview-ui
6. Testare con `npm run dev` in `webview-ui/`
7. Build VST3 e test in Reaper

---

## 8. Dipendenze webview-ui (da aggiungere per supportare WhyCremisi)

Per integrare il design WhyCremisi nel frontend production, vanno aggiunte:

```json
{
  "dependencies": {
    "framer-motion": "^12.38.0",
    "motion": "^12.23.24"
  },
  "devDependencies": {
    "tailwindcss": "^4.1.14",
    "@tailwindcss/vite": "^4.1.14"
  }
}
```

E in `vite.config.js` aggiungere il plugin tailwindcss.

---

*Ultimo aggiornamento: 2026-04-14 — Aura*