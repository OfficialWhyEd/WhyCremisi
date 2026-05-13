# Protocollo JSON C++ ↔ JavaScript v1.0

**Data:** 2026-04-12  
**Versione:** 1.0  
**Stato:** Definitivo per implementazione

---

## Overview

Protocollo di comunicazione bidirezionale tra il backend C++ (JUCE) e il frontend React (WebView).

**Direzione C++ → JavaScript:** `WebViewBridge::sendToFrontend()` usa `goToURL("javascript:...")`  
**Direzione JavaScript → C++:** URL interception via `pageAboutToLoad` o `alert()` bridging

---

## Message Format

Tutti i messaggi sono JSON con questa struttura base:

```json
{
  "type": "message_type",
  "id": "uuid-v4-optional",
  "timestamp": 1712923200,
  "payload": { ... }
}
```

| Campo | Tipo | Descrizione |
|-------|------|-------------|
| `type` | string | Tipo del messaggio (vedi tabella sotto) |
| `id` | string | UUID v4 per correlazione request/response (opzionale per events) |
| `timestamp` | number | Unix timestamp in millisecondi |
| `payload` | object | Dati specifici del messaggio |

---

## Message Types

### 1. C++ → JavaScript (Backend to Frontend)

#### `daw.transport`
Stato trasporto DAW (play, stop, record, tempo, posizione).

```json
{
  "type": "daw.transport",
  "id": null,
  "timestamp": 1712923200000,
  "payload": {
    "isPlaying": true,
    "isRecording": false,
    "bpm": 120.0,
    "positionSeconds": 45.5,
    "positionBars": 16.2,
    "timeSignature": {"numerator": 4, "denominator": 4}
  }
}
```

#### `daw.track`
Informazioni su una traccia.

```json
{
  "type": "daw.track",
  "id": null,
  "timestamp": 1712923200000,
  "payload": {
    "trackId": 1,
    "name": "Lead Vocal",
    "color": "#FF6B6B",
    "volume": 0.8,
    "volumeDb": -2.0,
    "pan": 0.0,
    "isMuted": false,
    "isSoloed": false,
    "isArmed": true
  }
}
```

#### `daw.meter`
Metering audio (livelli in dB).

```json
{
  "type": "daw.meter",
  "id": null,
  "timestamp": 1712923200000,
  "payload": {
    "trackId": 1,
    "leftDb": -12.5,
    "rightDb": -11.8,
    "peakLeftDb": -6.2,
    "peakRightDb": -5.9
  }
}
```

#### `daw.clip`
Informazioni su una clip/region.

```json
{
  "type": "daw.clip",
  "id": null,
  "timestamp": 1712923200000,
  "payload": {
    "trackId": 1,
    "clipId": "clip_001",
    "name": "Verse 1",
    "startTime": 10.0,
    "duration": 8.0,
    "color": "#4ECDC4",
    "isSelected": false
  }
}
```

#### `osc.message`
Messaggio OSC raw ricevuto (per debug/riscontro).

```json
{
  "type": "osc.message",
  "id": null,
  "timestamp": 1712923200000,
  "payload": {
    "address": "/track/1/volume",
    "value": 0.75,
    "valueType": "float"
  }
}
```

#### `ai.response`
Risposta dall'AI engine.

```json
{
  "type": "ai.response",
  "id": "req-uuid-123",
  "timestamp": 1712923200000,
  "payload": {
    "status": "success",
    "provider": "ollama",
    "model": "llama3.2",
    "content": "Suggerimento: alza i 3kHz di 2dB per chiarezza...",
    "tokensUsed": 150,
    "latencyMs": 850
  }
}
```

#### `ai.stream`
Streaming parziale della risposta AI (per LLM che supportano streaming).

```json
{
  "type": "ai.stream",
  "id": "req-uuid-123",
  "timestamp": 1712923200000,
  "payload": {
    "chunk": "Suggerimento: alza",
    "isDone": false
  }
}
```

#### `ui.widget.create`
Crea un widget dinamico nella UI (toolbox modulare).

