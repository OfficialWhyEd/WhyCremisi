# WhyCremisi Mask — Audio-Reactive Logo Specification

## Posizionamento nel Plugin

Il logo verrà posizionato **affianco al controllo Master Volume**, nella barra superiore del plugin (header strip). È il punto di maggiore visibilità e il meno affollato della UI.

```
┌────────────────────────────────────────────────────────┐
│  [LOGO]  WhyCremisi       MASTER VOL ●────  [≡] [⚙]  │
└────────────────────────────────────────────────────────┘
```

- Dimensioni suggerite in UI: **48×48 px** (o 40×40 in layout compatto)
- Bordo: nessuno — la maschera su sfondo nero si integra naturalmente con il dark theme
- La maschera NON tocca mai la chat AI né i controlli parametrici

---

## Comportamento Audio-Reattivo

### Concetto

La mascella della maschera si apre in sincronia con il segnale audio in ingresso. L'effetto è quello di una mascella meccanica che "parla" con la musica — precisa, immediata, organica.

### Anatomy della Maschera

La maschera ha tre zone animate:

```
    ┌──────────────────────┐
    │       TESTA          │  ← statica (poligoni grigi)
    │   [OCCHIO] [OCCHIO]  │  ← occhi: glow pulsante
    │                      │
    │ ─ ─ ─ ─ ─ ─ ─ ─ ─  │  ← FESSURA (mouth slot)
    │     PIASTRA JAW      │  ← si abbassa con il suono
    └──────────────────────┘
```

- **Testa + brow**: sempre statica
- **Occhi**: intensità del glow aumenta con i picchi RMS
- **Jaw plate**: si abbassa verticalmente in proporzione al livello audio

### Dati Audio → Animazione

| Sorgente dati | Valore | Uso |
|---|---|---|
| `meterL` / `meterR` | 0.0 – 1.0 | Ampiezza per apertura mascella |
| Media dei due meter | `(L + R) / 2` | Input principale |
| Smoothing | blend 0.4/0.6 | Risposta fluida ma reattiva |
| Soglia silenzio | < 0.03 | Nessun movimento sotto questa soglia |

### Calcolo Apertura Mascella

```js
// Smooth del livello audio (simile a BotFace)
smoothLevel = smoothLevel * 0.60 + rawLevel * 0.40

// Apertura mascella (px di traslazione verso il basso)
const JAW_MAX_OPEN = 10   // pixel massimi di apertura
const jawOffset = smoothLevel > 0.03
  ? smoothLevel * JAW_MAX_OPEN
  : 0

// Applicato come transform al gruppo SVG della jaw plate
// transform="translate(0, {jawOffset})"
```

### Glow degli Occhi

```js
// I filter blur degli occhi scalano con i picchi
const BASE_BLUR = 9    // px blur a riposo
const MAX_BLUR  = 18   // px blur al picco

const eyeBlur = BASE_BLUR + smoothLevel * (MAX_BLUR - BASE_BLUR)
// Applicato al <filter> id="eye-glow" → feGaussianBlur stdDeviation
```

---

## Struttura SVG per l'Animazione

La maschera SVG per il plugin sarà strutturata in **gruppi separati**:

```xml
<svg viewBox="0 0 400 400">

  <!-- TESTA (statica) -->
  <g id="head-static">
    <!-- tutti i poligoni del cranio e guance -->
  </g>

  <!-- OCCHI (glow variabile) -->
  <g id="eyes">
    <filter id="eye-glow">
      <feGaussianBlur stdDeviation="{eyeBlur}"/> <!-- animato -->
    </filter>
    <!-- esagoni occhi -->
  </g>

  <!-- PIASTRA JAW (si abbassa) -->
  <g id="jaw-plate" transform="translate(0, {jawOffset})">
    <!-- poligono jaw guard + mouth slot -->
  </g>

</svg>
```

In React (Framer Motion), i `motion.g` gestiranno `y` e `filter` in modo fluido:

```jsx
<motion.g
  id="jaw-plate"
  animate={{ y: jawOffset }}
  transition={{ duration: 0.05, ease: 'linear' }}
/>
```

---

## Note di Implementazione (per dopo)

1. **Non usare l'immagine JPG** per la versione animata — serve un SVG nativo con `<g>` separati
2. Il file `svg/whycremisi-mask.svg` contiene il logo embedded (per icone statiche)
3. Per la versione animata nel plugin, creare `whycremisi-mask-animated.svg` partendo dalla struttura a gruppi sopra
4. Il componente React sarà `<MaskLogo audioLevel={...} />` affiancato al `MasterVolumeKnob`
5. La bocca si apre sulla jaw plate — la fessura (mouth slot) diventa visibile quando la piastra scende
6. La transizione deve essere **fast** (< 60ms) per sentire la risposta alla musica in tempo reale

---

## Riferimenti

- Logo sorgente: `Loghi Ufficiali/whycremisi-mask-primary.jpg`
- App icon: `Loghi Ufficiali/whycremisi-mask-appicon.jpg`
- Audio level: `meterL`, `meterR` in `App.jsx` (già presenti, aggiornati a 30fps via WebSocket `daw.meter`)
- Esempio analogo già implementato: `BotFace.jsx` → `audioLevel` prop + `smoothAudio` state
