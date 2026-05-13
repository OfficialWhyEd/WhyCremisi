import { useState } from 'react'
import { motion } from 'framer-motion'
import BoxWrapper from './shared/BoxWrapper'

export default function CompressorBox({ correlation, onDawCmd, ...rest }) {
  const [compThresh, setCompThresh] = useState(-18)
  const [compRatio, setCompRatio] = useState(4)

  return (
    <BoxWrapper label="Compressor Settings" color="#9B59B6" icon="compress" {...rest}>
      <div className="space-y-2">
        <div className="grid grid-cols-3 gap-2">
          <div>
            <div className="flex justify-between text-[9px] font-mono mb-0.5">
              <span className="text-[#888] uppercase">Threshold</span>
              <span className="text-white font-bold">{compThresh} dB</span>
            </div>
            <input type="range" min="-40" max="0" step="1" value={compThresh}
              onChange={(e) => { const v = parseInt(e.target.value); setCompThresh(v); onDawCmd('compThreshold', { value: v }) }}
              className="w-full h-1 accent-[#9B59B6] cursor-pointer"
            />
          </div>
          <div>
            <div className="flex justify-between text-[9px] font-mono mb-0.5">
              <span className="text-[#888] uppercase">Ratio</span>
              <span className="text-white font-bold">{compRatio}:1</span>
            </div>
            <input type="range" min="1" max="20" step="0.5" value={compRatio}
              onChange={(e) => { const v = parseFloat(e.target.value); setCompRatio(v); onDawCmd('compRatio', { value: v }) }}
              className="w-full h-1 accent-[#DC143C] cursor-pointer"
            />
          </div>
          <div className="flex flex-col justify-center items-center">
            <div className="text-[10px] text-[#00FFaa] font-bold font-mono">
              {Math.max(0, (Math.abs(correlation) * 2 - 1) * compThresh * -0.1).toFixed(1)} dB
            </div>
            <div className="text-[8px] text-[#888] uppercase tracking-wider">Gain Red</div>
          </div>
        </div>
        <div className="grid grid-cols-2 gap-1">
          <motion.button whileTap={{ scale: 0.95 }}
            className="py-1 text-[9px] font-bold uppercase tracking-wider border text-[#888] border-[#333] hover:border-[#9B59B6]"
            onClick={() => onDawCmd('compBypass', { enabled: false })}>
            BYPASS
          </motion.button>
          <motion.button whileTap={{ scale: 0.95 }}
            className="py-1 text-[9px] font-bold uppercase tracking-wider border text-[#888] border-[#333] hover:border-[#00FFaa]"
            onClick={() => onDawCmd('compAuto')}>
            AUTO
          </motion.button>
        </div>
      </div>
    </BoxWrapper>
  )
}
