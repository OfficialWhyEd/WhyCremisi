import React, { useEffect, useRef, useState } from 'react'
import { motion, AnimatePresence } from 'framer-motion'

export const BotStates = {
  IDLE: 'idle', LISTENING: 'listening', THINKING: 'thinking',
  TYPING: 'typing', ERROR: 'error', SUCCESS: 'success',
  LOADING: 'loading', ADVISORY: 'advisory', SAD: 'sad'
}

const STATE_COLORS = {
  idle: '#00E5FF', listening: '#FFB000', thinking: '#FFB000',
  typing: '#00E5FF', error: '#DC143C', success: '#00FFaa',
  loading: '#00E5FF', advisory: '#FFB000', sad: '#4169E1'
}

const PATHS = {
  idle:      { l: 'M 26 44 Q 35 33 44 44', r: 'M 56 44 Q 65 33 74 44', m: 'M 38 65 Q 50 74 62 65' },
  sad:       { l: 'M 24 40 Q 35 50 46 50', r: 'M 54 50 Q 65 50 76 40', m: 'M 38 72 Q 50 62 62 72' },
  thinking:  { l: 'M 29 40 Q 37 34 45 40', r: 'M 55 38 Q 65 32 74 40', m: 'M 44 66 Q 50 66 56 66' },
  typing:    { l: 'M 26 41 Q 35 31 44 41', r: 'M 56 41 Q 65 31 74 41', m: 'M 36 62 Q 50 80 64 62' },
  listening: { l: 'M 24 41 Q 35 28 46 41', r: 'M 54 41 Q 65 28 76 41', m: 'M 38 65 Q 50 65 62 65' },
  error:     { l: 'M 27 41 Q 35 52 43 41', r: 'M 57 41 Q 65 52 73 41', m: 'M 37 70 Q 50 58 63 70' },
  success:   { l: 'M 23 46 Q 35 24 47 46', r: 'M 53 46 Q 65 24 77 46', m: 'M 33 60 Q 50 88 67 60' },
  loading:   { l: 'M 29 45 Q 35 45 41 45', r: 'M 59 45 Q 65 45 71 45', m: 'M 43 66 Q 50 66 57 66' },
  advisory:  { l: 'M 26 41 Q 35 37 44 41', r: 'M 56 38 Q 65 34 74 40', m: 'M 40 65 Q 50 65 60 65' }
}

const PUPILS = {
  idle: { lx: 35, ly: 41, rx: 65, ry: 41 }, sad: { lx: 33, ly: 44, rx: 65, ry: 44 },
  thinking: { lx: 37, ly: 38, rx: 67, ry: 36 }, typing: { lx: 35, ly: 38, rx: 65, ry: 38 },
  listening: { lx: 35, ly: 37, rx: 65, ry: 37 }, error: { lx: 35, ly: 44, rx: 65, ry: 44 },
  success: { lx: 35, ly: 39, rx: 65, ry: 39 }, loading: { lx: 35, ly: 45, rx: 65, ry: 45 },
  advisory: { lx: 35, ly: 40, rx: 67, ry: 38 }
}

