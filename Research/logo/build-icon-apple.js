/**
 * build-icon-apple.js
 * Genera un .icns perfetto seguendo Apple Icon Grid (HIG 2024 + liquid-glass ready).
 *
 * Regole Apple:
 *  - Canvas 1024×1024 (@1x reference), tutte le misure proporzionali
 *  - Corner radius = 22.37% del lato (229px su 1024)
 *  - Artwork fill: la maschera deve occupare ~80% dell'area visiva interna
 *  - Light source: top-left → highlight sottile in alto, ombra leggera in basso
 *  - Liquid glass readiness: foreground PNG trasparente + background PNG separati
 *
 * Output:
 *  platform/macos/     → tutte le misure per iconutil
 *  platform/WhyCremisi.icns  → .icns finale
 *  platform/icon-fg.png      → foreground layer (liquid glass)
 *  platform/icon-bg.png      → background layer (liquid glass)
 */

const sharp  = require('sharp')
const path   = require('path')
const fs     = require('fs')
const { execSync } = require('child_process')

const ROOT    = path.resolve(__dirname)
const SRC     = path.join(ROOT, 'Loghi Ufficiali', 'whycremisi-mask-primary-clean.png')
const MACOS   = path.join(ROOT, 'platform', 'macos')
const PLAT    = path.join(ROOT, 'platform')
const ICONSET = path.join(PLAT, 'WhyCremisi.iconset')

// macOS icon sizes required by iconutil
const SIZES = [
  { name: 'icon_16x16',      px: 16  },
  { name: 'icon_16x16@2x',   px: 32  },
  { name: 'icon_32x32',      px: 32  },
  { name: 'icon_32x32@2x',   px: 64  },
  { name: 'icon_128x128',    px: 128 },
  { name: 'icon_128x128@2x', px: 256 },
  { name: 'icon_256x256',    px: 256 },
  { name: 'icon_256x256@2x', px: 512 },
  { name: 'icon_512x512',    px: 512 },
  { name: 'icon_512x512@2x', px: 1024 },
]

// Apple standard macOS corner radius
const cornerRadiusPct = 0.2237

async function trimMask(maskFgBuffer) {
  return sharp(maskFgBuffer).trim({ threshold: 10 }).png().toBuffer()
}

async function buildIconAtSize(maskFgBuffer, size) {
  const radius = Math.round(size * cornerRadiusPct)

  // ── 1. Background: flat dark, same as original style ─────────────────────
  const bg = await sharp({
    create: { width: size, height: size, channels: 4, background: { r: 13, g: 10, b: 16, alpha: 255 } }
  })
  .png()
  .toBuffer()

  // ── 2. Mask at 85% — large like original, slight breathing room ───────────
  const maskTargetSize = Math.round(size * 0.72)
  const pad = Math.round((size - maskTargetSize) / 2)

  const maskResized = await sharp(maskFgBuffer)
    .resize(maskTargetSize, maskTargetSize, { fit: 'contain', background: { r:0,g:0,b:0,alpha:0 } })
    .png()
    .toBuffer()

  // ── 3. Composite + rounded rect clip ─────────────────────────────────────
  const roundedClip = Buffer.from(`
    <svg width="${size}" height="${size}" xmlns="http://www.w3.org/2000/svg">
      <rect width="${size}" height="${size}" rx="${radius}" ry="${radius}" fill="white"/>
    </svg>
  `)

  const composed = await sharp(bg)
    .composite([{ input: maskResized, top: pad, left: pad, blend: 'over' }])
    .png()
    .toBuffer()

  return sharp(composed)
    .composite([{ input: roundedClip, blend: 'dest-in' }])
    .png()
    .toBuffer()
}

async function buildForegroundLayer(maskFgBuffer, size) {
  // Liquid glass foreground: just the mask, transparent background, centered
  const maskTargetSize = Math.round(size * 0.72)
  const maskPad        = Math.round((size - maskTargetSize) / 2)
  const radius         = Math.round(size * cornerRadiusPct)

  const canvas = await sharp({
    create: { width: size, height: size, channels: 4, background: { r:0, g:0, b:0, alpha: 0 } }
  }).png().toBuffer()

  const maskResized = await sharp(maskFgBuffer)
    .resize(maskTargetSize, maskTargetSize, { fit: 'contain', background: { r:0,g:0,b:0,alpha:0 } })
    .png()
    .toBuffer()

  const roundedClip = Buffer.from(`
    <svg width="${size}" height="${size}" xmlns="http://www.w3.org/2000/svg">
      <rect width="${size}" height="${size}" rx="${radius}" ry="${radius}" fill="white"/>
    </svg>
  `)

  const composed = await sharp(canvas)
    .composite([{ input: maskResized, top: maskPad, left: maskPad }])
    .png()
    .toBuffer()

  return sharp(composed)
    .composite([{ input: roundedClip, blend: 'dest-in' }])
    .png()
    .toBuffer()
}

