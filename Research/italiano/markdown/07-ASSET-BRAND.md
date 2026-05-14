# Paper 07 — Asset e Brand Identity
## Logo, Icone e Specifiche Grafiche per Tutte le Piattaforme

```
────────────────────────────────────────────────────────────────
  WHYCREMISI RESEARCH PAPERS — N.07
  Brand Identity e Asset Grafici
  
  "L'identità visiva è la prima impressione."
────────────────────────────────────────────────────────────────
```

**Categoria:** Design e Brand

---

## 1. Identità Visiva

### Colori Primari

```
  ┌──────────────────────────────────────────────────────┐
  │                                                      │
  │   ████████  Cremisi       #DC143C   rgb(220,20,60)   │
  │   ████████  Amber         #FFB000   rgb(255,176,0)   │
  │   ████████  Nero profondo #0d0d0d   rgb(13,13,13)    │
  │   ████████  Bianco caldo  #E5E2E1   rgb(229,226,225) │
  │                                                      │
  │   Accenti:                                           │
  │   ████████  Cyan AI       #00E5FF   rgb(0,229,255)   │
  │   ████████  Verde OK      #00FFaa   rgb(0,255,170)   │
  │                                                      │
  └──────────────────────────────────────────────────────┘
```

### Tipografia

```
  Primario:    Space Grotesk — Bold, tracking stretto
               Usato per: titoli, etichette UI, brand name

  Secondario:  JetBrains Mono (o Space Mono)
               Usato per: valori numerici, codice, dati tecnici

  Fallback:    system-ui, -apple-system, sans-serif
```

### Carattere del Brand

```
  Aggettivi:   Preciso · Vivo · Industriale · Moderno
               Professionale · Intelligente · Caldo
  
  Non è:       Corporate · Freddo · Playful · Semplice
```

---

## 2. Logo — Specifiche

### Concept del Logo

Il logo combina la lettera **W** stilizzata con un'onda sinusoidale (waveform) che attraversa le barre verticali — evocando sia il nome "Why" sia la natura audio del prodotto.

```
  LOGO CONCEPT (rappresentazione ASCII):
  
  ╔═══════════════════════════════════════╗
  ║                                       ║
  ║    ██   ██  ██   ██  ██████           ║
  ║    ██   ██  ██   ██  ██               ║
  ║    ██ █ ██  ███████  ██████           ║
  ║    ███████  ██   ██  ██               ║
  ║     ██ ██   ██   ██  ██               ║
  ║                                       ║
  ║   ∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿       ║
  ║   CREMISI                             ║
  ║                                       ║
  ╚═══════════════════════════════════════╝
  
  Nota: Il logo definitivo va realizzato in Figma/Illustrator
        da un designer grafico partendo da queste specifiche.
```

### Varianti Richieste

```
  VARIANTE                    SFONDO        USO
  ──────────────────────────  ────────      ──────────────────────────
  Logo completo               Trasparente   Marketing, sito, print
  Logo completo               Nero #0d0d0d  App, UI, dark contexts
  Logo completo               Bianco        Stampa, materiali chiari
  Icona sola (W + onda)       Trasparente   Favicon, icona app piccola
  Wordmark solo               Trasparente   Co-branding, header testo
  Logo monocromatico          Trasparente   Uso su colori arbitrari
```

---

## 3. Formati Icona per Piattaforma

### macOS — Formato .icns

```
  File: WhyCremisi.icns
  
  Dimensioni obbligatorie (@1x e @2x Retina):
  
  16×16 px      icon_16x16.png
  16×16 px @2x  icon_16x16@2x.png    (= 32px)
  32×32 px      icon_32x32.png
  32×32 px @2x  icon_32x32@2x.png    (= 64px)
  128×128 px    icon_128x128.png
  128×128 @2x   icon_128x128@2x.png  (= 256px)
  256×256 px    icon_256x256.png
  256×256 @2x   icon_256x256@2x.png  (= 512px)
  512×512 px    icon_512x512.png
  512×512 @2x   icon_512x512@2x.png  (= 1024px)
  
  Strumento: iconutil (macOS) da cartella .iconset
  Comando:   iconutil -c icns WhyCremisi.iconset
```