```json
{
  "type": "ui.widget.create",
  "id": "widget-001",
  "timestamp": 1712923200000,
  "payload": {
    "widgetType": "slider",
    "title": "EQ High",
    "trackId": 1,
    "parameter": "eq.high",
    "minValue": -15.0,
    "maxValue": 15.0,
    "defaultValue": 0.0,
    "step": 0.1,
    "unit": "dB"
  }
}
```

#### `ui.widget.update`
Aggiorna un widget esistente.

```json
{
  "type": "ui.widget.update",
  "id": "widget-001",
  "timestamp": 1712923200000,
  "payload": {
    "widgetId": "widget-001",
    "currentValue": 3.5,
    "isEnabled": true
  }
}
```

#### `ui.widget.remove`
Rimuove un widget.

```json
{
  "type": "ui.widget.remove",
  "id": null,
  "timestamp": 1712923200000,
  "payload": {
    "widgetId": "widget-001"
  }
}
```

#### `plugin.error`
Errore del plugin.

```json
{
  "type": "plugin.error",
  "id": null,
  "timestamp": 1712923200000,
  "payload": {
    "code": "OSC_CONNECT_FAILED",
    "message": "Impossibile connettersi alla porta OSC 9000",
    "severity": "error"
  }
}
```

---

### 2. JavaScript → C++ (Frontend to Backend)

#### `plugin.init`
Inizializzazione quando la UI è pronta.

```json
{
  "type": "plugin.init",
  "id": "init-001",
  "timestamp": 1712923200000,
  "payload": {
    "version": "1.0.0",
    "capabilities": ["widgets", "ai", "osc"]
  }
}
```

#### `daw.command`
Comando per controllare il DAW (richiede supporto OSC bidirezionale).

```json
{
  "type": "daw.command",
  "id": "cmd-001",
  "timestamp": 1712923200000,
  "payload": {
    "command": "play",
    "target": "transport"
  }
}
```

Comandi supportati:
- `play`, `stop`, `record`, `pause` - Transport
- `setVolume` - `{trackId, valueDb}`
- `setPan` - `{trackId, value}`
- `armTrack` - `{trackId, armed}`
- `muteTrack` - `{trackId, muted}`
- `soloTrack` - `{trackId, soloed}`
- `setTempo` - `{bpm}`

#### `daw.request`
Richiesta informazioni dal DAW.

```json
{
  "type": "daw.request",
  "id": "req-track-001",
  "timestamp": 1712923200000,
  "payload": {
    "request": "trackInfo",
    "trackId": 1
  }
}
```

Richieste supportate:
- `transport` - Stato trasporto
- `trackInfo` - Info traccia specifica
- `trackList` - Lista tracce
- `meter` - Metering attuale
- `projectInfo` - Info progetto

#### `ai.prompt`
Invia prompt all'AI.

```json
{
  "type": "ai.prompt",
  "id": "ai-req-001",
  "timestamp": 1712923200000,
  "payload": {
    "prompt": "Analizza la traccia 1 e suggerisci EQ",
    "provider": "ollama",
    "model": "llama3.2",
    "stream": true,
    "context": {
      "trackId": 1,
      "trackName": "Lead Vocal",
      "meterDb": -12.5
    }
  }
}
```

#### `widget.valueChange`
Widget modificato dall'utente.

```json
{
  "type": "widget.valueChange",
  "id": "wch-001",
  "timestamp": 1712923200000,
  "payload": {
    "widgetId": "widget-001",
    "value": 3.5,
    "source": "user"
  }
}
```

#### `osc.send`
Invia messaggio OSC raw.

```json
{
  "type": "osc.send",
  "id": "osc-001",
  "timestamp": 1712923200000,
  "payload": {
    "address": "/track/1/volume",
    "value": 0.8,
    "valueType": "float"
  }
}
```

#### `config.get`
Richiede configurazione plugin.

```json
{
  "type": "config.get",
  "id": "cfg-001",
  "timestamp": 1712923200000,
  "payload": {
    "key": "osc.port"
  }
}
```

#### `config.set`
Imposta configurazione.

```json
{
  "type": "config.set",
  "id": "cfg-002",
  "timestamp": 1712923200000,
  "payload": {
    "key": "osc.port",
    "value": 9001
  }
}
```

