/**
 * WhyCremisi™ · A WhyEd Project
 * © 2026 WhyEd™ — @whyed.music · MIT License
 *
 * WhyCremisi Bridge — WebSocket singleton for VST ↔ React communication.
 *
 * Replaces the legacy app://message/ WebView approach with WebSocket RFC 6455
 * connecting to the WebSocketServer inside the plugin.
 * 
 * Il bridge può connettersi a:
 * - localhost:8080 (plugin in production/standalone)
 * - ws://localhost:8080 (stesso)
 * 
 * Usage:
 *   import { whycremisi } from './whycremisi-bridge.js';
 *   whycremisi.connect();
 */

const WS_DEFAULT_URL = 'ws://localhost:8080';
const RECONNECT_INTERVAL_MS = 2000;
const MAX_RECONNECT_ATTEMPTS = 10;

/**
 * Genera UUID v4
 */
function generateUUID() {
  return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function(c) {
    const r = Math.random() * 16 | 0;
    const v = c === 'x' ? r : (r & 0x3 | 0x8);
    return v.toString(16);
  });
}

/**
 * Stato connessione
 */
export const ConnectionState = {
  DISCONNECTED: 'disconnected',
  CONNECTING: 'connecting',
  CONNECTED: 'connected',
  RECONNECTING: 'reconnecting',
  ERROR: 'error'
};

/**
 * Classe principale del bridge WhyCremisi
 */
class WhyCremisiBridge {
  constructor() {
    this.ws = null;
    this.url = WS_DEFAULT_URL;
    this.state = ConnectionState.DISCONNECTED;
    this.messageListeners = new Map();
    this.pendingRequests = new Map();
    this.isInitialized = false;
    this.version = '1.0.0';
    this.reconnectAttempts = 0;
    this.reconnectTimer = null;
    this.lastConnectedAt = null;
    this.stateListeners = [];

    this.botState = 'idle';

    // Bind metodi
    this._handleMessage = this._handleMessage.bind(this);
    this._handleOpen = this._handleOpen.bind(this);
    this._handleClose = this._handleClose.bind(this);
    this._handleError = this._handleError.bind(this);
  }

  onStateChange(callback) {
    this.stateListeners.push(callback);
    return () => {
      this.stateListeners = this.stateListeners.filter(l => l !== callback);
    };
  }

  _setState(newState) {
    this.state = newState;
    this.stateListeners.forEach(l => l(newState));
  }

  /**
   * Connetti al WebSocket server del plugin
   * @param {string} url - URL del WebSocket server (default: ws://localhost:8080)
   * @returns {Promise} - Resolves quando connesso
   */
  connect(url = WS_DEFAULT_URL) {
    return new Promise((resolve, reject) => {
      if (this.state === ConnectionState.CONNECTED) {
        resolve();
        return;
      }

      this.url = url;
      this._setState(ConnectionState.CONNECTING);
      console.log('[WhyCremisi] Connessione a ' + url + '...');

      try {
        this.ws = new WebSocket(url);

        this.ws.onopen = (event) => {
          this._handleOpen(event);
          resolve();
        };

        this.ws.onclose = (event) => {
          this._handleClose(event);
        };

        this.ws.onerror = (event) => {
          this._handleError(event);
          reject(new Error('WebSocket error'));
        };

        this.ws.onmessage = (event) => {
          this._handleMessage(event);
        };

      } catch (e) {
        this._setState(ConnectionState.ERROR);
        console.error('[WhyCremisi] Errore connessione:', e);
        reject(e);
      }
    });
  }

  /**
   * Disconnetti dal WebSocket server
   */
  disconnect() {
    this._clearReconnectTimer();
    this.reconnectAttempts = 0;

    if (this.ws) {
      this.ws.onclose = null; // Prevent reconnect on intentional close
      this.ws.close(1000, 'Client disconnect');
      this.ws = null;
    }

    this._setState(ConnectionState.DISCONNECTED);
    console.log('[WhyCremisi] Disconnesso');
  }

  /**
   * Verifica se connesso
   */
  isConnected() {
    return this.state === ConnectionState.CONNECTED && this.ws && this.ws.readyState === WebSocket.OPEN;
  }

