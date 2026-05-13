import { useState } from 'react'
import { motion } from 'framer-motion'
import BoxWrapper from './shared/BoxWrapper'

export default function AdvisoryBox({ suggestion, personality, onDawCmd, onAnalyzeFurther }) {
  const [advDismissed, setAdvDismissed] = useState(false)
  const [advExecuted, setAdvExecuted] = useState(false)

  if (advDismissed) return null

  const s = suggestion || { freqLow: 200, freqHigh: 400, gainDb: -2.4, label: '200Hz-400Hz', description: 'harmonic crowding', transientPres: 84, confidence: 0.982 }
  const advColor = personality?.style === 'direct' ? '#DC143C' :
    personality?.style === 'consultative' ? '#FFB000' :
    personality?.style === 'analytical' ? '#00E5FF' :
    personality?.style === 'creative' ? '#AA44FF' : '#66FF88'
  const priorityLabel = personality?.style === 'direct' ? 'Critical' :
    personality?.style === 'consultative' ? 'Advisory' :
    personality?.style === 'analytical' ? 'Info' :
    personality?.style === 'creative' ? 'Exploratory' : 'Suggested'
  const actionLabel = personality?.style === 'direct' ? 'FORCE APPLY CHAIN' :
    personality?.style === 'consultative' ? 'REVIEW & APPLY' :
    personality?.style === 'analytical' ? 'SIMULATE CHAIN' :
    personality?.style === 'creative' ? 'EXPLORE ALTERNATIVES' : 'APPLY SUGGESTED CHAIN'
  const confidenceTag = `CONF: ${(s.confidence * 100).toFixed(0)}%`
  const styleName = personality?.style?.toUpperCase() || 'DEFAULT'
  const freqLabel = `${s.freqLow}Hz-${s.freqHigh}Hz`

  return (
    <motion.div
      initial={{ opacity: 0, y: 10, scale: 0.98, filter: 'blur(4px)' }}
      animate={{ opacity: 1, y: 0, scale: 1, filter: 'blur(0)' }}
      transition={{ delay: 0.35, duration: 0.6, ease: [0.22, 1, 0.36, 1] }}
      className="mt-2 animate-advisory-in group relative"
    >
      <div className="absolute -top-3 -right-1 opacity-5 pointer-events-none select-none">
        <pre className="text-xs leading-tight text-white font-mono">0x45 0x21 0x88{'\n'}[TRANS_LOCK]{'\n'}0xFF 0x00 0x12</pre>
      </div>
      <div className="advisory-card advisory-breathe bg-[#121212] p-4 relative font-mono transition-all duration-500 hover:bg-[#161616]"
        style={{ borderColor: advColor + '66' }}
      >
        <div className="absolute top-2 right-3 flex items-end gap-[2px] h-5 opacity-40">
          {[40, 70, 55, 90, 65].map((h, i) => (
            <div key={i} className="w-[2px]" style={{ height: h + '%', backgroundColor: advColor }} />
          ))}
        </div>
        <div className="flex justify-between items-start mb-3 border-b border-[#222] pb-2">
          <div className="flex items-center gap-2">
            <div className="w-1 h-5 relative overflow-hidden flex-shrink-0" style={{ backgroundColor: advColor }}>
              <motion.div className="absolute inset-0 bg-white/20" animate={{ y: ['0%', '100%', '0%'] }} transition={{ repeat: Infinity, duration: 2 }} />
            </div>
            <div>
              <span className="text-xs font-bold tracking-tighter uppercase block" style={{ color: advColor }}>
                AI_MASTERING_ADVISORY_{styleName}
              </span>
              <span className="text-xs text-[#888888] tracking-widest">
                NODE: CREMISI_X9 . STYLE: {styleName} . {freqLabel}
              </span>
            </div>
          </div>
          <div className="flex flex-col items-end">
            <span className="text-xs text-white/40 font-bold uppercase tracking-widest">Priority: <span style={{ color: advColor }}>{priorityLabel}</span></span>
            {personality?.experienceLevel && (
              <span className="text-xs text-[#888888] font-mono mt-0.5">EXP LVL: {personality.experienceLevel}</span>
            )}
          </div>
        </div>
        <div className="text-sm leading-relaxed text-[#FFB000] mb-3">
          <p className="text-xs leading-relaxed">
            {personality?.style === 'direct' ? 'Aggressive ' : personality?.style === 'consultative' ? 'Potential ' : personality?.style === 'creative' ? 'Creative ' : 'Detected '}
            {s.description} in{' '}
            <span className="text-white font-bold px-0.5" style={{ borderBottom: '1px solid ' + advColor + '66' }}>{freqLabel}</span>.{' '}
            {personality?.style === 'direct' ? 'Applying surgical dip of' :
             personality?.style === 'consultative' ? 'Consider gentle dip of' :
             personality?.style === 'analytical' ? 'Recommended dynamic dip of' :
             personality?.style === 'creative' ? 'Exploring experimental sculpting of' :
             'Suggesting gentle dip of'}{' '}
            <span className="text-white px-1 font-bold" style={{ backgroundColor: advColor }}>{s.gainDb}dB</span>.{' '}
            Transient preservation at{' '}
            <span className="text-white underline decoration-dotted">{s.transientPres}%</span>.
          </p>
        </div>
        <div className="flex items-center gap-2 mb-3 text-xs font-mono text-[#666] opacity-60">
          <span>HEX: {advColor}</span><span>.</span><span>VAL: {s.gainDb}dB_COR</span><span>.</span><span>{confidenceTag}</span><span>.</span><span>LAT: 0.2ms</span>
        </div>
        <div className="flex flex-wrap gap-2">
          <motion.button
            whileHover={{ scale: 1.02 }} whileTap={{ scale: 0.95 }}
            onClick={() => { setAdvExecuted(true); onDawCmd('applyEQ', { freq: freqLabel, gain: s.gainDb }) }}
            style={advExecuted ? { backgroundColor: '#00FFaa', color: '#000', border: '1px solid #00FFaa' } : { backgroundColor: advColor, color: '#fff' }}
            className="relative px-4 py-1.5 text-xs font-bold uppercase tracking-widest transition-all"
          >
            <span className="flex items-center gap-1.5">
              <span className="material-symbols-outlined text-[12px]">bolt</span>
              {advExecuted ? 'APPLIED' : actionLabel}
            </span>
          </motion.button>
          <motion.button
            className="border border-[#4d4d4d] text-[#888888] px-4 py-1.5 text-xs font-bold uppercase tracking-widest hover:border-[#FFB000] hover:text-[#FFB000] hover:shadow-[0_0_15px_rgba(255,176,0,0.2)] transition-all"
            whileTap={{ scale: 0.95 }}
            onClick={() => onAnalyzeFurther && onAnalyzeFurther(personality?.style)}
          >
            ANALYZE FURTHER
          </motion.button>
          <button className="text-[#888888] hover:text-white text-xs font-bold uppercase self-center transition-colors hover:underline"
            style={{ textDecorationColor: advColor }}
            onClick={() => setAdvDismissed(true)}>DISMISS</button>
        </div>
      </div>
    </motion.div>
  )
}