---

## Implementazione C++

### WebViewBridge aggiornato

Il meccanismo JS→C++ usa URL interception. La UI React naviga a un URL speciale che JUCE intercetta:

```cpp
// In PluginEditor, setup WebBrowserComponent
webView.addListener(this);

// Intercetta navigazioni da JavaScript
bool pageAboutToLoad(const String& url) override
{
    if (url.startsWith("app://message/"))
    {
        // Estrai JSON dal path
        String jsonStr = url.substring(14); // dopo "app://message/"
        jsonStr = URL::addEscapeChars(jsonStr, true); // decode
        webViewBridge.handleMessageFromFrontend(jsonStr);
        return false; // blocca navigazione reale
    }
    return true; // lascia navigare
}
```

### Invio C++ → JS

```cpp
void WebViewBridge::sendToFrontend(const nlohmann::json& message)
{
    if (!webView) return;
    
    String jsonStr = message.dump();
    
    // Escape per JavaScript
    jsonStr = jsonStr.replace("\\", "\\\\");   // prima \
    jsonStr = jsonStr.replace("\"", "\\\"");      // poi \"
    jsonStr = jsonStr.replace("'", "\\'");        // e '
    jsonStr = jsonStr.replace("\n", "\\n");      // newline
    jsonStr = jsonStr.replace("\r", "\\r");       // carriage return
    
    // Chiama la funzione globale nella UI
    String jsCode = "javascript:if(window.__whycremisiBridge){window.__whycremisiBridge.receiveMessage(\"" + jsonStr + "\");}";
    webView->goToURL(jsCode);
}
```

---

## Implementazione JavaScript

### Bridge JavaScript (iniettato nella pagina)

```javascript
// Questo viene iniettato da C++ o incluso nella build React
window.__whycremisiBridge = {
  // Ricezione messaggi da C++
  receiveMessage: (jsonString) => {
    try {
      const message = JSON.parse(jsonString);
      window.dispatchEvent(new CustomEvent('whycremisi-message', { detail: message }));
    } catch (e) {
      console.error('WhyCremisi: Failed to parse message', e);
    }
  },
  
  // Invio messaggi a C++
  sendMessage: (message) => {
    const jsonStr = JSON.stringify(message);
    // Naviga a URL speciale che C++ intercetta
    window.location.href = `app://message/${encodeURIComponent(jsonStr)}`;
  }
};

// Hook React per usare il bridge
export const useWhyCremisi = () => {
  const sendMessage = (type, payload) => {
    window.__whycremisiBridge?.sendMessage({
      type,
      id: generateUUID(),
      timestamp: Date.now(),
      payload
    });
  };
  
  useEffect(() => {
    const handler = (e) => {
      // Processa messaggi da C++
      console.log('Message from C++:', e.detail);
    };
    window.addEventListener('whycremisi-message', handler);
    return () => window.removeEventListener('whycremisi-message', handler);
  }, []);
  
  return { sendMessage };
};
```

---

## Schema Validazione (JSON Schema)

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "title": "WhyCremisi Message",
  "type": "object",
  "required": ["type", "timestamp", "payload"],
  "properties": {
    "type": {
      "type": "string",
      "enum": [
        "daw.transport", "daw.track", "daw.meter", "daw.clip", "daw.command", "daw.request",
        "osc.message", "osc.send",
        "ai.response", "ai.stream", "ai.prompt",
        "ui.widget.create", "ui.widget.update", "ui.widget.remove", "widget.valueChange",
        "plugin.init", "plugin.error",
        "config.get", "config.set"
      ]
    },
    "id": { "type": ["string", "null"] },
    "timestamp": { "type": "number" },
    "payload": { "type": "object" }
  }
}
```

---

## Changelog

| Versione | Data | Cambiamenti |
|----------|------|-------------|
| 1.0 | 2026-04-12 | Versione iniziale definitiva |

---

**File correlati:**
- `src/ui/WebViewBridge.h` - Header C++
- `src/ui/WebViewBridge.cpp` - Implementazione C++
- `webview-ui/src/` - Frontend React (da implementare)
