import { motion } from 'framer-motion'
import BoxWrapper from './shared/BoxWrapper'

export default function KnobBox({ meterL, meterR, ...rest }) {
  const items = [
    { label: 'Master Pan', val: 0, color: '#00E5FF' },
    { label: 'Stereo Width', val: Math.round((Math.abs(meterL - meterR) + 0.3) * 100), color: '#FFB000' },
  ]

  return (
    <BoxWrapper label="Pan / Stereo Position" color="#00E5FF" icon="track_changes" {...rest}>
      <div className="grid grid-cols-2 gap-3">
        {items.map(({ label, val, color }) => (
          <div key={label} className="flex flex-col items-center gap-1">
            <svg viewBox="0 0 60 60" className="w-12 h-12 rotate-[-90deg]">
              <circle cx="30" cy="30" r="22" fill="none" stroke="#222" strokeWidth="3" />
              <motion.circle cx="30" cy="30" r="22" fill="none" stroke={color} strokeWidth="3"
                strokeLinecap="round"
                initial={{ strokeDasharray: '0 138' }}
                animate={{ strokeDasharray: `${(val / 100) * 138} 138` }}
                transition={{ duration: 0.8 }}
                style={{ filter: `drop-shadow(0 0 3px ${color})` }}
              />
            </svg>
            <span className="text-xs text-[#aaa] uppercase font-bold">{label}</span>
            <span className="text-xs font-mono" style={{ color }}>{val}%</span>
          </div>
        ))}
      </div>
    </BoxWrapper>
  )
}
