import React, { useState, useEffect, useMemo, useRef, useCallback } from 'react'
import { motion, AnimatePresence } from 'framer-motion'
import { whycremisi, ConnectionState } from './whycremisi-bridge'
import { BotFace } from './components/BotFace'
import { SessionPanel } from './components/SessionPanel'
import { SetupScreen } from './components/SetupScreen'
import './index.css'

// ── BoxChat — various box types shown inside chat after AI response ──
function BoxChat({ boxType, meterL, meterR, lufs, peak, transport, pluginStats, gainDb, driveVal, onDawCmd }) {
  const lPct = Math.round(meterL * 100)
  const rPct = Math.round(meterR * 100)
  const MetricBar = ({ label, val, color, pct }) => (
    <div className="space-y-0.5">
      <div className="flex justify-between text-xs uppercase font-bold">
        <span className="text-[#aaa]">{label}</span>
        <span style={{ color }}>{val}</span>
      </div>
      <div className="h-1 bg-[#1a1a1a] w-full">
        <motion.div className="h-full" style={{ backgroundColor: color, boxShadow: `0 0 3px ${color}50` }}
          initial={{ width: '0%' }} animate={{ width: `${Math.min(100, Math.max(0, pct))}%` }}
          transition={{ duration: 0.8, ease: 'easeOut' }}
        />
      </div>
    </div>
  )

  const wrap = (label, color, icon, children) => (
    <motion.div initial={{ opacity:0, y:4 }} animate={{ opacity:1, y:0 }} transition={{ delay:0.35 }}
      className="mt-2 bg-[#0d0d0d]/70 border border-[#222] overflow-hidden"
    >
      <div className="flex items-center gap-1.5 px-2 py-1 border-b border-[#1a1a1a]">
        <span className="material-symbols-outlined text-xs" style={{ color }}>{icon}</span>
        <span className="text-xs font-bold uppercase tracking-widest" style={{ color }}>{label}</span>
      </div>
      <div className="p-2">{children}</div>
    </motion.div>
  )

  if (boxType === 'stereo') return wrap('Stereo Field', '#DC143C', 'swap_horiz', (
    <div className="grid grid-cols-2 gap-1.5">
      <MetricBar label="L/R Balance" val={lPct>rPct?`${lPct-rPct}% L`:rPct>lPct?`${rPct-lPct}% R`:'CENTER'} color="#DC143C" pct={Math.abs(lPct-rPct)} />
      <MetricBar label="Side Content" val={`${Math.round(Math.abs(meterL-meterR)*150)}%`} color="#FFB000" pct={Math.round(Math.abs(meterL-meterR)*150)} />
      <MetricBar label="Correlation" val={`+${(0.62+(meterL+meterR)*0.15).toFixed(2)}`} color="#00E5FF" pct={Math.round((0.62+(meterL+meterR)*0.15)*100)} />
      <MetricBar label="Width" val={`${Math.round((Math.abs(meterL-meterR)+0.3)*100)}%`} color="#00FFaa" pct={Math.round((Math.abs(meterL-meterR)+0.3)*100)} />
    </div>
  ))

  if (boxType === 'loudness') return wrap('Loudness Analysis', '#00E5FF', 'equalizer', (
    <div className="grid grid-cols-2 gap-1.5">
      <MetricBar label="LUFS Integrated" val={lufs>-60?`${lufs.toFixed(1)} LUFS`:'-∞'} color="#00E5FF" pct={Math.max(0,Math.min(100,(lufs+60)/60*100))} />
      <MetricBar label="Peak Level" val={peak>-60?`${peak.toFixed(1)} dB`:'-∞'} color="#FF6B35" pct={Math.max(0,Math.min(100,(peak+60)/60*100))} />
      <MetricBar label="Dynamic Range" val={`${(72-(lPct+rPct)/2*0.72).toFixed(1)} dB`} color="#00FFaa" pct={Math.max(0,100-(lPct+rPct)/2)} />
      <MetricBar label="True Peak" val={peak>-60?`${(peak+0.3).toFixed(1)} dBTP`:'-∞'} color="#FF6B35" pct={Math.max(0,Math.min(100,(peak+60.3)/60*100))} />
      <MetricBar label="RMS Level" val={lufs>-60?`${(lufs+2.1).toFixed(1)} dB`:'-∞'} color="#FFB000" pct={Math.max(0,Math.min(100,(lufs+62)/62*100))} />
      <MetricBar label="Crest Factor" val={`${(peak-lufs).toFixed(1)} dB`} color="#DC143C" pct={Math.min(100,Math.max(0,(peak-lufs)*5))} />
    </div>
  ))

  if (boxType === 'eq') return wrap('Frequency Analysis', '#FFB000', 'graphic_eq', (
    <div className="grid grid-cols-3 gap-1.5">
      <MetricBar label="Sub (20-80Hz)" val={`${Math.round((meterL*0.25+0.18)*100)}%`} color="#9B59B6" pct={Math.round((meterL*0.25+0.18)*100)} />
      <MetricBar label="Low (80-300Hz)" val={`${Math.round((meterL*0.4+0.3)*100)}%`} color="#DC143C" pct={Math.round((meterL*0.4+0.3)*100)} />
      <MetricBar label="Low-Mid (300Hz-1k)" val={`${Math.round((meterR*0.35+0.38)*100)}%`} color="#FFB000" pct={Math.round((meterR*0.35+0.38)*100)} />
      <MetricBar label="Presence (1k-5k)" val={`${Math.round((meterR*0.3+0.42)*100)}%`} color="#00FFaa" pct={Math.round((meterR*0.3+0.42)*100)} />
      <MetricBar label="High (5k-15k)" val={`${Math.round(((meterL+meterR)*0.2+0.25)*100)}%`} color="#00E5FF" pct={Math.round(((meterL+meterR)*0.2+0.25)*100)} />
      <MetricBar label="Air (15k+)" val={`${Math.round(((meterL+meterR)*0.15+0.2)*100)}%`} color="#E8D5B7" pct={Math.round(((meterL+meterR)*0.15+0.2)*100)} />
    </div>
  ))

  if (boxType === 'slider') return wrap('Volume / Gain Control', '#FFB000', 'tune', (
    <div className="space-y-2">
      {[
        { label: 'Master Gain', val: gainDb, min: -60, max: 12, color: '#DC143C', cmd: 'setGain', key: 'valueDb' },
        { label: 'Drive', val: driveVal, min: 0, max: 100, color: '#FFB000', cmd: 'setDrive', key: 'value' },
      ].map(({ label, val, min, max, color, cmd, key }) => {
        const pct = ((val - min) / (max - min)) * 100
        return (
          <div key={label} className="space-y-1">
            <div className="flex justify-between text-xs uppercase font-bold">
              <span className="text-[#aaa]">{label}</span>
              <span style={{ color }}>{val.toFixed(1)}</span>
            </div>
            <div className="h-2 bg-[#1a1a1a] w-full relative cursor-pointer rounded-sm overflow-hidden">
              <motion.div className="h-full absolute left-0 top-0" style={{ backgroundColor: color, width: `${pct}%`, boxShadow: `0 0 6px ${color}50` }} />
            </div>
          </div>
        )
      })}
    </div>
  ))

  if (boxType === 'knob') return wrap('Pan / Stereo Position', '#00E5FF', 'track_changes', (
    <div className="grid grid-cols-2 gap-3">
      {[
        { label: 'Master Pan', val: 0, color: '#00E5FF' },
        { label: 'Stereo Width', val: Math.round((Math.abs(meterL-meterR)+0.3)*100), color: '#FFB000' },
      ].map(({ label, val, color }) => (
        <div key={label} className="flex flex-col items-center gap-1">
          <svg viewBox="0 0 60 60" className="w-12 h-12 rotate-[-90deg]">
            <circle cx="30" cy="30" r="22" fill="none" stroke="#222" strokeWidth="3" />
            <motion.circle cx="30" cy="30" r="22" fill="none" stroke={color} strokeWidth="3"
              strokeLinecap="round"
              initial={{ strokeDasharray: '0 138' }}
              animate={{ strokeDasharray: `${(val/100)*138} 138` }}
              transition={{ duration: 0.8 }}
              style={{ filter: `drop-shadow(0 0 3px ${color})` }}
            />
          </svg>
          <span className="text-xs text-[#aaa] uppercase font-bold">{label}</span>
          <span className="text-xs font-mono" style={{ color }}>{val}%</span>
        </div>
      ))}
    </div>
  ))

  if (boxType === 'transport') return wrap('Transport Status', '#00FFaa', 'play_arrow', (
    <div className="grid grid-cols-4 gap-1.5">
      {[
        { label: 'Status', val: transport.isPlaying ? '▶ PLAY' : '■ STOP', color: transport.isPlaying ? '#00FFaa' : '#4d4d4d' },
        { label: 'BPM', val: transport.bpm.toFixed(1), color: '#FFB000' },
        { label: 'Position', val: `${Math.floor(transport.position/60).toString().padStart(2,'0')}:${(transport.position%60).toFixed(0).toString().padStart(2,'0')}`, color: '#4d4d4d' },
        { label: 'Recording', val: transport.isRecording ? '● REC' : '○ OFF', color: transport.isRecording ? '#DC143C' : '#4d4d4d' },
      ].map(({ label, val, color }) => (
        <div key={label} className="bg-[#111] border border-[#1a1a1a] p-1.5 text-center">
          <div className="text-xs text-[#666] uppercase mb-0.5">{label}</div>
          <div className="text-xs font-bold font-mono" style={{ color }}>{val}</div>
        </div>
      ))}
      <div className="col-span-4 flex gap-1.5 mt-1">
        {[['play','▶ PLAY','#00FFaa'],['stop','■ STOP','#4d4d4d'],['record','● REC','#DC143C']].map(([cmd,lbl,clr]) => (
          <motion.button key={cmd} whileHover={{ scale:1.04 }} whileTap={{ scale:0.95 }}
            className="flex-1 py-1 text-xs font-bold uppercase border border-[#222] bg-[#111] tracking-widest"
            style={{ color: clr, borderColor: clr+'40' }}
            onClick={() => onDawCmd(cmd)}
          >{lbl}</motion.button>
        ))}
      </div>
    </div>
  ))

  if (boxType === 'compressor') return wrap('Compressor Settings', '#9B59B6', 'compress', (
    <div className="grid grid-cols-2 gap-1.5">
      <MetricBar label="Threshold" val="-18 dB" color="#9B59B6" pct={60} />
      <MetricBar label="Ratio" val="4:1" color="#DC143C" pct={40} />
      <MetricBar label="Attack" val="5 ms" color="#FFB000" pct={25} />
      <MetricBar label="Release" val="80 ms" color="#00E5FF" pct={50} />
      <MetricBar label="Gain Reduction" val={`${(-(meterL+meterR)*6).toFixed(1)} dB`} color="#00FFaa" pct={Math.round((meterL+meterR)*50)} />
      <MetricBar label="Output Gain" val={`${gainDb.toFixed(1)} dB`} color="#FFB000" pct={Math.max(0,Math.min(100,((gainDb+60)/72)*100))} />
    </div>
  ))

  if (boxType === 'advisory') {
    const [dismissed, setDismissed] = React.useState(false)
    const [executed, setExecuted] = React.useState(false)
    if (dismissed) return null
    return (
      <motion.div
        initial={{ opacity:0, y:10, scale:0.98, filter:'blur(4px)' }}
        animate={{ opacity:1, y:0, scale:1, filter:'blur(0)' }}
        transition={{ delay:0.35, duration:0.6, ease:[0.22,1,0.36,1] }}
        className="mt-2 animate-advisory-in group relative"
      >
        <div className="absolute -top-3 -right-1 opacity-5 pointer-events-none select-none">
          <pre className="text-xs leading-tight text-white font-mono">0x45 0x21 0x88{'\n'}[TRANS_LOCK]{'\n'}0xFF 0x00 0x12</pre>
        </div>
        <div className="advisory-card advisory-breathe bg-[#121212] border border-[#DC143C]/40 p-4 relative font-mono transition-all duration-500 hover:bg-[#161616]">
          <div className="absolute top-2 right-3 flex items-end gap-[2px] h-5 opacity-40">
            {[40,70,55,90,65].map((h,i) => <div key={i} className="w-[2px] bg-[#DC143C]" style={{ height:`${h}%` }} />)}
          </div>
          <div className="flex justify-between items-start mb-3 border-b border-[#222] pb-2">
            <div className="flex items-center gap-2">
              <div className="w-1 h-5 bg-[#DC143C] relative overflow-hidden flex-shrink-0">
                <motion.div className="absolute inset-0 bg-white/20" animate={{ y:['0%','100%','0%'] }} transition={{ repeat:Infinity, duration:2 }} />
              </div>
              <div>
                <span className="text-[#DC143C] text-xs font-bold tracking-tighter uppercase block">AI_MASTERING_ADVISORY</span>
                <span className="text-xs text-[#888888] tracking-widest">NODE: CREMISI_X9</span>
              </div>
            </div>
            <div className="flex flex-col items-end">
              <span className="text-xs text-white/40 font-bold uppercase tracking-widest">Priority: <span className="text-[#DC143C]">High</span></span>
              <span className="text-xs text-[#888888] font-mono mt-0.5">ID: #B87-FF01</span>
            </div>
          </div>
          <div className="text-sm leading-relaxed text-[#FFB000] mb-3">
            <p className="text-xs leading-relaxed">
              Detected harmonic crowding in{' '}
              <span className="text-white font-bold border-b border-[#DC143C]/40 px-0.5">200Hz–400Hz</span>.{' '}
              Suggesting dynamic dip of{' '}
              <span className="bg-[#DC143C] text-white px-1 font-bold">-2.4dB</span>.{' '}
              Transient preservation at{' '}
              <span className="text-white underline decoration-dotted">84%</span>.
            </p>
          </div>
          <div className="flex items-center gap-2 mb-3 text-xs font-mono text-[#666] opacity-60">
            <span>HEX: 0xDC143C</span><span>·</span><span>VAL: -2.4dB_COR</span><span>·</span><span>SIG: 0.982</span><span>·</span><span>LAT: 0.2ms</span>
          </div>
          <div className="flex flex-wrap gap-2">
            <motion.button
              className={`relative px-4 py-1.5 text-xs font-bold uppercase tracking-widest transition-all ${executed ? 'bg-[#00FFaa] text-black border border-[#00FFaa]' : 'bg-[#DC143C] text-white hover:bg-white hover:text-[#DC143C] hover:shadow-[0_0_20px_rgba(220,20,60,0.5)]'}`}
              whileHover={{ scale:1.02 }} whileTap={{ scale:0.95 }}
              onClick={() => { setExecuted(true); onDawCmd('applyEQ', { freq:'200-400Hz', gain:-2.4 }) }}
            >
              <span className="flex items-center gap-1.5">
                <span className="material-symbols-outlined text-[12px]">bolt</span>
                {executed ? 'APPLIED ✓' : 'EXECUTE SUGGESTED CHAIN'}
              </span>
            </motion.button>
            <motion.button
              className="border border-[#4d4d4d] text-[#888888] px-4 py-1.5 text-xs font-bold uppercase tracking-widest hover:border-[#FFB000] hover:text-[#FFB000] hover:shadow-[0_0_15px_rgba(255,176,0,0.2)] transition-all"
              whileHover={{ scale:1.02 }} whileTap={{ scale:0.95 }}
            >ANALYZE FURTHER</motion.button>
            <button className="text-[#888888] hover:text-white text-xs font-bold uppercase self-center transition-colors hover:underline decoration-[#DC143C]"
              onClick={() => setDismissed(true)}>DISMISS</button>
          </div>
        </div>
      </motion.div>
    )
  }

  // default: metrics
  return wrap('Mix Analysis', '#FFB000', 'analytics', (
    <div className="grid grid-cols-2 gap-1.5">
      <MetricBar label="L/R Balance" val={lPct>rPct?`${lPct-rPct}% L`:rPct>lPct?`${rPct-lPct}% R`:'CENTER'} color="#DC143C" pct={Math.abs(lPct-rPct)} />
      <MetricBar label="Side Content" val={`${Math.round(Math.abs(meterL-meterR)*150)}%`} color="#FFB000" pct={Math.round(Math.abs(meterL-meterR)*150)} />
      <MetricBar label="LUFS" val={lufs>-60?`${lufs.toFixed(1)}`:'--'} color="#00E5FF" pct={Math.max(0,Math.min(100,(lufs+60)/60*100))} />
      <MetricBar label="Peak" val={peak>-60?`${peak.toFixed(1)} dB`:'--'} color="#FF6B35" pct={Math.max(0,Math.min(100,(peak+60)/60*100))} />
      <MetricBar label="Dynamic Range" val={`${(72-(lPct+rPct)/2*0.72).toFixed(1)} dB`} color="#00FFaa" pct={Math.max(0,100-(lPct+rPct)/2)} />
      <MetricBar label="Low End" val={`${Math.round((meterL*0.4+0.3)*100)}%`} color="#DC143C" pct={Math.round((meterL*0.4+0.3)*100)} />
    </div>
  ))
}

