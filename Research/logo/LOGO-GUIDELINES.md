# WhyCremisi — Logo System & Icon Specifications

```
────────────────────────────────────────────────────────────────
  WHYCREMISI · LOGO SYSTEM
  Complete Size Guide · Platform Specs · Build Instructions
────────────────────────────────────────────────────────────────
```

---

## Come usare questo sistema

Metti il tuo logo sorgente in questa cartella come `source.png` (o `source.svg`)  
poi esegui:

```bash
node build-icons.js
```

Lo script genera automaticamente **tutte** le varianti per tutte le piattaforme.

**Requisiti del file sorgente:**
- Formato: PNG o SVG
- Dimensione minima: **1024×1024 px**
- Sfondo: trasparente (PNG-32 con alpha)
- Contenuto: solo il segno/mark, senza testo (le varianti con testo si aggiungono dopo)

---

## 1. Apple / macOS — Liquid Glass Style

A partire da macOS 26 (Sequoia), Apple usa il linguaggio **Liquid Glass**: icone con profondità, traslucenza, rifrazione della luce e ombre morbide. Il tuo logo viene incapsulato in un contenitore vetro con effetti luminosi sopra.

### Caratteristiche Liquid Glass
```
  ┌─────────────────────────────────────────────┐
  │                                             │
  │   Highlight speculare (bianco, 18% opacità) │
  │   ┌─────────────────────────────────────┐   │
  │   │         [LOGO MARK]                 │   │
  │   │    traslucente · depth · refraction │   │
  │   └─────────────────────────────────────┘   │
  │   Ombra morbida sotto (nero, 30% opacità)   │
  │                                             │
  └─────────────────────────────────────────────┘

  Corner radius:  22.5% della dimensione (squircle continuo)
  Background:     #0d0d0d con blur 20px sottostante
  Highlight:      gradiente bianco top → trasparente, 12%
  Inner glow:     bordo interno #DC143C, 1px, 25% opacità
  Drop shadow:    0 8px 32px rgba(0,0,0,0.55)
```

### Tutte le dimensioni macOS

| File | Dimensione | Scale | Uso |
|------|-----------|-------|-----|
| `icon_16x16.png` | 16×16 px | @1x | Menu bar, Finder piccolo |
| `icon_16x16@2x.png` | 32×32 px | @2x | Menu bar retina |
| `icon_32x32.png` | 32×32 px | @1x | Finder standard |
| `icon_32x32@2x.png` | 64×64 px | @2x | Finder retina |
| `icon_128x128.png` | 128×128 px | @1x | Finder grande |
| `icon_128x128@2x.png` | 256×256 px | @2x | Finder grande retina |
| `icon_256x256.png` | 256×256 px | @1x | Spotlight, Quick Look |
| `icon_256x256@2x.png` | 512×512 px | @2x | Spotlight retina |
| `icon_512x512.png` | 512×512 px | @1x | App Store |
| `icon_512x512@2x.png` | 1024×1024 px | @2x | App Store retina |

**Output finale:** `AppIcon.icns` (bundle di tutti i PNG sopra, generato da `iconutil`)

### Corner Radius per dimensione (Liquid Glass)
| Dimensione | Radius | % |
|-----------|--------|---|
| 16px | 4px | 25% |
| 32px | 7px | 22% |
| 64px | 14px | 22% |
| 128px | 29px | 22.5% |
| 256px | 58px | 22.5% |
| 512px | 115px | 22.5% |
| 1024px | 230px | 22.5% |

---

## 2. Windows — Fluent Design Style

Windows 11 usa **Fluent Design**: icone piatte con angoli arrotondati moderati, colori vivaci, nessun vetro pesante. Il logo su sfondo solido `#0d0d0d` con corner radius del 20%.

