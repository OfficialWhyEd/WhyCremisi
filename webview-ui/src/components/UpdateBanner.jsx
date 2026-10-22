import React, { useEffect, useRef, useState } from 'react'
import { whycremisi, ConnectionState } from '../whycremisi-bridge'
import { configCall } from './onboarding/bridge'

// Aggiornamenti di WhyCremisi. All'apertura chiede al plugin se c'è una versione
// nuova (solo se il flag "automatici" è acceso) e lo dice. Niente si installa da solo
// mentre lavori: scarica, controlla la firma SHA-256 e installa quando chiudi la DAW.
// Il piccolo "v0.2.0" in basso a destra apre il pannello: interruttore e controllo manuale.

const TXT = {
  it: {
    nuova: (v) => `Nuova versione ${v}`,
    sub: 'Si installa quando chiudi la DAW. Il lavoro aperto non viene toccato.',
    aggiorna: 'Aggiorna alla chiusura',
    dopo: 'Più tardi',
    scarico: (p) => `Scarico… ${p}%`,
    pronto: 'Pronto. Si installa quando chiudi la DAW.',
    versione: 'Versione',
    auto: 'Aggiornamenti automatici',
    autoSub: 'All’apertura controlla se c’è una versione nuova e te lo dice.',
    controlla: 'Controlla ora',
    controllo: 'Controllo…',
    aggiornato: 'Hai già l’ultima versione.',
    nessunaPubblicata: 'Per ora non c’è nessun aggiornamento pubblicato.',
    chiudi: 'Chiudi',
    note: 'Novità',
  },
  en: {
    nuova: (v) => `New version ${v}`,
    sub: 'It installs when you close the DAW. Your open work is not touched.',
    aggiorna: 'Update on close',
    dopo: 'Later',
    scarico: (p) => `Downloading… ${p}%`,
    pronto: 'Ready. It installs when you close the DAW.',
    versione: 'Version',
    auto: 'Automatic updates',
    autoSub: 'On start it checks for a new version and tells you.',
    controlla: 'Check now',
    controllo: 'Checking…',
    aggiornato: 'You already have the latest version.',
    nessunaPubblicata: 'No update has been published yet.',
    chiudi: 'Close',
    note: 'What’s new',
  },
}

const card = {
  position: 'fixed', right: 16, bottom: 44, zIndex: 50, width: 'min(340px, calc(100vw - 32px))',
  background: 'var(--surface-2, #181516)', border: '1px solid var(--line-2, #3a3437)', borderRadius: 12,
  padding: 14, color: 'var(--ink-1, #f3eff0)', fontFamily: 'var(--font-ui, system-ui)', fontSize: 13,
  boxShadow: '0 8px 30px rgba(0,0,0,.45)',
}
const bottone = (primario) => ({
  border: primario ? 'none' : '1px solid var(--line-2, #3a3437)', borderRadius: 8, padding: '8px 12px', cursor: 'pointer',
  background: primario ? 'var(--crimson-600, #c8102e)' : 'transparent', color: primario ? '#fff' : 'var(--ink-2, #c4bcbf)',
  font: 'inherit', fontWeight: 600,
})

