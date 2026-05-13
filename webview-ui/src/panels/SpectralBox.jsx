import { motion } from 'framer-motion'
import BoxWrapper from './shared/BoxWrapper'

export default function SpectralBox({ spectrum, onDawCmd, ...rest }) {
  return (
    <BoxWrapper label="Spectral Analyzer" color="#00E5FF" icon="finance" {...rest}>
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
    </BoxWrapper>
  )
}
