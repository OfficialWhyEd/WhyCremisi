# WhyCremisi — Logo & Brand Identity Guidelines

```
────────────────────────────────────────────────────────────────
  WHYCREMISI · LOGO SYSTEM
  Brand Identity · Usage Rules · File Specifications
────────────────────────────────────────────────────────────────
```

---

## Naming Convention

The name is always written **WhyCremisi** — one word, capital W, capital C.

| ✓ Correct | ✗ Wrong |
|-----------|---------|
| WhyCremisi | Why Cremisi |
| WhyCremisi | whycremisi |
| WhyCremisi | WHYCREMISI |
| WhyCremisi | Why-Cremisi |

---

## Color System

| Role | Name | Hex | RGB | Usage |
|------|------|-----|-----|-------|
| Primary | Cremisi Red | `#DC143C` | 220, 20, 60 | Logo mark · Active states · CTA |
| Accent | Amber Gold | `#FFB000` | 255, 176, 0 | Highlights · Warnings · VU meters |
| Background | Deep Black | `#0d0d0d` | 13, 13, 13 | Plugin UI · Dark contexts |
| Background 2 | Panel | `#161616` | 22, 22, 22 | Cards · Panels |
| Text | Off-white | `#e0e0e0` | 224, 224, 224 | Body text |

### Minimum Color Contrast
- Logo on dark background: always on `#0d0d0d` or darker
- Logo on light background: only monochrome black version
- Never place the Cremisi Red logo on a red or orange background

---

## Logo Versions

### Version 1 — Full (Primary)
Wordmark + icon mark side by side.
Used for: website, splash screen, installer, marketing.

```
  ┌─────────────────────────────────────────┐
  │  ◈  WhyCremisi                          │
  │     AI Mix Assistant                    │
  └─────────────────────────────────────────┘
  Icon: ◈ in Cremisi Red
  Wordmark: Inter Black, white
  Tagline: Inter Light, muted
```

### Version 2 — Compact
Icon mark + wordmark on one line, no tagline.
Used for: plugin header, small UI spaces, email signatures.

```
  ┌────────────────────────┐
  │  ◈  WhyCremisi         │
  └────────────────────────┘
```

### Version 3 — Icon Only
Mark alone, no text.
Used for: app icon, favicon, DAW plugin thumbnail.

```
  ┌──────────┐
  │          │
  │    ◈     │
  │          │
  └──────────┘
  Background: #0d0d0d
  Mark: #DC143C
```

### Version 4 — Monochrome White
All-white version for dark backgrounds without color.
Used for: watermarks, embossed print, black-and-white contexts.

### Version 5 — Monochrome Black
All-black version for light backgrounds (avoid when possible).
Used for: legal documents, contracts, print on white paper.

---

## File Specifications

Place all logo files in this folder (`Research/logo/`) with this naming:

```
logo/
  ├── whycremisi-full-color.svg          ← master source (vector)
  ├── whycremisi-full-color.png          ← 2000px wide, transparent bg
  ├── whycremisi-compact-color.svg
  ├── whycremisi-compact-color.png
  ├── whycremisi-icon.svg                ← icon mark only
  ├── whycremisi-icon-512.png            ← 512×512
  ├── whycremisi-icon-256.png            ← 256×256
  ├── whycremisi-icon-128.png            ← 128×128
  ├── whycremisi-icon-64.png             ← 64×64
  ├── whycremisi-icon-32.png             ← 32×32 (favicon base)
  ├── whycremisi-mono-white.svg          ← all white
  ├── whycremisi-mono-black.svg          ← all black
  ├── AppIcon.icns                       ← macOS app icon
  ├── AppIcon.ico                        ← Windows app icon
  └── og-image.png                       ← 1200×630 social share
```

---

## Platform Icon Sizes

### macOS — AppIcon.icns
| Size | Scale | Usage |
|------|-------|-------|
| 16×16 | @1x | Menu bar, small Finder |
| 32×32 | @2x | Finder retina |
| 128×128 | @1x | Finder large |
| 256×256 | @2x | Finder large retina |
| 512×512 | @1x | App Store |
| 1024×1024 | @2x | App Store retina |

### Windows — AppIcon.ico
| Size | Usage |
|------|-------|
| 16×16 | Taskbar small |
| 32×32 | Taskbar standard |
| 48×48 | Desktop shortcut |
| 256×256 | High-DPI / Store |

### Linux
Format: PNG + SVG scalable
Sizes: 16, 24, 32, 48, 64, 128, 256, 512px
Path: `/usr/share/icons/hicolor/<size>/apps/whycremisi.png`

### Web / DAW
| File | Size | Usage |
|------|------|-------|
| favicon.ico | 32×32 | Browser tab |
| og-image.png | 1200×630 | Social sharing |
| daw-thumb.png | 256×256 | DAW plugin thumbnail |

---

## Clear Space

The logo must always have clear space around it equal to **the height of the icon mark**.

```
  ┌───────────────────────────────┐
  │  [clear]                      │
  │  [clear]  ◈  WhyCremisi  [cl] │
  │  [clear]                      │
  └───────────────────────────────┘
  Minimum clear space = height of ◈ icon
```

---

## Minimum Size

| Context | Minimum width |
|---------|--------------|
| Full logo (print) | 30mm |
| Full logo (screen) | 120px |
| Compact logo | 80px |
| Icon only | 16px |

Below minimum size, use the next simpler version.

---

## Forbidden Uses

| ✗ What | Why |
|--------|-----|
| Stretching or distorting | Breaks proportions |
| Changing colors arbitrarily | Breaks brand recognition |
| Adding drop shadows or glow | Clutters the mark |
| Placing on busy backgrounds | Reduces legibility |
| Rotating the wordmark | Breaks hierarchy |
| Using outline/stroke version | Not part of the system |
| Placing red logo on red/orange bg | Zero contrast |

---

## Typography Pairing (with logo)

When the logo appears alongside text:

| Use | Font | Weight |
|-----|------|--------|
| Product name | Inter | Black (900) |
| Tagline / descriptor | Inter | Light (300) italic |
| UI labels | Inter | Medium (500) |
| Technical values | JetBrains Mono | Regular (400) |

---

## How to Add Your Logo

1. Export your logo files following the naming convention above
2. Place them in this `Research/logo/` folder
3. Run `node Research/template/build.js` to regenerate PDFs — the build system will automatically use the logo in PDF headers once the SVG is present

---

*Once you provide the logo files, they will be integrated into:*
- *Plugin UI header (plugin side)*
- *PDF paper headers*
- *macOS .app bundle icon*
- *Windows installer icon*
- *DAW plugin thumbnail*
