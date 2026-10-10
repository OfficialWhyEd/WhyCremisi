import { whycremisi } from '../../whycremisi-bridge'
import { PROVIDERS, providerFor, needsKey } from './catalog'

// Tutto quello che l'onboarding dice al plugin passa da qui. Usa solo i
// messaggi che OscBridge.cpp gestisce davvero: config.set con ai.provider,
// ai.model, ai.apiKey, ai.authToken, ai.effort, ai.detectSubscription,
// ai.linkClaude, ai.openLogin, ai.getModels, ai.testConnection, e ai.prompt.

export const live = () => whycremisi.isConnected()

// Manda un config.set e aspetta il config.response con la stessa chiave.
export function configCall(key, extra = {}, timeoutMs = 25000) {
  return new Promise((resolve) => {
    if (!live()) { resolve({ offline: true }); return }
    let done = false
    let off = () => {}
    let timer = null
    const finish = (p) => {
      if (done) return
      done = true
      clearTimeout(timer)
      off()
      resolve(p)
    }
    off = whycremisi.on('config.response', (p) => { if (p?.key === key) finish(p) })
    timer = setTimeout(() => finish({ timeout: true }), timeoutMs)
    whycremisi.send({ type: 'config.set', payload: { key, ...extra } })
  })
}

export function setConfig(key, value, extra = {}) {
  if (live()) whycremisi.send({ type: 'config.set', payload: { key, value, ...extra } })
}

// Porta il plugin nella configurazione scelta. Chiave e abbonamento sono
// alternativi: ne mandiamo uno solo, e il token solo se incollato a mano
// (quello trovato sulla macchina il plugin ce l'ha gia').
export function applyConfig(form) {
  const provider = providerFor(form)
  setConfig('ai.provider', provider)
  setConfig('ai.model', form.model)
  if (provider === 'anthropic') setConfig('ai.effort', form.effort)
  if (form.path === 'subscription') {
    if (form.token) setConfig('ai.authToken', form.token)
  } else if (needsKey(form) && form.apiKey) {
    setConfig('ai.apiKey', form.apiKey, { provider })
  }
}

export function openKeyPage(providerId) {
  if (live()) {
    whycremisi.send({ type: 'config.set', payload: { key: 'ai.openLogin', provider: providerId } })
  } else {
    const url = PROVIDERS[providerId]?.keyUrl
    if (url) window.open('https://' + url, '_blank', 'noopener')
  }
}

// Messaggio di prova vero: passa dal modello e torna con una risposta.
// Il prefisso "onboarding-" serve ad App.jsx per non mostrarlo in chat.
export function promptTest(text, timeoutMs = 60000) {
  return new Promise((resolve) => {
    if (!live()) { resolve({ offline: true }); return }
    const id = 'onboarding-' + Date.now()
    const started = performance.now()
    let done = false
    let off = () => {}
    let timer = null
    const finish = (r) => {
      if (done) return
      done = true
      clearTimeout(timer)
      off()
      resolve({ ...r, ms: Math.round(performance.now() - started) })
    }
    off = whycremisi.on('ai.response', (p, m) => {
      if (m?.id !== id) return
      if (p?.status === 'thinking' || p?.status === 'pending') return
      finish({ ok: p?.status === 'success' && !!p?.content, content: p?.content || '' })
    })
    timer = setTimeout(() => finish({ timeout: true }), timeoutMs)
    whycremisi.sendMessage('ai.prompt', { prompt: text, stream: false }, { id })
  })
}

// Dal testo dell'errore a "cosa fare". Restituisce una chiave di traduzione.
export function classifyError(text = '') {
  const t = text.toLowerCase()
  if (/401|403|unauthor|invalid|rifiut|credenz|api.?key|x-api-key|permission/.test(t)) return 'key'
  if (/429|quota|rate|limit|credit|billing|insufficient/.test(t)) return 'quota'
  if (/ollama/.test(t)) return 'ollama'
  if (/404|not found|model/.test(t)) return 'model'
  if (/nessuna risposta|timeout|rete|connessione|network|resolve|econn/.test(t)) return 'network'
  return 'generic'
}
