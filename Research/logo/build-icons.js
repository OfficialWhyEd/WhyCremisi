#!/usr/bin/env node
/**
 * WhyCremisi — Icon Builder
 * Source: Loghi Ufficiali/whycremisi-mask-primary.jpg (official logo)
 * Generates all platform icon sizes from the official source.
 *
 * Usage:
 *   cd Research/logo && npm install sharp && node build-icons.js
 *
 * Output: platform/ subfolders (macos, windows, linux, favicon, daw)
 */

'use strict';
const fs   = require('fs');
const path = require('path');
const { execSync } = require('child_process');

let sharp;
try { sharp = require('sharp'); }
catch { console.error('\n  ERROR: run "npm install sharp" first\n'); process.exit(1); }

const ROOT   = __dirname;
// Use cleaned PNG (watermark removed, upscaled 1536px) as primary source
const SOURCE = path.join(ROOT, 'Loghi Ufficiali', 'whycremisi-mask-primary-clean.png');
const OUT    = path.join(ROOT, 'platform');

if (!fs.existsSync(SOURCE)) {
  console.error(`\n  SOURCE NOT FOUND: ${SOURCE}\n`);
  process.exit(1);
}

// ─── Platform definitions ──────────────────────────────────────────────────

const PLATFORMS = {

  macos: {
    dir: 'macos',
    radius: s => Math.round(s * 0.225),   // Apple squircle 22.5%
    glass: true,
    sizes: [
      { n: 'icon_16x16.png',       s: 16   },
      { n: 'icon_16x16@2x.png',    s: 32   },
      { n: 'icon_32x32.png',       s: 32   },
      { n: 'icon_32x32@2x.png',    s: 64   },
      { n: 'icon_128x128.png',     s: 128  },
      { n: 'icon_128x128@2x.png',  s: 256  },
      { n: 'icon_256x256.png',     s: 256  },
      { n: 'icon_256x256@2x.png',  s: 512  },
      { n: 'icon_512x512.png',     s: 512  },
      { n: 'icon_512x512@2x.png',  s: 1024 },
    ],
  },

  windows: {
    dir: 'windows',
    radius: s => Math.round(s * 0.20),    // Windows Fluent 20%
    sizes: [
      { n: 'win-16.png',  s: 16  },
      { n: 'win-20.png',  s: 20  },
      { n: 'win-24.png',  s: 24  },
      { n: 'win-32.png',  s: 32  },
      { n: 'win-40.png',  s: 40  },
      { n: 'win-48.png',  s: 48  },
      { n: 'win-64.png',  s: 64  },
      { n: 'win-96.png',  s: 96  },
      { n: 'win-128.png', s: 128 },
      { n: 'win-256.png', s: 256 },
      { n: 'win-512.png', s: 512 },
    ],
  },

  linux: {
    dir: 'linux',
    transparent: true,    // freedesktop.org: transparent bg, no effects
    sizes: [
      { n: 'linux-16.png',  s: 16  },
      { n: 'linux-22.png',  s: 22  },
      { n: 'linux-24.png',  s: 24  },
      { n: 'linux-32.png',  s: 32  },
      { n: 'linux-48.png',  s: 48  },
      { n: 'linux-64.png',  s: 64  },
      { n: 'linux-96.png',  s: 96  },
      { n: 'linux-128.png', s: 128 },
      { n: 'linux-256.png', s: 256 },
      { n: 'linux-512.png', s: 512 },
    ],
  },

  favicon: {
    dir: 'favicon',
    radius: s => Math.round(s * 0.15),
    sizes: [
      { n: 'favicon-16x16.png',   s: 16  },
      { n: 'favicon-32x32.png',   s: 32  },
      { n: 'favicon-48x48.png',   s: 48  },
      { n: 'favicon-96x96.png',   s: 96  },
      { n: 'favicon-180x180.png', s: 180 },   // apple-touch-icon
      { n: 'favicon-192x192.png', s: 192 },
      { n: 'favicon-512x512.png', s: 512 },
    ],
  },

  daw: {
    dir: 'daw',
    radius: s => Math.round(s * 0.10),
    sizes: [
      { n: 'daw-thumb-256.png', s: 256 },
      { n: 'daw-thumb-128.png', s: 128 },
      { n: 'daw-thumb-64.png',  s: 64  },
    ],
  },

};

