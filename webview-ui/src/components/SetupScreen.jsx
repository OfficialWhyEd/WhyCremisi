import React, { useState, useEffect, useRef } from 'react'
import { motion, AnimatePresence } from 'framer-motion'
import { whycremisi } from '../whycremisi-bridge'
import { TESTI, guessLang, saveLang } from './onboarding/setupTesti'

const modelsByProvider = {
  ollama:    ['llama3.2', 'llama3.1', 'mistral', 'mixtral', 'codellama', 'qwen2.5'],
  gemini:    ['gemini-3.1-flash-lite', 'gemini-3.1-flash-lite-preview', 'gemini-2.0-flash', 'gemini-1.5-pro'],
  openai:    ['gpt-4o', 'gpt-4o-mini', 'gpt-4-turbo', 'gpt-3.5-turbo'],
  anthropic: ['claude-opus-5-5', 'claude-sonnet-5-5', 'claude-haiku-5-5'],
  // Con l'account Claude il programma ufficiale accetta gli id pieni (provato).
  claudecode: ['claude-sonnet-5-5', 'claude-opus-5-5', 'claude-haiku-5-5', 'claude-fable-5-1'],
  // Con l'account ChatGPT il modello predefinito è quello dell'abbonamento.
  codex: ['default', 'gpt-5.6-luna', 'gpt-5.6-terra', 'gpt-6-luna'],
  openrouter: [
    'deepseek/deepseek-v4.1-flash',
    'nvidia/nemotron-3-ultra-550b-a55b:free',
    'nvidia/nemotron-3-super-120b-a12b:free',
    'google/gemma-4-31b-it:free',
    'minimax/minimax-m3.1-flash:free',
    'openrouter/free',
  ],
}

// I modelli con questo suffisso non si pagano.
const isFreeModel = (m) => m.endsWith(':free') || m === 'openrouter/free'

// Su OpenRouter gli id sono lunghi: nella UI mostriamo solo il nome.
const shortModelName = (m) => m.replace(':free', '').split('/').pop()

// Nomi dei modelli: nella UI si sceglie "Opus 5.5", non "claude-opus-5-5".
// Le note (equilibrato, rapido…) stanno nei testi, per lingua.
const modelNames = {
  'claude-opus-5-5': 'Opus 5.5',
  'claude-sonnet-5-5': 'Sonnet 5.5',
  'claude-haiku-5-5': 'Haiku 5.5',
  'claude-fable-5-1': 'Fable 5.1',
  sonnet: 'Sonnet',
  opus: 'Opus',
  haiku: 'Haiku',
}

// Profondità di ragionamento. Alza la qualità sui compiti difficili e
// il numero di token spesi.
const effortLevels = [
  { id: 'low',    label: 'Low' },
  { id: 'medium', label: 'Medium' },
  { id: 'high',   label: 'High' },
  { id: 'xhigh',  label: 'X-High' },
  { id: 'max',    label: 'Max' },
]

// Nome mostrato per ogni servizio. Per Ollama dipende dalla lingua.
const providerNames = {
  gemini: 'GEMINI',
  openai: 'ChatGPT',
  anthropic: 'CLAUDE',
  openrouter: 'OPENROUTER',
}

