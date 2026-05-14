import { useState, useEffect, useId } from 'react'
import { motion } from 'framer-motion'

const JAW_LINE     = 258
const JAW_MAX_OPEN = 28

const EYES = [
  { cx: 159, cy: 194, rx: 21, ry: 16 },
  { cx: 241, cy: 194, rx: 21, ry: 16 },
]

const BASE_BLUR = 3
const MAX_BLUR  = 9

export function MaskLogo({ audioLevel = 0, className = 'w-10 h-10' }) {
  const uid = useId().replace(/:/g, 'x')
  const [smooth, setSmooth] = useState(0)

  useEffect(() => {
    const raw = Math.min(1, Math.max(0, audioLevel))
    setSmooth(prev =>
      raw > prev
        ? prev * 0.30 + raw * 0.70   // fast attack
        : prev * 0.85 + raw * 0.15   // slow release
    )
  }, [audioLevel])

  const jawOffset  = smooth > 0.03 ? smooth * JAW_MAX_OPEN : 0
  const eyeBlur    = BASE_BLUR + smooth * (MAX_BLUR - BASE_BLUR)
  const eyeOpacity = 0.15 + smooth * 0.50

  const src = '/whycremisi-mask.png'

  return (
    <div className={`${className} relative flex-shrink-0`}>
      <svg viewBox="0 0 400 400" className="w-full h-full" style={{ display: 'block' }}>
        <defs>
          <clipPath id={`hc-${uid}`}>
            <rect x="0" y="0" width="400" height={JAW_LINE} />
          </clipPath>
          <clipPath id={`jc-${uid}`}>
            <rect x="0" y={JAW_LINE} width="400" height={400 - JAW_LINE} />
          </clipPath>
          <filter id={`eg-${uid}`} x="-80%" y="-80%" width="260%" height="260%">
            <feGaussianBlur in="SourceGraphic" stdDeviation={eyeBlur} result="blur" />
            <feMerge>
              <feMergeNode in="blur" />
              <feMergeNode in="SourceGraphic" />
            </feMerge>
          </filter>
        </defs>

        {/* Static head */}
        <image href={src} x="0" y="0" width="400" height="400" clipPath={`url(#hc-${uid})`} />

        {/* Animated jaw */}
        <g clipPath={`url(#jc-${uid})`}>
          <motion.g
            animate={{ y: jawOffset }}
            transition={{ duration: 0.04, ease: 'linear' }}
          >
            <image href={src} x="0" y="0" width="400" height="400" />
          </motion.g>
        </g>

        {/* Eye glow */}
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
