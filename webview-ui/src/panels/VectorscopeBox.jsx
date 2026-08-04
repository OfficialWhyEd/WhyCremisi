import { useState, useRef, useEffect } from 'react'
import { motion } from 'framer-motion'
import MetricBar from './shared/MetricBar'
import BoxWrapper from './shared/BoxWrapper'

// Su tela invece che in SVG: la traccia si ridisegna una trentina di volte
// al secondo e ogni punto e' un nodo del DOM in meno da far girare al
// browser. Con l'SVG i 256 punti diventavano 256 elementi rimpiazzati a
// ogni aggiornamento.
function ScopeCanvas({ points, correlation, size = 132 }) {
  const canvasRef = useRef(null)
  const scia = useRef(null)

  useEffect(() => {
    const canvas = canvasRef.current
    if (!canvas) return

    const dpr = window.devicePixelRatio || 1
    if (canvas.width !== size * dpr) {
      canvas.width = size * dpr
      canvas.height = size * dpr
    }

    const ctx = canvas.getContext('2d')
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0)

    const c = size / 2
    const r = size / 2 - 6

    // Persistenza: invece di cancellare, si sovrappone un velo scuro. Il
    // disegno di un istante fa resta appena visibile sotto quello nuovo,
    // e il movimento si legge come una scia invece che come uno sfarfallio.
    ctx.fillStyle = 'rgba(10,10,10,0.28)'
    ctx.fillRect(0, 0, size, size)

    // Reticolo: cerchio esterno, cerchio a meta' scala, assi mid/side e le
    // due diagonali che marcano i casi limite, tutto su L o tutto su R.
    ctx.strokeStyle = 'rgba(255,255,255,0.07)'
    ctx.lineWidth = 1
    ctx.beginPath(); ctx.arc(c, c, r, 0, Math.PI * 2); ctx.stroke()
    ctx.beginPath(); ctx.arc(c, c, r * 0.5, 0, Math.PI * 2); ctx.stroke()

    ctx.strokeStyle = 'rgba(255,255,255,0.05)'
    ctx.beginPath()
    ctx.moveTo(c - r, c); ctx.lineTo(c + r, c)
    ctx.moveTo(c, c - r); ctx.lineTo(c, c + r)
    ctx.stroke()

    ctx.setLineDash([2, 4])
    const d = r * 0.72
    ctx.beginPath()
    ctx.moveTo(c - d, c - d); ctx.lineTo(c + d, c + d)
    ctx.moveTo(c - d, c + d); ctx.lineTo(c + d, c - d)
    ctx.stroke()
    ctx.setLineDash([])

    if (!points || points.length < 4) {
      scia.current = null
      return
    }

    // Il colore segue la fase: viola quando i canali sono d'accordo, ambra
    // quando iniziano a divergere, rosso quando si cancellerebbero in mono.
    const tinta = correlation < 0.2 ? '220,20,60'
                : correlation < 0.5 ? '255,176,0'
                : '170,68,255'

    // Due passate: una larga e velata fa da alone, una sottile e piena
    // disegna il tratto. E' cosi' che un fosforo vero appare acceso.
    for (const [larghezza, opacita] of [[3.5, 0.14], [1.2, 0.95]]) {
      ctx.strokeStyle = `rgba(${tinta},${opacita})`
      ctx.lineWidth = larghezza
      ctx.lineJoin = 'round'
      ctx.lineCap = 'round'
      ctx.beginPath()
      for (let i = 0; i + 1 < points.length; i += 2) {
        const x = c + points[i] * r
        const y = c - points[i + 1] * r
        if (i === 0) ctx.moveTo(x, y)
        else ctx.lineTo(x, y)
      }
      ctx.stroke()
    }
  }, [points, correlation, size])

  return (
    <canvas
      ref={canvasRef}
      style={{ width: size, height: size, display: 'block' }}
      className="rounded-full bg-[#0a0a0a]"
    />
  )
}

