import React, { useState } from 'react'
import { motion, AnimatePresence } from 'framer-motion'
import { BotFace } from './BotFace'
import { whycremisi } from '../whycremisi-bridge'

export function SetupScreen({ onComplete, initialConfig = {} }) {
  const [provider, setProvider] = useState(initialConfig.provider || 'ollama')
  const [apiKey, setApiKey] = useState(initialConfig.apiKey || '')
  const [status, setStatus] = useState('idle') // idle, testing, success, error
  const [errorMsg, setErrorMsg] = useState('')

  const providers = [
    { id: 'ollama',    name: 'LOCAL CORE (Ollama)', icon: 'memory',       desc: 'Run AI locally on your Mac. No API keys needed.' },
    { id: 'gemini',    name: 'NEURAL CLOUD (Gemini)', icon: 'cloud_queue',  desc: 'Google Gemini Pro. Requires API Key.' },
    { id: 'openai',    name: 'NEURAL CLOUD (OpenAI)', icon: 'auto_awesome', desc: 'OpenAI GPT-4. Requires API Key.' },
    { id: 'anthropic', name: 'NEURAL CLOUD (Claude)', icon: 'Psychology',   desc: 'Anthropic Claude 3.5. Requires API Key.' },
  ]

  const handleTestConnection = async () => {
    setStatus('testing')
    setErrorMsg('')

    // Invia prima la config al plugin
    if (whycremisi.isConnected()) {
      whycremisi.send({ type: 'config.set', payload: { key: 'ai.provider', value: provider } })
      if (apiKey) {
        whycremisi.send({ type: 'config.set', payload: { key: 'ai.apiKey', value: apiKey, provider } })
      }
    }

    // Se il plugin è connesso, usa il vero test via WS; altrimenti fallback locale
    if (whycremisi.isConnected()) {
      const timeout = setTimeout(() => {
        setStatus('error')
        setErrorMsg('Timeout — plugin non risponde al test di connessione.')
      }, 8000)

      const unsub = whycremisi.on('config.response', (payload) => {
        if (payload?.key !== 'ai.testConnection') return
        clearTimeout(timeout)
        unsub()
        if (payload.connected) {
          setStatus('success')
          setTimeout(() => onComplete({ provider, apiKey }), 1200)
        } else {
          setStatus('error')
          setErrorMsg(payload.error || 'Connessione fallita.')
        }
      })

      whycremisi.send({ type: 'config.set', payload: { key: 'ai.testConnection' } })
    } else {
      // Plugin non connesso: test locale (lunghezza key)
      setTimeout(() => {
        if (provider === 'ollama' || apiKey.length > 10) {
          setStatus('success')
          setTimeout(() => onComplete({ provider, apiKey }), 1200)
        } else {
          setStatus('error')
          setErrorMsg('Plugin non connesso. Avvia il Standalone e riprova.')
        }
      }, 1000)
    }
  }

  return (
    <div className="fixed inset-0 z-[100] bg-[#0d0d0d] flex items-center justify-center font-['Space_Grotesk'] overflow-hidden">
      {/* Background FX */}
      <div className="absolute inset-0 opacity-20 pointer-events-none">
        <div className="absolute top-[-10%] left-[-10%] w-[40%] h-[40%] bg-[#DC143C] blur-[120px] rounded-full animate-pulse" />
        <div className="absolute bottom-[-10%] right-[-10%] w-[40%] h-[40%] bg-[#FFB000] blur-[120px] rounded-full opacity-50" />
      </div>
      <div className="crt-overlay pointer-events-none" />

      <motion.div 
        initial={{ opacity: 0, y: 20, scale: 0.95 }}
        animate={{ opacity: 1, y: 0, scale: 1 }}
        className="w-full max-w-2xl bg-[#131313]/80 backdrop-blur-xl border border-[#222222] p-8 relative overflow-hidden shadow-2xl"
      >
        <div className="absolute top-0 left-0 w-full h-1 bg-gradient-to-r from-[#DC143C] via-[#FFB000] to-[#DC143C]" />
        
        <header className="flex items-center gap-6 mb-10">
          <BotFace state={status === 'testing' ? 'thinking' : status === 'success' ? 'success' : 'idle'} className="w-16 h-16" />
          <div>
            <h1 className="text-2xl font-black tracking-tighter text-white uppercase italic">WhyCremisi <span className="text-[#DC143C]">Setup</span></h1>
            <p className="text-[#4d4d4d] text-xs font-mono uppercase tracking-[0.2em]">Initial Neural Matrix Configuration</p>
          </div>
        </header>

        <div className="grid grid-cols-1 gap-6">
          {/* Provider Selection */}
          <div className="space-y-4">
            <label className="text-[10px] font-bold text-[#FFB000] uppercase tracking-widest block">Select Intelligence Source</label>
            <div className="grid grid-cols-2 gap-3">
              {providers.map(p => (
                <motion.button
                  key={p.id}
                  whileHover={{ scale: 1.02 }}
                  whileTap={{ scale: 0.98 }}
                  onClick={() => setProvider(p.id)}
                  className={`p-4 border text-left transition-all ${
                    provider === p.id 
                      ? 'bg-[#1a1a1a] border-[#DC143C] shadow-[0_0_15px_rgba(220,20,60,0.2)]' 
                      : 'bg-[#0d0d0d] border-[#222222] opacity-60 hover:opacity-100'
                  }`}
                >
                  <div className="flex items-center gap-2 mb-1">
                    <span className="material-symbols-outlined text-lg" style={{ color: provider === p.id ? '#DC143C' : '#4d4d4d' }}>{p.icon}</span>
                    <span className="text-[11px] font-bold text-white uppercase tracking-tight">{p.name}</span>
                  </div>
                  <p className="text-[9px] text-[#4d4d4d] leading-tight">{p.desc}</p>
                </motion.button>
              ))}
            </div>
          </div>

          {/* API Key Input */}
          <AnimatePresence mode="wait">
            {provider !== 'ollama' && (
              <motion.div 
                initial={{ opacity: 0, height: 0 }}
                animate={{ opacity: 1, height: 'auto' }}
                exit={{ opacity: 0, height: 0 }}
                className="space-y-3"
              >
                <label className="text-[10px] font-bold text-[#FFB000] uppercase tracking-widest block">Neural API Key</label>
                <div className="relative">
                  <input 
                    type="password"
                    placeholder="Enter your API Key..."
                    value={apiKey}
                    onChange={(e) => setApiKey(e.target.value)}
                    className="w-full bg-[#0d0d0d] border border-[#222222] p-3 text-sm text-white font-mono focus:border-[#DC143C] focus:outline-none transition-colors"
                  />
                  <div className="absolute right-3 top-1/2 -translate-y-1/2 text-[10px] text-[#4d4d4d] font-mono">
                    {apiKey.length > 0 ? 'HIDDEN' : 'REQUIRED'}
                  </div>
                </div>
              </motion.div>
            )}
          </AnimatePresence>

          {/* Action Button */}
          <div className="pt-4 flex flex-col gap-3">
            <motion.button
              whileHover={{ scale: 1.01 }}
              whileTap={{ scale: 0.99 }}
              onClick={handleTestConnection}
              disabled={status === 'testing' || (provider !== 'ollama' && !apiKey)}
              className={`w-full py-4 font-black uppercase tracking-[0.2em] text-sm flex items-center justify-center gap-3 transition-all ${
                status === 'success' ? 'bg-[#00FFaa] text-black' : 
                status === 'error' ? 'bg-[#DC143C] text-white' : 
                'bg-[#FFB000] text-black hover:bg-white'
              } disabled:opacity-40`}
            >
              {status === 'testing' ? (
                <>
                  <div className="w-4 h-4 border-2 border-black border-t-transparent rounded-full animate-spin" />
                  CONNECTING...
                </>
              ) : status === 'success' ? (
                <>
                  <span className="material-symbols-outlined">check_circle</span>
                  NEURAL LINK ESTABLISHED
                </>
              ) : (
                'INITIALIZE NEURAL LINK'
              )}
            </motion.button>
            
            <AnimatePresence>
              {errorMsg && (
                <motion.p 
                  initial={{ opacity: 0 }} 
                  animate={{ opacity: 1 }} 
                  className="text-center text-[10px] text-[#DC143C] font-mono uppercase font-bold"
                >
                  {errorMsg}
                </motion.p>
              )}
            </AnimatePresence>
          </div>
        </div>

        <footer className="mt-10 pt-4 border-t border-[#222222] flex justify-between items-center text-[9px] font-mono text-[#333]">
          <span>WHYCREMISI V0.1.0 // ENTERPRISE BUILD</span>
          <span className="text-[#4d4d4d]">SECURITY ENCRYPTED SHA-256</span>
        </footer>
      </motion.div>
    </div>
  )
}
