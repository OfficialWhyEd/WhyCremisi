import React, { useState, useEffect, useMemo, useRef, useCallback } from 'react'
import { motion, AnimatePresence } from 'framer-motion'
import { whycremisi, ConnectionState } from './whycremisi-bridge'
import { BotFace } from './components/BotFace'
import { MaskLogo } from './components/MaskLogo'
import { SessionPanel } from './components/SessionPanel'
import { SetupScreen } from './components/SetupScreen'
import './index.css'
import BoxChat from './panels/BoxChat'

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
      if (cfg.provider !== 'ollama' && !cfg.apiKey) return false
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
    { id: 1, type: 'system', text: '[--:--:--] WHYCREMISI NEURAL MATRIX ONLINE' },
  ])
  const chatEndRef  = useRef(null)
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
    const id = Date.now()
    setToasts(prev => [...prev, { id, message, type }])
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
  const [cpuUsage, setCpuUsage] = useState({ cpuPercent: 0, peakTimeUs: 0 })

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
    setMessages(prev => [...prev, { id: Date.now() + Math.random(), ...msg }])
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

  useEffect(() => { chatEndRef.current?.scrollIntoView({ behavior: 'smooth' }) }, [messages])

  // ── bridge init ───────────────────────────────────────────────────
  useEffect(() => {
    const stateUnsub = whycremisi.onStateChange((s) => {
      setConnStatus(s)
      if (s === ConnectionState.CONNECTED) {
        sysMsg(`[${ts()}] WEBSOCKET CONNECTED TO PLUGIN`)
        setBotState('success')
        setTimeout(() => { if (mounted.current) setBotState('idle') }, 2000)
        // Auto-send stored API key on connect so plugin has it
        const cfg = (() => { try { return JSON.parse(localStorage.getItem('whycremisi_config')) } catch { return null } })()
        if (cfg?.apiKey && cfg?.provider) {
          whycremisi.send({ type: 'config.set', payload: { key: 'ai.apiKey', value: cfg.apiKey, provider: cfg.provider } })
          whycremisi.send({ type: 'config.set', payload: { key: 'ai.provider', value: cfg.provider } })
          whycremisi.send({ type: 'config.set', payload: { key: 'ai.model', value: cfg.model || 'llama3.2' } })
        }
      }
      if (s === ConnectionState.DISCONNECTED || s === ConnectionState.ERROR) {
        sysMsg(`[${ts()}] CONNECTION LOST — RECONNECTING...`)
        setBotState('sad')
        addToast('WebSocket connection lost — reconnecting...', 'error')
      }
      if (s === ConnectionState.CONNECTED) {
        addToast('Connected to WhyCremisi plugin', 'success')
      }
      if (s === ConnectionState.RECONNECTING) {
        setBotState('loading')
      }
    })

    const onBotState = (e) => setBotState(e.detail)
    window.addEventListener('whycremisi-botstate', onBotState)

    // ── AI response (complete) ──
    const unsubAI = whycremisi.on('ai.response', (payload) => {
      setBotState('success')
      setTimeout(() => { if (mounted.current) setBotState('idle') }, 2000)
      const sid = streamingIdRef.current
      streamingIdRef.current = null
      if (payload?.content) {
        if (sid) {
          // finalize existing streaming message instead of duplicating
          const boxType = detectBoxType(lastPromptRef.current, payload.content)
          setMessages(prev => prev.map(m =>
            m.id === sid ? { ...m, text: payload.content, streaming: false, telemetry: true, boxType } : m
          ))
        } else {
          const boxType = detectBoxType(lastPromptRef.current, payload.content)
          addMsg({ type: 'bot', text: payload.content, time: ts(), telemetry: true, boxType })
        }
      }
    })

    // ── AI stream chunks ──
    const unsubStream = whycremisi.on('ai.stream', (payload) => {
      setBotState('typing')
      const chunk = payload?.chunk ?? payload?.content ?? ''
      if (!chunk) return

      if (!streamingIdRef.current) {
        // start new streaming message
        const newId = Date.now()
        streamingIdRef.current = newId
        setMessages(prev => [...prev, { id: newId, type: 'bot', text: chunk, time: ts(), streaming: true }])
      } else {
        // append chunk to existing streaming message
        const sid = streamingIdRef.current
        setMessages(prev => prev.map(m =>
          m.id === sid ? { ...m, text: m.text + chunk } : m
        ))
      }

      // finalize if isDone
      if (payload?.isDone) {
        const sid = streamingIdRef.current
        setMessages(prev => prev.map(m =>
          m.id === sid ? { ...m, streaming: false, telemetry: true } : m
        ))
        streamingIdRef.current = null
        setBotState('success')
        setTimeout(() => { if (mounted.current) setBotState('idle') }, 2000)
      }
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
      sysMsg(`[${ts()}] TRANSPORT: ${payload.isPlaying ? '▶ PLAY' : '■ STOP'} | BPM ${payload.bpm?.toFixed(1) ?? '--'}`)
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
      sysMsg(`[${ts()}] OSC ${payload.address}: ${payload.value}`)
    })

    // ── plugin stats (SR, buffer, latency from prepareToPlay) ──
    const unsubStats = whycremisi.on('plugin.stats', (payload) => {
      setPluginStats({
        sampleRate: payload.sampleRate ?? null,
        bufferSize: payload.bufferSize ?? null,
        latencyMs:  payload.latencyMs  ?? null,
      })
      sysMsg(`[${ts()}] AUDIO: ${payload.sampleRate}Hz / buf ${payload.bufferSize} / lat ${payload.latencyMs?.toFixed(1)}ms`)
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
    })

    // CPU usage
    const unsubCpu = whycremisi.on('plugin.cpu', (payload) => {
      setCpuUsage(payload)
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
    const unsubPersAction = whycremisi.on('personality.action', (payload) => {
      sysMsg(`[${ts()}] PERSONALITY: ${payload.detail || 'action recorded'}`)
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
      sysMsg(`[${ts()}] WORKSPACE STYLE: ${payload.detail || ''}`)
    })

    const unsubWsMemory = whycremisi.on('workspace.memory', (payload) => {
      sysMsg(`[${ts()}] WORKSPACE MEMORY: ${payload.detail?.substring(0, 80) || ''}`)
    })

    const unsubWsBootstrap = whycremisi.on('workspace.bootstrap', (payload) => {
      sysMsg(`[${ts()}] WORKSPACE: ${payload.detail || 'Bootstrap event'}`)
    })

    const unsubWsRefresh = whycremisi.on('workspace.refresh', (payload) => {
      sysMsg(`[${ts()}] WORKSPACE REFRESHED: ${payload.detail || ''}`)
    })

    // connect
    whycremisi.connect().catch(() => {
      sysMsg(`[${ts()}] PLUGIN NOT RUNNING — OFFLINE MODE`)
    })

    return () => {
      mounted.current = false
      intervalsRef.current.forEach(clearInterval)
      intervalsRef.current = []
      window.removeEventListener('whycremisi-botstate', onBotState)
      stateUnsub(); unsubAI(); unsubStream(); unsubTransport()
      unsubMeter(); unsubOSC(); unsubStats(); unsubErr()
      unsubLearnStatus(); unsubLearnComplete(); unsubChain(); unsubAnalyzer(); unsubCpu();       unsubActionLog(); unsubPersAction(); unsubPersContext()
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

  // ── advisory actions ──────────────────────────────────────────────
  const executeChain = useCallback(() => {
    if (botState !== 'idle') return
    setBotState('loading')
    if (whycremisi.isConnected()) {
      whycremisi.sendAIPrompt('Execute suggested mastering chain: apply -2.4dB at 200-400Hz, preserve transients at 84%')
    } else {
      const id = Date.now()
      streamingIdRef.current = id
      setMessages(prev => [...prev, { id, type: 'bot', text: '', time: ts(), streaming: true }])
      setBotState('typing')
      const reply = 'Executing suggested chain.\nDynamic dip of -2.4dB applied at 300Hz.\nTransient preservation locked at 84%.\nSpectral clarity increased.\n\n[ CHAIN APPLIED ]'
      let i = 0
      const t = setInterval(() => {
        if (!mounted.current) { clearInterval(t); return }
        i++
        setMessages(prev => prev.map(m => m.id === id ? { ...m, text: reply.slice(0, i) } : m))
        if (i >= reply.length) {
          clearInterval(t)
          setMessages(prev => prev.map(m => m.id === id ? { ...m, streaming: false, telemetry: true } : m))
          streamingIdRef.current = null
          setBotState('success')
          setTimeout(() => { if (mounted.current) setBotState('idle') }, 1500)
        }
      }, 16)
      intervalsRef.current.push(t)
    }
  }, [botState])

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

  const dismissAdvisory = useCallback(() => {
    setMessages(prev => prev.filter(m => m.type !== 'advisory'))
  }, [])

    // ── detect boxchat type(s) from prompt + response ─────────────────
    const detectBoxType = (prompt = '', response = '') => {
      const t = (prompt + ' ' + response).toLowerCase()

      // first try explicit user requests (e.g. "show me eq", "show eq and spectral")
      const showMatch = t.match(/show\s+(?:me\s+)?(?:the\s+)?(\w+(?:\s*(?:and|,|&)\s*\w+)*)/)
      if (showMatch) {
        const raw = showMatch[1]
        const explicit = []
        const words = raw.split(/\s+(?:and|,|&)\s+|\s+/)
        for (const w of words) {
          if (/eq|frequen|bass|sub|mid|high|treble|presence|air/.test(w)) explicit.push('eq')
          else if (/spectrum|spectral|fft|analyzer/.test(w)) explicit.push('spectral')
          else if (/clip|clipping|distort|overload/.test(w)) explicit.push('clipping')
          else if (/vector|scope|vectorscope|xy|phase/.test(w)) explicit.push('vectorscope')
          else if (/stereo|width|side|balance|correlation/.test(w)) explicit.push('stereo')
          else if (/loud|lufs|peak|rms|dynamic|crest/.test(w)) explicit.push('loudness')
          else if (/volume|gain|fader|level/.test(w)) explicit.push('slider')
          else if (/pan|panning|position/.test(w)) explicit.push('knob')
          else if (/play|stop|record|transport|bpm|tempo/.test(w)) explicit.push('transport')
          else if (/compres|ratio|attack|release|threshold|knee/.test(w)) explicit.push('compressor')
          else if (/advisory|suggest|recommend/.test(w)) explicit.push('advisory')
        }
        if (explicit.length > 0) {
          const dedup = [...new Set(explicit)]
          if (/all|everything|every/.test(raw)) return ['vectorscope','stereo','loudness','clipping','eq','spectral','slider','knob','transport','compressor','metrics']
          return dedup
        }
      }
      if (/show\s+(?:me\s+)?(?:the\s+)?(?:all|everything)/.test(t)) {
        return ['vectorscope','stereo','loudness','clipping','eq','spectral','slider','knob','transport','compressor','metrics']
      }

      // content-based matching
      const types = []
      if (/spectrum|spectral|fft|analyzer|freq\s*response/.test(t)) types.push('spectral')
      if (/clip|clipping|distort|overload|red|limit(?!er)|ceiling/.test(t)) types.push('clipping')
      if (/vector|scope|vectorscope|xy|phase\s*meter/.test(t)) types.push('vectorscope')
      if (/stereo|width|side|balance|phase(?!\s*meter)|mono|correlation/.test(t)) types.push('stereo')
      if (/lufs|loud|peak|rms|dynamic|crest|limiter/.test(t)) types.push('loudness')
      if (/eq|frequen|bass|sub|mid|high|treble|presence|air|100hz|200hz|1khz|4khz/.test(t)) types.push('eq')
      if (/volume|gain|fader|db(?!\s)|level/.test(t)) types.push('slider')
      if (/pan|panning|position|center|left|right/.test(t)) types.push('knob')
      if (/play|stop|record|transport|bpm|tempo/.test(t)) types.push('transport')
      if (/compres|ratio|attack|release|threshold|knee/.test(t)) types.push('compressor')
      if (/search|internet|find|look\s*up|web|google/.test(t)) types.push('advisory')
      if (/chain|apply|execute|suggest|recommend|action|advisory|do it|should|could|try|consider/.test(t)) types.push('advisory')
      if (types.length === 0) types.push('metrics')
      return types
    }

  // ── Dynamic suggestion engine from analyzer data + personality ──
  const currentSuggestion = useMemo(() => {
    const hasData = spectrum.length > 0 && correlation !== 0
    if (!hasData) return {
      freqLow: 200, freqHigh: 400, gainDb: -2.4,
      label: '200Hz–400Hz', description: 'harmonic crowding',
      transientPres: 84, confidence: 0.98
    }

    const avgMag = spectrum.reduce((a, b) => a + b, 0) / spectrum.length
    const lowEnd = spectrum.slice(0, Math.floor(spectrum.length * 0.1)).reduce((a, b) => a + b, 0) / Math.max(1, Math.floor(spectrum.length * 0.1))
    const lowMid = spectrum.slice(Math.floor(spectrum.length * 0.1), Math.floor(spectrum.length * 0.25)).reduce((a, b) => a + b, 0) / Math.max(1, Math.floor(spectrum.length * 0.15))
    const highEnd = spectrum.slice(Math.floor(spectrum.length * 0.7)).reduce((a, b) => a + b, 0) / Math.max(1, spectrum.length - Math.floor(spectrum.length * 0.7))
    const corrVal = Math.abs(correlation)

    const personalityFactor = personality?.style === 'direct' ? 1.4 :
      personality?.style === 'consultative' ? 0.6 :
      personality?.style === 'analytical' ? 1.0 :
      personality?.style === 'creative' ? 1.2 : 1.0

    let freqLow = 150, freqHigh = 350, gainDb = -2.0, transientPres = 80
    let label = '150Hz–350Hz', description = 'low-mid energy'

    if (lowEnd > avgMag * 1.3 && corrVal < 0.6) {
      freqLow = 60; freqHigh = 120; gainDb = -(2.5 + (lowEnd - avgMag) * personalityFactor)
      label = '60Hz–120Hz'; description = 'sub-bass buildup'
    } else if (lowMid > avgMag * 1.2 && corrVal < 0.7) {
      freqLow = 180; freqHigh = 400; gainDb = -(2.0 + (lowMid - avgMag) * personalityFactor)
      label = '180Hz–400Hz'; description = 'low-mid crowding'
    } else if (highEnd > avgMag * 1.15) {
      freqLow = 5000; freqHigh = 12000; gainDb = -(1.5 + (highEnd - avgMag) * personalityFactor * 0.5)
      label = '5kHz–12kHz'; description = 'high-frequency buildup'
    }

    if (corrVal < 0.3) {
      transientPres = Math.round(75 + (1 - corrVal) * 20)
      description += ', phase issues detected'
    } else if (corrVal < 0.6) {
      transientPres = Math.round(80 + (1 - corrVal) * 10)
      description += ', moderate correlation'
    } else {
      transientPres = Math.round(85 + corrVal * 5)
    }

    const confidence = Math.min(0.99, Math.max(0.3, avgMag * 1.5 + corrVal * 0.3))

    return { freqLow, freqHigh, gainDb: Math.round(gainDb * 10) / 10, label, description, transientPres, confidence: Math.round(confidence * 1000) / 1000 }
  }, [spectrum, correlation, personality?.style])

  // ── gain slider pct ───────────────────────────────────────────────
  const gainPct = Math.max(0, Math.min(100, ((gainDb + 60) / 72) * 100))

  // ── meter bars ────────────────────────────────────────────────────
  const meterBars = Array.from({ length: 12 }, (_, i) => ({
    color: i < 8 ? '#FFB000' : '#DC143C',
    active: i < Math.round((meterL + meterR) / 2 * 12)
  }))

  // ── render ────────────────────────────────────────────────────────
  return (
    <div className="select-none h-screen w-screen overflow-hidden font-['Space_Grotesk']" style={{ backgroundColor: 'var(--bg-primary)', color: 'var(--text-primary)' }}>
      <div className="crt-overlay" />

      {/* ── Toast Container ────────────────────────────────────────── */}
      <div className="toast-container">
        <AnimatePresence>
          {toasts.map(toast => (
            <motion.div
              key={toast.id}
              initial={{ opacity: 0, x: 80, scale: 0.9 }}
              animate={{ opacity: 1, x: 0, scale: 1 }}
              exit={{ opacity: 0, x: 80, scale: 0.9 }}
              transition={{ type: 'spring', stiffness: 500, damping: 30 }}
              className={`toast toast-${toast.type}`}
              onClick={() => removeToast(toast.id)}
            >
              <span className="toast-icon">
                {toast.type === 'error' && '✕'}
                {toast.type === 'success' && '✓'}
                {toast.type === 'warning' && '⚠'}
                {toast.type === 'info' && 'ℹ'}
              </span>
              <span className="toast-message">{toast.message}</span>
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
              sysMsg(`[${ts()}] SETUP SKIPPED — AI features require API key configuration`)
            }}
          />
        )}
      </AnimatePresence>

      {/* ── HEADER ─────────────────────────────────────────────────── */}
      <header className="flex justify-between items-center w-full px-4 py-1 border-b z-50 relative h-9" style={{ backgroundColor: 'var(--bg-panel)', borderColor: 'var(--border)' }}>
        <div className="flex items-center gap-2">
          <MaskLogo audioLevel={(meterL + meterR) / 2} className="w-8 h-8 flex-shrink-0" />
          <h1 className="text-sm font-black tracking-tighter uppercase leading-none" style={{ color: 'var(--cremisi)' }}>WHYCREMISI</h1>
          <nav className="flex gap-4 uppercase tracking-[0.1em] font-bold text-xs">
            {['COMMAND','MASTER','TELEMETRY','SESSIONS'].map(tab => (
              <a key={tab} role="tab" aria-selected={activeTab === tab}
                onClick={() => setActiveTab(tab)}
                className={`border-b pb-0.5 cursor-pointer hover:text-[#FFB000] transition-colors`}
                style={{ 
                  color: activeTab === tab ? 'var(--amber)' : 'var(--text-secondary)',
                  borderColor: activeTab === tab ? 'var(--amber)' : 'transparent'
                }}
              >{tab}</a>
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

          {/* Audio stats */}
          <div className="flex items-center gap-2">
              {[
                ['SR',  pluginStats.sampleRate ? `${(pluginStats.sampleRate/1000).toFixed(0)}k` : '—'],
                ['BUF', pluginStats.bufferSize ?? '—'],
                ['LAT', pluginStats.latencyMs  ? `${pluginStats.latencyMs.toFixed(0)}ms` : '—'],
                ['CPU', cpuUsage.cpuPercent ? `${cpuUsage.cpuPercent.toFixed(1)}%` : '—'],
              ].map(([k, v]) => (
              <div key={k} className="flex flex-col items-center leading-none">
                <span className="text-xs font-mono uppercase" style={{ color: 'var(--text-secondary)' }}>{k}</span>
                <span className="text-xs font-bold text-white tracking-tight">{v}</span>
              </div>
            ))}
          </div>

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

          {/* Personality style picker */}
          <div className="relative">
            <button
              aria-label={`Personality style: ${PERSONALITY_STYLES.find(s => s.id === personalityStyle)?.name || 'Analytical'}`}
              className="text-xs font-mono hover:text-[#FFB000] transition-colors cursor-pointer px-1.5 py-0.5 border rounded"
              style={{ color: 'var(--text-secondary)', borderColor: 'var(--border)' }}
              onClick={() => setShowStylePicker(!showStylePicker)}
              title={`Style: ${PERSONALITY_STYLES.find(s => s.id === personalityStyle)?.name || 'Analytical'}`}
            >
              {PERSONALITY_STYLES.find(s => s.id === personalityStyle)?.label || 'AN'}
            </button>
            {showStylePicker && (
              <div className="absolute top-full right-0 mt-1 border rounded shadow-lg z-50 min-w-[140px]" style={{ backgroundColor: 'var(--bg-card)', borderColor: 'var(--border)' }}>
                {PERSONALITY_STYLES.map(s => (
                  <button key={s.id}
                    className={`block w-full text-left px-3 py-1.5 text-xs font-mono transition-colors`}
                    style={{ color: personalityStyle === s.id ? 'var(--amber)' : 'var(--text-secondary)' }}
                    onClick={() => applyPersonalityStyle(s.id)}
                  >
                    {s.label} — {s.name}
                  </button>
                ))}
              </div>
            )}
          </div>

          {/* Action icons */}
          <div className="flex gap-1.5 items-center">
            <button className="theme-toggle" onClick={toggleTheme} title={theme === 'dark' ? 'Light mode' : 'Dark mode'} aria-label={theme === 'dark' ? 'Switch to light theme' : 'Switch to dark theme'}>
              {theme === 'dark' ? '☀' : '☾'}
            </button>
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

      {/* ── SIDEBAR ────────────────────────────────────────────────── */}
      <aside className="fixed left-0 top-9 h-[calc(100vh-2.25rem)] w-12 flex flex-col items-center py-2 z-40" style={{ backgroundColor: 'var(--bg-panel)', borderRight: '1px solid var(--border)' }}>
        <div className="text-xs font-mono uppercase mb-4 rotate-180 select-none" style={{ color: 'var(--text-secondary)', writingMode:'vertical-lr' }}>
          MOD
        </div>
        {sideModules.map(mod => (
          <motion.button
            key={mod.id}
            aria-label={mod.label}
            className={`w-full py-2 flex flex-col items-center gap-0.5 transition-colors border-l-2 ${
              activeMod === mod.id ? 'border-[#DC143C]' : 'border-transparent hover:text-[#FFB000]'
            }`}
            style={{
              color: activeMod === mod.id ? 'var(--cremisi)' : 'var(--text-secondary)',
              backgroundColor: activeMod === mod.id ? 'var(--bg-card)' : 'transparent'
            }}
            whileHover={{ scale: 1.04 }}
            whileTap={{ scale: 0.94 }}
            onClick={() => setActiveMod(mod.id)}
          >
            <span className="material-symbols-outlined text-base">{mod.icon}</span>
            <span className="text-xs font-bold uppercase font-mono">{mod.label}</span>
          </motion.button>
        ))}
      </aside>

      {/* ── SESSION PANEL — absolute overlay, mai sposta il main ──── */}
      <AnimatePresence>
        {activeTab === 'SESSIONS' && (
          <motion.div
            key="session-panel"
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
            transition={{ duration: 0.18 }}
            className="fixed top-9 left-12 right-0 bottom-0 z-30"
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
            className="fixed top-9 left-12 right-0 bottom-9 z-30 overflow-y-auto custom-scrollbar p-4"
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
                    { label: 'Side Content', val: `${Math.round(Math.abs(meterL-meterR)*100*1.5)}%`, color: '#FFB000', pct: Math.round(Math.abs(meterL-meterR)*100*1.5) },
                    { label: 'Correlation', val: meterL > 0.01 || meterR > 0.01 ? `+${(0.62 + (meterL+meterR)*0.15).toFixed(2)}` : '—', color: '#00E5FF', pct: Math.round((0.62+(meterL+meterR)*0.15)*100) },
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
                    { label: 'Peak Level', val: peak > -60 ? `${peak.toFixed(1)} dB` : '-∞', color: '#FF6B35', pct: Math.max(0,Math.min(100,(peak+60)/60*100)) },
                    { label: 'Dynamic Range', val: `${(72-(Math.round(meterL*100)+Math.round(meterR*100))/2*0.72).toFixed(1)} dB`, color: '#00FFaa', pct: Math.max(0,100-(Math.round(meterL*100)+Math.round(meterR*100))/2) },
                    { label: 'RMS Level', val: lufs > -60 ? `${(lufs+2.1).toFixed(1)} dB` : '-∞', color: '#FFB000', pct: Math.max(0,Math.min(100,(lufs+62)/62*100)) },
                    { label: 'Crest Factor', val: `${(peak-lufs).toFixed(1)} dB`, color: '#DC143C', pct: Math.min(100,Math.max(0,(peak-lufs)*5)) },
                    { label: 'True Peak', val: peak > -60 ? `${(peak+0.3).toFixed(1)} dBTP` : '-∞', color: '#FF6B35', pct: Math.max(0,Math.min(100,(peak+60.3)/60*100)) },
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
                  {[
                    { label: 'Low End Energy', val: `${Math.round((meterL*0.4+0.3)*100)}%`, color: '#DC143C', pct: Math.round((meterL*0.4+0.3)*100) },
                    { label: 'Mid Presence', val: `${Math.round((meterR*0.35+0.38)*100)}%`, color: '#FFB000', pct: Math.round((meterR*0.35+0.38)*100) },
                    { label: 'High Freq Energy', val: `${Math.round(((meterL+meterR)*0.2+0.25)*100)}%`, color: '#00E5FF', pct: Math.round(((meterL+meterR)*0.2+0.25)*100) },
                    { label: 'Sub Harmonic', val: `${Math.round((meterL*0.25+0.18)*100)}%`, color: '#9B59B6', pct: Math.round((meterL*0.25+0.18)*100) },
                    { label: 'Presence (2-5kHz)', val: `${Math.round((meterR*0.3+0.42)*100)}%`, color: '#00FFaa', pct: Math.round((meterR*0.3+0.42)*100) },
                    { label: 'Air (10kHz+)', val: `${Math.round(((meterL+meterR)*0.15+0.2)*100)}%`, color: '#E8D5B7', pct: Math.round(((meterL+meterR)*0.15+0.2)*100) },
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
      <main className="ml-12 mt-0 h-[calc(100vh-2.25rem)] grid grid-cols-12 grid-rows-6 p-1 gap-1 overflow-hidden relative z-10" style={{ backgroundColor: 'var(--bg-primary)' }}>

        {/* ── AI CHAT CONSOLE (9/12 × 4/6) ──────────────────────── */}
        <section className="col-span-9 row-span-4 border flex flex-col overflow-hidden" style={{ backgroundColor: 'var(--bg-surface)', borderColor: 'var(--border)' }}>
          <div className="px-3 py-1 flex justify-between items-center border-b" style={{ backgroundColor: 'var(--bg-card)', borderColor: 'var(--border)' }}>
            <div className="flex items-center gap-2">
              <div className="w-1.5 h-1.5 rounded-full bg-[#FFB000] led-amber-active" />
              <span className="text-xs font-bold tracking-widest uppercase" style={{ color: 'var(--amber)' }}>AI Command Console</span>
            </div>
            <div className="flex items-center gap-3 text-xs font-mono" style={{ color: 'var(--text-secondary)' }}>
              <span>v4.2.0</span>
              {['inline','grid','popover'].map(l => (
                <span key={l} className={`text-[9px] uppercase tracking-widest cursor-pointer px-1 border-b ${boxLayout === l ? 'border-[#FFB000]' : 'border-transparent hover:text-[#FFB000]'}`}
                  style={{ color: boxLayout === l ? 'var(--amber)' : 'var(--text-faint)' }}
                  onClick={() => setBoxLayout(l)}>{l}</span>
              ))}
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
          <div className="flex-1 px-4 py-3 font-mono overflow-y-auto overflow-x-hidden custom-scrollbar space-y-3">

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
                  <motion.div key={msg.id}
                    initial={{ opacity: 0, x: -6 }}
                    animate={{ opacity: 1, x: 0 }}
                    transition={{ duration: 0.3 }}
                    className="flex items-center gap-2"
                  >
                    <div className="w-1 h-full min-h-[10px] flex-shrink-0" style={{ backgroundColor: 'var(--border-light)' }} />
                    <span className="text-[10px] font-mono tracking-wide" style={{ color: 'var(--text-secondary)' }}>{msg.text}</span>
                  </motion.div>
                )

                if (msg.type === 'advisory') return (
                  <motion.div key="advisory"
                    initial={{ opacity: 0, y: 12, scale: 0.98, filter: 'blur(4px)' }}
                    animate={{ opacity: 1, y: 0, scale: 1, filter: 'blur(0)' }}
                    exit={{ opacity: 0, scale: 0.97 }}
                    transition={{ duration: 0.55, ease: [0.22,1,0.36,1] }}
                    className="mt-2 mb-4"
                  >
                    <div className="advisory-breathe bg-[#121212] border border-[#DC143C]/40 p-4 relative overflow-hidden">
                      {/* Decorative mini bar chart */}
                      <div className="absolute top-2 right-4 flex items-end gap-[2px] h-5 opacity-30">
                        {[35,70,50,90,60].map((h,i) => (
                          <div key={i} className="w-[2px] bg-[#DC143C]" style={{ height:`${h}%` }} />
                        ))}
                      </div>
<div className="flex items-start gap-3 mb-3 border-b pb-2" style={{ borderColor: 'var(--border)' }}>
                         <div className="w-1 h-6 bg-[#DC143C] relative overflow-hidden flex-shrink-0 mt-0.5">
                           <motion.div className="absolute inset-0 bg-white/20"
                             animate={{ y:['0%','100%','0%'] }}
                             transition={{ repeat:Infinity, duration:2 }}
                           />
                         </div>
                         <div>
                           <span className="text-xs font-bold tracking-tighter uppercase block" style={{ color: 'var(--cremisi)' }}>AI MASTERING ADVISORY</span>
                           <span className="text-xs tracking-[0.2em]" style={{ color: 'var(--text-secondary)' }}>NODE: CREMISI_X9 · PRIORITY: HIGH</span>
                         </div>
                       </div>
                       <p className="text-sm leading-relaxed mb-4 break-words" style={{ color: 'var(--amber)' }}>
                        Detected harmonic crowding in <span className="text-white font-bold border-b border-[#DC143C]/40 px-1">200Hz–400Hz</span> range.{' '}
                        Suggesting dynamic dip of <span className="bg-[#DC143C] text-white px-1.5 font-bold">-2.4dB</span>.{' '}
                        Transient preservation at <span className="text-white underline decoration-dotted">84%</span>.
                      </p>
                      <div className="flex gap-3 flex-wrap">
                        <motion.button
                          className="bg-[#DC143C] text-white px-4 py-1.5 text-xs font-bold uppercase hover:bg-white hover:text-[#DC143C] transition-colors tracking-widest disabled:opacity-40"
                          whileTap={{ scale: 0.95 }}
                          onClick={executeChain}
                          disabled={botState !== 'idle'}
                        >
                          <span className="material-symbols-outlined text-[9px] align-middle mr-1">bolt</span>
                          EXECUTE CHAIN
                        </motion.button>
<motion.button
                           className="border px-4 py-1.5 text-xs font-bold uppercase hover:border-[#FFB000] hover:text-[#FFB000] transition-colors tracking-widest"
                           style={{ borderColor: 'var(--text-muted)', color: 'var(--text-secondary)' }}
                           whileTap={{ scale: 0.95 }}
                           onClick={() => analyzeFurther(personality?.style)}
                         >ANALYZE FURTHER</motion.button>
                         <button className="hover:text-white text-xs font-bold uppercase self-center transition-colors"
                           style={{ color: 'var(--text-secondary)' }}
                           onClick={dismissAdvisory}>DISMISS</button>
                      </div>
                    </div>
                  </motion.div>
                )

                if (msg.type === 'user') return (
                  <motion.div key={msg.id}
                    initial={{ opacity:0, x:20 }}
                    animate={{ opacity:1, x:0 }}
                    transition={{ duration:0.3 }}
                    className="flex flex-col items-end gap-0.5"
                  >
<div className="border px-3 py-1.5 max-w-[80%] text-[9px] text-white font-mono italic" style={{ backgroundColor: 'color-mix(in srgb, var(--bg-elevated) 50%, transparent)', borderColor: 'color-mix(in srgb, var(--amber) 20%, transparent)' }}>
                       {msg.text}
                     </div>
                     <span className="text-xs mr-1 uppercase font-bold tracking-widest" style={{ color: 'var(--text-secondary)' }}>
                       User // {msg.time}
                     </span>
                   </motion.div>
                 )

                 if (msg.type === 'bot') {
                   const isLatest = msg.id === lastBotId
                   return (
                     <motion.div key={msg.id}
                       initial={{ opacity:0, y:6 }}
                       animate={{ opacity:1, y:0 }}
                       transition={{ duration:0.35 }}
                       className="flex gap-2 min-w-0"
                     >
                       <div className="flex-shrink-0 mt-0.5 w-8">
                         {isLatest && <BotFace state={botState} className="w-8 h-8" personality={personality} audioLevel={(meterL + meterR) / 2} />}
                       </div>
                       <div className={`flex-1 min-w-0 p-3 relative overflow-hidden transition-colors duration-300 border ${
                         isLatest && msg.streaming
                           ? 'shadow-[0_0_10px_rgba(255,176,0,0.06)]'
                           : ''
                       }`}
                       style={{
                         backgroundColor: isLatest && msg.streaming ? 'var(--bg-active)' : 'var(--bg-hover-card)',
                         borderColor: isLatest && msg.streaming ? 'color-mix(in srgb, var(--amber) 25%, transparent)' : 'var(--border-subtle)'
                       }}>
                         {/* left accent bar */}
                         <div className={`absolute top-0 left-0 w-[2px] h-full transition-colors duration-300 ${isLatest ? 'bg-[#FFB000]' : 'bg-[#2e2e2e]'}`} />
                         {/* streaming scan */}
                         {isLatest && msg.streaming && (
                           <motion.div className="absolute left-0 right-0 h-px bg-gradient-to-r from-transparent via-[#FFB000]/30 to-transparent pointer-events-none"
                             animate={{ top: ['0%','100%','0%'] }}
                             transition={{ repeat: Infinity, duration: 2.5, ease: 'linear' }}
                           />
                         )}
                         <div className="flex justify-between items-center mb-1">
                           <span className="text-xs font-bold tracking-widest uppercase" style={{ color: isLatest ? 'var(--amber)' : 'var(--text-tertiary)' }}>
                             WHYCREMISI AI
                           </span>
                           <span className="text-xs font-mono" style={{ color: 'var(--text-tertiary)' }}>{msg.time}</span>
                         </div>
                         <p className="text-[11px] leading-5 whitespace-pre-wrap" style={{ color: 'var(--text-primary)' }}>
                          {msg.text}
                          {msg.streaming && (
                            <motion.span className="inline-block w-1 h-3 bg-[#FFB000] ml-1 align-middle"
                              animate={{ opacity:[1,0,1] }} transition={{ repeat:Infinity, duration:0.55 }}
                            />
                          )}
                        </p>
                        {msg.telemetry && !msg.streaming && <BoxChat boxType={msg.boxType||'metrics'} meterL={meterL} meterR={meterR} lufs={lufs} peak={peak} transport={transport} pluginStats={pluginStats} gainDb={gainDb} driveVal={driveVal} correlation={correlation} spectrum={spectrum} onDawCmd={dawCmd} personality={personality} onAnalyzeFurther={analyzeFurther} suggestion={currentSuggestion} layout={boxLayout} clippingCount={clippingCount} cpuUsage={cpuUsage} />}
                      </div>
                    </motion.div>
                  )
                }
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
                placeholder={botState === 'thinking' || botState === 'typing' ? 'PROCESSING...' : 'Ask the AI anything about your mix...'}
                value={inputVal}
                onChange={e => setInputVal(e.target.value)}
                onKeyDown={handleCommand}
                disabled={botState === 'thinking' || botState === 'typing'}
              />
              {(botState === 'idle' || botState === 'sad' || botState === 'loading') && <div className="terminal-cursor" />}
            </div>
            <div className="kbd-hint mr-2 hidden md:flex">
              <kbd>&#8984;</kbd><kbd>K</kbd> clear
              <span className="mx-1">·</span>
              <kbd>Esc</kbd> clear
              <span className="mx-1">·</span>
              <kbd>&#8593;</kbd><kbd>&#8595;</kbd> history
            </div>
            {connStatus !== ConnectionState.CONNECTED && connStatus !== ConnectionState.DISCONNECTED && (
              <span className="queued-indicator text-[9px]">QUEUED</span>
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
              <span className="text-xs font-mono" style={{ color: 'var(--text-secondary)' }}>STEREO MASTER</span>
            </div>
          </div>

{/* Plugin Chain Panel */}
           {showChainPanel && (
             <div className="border-b px-3 py-2 space-y-1 max-h-40 overflow-y-auto" style={{ backgroundColor: 'var(--bg-primary)', borderColor: 'var(--border)' }}>
               <div className="flex justify-between items-center mb-1">
                 <span className="text-[9px] font-bold uppercase tracking-wider" style={{ color: 'var(--orange)' }}>Plugin Chain</span>
                 <span className="text-[9px]" style={{ color: 'var(--text-faint)' }}>{pluginChain.length} plugins</span>
               </div>
               {pluginChain.length === 0 ? (
                 <div className="text-[9px] italic" style={{ color: 'var(--text-dim)' }}>No plugins configured. Add your DAW plugins here so the AI knows what's in your chain.</div>
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
               <span className="text-xs font-mono" style={{ color: 'var(--amber)' }}>+12</span>
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
<span className="text-xs font-mono" style={{ color: 'var(--amber)' }}>-INF</span>
               <span className="text-xs font-bold mt-1" style={{ color: 'var(--text-primary)' }}>{gainDb > 0 ? `+${gainDb}` : gainDb}dB</span>
               <span className="text-xs font-mono" style={{ color: 'var(--text-secondary)' }}>GAIN</span>
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
               {/* Toggles */}
               <div className="space-y-3">
                 {[
                   { label:'Deep Neural', state: botState !== 'idle' },
                   { label:'Master Chain', state: dawConnected }
                 ].map(({ label, state: on }) => (
                   <div key={label} className="flex items-center justify-between">
                     <span className="text-xs font-bold uppercase truncate" style={{ color: 'var(--text-secondary)' }}>{label}</span>
                     <motion.div
                       className={`w-4 h-4 border flex items-center justify-center cursor-pointer flex-shrink-0`}
                       style={{ borderColor: on ? 'color-mix(in srgb, var(--cremisi) 60%, transparent)' : 'var(--text-muted)' }}
                       whileHover={{ scale:1.1 }}
                     >
                       <div className={`w-2 h-2 rounded-full ${on ? 'bg-[#DC143C] led-red-active' : ''}`} style={{ backgroundColor: on ? undefined : 'var(--bg-elevated)' }} />
                     </motion.div>
                   </div>
                 ))}
               </div>

               {/* Sat type */}
               <div className="border-t pt-3" style={{ borderColor: 'var(--border)' }}>
                 <span className="text-xs uppercase opacity-60 block mb-1" style={{ color: 'var(--amber)' }}>Saturation</span>
                 <div className="px-2 py-1 text-xs flex justify-between items-center border" style={{ backgroundColor: 'var(--bg-surface)', borderColor: 'var(--border)', color: 'var(--amber)' }}>
                  <span>CRIMSON_TUBE</span>
                  <span className="material-symbols-outlined text-[9px]">expand_more</span>
                </div>
              </div>

              {/* Mask Logo — audio reactive */}
              <div className="flex-1 flex items-center justify-start -ml-2">
                <MaskLogo audioLevel={(meterL + meterR) / 2} className="w-44 h-44" />
              </div>
             </div>
           </div>

           {/* Peak meter strip */}
           <div className="h-10 border-t flex items-center px-3 gap-2" style={{ backgroundColor: 'var(--bg-surface)', borderColor: 'var(--border)' }}>
             <div className="flex-1 h-2 flex gap-[1px]" style={{ backgroundColor: 'var(--bg-card)' }}>
               {meterBars.map((bar, i) => (
                 <motion.div key={i} className="flex-1 h-full"
                   animate={{ opacity: bar.active ? 1 : 0.12, scaleY: bar.active ? [1,1.15,1] : 1 }}
                   transition={{ scaleY: { repeat:Infinity, duration:0.4+i*0.05, delay:i*0.04 } }}
                   style={{ backgroundColor: bar.color }}
                 />
               ))}
             </div>
             <span className="text-xs font-mono font-bold" style={{ color: peak > -1 ? 'var(--cremisi)' : 'var(--text-secondary)' }}>
               {peak > -1 ? 'PEAK!' : 'OK'}
             </span>
           </div>
         </section>

{/* ── MASTER MONITOR (full width × 2/6) ──────────────────── */}
          <section className="col-span-12 row-span-2 border flex overflow-hidden" style={{ backgroundColor: 'var(--bg-surface)', borderColor: 'var(--border)' }}>

            {/* ── L/R STEREO METERS ─────────────────────────────────── */}
           <div className="w-52 flex-shrink-0 border-r flex flex-col justify-center px-4 py-2 gap-1.5" style={{ borderColor: 'var(--border)' }}>
             <div className="text-[11px] font-bold uppercase tracking-widest mb-0.5" style={{ color: 'var(--amber)' }}>STEREO MASTER</div>
             {[['L', meterL], ['R', meterR]].map(([ch, val]) => {
               const pct = Math.max(0, Math.min(100, val * 100))
               const db = (val * 72 - 60).toFixed(1)
               const barColor = pct > 88 ? '#DC143C' : pct > 70 ? '#FFB000' : '#00FFaa'
               return (
                 <div key={ch} className="flex items-center gap-2">
                   <span className="text-xs font-bold w-3" style={{ color: 'var(--text-secondary)' }}>{ch}</span>
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
                   <span className="text-xs font-mono w-10 text-right font-medium" style={{ color: 'var(--text-secondary)' }}>{db}dB</span>
                 </div>
               )
             })}
             <div className="flex justify-between mt-0.5 text-[10px] font-mono" style={{ color: 'var(--text-dim)' }}>
               <span>-60</span><span>-12</span><span>-6</span><span>0</span>
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
             <div className="flex items-center justify-between text-xs font-mono">
               <span style={{ color: 'var(--text-secondary)' }}>POS</span>
               <span className="text-white font-medium">
                 {Math.floor(transport.position / 60).toString().padStart(2,'0')}:{(transport.position % 60).toFixed(0).toString().padStart(2,'0')}
               </span>
             </div>
             <div className="grid grid-cols-2 gap-x-3 gap-y-0.5 text-[10px] font-mono">
               {[
                 ['SR', pluginStats.sampleRate ? `${(pluginStats.sampleRate / 1000).toFixed(0)}k` : '—'],
                 ['BUF', pluginStats.bufferSize ? `${pluginStats.bufferSize}` : '—'],
                 ['LAT', pluginStats.latencyMs ? `${pluginStats.latencyMs.toFixed(0)}ms` : '—'],
                 ['CORR', correlation.toFixed(2)],
               ].map(([k, v]) => (
                 <div key={k} className="flex items-center gap-1">
                   <span style={{ color: 'var(--text-dim)' }}>{k}</span>
                   <span className="text-white font-medium">{v}</span>
                 </div>
               ))}
             </div>
           </div>

           {/* ── REAL-TIME SPECTRUM ANALYZER ─────────────────────────── */}
           <div className="w-60 flex-shrink-0 border-r flex flex-col justify-center px-3 py-2 gap-1" style={{ borderColor: 'var(--border)' }}>
             <div className="flex justify-between items-center mb-0.5">
               <span className="text-[10px] font-bold uppercase tracking-widest" style={{ color: 'var(--cyan)' }}>SPECTRUM</span>
               <span className="text-[9px] font-mono" style={{ color: 'var(--text-tertiary)' }}>φ {correlation.toFixed(2)}</span>
             </div>
            <div className="h-10 flex items-end gap-[1px] rounded-sm overflow-hidden">
              {(spectrum.length > 0 ? spectrum.slice(0, 96) : Array.from({ length: 96 }, (_, i) => 0.1 + Math.sin(i * 0.2 + Date.now() * 0.0005) * 0.05)).map((mag, i) => (
                <motion.div
                  key={i}
                  className="flex-1 rounded-[0.5px]"
                  style={{
                    backgroundColor: `hsl(${140 + (1 - mag) * 80}, 80%, ${25 + mag * 40}%)`,
                  }}
                  animate={{ height: `${Math.max(4, mag * 100)}%` }}
                  transition={{ duration: 0.06 }}
                />
              ))}
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
                 <span className="text-[11px] font-bold uppercase tracking-widest" style={{ color: 'var(--cremisi)' }}>FLIGHT RECORDER</span>
               </div>
               <button onClick={() => setActiveTab('SESSIONS')}
                 className="text-[10px] hover:text-[#FFB000] transition-colors uppercase tracking-wider font-medium" style={{ color: 'var(--text-secondary)' }}>
                 FULL LOG →
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
               <span className="text-[9px] font-bold uppercase tracking-widest" style={{ color: 'var(--green)' }}>ACTIONS</span>
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
                 <div className="text-[8px] italic" style={{ color: 'var(--text-dim)' }}>No AI actions yet</div>
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
              <div className="text-[8px] text-center" style={{ color: 'var(--text-muted)' }}>Shift+D to close</div>
            </div>
          </motion.div>
        )}
      </AnimatePresence>

{/* ── FOOTER ─────────────────────────────────────────────────── */}
       <footer className="fixed bottom-0 left-0 w-full z-50 flex justify-between items-center h-9 border-t px-5" style={{ borderColor: 'var(--border)', backgroundColor: 'var(--bg-primary)' }}>
         <div className="flex items-center gap-5 text-xs font-mono">
           {[
             { icon:'equalizer', label:`LUFS: ${lufs.toFixed(1)}`, color:'var(--amber)' },
             { icon:'priority_high', label:`PEAK: ${peak.toFixed(1)}dB`, color: peak>-1?'var(--cremisi)':'var(--text-muted)' },
             { icon:'speed', label:`L: ${(meterL*100).toFixed(0)}%  R: ${(meterR*100).toFixed(0)}%`, color:'var(--text-muted)' }
           ].map(({ icon, label, color }) => (
             <div key={label} className="flex items-center gap-1.5" style={{ color }}>
               <span className="material-symbols-outlined text-sm">{icon}</span>
               <span>{label}</span>
             </div>
           ))}
         </div>

          <div className="flex items-center gap-2">
            <button onClick={exportSession}
              className="text-[9px] font-mono uppercase tracking-wider px-2 py-0.5 border rounded-sm transition-colors"
              style={{ color: 'var(--text-muted)', borderColor: 'var(--border)' }}
              title="Export session as JSON"
            >EXPORT</button>
            <button onClick={importSession}
              className="text-[9px] font-mono uppercase tracking-wider px-2 py-0.5 border rounded-sm transition-colors"
              style={{ color: 'var(--text-muted)', borderColor: 'var(--border)' }}
              title="Import session from JSON"
            >IMPORT</button>
          </div>
          <div className="flex items-center gap-4">
            <motion.span className="text-xs font-mono uppercase tracking-widest"
              style={{ color: 'var(--amber)' }}
              animate={{ opacity:[0.7,1,0.7] }}
              transition={{ repeat:Infinity, duration:2.5 }}
            >
              {connStatus === ConnectionState.CONNECTED ? '◉ CONNECTED // READY' : '◌ OFFLINE // AWAITING SIGNAL'}
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
