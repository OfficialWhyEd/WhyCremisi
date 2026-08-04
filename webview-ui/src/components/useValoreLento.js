import { useEffect, useRef, useState } from 'react'

/*
  Un numero che si legge, invece di uno che sfarfalla.

  I meter arrivano dal plugin una trentina di volte al secondo, ed è giusto
  così: una barra che si muove deve muoversi. Ma la *cifra* accanto alla
  barra, aggiornata trenta volte al secondo, non è leggibile — l'occhio non
  fa in tempo a fissare un valore che è già cambiato, e il risultato è un
  tremolio continuo che stanca e non informa.

  Qui il valore viene campionato a una frequenza umana. La barra continua a
  scorrere fluida con il valore pieno; il numero si posa.

  Due accorgimenti in più:

  - `soglia` evita di riscrivere la cifra per variazioni che, una volta
    arrotondate, darebbero lo stesso testo: senza, un livello che oscilla
    fra -12,04 e -12,05 farebbe comunque un render a ogni campione.

  - le cifre vanno scritte con `tabular-nums` (classe `.num` in index.css),
    altrimenti passando da 1 a 8 la larghezza cambia e il numero "balla"
    lateralmente anche quando si aggiorna piano.
*/
export function useValoreLento (valore, { hz = 6, soglia = 0.05 } = {}) {
  const [mostrato, setMostrato] = useState(valore)
  const ultimoRef = useRef({ t: 0, v: valore })

  useEffect(() => {
    const intervallo = 1000 / hz
    const ora = performance.now()
    const passato = ora - ultimoRef.current.t

    const scrivi = () => {
      ultimoRef.current = { t: performance.now(), v: valore }
      setMostrato(valore)
    }

    // Cambio irrilevante: si lascia stare quello che c'è.
    if (Math.abs(valore - ultimoRef.current.v) < soglia) return

    if (passato >= intervallo) { scrivi(); return }

    // Cambio significativo arrivato troppo presto: si aspetta il turno,
    // così l'ultimo valore del periodo è quello che resta a schermo.
    const t = setTimeout(scrivi, intervallo - passato)
    return () => clearTimeout(t)
  }, [valore, hz, soglia])

  return mostrato
}

/*
  Come sopra, per un gruppo di valori che devono aggiornarsi insieme.
  Serve dove più cifre stanno sulla stessa riga: aggiornandole a scatti
  indipendenti la riga sembrerebbe agitarsi anche a valori fermi.
*/
export function useValoriLenti (oggetto, { hz = 6, soglia = 0.02 } = {}) {
  const [mostrato, setMostrato] = useState(oggetto)
  const vivoRef = useRef(oggetto)
  vivoRef.current = oggetto

  useEffect(() => {
    const id = setInterval(() => {
      setMostrato(prec => {
        const nuovo = vivoRef.current
        // Un solo timer per tutto il gruppo, e nessun render se nessuna
        // cifra si è mossa abbastanza da cambiare a schermo: a segnale
        // fermo il pannello non ridisegna affatto.
        const cambiato = Object.keys(nuovo).some(k => {
          const a = nuovo[k], b = prec[k]
          return typeof a === 'number' && typeof b === 'number'
            ? Math.abs(a - b) >= soglia
            : a !== b
        })
        return cambiato ? nuovo : prec
      })
    }, 1000 / hz)
    return () => clearInterval(id)
  }, [hz, soglia])

  return mostrato
}
