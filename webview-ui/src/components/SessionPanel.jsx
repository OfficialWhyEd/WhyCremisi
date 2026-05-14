import React, { useState, useEffect, useRef, useCallback } from 'react'
import { motion, AnimatePresence } from 'framer-motion'
import { whycremisi } from '../whycremisi-bridge'

// ── event type config ─────────────────────────────────────────────────────────
const EVENT_META = {
  transport:    { icon: 'play_circle',          color: '#00FFaa', label: 'TRANSPORT' },
  daw_command:  { icon: 'tune',                 color: '#FFB000', label: 'DAW CMD'  },
  ai_prompt:    { icon: 'chat',                 color: '#00CFFF', label: 'AI PROMPT' },
  ai_response:  { icon: 'smart_toy',            color: '#00CFFF', label: 'AI RESP'   },
  osc:          { icon: 'wifi',                 color: '#444455', label: 'OSC'       },
  ws_connect:   { icon: 'link',                 color: '#9B59B6', label: 'WS CONN'  },
  ws_disconnect:{ icon: 'link_off',             color: '#9B59B6', label: 'WS DISC'  },
  error:        { icon: 'error',                color: '#DC143C', label: 'ERROR'     },
  note:         { icon: 'sticky_note_2',        color: '#e5e2e1', label: 'NOTE'      },
  session_open: { icon: 'power_settings_new',   color: '#00FFaa', label: 'SESSION'   },
  session_close:{ icon: 'power_off',            color: '#555555', label: 'END'       },
  parameter:    { icon: 'sliders',              color: '#FFB000', label: 'PARAM'     },
}

const meta = (type) => EVENT_META[type] || { icon: 'circle', color: '#444444', label: type?.toUpperCase() || '?' }

// ── format seconds as MM:SS ───────────────────────────────────────────────────
const fmtDuration = (ms) => {
  if (!ms) return '00:00'
  const s = Math.floor(ms / 1000)
  return `${String(Math.floor(s / 60)).padStart(2,'0')}:${String(s % 60).padStart(2,'0')}`
}

// ── normalise an event from either format ────────────────────────────────────
// JSONL events use flat keys (type, isPlaying, bpm, command, prompt...)
// Live session.event payloads use { event_type, data: {...} }
const normalise = (ev) => {
  if (ev.event_type) return ev  // already normalised (live push)
  // Flatten JSONL event
  const { type, ms, elapsed_ms, time, ...rest } = ev
  return { event_type: type, elapsed_ms, time, ms, data: rest, id: ms }
}

// ── format event summary text ─────────────────────────────────────────────────
const summarize = (ev) => {
  const d = ev.data || {}
  switch (ev.event_type) {
    case 'transport':    return `${(d.isPlaying ?? d.is_playing) ? '▶ PLAY' : '■ STOP'} @ ${(d.bpm ?? '--')} BPM`
    case 'daw_command':  return d.command ?? '?'
    case 'ai_prompt':    return (d.prompt ?? '').slice(0, 80)
    case 'ai_response':  return `[${d.success ? 'OK' : 'ERR'}] ${(d.preview ?? d.response ?? '').slice(0, 80)} (${d.duration_ms ?? '?'}ms)`
    case 'osc':          return `${d.address ?? ''} = ${d.value ?? ''}`
    case 'ws_connect':
    case 'ws_disconnect': return `client ${d.client_id ?? '?'} ${ev.event_type === 'ws_connect' ? 'connected' : 'disconnected'}`
    case 'error':        return `[${d.code ?? '?'}] ${d.message ?? ''}`
    case 'session_open': return `session started — ${d.daw ?? 'Unknown DAW'}`
    case 'note':         return d.text ?? ''
    default:             return JSON.stringify(d).slice(0, 80)
  }
}

// ── event row ─────────────────────────────────────────────────────────────────
function EventRow({ ev, isNew }) {
  const m = meta(ev.event_type)
  return (
    <motion.div
      layout
      initial={isNew ? { opacity: 0, x: -16, backgroundColor: '#DC143C22' } : { opacity: 1 }}
      animate={{ opacity: 1, x: 0, backgroundColor: '#00000000' }}
      transition={{ duration: 0.35, ease: [0.22,1,0.36,1] }}
      className="flex items-start gap-3 py-1.5 px-3 border-b border-[#1a1a1a] hover:bg-[#111111] group"
    >
      {/* type chip */}
      <div className="flex items-center gap-1.5 min-w-[90px] flex-shrink-0 mt-0.5">
        <span className="material-symbols-outlined text-[13px]" style={{ color: m.color }}>{m.icon}</span>
        <span className="text-[9px] font-bold font-mono tracking-widest" style={{ color: m.color }}>{m.label}</span>
      </div>

      {/* summary */}
      <p className="flex-1 text-[10px] font-mono text-[#aaaaaa] leading-tight truncate group-hover:whitespace-normal group-hover:text-[#e5e2e1] transition-colors">
        {summarize(ev)}
      </p>

      {/* time */}
      <span className="text-[9px] font-mono text-[#444444] flex-shrink-0 ml-2">
        {ev.time ?? ev.elapsed_ms != null ? `+${(ev.elapsed_ms/1000).toFixed(1)}s` : ''}
      </span>
    </motion.div>
  )
}

