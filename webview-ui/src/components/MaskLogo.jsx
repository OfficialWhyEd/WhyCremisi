import { useId } from 'react'
import { motion, useTransform } from 'framer-motion'
import { useAudioMouth } from './useAudioMouth'

const JAW_LINE     = 258
const JAW_MAX_OPEN = 28

const EYES = [
  { cx: 159, cy: 194, rx: 21, ry: 16 },
  { cx: 241, cy: 194, rx: 21, ry: 16 },
]

export function MaskLogo({ audioLevel = 0, className = 'w-10 h-10' }) {
  const uid = useId().replace(/:/g, 'x')

  // L'apertura arriva come MotionValue: si aggiorna a ogni fotogramma senza
  // far ridisegnare il componente. Prima ogni campione dei meter provocava un
  // render dell'intero SVG, e sopra ci girava anche una transizione di 40 ms
  // che ripartiva da capo ogni volta: da lì il movimento a scatti.
  const apertura = useAudioMouth(audioLevel)

  const mascella  = useTransform(apertura, v => v * JAW_MAX_OPEN)
  const occhi     = useTransform(apertura, v => 0.18 + v * 0.55)
  // Gli occhi si allargano appena sugli accenti: lega lo sguardo al movimento
  // della bocca invece di lasciarli due luci fisse.
  const respiro   = useTransform(apertura, v => 1 + v * 0.12)

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
            <feGaussianBlur in="SourceGraphic" stdDeviation="5" result="blur" />
            <feMerge>
              <feMergeNode in="blur" />
              <feMergeNode in="SourceGraphic" />
            </feMerge>
          </filter>
        </defs>

        {/* Testa, ferma */}
        <image href={src} x="0" y="0" width="400" height="400" clipPath={`url(#hc-${uid})`} />

        {/* Mascella. Il ritaglio sta sul gruppo esterno e resta fermo alla
            linea del mento; a scorrere è l'immagine dentro. In questo modo la
            mascella scende senza lasciare una fascia vuota sotto il viso. */}
        <g clipPath={`url(#jc-${uid})`}>
          <motion.g style={{ y: mascella }}>
            <image href={src} x="0" y="0" width="400" height="400" />
          </motion.g>
        </g>

        {/* Bagliore degli occhi */}
        {EYES.map((e, i) => (
          <motion.ellipse
            key={i}
            cx={e.cx} cy={e.cy}
            rx={e.rx} ry={e.ry}
            fill="white"
            filter={`url(#eg-${uid})`}
            style={{ opacity: occhi, scale: respiro, originX: `${e.cx}px`, originY: `${e.cy}px` }}
          />
        ))}
      </svg>
    </div>
  )
}
