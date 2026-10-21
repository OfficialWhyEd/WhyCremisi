import React, { memo } from 'react'
import BoxChat from '../panels/BoxChat'

// Bolle della chat. Sono memoizzate: una risposta già finita non si
// ridisegna quando cambiano i meter (30 Hz) o altro stato dell'app.
// Solo se la bolla mostra un box richiesto dall'utente (boxType) riceve
// anche le misure live. Niente animazioni per token: il testo cresce e basta.

export const UserBubble = memo(function UserBubble ({ msg }) {
  return (
    <div className="flex flex-col items-end gap-0.5">
      <div className="border px-3 py-1.5 max-w-[80%] text-[11px] leading-5 text-white font-mono whitespace-pre-wrap break-words"
           style={{ backgroundColor: 'color-mix(in srgb, var(--bg-elevated) 50%, transparent)', borderColor: 'color-mix(in srgb, var(--amber) 20%, transparent)' }}>
        {msg.text}
      </div>
      <span className="text-xs mr-1 uppercase font-bold tracking-widest" style={{ color: 'var(--text-secondary)' }}>
        {msg.time}
      </span>
    </div>
  )
})

const haBox = (msg) => Array.isArray(msg.boxType) ? msg.boxType.length > 0 : !!msg.boxType

export const BotBubble = memo(function BotBubble ({ msg, isLatest, box }) {
  const streaming = !!msg.streaming
  return (
    <div className="min-w-0" style={{ overflowAnchor: 'none', contain: 'content' }}>
      <div className="p-3 relative border"
           style={{
             backgroundColor: 'var(--bg-hover-card)',
             borderColor: 'var(--border-subtle)'
           }}>
        <div className="absolute top-0 left-0 w-[2px] h-full" style={{ backgroundColor: isLatest ? 'var(--amber)' : 'var(--border-light)' }} />
        <div className="flex justify-between items-center mb-1">
          <span className="text-xs font-bold tracking-widest uppercase" style={{ color: isLatest ? 'var(--amber)' : 'var(--text-tertiary)' }}>
            WHYCREMISI AI
          </span>
          <span className="text-xs font-mono" style={{ color: 'var(--text-tertiary)' }}>{msg.time}</span>
        </div>
        <p className="text-[12px] leading-5 whitespace-pre-wrap break-words" style={{ color: 'var(--text-primary)' }}>
          {msg.text}
          {streaming && <span className="inline-block w-[5px] h-3 ml-1 align-middle" style={{ backgroundColor: 'var(--amber)', opacity: 0.8 }} />}
        </p>
        {!streaming && haBox(msg) && <BoxChat boxType={msg.boxType} {...box} />}
      </div>
    </div>
  )
}, (a, b) => {
  if (a.msg !== b.msg || a.isLatest !== b.isLatest) return false
  // le misure live contano solo se c'è un box visibile
  return haBox(b.msg) ? a.box === b.box : true
})