// ── stat pill ─────────────────────────────────────────────────────────────────
function StatPill({ label, value, color }) {
  return (
    <div className="flex flex-col items-center px-4 py-1.5 border-r border-[#222222] last:border-0">
      <span className="text-[13px] font-bold" style={{ color }}>{value}</span>
      <span className="text-[8px] font-mono text-[#4d4d4d] uppercase tracking-widest">{label}</span>
    </div>
  )
}

// ── filter button ─────────────────────────────────────────────────────────────
function FilterBtn({ type, active, onClick }) {
  const m = meta(type)
  return (
    <motion.button
      whileTap={{ scale: 0.93 }}
      onClick={onClick}
      className={`flex items-center gap-1 px-2 py-0.5 text-[9px] font-bold font-mono border transition-colors ${
        active
          ? 'border-current text-[#e5e2e1]'
          : 'border-transparent text-[#4d4d4d] hover:text-[#888888]'
      }`}
      style={active ? { color: m.color, borderColor: m.color + '60' } : {}}
    >
      <span className="material-symbols-outlined text-[11px]">{m.icon}</span>
      {m.label}
    </motion.button>
  )
}

// ── main component ────────────────────────────────────────────────────────────
export function SessionPanel() {
  const [events, setEvents]         = useState([])
  const [sessionInfo, setSessionInfo] = useState(null)
  const [newIds, setNewIds]         = useState(new Set())
  const [filter, setFilter]         = useState(null)   // null = all
  const [autoScroll, setAutoScroll] = useState(true)
  const [elapsedMs, setElapsedMs]   = useState(0)
  const feedEndRef = useRef(null)
  const startRef   = useRef(null)

  // ── request session data on mount ─────────────────────────────────
  const requestSession = useCallback(() => {
    if (whycremisi.isConnected()) {
      whycremisi.send({ type: 'session.get', id: 'sess-' + Date.now(), timestamp: Date.now(), payload: {} })
    }
  }, [])

  useEffect(() => {
    requestSession()
  }, [requestSession])

  // ── elapsed timer ──────────────────────────────────────────────────
  useEffect(() => {
    const t = setInterval(() => {
      if (startRef.current) setElapsedMs(Date.now() - startRef.current)
    }, 1000)
    return () => clearInterval(t)
  }, [])

  // ── bridge subscriptions ───────────────────────────────────────────
  useEffect(() => {
    // full session data dump (response to session.get)
    const unsubData = whycremisi.on('session.data', (payload) => {
      if (payload?.error) return

      setSessionInfo(payload)
      if (payload?.started_at_ms) startRef.current = payload.started_at_ms

      // Normalise events from JSONL format
      const evs = (payload?.events ?? []).map(normalise)
      setEvents(evs)
    })

    // live event push from plugin
    const unsubEv = whycremisi.on('session.event', (payload) => {
      const ev = normalise({
        id: Date.now() + Math.random(),
        ...payload
      })
      setEvents(prev => [...prev, ev].slice(-500))
      setNewIds(prev => new Set([...prev, ev.id]))
      // clear "new" highlight after 2s
      setTimeout(() => setNewIds(prev => {
        const n = new Set(prev)
        n.delete(ev.id)
        return n
      }), 2000)
    })

    // poll every 5s when connected (refreshes event count and stats)
    const pollTimer = setInterval(requestSession, 5000)

    return () => {
      unsubData()
      unsubEv()
      clearInterval(pollTimer)
    }
  }, [requestSession])

  // ── auto-scroll ────────────────────────────────────────────────────
  useEffect(() => {
    if (autoScroll) feedEndRef.current?.scrollIntoView({ behavior: 'smooth' })
  }, [events, autoScroll])

  // ── filtered events ────────────────────────────────────────────────
  const visible = filter ? events.filter(e => e.event_type === filter) : events

  // ── event type counts ──────────────────────────────────────────────
  const counts = events.reduce((acc, e) => {
    acc[e.event_type] = (acc[e.event_type] || 0) + 1
    return acc
  }, {})

  const filterTypes = ['transport','daw_command','ai_prompt','ai_response','osc','ws_connect','error']

  // ── render ─────────────────────────────────────────────────────────
  return (
    <div className="flex flex-col h-full bg-[#0d0d0d] text-[#e5e2e1] font-['Space_Grotesk']">

      {/* ── header bar ──────────────────────────────────────────────── */}
      <div className="bg-[#131313] border-b border-[#222222] px-4 py-2 flex items-center justify-between flex-shrink-0">
        <div className="flex items-center gap-3">
          <img src="/whycremisi-mask.png" className="w-5 h-5 flex-shrink-0 opacity-80" alt="WhyCremisi" />
          <div className="w-1.5 h-1.5 rounded-full bg-[#DC143C] led-red-active flex-shrink-0" />
          <span className="text-[10px] font-bold text-[#DC143C] tracking-widest uppercase">FLIGHT RECORDER</span>
          {sessionInfo?.session_id && (
            <span className="text-[9px] font-mono text-[#4d4d4d]">
              ID: {sessionInfo.session_id}
            </span>
          )}
        </div>

        <div className="flex items-center gap-1">
          {/* stat pills */}
          <StatPill label="EVENTS" value={events.length} color="#FFB000" />
          <StatPill label="ELAPSED" value={fmtDuration(elapsedMs)} color="#e5e2e1" />
          {counts.ai_prompt && <StatPill label="AI CALLS" value={counts.ai_prompt} color="#00CFFF" />}
          {counts.error && <StatPill label="ERRORS" value={counts.error} color="#DC143C" />}

          <motion.button
            whileHover={{ scale: 1.05 }} whileTap={{ scale: 0.95 }}
            onClick={requestSession}
            className="ml-3 text-[#4d4d4d] hover:text-[#FFB000] transition-colors"
            title="Refresh session data"
          >
            <span className="material-symbols-outlined text-base">refresh</span>
          </motion.button>
        </div>
      </div>

      {/* ── filter bar ──────────────────────────────────────────────── */}
      <div className="bg-[#0e0e0e] border-b border-[#1a1a1a] px-3 py-1 flex items-center gap-1 flex-shrink-0 overflow-x-auto">
        <button
          onClick={() => setFilter(null)}
          className={`px-2 py-0.5 text-[9px] font-bold font-mono border mr-1 transition-colors ${
            !filter ? 'border-[#FFB000]/40 text-[#FFB000]' : 'border-transparent text-[#4d4d4d] hover:text-[#888]'
          }`}
        >ALL</button>
        {filterTypes.map(t => (
          <FilterBtn
            key={t}
            type={t}
            active={filter === t}
            onClick={() => setFilter(prev => prev === t ? null : t)}
          />
        ))}

        <div className="ml-auto flex items-center gap-2">
          <button
            onClick={() => setAutoScroll(v => !v)}
            className={`text-[9px] font-mono transition-colors ${autoScroll ? 'text-[#FFB000]' : 'text-[#4d4d4d]'}`}
            title="Toggle auto-scroll"
          >
            <span className="material-symbols-outlined text-[13px] align-middle">
              {autoScroll ? 'vertical_align_bottom' : 'lock_open'}
            </span>
          </button>
          <span className="text-[9px] font-mono text-[#333333]">{visible.length} shown</span>
        </div>
      </div>

      {/* ── event feed ──────────────────────────────────────────────── */}
      <div
        className="flex-1 overflow-y-auto custom-scrollbar"
        onWheel={() => setAutoScroll(false)}
      >
        {visible.length === 0 ? (
          <div className="flex flex-col items-center justify-center h-full gap-3 opacity-30">
            <span className="material-symbols-outlined text-4xl text-[#DC143C]">history</span>
            <p className="text-[11px] font-mono text-[#4d4d4d] uppercase tracking-widest">
              {whycremisi.isConnected() ? 'Waiting for events...' : 'Plugin not connected'}
            </p>
          </div>
        ) : (
          <AnimatePresence initial={false}>
            {visible.map((ev, i) => (
              <EventRow
                key={ev.id ?? i}
                ev={ev}
                isNew={newIds.has(ev.id)}
              />
            ))}
          </AnimatePresence>
        )}
        <div ref={feedEndRef} />
      </div>

      {/* ── session path footer ──────────────────────────────────────── */}
      {sessionInfo?.session_dir && (
        <div className="bg-[#0a0a0a] border-t border-[#1a1a1a] px-3 py-1 flex items-center gap-2 flex-shrink-0">
          <span className="material-symbols-outlined text-[11px] text-[#333333]">folder</span>
          <span className="text-[9px] font-mono text-[#333333] truncate">{sessionInfo.session_dir}</span>
        </div>
      )}
    </div>
  )
}