// ─── Helpers ───────────────────────────────────────────────────────────────

function mkdir(d) { if (!fs.existsSync(d)) fs.mkdirSync(d, { recursive: true }); }

// Squircle clip mask — white shape on transparent; use with blend:'dest-in'
function squircleMask(w, h, r) {
  return Buffer.from(
    `<svg width="${w}" height="${h}" xmlns="http://www.w3.org/2000/svg">` +
    `<rect width="${w}" height="${h}" rx="${r}" ry="${r}" fill="white"/>` +
    `</svg>`
  );
}

function makeGlassOverlay(w, h, r) {
  const svg = `<svg width="${w}" height="${h}" xmlns="http://www.w3.org/2000/svg">
    <defs>
      <linearGradient id="g" x1="0" y1="0" x2="0" y2="1">
        <stop offset="0%"   stop-color="white" stop-opacity="0.22"/>
        <stop offset="45%"  stop-color="white" stop-opacity="0.05"/>
        <stop offset="100%" stop-color="white" stop-opacity="0"/>
      </linearGradient>
      <linearGradient id="ring" x1="0" y1="0" x2="0" y2="1">
        <stop offset="0%"   stop-color="#e8e8e8" stop-opacity="0.75"/>
        <stop offset="50%"  stop-color="#888888" stop-opacity="0.30"/>
        <stop offset="100%" stop-color="#444444" stop-opacity="0.55"/>
      </linearGradient>
      <clipPath id="clip">
        <rect width="${w}" height="${h}" rx="${r}" ry="${r}"/>
      </clipPath>
    </defs>
    <rect x="0" y="0" width="${w}" height="${h}" rx="${r}" fill="url(#g)" clip-path="url(#clip)"/>
    <rect x="0.75" y="0.75" width="${w - 1.5}" height="${h - 1.5}" rx="${Math.max(0, r - 0.75)}"
          fill="none" stroke="url(#ring)" stroke-width="1.5"/>
  </svg>`;
  return sharp(Buffer.from(svg)).png().toBuffer();
}

// ─── Main build ────────────────────────────────────────────────────────────

