# Paper 07 — Brand & Asset Identity
## Colors, Typography, Logo and Complete Icon Specification

```
────────────────────────────────────────────────────────────────
  WHYCREMISI RESEARCH PAPERS — N.07
  Brand Identity & Visual Asset System
────────────────────────────────────────────────────────────────
```

---

## 1. Brand Philosophy

WhyCremisi is a professional tool for professional musicians. The visual identity reflects this: **no decoration, no noise** — only intentional elements with a precise function.

```
  CREMISI RED    — Action · Danger · Power · Precision
  AMBER GOLD     — Warmth · Sound · Organic · Suggestion  
  DEEP BLACK     — Control · Focus · Mastery · Silence
```

The name "WhyCremisi" is always written as a single word, capital W and C. Never "Why Cremisi", never "whycremisi", never "WHYCREMISI".

---

## 2. Color Palette

### Primary Colors

| Name | Hex | RGB | Usage |
|------|-----|-----|-------|
| Cremisi Red | `#DC143C` | 220, 20, 60 | Logo · CTA buttons · Active states |
| Amber Gold | `#FFB000` | 255, 176, 0 | Accents · Warnings · VU meters |
| Deep Black | `#0d0d0d` | 13, 13, 13 | Primary background |

### Secondary Colors

| Name | Hex | Usage |
|------|-----|-------|
| Dark Gray | `#1a1a1a` | Panels · Cards · Containers |
| Medium Gray | `#2a2a2a` | Borders · Separators |
| Text Primary | `#e0e0e0` | Main text |
| Text Secondary | `#888888` | Labels · Metadata |
| Text Muted | `#555555` | Disabled · Placeholder |

### Status Colors

| State | Hex | Context |
|-------|-----|---------|
| Active / On | `#DC143C` | Recording · Armed tracks |
| Warning | `#FFB000` | Clip approaching · Attention |
| Safe / OK | `#22c55e` | Level safe · Connection OK |
| Error | `#ef4444` | Connection lost · Critical error |

---

## 3. Typography

```
  PRIMARY FONT:    Inter (UI text, labels, numbers)
  MONO FONT:       JetBrains Mono (values, code, telemetry)
  FALLBACKS:       -apple-system, BlinkMacSystemFont, sans-serif
```

### Type Scale

| Role | Size | Weight | Usage |
|------|------|--------|-------|
| Chat text | 10–11px | 400 | Bot and user messages |
| Labels | 9–10px | 500 | Parameter names · VU labels |
| Values | 11–12px | 600 | LUFS · dB · BPM (mono) |
| Section headers | 10px | 700 | Box titles (all caps) |
| UI buttons | 10px | 600 | Commands · Actions |

---

## 4. Logo Concept

```
  ┌─────────────────────────────────────────┐
  │                                         │
  │    ██╗    ██╗██╗  ██╗██╗   ██╗         │
  │    ██║    ██║██║  ██║╚██╗ ██╔╝         │
  │    ██║ █╗ ██║███████║ ╚████╔╝          │
  │    ██║███╗██║██╔══██║  ╚██╔╝           │
  │    ╚███╔███╔╝██║  ██║   ██║            │
  │     ╚══╝╚══╝ ╚═╝  ╚═╝   ╚═╝            │
  │                                         │
  │    C R E M I S I                        │
  │    ─────────────                        │
  │    AI MIX ASSISTANT                     │
  │                                         │
  └─────────────────────────────────────────┘
  
  Mark:  Stylised W with integrated diagonal slash
         (the "Why" as question — always questioning the mix)
  Color: Cremisi Red on Deep Black
  Mono:  White on Deep Black (for dark backgrounds)
         Deep Black on White (for light contexts — never preferred)
```

---

## 5. BotFace — Mascot

The BotFace is WhyCremisi's AI mascot — animated SVG rendered inside the plugin UI via React + Framer Motion.