  /**
   * Invia messaggio al plugin via WebSocket
   * @param {string} type - Tipo messaggio
   * @param {object} payload - Payload JSON
   * @param {object} options - Opzioni {id, onResponse, timeout}
   * @returns {string} - Message ID
   */
  sendMessage(type, payload = {}, options = {}) {
    if (!this.isConnected()) {
      console.warn('[WhyCremisi] Non connesso, messaggio non inviato:', type);
      return null;
    }

    const id = options.id || generateUUID();
    const message = {
      type,
      id,
      timestamp: Date.now(),
      payload
    };

    // Se c'è un callback per risposta, salva
    if (options.onResponse) {
      this.pendingRequests.set(id, {
        callback: options.onResponse,
        timeout: options.timeout || 30000,
        timer: setTimeout(() => {
          this.pendingRequests.delete(id);
          options.onResponse({ error: 'Timeout' });
        }, options.timeout || 30000)
      });
    }

    // Invia via WebSocket
    try {
      this.ws.send(JSON.stringify(message));
      console.log('[WhyCremisi] Inviato:', type, id ? `(id=${id})` : '');
    } catch (e) {
      console.error('[WhyCremisi] Errore invio:', e);
    }

    return id;
  }

  /**
   * Alias per sendMessage - sends a raw JSON message
   */
  send(jsonMessage) {
    if (!this.isConnected()) {
      console.warn('[WhyCremisi] Non connesso');
      return null;
    }
    try {
      this.ws.send(JSON.stringify(jsonMessage));
    } catch (e) {
      console.error('[WhyCremisi] Errore invio:', e);
    }
  }

  /**
   * Registra listener per un tipo di messaggio
   * @param {string} type - Tipo messaggio ('daw.transport', 'ai.response', ecc.)
   * @param {function} callback - Callback(payload, fullMessage)
   * @returns {function} - Unsubscribe function
   */
  on(type, callback) {
    if (!this.messageListeners.has(type)) {
      this.messageListeners.set(type, []);
    }
    this.messageListeners.get(type).push(callback);

    return () => {
      const listeners = this.messageListeners.get(type);
      const index = listeners.indexOf(callback);
      if (index > -1) {
        listeners.splice(index, 1);
      }
    };
  }

  /**
   * Rimuovi tutti i listener per un tipo
   */
  off(type) {
    if (type) {
      this.messageListeners.delete(type);
    } else {
      this.messageListeners.clear();
    }
  }

  /**
   * Registra listener per tutti i messaggi
   * @param {function} callback - Callback(message)
   * @returns {function} - Unsubscribe
   */
  onAny(callback) {
    if (!this.messageListeners.has('*')) {
      this.messageListeners.set('*', []);
    }
    this.messageListeners.get('*').push(callback);

    return () => {
      const listeners = this.messageListeners.get('*');
      const index = listeners.indexOf(callback);
      if (index > -1) {
        listeners.splice(index, 1);
      }
    };
  }

  /**
   * Richiede info al DAW
   */
  requestDAWInfo(requestType, trackId = null) {
    return new Promise((resolve, reject) => {
      const payload = { request: requestType };
      if (trackId !== null) {
        payload.trackId = trackId;
      }

      this.sendMessage('daw.request', payload, {
        onResponse: (response) => {
          if (response.error) {
            reject(new Error(response.error));
          } else {
            resolve(response);
          }
        },
        timeout: 5000
      });
    });
  }

  /**
   * Invia comando al DAW
   * @param {string} command - 'play', 'stop', 'record', 'setVolume', ecc.
   * @param {object} params - {trackId, value, valueDb, ...}
   */
  sendDAWCommand(command, params = {}) {
    return this.sendMessage('daw.command', {
      command,
      ...params
    });
  }

  /**
   * Invia prompt all'AI
   * @param {string} prompt - Testo prompt
   * @param {object} options - {provider, model, stream, context}
   */
  sendAIPrompt(prompt, options = {}) {
    const {
      provider = 'ollama',
      model = 'llama3.2',
      stream = false,
      context = {}
    } = options;

    return this.sendMessage('ai.prompt', {
      prompt,
      provider,
      model,
      stream,
      context
    }, {
      onResponse: options.onResponse,
      timeout: 60000
    });
  }

  /**
   * Crea widget dinamico
   */
  createWidget(widgetType, title, config = {}) {
    const widgetId = config.widgetId || generateUUID();
    return this.sendMessage('ui.widget.create', {
      widgetType,
      title,
      widgetId,
      ...config
    });
  }

  /**
   * Aggiorna widget
   */
  updateWidget(widgetId, values) {
    return this.sendMessage('ui.widget.update', {
      widgetId,
      ...values
    });
  }

  /**
   * Rimuovi widget
   */
  removeWidget(widgetId) {
    return this.sendMessage('ui.widget.remove', { widgetId });
  }

  /**
   * Invia messaggio OSC raw
   */
  sendOSC(address, value, valueType = 'float') {
    return this.sendMessage('osc.send', {
      address,
      value,
      valueType
    });
  }

  /**
   * Richiedi configurazione
   */
  getConfig(key) {
    return new Promise((resolve, reject) => {
      this.sendMessage('config.get', { key }, {
        onResponse: (response) => {
          if (response.error) reject(new Error(response.error));
          else resolve(response.value);
        },
        timeout: 3000
      });
    });
  }