### Windows — Formato .ico

```
  File: WhyCremisi.ico
  
  Dimensioni (tutte in un singolo .ico multi-size):
  
  16×16 px    (System tray, piccola)
  24×24 px    (Toolbar)
  32×32 px    (Desktop, standard)
  48×48 px    (Windows Explorer)
  64×64 px    (Taskbar)
  128×128 px  (High DPI)
  256×256 px  (PNG compresso dentro .ico)
  
  Strumento: ImageMagick o IcoFX
  Comando:   magick convert icon-256.png -resize 256x256 
             icon-128.png icon-64.png icon-48.png 
             icon-32.png icon-16.png WhyCremisi.ico
```

### Linux — Formati Standard XDG

```
  Directory: /usr/share/icons/hicolor/
  
  /16x16/apps/whycremisi.png
  /24x24/apps/whycremisi.png
  /32x32/apps/whycremisi.png
  /48x48/apps/whycremisi.png
  /64x64/apps/whycremisi.png
  /128x128/apps/whycremisi.png
  /256x256/apps/whycremisi.png
  /512x512/apps/whycremisi.png
  /scalable/apps/whycremisi.svg   ← vettoriale preferito
```

### Web — Favicon e PWA

```
  favicon.ico          16×16 + 32×32 multi-size
  favicon-16x16.png
  favicon-32x32.png
  apple-touch-icon.png             180×180 (iOS home screen)
  android-chrome-192x192.png       (Android)
  android-chrome-512x512.png       (Android splash)
  
  manifest.json:
  {
    "name": "WhyCremisi",
    "short_name": "WHY",
    "icons": [ ... ],
    "theme_color": "#DC143C",
    "background_color": "#0d0d0d"
  }
```

### Plugin DAW — Thumbnail

```
  VST3 Plugin Thumbnail:
  128×128 px PNG — mostrato nel browser plugin della DAW
  Sfondo: #0d0d0d con logo centered, padding 16px
```

---

## 4. Splash Screen

```
  Standalone app — splash al caricamento:
  
  Dimensioni: 600×400 px
  Contenuto:
    - Logo centrato (200px width)
    - "Versione 1.0.0" sotto
    - Barra di caricamento animata in cremisi
    - Copyright in basso
  
  Formato: PNG + animazione gestita via React/CSS
  Durata: max 2 secondi, poi fade-out automatico
```

---

## 5. File Sorgente da Produrre

```
  VETTORIALI (Illustrator / Figma / Inkscape)
  ──────────────────────────────────────────
  WhyCremisi-logo-full.svg          Logo completo vettoriale
  WhyCremisi-logo-icon.svg          Solo icona W+onda
  WhyCremisi-logo-wordmark.svg      Solo testo
  WhyCremisi-logo-mono.svg          Versione monocromatica
  
  EXPORT AUTOMATICO DA VETTORIALE
  ──────────────────────────────────────────
  → .icns    (macOS, da script iconutil)
  → .ico     (Windows, da ImageMagick)
  → .png     (tutte le dimensioni, da export Figma/script)
  → .svg     (web, già pronto)
```

---

## 6. Tono della Comunicazione

```
  NAMING PRODOTTO
  
  Corretto:     WhyCremisi (una parola, W e C maiuscole)
  Abbreviato:   WHY (tutto maiuscolo, nei contesti brevi)
  Mai:          whycremisi / WHYCREMISI / Why-Cremisi
  
  TAGLINE UFFICIALI (scegliere una)
  
  "The AI that knows your music."
  "L'intelligenza che governa il suono."
  "Not a tool. A collaborator."
  "Every plugin. Every parameter. One AI."
```

---

*Fine dei Research Papers — Versione Italiana*  
*Per la versione inglese: → `/Research/EN/`*
