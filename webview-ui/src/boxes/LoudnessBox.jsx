import { motion } from 'framer-motion'
import MetricBar from './shared/MetricBar'
import BoxWrapper from './shared/BoxWrapper'

export default function LoudnessBox({ lufs, peak, meterL, meterR, onDawCmd, ...rest }) {
  const lPct = Math.round(meterL * 100)
  const rPct = Math.round(meterR * 100)

  return (
    <BoxWrapper label="Loudness Analysis" color="#00E5FF" icon="equalizer" {...rest}>
      <div className="grid grid-cols-2 gap-1.5">
        <MetricBar label="LUFS Integrated" val={lufs > -60 ? `${lufs.toFixed(1)} LUFS` : '-∞'}
          color="#00E5FF" pct={Math.max(0, Math.min(100, (lufs + 60) / 60 * 100))} />
        <MetricBar label="Peak Level" val={peak > -60 ? `${peak.toFixed(1)} dB` : '-∞'}
          color="#FF6B35" pct={Math.max(0, Math.min(100, (peak + 60) / 60 * 100))} />
        <MetricBar label="Dynamic Range" val={`${(72 - (lPct + rPct) / 2 * 0.72).toFixed(1)} dB`}
          color="#00FFaa" pct={Math.max(0, 100 - (lPct + rPct) / 2)} />
        <MetricBar label="True Peak" val={peak > -60 ? `${(peak + 0.3).toFixed(1)} dBTP` : '-∞'}
          color="#FF6B35" pct={Math.max(0, Math.min(100, (peak + 60.3) / 60 * 100))} />
        <MetricBar label="RMS Level" val={lufs > -60 ? `${(lufs + 2.1).toFixed(1)} dB` : '-∞'}
          color="#FFB000" pct={Math.max(0, Math.min(100, (lufs + 62) / 62 * 100))} />
        <MetricBar label="Crest Factor" val={`${(peak - lufs).toFixed(1)} dB`}
          color="#DC143C" pct={Math.min(100, Math.max(0, (peak - lufs) * 5))} />
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
    </BoxWrapper>
  )
}
