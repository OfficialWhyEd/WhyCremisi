import { useState } from 'react'
import { motion } from 'framer-motion'
import MetricBar from './shared/MetricBar'
import BoxWrapper from './shared/BoxWrapper'

export default function VectorscopeBox({ correlation, onDawCmd, ...rest }) {
  const [midSide, setMidSide] = useState(false)
  const [phaseInvert, setPhaseInvert] = useState(false)
  return (
    <BoxWrapper label="Vectorscope" color="#AA44FF" icon="donut_small" {...rest}>
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
            &phi; = {correlation.toFixed(3)}
          </div>
        </div>
        <div className="flex-1 space-y-1.5 min-w-0">
          <MetricBar label="Phase Correlation" val={correlation.toFixed(3)}
            color={correlation < 0.3 ? '#DC143C' : correlation < 0.6 ? '#FFB000' : '#00FFaa'}
            pct={Math.max(0, Math.min(100, (correlation + 1) * 50))} />
          <MetricBar label="Mono Compatibility"
            val={correlation < 0.3 ? 'CRITICAL' : correlation < 0.6 ? 'FAIR' : 'GOOD'}
            color={correlation < 0.3 ? '#DC143C' : correlation < 0.6 ? '#FFB000' : '#00FFaa'}
            pct={Math.max(0, Math.min(100, correlation * 100))} />
          <div className="flex gap-1 pt-1">
            <motion.button whileTap={{ scale: 0.95 }}
              className={`flex-1 py-1 text-[9px] font-bold uppercase tracking-wider border ${midSide ? 'bg-[#AA44FF] text-black border-[#AA44FF]' : 'text-[#888] border-[#333] hover:border-[#AA44FF]'}`}
              onClick={() => { setMidSide(!midSide); onDawCmd('midSide', { enabled: !midSide }) }}>
              M/S
            </motion.button>
            <motion.button whileTap={{ scale: 0.95 }}
              className={`flex-1 py-1 text-[9px] font-bold uppercase tracking-wider border ${phaseInvert ? 'bg-[#DC143C] text-black border-[#DC143C]' : 'text-[#888] border-[#333] hover:border-[#DC143C]'}`}
              onClick={() => { setPhaseInvert(!phaseInvert); onDawCmd('phaseInvert', { enabled: !phaseInvert }) }}>
              &phi; INVERT
            </motion.button>
            <motion.button whileTap={{ scale: 0.95 }}
              className="flex-1 py-1 text-[9px] font-bold uppercase tracking-wider border text-[#888] border-[#333] hover:border-[#00FFaa]"
              onClick={() => onDawCmd('mono')}>
              MONO
            </motion.button>
          </div>
        </div>
      </div>
    </BoxWrapper>
  )
}
