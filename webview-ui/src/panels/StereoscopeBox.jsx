import { useState } from 'react'
import { motion } from 'framer-motion'
import MetricBar from './shared/MetricBar'
import BoxWrapper from './shared/BoxWrapper'

export default function StereoscopeBox({ meterL, meterR, onDawCmd, ...rest }) {
  const [midSide, setMidSide] = useState(false)
  const lPct = Math.round(meterL * 100)
  const rPct = Math.round(meterR * 100)

  return (
    <BoxWrapper label="Stereo Field" color="#DC143C" icon="swap_horiz" {...rest}>
      <div className="grid grid-cols-2 gap-1.5">
        <MetricBar label="L/R Balance" val={lPct > rPct ? `${lPct - rPct}% L` : rPct > lPct ? `${rPct - lPct}% R` : 'CENTER'}
          color="#DC143C" pct={Math.abs(lPct - rPct)} />
        <MetricBar label="Side Content" val={`${Math.round(Math.abs(meterL - meterR) * 150)}%`}
          color="#FFB000" pct={Math.round(Math.abs(meterL - meterR) * 150)} />
        <MetricBar label="Correlation" val={`+${(0.62 + (meterL + meterR) * 0.15).toFixed(2)}`}
          color="#00E5FF" pct={Math.round((0.62 + (meterL + meterR) * 0.15) * 100)} />
        <MetricBar label="Width" val={`${Math.round((Math.abs(meterL - meterR) + 0.3) * 100)}%`}
          color="#00FFaa" pct={Math.round((Math.abs(meterL - meterR) + 0.3) * 100)} />
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
    </BoxWrapper>
  )
}