### Caratteristiche Windows Fluent
```
  ┌──────────────────────────────────────────┐
  │  Background: #0d0d0d (piatto, solido)    │
  │  Corner radius: 20% della dimensione     │
  │  Border: nessuno                         │
  │  Shadow: nessuna (Fluent è flat)         │
  │  Logo mark: centrato, 72% dello spazio   │
  └──────────────────────────────────────────┘
```

### Tutte le dimensioni Windows

| File | Dimensione | Uso |
|------|-----------|-----|
| `win-16.png` | 16×16 px | Taskbar piccolo |
| `win-20.png` | 20×20 px | Taskbar medio |
| `win-24.png` | 24×24 px | Taskbar grande |
| `win-32.png` | 32×32 px | Desktop shortcut |
| `win-40.png` | 40×40 px | Start menu tile |
| `win-48.png` | 48×48 px | Desktop icona standard |
| `win-64.png` | 64×64 px | Taskbar @150% DPI |
| `win-96.png` | 96×96 px | Taskbar @200% DPI |
| `win-128.png` | 128×128 px | Explorer preview |
| `win-256.png` | 256×256 px | High-DPI / Store |

**Output finale:** `AppIcon.ico` (bundle multi-size, generato dallo script)

### Formati aggiuntivi Windows (Store / Modern)
| File | Dimensione | Uso |
|------|-----------|-----|
| `StoreLogo.png` | 50×50 px | Microsoft Store |
| `Square44x44Logo.png` | 44×44 px | App list |
| `Square150x150Logo.png` | 150×150 px | Start menu tile grande |
| `Square310x310Logo.png` | 310×310 px | Start menu tile jumbo |
| `Wide310x150Logo.png` | 310×150 px | Start menu tile wide |

---

## 3. Linux — Freedesktop Standard

Linux segue lo standard **freedesktop.org**: icone piatte SVG scalabili + PNG per ogni dimensione. Nessun effetto piattaforma, design pulito e neutro. Il logo su sfondo trasparente è preferito.

### Caratteristiche Linux
```
  Background: trasparente (preferito) o #0d0d0d flat
  Corner radius: 0% (quadrato puro) o massimo 10%
  Effetti: nessuno — solo il mark pulito
  SVG: scalabile infinitamente (priorità massima)
```

### Tutte le dimensioni Linux

| File | Dimensione | Path di installazione |
|------|-----------|----------------------|
| `linux-16.png` | 16×16 px | `/usr/share/icons/hicolor/16x16/apps/` |
| `linux-22.png` | 22×22 px | `/usr/share/icons/hicolor/22x22/apps/` |
| `linux-24.png` | 24×24 px | `/usr/share/icons/hicolor/24x24/apps/` |
| `linux-32.png` | 32×32 px | `/usr/share/icons/hicolor/32x32/apps/` |
| `linux-48.png` | 48×48 px | `/usr/share/icons/hicolor/48x48/apps/` |
| `linux-64.png` | 64×64 px | `/usr/share/icons/hicolor/64x64/apps/` |
| `linux-96.png` | 96×96 px | `/usr/share/icons/hicolor/96x96/apps/` |
| `linux-128.png` | 128×128 px | `/usr/share/icons/hicolor/128x128/apps/` |
| `linux-256.png` | 256×256 px | `/usr/share/icons/hicolor/256x256/apps/` |
| `linux-512.png` | 512×512 px | `/usr/share/icons/hicolor/512x512/apps/` |
| `whycremisi.svg` | scalabile | `/usr/share/icons/hicolor/scalable/apps/` |

---

## 4. Favicon — Web & DAW

### Favicon complete set

| File | Dimensione | Uso |
|------|-----------|-----|
| `favicon-16x16.png` | 16×16 px | Browser tab standard |
| `favicon-32x32.png` | 32×32 px | Browser tab retina / shortcut |
| `favicon-48x48.png` | 48×48 px | Windows site shortcut |
| `favicon-96x96.png` | 96×96 px | Android Chrome |
| `favicon-180x180.png` | 180×180 px | Apple Touch Icon (iPhone/iPad) |
| `favicon-192x192.png` | 192×192 px | Android Chrome splash |
| `favicon-512x512.png` | 512×512 px | PWA / Android maskable |
| `favicon.ico` | 16+32+48 px | Bundle ICO universale |
| `safari-pinned-tab.svg` | scalabile | Safari pinned tab (monocromatico) |