async function extractMaskForeground(srcPath) {
  // Luminance-to-alpha: extract mask from black background
  const { data, info } = await sharp(srcPath)
    .ensureAlpha()
    .raw()
    .toBuffer({ resolveWithObject: true })

  const rgba = new Uint8Array(data)
  for (let i = 0; i < info.width * info.height; i++) {
    const r = rgba[i*4], g = rgba[i*4+1], b = rgba[i*4+2]
    // Use max channel as alpha (luminance extraction)
    rgba[i*4+3] = Math.max(r, g, b)
  }

  return sharp(Buffer.from(rgba), {
    raw: { width: info.width, height: info.height, channels: 4 }
  })
  .png()
  .toBuffer()
}

async function main() {
  console.log('Building WhyCremisi macOS icons (Apple Icon Grid compliant)...\n')

  // Prepare output dirs
  if (!fs.existsSync(ICONSET)) fs.mkdirSync(ICONSET, { recursive: true })
  if (!fs.existsSync(MACOS))   fs.mkdirSync(MACOS,   { recursive: true })

  // Extract mask foreground from black background, then trim tight
  console.log('Extracting mask foreground (luminance-to-alpha)...')
  const maskFgRaw = await extractMaskForeground(SRC)
  const maskFg    = await trimMask(maskFgRaw)
  console.log('  ✓ Mask trimmed to artwork bounds')

  // Generate all iconset sizes
  console.log('Generating iconset sizes...')
  for (const { name, px } of SIZES) {
    const out = path.join(ICONSET, `${name}.png`)
    const buf = await buildIconAtSize(maskFg, px)
    fs.writeFileSync(out, buf)
    console.log(`  ✓ ${name}.png (${px}×${px})`)
  }

  // Copy iconset sizes to platform/macos as well
  for (const { name, px } of SIZES) {
    fs.copyFileSync(
      path.join(ICONSET, `${name}.png`),
      path.join(MACOS,   `${name}.png`)
    )
  }

  // Build .icns via iconutil
  console.log('\nBuilding .icns via iconutil...')
  const icnsOut = path.join(PLAT, 'WhyCremisi.icns')
  execSync(`iconutil --convert icns "${ICONSET}" --output "${icnsOut}"`)
  const icnsSize = (fs.statSync(icnsOut).size / 1024).toFixed(0)
  console.log(`  ✓ WhyCremisi.icns (${icnsSize}KB)`)

  // Generate liquid-glass-ready layer files at 1024px
  console.log('\nGenerating liquid glass layer files (1024px)...')
  const fgBuf = await buildForegroundLayer(maskFg, 1024)
  fs.writeFileSync(path.join(PLAT, 'icon-fg.png'), fgBuf)
  console.log('  ✓ icon-fg.png  (foreground layer — for Icon Composer)')

  const bgBuf = await buildIconAtSize(maskFg, 1024)
  // Write background-only by compositing transparent white over the mask area
  const bgOnlyBuf = await sharp({
    create: { width: 1024, height: 1024, channels: 4, background: { r:0,g:0,b:0,alpha:0 } }
  })
  .composite([{
    input: Buffer.from(`
      <svg width="1024" height="1024" xmlns="http://www.w3.org/2000/svg">
        <defs>
          <radialGradient id="bg" cx="42%" cy="35%" r="65%">
            <stop offset="0%"   stop-color="#1a0d1f"/>
            <stop offset="55%"  stop-color="#0d0810"/>
            <stop offset="100%" stop-color="#060408"/>
          </radialGradient>
          <radialGradient id="hl" cx="25%" cy="18%" r="40%">
            <stop offset="0%"  stop-color="#ffffff" stop-opacity="0.07"/>
            <stop offset="100%" stop-color="#ffffff" stop-opacity="0"/>
          </radialGradient>
        </defs>
        <rect width="1024" height="1024" rx="${Math.round(1024*cornerRadiusPct)}" ry="${Math.round(1024*cornerRadiusPct)}" fill="url(#bg)"/>
        <rect width="1024" height="1024" rx="${Math.round(1024*cornerRadiusPct)}" ry="${Math.round(1024*cornerRadiusPct)}" fill="url(#hl)"/>
      </svg>
    `),
    top: 0, left: 0
  }])
  .png()
  .toBuffer()

  const roundedBg = Buffer.from(`
    <svg width="1024" height="1024" xmlns="http://www.w3.org/2000/svg">
      <rect width="1024" height="1024" rx="${Math.round(1024*cornerRadiusPct)}" ry="${Math.round(1024*cornerRadiusPct)}" fill="white"/>
    </svg>
  `)
  const bgClipped = await sharp(bgOnlyBuf)
    .composite([{ input: roundedBg, blend: 'dest-in' }])
    .png()
    .toBuffer()

  fs.writeFileSync(path.join(PLAT, 'icon-bg.png'), bgClipped)
  console.log('  ✓ icon-bg.png  (background layer — for Icon Composer)')

  console.log('\nDone! Files:')
  console.log(`  ${path.join(PLAT, 'WhyCremisi.icns')}`)
  console.log(`  ${MACOS}/icon_*.png`)
  console.log(`  ${path.join(PLAT, 'icon-fg.png')} (liquid glass foreground)`)
  console.log(`  ${path.join(PLAT, 'icon-bg.png')} (liquid glass background)`)
}

main().catch(err => { console.error(err); process.exit(1) })
