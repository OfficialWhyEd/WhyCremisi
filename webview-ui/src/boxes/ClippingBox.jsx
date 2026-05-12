import { useState } from 'react'
import { motion } from 'framer-motion'
import BoxWrapper from './shared/BoxWrapper'

export default function ClippingBox({ peak, onDawCmd, clippingCount = 0, ...rest }) {
  const [clipThreshold, setClipThreshold] = useState(-1)
  const isClipping = peak > clipThreshold
  const clipPct = Math.min(100, Math.max(0, ((peak - clipThreshold) / 6) * 100))

  return (
    <BoxWrapper label="Clipping Detection" color="#FF6B35" icon="report" {...rest}>
      <div className="space-y-2">
        <div className={`p-2 border ${isClipping ? 'border-[#DC143C] bg-[#DC143C]/10' : 'border-[#333] bg-[#111]'} transition-all`}>
          <div className="flex justify-between items-center mb-1">
            <span className="text-xs font-bold uppercase tracking-wider" style={{ color: isClipping ? '#DC143C' : '#888' }}>
              {isClipping ? 'Clipping Detected' : 'No Clipping'}
            </span>
            <span className="text-xs font-mono" style={{ color: isClipping ? '#DC143C' : '#888' }}>
              {peak.toFixed(1)} dB
            </span>
          </div>
          <div className="h-2 bg-[#1a1a1a] w-full relative overflow-hidden">
            <motion.div className="h-full absolute left-0 top-0"
              style={{ backgroundColor: isClipping ? '#DC143C' : '#00FFaa', boxShadow: isClipping ? '0 0 8px #DC143C' : 'none' }}
              animate={{ width: `${clipPct}%` }}
              transition={{ duration: 0.1 }}
            />
            <div className="absolute top-0 bottom-0 w-px bg-[#FFB000]" style={{ left: `${((clipThreshold + 60) / 72) * 100}%` }} />
          </div>
        </div>
        <div className="flex items-center gap-2">
          <span className="text-[9px] text-[#888] font-mono uppercase">Threshold</span>
          <input type="range" min="-18" max="0" step="0.5" value={clipThreshold}
            onChange={(e) => setClipThreshold(parseFloat(e.target.value))}
            className="flex-1 h-1 accent-[#FF6B35] cursor-pointer"
          />
          <span className="text-[10px] text-white font-mono w-8 text-right">{clipThreshold.toFixed(1)} dB</span>
        </div>
        <div className="flex justify-between items-center text-[9px] font-mono text-[#888]">
          <span>CLIPS: <span className="text-[#FF6B35] font-bold">{clippingCount}</span></span>
          <span className="text-[#666]">RESET ON STOP</span>
        </div>
        <div className="flex gap-1">
          <motion.button whileTap={{ scale: 0.95 }}
            className="flex-1 py-1 text-[9px] font-bold uppercase tracking-wider border text-[#888] border-[#333] hover:border-[#DC143C]"
            onClick={() => onDawCmd('softClip', { enabled: true })}>
            SOFT CLIP
          </motion.button>
          <motion.button whileTap={{ scale: 0.95 }}
            className="flex-1 py-1 text-[9px] font-bold uppercase tracking-wider border text-[#888] border-[#333] hover:border-[#FF6B35]"
            onClick={() => onDawCmd('hardClip', { enabled: true })}>
            HARD CLIP
          </motion.button>
          <motion.button whileTap={{ scale: 0.95 }}
            className="flex-1 py-1 text-[9px] font-bold uppercase tracking-wider border text-[#888] border-[#333] hover:border-[#00E5FF]"
            onClick={() => onDawCmd('ceiling', { value: -1 })}>
            -1dB CEIL
          </motion.button>
        </div>
      </div>
    </BoxWrapper>
  )
}
