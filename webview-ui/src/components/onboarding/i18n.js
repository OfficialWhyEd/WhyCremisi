// Testi del primo avvio, italiano e inglese. Il resto dell'interfaccia ha
// ancora le stringhe scritte nei componenti: quando verra' rifatta usera'
// questo stesso meccanismo (guessLang, t).

const LANG_KEY = 'whycremisi_lang'

export function guessLang() {
  try {
    const saved = localStorage.getItem(LANG_KEY)
    if (saved === 'it' || saved === 'en') return { lang: saved, guessed: false }
  } catch { /* archivio non disponibile */ }
  const sys = (navigator.languages?.[0] || navigator.language || 'en').toLowerCase()
  return { lang: sys.startsWith('it') ? 'it' : 'en', guessed: true }
}

export function saveLang(lang) {
  try { localStorage.setItem(LANG_KEY, lang) } catch { /* ignora */ }
}

const STR = {
  it: {
    brand: 'WhyCremisi',
    back: 'Indietro', next: 'Continua', skip: 'Salta',
    stepOf: (n, tot) => `Passo ${n} di ${tot}`,
    steps: { welcome: 'Benvenuto', access: 'Accesso', connect: 'Collegamento', test: 'Prova', daw: 'DAW', ready: 'Pronto' },
    editLater: 'Tutto si cambia poi dalle impostazioni.',

    welcome: {
      title: 'Un collaboratore dentro la tua DAW.',
      body: 'WhyCremisi ascolta la sessione, capisce cosa chiedi e muove i parametri insieme a te.',
      lang: 'Lingua', langGuess: 'Scelta dal tuo sistema. Cambiala quando vuoi.', langSet: 'Puoi cambiarla dalle impostazioni.',
      start: 'Iniziamo', time: 'Servono due minuti.',
    },

    access: {
      title: 'Come vuoi collegarlo?',
      sub: 'Scegli una strada. Si cambia quando vuoi.',
      need: 'Serve', cost: 'Costo', privacy: 'Privacy',
      free: {
        name: 'Gratis', tag: 'Nessun pagamento, nessun abbonamento.',
        need: 'Ollama sul tuo PC, oppure una chiave gratuita',
        cost: 'Zero', privacy: 'Con Ollama nulla lascia il tuo PC',
      },
      subscription: {
        name: 'Abbonamento', tag: 'Entra con l’abbonamento Claude che hai già.',
        need: 'Claude Pro o Max, con Claude Code',
        cost: 'Quello che paghi già', privacy: 'I messaggi vanno ad Anthropic',
      },
      apikey: {
        name: 'Chiave API', tag: 'Scegli il fornitore e incolla la tua chiave.',
        need: 'Una chiave Anthropic, OpenAI, Gemini, Groq o OpenRouter',
        cost: 'A consumo: paghi quello che usi', privacy: 'I messaggi vanno al fornitore scelto',
      },
      soon: 'ChatGPT e Codex con abbonamento',
      soonTag: 'In arrivo',
    },

    connect: {
      free: { title: 'Scegli come restare gratis', sub: 'Tre modi, tutti senza spendere.' },
      subscription: { title: 'Entra con il tuo abbonamento', sub: 'Cerco un accesso Claude su questo PC.' },
      apikey: { title: 'Incolla la tua chiave', sub: 'Resta sul tuo PC, nel plugin.' },
      kinds: {
        ollama: { name: 'Ollama', note: 'Sul tuo PC' },
        openrouter: { name: 'OpenRouter', note: 'Modelli :free' },
        gemini: { name: 'Gemini', note: 'Chiave gratuita' },
      },
      provider: 'Fornitore',
      key: 'Chiave', show: 'Mostra', hide: 'Nascondi',
      keyHint: (p) => `Di solito inizia con ${p}`,
      getKey: 'Apri la pagina per ottenerla',
      getKeyAt: (u) => `Si prende su ${u}`,
      model: 'Modello', options: 'Modello e opzioni', effort: 'Ragionamento',
      effortNote: 'Più alto vuol dire risposte migliori e più token.',
      freeOnly: 'Solo modelli gratuiti.',
      offline: 'Questa finestra non è collegata al plugin, quindi non posso cercare nulla. Apri WhyCremisi come plugin o standalone.',

      ollama: {
        searching: 'Cerco Ollama sul tuo PC…',
        found: (n) => `Ollama c’è. ${n} ${n === 1 ? 'modello installato' : 'modelli installati'}.`,
        foundEmpty: 'Ollama c’è, ma non ha modelli. Scarica il primo da un terminale.',
        pullCmd: 'ollama pull llama3.2',
        missing: 'Non trovo Ollama.', missingHow: 'Installalo e avvialo, poi controlla di nuovo.',
        get: 'Si scarica da ollama.com/download', again: 'Controlla di nuovo',
      },
      sub: {
        searching: 'Cerco un abbonamento su questo PC…',
        found: 'Abbonamento trovato.', foundNote: 'Nessuna chiave da incollare.',
        link: 'Collega con Claude Code', linking: 'Approva nel browser…',
        linkNote: 'Claude Code è installato: si apre il browser, approvi e torni qui.',
        linkDone: 'Account collegato.',
        none: 'Non trovo un abbonamento.',
        noneHow: 'Installa Claude Code e accedi con il tuo account, oppure incolla un token.',
        token: 'Token dell’abbonamento', again: 'Cerca di nuovo',
      },
    },

    test: {
      title: 'Una prova vera',
      sub: 'Mando un messaggio al modello e aspetto la risposta.',
      steps: { apply: 'Applico le impostazioni', auth: 'Verifico l’accesso', prompt: 'Messaggio di prova al modello' },
      running: 'In corso', ok: 'Riuscito', fail: 'Non riuscito',
      replyFrom: (m, s) => `${m} ha risposto in ${s} s`,
      okTitle: 'Funziona.',
      failTitle: 'Qualcosa non va.',
      why: 'Motivo', todo: 'Cosa fare',
      retry: 'Riprova', change: 'Cambia accesso', continueAnyway: 'Continua senza prova',
      offline: 'Questa finestra non è collegata al plugin.',
      offlineDo: 'Apri WhyCremisi come plugin o standalone. Puoi continuare, ma l’accesso resterà non verificato.',
      timeout: 'Nessuna risposta entro il tempo massimo.',
      noReply: 'Il modello non ha risposto.',
      prompt: 'Rispondi con una sola parola: pronto.',
      hints: {
        key: 'La chiave o il token è stato rifiutato. Controllala e incollala di nuovo.',
        quota: 'Quota o limite raggiunto. Aspetta un po’ oppure scegli un altro modello.',
        ollama: 'Avvia Ollama e riprova. Il modello scelto deve essere scaricato.',
        model: 'Il modello non esiste per questo accesso. Scegline un altro.',
        network: 'Il servizio non risponde. Controlla la connessione e riprova.',
        generic: 'Riprova. Se continua, cambia modello o accesso.',
      },
    },

    daw: {
      title: 'Collegamento alla DAW',
      sub: 'Serve per muovere i parametri, non per chiacchierare.',
      checking: 'Controllo la DAW…',
      connected: 'DAW collegata', connectedNote: 'Sto ricevendo il suo trasporto.',
      silent: (d) => `${d} trovata, ma non risponde.`, none: 'Nessuna DAW collegata.',
      again: 'Controlla di nuovo', later: 'Si può fare dopo. Il chat funziona anche senza.',
      how: {
        ableton: ['Installa AbletonOSC (github.com/ideoforms/AbletonOSC) tra i Remote Script.', 'In Live: Impostazioni, Link/Tempo/MIDI, scegli AbletonOSC come Control Surface.'],
        reaper: ['In REAPER: Preferenze, Control Surfaces, Aggiungi, OSC.', 'Porta locale 9000, schema ReaperOSC.'],
        other: ['Attiva OSC nella tua DAW (porta 9000 in ascolto).