```
  ┌────────────────────────────────────────┐
  │                                        │
  │      ╭──────────────╮                  │
  │     │  ●        ●   │  ← Eyes (pupils) │
  │     │               │                  │
  │     │  ──────────   │  ← Mouth bar     │
  │      ╰──────────────╯                  │
  │                                        │
  └────────────────────────────────────────┘
```

### Emotional States (9 total)

| State | Trigger | Animation |
|-------|---------|-----------|
| `idle` | Default | Slow eye blink, subtle float |
| `thinking` | Processing AI prompt | Eye rotation, spin |
| `speaking` | Streaming response | Mouth wave oscillation |
| `happy` | User accepts suggestion | Eyes curve up (smile) |
| `surprised` | Clip / loud signal | Eyes wide open |
| `focused` | Plugin parameter change | Eyes narrow |
| `sad` | User rejects suggestion | Eyes drop |
| `alert` | Error / critical state | Eyes red, fast pulse |
| `sleeping` | No interaction > 5min | Eyes closed, breath cycle |

---

## 6. Icon Specification

All icons must ship as separate files per platform:

### macOS Icons (`AppIcon.icns`)

```
  Sizes required:
  16×16      @1x   (Finder small)
  32×32      @2x   (Finder retina)
  128×128    @1x   (Finder large)
  256×256    @2x   (Finder large retina)
  512×512    @1x   (App Store)
  1024×1024  @2x   (App Store retina)
```

### Windows Icons (`AppIcon.ico`)

```
  Sizes required:
  16×16      (taskbar small)
  32×32      (taskbar)
  48×48      (desktop)
  256×256    (high-DPI / Store)
```

### Linux Icons

```
  Format:    PNG (scalable preferred: SVG)
  Sizes:     16, 24, 32, 48, 64, 128, 256, 512
  Path:      /usr/share/icons/hicolor/<size>/apps/whycremisi.png
```

### Web / DAW

```
  Favicon:   32×32 PNG + .ico
  OG Image:  1200×630 PNG (social sharing)
  DAW tile:  256×256 PNG (plugin manager thumbnail)
```

---

## 7. UI Component Identity

### Panel Style
```css
  background:    #1a1a1a
  border:        1px solid #2a2a2a
  border-radius: 4px
  box-shadow:    0 2px 8px rgba(0,0,0,0.6)
```

### Active / Selected State
```css
  border-color:  #DC143C
  box-shadow:    0 0 0 1px #DC143C40
```

### Meter Colors
```
  Safe zone   (< -18 LUFS):   #22c55e  (green)
  Warning zone(-18 to -6):    #FFB000  (amber)
  Danger zone (> -6 LUFS):    #DC143C  (cremisi red)
```

### BoxChat Card Header
```
  Background:  rgba(220,20,60,0.08)
  Border-left: 2px solid #DC143C
  Text:        #DC143C, uppercase, 9px, letter-spacing: 0.1em
```

---

## 8. Animation Principles

```
  Philosophy:   Everything that moves has a reason to move.
                No decorative animation.

  Timing:       Fast responses  — 150ms ease-out
                State changes   — 200ms ease-in-out
                Data updates    — 100ms linear
                Mascot idle     — 2000–4000ms ease-in-out (organic)

  Easing:       cubic-bezier(0.16, 1, 0.3, 1)  — spring-like snaps
                ease-in-out                      — smooth state shifts
```

---

## 9. Voice and Tone

```
  ✓  Direct. Precise. No filler.
  ✓  Technical but not cryptic — translates jargon when helpful.
  ✓  Confident suggestions, never apologetic.
  ✓  Short sentences in chat. Maximum 2 lines before a break.

  ✗  Never says "I think", "maybe", "perhaps".
  ✗  Never repeats what the user just said.
  ✗  Never adds "Let me know if you have any questions."
  ✗  No emoji in professional contexts (only in casual/idle states).
```

---

*→ End of Research Papers Series*
*→ Return to: [Index](../00-INDEX.md)*
