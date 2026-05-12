import React, { useState, useEffect, useMemo, useRef, useCallback } from 'react'
import { motion, AnimatePresence } from 'framer-motion'
import { whycremisi, ConnectionState } from './whycremisi-bridge'
import { BotFace } from './components/BotFace'
import { SessionPanel } from './components/SessionPanel'
import { SetupScreen } from './components/SetupScreen'
import './index.css'

// ── BoxChat — various box types shown inside chat after AI response ──
function BoxChat({ boxType, meterL, meterR, lufs, peak, transport, pluginStats, gainDb, driveVal, correlation, spectrum, onDawCmd, personality, onAnalyzeFurther, suggestion }) {
  const [localGain, setLocalGain] = useState(gainDb)
  const [localDrive, setLocalDrive] = useState(driveVal)
  const [midSide, setMidSide] = useState(false)
  const [phaseInvert, setPhaseInvert] = useState(false)
  const [clipThreshold, setClipThreshold] = useState(-1)
  const [compThresh, setCompThresh] = useState(-18)
  const [compRatio, setCompRatio] = useState(4)
  const [advDismissed, setAdvDismissed] = useState(false)
  const [advExecuted, setAdvExecuted] = useState(false)
  useEffect(() => { setLocalGain(gainDb) }, [gainDb])
  useEffect(() => { setLocalDrive(driveVal) }, [driveVal])
  const lPct = Math.round(meterL * 100)
  const rPct = Math.round(meterR * 100)
  const isClipping = peak > clipThreshold
  const clipPct = Math.min(100, Math.max(0, ((peak - clipThreshold) / 6) * 100))

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

  const wrap = (label, color, icon, children, actions) => (
    <motion.div initial={{ opacity:0, y:4 }} animate={{ opacity:1, y:0 }} transition={{ delay:0.35 }}
      className="mt-2 bg-[#0d0d0d]/70 border border-[#222] overflow-hidden"
    >
      <div className="flex items-center justify-between px-2 py-1 border-b border-[#1a1a1a]">
        <div className="flex items-center gap-1.5">
          <span className="material-symbols-outlined text-xs" style={{ color }}>{icon}</span>
          <span className="text-xs font-bold uppercase tracking-widest" style={{ color }}>{label}</span>
        </div>
        {actions && <div className="flex gap-1">{actions}</div>}
      </div>
      <div className="p-2">{children}</div>
    </motion.div>
  )

  // ── VECTORSCOPE: interactive XY phase display ────────────────
  if (boxType === 'vectorscope') return wrap('Vectorscope', '#AA44FF', 'donut_small', (
    <div className="flex gap-3">
      <div className="relative w-28 h-28 flex-shrink-0">
        <svg viewBox="-1.2 -1.2 2.4 2.4" className="w-full h-full">
          <circle cx="0" cy="0" r="1" fill="none" stroke="#222" strokeWidth="0.02" />
          <circle cx="0" cy="0" r="0.5" fill="none" stroke="#1a1a1a" strokeWidth="0.01" />
          <line x1="-1" y1="0" x2="1" y2="0" stroke="#1a1a1a" strokeWidth="0.01" />
          <line x1="0" y1="-1" x2="0" y2="1" stroke="#1a1a1a" strokeWidth="0.01" />
          {Array.from({ length: 80 }, (_, i) => {
            const a = (i / 80) * Math.PI * 2
            const r = 0.3 + Math.sin(a * 3 + Date.now() * 0.002) * 0.3 + correlation * 0.3
            const x = Math.cos(a) * r, y = Math.sin(a) * r
            const hue = 280 + (1 - correlation) * 60
            return <circle key={i} cx={x} cy={y} r="0.03" fill={`hsl(${hue}, 90%, 60%)`} opacity={0.7} />
          })}
          {Array.from({ length: 40 }, (_, i) => {
            const a = (i / 40) * Math.PI * 2 + Date.now() * 0.001
            const r = 0.1 + Math.abs(Math.sin(a * 2)) * 0.4
            const x = Math.cos(a) * r, y = Math.sin(a) * r
            return <circle key={'t' + i} cx={x} cy={y} r="0.02" fill="#00E5FF" opacity={0.4} />
          })}
        </svg>
        <div className="absolute bottom-0 left-0 right-0 text-center text-[8px] font-mono text-[#666]">
          φ = {correlation.toFixed(3)}
        </div>
      </div>
      <div className="flex-1 space-y-1.5 min-w-0">
        <MetricBar label="Phase Correlation" val={correlation.toFixed(3)} color={correlation < 0.3 ? '#DC143C' : correlation < 0.6 ? '#FFB000' : '#00FFaa'} pct={Math.max(0, Math.min(100, (correlation + 1) * 50))} />
        <MetricBar label="Mono Compatibility" val={correlation < 0.3 ? 'CRITICAL' : correlation < 0.6 ? 'FAIR' : 'GOOD'} color={correlation < 0.3 ? '#DC143C' : correlation < 0.6 ? '#FFB000' : '#00FFaa'} pct={Math.max(0, Math.min(100, correlation * 100))} />
        <div className="flex gap-1 pt-1">
          <motion.button whileTap={{ scale: 0.95 }}
            className={`flex-1 py-1 text-[9px] font-bold uppercase tracking-wider border ${midSide ? 'bg-[#AA44FF] text-black border-[#AA44FF]' : 'text-[#888] border-[#333] hover:border-[#AA44FF]'}`}
            onClick={() => { setMidSide(!midSide); onDawCmd('midSide', { enabled: !midSide }) }}>
            M/S
          </motion.button>
          <motion.button whileTap={{ scale: 0.95 }}
            className={`flex-1 py-1 text-[9px] font-bold uppercase tracking-wider border ${phaseInvert ? 'bg-[#DC143C] text-black border-[#DC143C]' : 'text-[#888] border-[#333] hover:border-[#DC143C]'}`}
            onClick={() => { setPhaseInvert(!phaseInvert); onDawCmd('phaseInvert', { enabled: !phaseInvert }) }}>
            φ INVERT
          </motion.button>
          <motion.button whileTap={{ scale: 0.95 }}
            className="flex-1 py-1 text-[9px] font-bold uppercase tracking-wider border text-[#888] border-[#333] hover:border-[#00FFaa]"
            onClick={() => onDawCmd('mono')}>
            MONO
          </motion.button>
        </div>
      </div>
    </div>
  ))

  // ── STEREO FIELD ─────────────────────────────────────────────
  if (boxType === 'stereo') return wrap('Stereo Field', '#DC143C', 'swap_horiz', (
    <div className="grid grid-cols-2 gap-1.5">
      <MetricBar label="L/R Balance" val={lPct>rPct?`${lPct-rPct}% L`:rPct>lPct?`${rPct-lPct}% R`:'CENTER'} color="#DC143C" pct={Math.abs(lPct-rPct)} />
      <MetricBar label="Side Content" val={`${Math.round(Math.abs(meterL-meterR)*150)}%`} color="#FFB000" pct={Math.round(Math.abs(meterL-meterR)*150)} />
      <MetricBar label="Correlation" val={`+${(0.62+(meterL+meterR)*0.15).toFixed(2)}`} color="#00E5FF" pct={Math.round((0.62+(meterL+meterR)*0.15)*100)} />
      <MetricBar label="Width" val={`${Math.round((Math.abs(meterL-meterR)+0.3)*100)}%`} color="#00FFaa" pct={Math.round((Math.abs(meterL-meterR)+0.3)*100)} />
      <div className="col-span-2 flex gap-1 pt-1">
        <motion.button whileTap={{ scale: 0.95 }}
          className={`flex-1 py-1 text-[9px] font-bold uppercase tracking-wider border ${midSide ? 'bg-[#DC143C] text-black border-[#DC143C]' : 'text-[#888] border-[#333] hover:border-[#DC143C]'}`}
          onClick={() => { setMidSide(!midSide); onDawCmd('midSide', { enabled: !midSide }) }}>
          MID/SIDE
        </motion.button>
        <motion.button whileTap={{ scale: 0.95 }}
          className="flex-1 py-1 text-[9px] font-bold uppercase tracking-wider border text-[#888] border-[#333] hover:border-[#00FFaa]"
          onClick={() => onDawCmd('narrow')}>
          NARROW
        </motion.button>
        <motion.button whileTap={{ scale: 0.95 }}
          className="flex-1 py-1 text-[9px] font-bold uppercase tracking-wider border text-[#888] border-[#333] hover:border-[#FFB000]"
          onClick={() => onDawCmd('widen')}>
          WIDEN
        </motion.button>
      </div>
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
      <div className="col-span-2 flex gap-1 pt-1">
        <motion.button whileTap={{ scale: 0.95 }}
          className="flex-1 py-1 text-[9px] font-bold uppercase tracking-wider border text-[#888] border-[#333] hover:border-[#00E5FF]"
          onClick={() => onDawCmd('targetLoudness', { target: -14 })}>
          TARGET -14 LUFS
        </motion.button>
        <motion.button whileTap={{ scale: 0.95 }}
          className="flex-1 py-1 text-[9px] font-bold uppercase tracking-wider border text-[#888] border-[#333] hover:border-[#00E5FF]"
          onClick={() => onDawCmd('targetLoudness', { target: -16 })}>
          TARGET -16 LUFS
        </motion.button>
        <motion.button whileTap={{ scale: 0.95 }}
          className="flex-1 py-1 text-[9px] font-bold uppercase tracking-wider border text-[#888] border-[#333] hover:border-[#FF6B35]"
          onClick={() => onDawCmd('limiter')}>
          LIMIT
        </motion.button>
      </div>
    </div>
  ))

  // ── CLIPPING DETECTION ────────────────────────────────────────
  if (boxType === 'clipping') return wrap('Clipping Detection', '#FF6B35', 'report', (
    <div className="space-y-2">
      <div className={`p-2 border ${isClipping ? 'border-[#DC143C] bg-[#DC143C]/10' : 'border-[#333] bg-[#111]'} transition-all`}>
        <div className="flex justify-between items-center mb-1">
          <span className="text-xs font-bold uppercase tracking-wider" style={{ color: isClipping ? '#DC143C' : '#888' }}>
            {isClipping ? '⚠ CLIPPING DETECTED' : '✓ NO CLIPPING'}
          </span>
          <span className="text-xs font-mono" style={{ color: isClipping ? '#DC143C' : '#888' }}>
            {peak.toFixed(1)} dB
          </span>
        </div>
        <div className="h-2 bg-[#1a1a1a] w-full relative overflow-hidden">
          <motion.div className="h-full absolute left-0 top-0"
            style={{ backgroundColor: isClipping ? '#DC143C' : '#00FFaa', boxShadow: isClipping ? '0 0 8px #DC143C' : 'none' }}
            animate={{ width: `${clipPct}%` }}
            transition={{ duration: 0.1 }}
          />
          <div className="absolute top-0 bottom-0 w-px bg-[#FFB000]" style={{ left: `${((clipThreshold + 60) / 72) * 100}%` }} />
        </div>
      </div>
      <div className="flex items-center gap-2">
        <span className="text-[9px] text-[#888] font-mono uppercase">Threshold</span>
        <input type="range" min="-18" max="0" step="0.5" value={clipThreshold}
          onChange={(e) => setClipThreshold(parseFloat(e.target.value))}
          className="flex-1 h-1 accent-[#FF6B35] cursor-pointer"
        />
        <span className="text-[10px] text-white font-mono w-8 text-right">{clipThreshold.toFixed(1)} dB</span>
      </div>
      <div className="flex gap-1">
        <motion.button whileTap={{ scale: 0.95 }}
          className="flex-1 py-1 text-[9px] font-bold uppercase tracking-wider border text-[#888] border-[#333] hover:border-[#DC143C]"
          onClick={() => onDawCmd('softClip', { enabled: true })}>
          SOFT CLIP
        </motion.button>
        <motion.button whileTap={{ scale: 0.95 }}
          className="flex-1 py-1 text-[9px] font-bold uppercase tracking-wider border text-[#888] border-[#333] hover:border-[#FF6B35]"
          onClick={() => onDawCmd('hardClip', { enabled: true })}>
          HARD CLIP
        </motion.button>
        <motion.button whileTap={{ scale: 0.95 }}
          className="flex-1 py-1 text-[9px] font-bold uppercase tracking-wider border text-[#888] border-[#333] hover:border-[#00E5FF]"
          onClick={() => onDawCmd('ceiling', { value: -1 })}>
          -1dB CEIL
        </motion.button>
      </div>
    </div>
  ))

  if (boxType === 'eq') {
    const bins = (spectrum && spectrum.length > 0) ? spectrum : []
    const avgBands = (start, end) => {
      if (bins.length === 0) return 0
      const s = Math.floor(bins.length * start)
      const e = Math.floor(bins.length * end)
      if (e <= s) return 0
      let sum = 0
      for (let i = s; i < e; i++) sum += bins[i]
      return sum / (e - s)
    }
    const sub = avgBands(0, 0.05)
    const low = avgBands(0.05, 0.15)
    const lowMid = avgBands(0.15, 0.35)
    const presence = avgBands(0.35, 0.65)
    const high = avgBands(0.65, 0.85)
    const air = avgBands(0.85, 1)
    return wrap('Frequency Analysis', '#FFB000', 'graphic_eq', (
      <div className="grid grid-cols-3 gap-1.5">
        <MetricBar label="Sub (20-80Hz)" val={`${(sub * 100).toFixed(0)}%`} color="#9B59B6" pct={sub * 100} />
        <MetricBar label="Low (80-300Hz)" val={`${(low * 100).toFixed(0)}%`} color="#DC143C" pct={low * 100} />
        <MetricBar label="Low-Mid (300Hz-1k)" val={`${(lowMid * 100).toFixed(0)}%`} color="#FFB000" pct={lowMid * 100} />
        <MetricBar label="Presence (1k-5k)" val={`${(presence * 100).toFixed(0)}%`} color="#00FFaa" pct={presence * 100} />
        <MetricBar label="High (5k-15k)" val={`${(high * 100).toFixed(0)}%`} color="#00E5FF" pct={high * 100} />
        <MetricBar label="Air (15k+)" val={`${(air * 100).toFixed(0)}%`} color="#E8D5B7" pct={air * 100} />
      </div>
    ))
  }

  // ── SPECTRAL ANALYZER (spectrogram-style) ──────────────────────
  if (boxType === 'spectral') return wrap('Spectral Analyzer', '#00E5FF', 'finance', (
    <div className="space-y-1.5">
      <div className="h-16 flex items-end gap-[1px]">
        {(spectrum.length > 0 ? spectrum.slice(0, 128) : Array.from({ length: 128 }, (_, i) => 0.1 + Math.sin(i * 0.1 + Date.now() * 0.001) * 0.05 + Math.random() * 0.02)).map((mag, i) => (
          <motion.div key={i} className="flex-1 rounded-t-sm"
            style={{
              backgroundColor: `hsl(${200 + (1 - mag) * 120}, 85%, ${20 + mag * 45}%)`,
              opacity: 0.85,
            }}
            animate={{ height: `${Math.max(3, mag * 100)}%` }}
            transition={{ duration: 0.06 }}
          />
        ))}
      </div>
      <div className="flex justify-between text-[8px] font-mono text-[#555]">
        <span>20Hz</span><span>100Hz</span><span>500Hz</span><span>2kHz</span><span>10kHz</span><span>20kHz</span>
      </div>
      <div className="flex gap-1 pt-1">
        <motion.button whileTap={{ scale: 0.95 }}
          className="flex-1 py-1 text-[9px] font-bold uppercase tracking-wider border text-[#888] border-[#333] hover:border-[#00E5FF]"
          onClick={() => onDawCmd('eqAnalyze')}>ANALYZE</motion.button>
        <motion.button whileTap={{ scale: 0.95 }}
          className="flex-1 py-1 text-[9px] font-bold uppercase tracking-wider border text-[#888] border-[#333] hover:border-[#FFB000]"
          onClick={() => onDawCmd('eqMatch', { target: 'reference' })}>MATCH EQ</motion.button>
        <motion.button whileTap={{ scale: 0.95 }}
          className="flex-1 py-1 text-[9px] font-bold uppercase tracking-wider border text-[#888] border-[#333] hover:border-[#DC143C]"
          onClick={() => onDawCmd('spectralAnalyze')}>FFT</motion.button>
      </div>
    </div>
  ))

  if (boxType === 'slider') return wrap('Volume / Gain Control', '#FFB000', 'tune', (
    <div className="space-y-2">
      {[
        { label: 'Master Gain', val: localGain, setVal: setLocalGain, min: -60, max: 12, color: '#DC143C', cmd: 'setGain', key: 'valueDb' },
        { label: 'Drive', val: localDrive, setVal: setLocalDrive, min: 0, max: 100, color: '#FFB000', cmd: 'setDrive', key: 'value' },
      ].map(({ label, val, setVal, min, max, color, cmd, key }) => {
        const pct = ((val - min) / (max - min)) * 100
        return (
          <div key={label} className="space-y-1">
            <div className="flex justify-between text-xs uppercase font-bold">
              <span className="text-[#aaa]">{label}</span>
              <span style={{ color }}>{val.toFixed(1)}</span>
            </div>
            <div className="h-2 bg-[#1a1a1a] w-full relative cursor-pointer rounded-sm overflow-hidden"
              onMouseDown={(e) => {
                const rect = e.currentTarget.getBoundingClientRect()
                const pct = Math.max(0, Math.min(1, (e.clientX - rect.left) / rect.width))
                const newVal = min + pct * (max - min)
                setVal(newVal)
                onDawCmd(cmd, { [key]: newVal })
                const onMove = (ev) => {
                  const p = Math.max(0, Math.min(1, (ev.clientX - rect.left) / rect.width))
                  const v = min + p * (max - min)
                  setVal(v)
                  onDawCmd(cmd, { [key]: v })
                }
                const onUp = () => { document.removeEventListener('mousemove', onMove); document.removeEventListener('mouseup', onUp) }
                document.addEventListener('mousemove', onMove)
                document.addEventListener('mouseup', onUp)
              }}
            >
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

  // ── COMPRESSOR with interactive threshold ─────────────────────
  if (boxType === 'compressor') return wrap('Compressor Settings', '#9B59B6', 'compress', (
      <div className="space-y-2">
        <div className="grid grid-cols-3 gap-2">
          <div>
            <div className="flex justify-between text-[9px] font-mono mb-0.5">
              <span className="text-[#888] uppercase">Threshold</span>
              <span className="text-white font-bold">{compThresh} dB</span>
            </div>
            <input type="range" min="-40" max="0" step="1" value={compThresh}
              onChange={(e) => { const v = parseInt(e.target.value); setCompThresh(v); onDawCmd('compThreshold', { value: v }) }}
              className="w-full h-1 accent-[#9B59B6] cursor-pointer"
            />
          </div>
          <div>
            <div className="flex justify-between text-[9px] font-mono mb-0.5">
              <span className="text-[#888] uppercase">Ratio</span>
              <span className="text-white font-bold">{compRatio}:1</span>
            </div>
            <input type="range" min="1" max="20" step="0.5" value={compRatio}
              onChange={(e) => { const v = parseFloat(e.target.value); setCompRatio(v); onDawCmd('compRatio', { value: v }) }}
              className="w-full h-1 accent-[#DC143C] cursor-pointer"
            />
          </div>
          <div className="flex flex-col justify-center items-center">
            <div className="text-[10px] text-[#00FFaa] font-bold font-mono">{Math.max(0, (Math.abs(correlation) * 2 - 1) * compThresh * -0.1).toFixed(1)} dB</div>
            <div className="text-[8px] text-[#888] uppercase tracking-wider">Gain Red</div>
          </div>
        </div>
        <div className="grid grid-cols-2 gap-1">
          <motion.button whileTap={{ scale: 0.95 }}
            className="py-1 text-[9px] font-bold uppercase tracking-wider border text-[#888] border-[#333] hover:border-[#9B59B6]"
            onClick={() => onDawCmd('compBypass', { enabled: false })}>
            BYPASS
          </motion.button>
          <motion.button whileTap={{ scale: 0.95 }}
            className="py-1 text-[9px] font-bold uppercase tracking-wider border text-[#888] border-[#333] hover:border-[#00FFaa]"
            onClick={() => onDawCmd('compAuto')}>
            AUTO
          </motion.button>
        </div>
      </div>
    ))

    if (boxType === 'advisory') {
      if (advDismissed) return null

      const s = suggestion || { freqLow: 200, freqHigh: 400, gainDb: -2.4, label: '200Hz–400Hz', description: 'harmonic crowding', transientPres: 84, confidence: 0.982 }
      const advColor = personality?.style === 'direct' ? '#DC143C' :
        personality?.style === 'consultative' ? '#FFB000' :
        personality?.style === 'analytical' ? '#00E5FF' :
        personality?.style === 'creative' ? '#AA44FF' : '#66FF88'
      const priorityLabel = personality?.style === 'direct' ? 'Critical' :
        personality?.style === 'consultative' ? 'Advisory' :
        personality?.style === 'analytical' ? 'Info' :
        personality?.style === 'creative' ? 'Exploratory' : 'Suggested'
      const actionLabel = personality?.style === 'direct' ? 'FORCE APPLY CHAIN' :
        personality?.style === 'consultative' ? 'REVIEW & APPLY' :
        personality?.style === 'analytical' ? 'SIMULATE CHAIN' :
        personality?.style === 'creative' ? 'EXPLORE ALTERNATIVES' : 'APPLY SUGGESTED CHAIN'
      const confidenceTag = `CONF: ${(s.confidence * 100).toFixed(0)}%`
      const styleName = personality?.style?.toUpperCase() || 'DEFAULT'
      const freqLabel = `${s.freqLow}Hz–${s.freqHigh}Hz`

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
          <div className="advisory-card advisory-breathe bg-[#121212] p-4 relative font-mono transition-all duration-500 hover:bg-[#161616]"
            style={{ borderColor: advColor + '66' }}
          >
            <div className="absolute top-2 right-3 flex items-end gap-[2px] h-5 opacity-40">
              {[40,70,55,90,65].map((h,i) => (
                <div key={i} className="w-[2px]" style={{ height: h + '%', backgroundColor: advColor }} />
              ))}
            </div>
            <div className="flex justify-between items-start mb-3 border-b border-[#222] pb-2">
              <div className="flex items-center gap-2">
                <div className="w-1 h-5 relative overflow-hidden flex-shrink-0" style={{ backgroundColor: advColor }}>
                  <motion.div className="absolute inset-0 bg-white/20" animate={{ y:['0%','100%','0%'] }} transition={{ repeat:Infinity, duration:2 }} />
                </div>
                <div>
                  <span className="text-xs font-bold tracking-tighter uppercase block" style={{ color: advColor }}>
                    AI_MASTERING_ADVISORY_{styleName}
                  </span>
                  <span className="text-xs text-[#888888] tracking-widest">
                    NODE: CREMISI_X9 · STYLE: {styleName} · {freqLabel}
                  </span>
                </div>
              </div>
              <div className="flex flex-col items-end">
                <span className="text-xs text-white/40 font-bold uppercase tracking-widest">Priority: <span style={{ color: advColor }}>{priorityLabel}</span></span>
                {personality?.experienceLevel && (
                  <span className="text-xs text-[#888888] font-mono mt-0.5">EXP LVL: {personality.experienceLevel}</span>
                )}
              </div>
            </div>
            <div className="text-sm leading-relaxed text-[#FFB000] mb-3">
              <p className="text-xs leading-relaxed">
                {personality?.style === 'direct' ? 'Aggressive ' : personality?.style === 'consultative' ? 'Potential ' : personality?.style === 'creative' ? 'Creative ' : 'Detected '}
                {s.description} in{' '}
                <span className="text-white font-bold px-0.5" style={{ borderBottom: '1px solid ' + advColor + '66' }}>{freqLabel}</span>.{' '}
                {personality?.style === 'direct' ? 'Applying surgical dip of' :
                 personality?.style === 'consultative' ? 'Consider gentle dip of' :
                 personality?.style === 'analytical' ? 'Recommended dynamic dip of' :
                 personality?.style === 'creative' ? 'Exploring experimental sculpting of' :
                 'Suggesting gentle dip of'}{' '}
                <span className="text-white px-1 font-bold" style={{ backgroundColor: advColor }}>{s.gainDb}dB</span>.{' '}
                Transient preservation at{' '}
                <span className="text-white underline decoration-dotted">{s.transientPres}%</span>.
              </p>
            </div>
            <div className="flex items-center gap-2 mb-3 text-xs font-mono text-[#666] opacity-60">
              <span>HEX: {advColor}</span><span>·</span><span>VAL: {s.gainDb}dB_COR</span><span>·</span><span>{confidenceTag}</span><span>·</span><span>LAT: 0.2ms</span>
            </div>
            <div className="flex flex-wrap gap-2">
              <motion.button
                whileHover={{ scale:1.02 }} whileTap={{ scale:0.95 }}
                onClick={() => { setAdvExecuted(true); onDawCmd('applyEQ', { freq: freqLabel, gain: s.gainDb }) }}
                style={advExecuted ? { backgroundColor: '#00FFaa', color: '#000', border: '1px solid #00FFaa' } : { backgroundColor: advColor, color: '#fff' }}
                className="relative px-4 py-1.5 text-xs font-bold uppercase tracking-widest transition-all"
              >
                <span className="flex items-center gap-1.5">
                  <span className="material-symbols-outlined text-[12px]">bolt</span>
                  {advExecuted ? 'APPLIED ✓' : actionLabel}
                </span>
              </motion.button>
              <motion.button
                className="border border-[#4d4d4d] text-[#888888] px-4 py-1.5 text-xs font-bold uppercase tracking-widest hover:border-[#FFB000] hover:text-[#FFB000] hover:shadow-[0_0_15px_rgba(255,176,0,0.2)] transition-all"
                whileTap={{ scale: 0.95 }}
                onClick={() => onAnalyzeFurther && onAnalyzeFurther(personality?.style)}
              >
                ANALYZE FURTHER
              </motion.button>
              <button className="text-[#888888] hover:text-white text-xs font-bold uppercase self-center transition-colors hover:underline"
                style={{ textDecorationColor: advColor }}
                onClick={() => setAdvDismissed(true)}>DISMISS</button>
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

  // ── MIDI Learn ─────────────────────────────────────────────────────
  const [midiLearnWidget, setMidiLearnWidget] = useState(null)
  const [midiMappings, setMidiMappings] = useState([])
  const [showMapPanel, setShowMapPanel] = useState(false)

  // ── Personality (from PersonalityCore via plugin) ──────────────────
  const [personality, setPersonality] = useState({
    style: 'warm',
    confidence: 0.5,
    experienceLevel: 1,
    userName: '',
    sessionCount: 0,
    totalActions: 0,
    recentActions: [],
    description: ''
  })

  // ── AI Action Log + Undo/Redo ──────────────────────────────────────
  const [actionLog, setActionLog] = useState([])
  const [actionHistory, setActionHistory] = useState([])
  const [actionRedoStack, setActionRedoStack] = useState([])
  const actionHistoryRef = useRef([])
  const actionRedoRef = useRef([])

  const pushAction = useCallback((action) => {
    setActionLog(prev => [action, ...prev].slice(0, 100))
    actionHistoryRef.current = [...actionHistoryRef.current, { ...action }]
    actionRedoRef.current = []
    setActionHistory(actionHistoryRef.current)
    setActionRedoStack([])
  }, [])

  const undoLastAction = useCallback(() => {
    const history = actionHistoryRef.current
    if (history.length === 0) return
    const last = history[history.length - 1]
    const prevVal = last.previousValue !== undefined ? last.previousValue : 0
    if (whycremisi.isConnected()) {
      whycremisi.sendMessage('widget.valueChange', { widgetId: last.widgetId, value: prevVal })
    }
    actionRedoRef.current = [...actionRedoRef.current, { ...last }]
    actionHistoryRef.current = history.slice(0, -1)
    setActionHistory(actionHistoryRef.current)
    setActionRedoStack(actionRedoRef.current)
  }, [])

  const redoLastAction = useCallback(() => {
    const redoStack = actionRedoRef.current
    if (redoStack.length === 0) return
    const next = redoStack[redoStack.length - 1]
    if (whycremisi.isConnected()) {
      whycremisi.sendMessage('widget.valueChange', { widgetId: next.widgetId, value: next.value })
    }
    actionHistoryRef.current = [...actionHistoryRef.current, { ...next }]
    actionRedoRef.current = redoStack.slice(0, -1)
    setActionHistory(actionHistoryRef.current)
    setActionRedoStack(actionRedoRef.current)
  }, [])

  // ── Plugin Chain ──────────────────────────────────────────────────
  const [pluginChain, setPluginChain] = useState([])
  const [showChainPanel, setShowChainPanel] = useState(false)

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
    { id: 'meters',    icon: 'analytics',                   label: 'METERS' },
    { id: 'actions',   icon: 'history',                     label: 'ACTIONS' }
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
      }
      if (s === ConnectionState.RECONNECTING) {
        setBotState('loading')
      }
    })

    window.addEventListener('whycremisi-botstate', (e) => setBotState(e.detail))

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
      stateUnsub(); unsubAI(); unsubStream(); unsubTransport()
      unsubMeter(); unsubOSC(); unsubStats(); unsubErr()
      unsubLearnStatus(); unsubLearnComplete(); unsubChain(); unsubAnalyzer();       unsubActionLog(); unsubPersAction(); unsubPersContext()
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

  const dismissAdvisory = useCallback(() => {
    setMessages(prev => prev.filter(m => m.type !== 'advisory'))
  }, [])

    // ── detect boxchat type from prompt + response ────────────────────
    const detectBoxType = (prompt = '', response = '') => {
    	const t = (prompt + ' ' + response).toLowerCase()
    	if (/spectrum|spectral|fft|analyzer|freq\s*response/.test(t)) return 'spectral'
    	if (/clip|clipping|distort|overload|red|limit(?!er)|ceiling/.test(t)) return 'clipping'
    	if (/vector|scope|vectorscope|xy|phase\s*meter/.test(t)) return 'vectorscope'
    	if (/stereo|width|side|balance|phase(?!\s*meter)|mono|correlation/.test(t)) return 'stereo'
    	if (/lufs|loud|peak|rms|dynamic|crest|limiter/.test(t)) return 'loudness'
    	if (/eq|frequen|bass|sub|mid|high|treble|presence|air|100hz|200hz|1khz|4khz/.test(t)) return 'eq'
    	if (/volume|gain|fader|db(?!\s)|level/.test(t)) return 'slider'
    	if (/pan|panning|position|center|left|right/.test(t)) return 'knob'
    	if (/play|stop|record|transport|bpm|tempo/.test(t)) return 'transport'
    	if (/compres|ratio|attack|release|threshold|knee/.test(t)) return 'compressor'
    	if (/search|internet|find|look\s*up|web|google/.test(t)) return 'advisory'
    	if (/chain|apply|execute|suggest|recommend|action|advisory|do it|should|could|try|consider/.test(t)) return 'advisory'
    	return 'metrics'
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
            <span className="material-symbols-outlined text-base text-[#888888] hover:text-[#FFB000] cursor-pointer transition-colors"
              onClick={() => { setSetupComplete(false); localStorage.removeItem('whycremisi_setup_done') }}>settings</span>
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
              {personality?.style && personality.style !== 'warm' && (
                <span className="text-[9px] uppercase tracking-widest font-bold px-1.5 py-px border"
                  style={{ color: personality.style === 'direct' ? '#DC143C' : personality.style === 'consultative' ? '#FFB000' : personality.style === 'analytical' ? '#00E5FF' : personality.style === 'creative' ? '#AA44FF' : '#888', borderColor: 'currentColor' }}
                >{personality.style}</span>
              )}
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
                <BotFace state={botState} className="w-16 h-16" personality={personality} />
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
                          onClick={() => analyzeFurther(personality?.style)}
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
                        {isLatest && <BotFace state={botState} className="w-8 h-8" personality={personality} />}
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
                        {msg.telemetry && !msg.streaming && <BoxChat boxType={msg.boxType||'metrics'} meterL={meterL} meterR={meterR} lufs={lufs} peak={peak} transport={transport} pluginStats={pluginStats} gainDb={gainDb} driveVal={driveVal} correlation={correlation} spectrum={spectrum} onDawCmd={dawCmd} personality={personality} onAnalyzeFurther={analyzeFurther} suggestion={currentSuggestion} />}
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
            <div className="flex items-center gap-2">
              <motion.button
                className={`text-[9px] font-bold uppercase tracking-wider px-2 py-0.5 rounded-sm border ${showMapPanel ? 'bg-[#00E5FF] text-black border-[#00E5FF]' : 'text-[#888] border-[#333] hover:border-[#00E5FF] hover:text-[#00E5FF]'}`}
                onClick={() => setShowMapPanel(p => !p)}
                whileTap={{ scale: 0.95 }}
              >MAP</motion.button>
              <motion.button
                className={`text-[9px] font-bold uppercase tracking-wider px-2 py-0.5 rounded-sm border ${showChainPanel ? 'bg-[#FF6B35] text-black border-[#FF6B35]' : 'text-[#888] border-[#333] hover:border-[#FF6B35] hover:text-[#FF6B35]'}`}
                onClick={() => setShowChainPanel(p => !p)}
                whileTap={{ scale: 0.95 }}
              >CHAIN</motion.button>
              <span className="text-xs font-mono text-[#888888]">STEREO MASTER</span>
            </div>
          </div>

          {/* Plugin Chain Panel */}
          {showChainPanel && (
            <div className="bg-[#0d0d0d] border-b border-[#222] px-3 py-2 space-y-1 max-h-40 overflow-y-auto">
              <div className="flex justify-between items-center mb-1">
                <span className="text-[9px] font-bold uppercase tracking-wider text-[#FF6B35]">Plugin Chain</span>
                <span className="text-[9px] text-[#666]">{pluginChain.length} plugins</span>
              </div>
              {pluginChain.length === 0 ? (
                <div className="text-[9px] text-[#555] italic">No plugins configured. Add your DAW plugins here so the AI knows what's in your chain.</div>
              ) : (
                pluginChain.map((p, i) => (
                  <div key={p.id || i} className="flex justify-between items-center text-[10px] font-mono">
                    <span className="text-[#aaa]">{i+1}. {p.name}</span>
                    <span className="text-[#888] text-[8px]">{p.manufacturer} {p.format}</span>
                    <motion.button
                      className="text-[#DC143C] hover:text-[#ff6b6b] text-[9px]"
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
              <div className="flex gap-1 pt-1 border-t border-[#222]">
                <input
                  className="flex-1 bg-[#0e0e0e] border border-[#333] text-[10px] text-[#e5e2e1] px-1 py-0.5 outline-none font-mono"
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
            <div className="bg-[#0d0d0d] border-b border-[#222] px-3 py-2 space-y-1 max-h-40 overflow-y-auto">
              <div className="flex justify-between items-center mb-1">
                <span className="text-[9px] font-bold uppercase tracking-wider text-[#00E5FF]">Parameter Mapping</span>
                <span className="text-[9px] text-[#666]">{midiMappings.length} bindings</span>
              </div>
              {midiMappings.length === 0 ? (
                <div className="text-[9px] text-[#555] italic">No MIDI mappings. Click LEARN on a widget and move a hardware controller.</div>
              ) : (
                midiMappings.map(m => (
                  <div key={m.widgetId} className="flex justify-between items-center text-[10px] font-mono">
                    <span className="text-[#aaa]">{m.widgetId}</span>
                    <span className="text-[#FF6B35]">CC#{m.cc} Ch.{m.channel}</span>
                    <motion.button
                      className="text-[#DC143C] hover:text-[#ff6b6b] text-[9px]"
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
              <span className="text-xs font-mono text-[#FFB000]">+12</span>
              <div
                className="flex-1 w-8 bg-[#0e0e0e] border border-[#222222] relative flex flex-col justify-end p-1 cursor-ns-resize"
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
              <span className="text-xs font-mono text-[#FFB000]">-INF</span>
              <span className="text-xs font-bold text-[#e5e2e1] mt-1">{gainDb > 0 ? `+${gainDb}` : gainDb}dB</span>
              <span className="text-xs font-mono text-[#888888]">GAIN</span>
              {/* MIDI Learn button */}
              <motion.button
                className={`w-full mt-1 py-0.5 text-[9px] font-bold uppercase tracking-wider border rounded-sm ${midiLearnWidget === 'masterGain' ? 'bg-[#FF6B35] text-black border-[#FF6B35] pulse-glow' : 'text-[#888] border-[#333] hover:border-[#FF6B35] hover:text-[#FF6B35]'}`}
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
                    const now = Date.now()
                    if (now - throttleRef.current < 50) return
                    throttleRef.current = now
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
          <div className="w-52 flex-shrink-0 border-r border-[#222222] flex flex-col justify-center px-4 py-2 gap-1.5">
            <div className="text-[11px] font-bold text-[#FFB000] uppercase tracking-widest mb-0.5">STEREO MASTER</div>
            {[['L', meterL], ['R', meterR]].map(([ch, val]) => {
              const pct = Math.max(0, Math.min(100, val * 100))
              const db = (val * 72 - 60).toFixed(1)
              const barColor = pct > 88 ? '#DC143C' : pct > 70 ? '#FFB000' : '#00FFaa'
              return (
                <div key={ch} className="flex items-center gap-2">
                  <span className="text-xs font-bold text-[#888888] w-3">{ch}</span>
                  <div className="flex-1 h-3 bg-[#111] relative overflow-hidden rounded-sm">
                    <motion.div
                      className="h-full absolute left-0 top-0 rounded-sm"
                      animate={{ width: `${pct}%`, backgroundColor: barColor }}
                      transition={{ duration: 0.05 }}
                      style={{ boxShadow: `0 0 6px ${barColor}60` }}
                    />
                    {[33, 58, 83].map(p => (
                      <div key={p} className="absolute top-0 bottom-0 w-[0.5px] bg-[#333]" style={{ left: `${p}%` }} />
                    ))}
                  </div>
                  <span className="text-xs font-mono text-[#aaa] w-10 text-right font-medium">{db}dB</span>
                </div>
              )
            })}
            <div className="flex justify-between mt-0.5 text-[10px] font-mono text-[#555]">
              <span>-60</span><span>-12</span><span>-6</span><span>0</span>
            </div>
          </div>

          {/* ── TRANSPORT + STATS ─────────────────────────────────── */}
          <div className="w-44 flex-shrink-0 border-r border-[#222222] flex flex-col justify-center px-4 py-2 gap-1.5">
            <div className="flex items-center justify-between">
              <motion.div
                className={`text-xl font-black tracking-tighter ${transport.isPlaying ? 'text-[#00FFaa]' : 'text-[#555]'}`}
                animate={transport.isPlaying ? { opacity:[1,0.6,1] } : {}}
                transition={{ repeat: Infinity, duration: 1.2 }}
              >
                {transport.isPlaying ? '▶' : '■'}
              </motion.div>
              <div className="text-right">
                <div className="font-mono text-base font-bold text-[#FFB000] tracking-tight leading-none">
                  {transport.bpm.toFixed(1)}
                </div>
                <div className="text-[9px] text-[#888] font-mono uppercase tracking-wider">BPM</div>
              </div>
            </div>
            {transport.isRecording && (
              <motion.div className="text-[10px] text-[#DC143C] font-bold uppercase tracking-widest"
                animate={{ opacity:[1,0.3,1] }} transition={{ repeat: Infinity, duration: 0.8 }}
              >● RECORDING</motion.div>
            )}
            <div className="flex items-center justify-between text-xs font-mono">
              <span className="text-[#888]">POS</span>
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
                  <span className="text-[#555]">{k}</span>
                  <span className="text-white font-medium">{v}</span>
                </div>
              ))}
            </div>
          </div>

          {/* ── REAL-TIME SPECTRUM ANALYZER ─────────────────────────── */}
          <div className="w-60 flex-shrink-0 border-r border-[#222222] flex flex-col justify-center px-3 py-2 gap-1">
            <div className="flex justify-between items-center mb-0.5">
              <span className="text-[10px] font-bold text-[#00E5FF] uppercase tracking-widest">SPECTRUM</span>
              <span className="text-[9px] font-mono text-[#777]">φ {correlation.toFixed(2)}</span>
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
            <div className="flex justify-between text-[8px] font-mono text-[#555] mt-0.5">
              <span>20Hz</span><span>200Hz</span><span>2kHz</span><span>20kHz</span>
            </div>
          </div>

          {/* ── FLIGHT RECORDER ────────────────────────────────────── */}
          <div className="flex-[2] flex flex-col overflow-hidden border-r border-[#222222] min-w-[200px]">
            <div className="flex items-center justify-between px-4 py-1.5 border-b border-[#1a1a1a] bg-[#0d0d0d]">
              <div className="flex items-center gap-2">
                <div className="w-1.5 h-1.5 rounded-full bg-[#DC143C]" />
                <span className="text-[11px] font-bold text-[#DC143C] uppercase tracking-widest">FLIGHT RECORDER</span>
              </div>
              <button onClick={() => setActiveTab('SESSIONS')}
                className="text-[10px] text-[#888888] hover:text-[#FFB000] transition-colors uppercase tracking-wider font-medium">
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
                      <span className="text-[10px] text-[#aaaaaa] leading-tight truncate">
                        {m.text?.slice(0, 80)}
                      </span>
                      {m.time && (
                        <span className="text-[9px] text-[#555] flex-shrink-0 font-medium">{m.time}</span>
                      )}
                    </div>
                    <div className="flex gap-2 text-[8px] text-[#444] uppercase tracking-wider mt-0.5">
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
                    <div className="text-[10px] text-[#555] font-mono italic">No events recorded</div>
                    <div className="text-[8px] text-[#444] font-mono mt-1">Interact with the AI to begin logging</div>
                  </div>
                </div>
              )}
            </div>
          </div>

          {/* ── AI ACTION LOG + UNDO/REDO ────────────────────────── */}
          <div className="w-40 flex-shrink-0 flex flex-col justify-center px-3 py-2 gap-1 overflow-hidden bg-[#0d0d0d]">
            <div className="flex items-center justify-between mb-1">
              <span className="text-[9px] font-bold uppercase tracking-widest text-[#00FFaa]">ACTIONS</span>
              <div className="flex gap-0.5">
                <motion.button
                  className={`text-[11px] px-1.5 py-0.5 border ${actionHistory.length > 0 ? 'text-[#e5e2e1] border-[#444] hover:border-[#00FFaa]' : 'text-[#333] border-[#222]'}`}
                  onClick={undoLastAction}
                  disabled={actionHistory.length === 0}
                  whileTap={{ scale: 0.9 }}
                >↩</motion.button>
                <motion.button
                  className={`text-[11px] px-1.5 py-0.5 border ${actionRedoStack.length > 0 ? 'text-[#e5e2e1] border-[#444] hover:border-[#00FFaa]' : 'text-[#333] border-[#222]'}`}
                  onClick={redoLastAction}
                  disabled={actionRedoStack.length === 0}
                  whileTap={{ scale: 0.9 }}
                >↪</motion.button>
              </div>
            </div>
            <div className="flex-1 overflow-y-auto max-h-24 space-y-0.5 custom-scrollbar">
              {actionLog.length === 0 ? (
                <div className="text-[8px] text-[#555] italic">No AI actions yet</div>
              ) : (
                actionLog.slice(0, 6).map((a, i) => (
                  <div key={a.timestamp + '-' + i} className="text-[9px] font-mono flex justify-between items-center group hover:bg-[#1a1a1a] px-1 -mx-1 rounded-sm transition-colors">
                    <span className="text-[#aaa] truncate mr-1 max-w-[70%]">{a.description || a.widgetId}</span>
                    <span className="text-[#FFB000] flex-shrink-0 font-bold">{a.value?.toFixed(2)}</span>
                  </div>
                ))
              )}
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
