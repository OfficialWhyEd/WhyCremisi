import React, { useState, useEffect, useMemo, useRef, useCallback } from 'react'
import { motion, AnimatePresence } from 'framer-motion'
import { whycremisi, ConnectionState } from './whycremisi-bridge'
import { BotFace } from './components/BotFace'
import { SessionPanel } from './components/SessionPanel'
import './index.css'

export default function App() {
  // ── connection & bot ──────────────────────────────────────────────
  const [connStatus, setConnStatus] = useState(ConnectionState.DISCONNECTED)
  const [botState, setBotState]     = useState('idle')

  // ── messages ──────────────────────────────────────────────────────
  const [messages, setMessages] = useState([
    { id: 1, type: 'system', text: '[--:--:--] INITIALIZING NEURAL MATRIX... OK' },
    { id: 2, type: 'system', text: '[--:--:--] SCANNING SPECTRAL DENSITY... COMPLETE' },
    { id: 3, type: 'system', text: '[--:--:--] OSC BRIDGE LISTENING ON PORT 9000... READY' },
    { id: 4, type: 'advisory' }
  ])
  const chatEndRef  = useRef(null)
  const [inputVal, setInputVal] = useState('')
  const streamingIdRef = useRef(null)  // tracks current streaming message id

  // ── transport / DAW state ─────────────────────────────────────────
  const [transport, setTransport] = useState({ isPlaying: false, isRecording: false, bpm: 120.0, position: 0 })
  const [dawConnected, setDawConnected] = useState(false)

  // ── rack / meters ─────────────────────────────────────────────────
  const [gainDb, setGainDb] = useState(0)       // -60 to +12
  const [driveVal, setDriveVal] = useState(72.5)
  const [meterL, setMeterL] = useState(0.6)
  const [meterR, setMeterR] = useState(0.55)
  const [lufs, setLufs]     = useState(-14.2)
  const [peak, setPeak]     = useState(-0.1)

  // ── active tab ────────────────────────────────────────────────────
  const [activeTab, setActiveTab] = useState('COMMAND')

  // ── active side module ────────────────────────────────────────────
  const [activeMod, setActiveMod] = useState('ai')

  const sideModules = [
    { id: 'ai',        icon: 'memory',                     label: 'AI' },
    { id: 'transport', icon: 'play_arrow',                  label: 'TRANSPORT' },
    { id: 'comp',      icon: 'settings_input_component',    label: 'COMP' },
    { id: 'limit',     icon: 'linear_scale',                label: 'LIMIT' },
    { id: 'eq',        icon: 'graphic_eq',                  label: 'EQ' },
    { id: 'meters',    icon: 'analytics',                   label: 'METERS' }
  ]

  // ── helpers ───────────────────────────────────────────────────────
  const ts = () => new Date().toLocaleTimeString('en-US', { hour12: false })

  const addMsg = useCallback((msg) => {
    setMessages(prev => [...prev, { id: Date.now() + Math.random(), ...msg }])
  }, [])

  const sysMsg = useCallback((text) => addMsg({ type: 'system', text }), [addMsg])

  useEffect(() => { chatEndRef.current?.scrollIntoView({ behavior: 'smooth' }) }, [messages])

  // ── bridge init ───────────────────────────────────────────────────
  useEffect(() => {
    const stateUnsub = whycremisi.onStateChange((s) => {
      setConnStatus(s)
      if (s === ConnectionState.CONNECTED) {
        sysMsg(`[${ts()}] WEBSOCKET CONNECTED TO PLUGIN`)
        setBotState('success')
        setTimeout(() => setBotState('idle'), 2000)
      }
      if (s === ConnectionState.DISCONNECTED || s === ConnectionState.ERROR) {
        sysMsg(`[${ts()}] CONNECTION LOST — RECONNECTING...`)
        setBotState('sad')
      }
      if (s === ConnectionState.RECONNECTING) {
        setBotState('loading')
      }
    })

    window.addEventListener('whycremisi-botstate', (e) => setBotState(e.detail))

    // ── AI response (complete) ──
    const unsubAI = whycremisi.on('ai.response', (payload) => {
      setBotState('success')
      setTimeout(() => setBotState('idle'), 2000)
      streamingIdRef.current = null
      if (payload?.content) {
        addMsg({ type: 'bot', text: payload.content, time: ts(), telemetry: true })
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
        setTimeout(() => setBotState('idle'), 2000)
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

    // ── plugin errors ──
    const unsubErr = whycremisi.on('plugin.error', (payload) => {
      setBotState('error')
      setTimeout(() => setBotState('idle'), 3000)
      sysMsg(`[${ts()}] [ERROR] ${payload.message || payload.code || 'Unknown error'}`)
    })

    // connect
    whycremisi.connect().catch(() => {
      sysMsg(`[${ts()}] PLUGIN NOT RUNNING — OFFLINE MODE`)
    })

    return () => {
      stateUnsub(); unsubAI(); unsubStream(); unsubTransport()
      unsubMeter(); unsubOSC(); unsubErr()
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
    if (e.key !== 'Enter' || !inputVal.trim() || botState !== 'idle') return
    const cmd = inputVal.trim()
    const t = ts()
    addMsg({ type: 'user', text: cmd, time: t })
    setInputVal('')
    setBotState('thinking')

    if (whycremisi.isConnected()) {
      whycremisi.sendAIPrompt(cmd)
    } else {
      // offline fallback — simulate response
      setTimeout(() => {
        const offlineId = Date.now()
        streamingIdRef.current = offlineId
        setMessages(prev => [...prev, { id: offlineId, type: 'bot', text: '', time: ts(), streaming: true }])
        setBotState('typing')
        const reply = '[OFFLINE] Plugin not connected. Connect WhyCremisi VST to enable real AI responses.'
        let i = 0
        const ticker = setInterval(() => {
          i++
          setMessages(prev => prev.map(m => m.id === offlineId ? { ...m, text: reply.slice(0, i) } : m))
          if (i >= reply.length) {
            clearInterval(ticker)
            setMessages(prev => prev.map(m => m.id === offlineId ? { ...m, streaming: false } : m))
            streamingIdRef.current = null
            setBotState('idle')
          }
        }, 18)
      }, 600)
    }
  }

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
        i++
        setMessages(prev => prev.map(m => m.id === id ? { ...m, text: reply.slice(0, i) } : m))
        if (i >= reply.length) {
          clearInterval(t)
          setMessages(prev => prev.map(m => m.id === id ? { ...m, streaming: false, telemetry: true } : m))
          streamingIdRef.current = null
          setBotState('success')
          setTimeout(() => setBotState('idle'), 1500)
        }
      }, 16)
    }
  }, [botState])

  const dismissAdvisory = useCallback(() => {
    setMessages(prev => prev.filter(m => m.type !== 'advisory'))
  }, [])

  // ── gain slider pct ───────────────────────────────────────────────
  const gainPct = Math.max(0, Math.min(100, ((gainDb + 60) / 72) * 100))

  // ── meter bars ────────────────────────────────────────────────────
  const meterBars = Array.from({ length: 12 }, (_, i) => ({
    color: i < 8 ? '#FFB000' : '#DC143C',
    active: i < Math.round((meterL + meterR) / 2 * 12)
  }))

  // ── render ────────────────────────────────────────────────────────
  return (
    <div className="bg-[#0d0d0d] select-none h-screen w-screen overflow-hidden text-[#e5e2e1] font-['Space_Grotesk']">
      <div className="crt-overlay" />

      {/* ── HEADER ─────────────────────────────────────────────────── */}
      <header className="bg-[#131313] flex justify-between items-center w-full px-5 py-2 border-b border-[#222222] z-50 relative h-12">
        <div className="flex items-center gap-4">
          {/* BotFace in header */}
          <BotFace state={botState} className="w-9 h-9" />
          <h1 className="text-lg font-black tracking-tighter text-[#DC143C] uppercase leading-none">WHYCREMISI</h1>
          <nav className="flex gap-5 uppercase tracking-[0.1em] font-bold text-[11px]">
            {['COMMAND','MASTER','TELEMETRY','SESSIONS'].map(tab => (
              <a key={tab}
                onClick={() => setActiveTab(tab)}
                className={activeTab === tab
                  ? 'text-[#FFB000] border-b border-[#FFB000] pb-0.5 cursor-pointer'
                  : 'text-[#4d4d4d] hover:text-[#FFB000] transition-colors cursor-pointer'}
              >{tab}</a>
            ))}
          </nav>
        </div>

        <div className="flex items-center gap-5 text-[10px] font-mono text-[#FFB000] opacity-80">
          {/* DAW info */}
          <div className="flex flex-col items-end text-[9px]">
            <span>BPM: {transport.bpm.toFixed(1)}</span>
            <span className={transport.isPlaying ? 'text-[#00FFaa]' : 'text-[#4d4d4d]'}>
              {transport.isPlaying ? '▶ PLAY' : '■ STOP'}
            </span>
          </div>
          <div className="flex flex-col items-end border-l border-[#222222] pl-4">
            <span>SR: 96kHz</span>
            <span>BUF: 512</span>
          </div>
          <div className="flex flex-col items-end border-l border-[#222222] pl-4">
            <span>CPU: 12%</span>
            <span>LAT: 4.2ms</span>
          </div>

          {/* Connection LED */}
          <div className="flex items-center gap-2 ml-3 border-l border-[#222222] pl-4">
            <motion.div
              className={`w-2 h-2 rounded-full ${
                connStatus === ConnectionState.CONNECTED   ? 'bg-[#00FFaa]' :
                connStatus === ConnectionState.CONNECTING ||
                connStatus === ConnectionState.RECONNECTING ? 'bg-[#FFB000]' : 'bg-[#DC143C]'
              }`}
              animate={connStatus === ConnectionState.CONNECTING || connStatus === ConnectionState.RECONNECTING
                ? { opacity: [1,0.3,1] } : {}}
              transition={{ repeat: Infinity, duration: 0.8 }}
            />
            <span className="text-[9px] uppercase tracking-widest">
              {connStatus === ConnectionState.CONNECTED   ? 'CONNECTED' :
               connStatus === ConnectionState.CONNECTING  ? 'CONNECTING' :
               connStatus === ConnectionState.RECONNECTING? 'RECONNECT' : 'OFFLINE'}
            </span>
          </div>

          {/* Action icons */}
          <div className="flex gap-3 ml-1">
            <span className="material-symbols-outlined text-base text-[#4d4d4d] hover:text-[#FFB000] cursor-pointer">settings</span>
            <span className="material-symbols-outlined text-base text-[#4d4d4d] hover:text-[#DC143C] cursor-pointer"
              onClick={() => whycremisi.disconnect()}>power_settings_new</span>
          </div>
        </div>
      </header>

      {/* ── SIDEBAR ────────────────────────────────────────────────── */}
      <aside className="fixed left-0 top-12 h-[calc(100vh-3rem)] w-16 flex flex-col items-center py-3 bg-[#131313] border-r border-[#222222] z-40">
        <div className="text-[9px] font-mono text-[#4d4d4d] uppercase mb-6 rotate-180 select-none" style={{ writingMode:'vertical-lr' }}>
          MODULES
        </div>
        {sideModules.map(mod => (
          <motion.button
            key={mod.id}
            className={`w-full py-3 flex flex-col items-center gap-0.5 transition-colors ${
              activeMod === mod.id
                ? 'text-[#DC143C] bg-[#1a1a1a] border-l-4 border-[#DC143C]'
                : 'text-[#4d4d4d] hover:text-[#FFB000] hover:bg-[#1a1a1a] border-l-4 border-transparent'
            }`}
            whileHover={{ scale: 1.04 }}
            whileTap={{ scale: 0.94 }}
            onClick={() => setActiveMod(mod.id)}
          >
            <span className="material-symbols-outlined text-lg">{mod.icon}</span>
            <span className="text-[8px] font-bold uppercase font-mono">{mod.label}</span>
          </motion.button>
        ))}
      </aside>

      {/* ── SESSION PANEL ──────────────────────────────────────────── */}
      <AnimatePresence>
        {activeTab === 'SESSIONS' && (
          <motion.div
            key="session-panel"
            initial={{ opacity: 0, y: 8 }}
            animate={{ opacity: 1, y: 0 }}
            exit={{ opacity: 0, y: 8 }}
            transition={{ duration: 0.25, ease: [0.22,1,0.36,1] }}
            className="ml-16 mt-0 h-[calc(100vh-3rem)] relative z-10"
          >
            <SessionPanel />
          </motion.div>
        )}
      </AnimatePresence>

      {/* ── MAIN GRID ──────────────────────────────────────────────── */}
      <main className={`ml-16 mt-0 h-[calc(100vh-3rem)] grid grid-cols-12 grid-rows-6 p-1 gap-1 overflow-hidden bg-[#0d0d0d] relative z-10 ${activeTab !== 'COMMAND' ? 'hidden' : ''}`}>

        {/* ── AI CHAT CONSOLE (9/12 × 4/6) ──────────────────────── */}
        <section className="col-span-9 row-span-4 bg-[#0e0e0e] border border-[#222222] flex flex-col overflow-hidden">
          <div className="bg-[#1a1a1a] px-4 py-1.5 flex justify-between items-center border-b border-[#222222]">
            <div className="flex items-center gap-2">
              <div className="w-1.5 h-1.5 rounded-full bg-[#FFB000] led-amber-active" />
              <span className="text-[10px] font-bold text-[#FFB000] tracking-widest uppercase">AI COMMAND CONSOLE</span>
            </div>
            <div className="flex items-center gap-4 text-[9px] font-mono text-[#4d4d4d]">
              <span>NEURAL_MASTERING_V4.2.0</span>
              {dawConnected && <span className="text-[#00FFaa]">● DAW SYNC</span>}
            </div>
          </div>

          {/* Messages */}
          <div className="flex-1 px-6 py-4 font-mono overflow-y-auto custom-scrollbar space-y-4">
            <AnimatePresence>
              {messages.map((msg) => {
                if (msg.type === 'system') return (
                  <motion.div key={msg.id}
                    initial={{ opacity: 0 }}
                    animate={{ opacity: 0.45 }}
                    className="text-[10px] font-mono text-[#e5e2e1]"
                  >
                    {msg.text}
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
                    <div className="advisory-breathe bg-[#121212] border border-[#DC143C]/40 p-4 relative">
                      {/* Decorative mini bar chart */}
                      <div className="absolute top-2 right-4 flex items-end gap-[2px] h-5 opacity-30">
                        {[35,70,50,90,60].map((h,i) => (
                          <div key={i} className="w-[2px] bg-[#DC143C]" style={{ height:`${h}%` }} />
                        ))}
                      </div>
                      <div className="flex items-start gap-3 mb-3 border-b border-[#222222] pb-2">
                        <div className="w-1 h-6 bg-[#DC143C] relative overflow-hidden flex-shrink-0 mt-0.5">
                          <motion.div className="absolute inset-0 bg-white/20"
                            animate={{ y:['0%','100%','0%'] }}
                            transition={{ repeat:Infinity, duration:2 }}
                          />
                        </div>
                        <div>
                          <span className="text-[#DC143C] text-[11px] font-bold tracking-tighter uppercase block">AI MASTERING ADVISORY</span>
                          <span className="text-[8px] text-[#4d4d4d] tracking-[0.2em]">NODE: CREMISI_X9 · PRIORITY: HIGH</span>
                        </div>
                      </div>
                      <p className="text-sm text-[#FFB000] leading-relaxed mb-4">
                        Detected harmonic crowding in <span className="text-white font-bold border-b border-[#DC143C]/40 px-1">200Hz–400Hz</span> range.
                        Suggesting dynamic dip of <span className="bg-[#DC143C] text-white px-1.5 font-bold">-2.4dB</span>.
                        Transient preservation at <span className="text-white underline decoration-dotted">84%</span>.
                      </p>
                      <div className="flex gap-3 flex-wrap">
                        <motion.button
                          className="bg-[#DC143C] text-white px-4 py-1.5 text-[10px] font-bold uppercase hover:bg-white hover:text-[#DC143C] transition-colors tracking-widest disabled:opacity-40"
                          whileTap={{ scale: 0.95 }}
                          onClick={executeChain}
                          disabled={botState !== 'idle'}
                        >
                          <span className="material-symbols-outlined text-[13px] align-middle mr-1">bolt</span>
                          EXECUTE CHAIN
                        </motion.button>
                        <motion.button
                          className="border border-[#4d4d4d] text-[#4d4d4d] px-4 py-1.5 text-[10px] font-bold uppercase hover:border-[#FFB000] hover:text-[#FFB000] transition-colors tracking-widest"
                          whileTap={{ scale: 0.95 }}
                          onClick={() => { if(botState==='idle') { addMsg({type:'user',text:'Analyze the low end further',time:ts()}); setBotState('thinking') } }}
                        >ANALYZE FURTHER</motion.button>
                        <button className="text-[#4d4d4d] hover:text-white text-[9px] font-bold uppercase self-center transition-colors"
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
                    className="flex flex-col items-end gap-1"
                  >
                    <div className="bg-[#222222]/50 border border-[#FFB000]/20 px-4 py-2 max-w-[80%] text-sm text-white/90 font-mono italic">
                      {msg.text}
                    </div>
                    <span className="text-[9px] text-[#4d4d4d] mr-1 uppercase font-bold tracking-widest">
                      User // {msg.time}
                    </span>
                  </motion.div>
                )

                if (msg.type === 'bot') {
                  const isLatest = msg.id === lastBotId
                  return (
                    <motion.div key={msg.id}
                      initial={{ opacity:0, y:8 }}
                      animate={{ opacity:1, y:0 }}
                      transition={{ duration:0.4 }}
                      className="flex gap-4"
                    >
                      <div className="flex-shrink-0 mt-1">
                        {isLatest
                          ? <BotFace state={botState} className="w-14 h-14" />
                          : <div className="w-8 h-8 rounded-full border border-[#FFB000]/20 bg-[#1a1a1a] flex items-center justify-center opacity-30">
                              <span className="material-symbols-outlined text-[12px] text-[#FFB000]">smart_toy</span>
                            </div>
                        }
                      </div>
                      <div className="flex-1 bg-[#1a1a1a] border border-[#FFB000]/10 p-4 relative">
                        <div className="absolute top-0 left-0 w-[2px] h-full bg-[#FFB000]" />
                        <div className="flex justify-between items-start mb-1.5">
                          <span className="text-[#FFB000] text-[10px] font-bold tracking-tighter uppercase">WHYCREMISI AI</span>
                          <span className="text-[9px] text-[#4d4d4d] font-mono">{msg.time}</span>
                        </div>
                        <p className="text-sm text-[#e5e2e1] leading-relaxed whitespace-pre-wrap">
                          {msg.text}
                          {msg.streaming && (
                            <motion.span className="inline-block w-1.5 h-4 bg-[#FFB000] ml-1 align-middle"
                              animate={{ opacity:[1,0,1] }}
                              transition={{ repeat:Infinity, duration:0.7 }}
                            />
                          )}
                        </p>
                        {msg.telemetry && !msg.streaming && (
                          <motion.div initial={{ opacity:0 }} animate={{ opacity:1 }} transition={{ delay:0.3 }}
                            className="mt-3 grid grid-cols-2 gap-3 bg-[#0d0d0d]/60 p-3 border border-[#222222]"
                          >
                            {[['L/R Balance','52% R','#DC143C',52],['Side Content','38.4%','#FFB000',38.4]].map(([lbl,val,clr,pct])=>(
                              <div key={lbl} className="space-y-1">
                                <div className="flex justify-between text-[9px] uppercase font-bold">
                                  <span className="text-[#4d4d4d]">{lbl}</span>
                                  <span style={{ color:clr }}>{val}</span>
                                </div>
                                <div className="h-1.5 bg-[#222222] w-full">
                                  <motion.div className="h-full"
                                    style={{ backgroundColor:clr, boxShadow:`0 0 4px ${clr}60` }}
                                    initial={{ width:'0%' }}
                                    animate={{ width:`${pct}%` }}
                                    transition={{ duration:1, ease:'easeOut' }}
                                  />
                                </div>
                              </div>
                            ))}
                          </motion.div>
                        )}
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
          <div className="h-12 bg-[#131313] border-t border-[#222222] flex items-center px-5 gap-3">
            <span className="text-[#FFB000] font-bold text-sm tracking-widest shrink-0">CMD&gt;</span>
            <div className="flex-1 relative flex items-center">
              <input
                className="w-full bg-transparent border-none text-white font-mono text-sm focus:ring-0 focus:outline-none placeholder-[#4d4d4d] p-0 disabled:opacity-40"
                placeholder={botState !== 'idle' ? 'PROCESSING...' : 'Ask the AI anything about your mix...'}
                value={inputVal}
                onChange={e => setInputVal(e.target.value)}
                onKeyDown={handleCommand}
                disabled={botState !== 'idle'}
              />
              {botState === 'idle' && <div className="terminal-cursor" />}
            </div>
            <div className="flex gap-2 text-[#4d4d4d]">
              <motion.button whileHover={{ scale:1.1 }} whileTap={{ scale:0.9 }}
                className="material-symbols-outlined text-lg cursor-pointer hover:text-[#FFB000] transition-colors"
                onClick={() => dawCmd('play')}>play_arrow</motion.button>
              <motion.button whileHover={{ scale:1.1 }} whileTap={{ scale:0.9 }}
                className="material-symbols-outlined text-lg cursor-pointer hover:text-[#FFB000] transition-colors"
                onClick={() => dawCmd('stop')}>stop</motion.button>
              <motion.button whileHover={{ scale:1.1 }} whileTap={{ scale:0.9 }}
                className={`material-symbols-outlined text-lg cursor-pointer transition-colors ${transport.isRecording ? 'text-[#DC143C] led-red-active' : 'hover:text-[#DC143C]'}`}
                onClick={() => dawCmd('record')}>fiber_manual_record</motion.button>
            </div>
          </div>
        </section>

        {/* ── MASTERING RACK (3/12 × 4/6) ───────────────────────── */}
        <section className="col-span-3 row-span-4 bg-[#131313] border border-[#222222] flex flex-col">
          <div className="bg-[#1a1a1a] px-4 py-1.5 border-b border-[#222222] flex justify-between items-center">
            <span className="text-[10px] font-bold text-[#DC143C] tracking-widest uppercase">MASTERING RACK</span>
            <span className="text-[9px] font-mono text-[#4d4d4d]">STEREO MASTER</span>
          </div>

          <div className="flex-1 flex p-4 gap-5 overflow-hidden">
            {/* Gain fader */}
            <div className="w-14 flex flex-col items-center gap-1">
              <span className="text-[8px] font-mono text-[#FFB000]">+12</span>
              <div
                className="flex-1 w-8 bg-[#0e0e0e] border border-[#222222] relative flex flex-col justify-end p-1 cursor-ns-resize"
                onMouseMove={(e) => {
                  if (e.buttons !== 1) return
                  const rect = e.currentTarget.getBoundingClientRect()
                  const pct = 1 - (e.clientY - rect.top) / rect.height
                  setGainDb(Math.round((pct * 72 - 60) * 10) / 10)
                  if (whycremisi.isConnected()) whycremisi.sendDAWCommand('setGain', { value: pct })
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
              <span className="text-[8px] font-mono text-[#FFB000]">-INF</span>
              <span className="text-[10px] font-bold text-[#e5e2e1] mt-1">{gainDb > 0 ? `+${gainDb}` : gainDb}dB</span>
              <span className="text-[8px] font-mono text-[#4d4d4d]">GAIN</span>
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
                    <span className="text-[9px] font-bold uppercase text-[#4d4d4d] truncate">{label}</span>
                    <motion.div
                      className={`w-4 h-4 border flex items-center justify-center cursor-pointer flex-shrink-0 ${on ? 'border-[#DC143C]/60' : 'border-[#4d4d4d]'}`}
                      whileHover={{ scale:1.1 }}
                    >
                      <div className={`w-2 h-2 rounded-full ${on ? 'bg-[#DC143C] led-red-active' : 'bg-[#222222]'}`} />
                    </motion.div>
                  </div>
                ))}
              </div>

              {/* Sat type */}
              <div className="border-t border-[#222222] pt-3">
                <span className="text-[8px] text-[#FFB000] uppercase opacity-60 block mb-1">Saturation</span>
                <div className="bg-[#0e0e0e] border border-[#222222] px-2 py-1 text-[10px] text-[#FFB000] flex justify-between items-center">
                  <span>CRIMSON_TUBE</span>
                  <span className="material-symbols-outlined text-[13px]">expand_more</span>
                </div>
              </div>

              {/* Drive knob */}
              <div className="flex-1 flex flex-col items-center justify-center relative">
                <svg className="w-20 h-20 rotate-[-90deg] cursor-pointer" viewBox="0 0 100 100"
                  onMouseMove={(e) => {
                    if (e.buttons !== 1) return
                    const rect = e.currentTarget.getBoundingClientRect()
                    const cx = rect.left + rect.width/2, cy = rect.top + rect.height/2
                    const angle = Math.atan2(e.clientY - cy, e.clientX - cx) * 180 / Math.PI
                    const norm = Math.max(0, Math.min(100, ((angle + 180) / 360) * 100))
                    setDriveVal(Math.round(norm * 10) / 10)
                    if (whycremisi.isConnected()) whycremisi.sendDAWCommand('setDrive', { value: norm/100 })
                  }}
                >
                  <circle cx="50" cy="50" r="38" fill="none" stroke="#222222" strokeWidth="4" />
                  <motion.circle
                    cx="50" cy="50" r="38" fill="none" stroke="#DC143C" strokeWidth="4"
                    strokeLinecap="round"
                    initial={{ strokeDasharray:'0 239' }}
                    animate={{ strokeDasharray:`${driveVal / 100 * 239} 239` }}
                    transition={{ duration:0.15 }}
                    style={{ filter:'drop-shadow(0 0 4px #DC143C)' }}
                  />
                </svg>
                <div className="absolute flex flex-col items-center pointer-events-none">
                  <span className="text-[13px] font-bold text-[#e5e2e1]">{driveVal.toFixed(1)}</span>
                  <span className="text-[8px] text-[#FFB000] uppercase font-mono">DRIVE</span>
                </div>
              </div>
            </div>
          </div>

          {/* Peak meter strip */}
          <div className="h-10 bg-[#0e0e0e] border-t border-[#222222] flex items-center px-3 gap-2">
            <div className="flex-1 h-2 bg-[#1a1a1a] flex gap-[1px]">
              {meterBars.map((bar, i) => (
                <motion.div key={i} className="flex-1 h-full"
                  animate={{ opacity: bar.active ? 1 : 0.12, scaleY: bar.active ? [1,1.15,1] : 1 }}
                  transition={{ scaleY: { repeat:Infinity, duration:0.4+i*0.05, delay:i*0.04 } }}
                  style={{ backgroundColor: bar.color }}
                />
              ))}
            </div>
            <span className={`text-[9px] font-mono font-bold ${peak > -1 ? 'text-[#DC143C]' : 'text-[#4d4d4d]'}`}>
              {peak > -1 ? 'PEAK!' : 'OK'}
            </span>
          </div>
        </section>

        {/* ── VECTOR SCOPE / WAVEFORM (full width × 2/6) ─────────── */}
        <section className="col-span-12 row-span-2 bg-[#0e0e0e] border border-[#222222] relative overflow-hidden">
          <div className="absolute top-2 left-4 z-10 flex items-center gap-3">
            <span className="text-[10px] font-bold text-[#FFB000] uppercase opacity-80">VECTOR SCOPE · STEREO FIELD</span>
            <div className="h-[0.5px] w-32 bg-gradient-to-r from-[#FFB000] to-transparent" />
          </div>
          <div className="absolute top-2 right-4 z-10 flex gap-6 text-[8px] font-mono text-[#4d4d4d] uppercase">
            <span>L: {(meterL * -60 + 0).toFixed(1)}dB</span>
            <span>R: {(meterR * -60 + 0).toFixed(1)}dB</span>
            <span>LUFS: {lufs.toFixed(1)}</span>
          </div>

          {/* Grid */}
          <div className="absolute inset-0 opacity-15"
            style={{ backgroundImage:'linear-gradient(#4d4d4d 0.5px,transparent 0.5px),linear-gradient(90deg,#4d4d4d 0.5px,transparent 0.5px)', backgroundSize:'40px 40px' }}
          />
          <div className="absolute inset-0 opacity-5"
            style={{ backgroundImage:'linear-gradient(#4d4d4d 0.25px,transparent 0.25px),linear-gradient(90deg,#4d4d4d 0.25px,transparent 0.25px)', backgroundSize:'10px 10px' }}
          />
          {/* Center crosshair */}
          <div className="absolute inset-0 flex items-center justify-center opacity-20 pointer-events-none">
            <div className="w-full h-[0.5px] bg-[#FFB000]" />
            <div className="h-full w-[0.5px] bg-[#FFB000] absolute" />
          </div>

          {/* Scanlines */}
          <div className="absolute inset-0 overflow-hidden pointer-events-none">
            <motion.div className="w-full h-[1px] bg-gradient-to-r from-transparent via-[#FFB000]/20 to-transparent absolute"
              animate={{ top:['0%','100%'] }}
              transition={{ repeat:Infinity, duration:3, ease:'linear' }}
            />
            <motion.div className="w-full h-[1px] bg-gradient-to-r from-transparent via-[#DC143C]/15 to-transparent absolute"
              animate={{ top:['0%','100%'] }}
              transition={{ repeat:Infinity, duration:4.5, ease:'linear', delay:1 }}
            />
          </div>

          {/* Waveform */}
          <svg className="w-full h-full" preserveAspectRatio="none">
            <motion.path
              d="M0,80 Q50,10 100,90 T200,40 T300,110 T400,20 T500,85 T600,45 T700,95 T800,35 T900,105 T1000,60"
              fill="none" stroke="#DC143C" strokeOpacity="0.75" strokeWidth="1.5"
              style={{ filter:'drop-shadow(0 0 6px rgba(220,20,60,0.4))' }}
              initial={{ pathLength:0 }}
              animate={{ pathLength:1 }}
              transition={{ duration:2, ease:'easeInOut' }}
            />
            <motion.path
              d="M0,75 Q55,15 105,85 T205,35 T305,115 T405,25 T505,80 T605,50 T705,90 T805,40 T905,100 T1005,65"
              fill="none" stroke="#FFB000" strokeOpacity="0.35" strokeWidth="0.8"
              initial={{ pathLength:0 }}
              animate={{ pathLength:1 }}
              transition={{ duration:2.5, ease:'easeInOut', delay:0.3 }}
            />
          </svg>
        </section>
      </main>

      {/* ── FOOTER ─────────────────────────────────────────────────── */}
      <footer className="fixed bottom-0 left-0 w-full z-50 flex justify-between items-center h-9 border-t border-[#222222] bg-[#0d0d0d] px-5">
        <div className="flex items-center gap-5 text-[10px] font-mono">
          {[
            { icon:'equalizer', label:`LUFS: ${lufs.toFixed(1)}`, color:'#FFB000' },
            { icon:'priority_high', label:`PEAK: ${peak.toFixed(1)}dB`, color: peak>-1?'#DC143C':'#4d4d4d' },
            { icon:'speed', label:`L: ${(meterL*100).toFixed(0)}%  R: ${(meterR*100).toFixed(0)}%`, color:'#4d4d4d' }
          ].map(({ icon, label, color }) => (
            <div key={label} className="flex items-center gap-1.5" style={{ color }}>
              <span className="material-symbols-outlined text-sm">{icon}</span>
              <span>{label}</span>
            </div>
          ))}
        </div>

        <div className="flex items-center gap-4">
          <motion.span className="text-[9px] font-mono text-[#FFB000] uppercase tracking-widest"
            animate={{ opacity:[0.7,1,0.7] }}
            transition={{ repeat:Infinity, duration:2.5 }}
          >
            {connStatus === ConnectionState.CONNECTED ? '◉ CONNECTED // READY' : '◌ OFFLINE // AWAITING SIGNAL'}
          </motion.span>
          <motion.div
            className={`h-3 w-3 ${connStatus === ConnectionState.CONNECTED ? 'bg-[#FFB000] led-amber-active' : 'bg-[#4d4d4d]'}`}
            animate={connStatus === ConnectionState.CONNECTED
              ? { boxShadow:['0 0 6px #FFB000','0 0 14px #FFB000','0 0 6px #FFB000'] } : {}}
            transition={{ repeat:Infinity, duration:1.8 }}
          />
        </div>
      </footer>
    </div>
  )
}
