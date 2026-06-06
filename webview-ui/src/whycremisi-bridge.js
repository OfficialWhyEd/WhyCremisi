/**
 * WhyCremisi Bridge - JavaScript Bridge per comunicazione con Plugin VST via WebSocket
 * 
 * Sostituisce il vecchio approccio app://message/ (WebView JUCE)
 * con WebSocket RFC 6455 per connettersi al WebSocketServer nel plugin.
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
const MAX_RECONNECT_ATTEMPTS = 20;
const RECONNECT_BASE_MS = 500;
const RECONNECT_MAX_MS = 30000;

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
 * Calcola intervallo di retry con exponential backoff + jitter
 */
function calculateBackoff(attempt) {
  const exponential = Math.min(RECONNECT_BASE_MS * Math.pow(2, attempt), RECONNECT_MAX_MS);
  const jitter = Math.random() * 1000;
  return Math.round(exponential + jitter);
}

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
    this.messageQueue = [];
    this.healthCheckTimer = null;
    this.healthCheckIntervalMs = 15000;
    this.lastMessageAt = null;
    this.disconnectRequested = false;
    this.transport = 'ws'; // 'ws' | 'juce-ipc' | 'auto'

    this.botState = 'idle';

    // Bind metodi
    this._handleMessage = this._handleMessage.bind(this);
    this._handleOpen = this._handleOpen.bind(this);
    this._handleClose = this._handleClose.bind(this);
    this._handleError = this._handleError.bind(this);
    this._runHealthCheck = this._runHealthCheck.bind(this);
    this._flushQueue = this._flushQueue.bind(this);

    // Set up JUCE IPC bridge immediately so C++ can send messages
    this._setupJuceBridge();
  }

  /**
   * Set up window.__whycremisiBridge for JUCE WebView IPC (always available)
   */
  _setupJuceBridge() {
    if (window.__whycremisiBridge) return;
    const self = this;
    window.__whycremisiBridge = {
      receiveMessage: (jsonString) => {
        try {
          const msg = JSON.parse(jsonString);
          self._handleMessage({ data: jsonString });
        } catch (e) {
          console.error('[WhyCremisi] Errore parsing JUCE IPC:', e);
        }
      },
      sendMessage: (type, payload = {}, options = {}) => {
        return self.sendMessage(type, payload, options);
      },
      isConnected: () => self.state === ConnectionState.CONNECTED
    };
  }

  _onConnected() {
    this.reconnectAttempts = 0;
    this.lastConnectedAt = Date.now();
    this.lastMessageAt = Date.now();
    this._flushQueue();
    this._startHealthCheck();
    this.sendMessage('plugin.init', {
      version: this.version,
      capabilities: ['widgets', 'ai', 'osc', 'daw', 'midi']
    });
  }

  onStateChange(callback) {
    this.stateListeners.push(callback);
    return () => {
      this.stateListeners = this.stateListeners.filter(l => l !== callback);
    };
  }

  _setState(newState) {
    const prev = this.state;
    this.state = newState;
    if (prev !== newState) {
      this.stateListeners.forEach(l => l(newState, prev));
    }
  }

  /**
   * Connetti al WebSocket server del plugin
   * @param {string} url - URL del WebSocket server (default: ws://localhost:8080)
   * @returns {Promise} - Resolves quando connesso
   */
  connect(url = WS_DEFAULT_URL) {
    return new Promise((resolve) => {
      if (this.state === ConnectionState.CONNECTED) {
        resolve();
        return;
      }

      this.disconnectRequested = false;
      this.url = url;

      // Try WebSocket first
      this._setState(ConnectionState.CONNECTING);
      console.log('[WhyCremisi] Connessione via WebSocket a ' + url + '...');

      try {
        this.ws = new WebSocket(url);

        this.ws.onopen = (event) => {
          this.transport = 'ws';
          this._handleOpen(event);
          resolve();
        };

        this.ws.onclose = (event) => {
          this._handleClose(event);
        };

        this.ws.onerror = (event) => {
          console.warn('[WhyCremisi] WebSocket fallito, uso JUCE IPC');
          this.ws = null;
          this.transport = 'juce-ipc';
          this._setState(ConnectionState.CONNECTED);
          this._onConnected();
          resolve();
        };

        this.ws.onmessage = (event) => {
          this._handleMessage(event);
        };

      } catch (e) {
        console.warn('[WhyCremisi] WebSocket eccezione, uso JUCE IPC:', e.message);
        this.transport = 'juce-ipc';
        this._setState(ConnectionState.CONNECTED);
        this._onConnected();
        resolve();
      }
    });
  }

  /**
   * Disconnetti dal WebSocket server
   */
  disconnect() {
    this.disconnectRequested = true;
    this._clearReconnectTimer();
    this._stopHealthCheck();
    this.messageQueue = [];
    this.reconnectAttempts = 0;

    if (this.ws) {
      this.ws.onclose = null;
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
    if (this.state !== ConnectionState.CONNECTED) return false;
    if (this.transport === 'juce-ipc') return true;
    return this.ws && this.ws.readyState === WebSocket.OPEN;
  }

  /**
   * Send message via current transport (WS or JUCE IPC)
   */
  _sendViaTransport(message) {
    if (this.transport === 'juce-ipc') {
      const json = JSON.stringify(message);
      window.location = 'app://message/' + encodeURIComponent(json);
      return;
    }
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(JSON.stringify(message));
    }
  }

  /**
   * Invia messaggio al plugin via WebSocket
   * @param {string} type - Tipo messaggio
   * @param {object} payload - Payload JSON
   * @param {object} options - Opzioni {id, onResponse, timeout}
   * @returns {string} - Message ID (o null se in coda)
   */
  sendMessage(type, payload = {}, options = {}) {
    const id = options.id || generateUUID();
    const message = {
      type,
      id,
      timestamp: Date.now(),
      payload
    };

    if (!this.isConnected()) {
      // Accoda il messaggio per reinvio quando riconnesso
      this.messageQueue.push({ message, options });
      console.log('[WhyCremisi] Accodato (offline):', type);
      if (options.onResponse) {
        options.onResponse({ error: 'Not connected', queued: true });
      }
      return id;
    }

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

    try {
      this._sendViaTransport(message);
      console.log('[WhyCremisi] Inviato:', type, id ? `(id=${id})` : '', `(via ${this.transport})`);
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
      this.messageQueue.push({ message: jsonMessage, options: {} });
      console.log('[WhyCremisi] Accodato (offline):', jsonMessage.type || 'raw');
      return null;
    }
    try {
      this._sendViaTransport(jsonMessage);
      return true;
    } catch (e) {
      console.error('[WhyCremisi] Errore invio raw:', e);
      return false;
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
      stream = true,
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
   * Start MIDI Learn for a widget
   * @param {string} widgetId
   * @param {object} options - {onComplete, onStatus}
   * @returns {string} message ID
   */
  midiLearnStart(widgetId, options = {}) {
    const msgId = generateUUID();

    if (options.onComplete) {
      const unbind = this.on('midi.learn.complete', (payload) => {
        if (payload.widgetId === widgetId) {
          unbind();
          options.onComplete(payload);
        }
      });
    }

    if (options.onStatus) {
      const unbind = this.on('midi.learn.status', (payload) => {
        if (payload.widgetId === widgetId) {
          options.onStatus(payload);
          if (payload.status === 'cancelled' || payload.cc !== undefined) {
            unbind();
          }
        }
      });
    }

    return this.sendMessage('midi.learn.start', { widgetId }, { id: msgId });
  }

  /**
   * Stop MIDI Learn
   */
  midiLearnStop() {
    return this.sendMessage('midi.learn.stop', {});
  }

  /**
   * Get plugin chain (DAW plugin list)
   */
  getPluginChain() {
    return new Promise((resolve, reject) => {
      this.sendMessage('chain.get', {}, {
        onResponse: (response) => {
          if (response.error) reject(new Error(response.error));
          else resolve(response.plugins || []);
        },
        timeout: 3000
      });
    });
  }

  /**
   * Set plugin chain
   */
  setPluginChain(plugins) {
    return this.sendMessage('chain.set', { plugins });
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
   * Gets connection info (health)
   */
  /**
   * Latency tracking (placeholder — real implementation needs server-side timestamps)
   */
  getLatencyMs() {
    return 0
  }

  getAvgLatencyMs() {
    return 0
  }

  getMaxLatencyMs() {
    return 0
  }

  /**
   * Gets connection info (health)
   */
  getConnectionInfo() {
    return {
      state: this.state,
      url: this.url,
      reconnectAttempts: this.reconnectAttempts,
      maxReconnectAttempts: MAX_RECONNECT_ATTEMPTS,
      lastConnectedAt: this.lastConnectedAt,
      lastMessageAt: this.lastMessageAt,
      queueSize: this.messageQueue.length,
      uptime: this.lastConnectedAt ? Date.now() - this.lastConnectedAt : 0
    };
  }

  // ========================
  // Private Methods
  // ========================

  _handleOpen(event) {
    this._setState(ConnectionState.CONNECTED);
    console.log('[WhyCremisi] Connesso a', this.url);

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

    window.receiveFromPlugin = (jsonString) => {
      try {
        const msg = JSON.parse(jsonString);
        this._handleMessage({ data: jsonString });
      } catch (e) {
        console.error('[WhyCremisi] Errore parsing receiveFromPlugin:', e);
      }
    };

    this._onConnected();
  }

  _handleClose(event) {
    const wasConnected = this.state === ConnectionState.CONNECTED;
    this._setState(ConnectionState.DISCONNECTED);
    console.log('[WhyCremisi] Connessione chiusa:', event.code, event.reason);

    this._clearPendingRequests();
    this._stopHealthCheck();
    this.ws = null;

    // Auto-reconnect con backoff — unless intentional disconnect
    if (!this.disconnectRequested && this.reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
      this._scheduleReconnect();
    } else if (this.reconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
      this._setState(ConnectionState.ERROR);
      console.error('[WhyCremisi] Raggiunto massimo tentativi di riconnessione (' + MAX_RECONNECT_ATTEMPTS + ')');
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

    this.lastMessageAt = Date.now();

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

    const delay = calculateBackoff(this.reconnectAttempts - 1);
    console.log('[WhyCremisi] Retry in ' + delay + 'ms (attempt ' + this.reconnectAttempts + '/' + MAX_RECONNECT_ATTEMPTS + ')');

    this.reconnectTimer = setTimeout(() => {
      if (this.disconnectRequested) return;
      this.connect(this.url).catch(() => {
        // Will schedule another reconnect via _handleClose
      });
    }, delay);
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

  _flushQueue() {
    if (this.messageQueue.length === 0) return;
    const queue = [...this.messageQueue];
    this.messageQueue = [];
    console.log('[WhyCremisi] Svuoto coda messaggi:', queue.length, 'messaggi');
    queue.forEach(({ message, options }) => {
      if (options && options.onResponse) {
        this.sendMessage(message.type, message.payload, options);
      } else {
        try {
          this._sendViaTransport(message);
        } catch (e) {
          console.error('[WhyCremisi] Errore reinvio da coda:', e);
        }
      }
    });
  }

  _startHealthCheck() {
    this._stopHealthCheck();
    this.healthCheckTimer = setInterval(() => {
      this._runHealthCheck();
    }, this.healthCheckIntervalMs);
  }

  _stopHealthCheck() {
    if (this.healthCheckTimer) {
      clearInterval(this.healthCheckTimer);
      this.healthCheckTimer = null;
    }
  }

  _runHealthCheck() {
    if (this.state !== ConnectionState.CONNECTED) return;

    const now = Date.now();
    const idleTime = this.lastMessageAt ? now - this.lastMessageAt : 0;

    // Se non riceviamo messaggi da troppo tempo, verifica connessione
    if (idleTime > this.healthCheckIntervalMs * 4 && this.ws) {
      console.log('[WhyCremisi] Health check: idle da ' + (idleTime / 1000).toFixed(0) + 's, invio ping...');
      try {
        this.ws.send(JSON.stringify({ type: 'ping', id: generateUUID(), timestamp: now }));
      } catch (e) {
        console.error('[WhyCremisi] Health check fallito, riconnessione...');
        this._handleClose({ code: 4000, reason: 'Health check failed' });
      }
    }
  }

  /**
   * Resetta il contatore tentativi (utile dopo riconnessione manuale)
   */
  resetReconnectAttempts() {
    this.reconnectAttempts = 0;
  }
}

// ========================
// Export singleton
// ========================

export const whycremisi = new WhyCremisiBridge();
export default whycremisi;

// Esponi globalmente per debug console
window.__whycremisi = whycremisi;
