import { useEffect, useRef } from 'react'
import { useMotionValue } from 'framer-motion'

/*
  Apertura della bocca guidata dall'audio.

  Sostituisce lo smorzamento che stava dentro MaskLogo e BotFace, che aveva
  tre difetti e per questo la bocca "si apriva e restava lì":

  1. Era scritto dentro un useEffect legato al livello. Se il livello smetteva
     di cambiare — audio fermo, meter in pausa, finestra in secondo piano —
     l'effetto non ripartiva e il valore restava congelato a metà corsa. La
     bocca rimaneva aperta a metà finché qualcosa non muoveva di nuovo il
     meter.

  2. Lo smorzamento era a coefficiente fisso per aggiornamento, non per tempo.
     Con i meter che arrivano a ritmo variabile la stessa musica dava
     movimenti diversi, e la parte di rilascio non arrivava mai a zero.

  3. Il livello in ingresso è la posizione in dB riscalata fra 0 e 1: durante
     un brano sta quasi sempre fra 0,5 e 0,8 e si muove pochissimo. Usato così
     com'è tiene la mascella spalancata e ferma per tutta la canzone. Non è la
     musica che apre la bocca: è il fondo scala.

  Qui si guarda invece quanto il livello *supera il suo andamento medio*, cioè
  gli accenti: la bocca segue i colpi e si richiude fra uno e l'altro. Il ciclo
  gira su requestAnimationFrame, indipendente dal ritmo dei meter, e scrive in
  un MotionValue: nessun re-render di React a ogni fotogramma.
*/

// Sotto questa soglia (~ -58 dBFS con la scala usata dai meter) non c'è
// segnale: la bocca deve stare chiusa, non quasi chiusa.
const SILENZIO = 0.02

const costante = (dtMs, tauMs) => 1 - Math.exp(-dtMs / tauMs)

export function useAudioMouth (audioLevel, {
  attaccoMs = 40,     // quanto è pronta ad aprirsi
  rilascioMs = 190,   // quanto è morbida a richiudersi
} = {}) {
  const apertura = useMotionValue(0)

  // Il livello corrente viene letto dal ciclo di animazione, non lo insegue:
  // così il fotogramma è regolare anche se i meter arrivano a scatti.
  const livello = useRef(0)
  livello.current = Number.isFinite(audioLevel) ? Math.min(1, Math.max(0, audioLevel)) : 0

  useEffect(() => {
    let raf = 0
    let ultimo = performance.now()

    let media = 0        // andamento lento del livello
    let deviazione = 0   // di quanto oscilla di solito attorno alla media
    let valore = 0       // apertura attuale

    const passo = (ora) => {
      // Limite sul delta: tornando da una scheda in secondo piano il primo
      // fotogramma può valere secondi, e senza limite farebbe uno scatto.
      const dt = Math.min(64, Math.max(1, ora - ultimo))
      ultimo = ora

      const grezzo = livello.current

      // Riferimento lento: dove sta il pezzo in questo momento.
      media += (grezzo - media) * costante(dt, 700)

      const eccesso = grezzo - media
      deviazione += (Math.abs(eccesso) - deviazione) * costante(dt, 1500)

      // Normalizza sugli accenti tipici del brano, così un pezzo compresso e
      // uno dinamico muovono la bocca in modo comparabile. Il minimo evita che
      // in un passaggio quasi piatto il rumore venga amplificato a spalancata.
      const scala = Math.max(0.035, deviazione * 2.2)
      let obiettivo = grezzo < SILENZIO ? 0 : Math.min(1, Math.max(0, eccesso / scala))

      // Un filo di apertura di base quando la musica suona: a bocca del tutto
      // ferma fra un colpo e l'altro sembrerebbe spenta.
      if (grezzo >= SILENZIO) obiettivo = Math.min(1, obiettivo * 0.88 + 0.06)

      const tau = obiettivo > valore ? attaccoMs : rilascioMs
      valore += (obiettivo - valore) * costante(dt, tau)

      // Aggancio allo zero: senza questo il rilascio esponenziale lascia per
      // sempre un residuo e la bocca non si chiude mai del tutto.
      if (valore < 0.004 && obiettivo === 0) valore = 0

      apertura.set(valore)
      raf = requestAnimationFrame(passo)
    }

    raf = requestAnimationFrame(passo)
    return () => cancelAnimationFrame(raf)
  }, [apertura, attaccoMs, rilascioMs])

  return apertura
}
