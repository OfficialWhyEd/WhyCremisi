import React, { useState, useEffect, useMemo, useRef, useCallback } from 'react'
import { motion, AnimatePresence } from 'framer-motion'
import { whycremisi, ConnectionState } from './whycremisi-bridge'
import { BotFace } from './components/BotFace'
import { useValoriLenti } from './components/useValoreLento'
import { MaskLogo } from './components/MaskLogo'
import { SessionPanel } from './components/SessionPanel'
import { SetupScreen } from './components/SetupScreen'
import './index.css'
import { UserBubble, BotBubble } from './components/ChatBubble'

export default function App() {
  // ── setup state ───────────────────────────────────────────────────
  const [setupComplete, setSetupComplete] = useState(() => {
    const done = localStorage.getItem('whycremisi_setup_done') === 'true'
    if (!done) return false
    const saved = localStorage.getItem('whycremisi_config')
    if (!saved) return false
    try {
      const cfg = JSON.parse(saved)
      if (!cfg.provider) return false
      // Con l'abbonamento Claude non c'è nessuna chiave da trovare: pretenderla
      // qui rimandava alla schermata di accesso a ogni avvio. Se manca una
      // credenziale, la parola definitiva è del plugin (ai.status alla
      // connessione), non di questo archivio del browser.
      if (cfg.provider !== 'ollama' && !cfg.apiKey && cfg.claudeAuthMode !== 'subscription') return false
    } catch { return false }
    return true
  })
  const [config, setConfig] = useState(() => {
    const saved = localStorage.getItem('whycremisi_config')
    if (!saved) return { provider: 'ollama', model: 'llama3.2', apiKey: '' }
    try { return JSON.parse(saved) }
    catch { return { provider: 'ollama', model: 'llama3.2', apiKey: '' } }
  })

  // ── connection & bot ──────────────────────────────────────────────
  const [connStatus, setConnStatus] = useState(ConnectionState.DISCONNECTED)
  const [botState, setBotState]     = useState('idle')

  // ── messages ──────────────────────────────────────────────────────
  const [messages, setMessages] = useState([
    { id: 1, type: 'system', text: '[--:--:--] WHYCREMISI ATTIVO' },
  ])
  const chatEndRef  = useRef(null)
  const msgSeq      = useRef(100)        // id messaggi: contatore, mai collisioni (il primo messaggio ha id 1)
  const panelsRef   = useRef([])         // pannelli scelti dal modello (ui.showPanels) in questo turno
  const turnBotRef  = useRef(null)       // id della risposta già chiusa di questo turno
  const chunkBuf    = useRef('')         // token in arrivo, applicati una volta per frame
  const chunkRaf      = useRef(0)
  const lastTransportRef = useRef(null)  // per non scrivere in chat ogni evento transport
  const [inputVal, setInputVal] = useState('')
  const streamingIdRef = useRef(null)
  const lastPromptRef  = useRef('')
  const mounted = useRef(true)
  const intervalsRef = useRef([])
  const throttleRef = useRef(0)
  const fpsRef = useRef(0)
  const [fps, setFps] = useState(0)
  const rafRef = useRef(null)
  useEffect(() => {
    let frames = 0, last = performance.now()
    const tick = () => {
      frames++
      const now = performance.now()
      if (now - last >= 1000) {
        fpsRef.current = Math.round(frames * 1000 / (now - last))
        setFps(fpsRef.current)
        frames = 0
        last = now
      }
      rafRef.current = requestAnimationFrame(tick)
    }
    rafRef.current = requestAnimationFrame(tick)
    return () => {
      if (rafRef.current) cancelAnimationFrame(rafRef.current)
    }
  }, [])
  const MAX_MESSAGES = 200
  useEffect(() => {
    if (messages.length > MAX_MESSAGES) {
      setMessages(prev => prev.slice(-MAX_MESSAGES))
    }
  }, [messages.length])
  const inputRef = useRef(null)
  const searchRef = useRef(null)
  const [personality, setPersonality] = useState({ style: 'analytical' })
  const [personalityStyle, setPersonalityStyle] = useState('analytical')
  const [cmdHistory, setCmdHistory] = useState([])
  const [showStylePicker, setShowStylePicker] = useState(false)
  const [searchOpen, setSearchOpen] = useState(false)
  const [searchQuery, setSearchQuery] = useState('')
  const [historyIdx, setHistoryIdx] = useState(-1)
  const [debugOverlay, setDebugOverlay] = useState(false)
  const [theme, setTheme] = useState(() => localStorage.getItem('whycremisi_theme') || 'dark')
  const [actionHistory, setActionHistory] = useState([])
  const [actionRedoStack, setActionRedoStack] = useState([])
  const [actionLog, setActionLog] = useState([])
  const [toasts, setToasts] = useState([])
  const [pluginChain, setPluginChain] = useState([])
  useEffect(() => {
    document.documentElement.setAttribute('data-theme', theme)
    localStorage.setItem('whycremisi_theme', theme)
  }, [theme])
  const toggleTheme = useCallback(() => {
    setTheme(prev => prev === 'dark' ? 'light' : 'dark')
  }, [])
  const pushAction = (action) => {
    setActionHistory(prev => [...prev, action])
    setActionLog(prev => [action, ...prev].slice(0, 50))
  }
  const addToast = (message, type = 'info') => {
    const id = `${Date.now()}-${Math.random().toString(36).slice(2, 7)}`
    setToasts(prev => prev.some(t => t.message === message) ? prev : [...prev, { id, message, type }].slice(-3))
    setTimeout(() => setToasts(prev => prev.filter(t => t.id !== id)), 4000)
  }
  const removeToast = (id) => setToasts(prev => prev.filter(t => t.id !== id))
  const undoLastAction = () => {
    setActionHistory(prev => {
      if (prev.length === 0) return prev
      setActionRedoStack(r => [prev[prev.length-1], ...r])
      return prev.slice(0, -1)
    })
  }
  const redoLastAction = () => {
    setActionRedoStack(prev => {
      if (prev.length === 0) return prev
      setActionHistory(h => [...h, prev[0]])
      return prev.slice(1)
    })
  }
  const PERSONALITY_STYLES = [
    { id: 'analytical',  label: 'AN', name: 'Analytical',  icon: '📊' },
    { id: 'consultative',label: 'CS', name: 'Consultative',icon: '💬' },
    { id: 'direct',      label: 'DR', name: 'Direct',      icon: '⚡' },
    { id: 'creative',    label: 'CR', name: 'Creative',    icon: '🎨' },
    { id: 'warm',        label: 'WA', name: 'Warm',        icon: '🔥' },
  ]

  // ── transport / DAW state ─────────────────────────────────────────
  const [transport, setTransport] = useState({ isPlaying: false, isRecording: false, bpm: 120.0, position: 0 })
  const [dawConnected, setDawConnected] = useState(false)

  // ── rack / meters ─────────────────────────────────────────────────
  const [gainDb, setGainDb] = useState(0)
  const [driveVal, setDriveVal] = useState(72.5)
  const [meterL, setMeterL] = useState(0)
  const [meterR, setMeterR] = useState(0)
  const [lufs, setLufs]     = useState(-60)
  const [peak, setPeak]     = useState(-60)

  // ── Analyzer / Spectrum ────────────────────────────────────────────
  const [spectrum, setSpectrum] = useState([])
  const [correlation, setCorrelation] = useState(0)
  const [clippingCount, setClipping] = useState(0)
  // Misure vere dall'analyzer, non più stimate lato interfaccia.
  const [rms, setRms] = useState(-60)
  const [lufsShort, setLufsShort] = useState(-60)
  // Nuvola di punti stereo del vectorscope, interleaved x,y.
  const [scopePoints, setScopePoints] = useState([])
  const [gainReduction, setGainReduction] = useState(0)
  // Misure di mix: le calcola l'analizzatore sul suono che sta passando.
  // Prima non arrivavano fin qui e il cruscotto se le inventava.
  const [loudnessRange, setLoudnessRange] = useState(0)
  const [centroid, setCentroid] = useState(0)
  const [rolloff, setRolloff] = useState(0)
  // Energia percentuale delle dieci bande, dai bassi agli acuti.
  const [bandEnergy, setBandEnergy] = useState([])
  // Stato dello stadio stereo, riportato dal plugin dopo ogni comando: è lui
  // a toccare l'audio, quindi è lui a dire com'è messo. Prima ogni pulsante
  // teneva un proprio interruttore e si accendeva comunque.
  const [stereoState, setStereoState] = useState({ mono: false, sideOnly: false, phaseInvert: false, width: 1 })
  // Stato reale della catena interna. Il plugin lo trasmette a ogni
  // cambiamento (dsp.state) e finora nessuno lo ascoltava: il rack mostrava
  // due interruttori senza onClick e un menu "Saturation" per un modulo che
  // nel DSP non esiste.
  const [dspState, setDspState] = useState({ eqBypassed: true, compBypassed: true, limitBypassed: true })
  // C'è davvero segnale? Sotto questa soglia le misure non hanno senso e
  // vanno mostrate come non disponibili invece che come zero — e soprattutto
  // non si deve dare alcun consiglio di mix sul silenzio.
  const sonoro = rms > -55

  // Il modificatore cambia con la piattaforma: ⌘ su Mac, Ctrl altrove.
  const tastoComando = typeof navigator !== 'undefined' && /Mac|iPhone|iPad/.test(navigator.platform || '')
    ? '⌘' : 'Ctrl'

  // Vero quando tutti e tre i moduli della catena sono attivi.
  const tuttoAttivo = !dspState.eqBypassed && !dspState.compBypassed && !dspState.limitBypassed

  // ── Le cifre si leggono, le barre scorrono ────────────────────────
  //
  //  I meter arrivano dal plugin una trentina di volte al secondo. Le barre
  //  devono muoversi a quel ritmo, ma le *cifre* aggiornate trenta volte al
  //  secondo non si leggono: l'occhio non fa in tempo a fissarle e restano
  //  un tremolio continuo. Qui se ne prende un campione sei volte al
  //  secondo, tutte insieme — così la riga si aggiorna in blocco invece di
  //  agitarsi un numero alla volta — e a segnale fermo non si ridisegna.
  //
  //  Le cifre vanno poi scritte con la classe `.num`, che le rende a
  //  larghezza fissa: senza, passare da 1 a 8 sposta il numero di lato.
  const letture = useValoriLenti(
    { meterL, meterR, lufs, peak, rms, correlation, lufsShort, gainReduction },
    { hz: 6, soglia: 0.02 }
  )
  const [cpuUsage, setCpuUsage] = useState({ cpuPercent: 0, peakTimeUs: 0 })
  // Token spesi dall'inizio della sessione, aggiornati a ogni risposta.
  const [tokenUsage, setTokenUsage] = useState({ input: 0, output: 0, cacheRead: 0, cacheWrite: 0, requests: 0, total: 0 })

  // ── MIDI Learn ─────────────────────────────────────────────────────
  const [midiLearnWidget, setMidiLearnWidget] = useState(null)
  const [midiMappings, setMidiMappings] = useState([])
  const [showMapPanel, setShowMapPanel] = useState(false)
  const [sessionOpen, setSessionOpen] = useState(false)

  const [showChainPanel, setShowChainPanel] = useState(false)

  // ── plugin stats (from prepareToPlay via WebSocket) ───────────────
  const [pluginStats, setPluginStats] = useState({ sampleRate: null, bufferSize: null, latencyMs: null })

  // ── active tab ────────────────────────────────────────────────────
  const [activeTab, setActiveTab] = useState('COMMAND')

  // ── active side module ────────────────────────────────────────────
  const [activeMod, setActiveMod] = useState('ai')
  const [boxLayout, setBoxLayout] = useState('inline')

  const sideModules = [
    { id: 'ai',        icon: 'memory',                     label: 'AI' },
    { id: 'transport', icon: 'play_arrow',                  label: 'TRANSPORT' },
    { id: 'comp',      icon: 'settings_input_component',    label: 'COMP' },
    { id: 'limit',     icon: 'linear_scale',                label: 'LIMIT' },
    { id: 'eq',        icon: 'graphic_eq',                  label: 'EQ' },
    { id: 'meters',    icon: 'analytics',                   label: 'METERS' },
    { id: 'actions',   icon: 'history',                     label: 'ACTIONS' }
  ]

  // ── helpers ───────────────────────────────────────────────────────
  const ts = () => new Date().toLocaleTimeString('en-US', { hour12: false })

  const addMsg = useCallback((msg) => {
    setMessages(prev => [...prev, { id: ++msgSeq.current, ...msg }])
  }, [])

  const sysMsg = useCallback((text) => addMsg({ type: 'system', text }), [addMsg])

  // ── Export/Import session ─────────────────────────────────────────
  const exportSession = useCallback(() => {
    const data = {
      version: '1.0',
      exportedAt: new Date().toISOString(),
      messages: messages.map(m => ({ type: m.type, text: m.text, time: m.time })),
      config,
      personality,
      personalityStyle
    }
    const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' })
    const url = URL.createObjectURL(blob)
    const a = document.createElement('a')
    a.href = url
    a.download = `whycremisi-session-${Date.now()}.json`
    a.click()
    URL.revokeObjectURL(url)
    sysMsg(`[${ts()}] Session exported (${messages.length} messages)`)
  }, [messages, config, personality, personalityStyle, sysMsg])

  const importSession = useCallback(() => {
    const input = document.createElement('input')
    input.type = 'file'
    input.accept = '.json'
    input.onchange = (e) => {
      const file = e.target.files[0]
      if (!file) return
      const reader = new FileReader()
      reader.onload = (ev) => {
        try {
          const data = JSON.parse(ev.target.result)
          if (!data.messages || !Array.isArray(data.messages)) {
            sysMsg(`[${ts()}] Invalid session file`)
            return
          }
          const importedMsgs = data.messages.map((m, i) => ({ ...m, id: Date.now() + i + 1 }))
          setMessages(prev => [...prev, { id: Date.now(), type: 'system', text: `[${ts()}] SESSION IMPORTED (${data.messages.length} messages)` }, ...importedMsgs])
          if (data.config) setConfig(data.config)
          if (data.personalityStyle) setPersonalityStyle(data.personalityStyle)
          sysMsg(`[${ts()}] Imported ${data.messages.length} messages from session file`)
        } catch (err) {
          sysMsg(`[${ts()}] Import failed: ${err.message}`)
        }
      }
      reader.readAsText(file)
    }
    input.click()
  }, [sysMsg])

  // Auto-scroll solo se l'utente è già in fondo; istantaneo, non smooth:
  // lo smooth ad ogni chunk faceva "nuotare" il testo durante lo streaming.
  const listRef   = useRef(null)
  const stickRef  = useRef(true)
  const onListScroll = useCallback(() => {
    const el = listRef.current
    if (el) stickRef.current = el.scrollHeight - el.scrollTop - el.clientHeight < 80
  }, [])
  useEffect(() => {
    if (!stickRef.current) return
    const el = listRef.current
    if (el) el.scrollTop = el.scrollHeight
  }, [messages])

  // ── bridge init ───────────────────────────────────────────────────
  useEffect(() => {
    const stateUnsub = whycremisi.onStateChange((s) => {
      setConnStatus(s)
      if (s === ConnectionState.CONNECTED) {
        setBotState('success')
        setTimeout(() => { if (mounted.current) setBotState('idle') }, 2000)
        // Chi comanda sull'accesso è il plugin, non il browser.
        //
        // Prima qui si riversava sul plugin la copia tenuta in localStorage a
        // ogni connessione. Quella copia poteva essere vecchia, e siccome
        // impostare una chiave API azzera l'abbonamento, un apiKey rimasto
        // lì dentro cancellava in silenzio il collegamento a Claude appena
        // fatto. Ora si chiede al plugin come sta messo, e si manda qualcosa
        // solo se non ha già una credenziale sua.
        const unsubStato = whycremisi.on('config.response', (payload) => {
          if (payload?.key !== 'ai.status') return
          unsubStato()

          if (payload.ready) {
            // Il plugin è già a posto: si allinea la UI a lui e non si
            // richiede l'accesso. Prima la schermata di configurazione
            // ricompariva a ogni avvio a chi entra con l'abbonamento Claude,
            // perché il controllo pretendeva una chiave API in localStorage e
            // con l'abbonamento la chiave non esiste.
            setConfig(prev => ({
              ...prev,
              provider: payload.provider || prev?.provider,
              model:    payload.model    || prev?.model,
              effort:   payload.effort   || prev?.effort,
              claudeAuthMode: payload.hasSubscription ? 'subscription' : 'apikey',
            }))
            setSetupComplete(true)
            return
          }

          const cfg = (() => {
            try { return JSON.parse(localStorage.getItem('whycremisi_config')) } catch { return null }
          })()
          if (!cfg?.provider) return

          // Il provider per primo: sceglierlo può correggere il modello, e
          // farlo dopo la chiave rischierebbe di lasciare accoppiati un
          // fornitore e un modello che non gli appartiene.
          whycremisi.send({ type: 'config.set', payload: { key: 'ai.provider', value: cfg.provider } })
          if (cfg.model)
            whycremisi.send({ type: 'config.set', payload: { key: 'ai.model', value: cfg.model } })
          if (cfg.apiKey)
            whycremisi.send({ type: 'config.set', payload: { key: 'ai.apiKey', value: cfg.apiKey, provider: cfg.provider } })
        })
        whycremisi.send({ type: 'config.get', payload: { key: 'ai.status' } })
      }
      if (s === ConnectionState.DISCONNECTED || s === ConnectionState.ERROR) {
        setBotState('sad')
        addToast('Connessione persa — riconnessione in corso…', 'error')
      }
      if (s === ConnectionState.RECONNECTING) {
        setBotState('loading')
      }
    })

    const onBotState = (e) => setBotState(e.detail)
    window.addEventListener('whycremisi-botstate', onBotState)

    // ── AI response (complete) ──
    // I token si accumulano in chunkBuf e arrivano in React una volta per
    // frame: un setState per token faceva ridisegnare la chat centinaia di volte.
    const boxDelTurno = () => [...new Set([...detectBoxType(lastPromptRef.current), ...panelsRef.current])]
    const flushChunks = () => {
      chunkRaf.current = 0
      const add = chunkBuf.current
      if (!add) return
      chunkBuf.current = ''
      const sid = streamingIdRef.current
      if (!sid) return
      setMessages(prev => prev.map(m => m.id === sid ? { ...m, text: m.text + add } : m))
    }
    const flushNow = () => {
      if (chunkRaf.current) { cancelAnimationFrame(chunkRaf.current); chunkRaf.current = 0 }
      flushChunks()
    }

    const unsubAI = whycremisi.on('ai.response', (payload) => {
      setBotState('success')
      setTimeout(() => { if (mounted.current) setBotState('idle') }, 2000)
      chunkBuf.current = ''
      if (chunkRaf.current) { cancelAnimationFrame(chunkRaf.current); chunkRaf.current = 0 }
      const sid = streamingIdRef.current
      streamingIdRef.current = null
      if (payload?.content) {
        const boxType = boxDelTurno()
        turnBotRef.current = sid
        if (sid) {
          // finalize existing streaming message instead of duplicating
          setMessages(prev => prev.map(m =>
            m.id === sid ? { ...m, text: payload.content, streaming: false, boxType } : m
          ))
        } else {
          const nid = ++msgSeq.current
          turnBotRef.current = nid
          setMessages(prev => [...prev, { id: nid, type: 'bot', text: payload.content, time: ts(), boxType }])
        }
      }
    })

    // ── AI stream chunks ──
    const unsubStream = whycremisi.on('ai.stream', (payload) => {
      setBotState('typing')
      const chunk = payload?.chunk ?? payload?.content ?? ''

      if (chunk) {
        if (!streamingIdRef.current) {
          // start new streaming message
          const newId = ++msgSeq.current
          streamingIdRef.current = newId
          setMessages(prev => [...prev, { id: newId, type: 'bot', text: chunk, time: ts(), streaming: true }])
        } else {
          chunkBuf.current += chunk
          if (!chunkRaf.current) chunkRaf.current = requestAnimationFrame(flushChunks)
        }
      }

      // finalize if isDone
      if (payload?.isDone) {
        flushNow()
        const sid = streamingIdRef.current
        if (sid) {
          const boxType = boxDelTurno()
          turnBotRef.current = sid
          setMessages(prev => prev.map(m =>
            m.id === sid ? { ...m, streaming: false, boxType } : m
          ))
        }
        streamingIdRef.current = null
        setBotState('success')
        setTimeout(() => { if (mounted.current) setBotState('idle') }, 2000)
      }
    })

    // ── Pannelli scelti dal modello ──
    // Il modello chiama ui.showPanels solo quando un grafico serve davvero.
    // Se la risposta è già chiusa i pannelli si aggiungono lì, altrimenti
    // restano in attesa e si agganciano alla chiusura.
    const unsubPanels = whycremisi.on('ai.panels', (payload) => {
      const nuovi = Array.isArray(payload?.panels) ? payload.panels.filter(p => typeof p === 'string') : []
      if (nuovi.length === 0) return
      panelsRef.current = [...new Set([...panelsRef.current, ...nuovi])]
      const tid = turnBotRef.current
      if (tid) setMessages(prev => prev.map(m =>
        m.id === tid ? { ...m, boxType: [...new Set([...(m.boxType || []), ...nuovi])] } : m))
    })

    // ── DAW transport ──
    const unsubTransport = whycremisi.on('daw.transport', (payload) => {
      setTransport({
        isPlaying:   payload.isPlaying  ?? false,
        isRecording: payload.isRecording ?? false,
        bpm:         payload.bpm        ?? 120,
        position:    payload.positionSeconds ?? 0
      })
      setDawConnected(true)
      const statoTransport = `${!!payload.isPlaying}|${!!payload.isRecording}`
      if (lastTransportRef.current !== null && lastTransportRef.current !== statoTransport)
        sysMsg(`[${ts()}] ${payload.isPlaying ? '▶ PLAY' : '■ STOP'} | BPM ${payload.bpm?.toFixed(1) ?? '--'}`)
      lastTransportRef.current = statoTransport
    })

    // ── DAW meters ──
    const unsubMeter = whycremisi.on('daw.meter', (payload) => {
      if (payload.trackId === -1 || payload.trackId === undefined) {
        // master meters
        const lDb = payload.leftDb  ?? -20
        const rDb = payload.rightDb ?? -20
        setMeterL(Math.max(0, Math.min(1, (lDb + 60) / 72)))
        setMeterR(Math.max(0, Math.min(1, (rDb + 60) / 72)))
        setLufs(lDb)
        setPeak(Math.max(lDb, rDb))
      }
    })

    // ── OSC messages ──
    const unsubOSC = whycremisi.on('osc.message', (payload) => {
      // un messaggio OSC per riga riempiva la chat: restano nei log, non qui
    })

    // ── plugin stats (SR, buffer, latency from prepareToPlay) ──
    const unsubStats = whycremisi.on('plugin.stats', (payload) => {
      setPluginStats({
        sampleRate: payload.sampleRate ?? null,
        bufferSize: payload.bufferSize ?? null,
        latencyMs:  payload.latencyMs  ?? null,
      })
    })

    // ── plugin errors ──
    const unsubErr = whycremisi.on('plugin.error', (payload) => {
      setBotState('error')
      setTimeout(() => { if (mounted.current) setBotState('idle') }, 3000)
      sysMsg(`[${ts()}] [ERROR] ${payload.message || payload.code || 'Unknown error'}`)
      addToast(payload.message || payload.code || 'Unknown plugin error', 'error')
    })

    // MIDI Learn: aggiorna stato UI
    const unsubLearnStatus = whycremisi.on('midi.learn.status', (payload) => {
      if (payload.status === 'listening') {
        setMidiLearnWidget(payload.widgetId)
      } else if (payload.status === 'cancelled') {
        setMidiLearnWidget(null)
      }
    })

    const unsubLearnComplete = whycremisi.on('midi.learn.complete', (payload) => {
      setMidiLearnWidget(null)
      setMidiMappings(prev => {
        const filtered = prev.filter(m => m.widgetId !== payload.widgetId)
        return [...filtered, { widgetId: payload.widgetId, cc: payload.cc, channel: payload.channel }]
      })
      sysMsg(`[${ts()}] MIDI LEARN: ${payload.widgetId} → CC#${payload.cc} Ch.${payload.channel}`)
    })

    // Plugin Chain response
    const unsubChain = whycremisi.on('chain.response', (payload) => {
      if (payload.plugins) setPluginChain(payload.plugins)
    })

    // Request chain on connect
    if (whycremisi.isConnected()) {
      whycremisi.getPluginChain().then(setPluginChain).catch(() => {})
    }

    // Analyzer data (spectrum, correlation, loudness)
    const unsubAnalyzer = whycremisi.on('daw.analyzer', (payload) => {
      if (payload.spectrum) setSpectrum(payload.spectrum)
      if (payload.correlation !== undefined) setCorrelation(payload.correlation)
      if (payload.truePeak !== undefined) setPeak(payload.truePeak)
      if (payload.clippingCount !== undefined) setClipping(payload.clippingCount)
      // Misure che il plugin calcolava già ma che la UI buttava via:
      // senza queste i pannelli mostravano valori ricavati per stima.
      if (payload.rms !== undefined) setRms(payload.rms)
      if (payload.loudnessShortTerm !== undefined) setLufsShort(payload.loudnessShortTerm)
      if (payload.loudnessIntegrated !== undefined) setLufs(payload.loudnessIntegrated)
      if (payload.scope) setScopePoints(payload.scope)
      if (payload.gainReduction !== undefined) setGainReduction(payload.gainReduction)
      // Misure di mix vere, al posto delle formule inventate che stavano
      // nel cruscotto: gamma di loudness, baricentro dello spettro, rolloff
      // e l'energia percentuale delle dieci bande.
      if (payload.loudnessRange !== undefined) setLoudnessRange(payload.loudnessRange)
      if (payload.spectralCentroid !== undefined) setCentroid(payload.spectralCentroid)
      if (payload.spectralRolloff !== undefined) setRolloff(payload.spectralRolloff)
      if (payload.bandEnergyPercent) setBandEnergy(payload.bandEnergyPercent)
    })

    // Stato dello stadio stereo dopo ogni comando: mono, solo lati,
    // inversione di fase, ampiezza.
    const unsubStereo = whycremisi.on('dsp.stereo', (payload) => {
      if (payload) setStereoState(payload)
    })

    // Stato della catena interna: quali moduli sono attivi davvero.
    const unsubDsp = whycremisi.on('dsp.state', (payload) => {
      if (payload) setDspState(prev => ({ ...prev, ...payload }))
    })

    // CPU usage
    const unsubCpu = whycremisi.on('plugin.cpu', (payload) => {
      setCpuUsage(payload)
    })

    // Consumo di token: arriva dopo ogni risposta dell'AI, con il costo
    // della singola richiesta e il totale accumulato nella sessione.
    const unsubUsage = whycremisi.on('ai.usage', (payload) => {
      if (payload?.session) setTokenUsage(payload.session)
    })

    // AI Action log
    const unsubActionLog = whycremisi.on('ai.action.log', (payload) => {
      if (payload.widgetId) {
        pushAction({
          widgetId: payload.widgetId,
          value: payload.value,
          previousValue: payload.previousValue,
          description: payload.description || '',
          timestamp: Date.now()
        })
      }
    })

    // ── Personality events from PersonalityCore ──
    const unsubPersAction = whycremisi.on('personality.action', () => {
    })

    const unsubPersContext = whycremisi.on('personality.context', (payload) => {
      if (payload) {
        setPersonality(prev => ({
          ...prev,
          style: payload.style || prev.style,
          confidence: payload.confidence ?? prev.confidence,
          experienceLevel: payload.experienceLevel ?? prev.experienceLevel,
          userName: payload.userName || prev.userName,
          sessionCount: payload.sessionCount ?? prev.sessionCount,
          totalActions: payload.totalActions ?? prev.totalActions,
          recentActions: payload.recentActions || prev.recentActions,
          description: payload.description || prev.description
        }))
      }
    })

    // ── Workspace events from AgentWorkspace ──
    const unsubWsStyle = whycremisi.on('workspace.style', (payload) => {
      setPersonality(prev => ({
        ...prev,
        style: payload.style || payload.detail?.includes('analytical') ? 'analytical' :
               payload.detail?.includes('direct') ? 'direct' :
               payload.detail?.includes('consultative') ? 'consultative' :
               payload.detail?.includes('creative') ? 'creative' : prev.style
      }))
    })

    const unsubWsMemory = whycremisi.on('workspace.memory', () => {
    })

    const unsubWsBootstrap = whycremisi.on('workspace.bootstrap', () => {
    })

    const unsubWsRefresh = whycremisi.on('workspace.refresh', () => {
    })

    // connect
    whycremisi.connect().catch(() => {
    })

    return () => {
      mounted.current = false
      intervalsRef.current.forEach(clearInterval)
      intervalsRef.current = []
      window.removeEventListener('whycremisi-botstate', onBotState)
      stateUnsub(); unsubAI(); unsubStream(); unsubPanels(); unsubTransport()
      unsubMeter(); unsubOSC(); unsubStats(); unsubErr()
      unsubLearnStatus(); unsubLearnComplete(); unsubChain(); unsubAnalyzer(); unsubStereo(); unsubDsp(); unsubCpu(); unsubUsage(); unsubActionLog(); unsubPersAction(); unsubPersContext()
      unsubWsStyle(); unsubWsMemory(); unsubWsBootstrap(); unsubWsRefresh()
      whycremisi.disconnect()
    }
  }, [addMsg, sysMsg])

  // ── last bot message id ───────────────────────────────────────────
  const lastBotId = useMemo(() => {
    const bots = messages.filter(m => m.type === 'bot')
    return bots.length > 0 ? bots[bots.length - 1].id : null
  }, [messages])


  // ── send AI prompt ────────────────────────────────────────────────
  const handleCommand = (e) => {
    if (e.key !== 'Enter' || !inputVal.trim() || botState === 'thinking' || botState === 'typing') return
    const cmd = inputVal.trim()
    const t = ts()
    lastPromptRef.current = cmd
    panelsRef.current = []
    turnBotRef.current = null
    stickRef.current = true
    addMsg({ type: 'user', text: cmd, time: t })

    // Store in history, reset index
    setCmdHistory(prev => [cmd, ...prev].slice(0, 50))
    setHistoryIdx(-1)

    setInputVal('')
    setBotState('thinking')

    if (whycremisi.isConnected()) {
      const ctx = `L=${(meterL*100).toFixed(0)}% R=${(meterR*100).toFixed(0)}% LUFS=${lufs.toFixed(1)} PEAK=${peak.toFixed(1)} CORR=${correlation.toFixed(3)} BPM=${transport.bpm.toFixed(1)}`
      whycremisi.sendAIPrompt(cmd, { context: { meters: ctx } })
    } else {
      // offline fallback — simulate response
      setTimeout(() => {
        if (!mounted.current) return
        const offlineId = Date.now()
        streamingIdRef.current = offlineId
        setMessages(prev => [...prev, { id: offlineId, type: 'bot', text: '', time: ts(), streaming: true }])
        setBotState('typing')
        const reply = '[OFFLINE] Plugin not connected. Connect WhyCremisi VST to enable real AI responses.'
        let i = 0
        const ticker = setInterval(() => {
          if (!mounted.current) { clearInterval(ticker); return }
          i++
          setMessages(prev => prev.map(m => m.id === offlineId ? { ...m, text: reply.slice(0, i) } : m))
          if (i >= reply.length) {
            clearInterval(ticker)
            setMessages(prev => prev.map(m => m.id === offlineId ? { ...m, streaming: false } : m))
            streamingIdRef.current = null
            setBotState('idle')
          }
        }, 18)
        intervalsRef.current.push(ticker)
      }, 600)
    }
  }

  // ── keyboard shortcuts ─────────────────────────────────────────────
  const handleKeyDown = useCallback((e) => {
    const isInputFocused = document.activeElement === inputRef.current

    // Cmd+F: toggle message search
    if ((e.metaKey || e.ctrlKey) && e.key === 'f') {
      e.preventDefault()
      setSearchOpen(prev => {
        if (!prev) setTimeout(() => searchRef.current?.focus(), 50)
        return !prev
      })
      if (searchOpen) setSearchQuery('')
      return
    }

    // Cmd+K: clear messages (globale)
    if ((e.metaKey || e.ctrlKey) && e.key === 'k') {
      e.preventDefault()
      setMessages([
        { id: 1, type: 'system', text: `[${ts()}] BOARD CLEARED` },
      ])
      return
    }

    // Cmd+Shift+D: toggle debug overlay
    if ((e.metaKey || e.ctrlKey) && e.shiftKey && e.key === 'D') {
      e.preventDefault()
      setDebugOverlay(prev => !prev)
      return
    }

    // Escape: clear input or close modals
    if (e.key === 'Escape') {
      if (isInputFocused && inputVal) {
        setInputVal('')
        e.preventDefault()
        return
      }
      if (showStylePicker) { setShowStylePicker(false); e.preventDefault(); return }
      if (showChainPanel) { setShowChainPanel(false); e.preventDefault(); return }
      if (showMapPanel) { setShowMapPanel(false); e.preventDefault(); return }
      if (sessionOpen) { setSessionOpen(false); e.preventDefault(); return }
      return
    }

    // / (slash): focus input (globale)
    if (e.key === '/' && !isInputFocused && !e.metaKey && !e.ctrlKey && !e.altKey) {
      e.preventDefault()
      inputRef.current?.focus()
      return
    }

    // Up/Down: command history (solo quando input è focalizzato)
    if (isInputFocused && (e.key === 'ArrowUp' || e.key === 'ArrowDown')) {
      e.preventDefault()
      if (cmdHistory.length === 0) return
      setHistoryIdx(prev => {
        const dir = e.key === 'ArrowUp' ? -1 : 1
        let next = prev + dir
        if (next < -1) next = -1
        if (next >= cmdHistory.length) next = cmdHistory.length - 1
        setInputVal(next === -1 ? '' : cmdHistory[next])
        return next
      })
      return
    }

    // Cmd+Enter: send (quando input NON è focalizzato — es. textarea future)
    if ((e.metaKey || e.ctrlKey) && e.key === 'Enter') {
      if (inputVal.trim()) {
        e.preventDefault()
        handleCommand({ key: 'Enter' })
      }
      return
    }
  }, [inputVal, cmdHistory, showStylePicker, showChainPanel, showMapPanel, sessionOpen, handleCommand, searchOpen])

  // ── global keyboard shortcuts (must be after handleKeyDown) ────────
  useEffect(() => {
    window.addEventListener('keydown', handleKeyDown)
    return () => window.removeEventListener('keydown', handleKeyDown)
  }, [handleKeyDown])

  // ── DAW transport commands ────────────────────────────────────────
  const dawCmd = useCallback((action, params = {}) => {
    if (whycremisi.isConnected()) {
      whycremisi.sendDAWCommand(action, params)
    } else {
      sysMsg(`[${ts()}] DAW CMD ${action} — not connected`)
    }
  }, [sysMsg])

  const analyzeFurther = useCallback((style) => {
    if (botState !== 'idle') return
    const prompt = style === 'direct' ? 'Analyze low-end buildup and suggest aggressive EQ cuts in 200-400Hz range'
      : style === 'consultative' ? 'Examine the low-mid range carefully for potential harmonic crowding, suggest gentle corrections'
      : style === 'analytical' ? 'Run detailed spectral analysis on 200-400Hz range, identify specific frequency clusters and recommend targeted EQ'
      : style === 'creative' ? 'Explore creative EQ sculpting in the low-mid range to reveal hidden texture and movement'
      : 'Analyze the low end further and suggest mix improvements'
    setBotState('thinking')
    if (whycremisi.isConnected()) {
      whycremisi.sendAIPrompt(prompt)
    }
  }, [botState])

  const applyPersonalityStyle = useCallback((styleId) => {
    setPersonalityStyle(styleId)
    setShowStylePicker(false)
    if (whycremisi.isConnected())
      whycremisi.send({ type: 'ai.personalityStyle', payload: { style: styleId } })
  }, [setPersonalityStyle, setShowStylePicker])


    // ── Box nella chat: SOLO se l'utente li chiede ────────────────────
    //  Prima il tipo di box si ricavava con regex sul testo della risposta
    //  (basta la parola "level" o "should") e quasi ogni risposta si
    //  riempiva di box con le misure live. Ora una risposta è testo
    //  normale; un box compare solo con "show/mostra …" nel prompt.
    const detectBoxType = (prompt = '') => {
      const t = prompt.toLowerCase()
      const m = t.match(/(?:show|mostra(?:mi)?|fammi\s+vedere)\s+(?:me\s+|the\s+|il\s+|lo\s+|la\s+|i\s+|le\s+)*(.+)/)
      if (!m) return []
      const raw = m[1]
      if (/\b(all|everything|every|tutto|tutti)\b/.test(raw))
        return ['vectorscope','stereo','loudness','clipping','eq','spectral','slider','knob','transport','compressor','metrics']
      const explicit = []
      for (const w of raw.split(/[\s,&]+/)) {
        if (/^(eq|equali|frequen|bass|sub|mid|high|treble|presence|air)/.test(w)) explicit.push('eq')
        else if (/^(spectrum|spectral|spettro|fft|analyzer)/.test(w)) explicit.push('spectral')
        else if (/^(clip|distort|overload)/.test(w)) explicit.push('clipping')
        else if (/^(vector|scope|xy)/.test(w)) explicit.push('vectorscope')
        else if (/^(stereo|width|side|balance|correlation)/.test(w)) explicit.push('stereo')
        else if (/^(loud|lufs|peak|rms|dynamic|crest)/.test(w)) explicit.push('loudness')
        else if (/^(volume|gain|fader|level|livell)/.test(w)) explicit.push('slider')
        else if (/^(pan|panning|position)/.test(w)) explicit.push('knob')
        else if (/^(play|stop|record|transport|bpm|tempo)/.test(w)) explicit.push('transport')
        else if (/^(compres|ratio|attack|release|threshold|knee)/.test(w)) explicit.push('compressor')
        else if (/^(metrics|misure|meter)/.test(w)) explicit.push('metrics')
      }
      return [...new Set(explicit)]
    }

  // ── Suggerimento di correzione, da misure vere ────────────────────
  //
  //  Com'era prima, e perché non andava bene su uno strumento per mixare:
  //
  //  - senza audio restituiva comunque un consiglio preciso — taglia
  //    2,4 dB fra 200 e 400 Hz — dichiarando il 98% di confidenza. A
  //    plugin appena aperto, in silenzio, diceva già cosa correggere.
  //  - i "transienti %" li ricavava dalla correlazione, che misura la
  //    fase fra i canali e non dice nulla sui transienti.
  //  - quanti dB tagliare dipendeva dallo stile della personalità del
  //    chatbot: "diretto" consigliava 1,4 volte il taglio di
  //    "consultivo" sullo stesso identico audio.
  //  - la confidenza era l'ampiezza media moltiplicata per 1,5.
  //
  //  Ora: se non c'è segnale non c'è consiglio, le bande sono quelle
  //  misurate davvero, la cresta sostituisce i finti transienti (è la
  //  misura che indica quanto sono vivi), e la personalità cambia il
  //  modo di dirlo, non il numero di dB.
  const currentSuggestion = useMemo(() => {
    if (!sonoro || bandEnergy.length < 10) return null

    const b = bandEnergy
    const somma = (...idx) => idx.reduce((t, i) => t + (b[i] || 0), 0)

    // Ripartizione di riferimento per un mix equilibrato, sulle stesse
    // dieci bande logaritmiche che misura l'analizzatore.
    const zone = [
      { label: '20Hz–40Hz',    freqLow: 20,   freqHigh: 40,    misurato: somma(0),    atteso: 4,  nome: 'accumulo di sub' },
      { label: '40Hz–160Hz',   freqLow: 40,   freqHigh: 160,   misurato: somma(1, 2), atteso: 26, nome: 'bassi in eccesso' },
      { label: '160Hz–630Hz',  freqLow: 160,  freqHigh: 630,   misurato: somma(3, 4), atteso: 30, nome: 'medio-bassi affollati' },
      { label: '630Hz–2.5kHz', freqLow: 630,  freqHigh: 2500,  misurato: somma(5, 6), atteso: 24, nome: 'medi in avanti' },
      { label: '2.5kHz–5kHz',  freqLow: 2500, freqHigh: 5000,  misurato: somma(7),    atteso: 9,  nome: 'presenza aggressiva' },
      { label: '5kHz–20kHz',   freqLow: 5000, freqHigh: 20000, misurato: somma(8, 9), atteso: 7,  nome: 'alti in eccesso' },
    ]

    // La zona che si scosta di più dal riferimento, in proporzione.
    let peggiore = null
    for (const z of zone) {
      const scarto = (z.misurato - z.atteso) / z.atteso
      if (scarto > 0.35 && (!peggiore || scarto > peggiore.scarto))
        peggiore = { ...z, scarto }
    }

    // Niente si scosta abbastanza: meglio non dire niente che inventare
    // un intervento su un mix che sta già a posto.
    if (!peggiore) return null

    // Il taglio segue lo scarto misurato, con un tetto: nessun consiglio
    // automatico dovrebbe proporre più di 4 dB su una banda larga.
    const gainDb = -Math.min(4, Math.round(peggiore.scarto * 4 * 10) / 10)

    // La confidenza dice quanto è netto lo scostamento, non quanto è
    // forte il segnale.
    const confidence = Math.min(0.95, 0.4 + peggiore.scarto * 0.5)

    let description = peggiore.nome
    if (correlation < 0) description += ', e la fase collassa in mono'
    else if (correlation < 0.3) description += ', con correlazione bassa'

    return {
      freqLow: peggiore.freqLow,
      freqHigh: peggiore.freqHigh,
      gainDb,
      label: peggiore.label,
      description,
      // Fattore di cresta: true peak meno RMS. È questa la misura che dice
      // quanto sono vivi i transienti, non la correlazione.
      crestDb: (peak > -60 && rms > -60) ? Math.round((peak - rms) * 10) / 10 : null,
      confidence: Math.round(confidence * 1000) / 1000,
    }
  }, [bandEnergy, correlation, peak, rms, sonoro])

  // ── gain slider pct ───────────────────────────────────────────────
  const gainPct = Math.max(0, Math.min(100, ((gainDb + 60) / 72) * 100))

  // ── misure derivate, tutte da dati veri ───────────────────────────
  //
  //  Il cruscotto si scriveva da solo i numeri partendo dal livello dei
  //  meter: correlazione "0,62 più un po' di livello", energia in banda
  //  bassa "livello per 0,4 più 0,3", true peak "picco più 0,3". Erano
  //  valori plausibili a vedersi e senza alcun rapporto con il suono. Su
  //  uno strumento per mixare è peggio di un pannello vuoto: porta a
  //  decisioni sbagliate con la fiducia di chi ha letto una misura.
  //
  //  Quello che segue viene tutto dall'analizzatore del plugin.

  // Larghezza stereo dalla nuvola di punti del vectorscope, che arriva già
  // ruotata: x = side, y = mid. È il rapporto fra le due energie, cioè
  // quanto del segnale sta fuori dal centro.
  const stereoWidth = useMemo(() => {
    if (!scopePoints || scopePoints.length < 4) return 0
    let side = 0, mid = 0
    for (let i = 0; i + 1 < scopePoints.length; i += 2) {
      side += scopePoints[i] * scopePoints[i]
      mid  += scopePoints[i+1] * scopePoints[i+1]
    }
    const tot = side + mid
    return tot > 1e-9 ? Math.min(1, side / tot * 2) : 0
  }, [scopePoints])

  // Le dieci bande sono logaritmiche da 20 Hz a 20 kHz, una per ottava:
  // b0 20–40, b1 40–80, b2 80–160, b3 160–320, b4 320–630, b5 630–1,3k,
  // b6 1,3k–2,5k, b7 2,5k–5k, b8 5k–10k, b9 10k–20k. Qui si raggruppano
  // per come si parla di un mix, sommando le bande che le competono.
  const bandeSpettro = useMemo(() => {
    const b = bandEnergy
    const somma = (...idx) => idx.reduce((t, i) => t + (b[i] || 0), 0)
    const disponibile = b.length >= 10 && sonoro

    const voci = [
      { label: 'Sub (20–40Hz)',      pct: somma(0),    color: '#9B59B6' },
      { label: 'Bassi (40–160Hz)',   pct: somma(1, 2), color: '#DC143C' },
      { label: 'Medio-bassi (160–630)', pct: somma(3, 4), color: '#FF6B35' },
      { label: 'Medi (630Hz–2.5k)',  pct: somma(5, 6), color: '#FFB000' },
      { label: 'Presenza (2.5–5k)',  pct: somma(7),    color: '#00FFaa' },
      { label: 'Aria (5k–20k)',      pct: somma(8, 9), color: '#00E5FF' },
    ]

    return voci.map(v => ({
      ...v,
      val: disponibile ? `${v.pct.toFixed(1)}%` : '—',
      // La scala del riempimento è tarata sul massimo fra le voci: con la
      // percentuale grezza le bande alte resterebbero sempre schiacciate
      // a filo di zero e non si leggerebbe niente.
      pct: disponibile ? Math.min(100, v.pct * 3) : 0,
    }))
  }, [bandEnergy, sonoro])

  // ── meter bars ────────────────────────────────────────────────────
  const meterBars = Array.from({ length: 12 }, (_, i) => ({
    color: i < 8 ? '#FFB000' : '#DC143C',
    active: i < Math.round((meterL + meterR) / 2 * 12)
  }))

  // ── render ────────────────────────────────────────────────────────
  // Misure live per i box: servono solo ai messaggi che mostrano un box
  const boxProps = { meterL, meterR, lufs, peak, transport, pluginStats, gainDb, driveVal, correlation, spectrum, onDawCmd: dawCmd, personality, onAnalyzeFurther: analyzeFurther, suggestion: currentSuggestion, layout: boxLayout, clippingCount, cpuUsage, rms, lufsShort, scopePoints, gainReduction, stereoState }

  return (
    <div className="select-none h-screen w-screen overflow-hidden font-['Space_Grotesk']" style={{ backgroundColor: 'var(--bg-primary)', color: 'var(--text-primary)' }}>
      <div className="crt-overlay" />

      {/* ── Toast Container ────────────────────────────────────────── */}
      <div className="fixed bottom-10 right-3 z-[200] flex flex-col gap-2 items-end pointer-events-none">
        <AnimatePresence>
          {toasts.map(toast => (
            <motion.div
              key={toast.id}
              initial={{ opacity: 0, x: 80, scale: 0.9 }}
              animate={{ opacity: 1, x: 0, scale: 1 }}
              exit={{ opacity: 0, x: 80, scale: 0.9 }}
              transition={{ type: 'spring', stiffness: 500, damping: 30 }}
              className="pointer-events-auto cursor-pointer flex items-center gap-2 px-3 py-2 border text-[11px] font-mono" style={{ backgroundColor: 'var(--bg-elevated)', color: 'var(--text-primary)', borderColor: toast.type === 'error' ? 'var(--crimson, #DC143C)' : toast.type === 'success' ? '#22c55e' : 'var(--border-light)' }}
              onClick={() => removeToast(toast.id)}
            >
              <span className="text-xs">
                {toast.type === 'error' && '✕'}
                {toast.type === 'success' && '✓'}
                {toast.type === 'warning' && '⚠'}
                {toast.type === 'info' && 'ℹ'}
              </span>
              <span>{toast.message}</span>
            </motion.div>
          ))}
        </AnimatePresence>
      </div>

      <AnimatePresence>
        {!setupComplete && (
          <SetupScreen 
            initialConfig={config}
            onComplete={(newConfig) => {
              setConfig(newConfig)
              setSetupComplete(true)
              localStorage.setItem('whycremisi_setup_done', 'true')
              localStorage.setItem('whycremisi_config', JSON.stringify(newConfig))
              
              // Always send API key + config to plugin (critical for AI to work)
              if (whycremisi.isConnected()) {
                whycremisi.send({ type: 'config.set', payload: { key: 'ai.provider', value: newConfig.provider } })
                whycremisi.send({ type: 'config.set', payload: { key: 'ai.model', value: newConfig.model } })
                if (newConfig.apiKey) {
                  whycremisi.send({ type: 'config.set', payload: { key: 'ai.apiKey', value: newConfig.apiKey, provider: newConfig.provider } })
                }
              }
            }}
            onSkip={() => {
              setSetupComplete(true)
              localStorage.setItem('whycremisi_setup_done', 'true')
            }}
          />
        )}
      </AnimatePresence>

      {/* ── HEADER ─────────────────────────────────────────────────── */}
      <header className="flex justify-between items-center w-full px-4 py-1 border-b z-50 relative h-9" style={{ backgroundColor: 'var(--bg-panel)', borderColor: 'var(--border)' }}>
        <div className="flex items-center gap-2">
          <MaskLogo audioLevel={(meterL + meterR) / 2} className="w-8 h-8 flex-shrink-0" />
          <h1 className="text-sm font-black tracking-tighter uppercase leading-none" style={{ color: 'var(--cremisi)' }}>WHYCREMISI</h1>
          {/* Le schede si chiamano in italiano come tutto il resto. L'id
              interno resta in inglese perché è la chiave dello stato: a
              cambiare è quello che si legge, non quello che si confronta.
              Sono pulsanti, non link: non portano da nessuna parte. */}
          <nav className="flex gap-4" role="tablist">
            {[['COMMAND','Comandi'],['TELEMETRY','Telemetria'],['SESSIONS','Sessioni']].map(([tab, nome]) => (
              <button key={tab} role="tab" aria-selected={activeTab === tab}
                onClick={() => setActiveTab(tab)}
                className="etichetta border-b pb-0.5 cursor-pointer transition-colors hover:text-[#FFB000]"
                style={{
                  color: activeTab === tab ? 'var(--amber)' : 'var(--text-secondary)',
                  borderColor: activeTab === tab ? 'var(--amber)' : 'transparent'
                }}
              >{nome}</button>
            ))}
          </nav>
        </div>

        <div className="flex items-center gap-3 font-mono">
          {/* DAW transport */}
          <div className="flex items-center gap-2">
            <div className="flex flex-col items-center leading-none">
              <span className="text-xs font-mono uppercase" style={{ color: 'var(--text-secondary)' }}>BPM</span>
              <span className="text-sm font-black tracking-tight" style={{ color: 'var(--amber)' }}>{transport.bpm.toFixed(1)}</span>
            </div>
            <motion.div
              className={`px-1.5 py-px text-xs font-black uppercase tracking-widest border ${
                transport.isPlaying
                  ? 'border-[#00FFaa]/40 bg-[#00FFaa]/10'
                  : 'border-[#555]/40 bg-transparent'
              }`}
              style={{ color: transport.isPlaying ? 'var(--green)' : 'var(--text-secondary)' }}
              animate={transport.isPlaying ? { opacity:[1,0.7,1] } : {}}
              transition={{ repeat: Infinity, duration: 1.2 }}
            >
              {transport.isPlaying ? '▶ PLAY' : '■ STOP'}
            </motion.div>
          </div>

          <div className="w-px h-4" style={{ backgroundColor: 'var(--bg-elevated)' }} />

          {/* Dati del motore audio. Compaiono solo quando ci sono: prima
              erano quattro colonne con un trattino sotto ciascuna, che
              occupavano la parte migliore della barra senza dire niente.
              Un'intestazione senza contenuto non è uno stato vuoto, è
              rumore. */}
          {(pluginStats.sampleRate || pluginStats.bufferSize || cpuUsage.cpuPercent) ? (
            <div className="flex items-center gap-3">
              {[
                ['SR',  pluginStats.sampleRate ? `${(pluginStats.sampleRate/1000).toFixed(0)} kHz` : null],
                ['Buf', pluginStats.bufferSize ? `${pluginStats.bufferSize}` : null],
                ['Lat', pluginStats.latencyMs  ? `${pluginStats.latencyMs.toFixed(0)} ms` : null],
                ['CPU', cpuUsage.cpuPercent ? `${cpuUsage.cpuPercent.toFixed(0)} %` : null],
              ].filter(([, v]) => v !== null).map(([k, v]) => (
                <div key={k} className="flex items-baseline gap-1 leading-none">
                  <span className="etichetta" style={{ color: 'var(--text-muted)' }}>{k}</span>
                  <span className="num font-medium" style={{ fontSize: 'var(--t-micro)', color: 'var(--text-primary)' }}>{v}</span>
                </div>
              ))}
            </div>
          ) : null}

          {/* Token spesi nella sessione. Compare solo dopo la prima
              risposta, per non mostrare uno zero fisso a vuoto. */}
          {tokenUsage.total > 0 && (
            <>
              <div className="w-px h-4" style={{ backgroundColor: 'var(--bg-elevated)' }} />
              <div
                className="flex flex-col items-center leading-none"
                title={`${tokenUsage.requests} richieste — ${tokenUsage.input} in, ${tokenUsage.output} out, ${tokenUsage.cacheRead} letti da cache`}
              >
                <span className="text-xs font-mono uppercase" style={{ color: 'var(--text-secondary)' }}>TOK</span>
                <span className="text-xs font-bold text-white tracking-tight">
                  {tokenUsage.total >= 1000
                    ? `${(tokenUsage.total / 1000).toFixed(1)}k`
                    : tokenUsage.total}
                </span>
              </div>
            </>
          )}

          <div className="w-px h-4" style={{ backgroundColor: 'var(--bg-elevated)' }} />

          {/* Connection LED + health */}
          <div className="flex items-center gap-1.5 group relative">
            <motion.div
              className={`w-1.5 h-1.5 rounded-full ${
                connStatus === ConnectionState.CONNECTED    ? 'bg-[#00FFaa]' :
                connStatus === ConnectionState.CONNECTING ||
                connStatus === ConnectionState.RECONNECTING ? 'bg-[#FFB000]' : 'bg-[#DC143C]'
              }`}
              animate={connStatus === ConnectionState.CONNECTING || connStatus === ConnectionState.RECONNECTING
                ? { opacity: [1,0.3,1] } : {}}
              transition={{ repeat: Infinity, duration: 0.8 }}
            />
            <span className="text-xs uppercase tracking-wider" style={{ color: 'var(--text-secondary)' }}>
              {connStatus === ConnectionState.CONNECTED    ? 'LIVE' :
               connStatus === ConnectionState.CONNECTING   ? 'CONN' :
               connStatus === ConnectionState.RECONNECTING ? 'RECONN' : 'OFFLINE'}
            </span>
            {/* Health tooltip */}
            <div className="absolute top-full right-0 mt-1 border rounded shadow-lg z-50 min-w-[160px] hidden group-hover:block p-2" style={{ backgroundColor: 'var(--bg-card)', borderColor: 'var(--border)' }}>
              <div className="text-[9px] font-mono space-y-1" style={{ color: 'var(--text-secondary)' }}>
                <div className="flex justify-between"><span>Status:</span><span style={{ 
                  color: connStatus === 'connected' ? 'var(--green)' : 
                         connStatus === 'reconnecting' ? 'var(--amber)' : 'var(--cremisi)'
                }}>{connStatus.toUpperCase()}</span></div>
                <div className="flex justify-between"><span>Queued:</span><span style={{ color: 'var(--amber)' }}>{whycremisi.getConnectionInfo ? whycremisi.getConnectionInfo().queueSize : 0}</span></div>
                {connStatus === ConnectionState.RECONNECTING && (
                  <div className="flex justify-between"><span>Retry:</span><span style={{ color: 'var(--amber)' }}>{whycremisi.getConnectionInfo ? `${whycremisi.getConnectionInfo().reconnectAttempts}/${whycremisi.getConnectionInfo().maxReconnectAttempts}` : '0/0'}</span></div>
                )}
              </div>
            </div>
          </div>

          <div className="w-px h-4" style={{ backgroundColor: 'var(--bg-elevated)' }} />

          {/* Action icons */}
          <div className="flex gap-1.5 items-center">
            <span className="material-symbols-outlined text-base hover:text-[#FFB000] cursor-pointer transition-colors"
              style={{ color: 'var(--text-secondary)' }}
              role="button" aria-label="Open setup"
              onClick={() => { setSetupComplete(false); localStorage.removeItem('whycremisi_setup_done') }}>settings</span>
            <span className="material-symbols-outlined text-base hover:text-[#DC143C] cursor-pointer transition-colors"
              style={{ color: 'var(--text-secondary)' }}
              role="button" aria-label="Disconnect"
              onClick={() => whycremisi.disconnect()}>power_settings_new</span>
          </div>
        </div>
      </header>


      {/* ── SESSION PANEL — absolute overlay, mai sposta il main ──── */}
      <AnimatePresence>
        {activeTab === 'SESSIONS' && (
          <motion.div
            key="session-panel"
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
            transition={{ duration: 0.18 }}
            className="fixed top-9 left-0 right-0 bottom-0 z-30"
            style={{ backgroundColor: 'var(--bg-primary)' }}
          >
            <SessionPanel />
          </motion.div>
        )}
      </AnimatePresence>

      {/* ── TELEMETRY DASHBOARD — absolute overlay ────────────────── */}
      <AnimatePresence>
        {activeTab === 'TELEMETRY' && (
          <motion.div
            key="telemetry-panel"
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
            transition={{ duration: 0.18 }}
            className="fixed top-9 left-0 right-0 bottom-9 z-30 overflow-y-auto custom-scrollbar p-4"
            style={{ backgroundColor: 'var(--bg-primary)' }}
          >
            <div className="max-w-full space-y-4">
              <div className="flex items-center gap-3 mb-4">
                <div className="w-1.5 h-1.5 rounded-full bg-[#FFB000] led-amber-active" />
                <span className="text-xs font-bold tracking-widest uppercase" style={{ color: 'var(--amber)' }}>Telemetry Dashboard — Live Analysis</span>
                <div className="flex-1 h-px" style={{ backgroundColor: 'var(--border)' }} />
                <span className="text-xs font-mono" style={{ color: 'var(--text-faint)' }}>REALTIME</span>
              </div>

              {/* ── STEREO ANALYSIS ─────────────────────────────── */}
              <div className="border bg-[#0e0e0e]" style={{ backgroundColor: 'var(--bg-surface)', borderColor: 'var(--border)' }}>
                <div className="px-3 py-1.5 border-b flex items-center gap-2" style={{ borderColor: 'var(--border-subtle)' }}>
                  <div className="w-1 h-1 rounded-full bg-[#DC143C]" />
                  <span className="text-xs font-bold uppercase tracking-widest" style={{ color: 'var(--cremisi)' }}>Stereo Field Analysis</span>
                </div>
                <div className="p-3 grid grid-cols-3 gap-3">
                  {[
                    { label: 'L/R Balance', val: (() => { const lP=Math.round(meterL*100), rP=Math.round(meterR*100); return lP>rP?`${lP-rP}% L`:rP>lP?`${rP-lP}% R`:'CENTER' })(), color: '#DC143C', pct: Math.abs(Math.round(meterL*100)-Math.round(meterR*100)) },
                    // Larghezza stereo dalla nuvola di punti vera del
                    // vectorscope: rapporto fra energia side e energia mid.
                    // Prima era |L−R| moltiplicato per 1,5, un numero senza
                    // rapporto con il campo stereo.
                    { label: 'Stereo Width', val: sonoro ? `${Math.round(stereoWidth*100)}%` : '—', color: '#FFB000', pct: Math.round(stereoWidth*100) },
                    // Correlazione misurata dal DSP. Prima: 0,62 più un po'
                    // di livello — un valore che sembrava sempre buono anche
                    // su un mix fuori fase.
                    { label: 'Correlation', val: sonoro ? `${correlation >= 0 ? '+' : ''}${correlation.toFixed(2)}` : '—', color: correlation < 0 ? '#DC143C' : '#00E5FF', pct: Math.round((correlation+1)/2*100) },
                  ].map(({ label, val, color, pct }) => (
                    <div key={label} className="space-y-1.5">
                      <div className="flex justify-between text-xs uppercase font-bold">
                        <span style={{ color: 'var(--text-secondary)' }}>{label}</span>
                        <span style={{ color }}>{val}</span>
                      </div>
                      <div className="h-1.5 bg-[#111] w-full">
                        <motion.div className="h-full" style={{ backgroundColor: color, boxShadow: `0 0 4px ${color}50` }}
                          initial={{ width: '0%' }} animate={{ width: `${Math.min(100, Math.max(0, pct))}%` }}
                          transition={{ duration: 0.8, ease: 'easeOut' }}
                        />
                      </div>
                    </div>
                  ))}
                </div>
              </div>

              {/* ── LOUDNESS ────────────────────────────────────── */}
              <div className="border" style={{ backgroundColor: 'var(--bg-surface)', borderColor: 'var(--border)' }}>
                <div className="px-3 py-1.5 border-b flex items-center gap-2" style={{ borderColor: 'var(--border-subtle)' }}>
                  <div className="w-1 h-1 rounded-full bg-[#00E5FF]" />
                  <span className="text-xs font-bold uppercase tracking-widest" style={{ color: 'var(--cyan)' }}>Loudness & Dynamics</span>
                </div>
                <div className="p-3 grid grid-cols-3 gap-3">
                  {[
                    { label: 'LUFS Integrated', val: lufs > -60 ? `${lufs.toFixed(1)} LUFS` : '-∞', color: '#00E5FF', pct: Math.max(0,Math.min(100,(lufs+60)/60*100)) },
                    { label: 'LUFS Short-term', val: lufsShort > -60 ? `${lufsShort.toFixed(1)} LUFS` : '-∞', color: '#00E5FF', pct: Math.max(0,Math.min(100,(lufsShort+60)/60*100)) },
                    // Loudness Range vero, in LU. Prima: "72 meno il livello
                    // dei meter per 0,72", che scendeva quando il pezzo
                    // saliva di volume — l'opposto di ciò che misura.
                    { label: 'Loudness Range', val: loudnessRange > 0 ? `${loudnessRange.toFixed(1)} LU` : '—', color: '#00FFaa', pct: Math.min(100, loudnessRange*8) },
                    // RMS misurato, non "LUFS più 2,1".
                    { label: 'RMS Level', val: rms > -60 ? `${rms.toFixed(1)} dB` : '-∞', color: '#FFB000', pct: Math.max(0,Math.min(100,(rms+60)/60*100)) },
                    // Cresta = true peak meno RMS, entrambi in dB. Prima era
                    // picco meno LUFS: due unità diverse sottratte fra loro.
                    { label: 'Crest Factor', val: (peak > -60 && rms > -60) ? `${(peak-rms).toFixed(1)} dB` : '—', color: '#DC143C', pct: Math.min(100,Math.max(0,(peak-rms)*5)) },
                    // True peak vero dall'analizzatore, non "picco più 0,3".
                    { label: 'True Peak', val: peak > -60 ? `${peak.toFixed(1)} dBTP` : '-∞', color: peak > -1 ? '#DC143C' : '#FF6B35', pct: Math.max(0,Math.min(100,(peak+60)/60*100)) },
                  ].map(({ label, val, color, pct }) => (
                    <div key={label} className="space-y-1.5">
                      <div className="flex justify-between text-xs uppercase font-bold">
                        <span style={{ color: 'var(--text-secondary)' }}>{label}</span>
                        <span style={{ color }}>{val}</span>
                      </div>
                      <div className="h-1.5 bg-[#111] w-full">
                        <motion.div className="h-full" style={{ backgroundColor: color, boxShadow: `0 0 4px ${color}50` }}
                          initial={{ width: '0%' }} animate={{ width: `${Math.min(100, Math.max(0, pct))}%` }}
                          transition={{ duration: 0.8, ease: 'easeOut' }}
                        />
                      </div>
                    </div>
                  ))}
                </div>
              </div>

              {/* ── FREQUENCY CONTENT ───────────────────────────── */}
              <div className="border" style={{ backgroundColor: 'var(--bg-surface)', borderColor: 'var(--border)' }}>
                <div className="px-3 py-1.5 border-b flex items-center gap-2" style={{ borderColor: 'var(--border-subtle)' }}>
                  <div className="w-1 h-1 rounded-full bg-[#FFB000]" />
                  <span className="text-xs font-bold uppercase tracking-widest" style={{ color: 'var(--amber)' }}>Frequency Content</span>
                </div>
                <div className="p-3 grid grid-cols-3 gap-3">
                  {/* Energia per banda misurata sullo spettro reale. Prima
                      questi sei valori erano tutti della forma
                      "livello del meter per una costante più un'altra
                      costante": si muovevano insieme al volume e non
                      dicevano niente sul contenuto in frequenza. Le bande
                      sono logaritmiche da 20 Hz a 20 kHz, una per ottava,
                      e qui vengono raggruppate per come si parla di un mix. */}
                  {[
                    ...bandeSpettro,
                    // Baricentro e rolloff: due numeri che dicono dov'è il
                    // peso del suono e fin dove arriva davvero. Li calcolava
                    // già l'analizzatore ma non uscivano mai dal plugin.
                    { label: 'Baricentro', color: '#E8D5B7',
                      val: sonoro && centroid > 0 ? `${centroid >= 1000 ? (centroid/1000).toFixed(2)+' kHz' : Math.round(centroid)+' Hz'}` : '—',
                      pct: sonoro ? Math.min(100, centroid / 8000 * 100) : 0 },
                    { label: 'Rolloff 85%', color: '#8899AA',
                      val: sonoro && rolloff > 0 ? `${(rolloff/1000).toFixed(1)} kHz` : '—',
                      pct: sonoro ? Math.min(100, rolloff / 20000 * 100) : 0 },
                  ].map(({ label, val, color, pct }) => (
                    <div key={label} className="space-y-1.5">
                      <div className="flex justify-between text-xs uppercase font-bold">
                        <span style={{ color: 'var(--text-secondary)' }}>{label}</span>
                        <span style={{ color }}>{val}</span>
                      </div>
                      <div className="h-1.5 bg-[#111] w-full">
                        <motion.div className="h-full" style={{ backgroundColor: color, boxShadow: `0 0 4px ${color}50` }}
                          initial={{ width: '0%' }} animate={{ width: `${Math.min(100, Math.max(0, pct))}%` }}
                          transition={{ duration: 0.8, ease: 'easeOut' }}
                        />
                      </div>
                    </div>
                  ))}
                </div>
              </div>

              {/* ── TRANSPORT ───────────────────────────────────── */}
              <div className="border" style={{ backgroundColor: 'var(--bg-surface)', borderColor: 'var(--border)' }}>
                <div className="px-3 py-1.5 border-b flex items-center gap-2" style={{ borderColor: 'var(--border-subtle)' }}>
                  <div className="w-1 h-1 rounded-full bg-[#00FFaa]" />
                  <span className="text-xs font-bold uppercase tracking-widest" style={{ color: 'var(--green)' }}>Transport & Plugin Info</span>
                </div>
                <div className="p-3 grid grid-cols-4 gap-3">
                  {[
                    { label: 'BPM', val: transport.bpm.toFixed(1), color: '#FFB000' },
                    { label: 'Status', val: transport.isPlaying ? '▶ PLAY' : '■ STOP', color: transport.isPlaying ? '#00FFaa' : '#4d4d4d' },
                    { label: 'Sample Rate', val: pluginStats.sampleRate ? `${(pluginStats.sampleRate/1000).toFixed(1)}kHz` : '—', color: '#00E5FF' },
                    { label: 'Buffer Size', val: pluginStats.bufferSize ? `${pluginStats.bufferSize} smp` : '—', color: '#00E5FF' },
                    { label: 'Latency', val: pluginStats.latencyMs ? `${pluginStats.latencyMs.toFixed(1)}ms` : '—', color: '#00E5FF' },
                    { label: 'Position', val: `${Math.floor(transport.position/60).toString().padStart(2,'0')}:${(transport.position%60).toFixed(0).toString().padStart(2,'0')}`, color: '#4d4d4d' },
                    { label: 'DAW Link', val: dawConnected ? 'SYNC' : 'OFFLINE', color: dawConnected ? '#00FFaa' : '#DC143C' },
                    { label: 'Connection', val: connStatus === 'connected' ? 'LIVE' : 'OFFLINE', color: connStatus === 'connected' ? '#00FFaa' : '#DC143C' },
                  ].map(({ label, val, color }) => (
                    <div key={label} className="p-2 flex flex-col gap-0.5">
                      <span className="text-xs font-mono uppercase" style={{ color: 'var(--text-faint)' }}>{label}</span>
                      <span className="text-xs font-bold font-mono" style={{ color }}>{val}</span>
                    </div>
                  ))}
                </div>
              </div>
            </div>
          </motion.div>
        )}
      </AnimatePresence>

      {/* ── MAIN GRID — sempre nel DOM, mai mosso ──────────────────── */}
      <main className="ml-0 mt-0 h-[calc(100vh-2.25rem)] grid grid-cols-12 grid-rows-6 p-1 gap-1 overflow-hidden relative z-10" style={{ backgroundColor: 'var(--bg-primary)' }}>

        {/* ── AI CHAT CONSOLE (9/12 × 4/6) ──────────────────────── */}
        <section className="col-span-9 row-span-4 border flex flex-col overflow-hidden" style={{ backgroundColor: 'var(--bg-surface)', borderColor: 'var(--border)' }}>
          <div className="px-3 py-1 flex justify-between items-center border-b" style={{ backgroundColor: 'var(--bg-card)', borderColor: 'var(--border)' }}>
            <div className="flex items-center gap-2">
              <div className="w-1.5 h-1.5 rounded-full bg-[#FFB000] led-amber-active" />
              <span className="text-xs font-bold tracking-widest uppercase" style={{ color: 'var(--amber)' }}>AI Command Console</span>
            </div>
            <div className="flex items-center gap-3 text-xs font-mono" style={{ color: 'var(--text-secondary)' }}>
              {dawConnected && <span style={{ color: 'var(--green)' }}>● DAW SYNC</span>}
              {personality?.style && personality.style !== 'warm' && (
                <span className="text-[9px] uppercase tracking-widest font-bold px-1.5 py-px border"
                  style={{ color: personality.style === 'direct' ? 'var(--cremisi)' : personality.style === 'consultative' ? 'var(--amber)' : personality.style === 'analytical' ? 'var(--cyan)' : personality.style === 'creative' ? '#AA44FF' : 'var(--text-secondary)', borderColor: 'currentColor' }}
                >{personality.style}</span>
              )}
            </div>
          </div>

          {/* Search bar */}
          <AnimatePresence>
            {searchOpen && (
              <motion.div
                initial={{ height: 0, opacity: 0 }}
                animate={{ height: 32, opacity: 1 }}
                exit={{ height: 0, opacity: 0 }}
                className="overflow-hidden flex items-center gap-2 px-4 border-b" style={{ borderColor: 'var(--border)', backgroundColor: 'var(--bg-surface)' }}
              >
                <span className="material-symbols-outlined text-sm" style={{ color: 'var(--text-secondary)' }}>search</span>
                <input
                  ref={searchRef}
                  className="flex-1 bg-transparent border-none text-xs font-mono outline-none focus:ring-0" style={{ color: 'var(--text-primary)' }}
                  placeholder="Search messages..."
                  value={searchQuery}
                  onChange={e => setSearchQuery(e.target.value)}
                  onKeyDown={e => e.key === 'Escape' && (setSearchOpen(false), setSearchQuery(''))}
                />
                <span className="text-[9px] font-mono" style={{ color: 'var(--text-muted)' }}>
                  {searchQuery ? messages.filter(m => m.text?.toLowerCase().includes(searchQuery.toLowerCase())).length : 0} matches
                </span>
                <button onClick={() => { setSearchOpen(false); setSearchQuery('') }} className="text-xs" style={{ color: 'var(--text-secondary)' }}>✕</button>
              </motion.div>
            )}
          </AnimatePresence>

          {/* Messages */}
          <div ref={listRef} onScroll={onListScroll} className="flex-1 px-4 py-3 font-mono overflow-y-auto overflow-x-hidden custom-scrollbar space-y-3">

            {/* Empty state — mostrato finché non ci sono risposte AI */}
            {messages.filter(m => m.type === 'bot').length === 0 && (
              <motion.div
                initial={{ opacity: 0, y: 8 }}
                animate={{ opacity: 1, y: 0 }}
                className="flex flex-col items-center justify-center h-full gap-4 pt-12"
              >
                <BotFace state={botState} className="w-16 h-16" personality={personality} audioLevel={(meterL + meterR) / 2} />
                <div className="text-center space-y-1">
                  <p className="text-sm font-mono uppercase tracking-widest" style={{ color: 'var(--text-secondary)' }}>Pronto ad ascoltare</p>
                  <p className="text-xs font-mono" style={{ color: 'var(--text-faint)' }}>Scrivi qualcosa nel campo CMD qui sotto</p>
                </div>
              </motion.div>
            )}

            {/* Loading skeleton for slow AI responses */}
            <AnimatePresence>
              {botState === 'thinking' && !messages.some(m => m.streaming) && (
                <motion.div
                  initial={{ opacity: 0, y: 8 }}
                  animate={{ opacity: 1, y: 0 }}
                  exit={{ opacity: 0, y: -4 }}
                  transition={{ duration: 0.3 }}
                  className="skeleton-message"
                >
                  <div className="skeleton-avatar" />
                  <div className="skeleton-content">
                    <div className="skeleton-line amber" style={{ width: '35%' }} />
                    <div className="skeleton-line" />
                    <div className="skeleton-line" />
                    <div className="skeleton-line" style={{ width: '55%' }} />
                  </div>
                </motion.div>
              )}

              {messages.filter(msg => !searchQuery || msg.text?.toLowerCase().includes(searchQuery.toLowerCase())).map((msg) => {
                if (msg.type === 'system') return (
                  <div key={msg.id} className="flex items-center gap-2">
                    <div className="w-1 h-full min-h-[10px] flex-shrink-0" style={{ backgroundColor: 'var(--border-light)' }} />
                    <span className="text-[10px] font-mono tracking-wide" style={{ color: 'var(--text-secondary)' }}>{msg.text}</span>
                  </div>
                )

                if (msg.type === 'user') return <UserBubble key={msg.id} msg={msg} />

                if (msg.type === 'bot') return (
                  <BotBubble key={msg.id} msg={msg} isLatest={msg.id === lastBotId} box={boxProps} />
                )
                return null
              })}
            </AnimatePresence>
            <div ref={chatEndRef} />
          </div>

{/* Terminal input */}
           <div className="h-10 border-t flex items-center px-3 gap-2" style={{ backgroundColor: 'var(--bg-panel)', borderColor: 'var(--border)' }}>
             <span className="font-bold text-xs tracking-widest shrink-0" style={{ color: 'var(--amber)' }}>CMD&gt;</span>
            <div className="flex-1 relative flex items-center">
              <input
                ref={inputRef}
                className="w-full bg-transparent border-none text-white font-mono text-xs focus:ring-0 focus:outline-none placeholder-[#4d4d4d] p-0 disabled:opacity-40"
                placeholder={botState === 'thinking' || botState === 'typing' ? 'PROCESSING...' : 'Chiedi qualsiasi cosa sul tuo mix…'}
                value={inputVal}
                onChange={e => setInputVal(e.target.value)}
                onKeyDown={handleCommand}
                disabled={botState === 'thinking' || botState === 'typing'}
              />
              {(botState === 'idle' || botState === 'sad' || botState === 'loading') && <div className="terminal-cursor" />}
            </div>
            {/* Il tasto giusto per la piattaforma: qui c'era ⌘ fisso, che su
                Windows indica una scorciatoia che non esiste. E le due voci
                dicevano tutte e due "clear" pur facendo cose diverse. */}
            <div className="kbd-hint mr-2 hidden md:flex">
              <kbd>{tastoComando}</kbd><kbd>K</kbd> svuota
              <span className="mx-1">·</span>
              <kbd>Esc</kbd> annulla
              <span className="mx-1">·</span>
              <kbd>&#8593;</kbd><kbd>&#8595;</kbd> cronologia
            </div>
            {connStatus !== ConnectionState.CONNECTED && connStatus !== ConnectionState.DISCONNECTED && (
              <span className="queued-indicator etichetta">In coda</span>
            )}
<div className="flex gap-1.5" style={{ color: 'var(--text-secondary)' }}>
              <motion.button whileHover={{ scale:1.1 }} whileTap={{ scale:0.9 }}
                className="material-symbols-outlined text-base cursor-pointer hover:text-[#FFB000] transition-colors"
                aria-label="Play" onClick={() => dawCmd('play')}>play_arrow</motion.button>
              <motion.button whileHover={{ scale:1.1 }} whileTap={{ scale:0.9 }}
                className="material-symbols-outlined text-base cursor-pointer hover:text-[#FFB000] transition-colors"
                aria-label="Stop" onClick={() => dawCmd('stop')}>stop</motion.button>
              <motion.button whileHover={{ scale:1.1 }} whileTap={{ scale:0.9 }}
                className={`material-symbols-outlined text-base cursor-pointer transition-colors ${transport.isRecording ? 'led-red-active' : ''}`}
                style={{ color: transport.isRecording ? 'var(--cremisi)' : 'inherit' }}
                aria-label={transport.isRecording ? 'Stop recording' : 'Record'}
                onClick={() => dawCmd('record')}>fiber_manual_record</motion.button>
            </div>
          </div>
         </section>

        {/* ── MASTERING RACK (3/12 × 4/6) ───────────────────────── */}
        <section className="col-span-3 row-span-4 border flex flex-col" style={{ backgroundColor: 'var(--bg-panel)', borderColor: 'var(--border)' }}>
          <div className="px-3 py-1 border-b flex justify-between items-center" style={{ backgroundColor: 'var(--bg-card)', borderColor: 'var(--border)' }}>
            <div className="flex items-center gap-2">
              <div className="w-1.5 h-1.5 rounded-full bg-[#DC143C] led-red-active" />
              <span className="text-xs font-bold tracking-widest uppercase" style={{ color: 'var(--cremisi)' }}>Mastering Rack</span>
            </div>
            <div className="flex items-center gap-2">
              <motion.button
                className={`text-[9px] font-bold uppercase tracking-wider px-2 py-0.5 rounded-sm border ${showMapPanel ? 'bg-[#00E5FF] text-black border-[#00E5FF]' : 'hover:border-[#00E5FF] hover:text-[#00E5FF]'}`}
                style={{ color: showMapPanel ? '#000' : 'var(--text-secondary)', borderColor: showMapPanel ? 'var(--cyan)' : 'var(--border-light)' }}
                onClick={() => setShowMapPanel(p => !p)}
                whileTap={{ scale: 0.95 }}
              >MAP</motion.button>
              <motion.button
                className={`text-[9px] font-bold uppercase tracking-wider px-2 py-0.5 rounded-sm border ${showChainPanel ? 'bg-[#FF6B35] text-black border-[#FF6B35]' : 'hover:border-[#FF6B35] hover:text-[#FF6B35]'}`}
                style={{ color: showChainPanel ? '#000' : 'var(--text-secondary)', borderColor: showChainPanel ? 'var(--orange)' : 'var(--border-light)' }}
                onClick={() => setShowChainPanel(p => !p)}
                whileTap={{ scale: 0.95 }}
              >CHAIN</motion.button>
              <span className="text-xs font-mono" style={{ color: 'var(--text-secondary)' }}>Stereo master</span>
            </div>
          </div>

{/* Plugin Chain Panel */}
           {showChainPanel && (
             <div className="border-b px-3 py-2 space-y-1 max-h-40 overflow-y-auto" style={{ backgroundColor: 'var(--bg-primary)', borderColor: 'var(--border)' }}>
               <div className="flex justify-between items-center mb-1">
                 <span className="text-[9px] font-bold uppercase tracking-wider" style={{ color: 'var(--orange)' }}>Catena plugin</span>
                 <span className="text-[9px]" style={{ color: 'var(--text-faint)' }}>{pluginChain.length} plugin</span>
               </div>
               {pluginChain.length === 0 ? (
                 <div className="text-[9px] italic" style={{ color: 'var(--text-dim)' }}>Nessun plugin indicato. Elencando qui i plugin della tua DAW, l'AI sa cosa c'è nella catena.</div>
               ) : (
                 pluginChain.map((p, i) => (
                   <div key={p.id || i} className="flex justify-between items-center text-[10px] font-mono">
                     <span style={{ color: 'var(--text-secondary)' }}>{i+1}. {p.name}</span>
                     <span className="text-[8px]" style={{ color: 'var(--text-secondary)' }}>{p.manufacturer} {p.format}</span>
                     <motion.button
                       className="hover:text-[#ff6b6b] text-[9px]"
                       style={{ color: 'var(--cremisi)' }}
                       onClick={() => {
                         const updated = pluginChain.filter(x => x.id !== p.id)
                         setPluginChain(updated)
                         if (whycremisi.isConnected()) whycremisi.setPluginChain(updated)
                       }}
                       whileTap={{ scale: 0.9 }}
                     >REMOVE</motion.button>
                   </div>
                 ))
               )}
               {/* Add plugin form */}
               <div className="flex gap-1 pt-1 border-t" style={{ borderColor: 'var(--border)' }}>
                 <input
                   className="flex-1 text-[10px] px-1 py-0.5 outline-none font-mono"
                   style={{ backgroundColor: 'var(--bg-surface)', borderColor: 'var(--border-light)', color: 'var(--text-primary)' }}
                  placeholder="Plugin name..."
                  id="chainPluginName"
                  onKeyDown={(e) => {
                    if (e.key === 'Enter') {
                      const name = e.target.value.trim()
                      if (!name) return
                      const id = 'plugin_' + Date.now()
                      const updated = [...pluginChain, { id, name, manufacturer: '', format: 'VST3', slot: pluginChain.length, enabled: true }]
                      setPluginChain(updated)
                      if (whycremisi.isConnected()) whycremisi.setPluginChain(updated)
                      e.target.value = ''
                    }
                  }}
                />
                <motion.button
                  className="text-[9px] bg-[#FF6B35] text-black font-bold px-2 py-0.5 rounded-sm"
                  onClick={() => {
                    const input = document.getElementById('chainPluginName')
                    const name = input?.value?.trim()
                    if (!name) return
                    const id = 'plugin_' + Date.now()
                    const updated = [...pluginChain, { id, name, manufacturer: '', format: 'VST3', slot: pluginChain.length, enabled: true }]
                    setPluginChain(updated)
                    if (whycremisi.isConnected()) whycremisi.setPluginChain(updated)
                    if (input) input.value = ''
                  }}
                  whileTap={{ scale: 0.9 }}
                >ADD</motion.button>
              </div>
            </div>
          )}

{/* Parameter Mapping Panel */}
           {showMapPanel && (
             <div className="border-b px-3 py-2 space-y-1 max-h-40 overflow-y-auto" style={{ backgroundColor: 'var(--bg-primary)', borderColor: 'var(--border)' }}>
               <div className="flex justify-between items-center mb-1">
                 <span className="text-[9px] font-bold uppercase tracking-wider" style={{ color: 'var(--cyan)' }}>Parameter Mapping</span>
                 <span className="text-[9px]" style={{ color: 'var(--text-faint)' }}>{midiMappings.length} bindings</span>
               </div>
               {midiMappings.length === 0 ? (
                 <div className="text-[9px] italic" style={{ color: 'var(--text-dim)' }}>No MIDI mappings. Click LEARN on a widget and move a hardware controller.</div>
               ) : (
                 midiMappings.map(m => (
                   <div key={m.widgetId} className="flex justify-between items-center text-[10px] font-mono">
                     <span style={{ color: 'var(--text-secondary)' }}>{m.widgetId}</span>
                     <span style={{ color: 'var(--orange)' }}>CC#{m.cc} Ch.{m.channel}</span>
                     <motion.button
                       className="hover:text-[#ff6b6b] text-[9px]"
                       style={{ color: 'var(--cremisi)' }}
                       onClick={() => {
                         setMidiMappings(prev => prev.filter(x => x.widgetId !== m.widgetId))
                         if (whycremisi.isConnected()) {
                           whycremisi.sendMessage('config.set', { key: `midi.unmap.${m.widgetId}`, value: '' })
                         }
                       }}
                       whileTap={{ scale: 0.9 }}
                     >REMOVE</motion.button>
                   </div>
                 ))
               )}
             </div>
           )}

           <div className="flex-1 flex p-3 gap-4 overflow-hidden">
             {/* Gain fader */}
             <div className="w-14 flex flex-col items-center gap-1">
               {/* Estremi della scala. In fondo c'era scritto "−INF", ma il
                   cursore copre −60…+12 dB e a fondo corsa vale −60, non
                   meno infinito: la scala diceva una cosa che il controllo
                   non fa. */}
               <span className="num font-mono" style={{ fontSize: 'var(--t-micro)', color: 'var(--amber)' }}>+12</span>
               <div
                 className="flex-1 w-8 border relative flex flex-col justify-end p-1 cursor-ns-resize"
                 style={{ backgroundColor: 'var(--bg-surface)', borderColor: 'var(--border)' }}
                onMouseMove={(e) => {
                  if (e.buttons !== 1) return
                  const now = Date.now()
                  if (now - throttleRef.current < 50) return
                  throttleRef.current = now
                  const rect = e.currentTarget.getBoundingClientRect()
                  const pct = 1 - (e.clientY - rect.top) / rect.height
                  const valDb = Math.round((pct * 72 - 60) * 10) / 10
                  setGainDb(valDb)
                  if (whycremisi.isConnected()) whycremisi.sendDAWCommand('setGain', { valueDb: valDb })
                }}
              >
                {[10,25,50,75].map(p => (
                  <div key={p} className="absolute inset-x-1 h-[0.5px] bg-[#4d4d4d] opacity-30" style={{ top:`${p}%` }} />
                ))}
                <motion.div
                  className="w-full bg-gradient-to-t from-[#DC143C]/30 to-[#DC143C] border-t-2 border-[#ffb3b3]"
                  style={{ height: `${gainPct}%`, boxShadow:'0 0 8px rgba(220,20,60,0.4)' }}
                  animate={{ height: `${gainPct}%` }}
                  transition={{ duration:0.1 }}
                />
              </div>
               <span className="num font-mono" style={{ fontSize: 'var(--t-micro)', color: 'var(--amber)' }}>−60</span>
               <span className="num font-bold mt-1 whitespace-nowrap" style={{ fontSize: 'var(--t-dato)', color: 'var(--text-primary)', lineHeight: 1 }}>
                 {gainDb > 0 ? `+${gainDb.toFixed(1)}` : gainDb.toFixed(1)}
                 <span style={{ fontSize: 'var(--t-micro)', color: 'var(--text-muted)', marginLeft: '0.15em' }}>dB</span>
               </span>
               <span className="etichetta" style={{ color: 'var(--text-muted)' }}>Guadagno</span>
               {/* MIDI Learn button */}
               <motion.button
                 className={`w-full mt-1 py-0.5 text-[9px] font-bold uppercase tracking-wider border rounded-sm ${midiLearnWidget === 'masterGain' ? 'bg-[#FF6B35] text-black border-[#FF6B35] pulse-glow' : 'hover:border-[#FF6B35] hover:text-[#FF6B35]'}`}
                 style={{ color: midiLearnWidget === 'masterGain' ? '#000' : 'var(--text-secondary)', borderColor: midiLearnWidget === 'masterGain' ? 'var(--orange)' : 'var(--border-light)' }}
                 onClick={() => {
                   if (midiLearnWidget === 'masterGain') {
                     whycremisi.midiLearnStop()
                   } else {
                     whycremisi.midiLearnStart('masterGain')
                   }
                 }}
                 whileTap={{ scale: 0.95 }}
               >{midiLearnWidget === 'masterGain' ? 'LEARNING...' : 'LEARN'}</motion.button>
             </div>

             {/* Controls cluster */}
             <div className="flex-1 flex flex-col gap-4 min-w-0">
               {/* ── La catena, con lo stato vero ────────────────────
                   Qui c'erano due caselle "Deep Neural" e "Master Chain"
                   con il cursore a manina e l'ingrandimento al passaggio
                   del mouse, ma senza alcun onClick: erano spie travestite
                   da interruttori, e cliccarle non faceva niente. Sotto,
                   un menu "Saturation → CRIMSON_TUBE" con la freccia del
                   menu a tendina, per un modulo che nel DSP non esiste.

                   Al loro posto ci sono i tre moduli che esistono davvero,
                   con lo stato che riporta il plugin e un interruttore che
                   li accende e li spegne per davvero. */}
               <div>
                 <div className="flex items-baseline justify-between mb-2">
                   <span className="etichetta" style={{ color: 'var(--amber)' }}>Catena</span>
                   <button
                     // Nel bridge "enabled" su questi comandi significa
                     // "bypass attivo": passare tuttoAttivo spegne tutto,
                     // il contrario accende tutto.
                     onClick={() => dawCmd('dspBypassAll', { enabled: tuttoAttivo })}
                     className="etichetta transition-colors hover:text-white"
                     style={{ color: 'var(--text-muted)' }}
                     title={tuttoAttivo ? 'Spegne tutti i moduli' : 'Accende tutti i moduli'}
                   >
                     {tuttoAttivo ? 'Spegni tutto' : 'Accendi tutto'}
                   </button>
                 </div>

                 <div className="space-y-1.5">
                   {[
                     { nome: 'EQ',          attivo: !dspState.eqBypassed,    cmd: 'eqBypass' },
                     { nome: 'Compressore', attivo: !dspState.compBypassed,  cmd: 'compBypass' },
                     { nome: 'Limiter',     attivo: !dspState.limitBypassed, cmd: 'limiter' },
                   ].map(({ nome, attivo, cmd }) => (
                     <button
                       key={nome}
                       role="switch"
                       aria-checked={attivo}
                       onClick={() => dawCmd(cmd, cmd === 'limiter' ? { enabled: !attivo } : { enabled: attivo })}
                       className="w-full flex items-center justify-between px-2 py-1.5 border rounded-sm transition-colors"
                       style={{
                         backgroundColor: attivo ? 'var(--bg-card)' : 'transparent',
                         borderColor: attivo ? 'color-mix(in srgb, var(--cremisi) 55%, transparent)' : 'var(--border)',
                         color: attivo ? 'var(--text-primary)' : 'var(--text-muted)',
                       }}
                       title={attivo ? `${nome} attivo — clic per escluderlo` : `${nome} escluso — clic per attivarlo`}
                     >
                       <span style={{ fontSize: 'var(--t-micro)', fontWeight: 600 }}>{nome}</span>
                       <span
                         className="w-1.5 h-1.5 rounded-full flex-shrink-0"
                         style={{ backgroundColor: attivo ? 'var(--cremisi)' : 'var(--bg-elevated)' }}
                       />
                     </button>
                   ))}
                 </div>

                 {/* Una riga di stato, non due interruttori finti. */}
                 <div className="mt-2.5 pt-2.5 border-t flex flex-col gap-1" style={{ borderColor: 'var(--border)' }}>
                   {[
                     { nome: 'AI', acceso: botState !== 'idle', stato: botState !== 'idle' ? 'al lavoro' : 'in attesa' },
                     { nome: 'DAW', acceso: dawConnected, stato: dawConnected ? 'collegata' : 'non collegata' },
                   ].map(({ nome, acceso, stato }) => (
                     <div key={nome} className="flex items-center justify-between" style={{ fontSize: 'var(--t-micro)' }}>
                       <span className="etichetta" style={{ color: 'var(--text-muted)' }}>{nome}</span>
                       <span className="flex items-center gap-1.5" style={{ color: acceso ? 'var(--text-secondary)' : 'var(--text-dim)' }}>
                         {stato}
                         <span className="w-1.5 h-1.5 rounded-full" style={{ backgroundColor: acceso ? 'var(--green)' : 'var(--bg-elevated)' }} />
                       </span>
                     </div>
                   ))}
                 </div>
              </div>

              {/* Mask Logo — audio reactive */}
              <div className="flex-1 flex items-center justify-center min-h-0">
                <MaskLogo audioLevel={(meterL + meterR) / 2} className="w-32 h-32" />
              </div>
             </div>
           </div>

           {/* Peak meter strip */}
           <div className="h-10 border-t flex items-center px-3 gap-2" style={{ backgroundColor: 'var(--bg-surface)', borderColor: 'var(--border)' }}>
             <div className="flex-1 h-2 flex gap-[1px]" style={{ backgroundColor: 'var(--bg-card)' }}>
               {/* Le tacche seguono il livello e basta. Prima pulsavano con
                   un'animazione infinita indipendente dall'audio: si
                   muovevano anche in silenzio, facendo sembrare che stesse
                   passando del segnale. */}
               {meterBars.map((bar, i) => (
                 <motion.div key={i} className="flex-1 h-full"
                   animate={{ opacity: bar.active ? 1 : 0.12 }}
                   transition={{ duration: 0.08 }}
                   style={{ backgroundColor: bar.color }}
                 />
               ))}
             </div>
             <span className="etichetta" style={{ color: letture.peak > -1 ? 'var(--cremisi)' : 'var(--text-muted)' }}>
               {letture.peak > -1 ? 'Picco!' : sonoro ? 'Ok' : '—'}
             </span>
           </div>
         </section>

{/* ── MASTER MONITOR (full width × 2/6) ──────────────────── */}
          <section className="col-span-12 row-span-2 border flex overflow-hidden" style={{ backgroundColor: 'var(--bg-surface)', borderColor: 'var(--border)' }}>

            {/* ── L/R STEREO METERS ─────────────────────────────────── */}
           <div className="w-52 flex-shrink-0 border-r flex flex-col justify-center px-4 py-2 gap-1.5" style={{ borderColor: 'var(--border)' }}>
             <div className="etichetta mb-0.5" style={{ color: 'var(--amber)' }}>Stereo master</div>
             {/* La barra segue il valore pieno a trenta fotogrammi, la cifra
                 quello campionato: la prima deve scorrere, la seconda si
                 deve poter leggere. */}
             {[['L', meterL, letture.meterL], ['R', meterR, letture.meterR]].map(([ch, vivo, lento]) => {
               const pct = Math.max(0, Math.min(100, vivo * 100))
               const db = (lento * 72 - 60)
               const barColor = pct > 88 ? '#DC143C' : pct > 70 ? '#FFB000' : '#00FFaa'
               return (
                 <div key={ch} className="flex items-center gap-2">
                   <span className="font-bold w-3" style={{ fontSize: 'var(--t-micro)', color: 'var(--text-secondary)' }}>{ch}</span>
                   <div className="flex-1 h-3 bg-[#111] relative overflow-hidden rounded-sm">
                     <motion.div
                       className="h-full absolute left-0 top-0 rounded-sm"
                       animate={{ width: `${pct}%`, backgroundColor: barColor }}
                       transition={{ duration: 0.05 }}
                       style={{ boxShadow: `0 0 6px ${barColor}60` }}
                     />
                     {[33, 58, 83].map(p => (
                       <div key={p} className="absolute top-0 bottom-0 w-[0.5px]" style={{ left: `${p}%`, backgroundColor: 'var(--border-light)' }} />
                     ))}
                   </div>
                   <span className="num font-mono w-14 text-right" style={{ fontSize: 'var(--t-micro)', color: 'var(--text-secondary)' }}>
                     {db <= -59.9 ? '−∞' : `${db.toFixed(1)} dB`}
                   </span>
                 </div>
               )
             })}
             <div className="flex justify-between mt-0.5 num font-mono" style={{ fontSize: 'var(--t-micro)', color: 'var(--text-dim)' }}>
               <span>−60</span><span>−12</span><span>−6</span><span>0</span>
             </div>
           </div>

           {/* ── TRANSPORT + STATS ─────────────────────────────────── */}
           <div className="w-44 flex-shrink-0 border-r flex flex-col justify-center px-4 py-2 gap-1.5" style={{ borderColor: 'var(--border)' }}>
             <div className="flex items-center justify-between">
               <motion.div
                 className="text-xl font-black tracking-tighter"
                 style={{ color: transport.isPlaying ? 'var(--green)' : 'var(--text-dim)' }}
                 animate={transport.isPlaying ? { opacity:[1,0.6,1] } : {}}
                 transition={{ repeat: Infinity, duration: 1.2 }}
               >
                 {transport.isPlaying ? '▶' : '■'}
               </motion.div>
               <div className="text-right">
                 <div className="font-mono text-base font-bold tracking-tight leading-none" style={{ color: 'var(--amber)' }}>
                   {transport.bpm.toFixed(1)}
                 </div>
                 <div className="text-[9px] font-mono uppercase tracking-wider" style={{ color: 'var(--text-secondary)' }}>BPM</div>
               </div>
             </div>
             {transport.isRecording && (
               <motion.div className="text-[10px] font-bold uppercase tracking-widest"
                 style={{ color: 'var(--cremisi)' }}
                 animate={{ opacity:[1,0.3,1] }} transition={{ repeat: Infinity, duration: 0.8 }}
               >● RECORDING</motion.div>
             )}
             <div className="flex items-center justify-between font-mono" style={{ fontSize: 'var(--t-micro)' }}>
               <span className="etichetta" style={{ color: 'var(--text-muted)' }}>Pos</span>
               <span className="num" style={{ color: 'var(--text-primary)' }}>
                 {Math.floor(transport.position / 60).toString().padStart(2,'0')}:{(transport.position % 60).toFixed(0).toString().padStart(2,'0')}
               </span>
             </div>
             {/* Anche qui: si mostra solo ciò che esiste. La correlazione
                 c'è sempre quando passa audio, il resto dipende dal motore. */}
             <div className="grid grid-cols-2 gap-x-3 gap-y-1 font-mono" style={{ fontSize: 'var(--t-micro)' }}>
               {[
                 ['SR', pluginStats.sampleRate ? `${(pluginStats.sampleRate / 1000).toFixed(0)}k` : null],
                 ['Buf', pluginStats.bufferSize ? `${pluginStats.bufferSize}` : null],
                 ['Lat', pluginStats.latencyMs ? `${pluginStats.latencyMs.toFixed(0)} ms` : null],
                 ['Corr', sonoro ? letture.correlation.toFixed(2) : null],
               ].filter(([, v]) => v !== null).map(([k, v]) => (
                 <div key={k} className="flex items-baseline gap-1">
                   <span className="etichetta" style={{ color: 'var(--text-dim)' }}>{k}</span>
                   <span className="num" style={{ color: 'var(--text-primary)' }}>{v}</span>
                 </div>
               ))}
             </div>
           </div>

           {/* ── REAL-TIME SPECTRUM ANALYZER ─────────────────────────── */}
           <div className="w-60 flex-shrink-0 border-r flex flex-col justify-center px-3 py-2 gap-1" style={{ borderColor: 'var(--border)' }}>
             <div className="flex justify-between items-center mb-0.5">
               <span className="text-[10px] font-bold uppercase tracking-widest" style={{ color: 'var(--cyan)' }}>Spettro</span>
               <span className="text-[9px] font-mono" style={{ color: 'var(--text-tertiary)' }}>φ {correlation.toFixed(2)}</span>
             </div>
            {/* Senza spettro non si disegna niente. Prima il ripiego era
                un'onda animata con Math.sin(Date.now()): a plugin scollegato
                o in silenzio sembrava che stesse passando audio. */}
            <div className="h-10 flex items-end gap-[1px] rounded-sm overflow-hidden">
              {spectrum.length > 0 ? spectrum.slice(0, 96).map((mag, i) => (
                <motion.div
                  key={i}
                  className="flex-1 rounded-[0.5px]"
                  style={{
                    backgroundColor: `hsl(${140 + (1 - mag) * 80}, 80%, ${25 + mag * 40}%)`,
                  }}
                  animate={{ height: `${Math.max(4, mag * 100)}%` }}
                  transition={{ duration: 0.06 }}
                />
              )) : (
                <div className="w-full h-px self-center" style={{ backgroundColor: 'var(--border)' }} />
              )}
            </div>
<div className="flex justify-between text-[8px] font-mono mt-0.5" style={{ color: 'var(--text-dim)' }}>
               <span>20Hz</span><span>200Hz</span><span>2kHz</span><span>20kHz</span>
             </div>
           </div>

           {/* ── FLIGHT RECORDER ────────────────────────────────────── */}
           <div className="flex-[2] flex flex-col overflow-hidden border-r min-w-[200px]" style={{ borderColor: 'var(--border)' }}>
             <div className="flex items-center justify-between px-4 py-1.5 border-b" style={{ borderColor: 'var(--border-subtle)', backgroundColor: 'var(--bg-primary)' }}>
               <div className="flex items-center gap-2">
                 <div className="w-1.5 h-1.5 rounded-full bg-[#DC143C]" />
                 <span className="text-[11px] font-bold uppercase tracking-widest" style={{ color: 'var(--cremisi)' }}>Registro di sessione</span>
               </div>
               <button onClick={() => setActiveTab('SESSIONS')}
                 className="text-[10px] hover:text-[#FFB000] transition-colors uppercase tracking-wider font-medium" style={{ color: 'var(--text-secondary)' }}>
                 Registro completo →
               </button>
             </div>
            <div className="flex-1 overflow-y-auto flex flex-col-reverse px-3 py-2 gap-1.5 custom-scrollbar">
              {[...messages].reverse().filter(m => m.type !== 'advisory').slice(0, 6).map(m => (
                <div key={m.id}
                  className="flex items-start gap-2.5 text-xs font-mono opacity-70 hover:opacity-100 transition-opacity group cursor-default"
                >
                  <div className="flex-shrink-0 mt-0.5 w-4 flex justify-center">
                    <div className={`w-2 h-2 rounded-sm ${
                      m.type === 'system' ? 'bg-[#555]' :
                      m.type === 'user' ? 'bg-[#FFB000]' : 'bg-[#00CFFF]'
                    }`} />
                  </div>
                  <div className="flex-1 min-w-0">
<div className="flex items-center gap-2">
                       <span className="text-[10px] leading-tight truncate" style={{ color: 'var(--text-secondary)' }}>
                         {m.text?.slice(0, 80)}
                       </span>
                       {m.time && (
                         <span className="text-[9px] flex-shrink-0 font-medium" style={{ color: 'var(--text-dim)' }}>{m.time}</span>
                       )}
                     </div>
                     <div className="flex gap-2 text-[8px] uppercase tracking-wider mt-0.5" style={{ color: '#444' }}>
                       <span className={
                         m.type === 'system' ? 'text-[#555]' :
                         m.type === 'user' ? 'text-[#FFB000]/60' : 'text-[#00CFFF]/60'
                       }>{m.type}</span>
                     </div>
                   </div>
                 </div>
               ))}
               {messages.filter(m => m.type !== 'advisory').length === 0 && (
                 <div className="flex items-center justify-center h-full">
                   <div className="text-center">
                     <div className="text-[10px] font-mono italic" style={{ color: 'var(--text-dim)' }}>No events recorded</div>
                     <div className="text-[8px] font-mono mt-1" style={{ color: '#444' }}>Interact with the AI to begin logging</div>
                   </div>
                 </div>
               )}
             </div>
           </div>

           {/* ── AI ACTION LOG + UNDO/REDO ────────────────────────── */}
           <div className="w-40 flex-shrink-0 flex flex-col justify-center px-3 py-2 gap-1 overflow-hidden" style={{ backgroundColor: 'var(--bg-primary)' }}>
             <div className="flex items-center justify-between mb-1">
               <span className="text-[9px] font-bold uppercase tracking-widest" style={{ color: 'var(--green)' }}>Azioni</span>
               <div className="flex gap-0.5">
                 <motion.button
                   className="text-[11px] px-1.5 py-0.5 border"
                   style={{ 
                     color: actionHistory.length > 0 ? 'var(--text-primary)' : '#333',
                     borderColor: actionHistory.length > 0 ? '#444' : 'var(--border)'
                   }}
                   onClick={undoLastAction}
                   disabled={actionHistory.length === 0}
                   whileTap={{ scale: 0.9 }}
                 >↩</motion.button>
                 <motion.button
                   className="text-[11px] px-1.5 py-0.5 border"
                   style={{ 
                     color: actionRedoStack.length > 0 ? 'var(--text-primary)' : '#333',
                     borderColor: actionRedoStack.length > 0 ? '#444' : 'var(--border)'
                   }}
                   onClick={redoLastAction}
                   disabled={actionRedoStack.length === 0}
                   whileTap={{ scale: 0.9 }}
                 >↪</motion.button>
               </div>
             </div>
             <div className="flex-1 overflow-y-auto max-h-24 space-y-0.5 custom-scrollbar">
               {actionLog.length === 0 ? (
                 <div className="text-[8px] italic" style={{ color: 'var(--text-dim)' }}>Nessuna azione ancora</div>
               ) : (
                 actionLog.slice(0, 6).map((a, i) => (
                   <div key={a.timestamp + '-' + i} className="text-[9px] font-mono flex justify-between items-center group px-1 -mx-1 rounded-sm transition-colors">
                     <span className="truncate mr-1 max-w-[70%]" style={{ color: 'var(--text-secondary)' }}>{a.description || a.widgetId}</span>
                     <span className="flex-shrink-0 font-bold" style={{ color: 'var(--amber)' }}>{a.value?.toFixed(2)}</span>
                   </div>
                 ))
               )}
             </div>
           </div>
         </section>
       </main>

{/* ── DEBUG OVERLAY ─────────────────────────────────────────── */}
      <AnimatePresence>
        {debugOverlay && (
          <motion.div
            initial={{ opacity: 0, y: -10 }}
            animate={{ opacity: 1, y: 0 }}
            exit={{ opacity: 0, y: -10 }}
            className="fixed top-9 right-2 z-[300] border rounded text-[9px] font-mono leading-relaxed p-2 min-w-[160px]"
            style={{ backgroundColor: 'var(--bg-card)', borderColor: 'var(--border)', color: 'var(--text-secondary)' }}
          >
            <div className="font-bold text-[10px] mb-1 uppercase tracking-wider" style={{ color: 'var(--amber)' }}>Debug</div>
            <div className="flex justify-between"><span>FPS</span><span style={{ color: fps < 30 ? 'var(--cremisi)' : fps < 50 ? 'var(--amber)' : 'var(--green)' }}>{fps}</span></div>
            <div className="flex justify-between"><span>Latency</span><span style={{ color: 'var(--cyan)' }}>{whycremisi.getLatencyMs ? whycremisi.getLatencyMs() : 0}ms</span></div>
            <div className="flex justify-between"><span>Avg Lat</span><span style={{ color: 'var(--text-faint)' }}>{whycremisi.getAvgLatencyMs ? whycremisi.getAvgLatencyMs().toFixed(0) : 0}ms</span></div>
            <div className="flex justify-between"><span>Max Lat</span><span style={{ color: 'var(--text-faint)' }}>{whycremisi.getMaxLatencyMs ? whycremisi.getMaxLatencyMs() : 0}ms</span></div>
            <div className="flex justify-between"><span>Queue</span><span style={{ color: 'var(--amber)' }}>{whycremisi.getConnectionInfo ? whycremisi.getConnectionInfo().queueSize : 0}</span></div>
            <div className="flex justify-between"><span>Retries</span><span style={{ color: 'var(--text-faint)' }}>{whycremisi.getConnectionInfo ? `${whycremisi.getConnectionInfo().reconnectAttempts}/${whycremisi.getConnectionInfo().maxReconnectAttempts}` : '0/0'}</span></div>
            <div className="flex justify-between"><span>Uptime</span><span style={{ color: 'var(--text-faint)' }}>
              {(() => {
                const info = whycremisi.getConnectionInfo ? whycremisi.getConnectionInfo() : {}
                const s = Math.floor((info.uptime || 0) / 1000)
                return `${Math.floor(s / 60)}m ${s % 60}s`
              })()}
            </span></div>
            <div className="flex justify-between"><span>Messages</span><span style={{ color: 'var(--text-muted)' }}>{messages.length}</span></div>
            <div className="flex justify-between"><span>Conn</span><span style={{ color: connStatus === 'connected' ? 'var(--green)' : 'var(--cremisi)' }}>{connStatus}</span></div>
            <div className="mt-1 pt-1 border-t" style={{ borderColor: 'var(--border)' }}>
              <div className="text-[8px] text-center" style={{ color: 'var(--text-muted)' }}>Shift+D per chiudere</div>
            </div>
          </motion.div>
        )}
      </AnimatePresence>

{/* ── FOOTER ─────────────────────────────────────────────────── */}
       <footer className="fixed bottom-0 left-0 w-full z-50 flex justify-between items-center h-9 border-t px-5" style={{ borderColor: 'var(--border)', backgroundColor: 'var(--bg-primary)' }}>
         {/* Le cifre vengono dal campione lento e sono a larghezza fissa:
             prima si aggiornavano trenta volte al secondo e cambiavano
             anche larghezza, quindi tremolavano e si spostavano di lato. */}
         <div className="flex items-center gap-5 font-mono" style={{ fontSize: 'var(--t-micro)' }}>
           {[
             { icon:'equalizer', etichetta:'LUFS', valore: letture.lufs > -60 ? letture.lufs.toFixed(1) : '−∞', larghezza:'3.2em', color:'var(--amber)' },
             { icon:'priority_high', etichetta:'Picco', valore: letture.peak > -60 ? `${letture.peak.toFixed(1)} dB` : '−∞', larghezza:'4.4em', color: letture.peak > -1 ? 'var(--cremisi)' : 'var(--text-muted)' },
             { icon:'speed', etichetta:'L / R', valore: `${Math.round(letture.meterL*100)}% · ${Math.round(letture.meterR*100)}%`, larghezza:'5.4em', color:'var(--text-muted)' }
           ].map(({ icon, etichetta, valore, larghezza, color }) => (
             <div key={etichetta} className="flex items-center gap-1.5" style={{ color }}>
               <span className="material-symbols-outlined text-sm" aria-hidden="true">{icon}</span>
               <span className="etichetta" style={{ color: 'var(--text-muted)' }}>{etichetta}</span>
               <span className="num" style={{ display: 'inline-block', minWidth: larghezza }}>{valore}</span>
             </div>
           ))}
         </div>

          <div className="flex items-center gap-2">
            <button onClick={exportSession}
              className="text-[9px] font-mono uppercase tracking-wider px-2 py-0.5 border rounded-sm transition-colors"
              style={{ color: 'var(--text-muted)', borderColor: 'var(--border)' }}
              title="Esporta la sessione in JSON"
            >Esporta</button>
            <button onClick={importSession}
              className="text-[9px] font-mono uppercase tracking-wider px-2 py-0.5 border rounded-sm transition-colors"
              style={{ color: 'var(--text-muted)', borderColor: 'var(--border)' }}
              title="Import session from JSON"
            >Importa</button>
          </div>
          <div className="flex items-center gap-4">
            <motion.span className="text-xs font-mono uppercase tracking-widest"
              style={{ color: 'var(--amber)' }}
              animate={{ opacity:[0.7,1,0.7] }}
              transition={{ repeat:Infinity, duration:2.5 }}
            >
              {connStatus === ConnectionState.CONNECTED ? '◉ Collegato' : '◌ Non collegato'}
            </motion.span>
           <motion.div
             className="h-3 w-3"
             style={{ backgroundColor: connStatus === ConnectionState.CONNECTED ? '#FFB000' : 'var(--text-muted)' }}
             animate={connStatus === ConnectionState.CONNECTED
               ? { boxShadow:['0 0 6px #FFB000','0 0 14px #FFB000','0 0 6px #FFB000'] } : {}}
             transition={{ repeat:Infinity, duration:1.8 }}
           />
         </div>
       </footer>
    </div>
  )
}
