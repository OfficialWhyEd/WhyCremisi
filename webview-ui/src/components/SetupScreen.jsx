import React, { useState } from 'react'
import { motion, AnimatePresence } from 'framer-motion'
import { whycremisi } from '../whycremisi-bridge'

const modelsByProvider = {
  ollama:    ['llama3.2', 'llama3.1', 'mistral', 'mixtral', 'codellama', 'qwen2.5'],
  gemini:    ['gemini-3.1-flash-lite', 'gemini-3.1-flash-lite-preview', 'gemini-2.0-flash', 'gemini-1.5-pro'],
  openai:    ['gpt-4o', 'gpt-4o-mini', 'gpt-4-turbo', 'gpt-3.5-turbo'],
  anthropic: ['claude-3-5-sonnet-20241022', 'claude-3-5-haiku-20241022', 'claude-3-opus-20240229'],
}

const providerNames = {
  ollama: 'LOCAL CORE',
  gemini: 'GEMINI',
  openai: 'OPENAI',
  anthropic: 'CLAUDE',
}

export function SetupScreen({ onComplete, onSkip, initialConfig = {} }) {
  const [provider, setProvider] = useState(initialConfig.provider || 'ollama')
  const [model, setModel] = useState(initialConfig.model || modelsByProvider[initialConfig.provider]?.[0] || 'llama3.2')
  const [apiKey, setApiKey] = useState(initialConfig.apiKey || '')
  const [status, setStatus] = useState('idle')
  const [errorMsg, setErrorMsg] = useState('')
  const [showKey, setShowKey] = useState(false)

  const providers = [
    { id: 'ollama',    icon: 'memory',       desc: 'Run AI locally on your Mac. No API key needed.', keyRequired: false },
    { id: 'gemini',    icon: 'cloud_queue',  desc: 'Google Gemini Pro.', keyRequired: true, keyPrefix: 'AIza' },
    { id: 'openai',    icon: 'auto_awesome', desc: 'OpenAI GPT-4o.', keyRequired: true, keyPrefix: 'sk-' },
    { id: 'anthropic', icon: 'Psychology',   desc: 'Anthropic Claude 3.5.', keyRequired: true, keyPrefix: 'sk-ant-' },
  ]

  const handleTestConnection = async () => {
    setStatus('testing')
    setErrorMsg('')

    if (whycremisi.isConnected()) {
      whycremisi.send({ type: 'config.set', payload: { key: 'ai.provider', value: provider } })
      whycremisi.send({ type: 'config.set', payload: { key: 'ai.model', value: model } })
      if (apiKey)
        whycremisi.send({ type: 'config.set', payload: { key: 'ai.apiKey', value: apiKey, provider } })
    }

    if (whycremisi.isConnected()) {
      const timeout = setTimeout(() => {
        setStatus('error')
        setErrorMsg('Timeout — no response from plugin.')
      }, 8000)

      const unsub = whycremisi.on('config.response', (payload) => {
        if (payload?.key !== 'ai.testConnection') return
        clearTimeout(timeout); unsub()
        if (payload.connected) {
          setStatus('success')
          setTimeout(() => onComplete({ provider, model, apiKey }), 1200)
        } else {
          setStatus('error')
          setErrorMsg(payload.error || 'Connection failed.')
        }
      })
      whycremisi.send({ type: 'config.set', payload: { key: 'ai.testConnection' } })
    } else {
      setTimeout(() => {
        if (provider === 'ollama' || apiKey.length > 10) {
          setStatus('success')
          setTimeout(() => onComplete({ provider, model, apiKey }), 1200)
        } else {
          setStatus('error')
          setErrorMsg('Plugin not connected. Launch Standalone and retry.')
        }
      }, 1000)
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
              <div className="w-7 h-7 border border-[#DC143C] flex items-center justify-center">
                <span className="text-[10px] font-black text-[#DC143C] tracking-tighter">W</span>
              </div>
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

          <div className="grid grid-cols-4 gap-1.5 mb-3">
            {providers.map(p => {
              const isSelected = provider === p.id
              return (
                <motion.button
                  key={p.id}
                  whileHover={{ scale: 1.02 }}
                  whileTap={{ scale: 0.98 }}
                  onClick={() => { setProvider(p.id); setModel(modelsByProvider[p.id][0]) }}
                  className={`p-2 border text-left transition-all ${
                    isSelected
                      ? 'bg-[#1a1a1a] border-[#DC143C]'
                      : 'bg-[#0d0d0d] border-[#1a1a1a] opacity-50 hover:opacity-100'
                  }`}
                >
                  <div className="flex items-center gap-1.5 mb-0.5">
                    <span className="material-symbols-outlined text-xs" style={{ color: isSelected ? '#DC143C' : '#555' }}>{p.icon}</span>
                    <span className="text-[9px] font-bold text-white uppercase tracking-tight">{providerNames[p.id]}</span>
                  </div>
                  <p className="text-[7px] text-[#555] leading-tight">{p.desc}</p>
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
                <div className="mb-3">
                  <label className="text-[8px] font-bold text-[#FFB000] uppercase tracking-widest block mb-1">
                    API Key
                  </label>
                  <div className="relative flex">
                    <input
                      type={showKey ? 'text' : 'password'}
                      placeholder={provider === 'gemini' ? 'AIza...' : provider === 'openai' ? 'sk-...' : 'sk-ant-...'}
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
                </div>
              )}

              <div className="mb-3">
                <label className="text-[8px] font-bold text-[#00E5FF] uppercase tracking-widest block mb-1">Model</label>
                <div className="flex flex-wrap gap-1">
                  {(modelsByProvider[provider] || []).map(m => (
                    <motion.button
                      key={m}
                      whileHover={{ scale: 1.02 }}
                      whileTap={{ scale: 0.98 }}
                      onClick={() => setModel(m)}
                      className={`px-2 py-0.5 text-[8px] font-bold uppercase tracking-wider border transition-all ${
                        model === m
                          ? 'bg-[#DC143C] text-white border-[#DC143C]'
                          : 'bg-[#0d0d0d] text-[#777] border-[#1a1a1a] hover:border-[#DC143C] hover:text-white'
                      }`}
                    >{m}</motion.button>
                  ))}
                </div>
              </div>
            </motion.div>
          </AnimatePresence>

          <div className="flex items-center gap-2">
            <motion.button
              whileHover={{ scale: 1.01 }}
              whileTap={{ scale: 0.99 }}
              onClick={handleTestConnection}
              disabled={status === 'testing' || (provider !== 'ollama' && !apiKey)}
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