export default function VectorscopeBox({ correlation = 0, scopePoints = [], stereoState = null, onDawCmd, ...rest }) {
  // Lo stato lo tiene il plugin, che è quello che tocca l'audio. Prima
  // ciascun pulsante teneva il proprio interruttore locale: si accendeva
  // comunque, anche quando il comando non arrivava da nessuna parte —
  // e infatti non arrivava, perché veniva spedito a un indirizzo OSC
  // inventato che nessuna DAW ascolta.
  const sideOnly    = stereoState?.sideOnly ?? false
  const phaseInvert = stereoState?.phaseInvert ?? false
  const mono        = stereoState?.mono ?? false

  const segnale = scopePoints && scopePoints.length >= 4

  // Giudizio in parole: e' quello che serve davvero sapere guardando.
  const stato = !segnale ? { testo: 'nessun segnale', colore: '#6f6f6f' }
    : correlation < 0    ? { testo: 'si cancella in mono', colore: '#DC143C' }
    : correlation < 0.3  ? { testo: 'fase critica',        colore: '#DC143C' }
    : correlation < 0.6  ? { testo: 'accettabile',         colore: '#FFB000' }
    : correlation > 0.97 ? { testo: 'quasi mono',          colore: '#FFB000' }
    :                      { testo: 'in fase',             colore: '#00FFaa' }

  return (
    <BoxWrapper label="Vectorscope" color="#AA44FF" icon="donut_small" {...rest}>
      <div className="flex gap-3.5">
        <div className="relative flex-shrink-0">
          {/* Scocca e nucleo: il tondo dello strumento sta dentro una
              cornice, con raggi concentrici invece che appoggiato piatto. */}
          <div className="rounded-full p-[3px] bg-white/[0.04] ring-1 ring-white/[0.06]">
            <ScopeCanvas points={scopePoints} correlation={correlation} />
          </div>

          {/* Etichette degli assi, dentro lo strumento come su un oscilloscopio */}
          <span className="absolute top-1.5 left-1/2 -translate-x-1/2 text-[7px] font-mono text-[#5a5a5a] tracking-widest">M</span>
          <span className="absolute bottom-1.5 left-1/2 -translate-x-1/2 text-[7px] font-mono text-[#5a5a5a] tracking-widest">M</span>
          <span className="absolute left-1.5 top-1/2 -translate-y-1/2 text-[7px] font-mono text-[#5a5a5a]">S</span>
          <span className="absolute right-1.5 top-1/2 -translate-y-1/2 text-[7px] font-mono text-[#5a5a5a]">S</span>
        </div>

        <div className="flex-1 min-w-0 flex flex-col">
          {/* La correlazione in grande: e' il numero per cui si apre questo
              pannello, e prima stava in una barra come tutti gli altri. */}
          <div className="mb-2.5">
            <div className="flex items-baseline gap-2">
              <span
                className="font-mono font-bold text-[26px] leading-none tabular-nums transition-colors duration-500"
                style={{ color: stato.colore }}
              >
                {segnale ? (correlation >= 0 ? '+' : '') + correlation.toFixed(2) : '—'}
              </span>
              <span className="text-[10px] text-[#7a7a7a] tracking-tight">correlazione</span>
            </div>
            <div
              className="text-[10px] font-semibold mt-1 transition-colors duration-500"
              style={{ color: stato.colore }}
            >
              {stato.testo}
            </div>
          </div>

          {/* Scala da −1 a +1 con l'indicatore che scorre: mostra dove sta
              il valore rispetto agli estremi, non solo quanto e' grande. */}
          <div className="relative h-[5px] rounded-full bg-white/[0.06] mb-1">
            <div className="absolute inset-y-0 left-0 w-1/2 rounded-l-full bg-[#DC143C]/20" />
            <motion.div
              className="absolute top-1/2 w-[3px] h-[11px] rounded-full -translate-y-1/2"
              style={{ backgroundColor: stato.colore }}
              animate={{ left: `${Math.max(0, Math.min(100, (correlation + 1) * 50))}%` }}
              transition={{ duration: 0.35, ease: [0.16, 1, 0.3, 1] }}
            />
          </div>
          <div className="flex justify-between text-[8px] font-mono text-[#5a5a5a] mb-3">
            <span>−1 opposti</span><span>0</span><span>+1 identici</span>
          </div>

          <div className="flex gap-1 mt-auto">
            {[
              // "Lati": si ascolta solo quello che sta fuori dal centro.
              { attivo: sideOnly, testo: 'Lati', tinta: '#AA44FF', aiuto: 'Ascolta solo il segnale laterale',
                click: () => onDawCmd('midSide', { enabled: !sideOnly }) },
              // Inversione di fase del destro: se in mono il suono diventa
              // più pieno invece che più magro, c'era un problema di fase.
              { attivo: phaseInvert, testo: 'Ø', tinta: '#DC143C', aiuto: 'Inverte la fase del canale destro',
                click: () => onDawCmd('phaseInvert', { enabled: !phaseInvert }) },
              { attivo: mono, testo: 'Mono', tinta: '#00FFaa', aiuto: 'Somma in mono per provare la compatibilità',
                click: () => onDawCmd('mono', { enabled: !mono }) },
            ].map(b => (
              <motion.button
                key={b.testo}
                whileTap={{ scale: 0.96 }}
                onClick={b.click}
                title={b.aiuto}
                className="flex-1 rounded-full py-1 text-[9px] font-bold tracking-tight transition-all duration-300"
                style={b.attivo
                  ? { backgroundColor: b.tinta, color: '#0a0a0a' }
                  : { color: '#9a9a9a', boxShadow: 'inset 0 0 0 1px rgba(255,255,255,0.10)' }}
              >
                {b.testo}
              </motion.button>
            ))}
          </div>
        </div>
      </div>
    </BoxWrapper>
  )
}
