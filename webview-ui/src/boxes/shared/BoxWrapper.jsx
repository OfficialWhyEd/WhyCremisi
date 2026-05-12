import { motion } from 'framer-motion'

export default function BoxWrapper({ label, color, icon, children, actions, delay = 0.35, minimized, onMinimize, onClose }) {
  return (
    <motion.div initial={{ opacity: 0, y: 4 }} animate={{ opacity: 1, y: 0 }} transition={{ delay }}
      className="mt-2 bg-[#0d0d0d]/70 border border-[#222] overflow-hidden"
    >
      <div className="flex items-center justify-between px-2 py-1 border-b border-[#1a1a1a]">
        <div className="flex items-center gap-1.5">
          <span className="material-symbols-outlined text-xs" style={{ color }}>{icon}</span>
          <span className="text-xs font-bold uppercase tracking-widest" style={{ color }}>{label}</span>
        </div>
        <div className="flex gap-1">
          {actions}
          {onMinimize && (
            <span className="material-symbols-outlined text-xs text-[#666] hover:text-white cursor-pointer transition-colors"
              onClick={onMinimize}>
              {minimized ? 'expand_more' : 'expand_less'}
            </span>
          )}
          {onClose && (
            <span className="material-symbols-outlined text-xs text-[#666] hover:text-[#DC143C] cursor-pointer transition-colors"
              onClick={onClose}>close</span>
          )}
        </div>
      </div>
      {!minimized && <div className="p-2">{children}</div>}
    </motion.div>
  )
}