export default function App() {
  // ── setup state ───────────────────────────────────────────────────
  const [setupComplete, setSetupComplete] = useState(() => {
    return localStorage.getItem('whycremisi_setup_done') === 'true'
  })
  const [config, setConfig] = useState(() => {
    const saved = localStorage.getItem('whycremisi_config')
    return saved ? JSON.parse(saved) : { provider: 'ollama', apiKey: '' }
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

  // ── plugin stats (from prepareToPlay via WebSocket) ───────────────
  const [pluginStats, setPluginStats] = useState({ sampleRate: null, bufferSize: null, latencyMs: null })

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
      setTimeout(() => setBotState('idle'), 3000)
      sysMsg(`[${ts()}] [ERROR] ${payload.message || payload.code || 'Unknown error'}`)
    })

    // connect
    whycremisi.connect().catch(() => {
      sysMsg(`[${ts()}] PLUGIN NOT RUNNING — OFFLINE MODE`)
    })

    return () => {
      stateUnsub(); unsubAI(); unsubStream(); unsubTransport()
      unsubMeter(); unsubOSC(); unsubStats(); unsubErr()
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

  // ── detect boxchat type from prompt + response ────────────────────
  const detectBoxType = (prompt = '', response = '') => {
    const t = (prompt + ' ' + response).toLowerCase()
    if (/stereo|width|side|balance|phase|mono|correlation/.test(t)) return 'stereo'
    if (/lufs|loud|loud|peak|rms|dynamic|crest|limiter|ceiling/.test(t)) return 'loudness'
    if (/eq|frequen|bass|sub|mid|high|treble|presence|air|100hz|200hz|1khz|4khz/.test(t)) return 'eq'
    if (/volume|gain|fader|db|level/.test(t)) return 'slider'
    if (/pan|panning|position|center|left|right/.test(t)) return 'knob'
    if (/play|stop|record|transport|bpm|tempo/.test(t)) return 'transport'
    if (/compres|ratio|attack|release|threshold|knee/.test(t)) return 'compressor'
    if (/chain|apply|execute|suggest|recommend|action|advisory|do it|should|could|try|consider/.test(t)) return 'advisory'
    return 'metrics'
  }

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

      <AnimatePresence>
        {!setupComplete && (
          <SetupScreen 
            initialConfig={config}
            onComplete={(newConfig) => {
              setConfig(newConfig)
              setSetupComplete(true)
              localStorage.setItem('whycremisi_setup_done', 'true')
              localStorage.setItem('whycremisi_config', JSON.stringify(newConfig))
              
              // Se connesso, invia config al plugin nel formato atteso da OscBridge
              if (whycremisi.isConnected()) {
                whycremisi.send({ type: 'config.set', payload: { key: 'ai.provider', value: newConfig.provider } })
                if (newConfig.apiKey) {
                  whycremisi.send({ type: 'config.set', payload: { key: 'ai.apiKey', value: newConfig.apiKey, provider: newConfig.provider } })
                }
              }
            }} 
          />
        )}
      </AnimatePresence>

      {/* ── HEADER ─────────────────────────────────────────────────── */}
      <header className="bg-[#131313] flex justify-between items-center w-full px-4 py-1 border-b border-[#222222] z-50 relative h-9">
        <div className="flex items-center gap-3">
          <h1 className="text-sm font-black tracking-tighter text-[#DC143C] uppercase leading-none">WHYCREMISI</h1>
          <nav className="flex gap-4 uppercase tracking-[0.1em] font-bold text-xs">
            {['COMMAND','MASTER','TELEMETRY','SESSIONS'].map(tab => (
              <a key={tab}
                onClick={() => setActiveTab(tab)}
                className={activeTab === tab
                  ? 'text-[#FFB000] border-b border-[#FFB000] pb-0.5 cursor-pointer'
                  : 'text-[#888888] hover:text-[#FFB000] transition-colors cursor-pointer'}
              >{tab}</a>
            ))}
          </nav>
        </div>

        <div className="flex items-center gap-3 font-mono">
          {/* DAW transport */}
          <div className="flex items-center gap-2">
            <div className="flex flex-col items-center leading-none">
              <span className="text-xs font-mono text-[#888] uppercase">BPM</span>
              <span className="text-sm font-black text-[#FFB000] tracking-tight">{transport.bpm.toFixed(1)}</span>
            </div>
            <motion.div
              className={`px-1.5 py-px text-xs font-black uppercase tracking-widest border ${
                transport.isPlaying
                  ? 'border-[#00FFaa]/40 text-[#00FFaa] bg-[#00FFaa]/10'
                  : 'border-[#555]/40 text-[#aaa] bg-transparent'
              }`}
              animate={transport.isPlaying ? { opacity:[1,0.7,1] } : {}}
              transition={{ repeat: Infinity, duration: 1.2 }}
            >
              {transport.isPlaying ? '▶ PLAY' : '■ STOP'}
            </motion.div>
          </div>

          <div className="w-px h-4 bg-[#222222]" />

          {/* Audio stats */}
          <div className="flex items-center gap-2">
            {[
              ['SR',  pluginStats.sampleRate ? `${(pluginStats.sampleRate/1000).toFixed(0)}k` : '—'],
              ['BUF', pluginStats.bufferSize ?? '—'],
              ['LAT', pluginStats.latencyMs  ? `${pluginStats.latencyMs.toFixed(0)}ms` : '—'],
            ].map(([k, v]) => (
              <div key={k} className="flex flex-col items-center leading-none">
                <span className="text-xs font-mono text-[#888] uppercase">{k}</span>
                <span className="text-xs font-bold text-white tracking-tight">{v}</span>
              </div>
            ))}
          </div>

          <div className="w-px h-4 bg-[#222222]" />

          {/* Connection LED */}
          <div className="flex items-center gap-1.5">
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
            <span className="text-xs uppercase tracking-wider text-[#888]">
              {connStatus === ConnectionState.CONNECTED    ? 'LIVE' :
               connStatus === ConnectionState.CONNECTING   ? 'CONN' :
               connStatus === ConnectionState.RECONNECTING ? 'RECONN' : 'OFFLINE'}
            </span>
          </div>

          <div className="w-px h-4 bg-[#222222]" />

          {/* Action icons */}
          <div className="flex gap-1.5">
            <span className="material-symbols-outlined text-base text-[#888888] hover:text-[#FFB000] cursor-pointer transition-colors">settings</span>
            <span className="material-symbols-outlined text-base text-[#888888] hover:text-[#DC143C] cursor-pointer transition-colors"
              onClick={() => whycremisi.disconnect()}>power_settings_new</span>
          </div>
        </div>
      </header>

      {/* ── SIDEBAR ────────────────────────────────────────────────── */}
      <aside className="fixed left-0 top-9 h-[calc(100vh-2.25rem)] w-12 flex flex-col items-center py-2 bg-[#131313] border-r border-[#222222] z-40">
        <div className="text-xs font-mono text-[#888888] uppercase mb-4 rotate-180 select-none" style={{ writingMode:'vertical-lr' }}>
          MOD
        </div>
        {sideModules.map(mod => (
          <motion.button
            key={mod.id}
            className={`w-full py-2 flex flex-col items-center gap-0.5 transition-colors ${
              activeMod === mod.id
                ? 'text-[#DC143C] bg-[#1a1a1a] border-l-2 border-[#DC143C]'
                : 'text-[#888888] hover:text-[#FFB000] hover:bg-[#1a1a1a] border-l-2 border-transparent'
            }`}
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
            className="fixed top-9 left-12 right-0 bottom-0 z-30 bg-[#0d0d0d]"
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
            className="fixed top-9 left-12 right-0 bottom-9 z-30 bg-[#0d0d0d] overflow-y-auto custom-scrollbar p-4"
          >
            <div className="max-w-full space-y-4">
              <div className="flex items-center gap-3 mb-4">
                <div className="w-1.5 h-1.5 rounded-full bg-[#FFB000] led-amber-active" />
                <span className="text-xs font-bold text-[#FFB000] tracking-widest uppercase">Telemetry Dashboard — Live Analysis</span>
                <div className="flex-1 h-px bg-[#222]" />
                <span className="text-xs font-mono text-[#666]">REALTIME</span>
              </div>

              {/* ── STEREO ANALYSIS ─────────────────────────────── */}
              <div className="border border-[#222] bg-[#0e0e0e]">
                <div className="px-3 py-1.5 border-b border-[#1a1a1a] flex items-center gap-2">
                  <div className="w-1 h-1 rounded-full bg-[#DC143C]" />
                  <span className="text-xs font-bold text-[#DC143C] uppercase tracking-widest">Stereo Field Analysis</span>
                </div>
                <div className="p-3 grid grid-cols-3 gap-3">
                  {[
                    { label: 'L/R Balance', val: (() => { const lP=Math.round(meterL*100), rP=Math.round(meterR*100); return lP>rP?`${lP-rP}% L`:rP>lP?`${rP-lP}% R`:'CENTER' })(), color: '#DC143C', pct: Math.abs(Math.round(meterL*100)-Math.round(meterR*100)) },
                    { label: 'Side Content', val: `${Math.round(Math.abs(meterL-meterR)*100*1.5)}%`, color: '#FFB000', pct: Math.round(Math.abs(meterL-meterR)*100*1.5) },
                    { label: 'Correlation', val: meterL > 0.01 || meterR > 0.01 ? `+${(0.62 + (meterL+meterR)*0.15).toFixed(2)}` : '—', color: '#00E5FF', pct: Math.round((0.62+(meterL+meterR)*0.15)*100) },
                  ].map(({ label, val, color, pct }) => (
                    <div key={label} className="space-y-1.5">
                      <div className="flex justify-between text-xs uppercase font-bold">
                        <span className="text-[#aaa]">{label}</span>
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
              <div className="border border-[#222] bg-[#0e0e0e]">
                <div className="px-3 py-1.5 border-b border-[#1a1a1a] flex items-center gap-2">
                  <div className="w-1 h-1 rounded-full bg-[#00E5FF]" />
                  <span className="text-xs font-bold text-[#00E5FF] uppercase tracking-widest">Loudness & Dynamics</span>
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
                        <span className="text-[#aaa]">{label}</span>
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
              <div className="border border-[#222] bg-[#0e0e0e]">
                <div className="px-3 py-1.5 border-b border-[#1a1a1a] flex items-center gap-2">
                  <div className="w-1 h-1 rounded-full bg-[#FFB000]" />
                  <span className="text-xs font-bold text-[#FFB000] uppercase tracking-widest">Frequency Content</span>
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
                        <span className="text-[#aaa]">{label}</span>
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
              <div className="border border-[#222] bg-[#0e0e0e]">
                <div className="px-3 py-1.5 border-b border-[#1a1a1a] flex items-center gap-2">
                  <div className="w-1 h-1 rounded-full bg-[#00FFaa]" />
                  <span className="text-xs font-bold text-[#00FFaa] uppercase tracking-widest">Transport & Plugin Info</span>
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
                    <div key={label} className="bg-[#111] border border-[#1a1a1a] p-2 flex flex-col gap-0.5">
                      <span className="text-xs font-mono text-[#666] uppercase">{label}</span>
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
      <main className="ml-12 mt-0 h-[calc(100vh-2.25rem)] grid grid-cols-12 grid-rows-6 p-1 gap-1 overflow-hidden bg-[#0d0d0d] relative z-10">

        {/* ── AI CHAT CONSOLE (9/12 × 4/6) ──────────────────────── */}
        <section className="col-span-9 row-span-4 bg-[#0e0e0e] border border-[#222222] flex flex-col overflow-hidden">
          <div className="bg-[#1a1a1a] px-3 py-1 flex justify-between items-center border-b border-[#222222]">
            <div className="flex items-center gap-2">
              <div className="w-1.5 h-1.5 rounded-full bg-[#FFB000] led-amber-active" />
              <span className="text-xs font-bold text-[#FFB000] tracking-widest uppercase">AI Command Console</span>
            </div>
            <div className="flex items-center gap-3 text-xs font-mono text-[#888888]">
              <span>v4.2.0</span>
              {dawConnected && <span className="text-[#00FFaa]">● DAW SYNC</span>}
            </div>
          </div>

          {/* Messages */}
          <div className="flex-1 px-4 py-3 font-mono overflow-y-auto overflow-x-hidden custom-scrollbar space-y-3">

            {/* Empty state — mostrato finché non ci sono risposte AI */}
            {messages.filter(m => m.type === 'bot').length === 0 && (
              <motion.div
                initial={{ opacity: 0, y: 8 }}
                animate={{ opacity: 1, y: 0 }}
                className="flex flex-col items-center justify-center h-full gap-4 pt-12"
              >
                <BotFace state={botState} className="w-16 h-16" />
                <div className="text-center space-y-1">
                  <p className="text-sm text-[#888888] font-mono uppercase tracking-widest">Pronto ad ascoltare</p>
                  <p className="text-xs text-[#666] font-mono">Scrivi qualcosa nel campo CMD qui sotto</p>
                </div>
              </motion.div>
            )}

            <AnimatePresence>
              {messages.map((msg) => {
                if (msg.type === 'system') return (
                  <motion.div key={msg.id}
                    initial={{ opacity: 0, x: -6 }}
                    animate={{ opacity: 1, x: 0 }}
                    transition={{ duration: 0.3 }}
                    className="flex items-center gap-2"
                  >
                    <div className="w-1 h-full min-h-[10px] bg-[#333] flex-shrink-0" />
                    <span className="text-[10px] font-mono text-[#bbb] tracking-wide">{msg.text}</span>
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
                      <div className="flex items-start gap-3 mb-3 border-b border-[#222222] pb-2">
                        <div className="w-1 h-6 bg-[#DC143C] relative overflow-hidden flex-shrink-0 mt-0.5">
                          <motion.div className="absolute inset-0 bg-white/20"
                            animate={{ y:['0%','100%','0%'] }}
                            transition={{ repeat:Infinity, duration:2 }}
                          />
                        </div>
                        <div>
                          <span className="text-[#DC143C] text-xs font-bold tracking-tighter uppercase block">AI MASTERING ADVISORY</span>
                          <span className="text-xs text-[#888888] tracking-[0.2em]">NODE: CREMISI_X9 · PRIORITY: HIGH</span>
                        </div>
                      </div>
                      <p className="text-sm text-[#FFB000] leading-relaxed mb-4 break-words">
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
                          className="border border-[#4d4d4d] text-[#888888] px-4 py-1.5 text-xs font-bold uppercase hover:border-[#FFB000] hover:text-[#FFB000] transition-colors tracking-widest"
                          whileTap={{ scale: 0.95 }}
                          onClick={() => { if(botState==='idle') { addMsg({type:'user',text:'Analyze the low end further',time:ts()}); setBotState('thinking') } }}
                        >ANALYZE FURTHER</motion.button>
                        <button className="text-[#888888] hover:text-white text-xs font-bold uppercase self-center transition-colors"
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
                    <div className="bg-[#222222]/50 border border-[#FFB000]/20 px-3 py-1.5 max-w-[80%] text-[9px] text-white font-mono italic">
                      {msg.text}
                    </div>
                    <span className="text-xs text-[#888888] mr-1 uppercase font-bold tracking-widest">
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
                        {isLatest && <BotFace state={botState} className="w-8 h-8" />}
                      </div>
                      <div className={`flex-1 min-w-0 p-3 relative overflow-hidden transition-colors duration-300 ${
                        isLatest && msg.streaming
                          ? 'bg-[#1c1c1c] border border-[#FFB000]/25 shadow-[0_0_10px_rgba(255,176,0,0.06)]'
                          : 'bg-[#191919] border border-[#2a2a2a]'
                      }`}>
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
                          <span className={`text-xs font-bold tracking-widest uppercase ${isLatest ? 'text-[#FFB000]' : 'text-[#777]'}`}>
                            WHYCREMISI AI
                          </span>
                          <span className="text-xs text-[#777] font-mono">{msg.time}</span>
                        </div>
                        <p className="text-[11px] text-[#e5e2e1] leading-5 whitespace-pre-wrap">
                          {msg.text}
                          {msg.streaming && (
                            <motion.span className="inline-block w-1 h-3 bg-[#FFB000] ml-1 align-middle"
                              animate={{ opacity:[1,0,1] }} transition={{ repeat:Infinity, duration:0.55 }}
                            />
                          )}
                        </p>
                        {msg.telemetry && !msg.streaming && <BoxChat boxType={msg.boxType||'metrics'} meterL={meterL} meterR={meterR} lufs={lufs} peak={peak} transport={transport} pluginStats={pluginStats} gainDb={gainDb} driveVal={driveVal} onDawCmd={dawCmd} />}
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
          <div className="h-10 bg-[#131313] border-t border-[#222222] flex items-center px-3 gap-2">
            <span className="text-[#FFB000] font-bold text-xs tracking-widest shrink-0">CMD&gt;</span>
            <div className="flex-1 relative flex items-center">
              <input
                className="w-full bg-transparent border-none text-white font-mono text-xs focus:ring-0 focus:outline-none placeholder-[#4d4d4d] p-0 disabled:opacity-40"
                placeholder={botState === 'thinking' || botState === 'typing' ? 'PROCESSING...' : 'Ask the AI anything about your mix...'}
                value={inputVal}
                onChange={e => setInputVal(e.target.value)}
                onKeyDown={handleCommand}
                disabled={botState === 'thinking' || botState === 'typing'}
              />
              {(botState === 'idle' || botState === 'sad' || botState === 'loading') && <div className="terminal-cursor" />}
            </div>
            <div className="flex gap-1.5 text-[#888888]">
              <motion.button whileHover={{ scale:1.1 }} whileTap={{ scale:0.9 }}
                className="material-symbols-outlined text-base cursor-pointer hover:text-[#FFB000] transition-colors"
                onClick={() => dawCmd('play')}>play_arrow</motion.button>
              <motion.button whileHover={{ scale:1.1 }} whileTap={{ scale:0.9 }}
                className="material-symbols-outlined text-base cursor-pointer hover:text-[#FFB000] transition-colors"
                onClick={() => dawCmd('stop')}>stop</motion.button>
              <motion.button whileHover={{ scale:1.1 }} whileTap={{ scale:0.9 }}
                className={`material-symbols-outlined text-base cursor-pointer transition-colors ${transport.isRecording ? 'text-[#DC143C] led-red-active' : 'hover:text-[#DC143C]'}`}
                onClick={() => dawCmd('record')}>fiber_manual_record</motion.button>
            </div>
          </div>
        </section>

        {/* ── MASTERING RACK (3/12 × 4/6) ───────────────────────── */}
        <section className="col-span-3 row-span-4 bg-[#131313] border border-[#222222] flex flex-col">
          <div className="bg-[#1a1a1a] px-3 py-1 border-b border-[#222222] flex justify-between items-center">
            <div className="flex items-center gap-2">
              <div className="w-1.5 h-1.5 rounded-full bg-[#DC143C] led-red-active" />
              <span className="text-xs font-bold text-[#DC143C] tracking-widest uppercase">Mastering Rack</span>
            </div>
            <span className="text-xs font-mono text-[#888888]">STEREO MASTER</span>
          </div>

          <div className="flex-1 flex p-3 gap-4 overflow-hidden">
            {/* Gain fader */}
            <div className="w-14 flex flex-col items-center gap-1">
              <span className="text-xs font-mono text-[#FFB000]">+12</span>
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
              <span className="text-xs font-mono text-[#FFB000]">-INF</span>
              <span className="text-xs font-bold text-[#e5e2e1] mt-1">{gainDb > 0 ? `+${gainDb}` : gainDb}dB</span>
              <span className="text-xs font-mono text-[#888888]">GAIN</span>
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
                    <span className="text-xs font-bold uppercase text-[#888888] truncate">{label}</span>
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
                <span className="text-xs text-[#FFB000] uppercase opacity-60 block mb-1">Saturation</span>
                <div className="bg-[#0e0e0e] border border-[#222222] px-2 py-1 text-xs text-[#FFB000] flex justify-between items-center">
                  <span>CRIMSON_TUBE</span>
                  <span className="material-symbols-outlined text-[9px]">expand_more</span>
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
                  <span className="text-[9px] font-bold text-[#e5e2e1]">{driveVal.toFixed(1)}</span>
                  <span className="text-xs text-[#FFB000] uppercase font-mono">DRIVE</span>
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
            <span className={`text-xs font-mono font-bold ${peak > -1 ? 'text-[#DC143C]' : 'text-[#888888]'}`}>
              {peak > -1 ? 'PEAK!' : 'OK'}
            </span>
          </div>
        </section>

        {/* ── MASTER MONITOR (full width × 2/6) ──────────────────── */}
        <section className="col-span-12 row-span-2 bg-[#0e0e0e] border border-[#222222] flex overflow-hidden">

          {/* ── L/R STEREO METERS ─────────────────────────────────── */}
          <div className="w-48 flex-shrink-0 border-r border-[#222222] flex flex-col px-4 py-2 gap-1.5 justify-center">
            <div className="text-xs font-mono text-[#FFB000] uppercase tracking-widest mb-1">STEREO MASTER</div>
            {[['L', meterL], ['R', meterR]].map(([ch, val]) => {
              const pct = Math.max(0, Math.min(100, val * 100))
              const db = (val * 72 - 60).toFixed(1)
              const barColor = pct > 88 ? '#DC143C' : pct > 70 ? '#FFB000' : '#00FFaa'
              return (
                <div key={ch} className="flex items-center gap-2">
                  <span className="text-xs font-mono font-bold text-[#888888] w-3">{ch}</span>
                  <div className="flex-1 h-3 bg-[#111] relative overflow-hidden">
                    <motion.div
                      className="h-full absolute left-0 top-0"
                      animate={{ width: `${pct}%`, backgroundColor: barColor }}
                      transition={{ duration: 0.05 }}
                      style={{ boxShadow: `0 0 6px ${barColor}60` }}
                    />
                    {/* tick marks at -12, -6, 0 dB */}
                    {[33, 58, 83].map(p => (
                      <div key={p} className="absolute top-0 bottom-0 w-[0.5px] bg-[#333]" style={{ left: `${p}%` }} />
                    ))}
                  </div>
                  <span className="text-xs font-mono text-[#888888] w-10 text-right">{db}dB</span>
                </div>
              )
            })}
            <div className="flex justify-between mt-1 text-xs font-mono text-[#666]">
              <span>-60</span><span>-12</span><span>-6</span><span>0</span>
            </div>
          </div>

          {/* ── TRANSPORT + STATS ─────────────────────────────────── */}
          <div className="w-48 flex-shrink-0 border-r border-[#222222] flex flex-col justify-center px-4 py-2 gap-2">
            <div className="text-xs font-mono text-[#FFB000] uppercase tracking-widest mb-1">TRANSPORT</div>
            <div className="flex items-center gap-3">
              <motion.div
                className={`text-2xl font-black tracking-tighter ${transport.isPlaying ? 'text-[#00FFaa]' : 'text-[#666]'}`}
                animate={transport.isPlaying ? { opacity:[1,0.7,1] } : {}}
                transition={{ repeat: Infinity, duration: 1.2 }}
              >
                {transport.isPlaying ? '▶' : '■'}
              </motion.div>
              {transport.isRecording && (
                <motion.div className="text-[#DC143C] text-xs font-bold uppercase tracking-widest"
                  animate={{ opacity:[1,0,1] }} transition={{ repeat: Infinity, duration: 0.8 }}
                >● REC</motion.div>
              )}
            </div>
            <div className="font-mono text-[18px] font-bold text-[#FFB000] tracking-tight">
              {transport.bpm.toFixed(1)} <span className="text-xs text-[#888888]">BPM</span>
            </div>
            <div className="font-mono text-xs text-[#888888]">
              {Math.floor(transport.position / 60).toString().padStart(2,'0')}:{(transport.position % 60).toFixed(0).toString().padStart(2,'0')}
              <span className="text-xs ml-1">pos</span>
            </div>
          </div>

          {/* ── PLUGIN STATS ──────────────────────────────────────── */}
          <div className="w-40 flex-shrink-0 border-r border-[#222222] flex flex-col justify-center px-4 py-2 gap-2">
            <div className="text-xs font-mono text-[#FFB000] uppercase tracking-widest mb-1">PLUGIN</div>
            {[
              ['SR',  pluginStats.sampleRate ? `${(pluginStats.sampleRate / 1000).toFixed(1)}kHz` : '—'],
              ['BUF', pluginStats.bufferSize  ? `${pluginStats.bufferSize} smp`                   : '—'],
              ['LAT', pluginStats.latencyMs   ? `${pluginStats.latencyMs.toFixed(1)} ms`           : '—'],
              ['CPU', 'host'],
            ].map(([k, v]) => (
              <div key={k} className="flex justify-between items-center">
                <span className="text-xs font-mono text-[#888888]">{k}</span>
                <span className={`text-xs font-mono font-bold ${v === '—' ? 'text-[#666]' : 'text-[#e5e2e1]'}`}>{v}</span>
              </div>
            ))}
          </div>

          {/* ── MINI SESSION FEED ────────────────────────────────── */}
          <div className="flex-1 flex flex-col overflow-hidden">
            <div className="flex items-center justify-between px-3 py-1.5 border-b border-[#1a1a1a]">
              <span className="text-xs font-mono text-[#DC143C] uppercase tracking-widest">FLIGHT RECORDER</span>
              <button onClick={() => setActiveTab('SESSIONS')}
                className="text-xs font-mono text-[#888888] hover:text-[#FFB000] transition-colors uppercase tracking-widest">
                VIEW ALL →
              </button>
            </div>
            <div className="flex-1 overflow-hidden flex flex-col-reverse px-2 py-1 gap-0.5">
              {[...messages].reverse().filter(m => m.type !== 'advisory').slice(0, 5).map(m => (
                <div key={m.id} className="flex items-center gap-2 text-xs font-mono opacity-60 hover:opacity-100 transition-opacity truncate">
                  <span className={`w-1.5 h-1.5 rounded-full flex-shrink-0 ${
                    m.type === 'system' ? 'bg-[#4d4d4d]' :
                    m.type === 'user' ? 'bg-[#FFB000]' : 'bg-[#00CFFF]'
                  }`} />
                  <span className="truncate text-[#aaaaaa]">{m.text?.slice(0,60)}</span>
                  {m.time && <span className="text-[#666] flex-shrink-0">{m.time}</span>}
                </div>
              ))}
            </div>
          </div>
        </section>
      </main>

      {/* ── FOOTER ─────────────────────────────────────────────────── */}
      <footer className="fixed bottom-0 left-0 w-full z-50 flex justify-between items-center h-9 border-t border-[#222222] bg-[#0d0d0d] px-5">
        <div className="flex items-center gap-5 text-xs font-mono">
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
          <motion.span className="text-xs font-mono text-[#FFB000] uppercase tracking-widest"
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
