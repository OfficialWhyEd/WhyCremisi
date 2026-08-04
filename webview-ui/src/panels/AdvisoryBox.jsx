import { useState } from 'react'
import { motion } from 'framer-motion'

// Il tono cambia con la personalità dell'agente, il colore lo segue.
const TONI = {
  direct:       { colore: '#DC143C', priorita: 'Critico',      azione: 'Applica' },
  consultative: { colore: '#FFB000', priorita: 'Consigliato',  azione: 'Rivedi e applica' },
  analytical:   { colore: '#00E5FF', priorita: 'Rilevato',     azione: 'Applica' },
  creative:     { colore: '#AA44FF', priorita: 'Da esplorare', azione: 'Prova' },
  warm:         { colore: '#66FF88', priorita: 'Suggerito',    azione: 'Applica' },
}

// Curva di uscita esponenziale: parte decisa e si posa, invece di arrivare
// a velocità costante come farebbe un ease-in-out.
const POSA = [0.16, 1, 0.3, 1]

export default function AdvisoryBox({ suggestion, personality, onDawCmd, onAnalyzeFurther }) {
  const [chiuso, setChiuso] = useState(false)
  const [applicato, setApplicato] = useState(false)

  // Nessun suggerimento, nessun riquadro. Prima qui c'era un consiglio
  // inventato — 200-400 Hz, −2,4 dB, confidenza 98,2% — che compariva
  // anche quando l'AI non aveva detto niente, e il pulsante lo applicava.
  if (chiuso || !suggestion) return null

  const s = suggestion
  const tono = TONI[personality?.style] || TONI.warm
  const { colore } = tono

  const bandaTesto = s.freqLow && s.freqHigh
    ? `${s.freqLow}–${s.freqHigh} Hz`
    : (s.label || '')
  const guadagno = typeof s.gainDb === 'number' ? s.gainDb : null
  // La confidenza si mostra solo se l'ha davvero fornita chi suggerisce.
  const confidenza = typeof s.confidence === 'number' ? Math.round(s.confidence * 100) : null

  return (
    <motion.div
      initial={{ opacity: 0, y: 12, filter: 'blur(6px)' }}
      animate={{ opacity: 1, y: 0, filter: 'blur(0px)' }}
      transition={{ duration: 0.55, ease: POSA }}
      className="mt-2"
    >
      {/* Scocca esterna e nucleo interno: due raggi concentrici, come una
          piastra montata dentro una cornice. Il bordo colorato dice a colpo
          d'occhio con che tono sta parlando l'agente. */}
      <div
        className="rounded-[18px] p-[3px] transition-colors duration-500"
        style={{ background: `linear-gradient(160deg, ${colore}26, transparent 60%)` }}
      >
        <div
          className="relative rounded-[15px] bg-[#101010] overflow-hidden"
          style={{ boxShadow: `inset 0 1px 0 rgba(255,255,255,0.06), 0 8px 24px -12px ${colore}55` }}
        >
          {/* Filo di luce lungo il bordo superiore: nasce dal colore del tono */}
          <div
            className="absolute inset-x-0 top-0 h-px"
            style={{ background: `linear-gradient(90deg, transparent, ${colore}, transparent)` }}
          />

          <div className="p-4">
            {/* Intestazione: la banda è il soggetto, il resto è contorno */}
            <div className="flex items-start justify-between gap-3 mb-3">
              <div className="min-w-0">
                <div
                  className="text-[9px] font-bold uppercase tracking-[0.18em] mb-1"
                  style={{ color: colore }}
                >
                  {tono.priorita}
                </div>
                <h3 className="text-white font-bold leading-none tracking-tight text-[20px]">
                  {bandaTesto}
                </h3>
                {s.description && (
                  <p className="text-[11px] text-[#8a8a8a] mt-1.5 leading-snug">
                    {s.description}
                  </p>
                )}
              </div>

              {/* La correzione proposta, grande quanto la sua importanza */}
              {guadagno !== null && (
                <div className="text-right flex-shrink-0">
                  <div
                    className="font-mono font-bold leading-none text-[26px] tabular-nums"
                    style={{ color: colore }}
                  >
                    {guadagno > 0 ? '+' : ''}{guadagno.toFixed(1)}
                  </div>
                  <div className="text-[9px] text-[#6f6f6f] uppercase tracking-[0.16em] mt-1">
                    dB
                  </div>
                </div>
              )}
            </div>

            {/* Dettagli veri, e solo quelli che ci sono davvero */}
            {(confidenza !== null || typeof s.crestDb === 'number') && (
              <div className="flex items-center gap-4 mb-3.5 text-[10px] font-mono text-[#7a7a7a]">
                {confidenza !== null && (
                  <span className="flex items-center gap-1.5">
                    <span className="w-8 h-[3px] rounded-full bg-white/10 overflow-hidden">
                      <motion.span
                        className="block h-full rounded-full"
                        style={{ backgroundColor: colore }}
                        initial={{ width: 0 }}
                        animate={{ width: `${confidenza}%` }}
                        transition={{ duration: 0.7, ease: POSA, delay: 0.15 }}
                      />
                    </span>
                    confidenza {confidenza}%
                  </span>
                )}
                {/* Fattore di cresta misurato: true peak meno RMS. Prima
                    qui c'era "transienti X%", ricavato dalla correlazione
                    fra i canali — che misura la fase, non i transienti.
                    Riferimento: 14–20 dB non compresso, sotto 9 è limitato
                    a morte. */}
                {typeof s.crestDb === 'number' && (
                  <span title="Fattore di cresta: 14–20 dB non compresso, sotto 9 è limitato a morte">
                    cresta {s.crestDb.toFixed(1)} dB
                  </span>
                )}
              </div>
            )}

            <div className="flex flex-wrap items-center gap-2">
              <motion.button
                whileTap={{ scale: 0.97 }}
                onClick={() => {
                  setApplicato(true)
                  onDawCmd('applyEQ', {
                    freq: s.freqLow && s.freqHigh ? `${s.freqLow}-${s.freqHigh}` : s.label,
                    gain: guadagno,
                  })
                }}
                disabled={applicato || guadagno === null}
                className="group inline-flex items-center gap-2 rounded-full pl-4 pr-1.5 py-1.5 text-[11px] font-bold tracking-tight transition-all duration-500 disabled:opacity-100"
                style={applicato
                  ? { backgroundColor: '#0f2b1f', color: '#66FF88' }
                  : { backgroundColor: colore, color: '#0a0a0a' }}
              >
                {applicato ? 'Applicato' : tono.azione}
                <span
                  className="w-6 h-6 rounded-full flex items-center justify-center transition-transform duration-500 group-hover:translate-x-0.5"
                  style={{ backgroundColor: applicato ? '#66FF8822' : 'rgba(0,0,0,0.18)' }}
                >
                  <span className="material-symbols-outlined text-[13px]">
                    {applicato ? 'check' : 'arrow_forward'}
                  </span>
                </span>
              </motion.button>

              <button
                className="rounded-full px-3.5 py-1.5 text-[11px] font-semibold text-[#9a9a9a] transition-colors duration-300 hover:text-white"
                style={{ boxShadow: 'inset 0 0 0 1px rgba(255,255,255,0.10)' }}
                onClick={() => onAnalyzeFurther && onAnalyzeFurther(personality?.style)}
              >
                Approfondisci
              </button>

              <button
                className="ml-auto text-[11px] text-[#6f6f6f] transition-colors duration-300 hover:text-[#9a9a9a]"
                onClick={() => setChiuso(true)}
              >
                Ignora
              </button>
            </div>
          </div>
        </div>
      </div>
    </motion.div>
  )
}
