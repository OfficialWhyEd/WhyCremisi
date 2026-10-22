// Catalogo di cio' che il plugin sa davvero fare (verificato in src/ai:
// AiEngine::Provider = Ollama, Gemini, Anthropic, OpenAI, OpenRouter, Groq).
// Se un fornitore non e' qui, il C++ non lo gestisce: non va mostrato.

export const PROVIDERS = {
  ollama: {
    id: 'ollama', name: 'Ollama', icon: 'memory',
    models: ['llama3.2', 'llama3.1', 'mistral', 'qwen2.5'],
    keyUrl: 'ollama.com/download',
  },
  openrouter: {
    id: 'openrouter', name: 'OpenRouter', icon: 'hub',
    prefix: 'sk-or-', keyUrl: 'openrouter.ai/keys',
    // Gli stessi modelli gratuiti che il C++ propone (OpenRouterProvider::freeModels).
    models: [
      'deepseek/deepseek-r1:free',
      'meta-llama/llama-3.3-70b-instruct:free',
      'qwen/qwen-2.5-72b-instruct:free',
      'google/gemma-2-9b-it:free',
    ],
    paidModels: ['anthropic/claude-sonnet-4.5', 'openai/gpt-4o', 'google/gemini-2.0-flash-001'],
  },
  gemini: {
    id: 'gemini', name: 'Google Gemini', icon: 'cloud_queue',
    prefix: 'AIza', keyUrl: 'aistudio.google.com/app/apikey',
    models: ['gemini-2.0-flash', 'gemini-3.1-flash-lite'],
  },
  anthropic: {
    id: 'anthropic', name: 'Anthropic (Claude)', icon: 'psychology',
    prefix: 'sk-ant-', keyUrl: 'console.anthropic.com/settings/keys',
    models: ['claude-sonnet-5', 'claude-opus-5', 'claude-haiku-4-5'],
  },
  openai: {
    id: 'openai', name: 'OpenAI', icon: 'auto_awesome',
    prefix: 'sk-', keyUrl: 'platform.openai.com/api-keys',
    models: ['gpt-4o', 'gpt-4o-mini'],
  },
  groq: {
    id: 'groq', name: 'Groq', icon: 'bolt',
    prefix: 'gsk_', keyUrl: 'console.groq.com/keys',
    models: ['llama-3.3-70b-versatile', 'llama-3.1-8b-instant'],
  },
  llamacpp: {
    id: 'llamacpp', name: 'llama.cpp', icon: 'dns',
    // Nessuna chiave e nessun download: parla con il llama-server che gira
    // gia' sulla macchina. L'elenco vero dei modelli lo da' /v1/models.
    models: [],
    keyUrl: '',
  },
}

// Chiave API: l'ordine in cui compaiono.
export const API_KEY_PROVIDERS = ['anthropic', 'openai', 'gemini', 'groq', 'openrouter']
// Strada gratuita: tre modi, tutti realmente collegati.
export const FREE_KINDS = ['ollama', 'openrouter', 'gemini', 'llamacpp']

export const MODEL_LABELS = {
  'claude-opus-5': 'Opus 5',
  'claude-sonnet-5': 'Sonnet 5',
  'claude-haiku-4-5': 'Haiku 4.5',
}
export const modelLabel = (m) => MODEL_LABELS[m] || m.replace(':free', '').split('/').pop()
export const isFree = (m) => m.endsWith(':free')

export const EFFORTS = ['low', 'medium', 'high', 'xhigh', 'max']

// Fornitore effettivo in base alla strada scelta.
export function providerFor(form) {
  if (form.path === 'subscription') return 'anthropic'
  if (form.path === 'free') return form.freeKind
  return form.apiProvider
}

export function defaultModel(providerId) {
  return PROVIDERS[providerId].models[0]
}

export function keyLooksRight(providerId, key) {
  const p = PROVIDERS[providerId]
  return !!key && key.length >= 20 && (!p.prefix || key.startsWith(p.prefix))
}

export function needsKey(form) {
  const id = providerFor(form)
  return form.path === 'apikey' || (form.path === 'free' && id !== 'ollama')
}
