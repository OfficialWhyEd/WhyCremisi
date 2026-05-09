import React, { useState, useEffect, useMemo, useRef, useCallback } from 'react'
import { motion, AnimatePresence } from 'framer-motion'
import { whycremisi, ConnectionState } from './whycremisi-bridge'
import { BotFace } from './components/BotFace'
import './index.css'

// ============================================================
// WHYCREMISI VST PLUGIN - App.jsx
// Versione FUSA: Layout Heartbroken + Bridge Aure (Aura)
// ============================================================

export default function App() {
  // ---------- STATI CONNESSIONE ----------
  const [connectionStatus, setConnectionStatus] = useState(ConnectionState.DISCONNECTED)
  const [botState, setBotState] = useState('idle')
  const [inputValue, setInputValue] = useState('')
  
  // ---------- STATI MESSAGGI ----------
  const [messages, setMessages] = useState([
    { id: 1, type: 'system', text: '[--:--:--] INITIALIZING NEURAL MATRIX... OK' },
    { id: 2, type: 'system', text: '[--:--:--] SCANNING SPECTRAL DENSITY... COMPLETE' },
    { id: 3, type: 'system', text: '[--:--:--] ANALYZING TRANSIENT RESPONSE IN 440HZ RANGE... DONE.' },
    { id: 4, type: 'advisory' }
  ])
  
  const [lastMessage, setLastMessage] = useState(null)
  const chatEndRef = useRef(null)

  // ---------- SIDE MODULES ----------
  const [activeSideModule, setActiveSideModule] = useState('ai')

  const sideModules = [
    { id: 'ai', icon: 'memory', label: 'AI ENGINE' },
    { id: 'transport', icon: 'play_arrow', label: 'TRANSPORT' },
    { id: 'comp', icon: 'settings_input_component', label: 'COMP' },
    { id: 'limit', icon: 'linear_scale', label: 'LIMIT' },
    { id: 'eq', icon: 'graphic_eq', label: 'EQ' },
    { id: 'meters', icon: 'analytics', label: 'METERS' }
  ]

  // ---------- SCROLL AUTOMATICO ----------
  const scrollToBottom = () => {
    chatEndRef.current?.scrollIntoView({ behavior: 'smooth' })
  }

  useEffect(() => {
    scrollToBottom()
  }, [messages])

  // ---------- INIZIALIZZAZIONE BRIDGE ----------
  useEffect(() => {
    whycremisi.onAny((message) => {
      setLastMessage(message)
      console.log('[WhyCremisi]', message)
    })

    // Listener cambio stato connessione
    const stateUnsub = whycremisi.onStateChange((state) => {
      setConnectionStatus(state)
      console.log('[WhyCremisi] Connection state:', state)
    })

    // Listener bot state da eventi
    window.addEventListener('whycremisi-botstate', (e) => {
      setBotState(e.detail)
    })

    // Connettiti al plugin
    whycremisi.connect().catch((err) => {
      console.warn('[WhyCremisi] Connessione WebSocket fallita:', err.message)
    })

    // ---------- LISTENER AI ----------
    const unsubAI = whycremisi.on('ai.response', (payload) => {
      console.log('[WhyCremisi] AI Response:', payload)
      setBotState('success')
      setTimeout(() => setBotState('idle'), 2000)
      
      if (payload && payload.content) {
        const timeStr = new Date().toLocaleTimeString('en-US', { hour12: false })
        setMessages(prev => [...prev, {
          id: Date.now(),
          type: 'bot',
          text: payload.content,
          time: timeStr,
          telemetry: true
        }])
      }
    })

    const unsubAIStream = whycremisi.on('ai.stream', (payload) => {
      console.log('[WhyCremisi] AI Stream:', payload)
      setBotState('typing')
      // Streaming characters - update last bot message
      if (payload && payload.content) {
        setMessages(prev => {
          const lastBot = [...prev].reverse().find(m => m.type === 'bot')
          if (lastBot) {
            return prev.map(m => m.id === lastBot.id ? { ...m, text: m.text + payload.content } : m)
          }
          return prev
        })
      }
    })

    // ---------- LISTENER ERRORI ----------
    const unsubError = whycremisi.on('plugin.error', (payload) => {
      console.error('[WhyCremisi] Plugin Error:', payload)
      setBotState('error')
      setTimeout(() => setBotState('idle'), 3000)
      setMessages(prev => [...prev, {
        id: Date.now(),
        type: 'system',
        text: `[ERROR] ${payload.message || payload.code || 'Unknown error'}`
      }])
    })

    // ---------- LISTENER DAW TRANSPORT ----------
    const unsubTransport = whycremisi.on('daw.transport', (payload) => {
      console.log('[WhyCremisi] DAW Transport:', payload)
      setMessages(prev => [...prev, {
        id: Date.now(),
        type: 'system',
        text: `[TRANSPORT] Play=${payload.isPlaying} Rec=${payload.isRecording} BPM=${payload.bpm || 'N/A'}`
      }])
    })

    // ---------- LISTENER PARAMETRI VST ----------
    const unsubParam = whycremisi.on('daw.parameter', (payload) => {
      console.log('[WhyCremisi] DAW Parameter:', payload)
      setMessages(prev => [...prev, {
        id: Date.now(),
        type: 'system',
        text: `[PARAM] ${payload.name || payload.address}: ${payload.value}`
      }])
    })

    // Cleanup
    return () => {
      whycremisi.off('*')
      stateUnsub()
      unsubAI()
      unsubAIStream()
      unsubError()
      unsubTransport()
      unsubParam()
      whycremisi.disconnect()
    }
  }, [])

  // ---------- EFFETTO TYPING (da Heartbroken) ----------
  const simulateTyping = async (text, messageId, endState = 'idle') => {
    setBotState('typing')
    let currentText = ''
    
    setMessages(prev => [...prev, {
      id: messageId,
      type: 'bot',
      text: '',
      telemetry: false
    }])

    for (let i = 0; i < text.length; i++) {
      currentText += text[i]
      setMessages(prev => prev.map(msg =>
        msg.id === messageId ? { ...msg, text: currentText } : msg
      ))
      await new Promise(r => setTimeout(r, 15 + Math.random() * 25))
    }

    setBotState('success')
    setTimeout(() => setBotState(endState), 1500)
  }

  // ---------- HANDLER COMANDI ----------
  const handleCommand = (e) => {
    if (e.key === 'Enter' && inputValue.trim() && botState === 'idle') {
      const newMsgId = Date.now()
      const timeStr = new Date().toLocaleTimeString('en-US', { hour12: false, hour: '2-digit', minute: '2-digit', second: '2-digit' })
      const command = inputValue.trim()

      // Aggiungi messaggio utente
      setMessages(prev => [...prev, {
        id: newMsgId,
        type: 'user',
        text: command,
        time: timeStr
      }])

      setInputValue('')
      setBotState('thinking')

      // Invia al bridge - PROMPT AI
      if (whycremisi.isConnected()) {
        whycremisi.sendAIPrompt(command, {
          onResponse: (payload) => {
            console.log('[WhyCremisi] Prompt response:', payload)
            setBotState('success')
            setTimeout(() => setBotState('idle'), 2000)
          },
          onError: (err) => {
            console.error('[WhyCremisi] Prompt error:', err)
            setBotState('error')
            setTimeout(() => setBotState('idle'), 2000)
            setMessages(prev => [...prev, {
              id: Date.now(),
              type: 'system',
              text: `[ERROR] ${err.message || 'Prompt failed'}`
            }])
          }
        })
      } else {
        // Offline - simula risposta
        setBotState('error')
        setTimeout(() => setBotState('idle'), 2000)
        setMessages(prev => [...prev, {
          id: Date.now(),
          type: 'system',
          text: '[ERROR] Not connected to plugin'
        }])
      }
    }
  }

  // ---------- HANDLER TRANSPORT DAW ----------
  const handleTransport = useCallback((action) => {
    if (whycremisi.isConnected()) {
      whycremisi.sendDAWCommand(action)
      console.log('[WhyCremisi] DAW Command:', action)
    } else {
      console.warn('[WhyCremisi] Cannot send DAW command - not connected')
    }
  }, [])

  // ---------- HANDLER ADVISORY ----------
  const executeSuggestedChain = () => {
    if (botState !== 'idle') return
    setBotState('loading')
    
    setTimeout(() => {
      simulateTyping(
        "Executing suggested chain. Dynamic dip of -2.4dB applied at 300Hz. Transient preservation locked at 84%. Spectral clarity increased.\n\n[ HB + ]",
        Date.now()
      )
    }, 1000)
  }

  // ---------- UTILITY ----------
  const lastBotMsgId = useMemo(() => {
    const botMsgs = messages.filter(m => m.type === 'bot')
    return botMsgs.length > 0 ? botMsgs[botMsgs.length - 1].id : null
  }, [messages])

  const dismissAdvisory = (id) => {
    setMessages(prev => prev.filter(m => m.id !== id))
  }

  // ============================================================
  // RENDER
  // ============================================================
  return (
    <div className="bg-background select-none h-screen w-screen overflow-hidden text-[#e5e2e1] font-['Space_Grotesk']">
      {/* CRT Overlay Effect */}
      <div className="crt-overlay"></div>

      {/* ========== TOP NAVIGATION ========== */}
      <header className="bg-[#131313] flex justify-between items-center w-full px-6 py-3 border-b border-[#222222] z-50 relative">
        <div className="flex items-center gap-6">
          <h1 className="text-xl font-black tracking-tighter text-[#DC143C] uppercase">WHYCREMISI</h1>
          <div className="flex gap-4">
            <nav className="flex gap-6 font-['Space_Grotesk'] uppercase tracking-[0.1em] font-bold text-sm">
              <a className="text-[#FFB000] border-b-2 border-[#FFB000] pb-1" href="#">COMMAND</a>
              <a className="text-[#4d4d4d] hover:text-[#FFB000] transition-colors" href="#">MASTER</a>
              <a className="text-[#4d4d4d] hover:text-[#FFB000] transition-colors" href="#">TELEMETRY</a>
              <a className="text-[#4d4d4d] hover:text-[#FFB000] transition-colors" href="#">VECTORS</a>
            </nav>
          </div>
        </div>
        
        <div className="flex items-center gap-6 text-[10px] font-mono text-[#FFB000] opacity-80">
          <div className="flex flex-col items-end">
            <span>SR: 96kHz</span>
            <span>BUF: 512</span>
          </div>
          <div className="flex flex-col items-end border-l border-[#222222] pl-4">
            <span>CPU: 12%</span>
            <span>LATENCY: 4.2ms</span>
          </div>
          {/* Connection Status Indicator */}
          <div className="flex items-center gap-2 ml-4">
            <div className={`w-2 h-2 rounded-full ${
              connectionStatus === ConnectionState.CONNECTED
                ? 'bg-[#00FFaa] led-amber-active'
                : connectionStatus === ConnectionState.CONNECTING
                ? 'bg-[#FFB000] animate-pulse'
                : 'bg-[#DC143C] led-red-active'
            }`}></div>
            <span>
              {connectionStatus === ConnectionState.CONNECTED ? 'CONNECTED'
                : connectionStatus === ConnectionState.CONNECTING ? 'CONNECTING...'
                : 'DISCONNECTED'}
            </span>
          </div>
          <div className="flex gap-3 ml-2">
            <span className="material-symbols-outlined text-sm text-[#4d4d4d] hover:text-[#FFB000] cursor-pointer">settings</span>
            <span className="material-symbols-outlined text-sm text-[#4d4d4d] hover:text-[#FFB000] cursor-pointer">power_settings_new</span>
          </div>
        </div>
      </header>

      {/* ========== SIDE MODULE ========== */}
      <aside className="fixed left-0 top-12 h-[calc(100vh-3rem)] w-20 flex flex-col items-center py-4 bg-[#131313] border-r border-[#222222] z-40">
        <div className="text-[10px] font-mono text-[#4d4d4d] uppercase mb-8 rotate-180" style={{ writingMode: 'vertical-lr' }}>
          MODULES // V1.0.4-STABLE
        </div>
        <div className="flex flex-col w-full">
          {sideModules.map(mod => (
            <motion.div
              key={mod.id}
              className={`py-4 flex flex-col items-center gap-1 cursor-pointer transition-all ${
                activeSideModule === mod.id
                  ? 'text-[#DC143C] bg-[#1a1a1a] border-l-4 border-[#DC143C]'
                  : 'text-[#4d4d4d] hover:text-[#FFB000] hover:bg-[#1a1a1a]'
              }`}
              whileHover={{ scale: 1.05 }}
              whileTap={{ scale: 0.95 }}
              onClick={() => {
                setActiveSideModule(mod.id)
                if (mod.id === 'transport') handleTransport('play')
                if (mod.id === 'comp') handleTransport('comp')
                if (mod.id === 'eq') handleTransport('eq')
                if (mod.id === 'limit') handleTransport('limit')
                if (mod.id === 'meters') handleTransport('meters')
              }}
            >
              <span className="material-symbols-outlined text-xl">{mod.icon}</span>
              <span className="font-['Space_Grotesk'] font-mono text-[10px] uppercase">{mod.label}</span>
            </motion.div>
          ))}
        </div>
      </aside>

      {/* ========== MAIN GRID ========== */}
      <main className="ml-20 mt-0 h-[calc(100vh-3rem)] grid grid-cols-12 grid-rows-6 p-1 gap-1 overflow-hidden bg-[#0d0d0d] relative z-10">

        {/* ========== AI CHAT INTERFACE ========== */}
        <section className="col-span-9 row-span-4 bg-[#0e0e0e] relative border border-[#222222] flex flex-col overflow-hidden">
          <div className="bg-[#1a1a1a] px-4 py-2 flex justify-between items-center border-b border-[#222222]">
            <div className="flex items-center gap-2">
              <div className="w-2 h-2 rounded-full bg-[#FFB000] led-amber-active"></div>
              <span className="text-[10px] font-bold text-[#FFB000] tracking-widest uppercase">AI COMMAND CONSOLE</span>
            </div>
            <span className="text-[9px] text-[#4d4d4d] font-mono">NEURAL_MASTERING_V4.2.0</span>
          </div>

          {/* Message History */}
          <div className="flex-1 p-6 font-mono overflow-y-auto custom-scrollbar space-y-6">
            <AnimatePresence>
              {messages.map((msg) => {
                // SYSTEM MESSAGE
                if (msg.type === 'system') {
                  return (
                    <motion.div
                      key={msg.id}
                      initial={{ opacity: 0 }}
                      animate={{ opacity: 0.4 }}
                      className="text-[10px] font-mono"
                    >
                      {msg.text?.split('...')[0]}... <span className="text-[#FFB000]">{msg.text?.split('...')[1]}</span>
                    </motion.div>
                  )
                }

                // ADVISORY CARD (AI suggerisce azioni)
                if (msg.type === 'advisory') {
                  return (
                    <motion.div
                      key="advisory-1"
                      initial={{ opacity: 0, y: 10, scale: 0.98, filter: 'blur(4px)' }}
                      animate={{ opacity: 1, y: 0, scale: 1, filter: 'blur(0)' }}
                      transition={{ duration: 0.6, ease: [0.22, 1, 0.36, 1] }}
                      exit={{ opacity: 0, scale: 0.98 }}
                      className="animate-advisory-in group relative mt-4 mb-6"
                    >
                      <div className="absolute -top-4 -right-2 opacity-5 pointer-events-none select-none parallax-item">
                        <pre className="text-[8px] leading-tight text-white font-mono">
                          0x45 0x21 0x88{'\n'}
                          [TRANS_LOCK]{'\n'}
                          0xFF 0x00 0x12
                        </pre>
                      </div>
                      <div className="advisory-card advisory-breathe bg-[#121212] border border-[#DC143C]/40 p-5 relative font-mono transition-all duration-500 hover:bg-[#161616] group-hover:scale-[1.005]">
                        {/* Mini bar graph */}
                        <div className="absolute top-2 right-4 flex items-end gap-[2px] h-6 opacity-40 parallax-item">
                          {[40, 70, 55, 90, 65].map((h, i) => (
                            <div key={i} className="w-[2px] bg-[#DC143C]" style={{ height: `${h}%` }}></div>
                          ))}
                        </div>
                        
                        <div className="flex justify-between items-start mb-4 border-b border-[#222222] pb-2">
                          <div className="flex items-center gap-3">
                            <div className="w-1.5 h-6 bg-[#DC143C] relative overflow-hidden">
                              <motion.div
                                className="absolute inset-0 bg-white/20"
                                animate={{ y: ['0%', '100%', '0%'] }}
                                transition={{ repeat: Infinity, duration: 2 }}
                              />
                            </div>
                            <div>
                              <span className="text-[#DC143C] text-[11px] font-bold tracking-tighter uppercase block">AI_MASTERING_ADVISORY_CORE</span>
                              <span className="text-[8px] text-[#4d4d4d] tracking-[0.2em]">NODE: CREMISI_X9</span>
                            </div>
                          </div>
                          <div className="flex flex-col items-end">
                            <span className="text-[9px] text-white/40 font-bold uppercase tracking-widest">Priority: <span className="text-[#DC143C]">High</span></span>
                            <span className="text-[7px] text-[#4d4d4d] font-mono mt-0.5">ID: #B87-FF01</span>
                          </div>
                        </div>
                        
                        <div className="text-sm leading-relaxed text-[#FFB000] space-y-5 relative">
                          <p className="leading-relaxed drop-shadow-[0_0_8px_rgba(255,176,0,0.1)]">
                            Detected significant harmonic crowding in the <span className="text-white border-b border-[#DC143C]/40 px-1 font-bold">200Hz - 400Hz</span> range.
                            Suggesting a surgical dynamic dip of <span className="bg-[#DC143C] text-white px-1.5 font-bold">-2.4dB</span> to increase spectral clarity.
                            Transient preservation currently at <span className="text-white underline decoration-dotted decoration-[#4d4d4d]">84%</span>.
                          </p>
                          
                          <div className="flex gap-4 opacity-30 text-[8px] font-mono parallax-item">
                            <span>HEX: 0xDC143C</span>
                            <span>VAL: -2.4dB_COR</span>
                            <span>SIG: 0.982</span>
                            <span>LAT: 0.2ms</span>
                          </div>
                          
                          <div className="flex flex-wrap gap-4 pt-2">
                            <motion.button
                              className="group/btn relative bg-[#DC143C] text-white px-5 py-2 text-[10px] font-bold uppercase overflow-hidden transition-all duration-300 active:scale-95 hover:shadow-[0_0_20px_rgba(220,20,60,0.6)] hover:bg-white hover:text-[#DC143C] tracking-widest border border-transparent disabled:opacity-50 disabled:cursor-not-allowed"
                              whileHover={{ scale: 1.02 }}
                              whileTap={{ scale: 0.95 }}
                              onClick={executeSuggestedChain}
                              disabled={botState !== 'idle'}
                            >
                              <span className="relative z-10 flex items-center gap-2">
                                <span className="material-symbols-outlined text-[14px]">bolt</span>
                                EXECUTE SUGGESTED CHAIN
                              </span>
                            </motion.button>
                            
                            <motion.button
                              className="group/btn border border-[#4d4d4d] text-[#4d4d4d] px-5 py-2 text-[10px] font-bold uppercase transition-all duration-300 active:scale-95 hover:border-[#FFB000] hover:text-[#FFB000] hover:shadow-[0_0_15px_rgba(255,176,0,0.2)] tracking-widest"
                              whileHover={{ scale: 1.02 }}
                              whileTap={{ scale: 0.95 }}
                            >
                              ANALYZE FURTHER
                            </motion.button>
                            
                            <motion.button
                              className="text-[#4d4d4d] hover:text-white transition-colors text-[9px] font-bold uppercase self-center hover:underline decoration-[#DC143C]"
                              whileHover={{ scale: 1.05 }}
                              onClick={() => dismissAdvisory('advisory-1')}
                            >
                              DISMISS
                            </motion.button>
                          </div>
                        </div>
                      </div>
                    </motion.div>
                  )
                }

                // USER MESSAGE
                if (msg.type === 'user') {
                  return (
                    <motion.div
                      key={msg.id}
                      initial={{ opacity: 0, x: 20 }}
                      animate={{ opacity: 1, x: 0 }}
                      transition={{ duration: 0.3 }}
                      className="flex flex-col items-end space-y-1 mt-4"
                    >
                      <div className="bg-[#222222]/50 border border-[#FFB000]/20 px-4 py-2 max-w-[80%] text-sm text-white/90 font-mono italic">
                        {msg.text}
                      </div>
                      <span className="text-[9px] text-[#4d4d4d] mr-2 uppercase font-bold tracking-widest">
                        User Request // {msg.time}
                      </span>
                    </motion.div>
                  )
                }

                // BOT MESSAGE
                if (msg.type === 'bot') {
                  const isLatest = msg.id === lastBotMsgId
                  return (
                    <motion.div
                      key={msg.id}
                      initial={{ opacity: 0, y: 10 }}
                      animate={{ opacity: 1, y: 0 }}
                      transition={{ duration: 0.4 }}
                      className="flex gap-4 mt-6"
                    >
                      {/* BotFace */}
                      <div className="flex-shrink-0 mt-1 w-16 h-16 flex items-center justify-center">
                        {isLatest ? (
                          <BotFace state={botState} className="w-16 h-16" />
                        ) : (
                          <div className="w-8 h-8 rounded-full border border-[#FFB000]/20 bg-[#1a1a1a] flex items-center justify-center opacity-30">
                            <span className="material-symbols-outlined text-[12px] text-[#FFB000]">smart_toy</span>
                          </div>
                        )}
                      </div>

                      {/* Message content */}
                      <div className="flex-1 bg-[#1a1a1a] border border-[#FFB000]/10 p-4 relative font-mono">
                        <div className="absolute top-0 left-0 w-[2px] h-full bg-[#FFB000]"></div>
                        <div className="flex justify-between items-start mb-2">
                          <span className="text-[#FFB000] text-[11px] font-bold tracking-tighter uppercase">AI_TELEMETRY_ENGINE</span>
                          <span className="text-[9px] text-[#4d4d4d]">DATA_FETCH: SUCCESS</span>
                        </div>
                        
                        <div className="text-sm text-[#e5e2e1] space-y-4">
                          <p className="leading-relaxed whitespace-pre-wrap">
                            {msg.text}
                            {/* Typing cursor */}
                            {botState === 'typing' && isLatest && (
                              <motion.span
                                className="inline-block w-1.5 h-4 bg-[#FFB000] ml-1"
                                animate={{ opacity: [1, 0, 1] }}
                                transition={{ repeat: Infinity, duration: 0.8 }}
                              />
                            )}
                          </p>

                          {/* Telemetry data */}
                          {msg.telemetry && (
                            <motion.div
                              initial={{ opacity: 0 }}
                              animate={{ opacity: 1 }}
                              className="grid grid-cols-1 sm:grid-cols-2 gap-4 bg-[#0d0d0d]/50 p-3 border border-[#222222]"
                            >
                              <div className="space-y-1">
                                <div className="flex justify-between text-[9px] uppercase font-bold">
                                  <span className="text-[#4d4d4d]">L/R Balance</span>
                                  <span className="text-[#DC143C]">52% R</span>
                                </div>
                                <div className="h-1.5 bg-[#222222] w-full relative">
                                  <div className="absolute left-1/2 -translate-x-1/2 w-[1px] h-full bg-[#4d4d4d] z-10"></div>
                                  <motion.div
                                    className="h-full bg-[#DC143C] w-[52%]"
                                    initial={{ width: '0%' }}
                                    animate={{ width: '52%' }}
                                    transition={{ duration: 1, ease: 'easeOut' }}
                                    style={{ marginLeft: '48%' }}
                                  />
                                </div>
                              </div>
                              <div className="space-y-1">
                                <div className="flex justify-between text-[9px] uppercase font-bold">
                                  <span className="text-[#4d4d4d]">Side Content</span>
                                  <span className="text-[#FFB000]">38.4%</span>
                                </div>
                                <div className="h-1.5 bg-[#222222] w-full">
                                  <motion.div
                                    className="h-full bg-[#FFB000]"
                                    initial={{ width: '0%' }}
                                    animate={{ width: '38.4%' }}
                                    transition={{ duration: 1, ease: 'easeOut' }}
                                    style={{ boxShadow: '0 0 5px rgba(255,176,0,0.4)' }}
                                  />
                                </div>
                              </div>
                            </motion.div>
                          )}
                        </div>
                      </div>
                    </motion.div>
                  )
                }

                return null
              })}
            </AnimatePresence>
            <div ref={chatEndRef} />
          </div>

          {/* Terminal Input */}
          <div className="h-14 bg-[#131313] border-t border-[#222222] flex items-center px-6 gap-4">
            <span className="text-[#FFB000] font-bold text-sm tracking-widest shrink-0">CMD &gt;</span>
            <div className="flex-1 relative flex items-center">
              <input
                className="w-full bg-transparent border-none text-white font-mono text-sm focus:ring-0 focus:outline-none placeholder-[#4d4d4d] p-0 disabled:opacity-50"
                placeholder={botState !== 'idle' ? 'PROCESSING...' : 'Type command (e.g. /analyze_spectral, play, stop)...'}
                type="text"
                value={inputValue}
                onChange={(e) => setInputValue(e.target.value)}
                onKeyDown={handleCommand}
                disabled={botState !== 'idle'}
              />
              {botState === 'idle' && <div className="terminal-cursor"></div>}
            </div>
            <div className="flex gap-2 text-[#4d4d4d]">
              <span className="material-symbols-outlined text-lg cursor-pointer hover:text-[#FFB000]">mic</span>
              <span className="material-symbols-outlined text-lg cursor-pointer hover:text-[#FFB000]">attachment</span>
              <span className="material-symbols-outlined text-lg cursor-pointer hover:text-[#FFB000] ml-2" onClick={() => handleTransport('play')}>play_arrow</span>
              <span className="material-symbols-outlined text-lg cursor-pointer hover:text-[#FFB000]" onClick={() => handleTransport('stop')}>stop</span>
              <span className="material-symbols-outlined text-lg cursor-pointer hover:text-[#FFB000]" onClick={() => handleTransport('record')}>fiber_manual_record</span>
            </div>
          </div>
        </section>

        {/* ========== MASTERING RACK ========== */}
        <section className="col-span-3 row-span-4 bg-[#131313] border border-[#222222] flex flex-col">
          <div className="bg-[#1a1a1a] px-4 py-2 border-b border-[#222222]">
            <span className="text-[10px] font-bold text-[#DC143C] tracking-widest uppercase">MASTERING RACK</span>
          </div>
          
          <div className="flex-1 flex p-4 gap-6">
            {/* Precision Gain Slider */}
            <div className="w-16 flex flex-col items-center gap-2">
              <span className="text-[8px] font-mono text-[#FFB000]">+12</span>
              <div className="flex-1 w-8 bg-[#0e0e0e] border border-[#222222] relative flex flex-col justify-end p-1">
                {[10, 25, 50, 75].map(pct => (
                  <div key={pct} className="absolute inset-x-0 h-[1px] bg-[#4d4d4d] opacity-20" style={{ top: `${pct}%` }}></div>
                ))}
                <div className="w-full h-[60%] bg-gradient-to-t from-[#DC143C]/20 to-[#DC143C] border-t-2 border-[#ffb3b3] shadow-[0_0_10px_rgba(220,20,60,0.5)] smooth-transition"></div>
                <div className="absolute bottom-[60%] left-[-10px] right-[-10px] h-4 bg-[#e5e2e1] cursor-ns-resize shadow-lg z-10 smooth-transition"></div>
              </div>
              <span className="text-[8px] font-mono text-[#FFB000]">-INF</span>
              <span className="text-[10px] font-bold text-[#e5e2e1] mt-2">GAIN</span>
            </div>

            {/* Controls Cluster */}
            <div className="flex-1 flex flex-col gap-6">
              <div className="space-y-4">
                <div className="flex items-center justify-between">
                  <span className="text-[9px] font-bold uppercase text-[#4d4d4d]">Deep Neural</span>
                  <motion.div
                    className="w-4 h-4 bg-[#1a1a1a] border border-[#DC143C]/30 flex items-center justify-center cursor-pointer"
                    whileHover={{ scale: 1.1 }}
                    onClick={() => setBotState(botState === 'idle' ? 'thinking' : 'idle')}
                  >
                    <div className={`w-2 h-2 rounded-full ${botState !== 'idle' ? 'bg-[#DC143C] led-red-active' : 'bg-[#222222]'}`}></div>
                  </motion.div>
                </div>
                <div className="flex items-center justify-between">
                  <span className="text-[9px] font-bold uppercase text-[#4d4d4d]">Master Chain</span>
                  <motion.div
                    className="w-4 h-4 bg-[#1a1a1a] border border-[#4d4d4d] flex items-center justify-center cursor-pointer"
                    whileHover={{ scale: 1.1 }}
                  >
                    <div className="w-2 h-2 rounded-full bg-[#222222]"></div>
                  </motion.div>
                </div>
              </div>

              <div className="flex-1 border-t border-[#222222] pt-4 flex flex-col gap-4">
                <div className="flex flex-col gap-1">
                  <span className="text-[8px] text-[#FFB000] uppercase opacity-60">Saturation Type</span>
                  <motion.div
                    className="bg-[#0e0e0e] border border-[#222222] px-2 py-1 text-[10px] text-[#FFB000] flex justify-between items-center cursor-pointer hover:border-[#FFB000]/40 transition-colors"
                    whileHover={{ borderColor: 'rgba(255,176,0,0.4)' }}
                  >
                    <span>CRIMSON_TUBE</span>
                    <span className="material-symbols-outlined text-[14px]">expand_more</span>
                  </motion.div>
                </div>

                {/* Knob SVG */}
                <div className="flex-1 flex flex-col items-center justify-center relative">
                  <svg className="w-24 h-24 rotate-[-90deg]" viewBox="0 0 100 100">
                    <circle cx="50" cy="50" fill="none" r="40" stroke="#222222" strokeWidth="4"></circle>
                    <motion.circle
                      className="knob-segment"
                      cx="50" cy="50" fill="none" r="40"
                      stroke="#DC143C"
                      strokeDasharray="180 251"
                      strokeWidth="4"
                      initial={{ strokeDasharray: '0 251' }}
                      animate={{ strokeDasharray: '180 251' }}
                      transition={{ duration: 1, ease: 'easeOut' }}
                    />
                  </svg>
                  <div className="absolute flex flex-col items-center">
                    <span className="text-[12px] font-bold text-[#e5e2e1]">72.5</span>
                    <span className="text-[8px] text-[#FFB000] uppercase font-mono">DRIVE</span>
                  </div>
                </div>
              </div>
            </div>
          </div>

          {/* Peak Meter */}
          <div className="h-12 bg-[#0e0e0e] border-t border-[#222222] flex items-center px-4 gap-2">
            <div className="flex-1 h-2 bg-[#1a1a1a] flex gap-[1px]">
              {[40, 40, 60, 80, 100, 60, 100, 100].map((opacity, i) => (
                <motion.div
                  key={i}
                  className="flex-1 h-full"
                  style={{
                    backgroundColor: i < 5 ? '#FFB000' : '#DC143C',
                    opacity: opacity / 100
                  }}
                  animate={{
                    opacity: [opacity / 100, (opacity + 10) / 100, opacity / 100],
                    scaleY: [1, 1.1, 1]
                  }}
                  transition={{
                    repeat: Infinity,
                    duration: 0.5 + i * 0.1,
                    delay: i * 0.05
                  }}
                />
              ))}
            </div>
            <span className="text-[10px] font-mono text-[#DC143C] font-bold">PEAK!</span>
          </div>
        </section>

        {/* ========== VECTOR SCOPE ========== */}
        <section className="col-span-12 row-span-2 bg-[#0e0e0e] border border-[#222222] relative overflow-hidden flex flex-col">
          <div className="absolute top-2 left-4 z-10 flex items-center gap-4">
            <span className="text-[10px] font-bold text-[#FFB000] uppercase opacity-80">Vector Scope: STEREO_FIELD</span>
            <div className="h-[1px] w-48 bg-gradient-to-r from-[#FFB000] to-transparent"></div>
          </div>

          <div className="flex-1 relative">
            {/* Grid background */}
            <div className="absolute inset-0">
              <div className="absolute inset-0 bg-gradient-to-b from-[#0d0d0d] via-[#0e0e0e] to-[#0d0d0d]"></div>
              <div className="absolute inset-0 opacity-20" style={{
                backgroundImage: 'linear-gradient(#4d4d4d 0.5px, transparent 0.5px), linear-gradient(90deg, #4d4d4d 0.5px, transparent 0.5px)',
                backgroundSize: '40px 40px'
              }}></div>
              <div className="absolute inset-0 opacity-5" style={{
                backgroundImage: 'linear-gradient(#4d4d4d 0.25px, transparent 0.25px), linear-gradient(90deg, #4d4d4d 0.25px, transparent 0.25px)',
                backgroundSize: '10px 10px'
              }}></div>
              {/* Scanlines */}
              <div className="absolute inset-0 overflow-hidden pointer-events-none">
                <motion.div
                  className="w-full h-[1px] bg-gradient-to-r from-transparent via-[#FFB000]/20 to-transparent absolute"
                  animate={{ top: ['0%', '100%'] }}
                  transition={{ repeat: Infinity, duration: 3, ease: 'linear' }}
                />
                <motion.div
                  className="w-full h-[1px] bg-gradient-to-r from-transparent via-[#DC143C]/20 to-transparent absolute"
                  animate={{ top: ['0%', '100%'] }}
                  transition={{ repeat: Infinity, duration: 4, ease: 'linear', delay: 0.5 }}
                />
              </div>
              {/* Center cross */}
              <div className="absolute inset-0 flex items-center justify-center opacity-30 pointer-events-none">
                <div className="w-full h-[0.5px] bg-[#FFB000]"></div>
                <div className="h-full w-[0.5px] bg-[#FFB000] absolute"></div>
              </div>
              <div className="absolute bottom-2 right-4 flex gap-4 text-[8px] font-mono text-[#4d4d4d] uppercase">
                <span>Gain_Scale: Linear</span>
                <span>Res: 24-bit/96kHz</span>
              </div>
            </div>

            {/* Waveform SVG */}
            <svg className="w-full h-full" preserveAspectRatio="none">
              <motion.path
                className="drop-shadow-[0_0_8px_rgba(220,20,60,0.4)]"
                d="M0,80 Q50,10 100,90 T200,40 T300,110 T400,20 T500,85 T600,45 T700,95 T800,35 T900,105 T1000,60"
                fill="none"
                stroke="#DC143C"
                strokeOpacity="0.8"
                strokeWidth="1.5"
                initial={{ pathLength: 0 }}
                animate={{ pathLength: 1 }}
                transition={{ duration: 2, ease: 'easeInOut' }}
              />
              <motion.path
                className="drop-shadow-[0_0_4px_rgba(255,176,0,0.3)]"
                d="M0,75 Q55,15 105,85 T205,35 T305,115 T405,25 T505,80 T605,50 T705,90 T805,40 T905,100 T1005,65"
                fill="none"
                stroke="#FFB000"
                strokeOpacity="0.4"
                strokeWidth="0.8"
                initial={{ pathLength: 0 }}
                animate={{ pathLength: 1 }}
                transition={{ duration: 2.5, ease: 'easeInOut', delay: 0.3 }}
              />
            </svg>
          </div>
        </section>
      </main>

      {/* ========== FOOTER ========== */}
      <footer className="fixed bottom-0 left-0 w-full z-50 flex justify-between items-center h-10 border-t border-[#222222] bg-[#0d0d0d] px-6">
        <div className="flex items-center gap-4">
          <motion.div
            className="flex items-center gap-2 px-4 text-[#FFB000] font-mono text-[12px]"
            whileHover={{ scale: 1.02 }}
          >
            <span className="material-symbols-outlined text-sm">equalizer</span>
            <span>LUFS: -14.2</span>
          </motion.div>
          <div className="flex items-center gap-2 px-4 text-[#4d4d4d] font-mono text-[12px]">
            <span className="material-symbols-outlined text-sm">priority_high</span>
            <span>PEAK: -0.1dB</span>
          </div>
          <div className="flex items-center gap-2 px-4 text-[#4d4d4d] font-mono text-[12px]">
            <span className="material-symbols-outlined text-sm">speed</span>
            <span>RMS: -16.4</span>
          </div>
        </div>
        <div className="flex items-center gap-4">
          <motion.div
            className="text-[10px] font-mono text-[#FFB000]"
            animate={{ opacity: [0.8, 1, 0.8] }}
            transition={{ repeat: Infinity, duration: 2 }}
          >
            {connectionStatus === ConnectionState.CONNECTED
              ? '[CONNECTED] // READY'
              : '[OFFLINE] // AWAITING SIGNAL'}
          </motion.div>
          <motion.div
            className={`h-4 w-4 ${
              connectionStatus === ConnectionState.CONNECTED
                ? 'bg-[#FFB000] led-amber-active'
                : 'bg-[#4d4d4d]'
            }`}
            animate={connectionStatus === ConnectionState.CONNECTED ? {
              boxShadow: ['0 0 8px #FFB000', '0 0 15px #FFB000', '0 0 8px #FFB000']
            } : {}}
            transition={{ repeat: Infinity, duration: 1.5 }}
          />
        </div>
      </footer>
    </div>
  )
}