async function build() {
  console.log('\n  ┌─────────────────────────────────────────┐');
  console.log('  │   WhyCremisi Icon Builder               │');
  console.log('  │   Source: Loghi Ufficiali (official)    │');
  console.log('  └─────────────────────────────────────────┘\n');

  mkdir(OUT);
  let total = 0;
  let errors = 0;

  const sourceBuf = fs.readFileSync(SOURCE);

  for (const [pfKey, cfg] of Object.entries(PLATFORMS)) {
    const dir = path.join(OUT, cfg.dir);
    mkdir(dir);
    console.log(`  ▸ ${pfKey.toUpperCase()}`);

    for (const spec of cfg.sizes) {
      const w    = spec.w || spec.s;
      const h    = spec.h || spec.s;
      const dest = path.join(dir, spec.n);
      const r    = cfg.radius ? cfg.radius(Math.min(w, h)) : 0;

      try {

        if (cfg.transparent) {
          // Linux — logo fills to 90%, centered on transparent canvas.
          // Source has black bg baked in; we keep a small inset so it
          // reads cleanly on both light and dark desktop themes.
          const logoResized = await sharp(sourceBuf)
            .resize(Math.round(w * 0.90), Math.round(h * 0.90), {
              fit: 'contain',
              background: { r: 0, g: 0, b: 0, alpha: 0 },
            })
            .png()
            .toBuffer();

          await sharp({
            create: { width: w, height: h, channels: 4,
                      background: { r: 0, g: 0, b: 0, alpha: 0 } }
          })
          .composite([{ input: logoResized, gravity: 'centre' }])
          .png({ compressionLevel: 9 })
          .toFile(dest);

        } else {
          // All other platforms:
          // 1. Fill icon dimensions exactly (cover) — source black merges
          //    seamlessly with itself; no secondary background layer needed.
          // 2. Clip to squircle shape via dest-in mask.
          // 3. macOS only: add Liquid Glass highlight on top.

          const logoFilled = await sharp(sourceBuf)
            .resize(w, h, { fit: 'cover', position: 'centre' })
            .png()
            .toBuffer();

          let result;
          if (r > 0) {
            result = await sharp(logoFilled)
              .composite([{ input: squircleMask(w, h, r), blend: 'dest-in' }])
              .png()
              .toBuffer();
          } else {
            result = logoFilled;
          }

          if (cfg.glass) {
            const glassBuf = await makeGlassOverlay(w, h, r);
            await sharp(result)
              .composite([{ input: glassBuf }])
              .png({ compressionLevel: 9 })
              .toFile(dest);
          } else {
            await sharp(result)
              .png({ compressionLevel: 9 })
              .toFile(dest);
          }
        }

        process.stdout.write(`    ✓  ${spec.n}\n`);
        total++;

      } catch (err) {
        process.stdout.write(`    ✗  ${spec.n}: ${err.message}\n`);
        errors++;
      }
    }
    console.log('');
  }

  // ── macOS .iconset → .icns ──────────────────────────────────────────────
  try {
    const iconset = path.join(OUT, 'WhyCremisi.iconset');
    mkdir(iconset);
    const macDir = path.join(OUT, 'macos');
    const files  = [
      'icon_16x16.png','icon_16x16@2x.png','icon_32x32.png','icon_32x32@2x.png',
      'icon_128x128.png','icon_128x128@2x.png','icon_256x256.png','icon_256x256@2x.png',
      'icon_512x512.png','icon_512x512@2x.png',
    ];
    files.forEach(f => {
      const src = path.join(macDir, f);
      if (fs.existsSync(src)) fs.copyFileSync(src, path.join(iconset, f));
    });
    try {
      execSync(`iconutil -c icns "${iconset}" -o "${path.join(OUT, 'WhyCremisi.icns')}"`,
               { stdio: 'pipe' });
      console.log('  ✓  WhyCremisi.icns (macOS native bundle)');
    } catch {
      console.log('  ~  .icns: install Xcode CLI tools to enable iconutil');
    }
  } catch (e) { console.log(`  ~  iconset: ${e.message}`); }

  // ── Linux: copy SVG as scalable icon ────────────────────────────────────
  try {
    const svgSrc = path.join(ROOT, 'svg', 'whycremisi-mask-transparent.svg');
    if (fs.existsSync(svgSrc)) {
      fs.copyFileSync(svgSrc, path.join(OUT, 'linux', 'whycremisi.svg'));
      console.log('  ✓  linux/whycremisi.svg (scalable)');
    }
  } catch (e) { console.log(`  ~  ${e.message}`); }

  // ── Favicon HTML snippet ─────────────────────────────────────────────────
  const html = `<!-- WhyCremisi Favicon — paste in <head> -->
<link rel="icon"             type="image/png" sizes="32x32"   href="/favicon-32x32.png">
<link rel="icon"             type="image/png" sizes="96x96"   href="/favicon-96x96.png">
<link rel="icon"             type="image/png" sizes="16x16"   href="/favicon-16x16.png">
<link rel="apple-touch-icon" sizes="180x180"                  href="/favicon-180x180.png">
<link rel="manifest"                                           href="/site.webmanifest">
<meta name="theme-color"     content="#000000">
`;
  fs.writeFileSync(path.join(OUT, 'favicon', 'favicon-snippet.html'), html);

  // ── site.webmanifest ─────────────────────────────────────────────────────
  const manifest = {
    name: 'WhyCremisi', short_name: 'WhyCremisi',
    icons: [
      { src: '/favicon-192x192.png', sizes: '192x192', type: 'image/png' },
      { src: '/favicon-512x512.png', sizes: '512x512', type: 'image/png', purpose: 'any maskable' },
    ],
    theme_color: '#000000', background_color: '#000000', display: 'standalone',
  };
  fs.writeFileSync(path.join(OUT, 'favicon', 'site.webmanifest'),
                   JSON.stringify(manifest, null, 2));
  console.log('  ✓  site.webmanifest');

  // ── Summary ──────────────────────────────────────────────────────────────
  console.log(`\n  ─────────────────────────────────────────`);
  console.log(`  Generated: ${total} files   Errors: ${errors}`);
  console.log(`  Output:    ${OUT}`);
  console.log(`  ─────────────────────────────────────────\n`);
}

build().catch(err => { console.error('Fatal:', err); process.exit(1); });
