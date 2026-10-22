import React, { useState } from 'react'
import { configCall } from './onboarding/bridge'
import { whycremisi } from '../whycremisi-bridge'

// Accesso a WhyCremisi: Google, oppure email e password. Prima schermata per
// chi non è ancora entrato. Il lavoro vero lo fa il plugin (account.*): qui si
// mostrano solo i passi e i motivi degli errori, in italiano o in inglese.

const TXT = {
  it: {
    titolo: 'Entra in WhyCremisi',
    sotto: 'Un account solo, su ogni DAW. Ti serve per ritrovare le tue sessioni e il tuo abbonamento.',
    google: 'Continua con Google',
    oppure: 'oppure con la tua email',
    email: 'Email',
    password: 'Password (almeno 8 caratteri)',
    entra: 'Accedi',
    crea: 'Crea account',
    haiAccount: 'Hai già un account? Accedi',
    nuovo: 'Prima volta? Crea un account',
    dimenticata: 'Password dimenticata',
    piuTardi: 'Più tardi',
    attesaGoogle: 'Si è aperta la pagina di Google nel browser. Completa lì l’accesso e torna qui.',
    attesa: 'Un attimo…',
    timeout: 'Il plugin non ha risposto. Riprova tra poco.',
    offline: 'Il plugin non è collegato alla finestra. Riapri il plugin e riprova.',
  },
  en: {
    titolo: 'Sign in to WhyCremisi',
    sotto: 'One account, on every DAW. It keeps your sessions and your subscription.',
    google: 'Continue with Google',
    oppure: 'or with your email',
    email: 'Email',
    password: 'Password (at least 8 characters)',
    entra: 'Sign in',
    crea: 'Create account',
    haiAccount: 'Already have an account? Sign in',
    nuovo: 'First time? Create an account',
    dimenticata: 'Forgot password',
    piuTardi: 'Later',
    attesaGoogle: 'The Google page opened in your browser. Finish signing in there, then come back.',
    attesa: 'One moment…',
    timeout: 'The plugin did not answer. Try again shortly.',
    offline: 'The plugin is not connected to this window. Reopen the plugin and try again.',
  },
}

const campo = {
  width: '100%', padding: '10px 12px', fontSize: 13, color: 'var(--text-primary)',
  background: 'var(--bg-elevated)', border: '1px solid var(--border-light)', outline: 'none',
}
const bottone = (primario) => ({
  width: '100%', padding: '11px 12px', fontSize: 13, fontWeight: 700, cursor: 'pointer',
  color: primario ? '#000' : 'var(--text-primary)',
  background: primario ? 'var(--amber)' : 'transparent',
  border: '1px solid ' + (primario ? 'var(--amber)' : 'var(--border-light)'),
})
const link = { background: 'none', border: 'none', color: 'var(--text-secondary)', fontSize: 12, cursor: 'pointer', textDecoration: 'underline' }

export function AccountGate ({ onDone, onSkip }) {
  const [lang, setLang] = useState(() => {
    try { return localStorage.getItem('whycremisi_lang') || ((navigator.language || 'it').startsWith('en') ? 'en' : 'it') } catch { return 'it' }
  })
  const [nuovo, setNuovo] = useState(false)
  const [email, setEmail] = useState('')
  const [password, setPassword] = useState('')
  const [msg, setMsg] = useState('')
  const [errore, setErrore] = useState(false)
  const [occupato, setOccupato] = useState(false)
  const t = TXT[lang]

  const cambiaLingua = (l) => { setLang(l); try { localStorage.setItem('whycremisi_lang', l) } catch {} }

  const esegui = async (key, extra, testoAttesa) => {
    setOccupato(true); setErrore(false); setMsg(testoAttesa || t.attesa)
    const p = await configCall(key, { lang, ...extra }, key === 'account.google' ? 200000 : 30000)
    // il plugin risponde subito "working" e poi con l'esito vero
    let r = p
    if (p?.status === 'working') {
      r = await new Promise((resolve) => {
        const off = whycremisi.on('config.response', (x) => {
          if (x?.key === key && x.status !== 'working') { clearTimeout(timer); off(); resolve(x) }
        })
        const timer = setTimeout(() => { off(); resolve({ timeout: true }) }, key === 'account.google' ? 200000 : 30000)
      })
    }
    setOccupato(false)
    if (r?.offline) { setErrore(true); setMsg(t.offline); return }
    if (r?.timeout) { setErrore(true); setMsg(t.timeout); return }
    if (r?.status === 'ok' && r.loggedIn) { onDone && onDone(r.email); return }
    if (r?.status === 'ok') { setErrore(false); setMsg(r.message || ''); return }
    setErrore(true); setMsg(r?.message || t.timeout)
  }

  return (
    <div style={{ position: 'fixed', inset: 0, zIndex: 60, display: 'flex', alignItems: 'center', justifyContent: 'center', background: 'var(--bg-base, #0b0b0b)', padding: 16 }}>
      <div style={{ width: '100%', maxWidth: 360, display: 'flex', flexDirection: 'column', gap: 12 }}>
        <div style={{ display: 'flex', justifyContent: 'flex-end', gap: 8, fontSize: 11 }}>
          {['it', 'en'].map(l => (
            <button key={l} onClick={() => cambiaLingua(l)}
              style={{ ...link, textDecoration: 'none', color: lang === l ? 'var(--amber)' : 'var(--text-tertiary)', fontWeight: lang === l ? 700 : 400 }}>
              {l.toUpperCase()}
            </button>
          ))}
        </div>
        <h1 style={{ fontSize: 20, fontWeight: 700, color: 'var(--text-primary)', margin: 0 }}>{t.titolo}</h1>
        <p style={{ fontSize: 12, lineHeight: 1.5, color: 'var(--text-secondary)', margin: 0 }}>{t.sotto}</p>

        <button disabled={occupato} style={bottone(true)} onClick={() => esegui('account.google', {}, t.attesaGoogle)}>{t.google}</button>
        <div style={{ textAlign: 'center', fontSize: 11, color: 'var(--text-tertiary)' }}>{t.oppure}</div>

        <input style={campo} type="email" autoComplete="email" placeholder={t.email} value={email} onChange={e => setEmail(e.target.value)} />
        <input style={campo} type="password" autoComplete={nuovo ? 'new-password' : 'current-password'} placeholder={t.password}
               value={password} onChange={e => setPassword(e.target.value)}
               onKeyDown={e => { if (e.key === 'Enter' && !occupato) esegui(nuovo ? 'account.signUp' : 'account.signIn', { email, password }) }} />
        <button disabled={occupato} style={bottone(false)} onClick={() => esegui(nuovo ? 'account.signUp' : 'account.signIn', { email, password })}>
          {nuovo ? t.crea : t.entra}
        </button>

        {msg && <div role="status" style={{ fontSize: 12, lineHeight: 1.5, color: errore ? '#ff6b6b' : 'var(--text-secondary)' }}>{msg}</div>}

        <div style={{ display: 'flex', justifyContent: 'space-between', flexWrap: 'wrap', gap: 8 }}>
          <button style={link} onClick={() => { setNuovo(!nuovo); setMsg('') }}>{nuovo ? t.haiAccount : t.nuovo}</button>
          {!nuovo && <button style={link} disabled={occupato} onClick={() => esegui('account.reset', { email })}>{t.dimenticata}</button>}
        </div>
        <button style={{ ...link, alignSelf: 'center', marginTop: 4 }} onClick={onSkip}>{t.piuTardi}</button>
      </div>
    </div>
  )
}
