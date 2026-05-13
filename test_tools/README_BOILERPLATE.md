# 🚀 Boilerplate React — WhyCremisi VST Plugin UI

**Creato da:** Aura  
**Data:** 2026-04-12  
**Destinatario:** Heartbroken

---

## ✅ Cosa è stato creato

Un progetto React completo e funzionante con Vite.

---

## 📁 Struttura

```
webview-ui/
├── index.html              # Entry point HTML
├── package.json            # Dipendenze
├── src/
│   ├── main.jsx           # Entry point React
│   ├── App.jsx            # Componente principale
│   ├── App.css            # Stili App
│   ├── index.css          # Stili globali
│   └── components/
│       ├── index.js       # Export componenti
│       ├── Toolbox.jsx    # 🎯 Componente principale (toolbox modulare)
│       ├── Toolbox.css
│       ├── WidgetSlider.jsx
│       ├── WidgetSlider.css
│       ├── WidgetKnob.jsx
│       ├── WidgetKnob.css
│       ├── WidgetButton.jsx
│       └── WidgetButton.css
```

---

## 🎯 Componenti Pronti

### 1. Toolbox (`Toolbox.jsx`)
**Cosa fa:**
- Riceve messaggi dal plugin via `window.receiveFromPlugin`
- Genera **proposte widget** automaticamente
- Mostra popup "Vuoi aggiungere questo widget?"
- Gestisce widget attivi (aggiungi/rimuovi)
- Mostra log degli eventi

**Come funziona:**
```javascript
// Il plugin invia questo
window.receiveFromPlugin(JSON.stringify({
  message_type: 'event',
  event: {
    type: 'parameter_change',
    category: 'volume',
    target: { track_id: 1, param_name: 'volume' },
    value: { raw: 0.75 }
  }
}));

// La Toolbox genera automaticamente una proposta per uno slider
```

### 2. WidgetSlider, WidgetKnob, WidgetButton
Tutti funzionanti con:
- Visualizzazione valore
- Invio al plugin quando cambiato
- Pulsante rimuovi (×)

---

## 🚀 Come avviare

### 1. Entra nella cartella
```bash
cd /path/to/VST-PlugIn-Ai/webview-ui
```

### 2. Installa dipendenze (già fatto)
```bash
npm install
```

### 3. Avvia in sviluppo
```bash
npm run dev
```

Apre browser su `http://localhost:5173`

---

## 🧪 Come testare senza plugin C++

### Opzione A: Console browser
Apri DevTools (F12) e scrivi:
```javascript
// Simula messaggio dal plugin
window.receiveFromPlugin(JSON.stringify({
  message_type: 'event',
  event: {
    type: 'parameter_change',
    category: 'volume',
    target: { track_id: 1, param_name: 'volume' },
    value: { raw: 0.75 }
  }
}));
```

Vedrai apparire una **proposta** nella UI!

### Opzione B: Mock server OSC + AI
Usa i due mock server che ho creato:
- `mock_osc_server.py` — invia eventi OSC falsi
- `mock_ai_server.py` — risponde con proposte AI

---

## 🎨 Stili

Tema dark con colori:
- **Sfondo:** `#1a1a2e` (blu notte)
- **Accento:** `#e94560` (rosso/rosa)
- **Secondario:** `#533483` (viola)

Tutti i componenti sono **responsive**.

---

## ✏️ Cosa modificare tu

| File | Cosa fare |
|------|-----------|
| `Toolbox.jsx` | Aggiungi più tipi di proposte (XY pad, meter, scope) |
| `Widget*.jsx` | Migliora styling, aggiungi animazioni |
| `App.jsx` | Aggiungi menu, impostazioni, about |
| `App.css` | Cambia tema colori se vuoi |

---

## 🔌 Connessione al plugin reale

Quando Edo avrà il C++ pronto:

1. Il C++ chiama `webView->evaluateJavascript("window.receiveFromPlugin('...')")`
2. La Toolbox riceve automaticamente
3. Rispondi con `window.sendToPlugin({...})` per inviare al C++

---

## 📦 Build per produzione

```bash
npm run build
```

Crea cartella `dist/` con file statici. Edo la includerà nel plugin VST.

---

## ❓ Problemi?

**"Non vedo nulla"** → Controlla console (F12) per errori

**"Proposte non appaiono"** → Usa la console per simulare messaggi (vedi sopra)

**"Voglio aggiungere widget XY"** → Modifica `generateProposal()` in `Toolbox.jsx`

---

**Heartbroken, hai tutto per partire. Buon lavoro! 🚀**
