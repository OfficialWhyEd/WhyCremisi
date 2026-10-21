import React, { useState, useEffect, useRef } from 'react'
import { motion, AnimatePresence } from 'framer-motion'
import { whycremisi } from '../whycremisi-bridge'

const modelsByProvider = {
  ollama:    ['llama3.2', 'llama3.1', 'mistral', 'mixtral', 'codellama', 'qwen2.5'],
  gemini:    ['gemini-3.1-flash-lite', 'gemini-3.1-flash-lite-preview', 'gemini-2.0-flash', 'gemini-1.5-pro'],
  openai:    ['gpt-4o', 'gpt-4o-mini', 'gpt-4-turbo', 'gpt-3.5-turbo'],
  anthropic: ['claude-opus-5', 'claude-sonnet-5', 'claude-haiku-4-5'],
  // Con l'account Claude i modelli sono gli alias del programma ufficiale.
  claudecode: ['sonnet', 'opus', 'haiku'],
  // Con l'account ChatGPT il modello predefinito è quello dell'abbonamento.
  codex: ['default', 'gpt-5-codex', 'gpt-5'],
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

// Etichette leggibili: nella UI si sceglie "Opus 5", non "claude-opus-5".
const modelLabels = {
  'claude-opus-5':    { name: 'Opus 5',    note: 'Il più capace' },
  'claude-sonnet-5':  { name: 'Sonnet 5',  note: 'Equilibrato' },
  'claude-haiku-4-5': { name: 'Haiku 4.5', note: 'Rapido ed economico' },
  sonnet: { name: 'Sonnet', note: 'Equilibrato: ideale per il lavoro in DAW' },
  opus:   { name: 'Opus',   note: 'Il più capace, più lento' },
  haiku:  { name: 'Haiku',  note: 'Rapido ed economico' },
  default: { name: 'Predefinito', note: 'Il modello che ChatGPT sceglie per il tuo abbonamento' },
}

// Profondità di ragionamento. Alza la qualità sui compiti difficili e
// il numero di token spesi.
const effortLevels = [
  { id: 'low',    label: 'Low',    note: 'Risposte rapide' },
  { id: 'medium', label: 'Medium', note: 'Compromesso' },
  { id: 'high',   label: 'High',   note: 'Predefinito' },
  { id: 'xhigh',  label: 'X-High', note: 'Compiti complessi' },
  { id: 'max',    label: 'Max',    note: 'Nessun compromesso' },
]

const providerNames = {
  ollama: 'LOCAL CORE',
  gemini: 'GEMINI',
  openai: 'OPENAI',
  anthropic: 'CLAUDE',
  openrouter: 'OPENROUTER',
}

export function SetupScreen({ onComplete, onSkip, initialConfig = {} }) {
  // "claudecode" è come il plugin chiama l'accesso con l'account Claude; qui
  // sta sotto la card Claude, con la modalità "Abbonamento".
  const savedClaudeCode = initialConfig.provider === 'claudecode'
  const savedCodex = initialConfig.provider === 'codex'
  const [provider, setProvider] = useState(
    savedClaudeCode ? 'anthropic' : savedCodex ? 'openai' : (initialConfig.provider || 'ollama'))
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
    (savedClaudeCode || savedCodex) ? 'subscription' : (initialConfig.claudeAuthMode || 'subscription'))
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
  const needsApiKey = provider !== 'ollama' && !usingSubscription

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
            setCollegamento('Accesso non completato. Riprova.')
            return
          }
          whycremisi.send({ type: 'config.set', payload: { key: chiaveDetect } })
        }, 2500)
      } else {
        // Il motivo va detto: un pulsante che torna com'era non spiega niente.
        setCollegamento(payload.message || 'Collegamento non riuscito.')
      }
    })
    whycremisi.send({ type: 'config.set', payload: { key: chiaveDetect } })
    return () => {
      unsub(); unsubLink()
      if (pollRef.current) { clearInterval(pollRef.current); pollRef.current = null }
    }
  }, [provider])

  const providers = [
    { id: 'ollama',    icon: 'memory',       desc: 'In locale sulla tua macchina. Nessuna chiave.', keyRequired: false },
    { id: 'gemini',    icon: 'cloud_queue',  desc: 'Google Gemini.', keyRequired: true, keyPrefix: 'AIza' },
    { id: 'openai',    icon: 'auto_awesome', desc: 'Il tuo account ChatGPT, oppure una chiave API.', keyRequired: true, keyPrefix: 'sk-' },
    { id: 'anthropic', icon: 'psychology',   desc: 'Il tuo account Claude, oppure una chiave API.', keyRequired: true, keyPrefix: 'sk-ant-' },
    { id: 'openrouter', icon: 'hub',         desc: 'Senza abbonamento: modelli gratuiti con una chiave gratis.', keyRequired: true, keyPrefix: 'sk-or-' },
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
        setErrorMsg('Nessuna risposta dal plugin entro 20 secondi.')
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
          setErrorMsg(payload.error || 'Verifica non riuscita, senza motivo riportato.')
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
          setErrorMsg('Plugin non collegato. Avvia lo Standalone e riprova.')
        }
      }, 1000)
      timeoutsRef.current.push(t)
    }
  }

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
        className="relative w-full max-w-lg bg-[#131313]/95 backdrop-blur-xl border border-[#222222] shadow-2xl overflow-hidden"
      >
        <div className="absolute top-0 left-0 w-full h-[2px] bg-gradient-to-r from-[#DC143C] via-[#FFB000] to-[#00E5FF]" />

        <div className="px-5 py-4">
          <header className="flex items-center justify-between mb-4">
            <div className="flex items-center gap-3">
              <img src="/whycremisi-mask.png" className="w-7 h-7 flex-shrink-0" alt="WhyCremisi" />
              <div>
                <h2 className="text-xs font-black tracking-tighter text-white uppercase leading-none">
                  Neural <span className="text-[#DC143C]">Link</span>
                </h2>
                <p className="text-[8px] text-[#4d4d4d] font-mono uppercase tracking-[0.15em] leading-tight">Configure AI Backend</p>
              </div>
            </div>
            <div className="flex items-center gap-2">
              {onSkip && (
                <button
                  onClick={onSkip}
                  className="text-[8px] text-[#555] hover:text-[#888] font-mono uppercase tracking-widest underline decoration-dotted underline-offset-2 transition-colors"
                >
                  Skip
                </button>
              )}
            </div>
          </header>

          {/* Due colonne, con l'ultima card a piena larghezza quando i
              provider sono in numero dispari: così nessuna resta orfana
              su una riga occupandone solo un quarto. */}
          <div className="grid grid-cols-2 gap-1.5 mb-3 items-stretch">
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
                  className={`px-2 py-1.5 border text-left transition-all flex items-center gap-2 min-h-[2.75rem] ${
                    isSelected
                      ? 'bg-[#1a1a1a] border-[#DC143C]'
                      : 'bg-[#0d0d0d] border-[#1a1a1a] opacity-50 hover:opacity-100'
                  }`}
                >
                  <span className="material-symbols-outlined text-sm flex-shrink-0" style={{ color: isSelected ? '#DC143C' : '#555' }}>{p.icon}</span>
                  <span className="min-w-0">
                    <span className="block text-[9px] font-bold text-white uppercase tracking-tight leading-none mb-0.5">{providerNames[p.id]}</span>
                    <span className="block text-[7px] text-[#555] leading-tight">{p.desc}</span>
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
              {hasAccount && (
                <div className="mb-3">
                  <label className="text-[8px] font-bold text-[#FFB000] uppercase tracking-widest block mb-1">
                    Accesso
                  </label>
                  <div className="grid grid-cols-2 gap-1.5">
                    {[
                      { id: 'subscription', label: 'Il mio account', note: isChatGpt ? 'ChatGPT Plus, Pro o Business, nessuna chiave' : 'Claude Pro o Max, nessuna chiave' },
                      { id: 'apikey',       label: 'Chiave API',  note: 'Consumo a token' },
                    ].map(mode => {
                      const active = claudeAuthMode === mode.id
                      return (
                        <button
                          key={mode.id}
                          onClick={() => setClaudeAuthMode(mode.id)}
                          className={`px-2 py-1.5 border text-left transition-all ${
                            active
                              ? 'bg-[#1a1a1a] border-[#DC143C]'
                              : 'bg-[#0d0d0d] border-[#1a1a1a] opacity-60 hover:opacity-100'
                          }`}
                        >
                          <div className="text-[9px] font-bold text-white uppercase tracking-tight">{mode.label}</div>
                          <div className="text-[7px] text-[#555] leading-tight">{mode.note}</div>
                        </button>
                      )
                    })}
                  </div>

                  {usingSubscription && (
                    <div className="mt-1.5">
                      {subscriptionDetected === true ? (
                        <p className="text-[8px] text-[#00E5FF] font-mono">
                          Account {nomeAccount} collegato — nessuna chiave da inserire.
                        </p>
                      ) : claudeCodePresente ? (
                        // Il programma "claude" c'è: un clic apre il suo login
                        // ufficiale. Il plugin non vede mai le credenziali.
                        <>
                          <button
                            type="button"
                            disabled={collegamento === 'corso'}
                            onClick={() => {
                              setCollegamento('corso')
                              whycremisi.send({ type: 'config.set', payload: { key: 'ai.linkClaude' } })
                            }}
                            className="group w-full flex items-center justify-between gap-2 bg-white text-black px-3 py-2 rounded-full disabled:opacity-60 transition-all duration-500 ease-[cubic-bezier(0.32,0.72,0,1)] active:scale-[0.98]"
                          >
                            <span className="text-[10px] font-medium tracking-tight">
                              {collegamento === 'corso' ? 'In attesa dell’accesso…' : `Collega il mio account ${nomeAccount}`}
                            </span>
                            <span className="w-5 h-5 rounded-full bg-black/5 flex items-center justify-center transition-transform duration-500 ease-[cubic-bezier(0.32,0.72,0,1)] group-hover:translate-x-0.5 group-hover:-translate-y-[1px]">
                              <span className="text-[9px]">↗</span>
                            </span>
                          </button>
                          <p className="text-[7px] text-[#555] leading-tight mt-1">
                            {collegamento && collegamento !== 'corso' && collegamento !== 'fatto'
                              ? collegamento
                              : collegamento === 'corso'
                                ? 'Completa l’accesso nella finestra e nel browser, poi torna qui.'
                                : `Si apre il login ufficiale di ${nomeAccount}: accedi con il tuo account, non devi incollare nulla.`}
                          </p>
                        </>
                      ) : (
                        <>
                          <p className="text-[8px] text-[#FFB000] font-mono">
                            {claudeCodePresente === false
                              ? `Il programma ${isChatGpt ? 'Codex' : 'Claude'} non è installato su questo computer.`
                              : `Controllo dell’account ${nomeAccount}…`}
                          </p>
                          {claudeCodePresente === false && (
                            <p className="text-[7px] text-[#555] leading-tight mt-0.5">
                              {isChatGpt ? 'Installa Codex da openai.com/codex' : 'Installa Claude Code da claude.com/claude-code'}, poi riapri questa schermata:
                              l’accesso si fa con un clic. In alternativa usa una chiave API.
                            </p>
                          )}
                        </>
                      )}
                    </div>
                  )}
                </div>
              )}

              {provider === 'openrouter' && (
                <div className="mb-2 px-2 py-1.5 border border-[#1a1a1a] bg-[#0d0d0d]">
                  <p className="text-[8px] text-[#00E5FF] font-bold uppercase tracking-widest mb-0.5">
                    Nessun abbonamento necessario
                  </p>
                  <p className="text-[7px] text-[#777] leading-relaxed">
                    La chiave di OpenRouter si crea gratis su openrouter.ai. DeepSeek V4.1 Flash è il primo modello:
                    costa pochi centesimi ogni migliaia di richieste. I modelli{' '}
                    <span className="text-[#00FFaa] font-bold">FREE</span> non si pagano, ma hanno limiti più stretti.
                    Se uno è sovraccarico o senza credito, si passa da soli a uno gratuito.
                  </p>
                </div>
              )}

              {needsApiKey && (
                <div className="mb-3">
                  <label className="text-[8px] font-bold text-[#FFB000] uppercase tracking-widest block mb-1">
                    API Key
                  </label>
                  {/* Accesso col proprio account: apre la pagina vera del
                      fornitore nel browser, dove si entra con la propria
                      email. Da lì si copia la chiave e si torna qui — una
                      volta sola, poi resta salvata. */}
                  <button
                    onClick={() => whycremisi.send({
                      type: 'config.set',
                      payload: { key: 'ai.openLogin', provider },
                    })}
                    className="group w-full mb-1.5 inline-flex items-center justify-between rounded-full pl-3.5 pr-1.5 py-1.5 text-[10px] font-bold tracking-tight transition-all duration-500"
                    style={{ backgroundColor: '#ffffff', color: '#0a0a0a' }}
                  >
                    Accedi con {providerNames[provider]}
                    <span className="w-5 h-5 rounded-full bg-black/10 flex items-center justify-center transition-transform duration-500 group-hover:translate-x-0.5">
                      <span className="material-symbols-outlined text-[12px]">open_in_new</span>
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
                      className="flex-1 bg-[#0d0d0d] border border-[#1a1a1a] px-2.5 py-1.5 text-xs text-white font-mono focus:border-[#DC143C] focus:outline-none transition-colors"
                    />
                    <button
                      onClick={() => setShowKey(!showKey)}
                      className="px-2 border border-l-0 border-[#1a1a1a] text-[#555] hover:text-[#FFB000] transition-colors"
                    >
                      <span className="material-symbols-outlined text-sm">{showKey ? 'visibility_off' : 'visibility'}</span>
                    </button>
                  </div>
                  {apiKey.length > 0 && provider === 'gemini' && !apiKey.startsWith('AIza') && (
                    <p className="text-[8px] text-[#FFB000] font-mono mt-0.5">Gemini keys start with "AIza..."</p>
                  )}
                  {apiKey.length > 0 && provider === 'openai' && !apiKey.startsWith('sk-') && (
                    <p className="text-[8px] text-[#FFB000] font-mono mt-0.5">OpenAI keys start with "sk-..."</p>
                  )}
                  {apiKey.length > 0 && provider === 'anthropic' && !apiKey.startsWith('sk-ant-') && (
                    <p className="text-[8px] text-[#FFB000] font-mono mt-0.5">Anthropic keys start with "sk-ant-..."</p>
                  )}
                  {apiKey.length > 0 && provider === 'openrouter' && !apiKey.startsWith('sk-or-') && (
                    <p className="text-[8px] text-[#FFB000] font-mono mt-0.5">OpenRouter keys start with "sk-or-..."</p>
                  )}
                </div>
              )}

              <div className="mb-3">
                <label className="text-[8px] font-bold text-[#00E5FF] uppercase tracking-widest block mb-1">Model</label>
                {/* Su OpenRouter i nomi sono lunghi e disuguali: in griglia
                    restano incolonnati invece di spezzarsi a caso. */}
                <div className={provider === 'openrouter' ? 'grid grid-cols-2 gap-1' : 'flex flex-wrap gap-1'}>
                  {modelList.map(m => {
                    const label = modelLabels[m]
                    const free = isFreeModel(m)
                    const text = label ? label.name
                      : provider === 'openrouter' ? shortModelName(m)
                      : m
                    return (
                      <motion.button
                        key={m}
                        whileHover={{ scale: 1.02 }}
                        whileTap={{ scale: 0.98 }}
                        onClick={() => setModel(m)}
                        title={label ? `${m} — ${label.note}` : m}
                        className={`px-2 py-1 text-[8px] font-bold uppercase tracking-wider border transition-all flex items-center justify-between gap-1 min-w-0 ${
                          model === m
                            ? 'bg-[#DC143C] text-white border-[#DC143C]'
                            : 'bg-[#0d0d0d] text-[#777] border-[#1a1a1a] hover:border-[#DC143C] hover:text-white'
                        }`}
                      >
                        <span className="truncate">{text}</span>
                        {free && (
                          <span className={`text-[6px] px-1 leading-[1.4] flex-shrink-0 ${
                            model === m ? 'bg-white/20 text-white' : 'bg-[#00FFaa]/15 text-[#00FFaa]'
                          }`}>FREE</span>
                        )}
                      </motion.button>
                    )
                  })}
                </div>
                {hasAccount && modelLabels[model] && (
                  <p className="text-[7px] text-[#555] leading-tight mt-0.5">{modelLabels[model].note}</p>
                )}
              </div>

              {(isClaude || usingSubscription) && (
                <div className="mb-3">
                  <label className="text-[8px] font-bold text-[#00E5FF] uppercase tracking-widest block mb-1">
                    Ragionamento
                  </label>
                  <div className="flex flex-wrap gap-1">
                    {effortLevels.filter(lvl => !(isChatGpt && lvl.id === 'max')).map(lvl => (
                      <motion.button
                        key={lvl.id}
                        whileHover={{ scale: 1.02 }}
                        whileTap={{ scale: 0.98 }}
                        onClick={() => setEffort(lvl.id)}
                        title={lvl.note}
                        className={`px-2 py-0.5 text-[8px] font-bold uppercase tracking-wider border transition-all ${
                          effort === lvl.id
                            ? 'bg-[#DC143C] text-white border-[#DC143C]'
                            : 'bg-[#0d0d0d] text-[#777] border-[#1a1a1a] hover:border-[#DC143C] hover:text-white'
                        }`}
                      >{lvl.label}</motion.button>
                    ))}
                  </div>
                  <p className="text-[7px] text-[#555] leading-tight mt-0.5">
                    Più alto significa risposte migliori sui compiti difficili e più token consumati.
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
                // Con l'account Claude serve l'accesso già fatto.
                (usingSubscription && !subscriptionDetected)
              }
              className={`flex-1 py-2 font-black uppercase tracking-[0.15em] text-[9px] flex items-center justify-center gap-2 transition-all ${
                status === 'success' ? 'bg-[#00FFaa] text-black' :
                status === 'error' ? 'bg-[#DC143C] text-white' :
                'bg-[#FFB000] text-black hover:bg-white'
              } disabled:opacity-40`}
            >
              {status === 'testing' ? (
                <><div className="w-3 h-3 border-2 border-black border-t-transparent rounded-full animate-spin" />CONNECT</>
              ) : status === 'success' ? (
                <><span className="material-symbols-outlined text-xs">check_circle</span>LINKED</>
              ) : (
                'INITIALIZE LINK'
              )}
            </motion.button>
          </div>

          <AnimatePresence>
            {errorMsg && (
              <motion.p
                initial={{ opacity: 0, height: 0 }}
                animate={{ opacity: 1, height: 'auto' }}
                exit={{ opacity: 0, height: 0 }}
                className="text-center text-[8px] text-[#DC143C] font-mono uppercase font-bold mt-2"
              >
                {errorMsg}
              </motion.p>
            )}
          </AnimatePresence>
        </div>

        <div className="px-5 py-1.5 border-t border-[#1a1a1a] flex justify-between items-center">
          <span className="text-[7px] font-mono text-[#333] tracking-wider">WHYCREMISI AI BACKEND</span>
          <span className="text-[7px] font-mono text-[#333]">
            {status === 'success' ? 'AUTHENTICATED' : provider === 'ollama' ? 'LOCAL MODE' : 'KEY PENDING'}
          </span>
        </div>
      </motion.div>
    </motion.div>
  )
}