export function UpdateBanner() {
  const [lang] = useState(() => {
    try { return localStorage.getItem('whycremisi_lang') || ((navigator.language || 'it').startsWith('en') ? 'en' : 'it') } catch { return 'it' }
  })
  const t = TXT[lang] || TXT.it
  const [stato, setStato] = useState({ version: '', auto: true })
  const [nuova, setNuova] = useState(null)        // { latest, notes }
  const [fase, setFase] = useState('idle')        // idle | scarico | pronto
  const [percento, setPercento] = useState(0)
  const [aperto, setAperto] = useState(false)     // pannello impostazioni
  const [messaggio, setMessaggio] = useState('')
  const [controllando, setControllando] = useState(false)
  const [nascosto, setNascosto] = useState(false)
  const fatto = useRef(false)

  const controlla = async (manuale) => {
    setControllando(true)
    if (manuale) setMessaggio('')
    const p = await configCall('update.check', { lang }, 40000)
    setControllando(false)
    if (p?.available) { setNuova({ latest: p.latest, notes: p.notes || '' }); setNascosto(false); return }
    if (manuale) setMessaggio(p?.status === 'error' ? (p.message || t.nessunaPubblicata) : t.aggiornato)
  }

  useEffect(() => {
    const avvia = async () => {
      if (fatto.current) return
      fatto.current = true
      const s = await configCall('update.status', {})
      if (!s || s.offline || s.timeout) { fatto.current = false; return }
      setStato({ version: s.version, auto: !!s.auto })
      if (s.auto) controlla(false)
    }
    if (whycremisi.isConnected()) avvia()
    const off = whycremisi.onStateChange((s) => { if (s === ConnectionState.CONNECTED) avvia() })
    const offProg = whycremisi.on('config.response', (p) => {
      if (p?.key === 'update.progress') setPercento(p.percent || 0)
    })
    return () => { off?.(); offProg?.() }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [])

  const aggiorna = async () => {
    setFase('scarico'); setPercento(0)
    const p = await configCall('update.apply', { lang }, 600000)
    if (p?.installOnClose) setFase('pronto')
    else { setFase('idle'); setMessaggio(p?.message || ''); setAperto(true) }
  }

  const cambiaAuto = async () => {
    const p = await configCall('update.auto', { value: !stato.auto })
    if (p?.status === 'ok') setStato((s) => ({ ...s, auto: !!p.auto }))
  }

  if (!stato.version && !nuova) return null

  return (
    <>
      {nuova && !nascosto && (
        <div role="status" style={card}>
          <div style={{ fontWeight: 700, fontSize: 15 }}>{t.nuova(nuova.latest)}</div>
          {fase === 'idle' && <div style={{ color: 'var(--ink-2, #c4bcbf)', margin: '6px 0 12px' }}>{t.sub}</div>}
          {fase === 'scarico' && (
            <div style={{ margin: '10px 0' }}>
              <div style={{ color: 'var(--ink-2, #c4bcbf)', marginBottom: 6 }}>{t.scarico(percento)}</div>
              <div style={{ height: 4, background: 'var(--line-1, #2a2527)', borderRadius: 2 }}>
                <div style={{ height: 4, width: `${percento}%`, background: 'var(--crimson-500, #dc143c)', borderRadius: 2, transition: 'width .2s' }} />
              </div>
            </div>
          )}
          {fase === 'pronto' && <div style={{ color: 'var(--state-ok, #4fd69c)', margin: '6px 0 12px' }}>✓ {t.pronto}</div>}
          {fase === 'idle' && (
            <div style={{ display: 'flex', gap: 8 }}>
              <button style={bottone(true)} onClick={aggiorna}>{t.aggiorna}</button>
              <button style={bottone(false)} onClick={() => setNascosto(true)}>{t.dopo}</button>
            </div>
          )}
          {fase === 'pronto' && <button style={bottone(false)} onClick={() => setNascosto(true)}>{t.chiudi}</button>}
        </div>
      )}

      {aperto && (
        <div style={{ ...card, bottom: nuova && !nascosto ? 190 : 44 }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 10 }}>
            <strong>{t.versione} {stato.version}</strong>
            <button style={{ ...bottone(false), padding: '2px 8px' }} onClick={() => setAperto(false)} aria-label={t.chiudi}>×</button>
          </div>
          <label style={{ display: 'flex', gap: 10, alignItems: 'flex-start', cursor: 'pointer' }}>
            <input type="checkbox" checked={stato.auto} onChange={cambiaAuto} style={{ marginTop: 3 }} />
            <span>
              <span style={{ display: 'block', fontWeight: 600 }}>{t.auto}</span>
              <span style={{ color: 'var(--ink-3, #9a9194)' }}>{t.autoSub}</span>
            </span>
          </label>
          <div style={{ display: 'flex', gap: 8, alignItems: 'center', marginTop: 12 }}>
            <button style={bottone(false)} disabled={controllando} onClick={() => controlla(true)}>
              {controllando ? t.controllo : t.controlla}
            </button>
          </div>
          {messaggio && <div style={{ marginTop: 10, color: 'var(--ink-2, #c4bcbf)' }}>{messaggio}</div>}
          {nuova?.notes && <div style={{ marginTop: 10, color: 'var(--ink-3, #9a9194)' }}><strong>{t.note}:</strong> {nuova.notes}</div>}
        </div>
      )}

      <button
        onClick={() => setAperto((a) => !a)}
        title={t.versione}
        style={{
          position: 'fixed', right: 16, bottom: 12, zIndex: 50, background: 'transparent', border: 'none',
          color: nuova ? 'var(--crimson-400, #ff6b85)' : 'var(--ink-3, #9a9194)', cursor: 'pointer',
          fontFamily: 'var(--font-mono, monospace)', fontSize: 12, padding: 4,
        }}
      >
        v{stato.version}{nuova ? ' •' : ''}
      </button>
    </>
  )
}

export default UpdateBanner