  /**
   * Imposta configurazione
   */
  setConfig(key, value) {
    return this.sendMessage('config.set', { key, value });
  }

  /**
   * Gets the connection state
   */
  getState() {
    return this.state;
  }

  /**
   * Bot state management (for BotFace integration)
   */
  setBotState(state) {
    this.botState = state;
    window.dispatchEvent(new CustomEvent('whycremisi-botstate', { detail: state }));
  }

  getBotState() {
    return this.botState;
  }

  /**
   * Gets connection info
   */
  getConnectionInfo() {
    return {
      state: this.state,
      url: this.url,
      reconnectAttempts: this.reconnectAttempts,
      lastConnectedAt: this.lastConnectedAt
    };
  }

  // ========================
  // Private Methods
  // ========================

  _handleOpen(event) {
    this._setState(ConnectionState.CONNECTED);
    this.reconnectAttempts = 0;
    this.lastConnectedAt = Date.now();
    console.log('[WhyCremisi] Connesso a', this.url);

    // Esponi globalmente per retrocompatibilita con prototipo di Edo e C++ WebView
    window.__whycremisiBridge = {
      receiveMessage: (jsonString) => {
        try {
          const msg = JSON.parse(jsonString);
          this._handleMessage({ data: jsonString });
        } catch (e) {
          console.error('[WhyCremisi] Errore parsing:', e);
        }
      },
      sendMessage: this.sendMessage.bind(this),
      isConnected: this.isConnected.bind(this)
    };

    // Per C++ JUCE: callback globale ricevuta messaggi
    window.receiveFromPlugin = (jsonString) => {
      try {
        const msg = JSON.parse(jsonString);
        this._handleMessage({ data: jsonString });
      } catch (e) {
        console.error('[WhyCremisi] Errore parsing receiveFromPlugin:', e);
      }
    };

    // Notifica plugin che siamo pronti
    this.sendMessage('plugin.init', {
      version: this.version,
      capabilities: ['widgets', 'ai', 'osc', 'daw']
    });
  }

  _handleClose(event) {
    const wasConnected = this.state === ConnectionState.CONNECTED;
    this._setState(ConnectionState.DISCONNECTED);
    console.log('[WhyCremisi] Connessione chiusa:', event.code, event.reason);

    // Cleanup
    this._clearPendingRequests();
    this.ws = null;

    // Auto-reconnect se non era una chiusura intenzionale
    if (wasConnected && event.code !== 1000 && this.reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
      this._scheduleReconnect();
    }
  }

  _handleError(event) {
    console.error('[WhyCremisi] WebSocket error');
    this._setState(ConnectionState.ERROR);
  }

  _handleMessage(event) {
    let message;
    try {
      message = typeof event.data === 'string' ? JSON.parse(event.data) : event.data;
    } catch (e) {
      console.error('[WhyCremisi] Errore parsing messaggio:', e);
      return;
    }

    const { type, id, payload } = message;

    // Log per debug
    console.log('[WhyCremisi] Ricevuto:', type, id ? `(id=${id})` : '', payload || '');

    // Auto-update bot state based on message type
    if (type === 'ai.prompt' || type === 'daw.request') {
      this.setBotState('thinking');
    } else if (type === 'ai.stream') {
      this.setBotState('typing');
    } else if (type === 'ai.response') {
      this.setBotState('success');
      setTimeout(() => this.setBotState('idle'), 2000);
    } else if (type === 'plugin.error') {
      this.setBotState('error');
      setTimeout(() => this.setBotState('idle'), 3000);
    }

    // Gestisci risposte pendenti
    if (id && this.pendingRequests.has(id)) {
      const pending = this.pendingRequests.get(id);
      clearTimeout(pending.timer);
      pending.callback(payload);
      this.pendingRequests.delete(id);
    }

    // Chiama listeners registrati per tipo
    const listeners = this.messageListeners.get(type);
    if (listeners) {
      listeners.forEach(callback => {
        try {
          callback(payload, message);
        } catch (e) {
          console.error('[WhyCremisi] Errore in listener:', e);
        }
      });
    }

    // Chiama listeners generici (*)
    const anyListeners = this.messageListeners.get('*');
    if (anyListeners) {
      anyListeners.forEach(callback => {
        try {
          callback(message);
        } catch (e) {
          console.error('[WhyCremisi] Errore in listener (*):', e);
        }
      });
    }
  }

  _scheduleReconnect() {
    this._clearReconnectTimer();
    this._setState(ConnectionState.RECONNECTING);
    this.reconnectAttempts++;
    console.log('[WhyCremisi] Retry connessione tra ' + RECONNECT_INTERVAL_MS + 'ms (attempt ' + this.reconnectAttempts + '/' + MAX_RECONNECT_ATTEMPTS + ')');

    this.reconnectTimer = setTimeout(() => {
      this.connect(this.url).catch(() => {
        // Will schedule another reconnect via _handleClose
      });
    }, RECONNECT_INTERVAL_MS);
  }

