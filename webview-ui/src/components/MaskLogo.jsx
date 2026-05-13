import { useState, useEffect, useId } from 'react'
import { motion } from 'framer-motion'

// Coordinates in 400×400 viewBox (source image 512×512, face centred)
// Jaw line: y where face meets jaw plate (~67% of face height)
const JAW_LINE    = 258
const JAW_MAX_OPEN = 14   // max jaw translateY in viewBox units

// Eye positions (left/right in 400×400 viewBox)
const EYES = [
  { cx: 159, cy: 194, rx: 21, ry: 16 },
  { cx: 241, cy: 194, rx: 21, ry: 16 },
]

// Glow filter blur range
const BASE_BLUR = 3
const MAX_BLUR  = 8

export function MaskLogo({ audioLevel = 0, className = 'w-10 h-10' }) {
  const uid = useId().replace(/:/g, 'x')
  const [smooth, setSmooth] = useState(0)

  useEffect(() => {
    setSmooth(prev => prev * 0.60 + Math.min(1, Math.max(0, audioLevel)) * 0.40)
  }, [audioLevel])

  const jawOffset  = smooth > 0.03 ? smooth * JAW_MAX_OPEN : 0
  const eyeBlur    = BASE_BLUR + smooth * (MAX_BLUR - BASE_BLUR)
  const eyeOpacity = 0.10 + smooth * 0.28

  const src = '/whycremisi-mask.png'

  return (
    <div className={`${className} relative flex-shrink-0`}>
      <svg
        viewBox="0 0 400 400"
        className="w-full h-full"
        style={{ display: 'block' }}
      >
        <defs>
          {/* Static head: everything above the jaw line */}
          <clipPath id={`hc-${uid}`}>
            <rect x="0" y="0" width="400" height={JAW_LINE} />
          </clipPath>

          {/* Jaw window: everything below the jaw line (fixed in screen space) */}
          <clipPath id={`jc-${uid}`}>
            <rect x="0" y={JAW_LINE} width="400" height={400 - JAW_LINE} />
          </clipPath>

          {/* Eye glow — feBlend keeps source on top of blurred copy */}
          <filter id={`eg-${uid}`} x="-80%" y="-80%" width="260%" height="260%">
            <feGaussianBlur in="SourceGraphic" stdDeviation={eyeBlur} result="blur" />
            <feMerge>
              <feMergeNode in="blur" />
              <feMergeNode in="SourceGraphic" />
            </feMerge>
          </filter>
        </defs>

        {/* ── TESTA STATICA ─────────────────────────────────── */}
        <image
          href={src}
          x="0" y="0"
          width="400" height="400"
          clipPath={`url(#hc-${uid})`}
        />

        {/* Black fill behind jaw — revealed as gap when jaw moves down */}
        <rect x="0" y={JAW_LINE} width="400" height={400 - JAW_LINE} fill="#000" />

        {/* ── JAW PLATE ANIMATA ─────────────────────────────── */}
        <g clipPath={`url(#jc-${uid})`}>
          <motion.g
            animate={{ y: jawOffset }}
            transition={{ duration: 0.05, ease: 'linear' }}
          >
            <image href={src} x="0" y="0" width="400" height="400" />
          </motion.g>
        </g>

        {/* ── GLOW OCCHI ────────────────────────────────────── */}
        {EYES.map((e, i) => (
          <ellipse
            key={i}
            cx={e.cx} cy={e.cy}
            rx={e.rx} ry={e.ry}
            fill="white"
            opacity={eyeOpacity}
            filter={`url(#eg-${uid})`}
          />
        ))}
      </svg>
    </div>
  )
}