### HTML da inserire nel `<head>` del sito
```html
<link rel="icon" type="image/png" sizes="32x32" href="/favicon-32x32.png">
<link rel="icon" type="image/png" sizes="16x16" href="/favicon-16x16.png">
<link rel="apple-touch-icon" sizes="180x180" href="/favicon-180x180.png">
<link rel="manifest" href="/site.webmanifest">
<meta name="theme-color" content="#0d0d0d">
```

### site.webmanifest
```json
{
  "name": "WhyCremisi",
  "short_name": "WhyCremisi",
  "icons": [
    { "src": "/favicon-192x192.png", "sizes": "192x192", "type": "image/png" },
    { "src": "/favicon-512x512.png", "sizes": "512x512", "type": "image/png" }
  ],
  "theme_color": "#DC143C",
  "background_color": "#0d0d0d",
  "display": "standalone"
}
```

---

## 5. DAW & Plugin

| File | Dimensione | Uso |
|------|-----------|-----|
| `daw-thumb.png` | 256×256 px | Plugin manager thumbnail (tutti i DAW) |
| `daw-thumb-small.png` | 128×128 px | Plugin rack view |
| `og-image.png` | 1200×630 px | Social sharing (Twitter, Facebook, Discord) |
| `og-image-square.png` | 1200×1200 px | Instagram / LinkedIn |

---

## 6. Varianti del logo

| File | Contenuto | Sfondo | Uso |
|------|----------|--------|-----|
| `logo-full-color.svg` | Mark + Wordmark + Tagline | trasparente | Website, marketing |
| `logo-compact-color.svg` | Mark + Wordmark | trasparente | Header plugin, email |
| `logo-mark-color.svg` | Solo mark ◈ | trasparente | App icon base |
| `logo-mono-white.svg` | Mark + Wordmark bianco | trasparente | Su sfondi scuri |
| `logo-mono-black.svg` | Mark + Wordmark nero | trasparente | Stampa, contratti |
| `logo-reversed.svg` | Mark cremisi su nero | #0d0d0d | Materiale promozionale |

---

## 7. Come funziona lo script

Posiziona il file sorgente e lancia:

```bash
# Con PNG sorgente
cp tuo-logo.png source.png
node build-icons.js

# Con SVG sorgente  
cp tuo-logo.svg source.svg
node build-icons.js
```

Lo script:
1. Legge `source.png` o `source.svg`
2. Genera tutte le dimensioni PNG per macOS, Windows, Linux, Web
3. Applica il trattamento corretto per piattaforma:
   - **macOS**: squircle + effetto Liquid Glass (highlight + ombra)
   - **Windows**: corner radius 20%, sfondo piatto
   - **Linux**: trasparente, quadrato o minimo radius
   - **Favicon**: quadrato con padding, sfondo #0d0d0d
4. Assembla `AppIcon.icns` (macOS) via `iconutil`
5. Assembla `AppIcon.ico` (Windows) via bundle multi-size
6. Genera `site.webmanifest`
7. Stampa un report di tutti i file generati

**Output:** cartella `output/` con tutte le varianti organizzate per piattaforma.

---

## 8. Regole d'uso

| ✓ Corretto | ✗ Sbagliato |
|-----------|------------|
| WhyCremisi su sfondo scuro | Logo su sfondo rosso/arancio |
| Mark solo quando < 40px | Wordmark sotto i 16px |
| Versione monocromatica su B&N | Logo colorato su stampa B&N |
| Clear space = altezza del mark | Logo tagliato ai bordi |
| Proporzioni originali sempre | Stretching o distorsione |