  _clearReconnectTimer() {
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }
  }

  _clearPendingRequests() {
    this.pendingRequests.forEach(pending => {
      clearTimeout(pending.timer);
      pending.callback({ error: 'Connection closed' });
    });
    this.pendingRequests.clear();
  }
}

// ========================
// React Hook
// ========================

export function useWhyCremisi() {
  const { useState, useEffect, useCallback, useRef } = require('react');

  const [transport, setTransport] = useState({
    isPlaying: false,
    isRecording: false,
    bpm: 120.0,
    positionSeconds: 0.0,
    timeSignature: { numerator: 4, denominator: 4 }
  });

  const [tracks, setTracks] = useState([]);
  const [widgets, setWidgets] = useState([]);
  const [aiStatus, setAiStatus] = useState('idle');
  const [connectionState, setConnectionState] = useState(ConnectionState.DISCONNECTED);

  const bridgeRef = useRef(whycremisi);

  // Auto-connect on mount
  useEffect(() => {
    const bridge = bridgeRef.current;

    // Listen for connection state changes
    const unsubState = bridge.onAny((msg) => {
      // Check if it's a connection-related message
      if (msg.type === 'plugin.init') {
        setConnectionState(ConnectionState.CONNECTED);
      }
    });

    // Connect
    bridge.connect().then(() => {
      setConnectionState(ConnectionState.CONNECTED);
    }).catch(() => {
      setConnectionState(ConnectionState.ERROR);
    });

    // Register listeners
    const unsubTransport = bridge.on('daw.transport', (payload) => {
      setTransport(payload);
    });

    const unsubTrack = bridge.on('daw.track', (payload) => {
      setTracks(prev => {
        const index = prev.findIndex(t => t.trackId === payload.trackId);
        if (index >= 0) {
          const updated = [...prev];
          updated[index] = payload;
          return updated;
        }
        return [...prev, payload];
      });
    });

    const unsubMeter = bridge.on('daw.meter', (payload) => {
      // Could update track meter display
      setTracks(prev => prev.map(t =>
        t.trackId === payload.trackId ? { ...t, meter: payload } : t
      ));
    });

    const unsubWidget = bridge.on('ui.widget.create', (payload) => {
      setWidgets(prev => [...prev, payload]);
    });

    const unsubWidgetUpdate = bridge.on('ui.widget.update', (payload) => {
      setWidgets(prev => prev.map(w =>
        w.widgetId === payload.widgetId ? { ...w, ...payload } : w
      ));
    });

    const unsubWidgetRemove = bridge.on('ui.widget.remove', (payload) => {
      setWidgets(prev => prev.filter(w => w.widgetId !== payload.widgetId));
    });

    const unsubAI = bridge.on('ai.response', () => {
      setAiStatus('complete');
    });

    const unsubAIStream = bridge.on('ai.stream', () => {
      setAiStatus('streaming');
    });

    const unsubError = bridge.on('plugin.error', (payload) => {
      console.error('[WhyCremisi] Plugin error:', payload);
    });

    // Cleanup
    return () => {
      unsubState();
      unsubTransport();
      unsubTrack();
      unsubMeter();
      unsubWidget();
      unsubWidgetUpdate();
      unsubWidgetRemove();
      unsubAI();
      unsubAIStream();
      unsubError();
      bridge.disconnect();
    };
  }, []);

  const sendCommand = useCallback((command, params = {}) => {
    return bridgeRef.current.sendDAWCommand(command, params);
  }, []);

  const askAI = useCallback((prompt, options = {}) => {
    setAiStatus('thinking');
    return bridgeRef.current.sendAIPrompt(prompt, options);
  }, []);

  return {
    transport,
    tracks,
    widgets,
    aiStatus,
    connectionState,
    sendCommand,
    askAI,
    createWidget: bridgeRef.current.createWidget.bind(bridgeRef.current),
    updateWidget: bridgeRef.current.updateWidget.bind(bridgeRef.current),
    removeWidget: bridgeRef.current.removeWidget.bind(bridgeRef.current),
    sendOSC: bridgeRef.current.sendOSC.bind(bridgeRef.current),
    requestDAWInfo: bridgeRef.current.requestDAWInfo.bind(bridgeRef.current),
    connect: () => bridgeRef.current.connect(),
    disconnect: () => bridgeRef.current.disconnect(),
    bridge: bridgeRef.current
  };
}

// ========================
// Export singleton
// ========================

export const whycremisi = new WhyCremisiBridge();
export default whycremisi;