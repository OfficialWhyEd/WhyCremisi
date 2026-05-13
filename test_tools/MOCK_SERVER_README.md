# 🧪 Mock Server OSC — Spiegazione Semplice

## Cos'è?

Un **programmino finto** che finge di essere Reaper o Ableton.

Invece di aprire il DAW vero, lanci questo script e **sembra** che ci sia un DAW che invia messaggi al plugin.

---

## A cosa serve?

| Problema | Soluzione |
|----------|-----------|
| Heartbroken non ha il plugin C++ di Edo pronto | Usa il mock server per testare la UI |
| Edo non vuole aprire Ableton ogni volta | Usa il mock per vedere se i messaggi arrivano |
| Volete testare la connessione senza DAW aperto | Il mock invia dati falsi ma realistici |

---

## Come funziona?

Il programma **inventa** messaggi tipo:
- "La Track 1 ha volume 0.75"
- "La Track 1 ha pan -0.5"
- "È stato premuto Play"

Li invia **come se** venissero da Reaper, ma in realtà sono generati da questo script.

---

## Come si usa?

### 1. Installare dipendenza (una sola volta)
```bash
pip install python-osc
```

### 2. Avviare il mock server
```bash
cd /path/to/VST-PlugIn-Ai/Tools
python3 mock_osc_server.py
```

### 3. Vedere i messaggi arrivare
Il programma stampa:
```
🎵 Mock DAW Server avviato
Invio messaggi OSC falsi a 127.0.0.1:9000
📤 Messaggio 0: Volume Track 1 = 0.75
📤 Messaggio 1: Pan Track 1 = -0.42
📤 Messaggio 2: Volume Track 1 = 0.83
...
```

### 4. Fermare
Premi `CTRL+C` nel terminale.

---

## ⚠️ Importante

Questo **NON** è un vero DAW.
- Non fa musica
- Non salva progetti
- Non sostituisce Reaper o Ableton

**Serve solo per testare** che il plugin riceve messaggi correttamente.

---

## Per chi è utile

### Heartbroken (React)
- Puoi testare la UI senza aspettare che Edo finisca il C++
- Vedi arrivare dati, testi la toolbox modulare
- Verifichi che i widget si aggiornano

### Edo (C++)
- Puoi verificare che il tuo codice riceve OSC
- Senza aprire Ableton ogni volta
- Più veloce per debug

---

Creato da Aura per testare il plugin VST senza DAW vero.