export function SetupScreen({ onComplete, onSkip, initialConfig = {} }) {
  // "claudecode" è come il plugin chiama l'accesso con l'account Claude; qui
  // sta sotto la card Claude, con la modalità "Abbonamento".
  const [lang, setLang] = useState(() => guessLang().lang)
  const t = TESTI[lang]
  const cambiaLingua = (l) => { setLang(l); saveLang(l) }
  const nomeProvider = (id) => (id === 'ollama' ? t.nameOllama : providerNames[id])
  const savedClaudeCode = initialConfig.provider === 'claudecode'
  const savedCodex = initialConfig.provider === 'codex'
  const [provider, setProvider] = useState(
    savedClaudeCode ? 'anthropic' : savedCodex ? 'openai' : (initialConfig.provider || 'anthropic'))
  const [model, setModel] = useState(
    initialConfig.model
    || modelsByProvider[savedClaudeCode ? 'claudecode' : savedCodex ? 'codex' : initialConfig.provider]?.[0]
    || 'llama3.2')
  const [apiKey, setApiKey] = useState(initialConfig.apiKey || '')
  const [status, setStatus] = useState('idle')
  const [errorMsg, setErrorMsg] = useState('')
  const [showKey, setShowKey] = useState(false)
  // Su Claude si può entrare con un abbonamento invece che con una chiave API.
  const [claudeAuthMode, setClaudeAuthMode] = useState(
    'subscription')
  const [effort, setEffort] = useState(initialConfig.effort || 'high')
  // null = non ancora verificato, true/false = esito del rilevamento.
  const [subscriptionDetected, setSubscriptionDetected] = useState(null)
  // Modelli riportati dal fornitore, quando li sa dire lui.
  const [modelliVivi, setModelliVivi] = useState(null)
  // Il programma "claude" è installato su questa macchina?
  const [claudeCodePresente, setClaudeCodePresente] = useState(null)
  const [collegamento, setCollegamento] = useState(null) // null | 'corso' | 'fatto' | messaggio d'errore
  const unsubRef = useRef(null)
  const timeoutsRef = useRef([])
  const pollRef = useRef(null)

  const isClaude = provider === 'anthropic'
  const isChatGpt = provider === 'openai'
  // Claude e ChatGPT: si entra con il proprio account tramite il programma ufficiale.
  const hasAccount = isClaude || isChatGpt
  const usingSubscription = hasAccount && claudeAuthMode === 'subscription'
  const nomeAccount = isChatGpt ? 'ChatGPT' : 'Claude'
  const chiaveDetect = isChatGpt ? 'ai.detectCodex' : 'ai.detectSubscription'
  const chiaveLink = isChatGpt ? 'ai.linkCodex' : 'ai.linkClaude'
  // Il fornitore che il plugin deve usare davvero.
  const effectiveProvider = usingSubscription ? (isChatGpt ? 'codex' : 'claudecode') : provider
  const modelList = (provider === 'gemini' && modelliVivi) || modelsByProvider[effectiveProvider] || []
  // Il campo credenziale non serve quando si usa Ollama in locale né quando
  // si entra con l'abbonamento Claude.
  // OpenRouter: l'accesso con un clic ha già salvato la chiave nel plugin.
  const [orCollegato, setOrCollegato] = useState(false)
  const [orInCorso, setOrInCorso] = useState(false)
  const needsApiKey = provider !== 'ollama' && !usingSubscription && !(provider === 'openrouter' && orCollegato)

  useEffect(() => {
    return () => {
      if (unsubRef.current) unsubRef.current()
      timeoutsRef.current.forEach(clearTimeout)
      if (pollRef.current) clearInterval(pollRef.current)
    }
  }, [])

  // Il modello deve appartenere al fornitore scelto: passando da chiave API
  // ad abbonamento (o viceversa) l'elenco cambia nome.
  useEffect(() => {
    const lista = modelsByProvider[effectiveProvider]
    if (lista && !lista.includes(model) && !(provider === 'gemini' && modelliVivi)) setModel(lista[0])
  }, [effectiveProvider])

  // Su Gemini l'elenco dei modelli lo dà Google, non una lista scritta qui:
  // le liste fisse invecchiano e chi sceglie un modello ritirato riceve un
  // errore senza capire perché. Si chiede appena c'è una chiave.
  useEffect(() => {
    const unsub = whycremisi.on('config.response', (payload) => {
      if (payload?.key !== 'ai.linkOpenRouter') return
      if (payload.status === 'ok') { setOrCollegato(true); setOrInCorso(false); setErrorMsg('') }
      else if (payload.status === 'error') { setOrInCorso(false); setErrorMsg(payload.message || t.loginFail) }
    })
    return () => unsub()
  }, [])

  useEffect(() => {
    if (provider !== 'gemini' || !apiKey || apiKey.length < 20) return
    if (!whycremisi.isConnected()) return
    const unsub = whycremisi.on('config.response', (payload) => {
      if (payload?.key !== 'ai.getModels') return
      if (Array.isArray(payload.models) && payload.models.length) {
        setModelliVivi(payload.models)
        if (!payload.models.includes(model)) setModel(payload.models[0])
      }
    })
    // Il motore va messo al corrente della chiave appena digitata, altrimenti
    // interrogherebbe Google senza credenziali e non riceverebbe nulla.
    // Il campo "provider" va incluso: senza, il plugin scartava il messaggio
    // in silenzio e restava senza chiave, quindi interrogava Google senza
    // credenziali e l'elenco dei modelli non arrivava mai.
    whycremisi.send({ type: 'config.set', payload: { key: 'ai.provider', value: 'gemini' } })
    whycremisi.send({ type: 'config.set', payload: { key: 'ai.apiKey', value: apiKey, provider: 'gemini' } })
    const attesa = setTimeout(() => {
      whycremisi.send({ type: 'config.set', payload: { key: 'ai.getModels' } })
    }, 300)
    return () => { clearTimeout(attesa); unsub() }
  }, [provider, apiKey])

  // Quando si sceglie Claude, chiediamo al plugin se sulla macchina c'è già
  // un abbonamento utilizzabile: se sì non serve chiedere nulla all'utente.
  useEffect(() => {
    if (!hasAccount || !whycremisi.isConnected()) return
    setSubscriptionDetected(null); setClaudeCodePresente(null); setCollegamento(null)
    const unsub = whycremisi.on('config.response', (payload) => {
      if (payload?.key !== chiaveDetect) return
      setSubscriptionDetected(!!payload.found)
      setClaudeCodePresente(!!(isChatGpt ? payload.codex : payload.claudeCode))
      if (payload.found) {
        // Accesso completato nel browser: si smette di controllare.
        if (pollRef.current) { clearInterval(pollRef.current); pollRef.current = null }
        setCollegamento((c) => (c === 'corso' ? 'fatto' : c))
      }
    })
    const unsubLink = whycremisi.on('config.response', (payload) => {
      if (payload?.key !== chiaveLink) return
      if (payload.status === 'opened') {
        // Il login lo fa il programma ufficiale in una finestra a parte:
        // qui si resta a guardare quando risulta collegato.
        setCollegamento('corso')
        if (pollRef.current) clearInterval(pollRef.current)
        const inizio = Date.now()
        pollRef.current = setInterval(() => {
          if (Date.now() - inizio > 5 * 60 * 1000) {
            clearInterval(pollRef.current); pollRef.current = null
            setCollegamento(t.linkFail)
            return
          }
          whycremisi.send({ type: 'config.set', payload: { key: chiaveDetect } })
        }, 2500)
      } else {
        // Il motivo va detto: un pulsante che torna com'era non spiega niente.
        setCollegamento(payload.message || t.linkFail2)
      }
    })
    whycremisi.send({ type: 'config.set', payload: { key: chiaveDetect } })
    return () => {
      unsub(); unsubLink()
      if (pollRef.current) { clearInterval(pollRef.current); pollRef.current = null }
    }
  }, [provider])

  const providers = [
    { id: 'anthropic', icon: 'psychology',   desc: t.descClaude },
    { id: 'openai',    icon: 'auto_awesome', desc: t.descChat },
    { id: 'gemini',    icon: 'cloud_queue',  desc: t.descGemini },
    { id: 'openrouter', icon: 'hub',         desc: t.descOpenrouter },
    { id: 'ollama',    icon: 'memory',       desc: t.descOllama },
  ]

  const handleTestConnection = async () => {
    setStatus('testing')
    setErrorMsg('')

    if (whycremisi.isConnected()) {
      whycremisi.send({ type: 'config.set', payload: { key: 'ai.provider', value: effectiveProvider } })
      whycremisi.send({ type: 'config.set', payload: { key: 'ai.model', value: model } })
      if (isClaude || usingSubscription)
        whycremisi.send({ type: 'config.set', payload: { key: 'ai.effort', value: effort } })
      // Con l'account Claude non c'è nessuna credenziale da passare: l'accesso
      // sta nel programma ufficiale, il plugin non vede né conserva token.
      if (!usingSubscription && apiKey)
        whycremisi.send({ type: 'config.set', payload: { key: 'ai.apiKey', value: apiKey, provider } })
    }

    if (whycremisi.isConnected()) {
      // Verificare una credenziale è una chiamata di rete vera: 8 secondi
      // scadevano prima che il fornitore rispondesse, e l'accesso risultava
      // fallito quando invece stava andando a buon fine.
      const timeout = setTimeout(() => {
        setStatus('error')
        setErrorMsg(t.noReply)
      }, 20000)
      timeoutsRef.current.push(timeout)

      const unsub = whycremisi.on('config.response', (payload) => {
        if (payload?.key !== 'ai.testConnection') return
        clearTimeout(timeout); unsub()
        unsubRef.current = null
        if (payload.connected) {
          setStatus('success')
          const t = setTimeout(() => onComplete({ provider: effectiveProvider, model, apiKey: usingSubscription ? '' : apiKey, effort, claudeAuthMode }), 1200)
          timeoutsRef.current.push(t)
        } else {
          setStatus('error')
          // Il motivo vero arriva dal plugin: chiave rifiutata, quota finita,
          // servizio spento. Prima usciva sempre "Connection failed." e non
          // c'era modo di capire cosa correggere.
          setErrorMsg(payload.error || t.cantTalk(nomeAccount))
        }
      })
      unsubRef.current = unsub
      whycremisi.send({ type: 'config.set', payload: { key: 'ai.testConnection' } })
    } else {
      const t = setTimeout(() => {
        const hasCredential = provider === 'ollama'
          || (usingSubscription && subscriptionDetected)
          || apiKey.length > 10
        if (hasCredential) {
          setStatus('success')
          const t2 = setTimeout(() => onComplete({ provider: effectiveProvider, model, apiKey: usingSubscription ? '' : apiKey, effort, claudeAuthMode }), 1200)
          timeoutsRef.current.push(t2)
        } else {
          setStatus('error')
          setErrorMsg(t.noPlugin)
        }
      }, 1000)
      timeoutsRef.current.push(t)
    }
  }

  const Etichetta = ({ n, children }) => (
    <label className="flex items-center gap-2 text-[11px] font-bold text-white tracking-tight mb-1.5">
      <span className="w-4 h-4 rounded-full bg-[#DC143C] text-white text-[9px] flex items-center justify-center flex-shrink-0">{n}</span>
      {children}
    </label>
  )

  const nomeModello = (m) => modelNames[m] || (provider === 'openrouter' ? shortModelName(m) : m)
  const notaModello = (m) => t.models[m] || ''

  return (
    <motion.div
      initial={{ opacity: 0 }}
      animate={{ opacity: 1 }}
      exit={{ opacity: 0 }}
      className="fixed inset-0 z-[90] flex items-center justify-center pointer-events-auto"
    >
      <motion.div
        initial={{ opacity: 0 }}
        animate={{ opacity: 1 }}
        exit={{ opacity: 0 }}
        className="absolute inset-0 bg-black/60 backdrop-blur-sm"
      />

      <motion.div
        initial={{ opacity: 0, y: -30, scale: 0.97, filter: 'blur(4px)' }}
        animate={{ opacity: 1, y: 0, scale: 1, filter: 'blur(0)' }}
        exit={{ opacity: 0, y: -20, scale: 0.97, filter: 'blur(4px)' }}
        transition={{ type: 'spring', damping: 25, stiffness: 300 }}
        className="relative w-full max-w-lg max-h-[96vh] overflow-y-auto bg-[#131313]/95 backdrop-blur-xl border border-[#222222] shadow-2xl"
      >
        <div className="absolute top-0 left-0 w-full h-[2px] bg-gradient-to-r from-[#DC143C] via-[#FFB000] to-[#00E5FF]" />

        <div className="px-5 py-4">
          <header className="flex items-start justify-between mb-4 gap-3">
            <div className="flex items-center gap-3">
              <img src="/whycremisi-mask.png" className="w-8 h-8 flex-shrink-0" alt="WhyCremisi" />
              <div>
                <h2 className="text-base font-black tracking-tight text-white leading-tight">{t.title}</h2>
                <p className="text-[11px] text-[#8a8a8a] leading-tight">{t.subtitle}</p>
              </div>
            </div>
            <div className="flex items-center gap-2 flex-shrink-0">
              <div className="flex border border-[#222] rounded-full overflow-hidden" role="group" aria-label="Lingua / Language">
                {['it', 'en'].map(l => (
                  <button
                    key={l}
                    onClick={() => cambiaLingua(l)}
                    aria-pressed={lang === l}
                    className={`px-2 py-0.5 text-[10px] font-bold uppercase transition-colors ${
                      lang === l ? 'bg-white text-black' : 'text-[#777] hover:text-white'
                    }`}
                  >{l}</button>
                ))}
              </div>
              {onSkip && (
                <button
                  onClick={onSkip}
                  className="text-[10px] text-[#777] hover:text-white underline decoration-dotted underline-offset-2 transition-colors"
                >
                  {t.skip}
                </button>
              )}
            </div>
          </header>

          <Etichetta n="1">{t.step1}</Etichetta>
          {/* Due colonne, con l'ultima card a piena larghezza quando i
              provider sono in numero dispari: così nessuna resta orfana. */}
          <div className="grid grid-cols-2 gap-1.5 mb-4 items-stretch">
            {providers.map((p, idx) => {
              const isSelected = provider === p.id
              const isLastOdd = idx === providers.length - 1 && providers.length % 2 === 1
              return (
                <motion.button
                  key={p.id}
                  whileHover={{ scale: 1.02 }}
                  whileTap={{ scale: 0.98 }}
                  style={isLastOdd ? { gridColumn: '1 / -1' } : undefined}
                  onClick={() => {
                    setProvider(p.id)
                    setModel(modelsByProvider[claudeAuthMode === 'subscription' && p.id === 'anthropic' ? 'claudecode' : claudeAuthMode === 'subscription' && p.id === 'openai' ? 'codex' : p.id][0])
                  }}
                  className={`px-2.5 py-2 border text-left transition-all flex items-center gap-2 min-h-[3rem] ${
                    isSelected
                      ? 'bg-[#1a1a1a] border-[#DC143C]'
                      : 'bg-[#0d0d0d] border-[#1a1a1a] opacity-60 hover:opacity-100'
                  }`}
                >
                  <span className="material-symbols-outlined text-base flex-shrink-0" style={{ color: isSelected ? '#DC143C' : '#666' }}>{p.icon}</span>
                  <span className="min-w-0">
                    <span className="block text-[11px] font-bold text-white tracking-tight leading-tight">{nomeProvider(p.id)}</span>
                    <span className="block text-[10px] text-[#8a8a8a] leading-tight">{p.desc}</span>
                  </span>
                </motion.button>
              )
            })}
          </div>

          <AnimatePresence mode="wait">
            <motion.div
              key={provider}
              initial={{ opacity: 0, height: 0 }}
              animate={{ opacity: 1, height: 'auto' }}
              exit={{ opacity: 0, height: 0 }}
              className="overflow-hidden"
            >
              {provider !== 'ollama' && (
                <div className="mb-4">
                  <Etichetta n="2">{t.step2}</Etichetta>

                  {hasAccount && (
                    <div className="grid grid-cols-2 gap-1.5">
                      {[
                        { id: 'subscription', label: t.accessMine, note: isChatGpt ? t.noteChatAcc : t.noteClaudeAcc },
                        { id: 'apikey',       label: t.accessKey,  note: t.noteKey },
                      ].map(mode => {
                        const active = claudeAuthMode === mode.id
                        return (
                          <button
                            key={mode.id}
                            onClick={() => setClaudeAuthMode(mode.id)}
                            className={`px-2.5 py-2 border text-left transition-all ${
                              active
                                ? 'bg-[#1a1a1a] border-[#DC143C]'
                                : 'bg-[#0d0d0d] border-[#1a1a1a] opacity-60 hover:opacity-100'
                            }`}
                          >
                            <div className="text-[11px] font-bold text-white tracking-tight">{mode.label}</div>
                            <div className="text-[10px] text-[#8a8a8a] leading-tight">{mode.note}</div>
                          </button>
                        )
                      })}
                    </div>
                  )}

                  {usingSubscription && (
                    <div className="mt-2">
                      {subscriptionDetected === true ? (
                        <p className="text-[11px] text-[#00E5FF] font-bold">
                          {t.accountOk(nomeAccount)}
                        </p>
                      ) : claudeCodePresente ? (
                        // Il programma c'è: un clic apre il suo login
                        // ufficiale. Il plugin non vede mai le credenziali.
                        <>
                          <button
                            type="button"
                            disabled={collegamento === 'corso'}
                            onClick={() => {
                              setCollegamento('corso')
                              whycremisi.send({ type: 'config.set', payload: { key: chiaveLink } })
                            }}
                            className="group w-full flex items-center justify-between gap-2 bg-white text-black px-4 py-2.5 rounded-full disabled:opacity-60 transition-all duration-500 ease-[cubic-bezier(0.32,0.72,0,1)] active:scale-[0.98]"
                          >
                            <span className="text-[12px] font-semibold tracking-tight">
                              {collegamento === 'corso' ? t.waiting : t.linkBtn(nomeAccount)}
                            </span>
                            <span className="w-6 h-6 rounded-full bg-black/5 flex items-center justify-center transition-transform duration-500 ease-[cubic-bezier(0.32,0.72,0,1)] group-hover:translate-x-0.5 group-hover:-translate-y-[1px]">
                              <span className="text-[11px]">↗</span>
                            </span>
                          </button>
                          <p className="text-[10px] text-[#8a8a8a] leading-snug mt-1.5">
                            {collegamento && collegamento !== 'corso' && collegamento !== 'fatto'
                              ? <span className="text-[#FFB000]">{collegamento}</span>
                              : collegamento === 'corso' ? t.linkWait : t.linkHint(nomeAccount)}
                          </p>
                        </>
                      ) : (
                        <>
                          <p className="text-[11px] text-[#FFB000] font-bold">
                            {claudeCodePresente === false
                              ? t.notInstalled(isChatGpt ? 'Codex' : 'Claude')
                              : t.checking(nomeAccount)}
                          </p>
                          {claudeCodePresente === false && (
                            <p className="text-[10px] text-[#8a8a8a] leading-snug mt-0.5">
                              {t.installHint(isChatGpt ? 'openai.com/codex' : 'claude.com/claude-code')}
                            </p>
                          )}
                        </>
                      )}
                    </div>
                  )}

                  {provider === 'openrouter' && (
                    <div className="mb-2 px-2.5 py-2 border border-[#1a1a1a] bg-[#0d0d0d]">
                      <p className="text-[11px] text-[#00E5FF] font-bold mb-0.5">{t.orTitle}</p>
                      {orCollegato && (
                        <p className="text-[11px] text-[#00FFaa] font-bold mb-0.5">{t.orLinked}</p>
                      )}
                      <p className="text-[10px] text-[#8a8a8a] leading-snug">{t.orBody}</p>
                    </div>
                  )}

                  {needsApiKey && (
                    <div className={hasAccount ? 'mt-2' : ''}>
                      {/* Accesso col proprio account: apre la pagina vera del
                          fornitore nel browser. Su OpenRouter un clic basta e
                          la chiave torna da sola. */}
                      <button
                        onClick={() => {
                          if (provider === 'openrouter') {
                            setOrInCorso(true)
                            whycremisi.send({ type: 'config.set', payload: { key: 'ai.linkOpenRouter' } })
                          } else {
                            whycremisi.send({ type: 'config.set', payload: { key: 'ai.openLogin', provider } })
                          }
                        }}
                        className="group w-full mb-1.5 inline-flex items-center justify-between rounded-full pl-4 pr-2 py-2 text-[12px] font-semibold tracking-tight transition-all duration-500"
                        style={{ backgroundColor: '#ffffff', color: '#0a0a0a' }}
                      >
                        {provider === 'openrouter' && orInCorso ? t.waiting : t.loginWith(nomeProvider(provider))}
                        <span className="w-6 h-6 rounded-full bg-black/10 flex items-center justify-center transition-transform duration-500 group-hover:translate-x-0.5">
                          <span className="material-symbols-outlined text-[14px]">open_in_new</span>
                        </span>
                      </button>

                      <div className="relative flex">
                        <input
                          type={showKey ? 'text' : 'password'}
                          placeholder={
                            provider === 'gemini' ? 'AIza...'
                            : provider === 'openai' ? 'sk-...'
                            : provider === 'openrouter' ? 'sk-or-...'
                            : 'sk-ant-...'
                          }
                          value={apiKey}
                          onChange={(e) => setApiKey(e.target.value)}
                          className="flex-1 bg-[#0d0d0d] border border-[#1a1a1a] px-3 py-2 text-[12px] text-white font-mono focus:border-[#DC143C] focus:outline-none transition-colors"
                        />
                        <button
                          onClick={() => setShowKey(!showKey)}
                          className="px-2.5 border border-l-0 border-[#1a1a1a] text-[#777] hover:text-[#FFB000] transition-colors"
                        >
                          <span className="material-symbols-outlined text-base">{showKey ? 'visibility_off' : 'visibility'}</span>
                        </button>
                      </div>
                      {apiKey.length > 0 && provider === 'gemini' && !apiKey.startsWith('AIza') && (
                        <p className="text-[10px] text-[#FFB000] mt-1">{t.keyBad('AIza')}</p>
                      )}
                      {apiKey.length > 0 && provider === 'openai' && !apiKey.startsWith('sk-') && (
                        <p className="text-[10px] text-[#FFB000] mt-1">{t.keyBad('sk-')}</p>
                      )}
                      {apiKey.length > 0 && provider === 'anthropic' && !apiKey.startsWith('sk-ant-') && (
                        <p className="text-[10px] text-[#FFB000] mt-1">{t.keyBad('sk-ant-')}</p>
                      )}
                      {apiKey.length > 0 && provider === 'openrouter' && !apiKey.startsWith('sk-or-') && (
                        <p className="text-[10px] text-[#FFB000] mt-1">{t.keyBad('sk-or-')}</p>
                      )}
                    </div>
                  )}
                </div>
              )}

              <div className="mb-4">
                <Etichetta n={provider === 'ollama' ? '2' : '3'}>{t.step3}</Etichetta>
                {/* Su OpenRouter i nomi sono lunghi e disuguali: in griglia
                    restano incolonnati invece di spezzarsi a caso. */}
                <div className={provider === 'openrouter' ? 'grid grid-cols-2 gap-1.5' : 'flex flex-wrap gap-1.5'}>
                  {modelList.map(m => {
                    const free = isFreeModel(m)
                    const nome = m === 'default' ? t.defaultName : nomeModello(m)
                    return (
                      <motion.button
                        key={m}
                        whileHover={{ scale: 1.02 }}
                        whileTap={{ scale: 0.98 }}
                        onClick={() => setModel(m)}
                        title={notaModello(m) ? `${m}: ${notaModello(m)}` : m}
                        className={`px-2.5 py-1.5 text-[11px] font-semibold border transition-all flex items-center justify-between gap-1.5 min-w-0 ${
                          model === m
                            ? 'bg-[#DC143C] text-white border-[#DC143C]'
                            : 'bg-[#0d0d0d] text-[#999] border-[#1a1a1a] hover:border-[#DC143C] hover:text-white'
                        }`}
                      >
                        <span className="truncate">{nome}</span>
                        {free && (
                          <span className={`text-[8px] px-1 leading-[1.5] flex-shrink-0 ${
                            model === m ? 'bg-white/20 text-white' : 'bg-[#00FFaa]/15 text-[#00FFaa]'
                          }`}>{t.free}</span>
                        )}
                      </motion.button>
                    )
                  })}
                </div>
                {notaModello(model) && (
                  <p className="text-[10px] text-[#8a8a8a] leading-snug mt-1">{notaModello(model)}</p>
                )}
              </div>

              {(isClaude || usingSubscription) && (
                <div className="mb-4">
                  <label className="text-[11px] font-bold text-white tracking-tight block mb-1.5">{t.step4}</label>
                  <div className="flex flex-wrap gap-1.5">
                    {effortLevels.filter(lvl => !(isChatGpt && lvl.id === 'max')).map(lvl => (
                      <motion.button
                        key={lvl.id}
                        whileHover={{ scale: 1.02 }}
                        whileTap={{ scale: 0.98 }}
                        onClick={() => setEffort(lvl.id)}
                        title={t.efforts[lvl.id]}
                        className={`px-2.5 py-1 text-[11px] font-semibold border transition-all ${
                          effort === lvl.id
                            ? 'bg-[#DC143C] text-white border-[#DC143C]'
                            : 'bg-[#0d0d0d] text-[#999] border-[#1a1a1a] hover:border-[#DC143C] hover:text-white'
                        }`}
                      >{lvl.label}</motion.button>
                    ))}
                  </div>
                  <p className="text-[10px] text-[#8a8a8a] leading-snug mt-1">
                    {t.efforts[effort]}. {t.effortNote}
                  </p>
                </div>
              )}
            </motion.div>
          </AnimatePresence>

          <div className="flex items-center gap-2">
            <motion.button
              whileHover={{ scale: 1.01 }}
              whileTap={{ scale: 0.99 }}
              onClick={handleTestConnection}
              disabled={
                status === 'testing' ||
                (needsApiKey && !apiKey) ||
                // Con l'account serve l'accesso già fatto.
                (usingSubscription && !subscriptionDetected)
              }
              className={`flex-1 py-3 font-black tracking-wide text-[13px] flex items-center justify-center gap-2 transition-all ${
                status === 'success' ? 'bg-[#00FFaa] text-black' :
                status === 'error' ? 'bg-[#DC143C] text-white' :
                'bg-[#FFB000] text-black hover:bg-white'
              } disabled:opacity-40`}
            >
              {status === 'testing' ? (
                <><div className="w-3.5 h-3.5 border-2 border-black border-t-transparent rounded-full animate-spin" />{t.trying}</>
              ) : status === 'success' ? (
                <><span className="material-symbols-outlined text-base">check_circle</span>{t.connected}</>
              ) : (
                t.start
              )}
            </motion.button>
          </div>

          <AnimatePresence>
            {errorMsg && (
              <motion.p
                initial={{ opacity: 0, height: 0 }}
                animate={{ opacity: 1, height: 'auto' }}
                exit={{ opacity: 0, height: 0 }}
                className="text-center text-[12px] text-[#ff6b6b] font-semibold mt-2 leading-snug"
              >
                {errorMsg}
              </motion.p>
            )}
          </AnimatePresence>
        </div>

        <div className="px-5 py-1.5 border-t border-[#1a1a1a] flex justify-between items-center">
          <span className="text-[9px] font-mono text-[#444] tracking-wider">WHYCREMISI</span>
          <span className="text-[9px] font-mono text-[#444]">
            {status === 'success' ? t.footerOk : provider === 'ollama' ? t.nameOllama : t.footerNo}
          </span>
        </div>
      </motion.div>
    </motion.div>
  )
}
