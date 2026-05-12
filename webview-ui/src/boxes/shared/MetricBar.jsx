import { motion } from 'framer-motion'

export default function MetricBar({ label, val, color, pct }) {
  return (
    <div className="space-y-0.5">
      <div className="flex justify-between text-xs uppercase font-bold">
        <span className="text-[#aaa]">{label}</span>
        <span style={{ color }}>{val}</span>
      </div>
      <div className="h-1 bg-[#1a1a1a] w-full">
        <motion.div className="h-full" style={{ backgroundColor: color, boxShadow: `0 0 3px ${color}50` }}
          initial={{ width: '0%' }} animate={{ width: `${Math.min(100, Math.max(0, pct))}%` }}
          transition={{ duration: 0.8, ease: 'easeOut' }}
        />
      </div>
    </div>
  )
}