export function BotFace({ state = 'idle', className = 'w-16 h-16', personality = null }) {
  const prevState = useRef(state)
  const [pulseKey, setPulseKey] = useState(0)
  const [glitchActive, setGlitchActive] = useState(false)
  const [burstActive, setBurstActive] = useState(false)
  const [pupilOffset, setPupilOffset] = useState({ x: 0, y: 0 })

  const color = STATE_COLORS[state] || STATE_COLORS.idle
  const paths = PATHS[state] || PATHS.idle
  const pupils = PUPILS[state] || PUPILS.idle

  useEffect(() => {
    if (prevState.current === state) return
    setPulseKey(k => k + 1)
    if (state === 'error') {
      setGlitchActive(true)
      setTimeout(() => setGlitchActive(false), 450)
    }
    if (state === 'success') {
      setBurstActive(true)
      setTimeout(() => setBurstActive(false), 800)
    }
    prevState.current = state
  }, [state])

  useEffect(() => {
    if (state !== 'idle') { setPupilOffset({ x: 0, y: 0 }); return }
    const id = setInterval(() => {
      setPupilOffset({ x: (Math.random() - 0.5) * 3, y: (Math.random() - 0.5) * 2 })
    }, 2200 + Math.random() * 1400)
    return () => clearInterval(id)
  }, [state])

  const mouthAnimate = state === 'typing'
    ? [paths.m, 'M 40 64 Q 50 70 60 64', paths.m]
    : state === 'listening' ? [paths.m, 'M 34 65 Q 50 65 66 65', paths.m]
    : state === 'loading'   ? [paths.m, 'M 41 66 Q 50 66 59 66', paths.m]
    : paths.m

  const mouthTrans = (state === 'typing' || state === 'listening' || state === 'loading')
    ? { repeat: Infinity, duration: 0.55, ease: 'easeInOut' }
    : { duration: 0.5, type: 'spring', bounce: 0.45 }

  const eyeL = state === 'idle' ? [paths.l, paths.l, 'M 26 44 Q 35 44 44 44', paths.l] : paths.l
  const eyeR = state === 'idle' ? [paths.r, paths.r, 'M 56 44 Q 65 44 74 44', paths.r] : paths.r
  const eyeTrans = state === 'idle'
    ? { repeat: Infinity, duration: 5, times: [0, 0.94, 0.96, 1] }
    : { duration: 0.5, type: 'spring', bounce: 0.45 }

  const glowPx = { idle:10, listening:16, thinking:14, typing:12, error:22, success:26, loading:8, advisory:15, sad:6 }[state] || 10
  const glowOp = { idle:0.5, listening:0.8, thinking:0.7, typing:0.6, error:0.9, success:1.0, loading:0.4, advisory:0.75, sad:0.3 }[state] || 0.5

  const glowFilter = `drop-shadow(0 0 ${glowPx}px ${color}) drop-shadow(0 0 ${Math.round(glowPx*0.35)}px ${color})`

  const bobAnim = { y: state === 'sad' ? [0,2,0] : [0,-3,0], scale: [1, 1.018, 1] }
  const bobTrans = glitchActive
    ? { duration: 0 }
    : { y: { repeat:Infinity, duration: state==='sad'?6:4, ease:'easeInOut' },
        scale: { repeat:Infinity, duration:3.5, ease:'easeInOut' } }

  const glitchAnim = glitchActive ? {
    x: [0,-3,3,-2,2,0],
    opacity: [1,0.5,1,0.6,1,1]
  } : { x: 0, opacity: 1 }

  return (
    <motion.div
      className={`relative flex items-center justify-center ${className}`}
      animate={{ ...bobAnim, ...glitchAnim }}
      transition={bobTrans}
    >
      <AnimatePresence mode="sync">
        <motion.div
          key={`ring-${pulseKey}`}
          className="absolute inset-0 rounded-full pointer-events-none"
          style={{ border: `1px solid ${color}` }}
          initial={{ scale: 0.75, opacity: 0.9 }}
          animate={{ scale: 2.0, opacity: 0 }}
          transition={{ duration: 0.65, ease: 'easeOut' }}
        />
      </AnimatePresence>

      <AnimatePresence>
        {burstActive && (
          <motion.div
            key="burst"
            className="absolute inset-[-6px] rounded-full pointer-events-none"
            style={{ border: `2px solid ${color}`, boxShadow: `0 0 18px ${color}` }}
            initial={{ scale: 0.85, opacity: 1 }}
            animate={{ scale: 2.2, opacity: 0 }}
            transition={{ duration: 0.75, ease: 'easeOut' }}
          />
        )}
      </AnimatePresence>

      <motion.svg
        viewBox="0 0 100 100"
        className="w-full h-full"
        style={{ color, filter: glowFilter }}
        animate={
          state === 'loading' ? { opacity: [0.6, 1, 0.6] } :
          state === 'thinking' ? { rotate: [0, -1.5, 1.5, 0] } : {}
        }
        transition={
          state === 'loading' ? { repeat:Infinity, duration:1.2, ease:'easeInOut' } :
          state === 'thinking' ? { repeat:Infinity, duration:2.2, ease:'easeInOut' } : {}
        }
      >
        <motion.circle cx="50" cy="50" r="46" fill="none" stroke="currentColor" strokeWidth="0.4"
          animate={{ opacity: [glowOp*0.3, glowOp*0.65, glowOp*0.3] }}
          transition={{ repeat:Infinity, duration:3.2, ease:'easeInOut' }}
        />

        {state === 'loading' && (
          <motion.rect x="8" width="84" height="1" fill="currentColor" fillOpacity="0.3"
            animate={{ y: [15, 82, 15] }}
            transition={{ repeat:Infinity, duration:1.8, ease:'linear' }}
          />
        )}

        <motion.g stroke="currentColor" strokeWidth="4.2" strokeLinecap="round" fill="none">
          <motion.path d={paths.l} animate={{ d: eyeL }} transition={eyeTrans} />
          <motion.path d={paths.r} animate={{ d: eyeR }} transition={eyeTrans} />
          <motion.path d={paths.m} animate={{ d: mouthAnimate }} transition={mouthTrans} />
        </motion.g>

        <motion.circle r="2.8" fill="currentColor"
          animate={{ cx: pupils.lx + pupilOffset.x, cy: pupils.ly + pupilOffset.y, opacity: state==='loading'?0:0.9 }}
          transition={{ duration:1.4, ease:'easeInOut' }}
        />
        <motion.circle r="2.8" fill="currentColor"
          animate={{ cx: pupils.rx + pupilOffset.x, cy: pupils.ry + pupilOffset.y, opacity: state==='loading'?0:0.9 }}
          transition={{ duration:1.4, ease:'easeInOut' }}
        />

        {state === 'thinking' && [0,1,2].map(i => (
          <motion.circle key={i} cx={43+i*7} cy="82" r="2.2" fill="currentColor"
            animate={{ opacity:[0.2,1,0.2], scale:[0.7,1.3,0.7] }}
            transition={{ repeat:Infinity, duration:1.1, delay:i*0.18, ease:'easeInOut' }}
          />
        ))}

        {state === 'advisory' && (
          <motion.line x1="57" y1="29" x2="73" y2="26"
            stroke="currentColor" strokeWidth="2.5" strokeLinecap="round"
            initial={{ opacity:0 }} animate={{ opacity:0.75 }} transition={{ duration:0.3 }}
          />
        )}

        {state === 'sad' && (
          <>
            <motion.circle cx="31" r="2" fill="currentColor" fillOpacity="0.5"
              animate={{ cy:[54,75], opacity:[0.6,0] }}
              transition={{ repeat:Infinity, duration:2.5, ease:'easeIn', delay:0.5 }}
            />
            <motion.circle cx="69" r="2" fill="currentColor" fillOpacity="0.5"
              animate={{ cy:[54,75], opacity:[0.6,0] }}
              transition={{ repeat:Infinity, duration:2.5, ease:'easeIn', delay:1.2 }}
            />
          </>
        )}
      </motion.svg>

      {/* ── Personality info badge ── */}
      {personality && personality.style && personality.style !== 'warm' && (
        <motion.div
          className="absolute -bottom-1 left-1/2 -translate-x-1/2 whitespace-nowrap px-1.5 py-[1px] text-[6px] font-bold uppercase tracking-widest"
          style={{
            backgroundColor: personality.style === 'direct' ? '#DC143C' :
              personality.style === 'consultative' ? '#FFB000' :
              personality.style === 'analytical' ? '#00E5FF' :
              personality.style === 'creative' ? '#AA44FF' : '#888',
            color: '#000'
          }}
          initial={{ opacity: 0, y: 4 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ delay: 0.3 }}
        >
          {personality.style}
        </motion.div>
      )}
    </motion.div>
  )
}
