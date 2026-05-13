#!/usr/bin/env node

const fs = require('fs');
const path = require('path');
const sharp = require('sharp');

const ROOT = __dirname;
const SRC = path.join(ROOT, 'whycremisi-logo-source.svg');
const OUT = path.join(ROOT, 'output');

const COLORS = { cremisi:'#DC143C', amber:'#FFB000', black:'#0d0d0d', white:'#E5E2E1' };

const PLATFORMS = {
  macos: {
    dir: 'macos',
    sizes: [
      { n:'icon_16x16.png', s:16 }, { n:'icon_16x16@2x.png', s:32 },
      { n:'icon_32x32.png', s:32 }, { n:'icon_32x32@2x.png', s:64 },
      { n:'icon_128x128.png', s:128 }, { n:'icon_128x128@2x.png', s:256 },
      { n:'icon_256x256.png', s:256 }, { n:'icon_256x256@2x.png', s:512 },
      { n:'icon_512x512.png', s:512 }, { n:'icon_512x512@2x.png', s:1024 },
    ],
    radius: s => Math.round(s * 0.225),
    bg: '#0d0d0d',
    glass: true,
  },
  windows: {
    dir: 'windows',
    sizes: [
      { n:'win-16.png', s:16 }, { n:'win-20.png', s:20 }, { n:'win-24.png', s:24 },
      { n:'win-32.png', s:32 }, { n:'win-40.png', s:40 }, { n:'win-48.png', s:48 },
      { n:'win-64.png', s:64 }, { n:'win-96.png', s:96 }, { n:'win-128.png', s:128 },
      { n:'win-256.png', s:256 },
    ],
    radius: s => Math.round(s * 0.20),
    bg: '#0d0d0d',
  },
  windowsStore: {
    dir: 'windows-store',
    sizes: [
      { n:'StoreLogo.png', s:50 }, { n:'Square44x44Logo.png', s:44 },
      { n:'Square150x150Logo.png', s:150 }, { n:'Square310x310Logo.png', s:310 },
      { n:'Wide310x150Logo.png', s:310, h:150 },
    ],
    radius: s => Math.round(s * 0.15),
    bg: '#0d0d0d',
  },
  linux: {
    dir: 'linux',
    sizes: [
      { n:'linux-16.png', s:16 }, { n:'linux-22.png', s:22 }, { n:'linux-24.png', s:24 },
      { n:'linux-32.png', s:32 }, { n:'linux-48.png', s:48 }, { n:'linux-64.png', s:64 },
      { n:'linux-96.png', s:96 }, { n:'linux-128.png', s:128 }, { n:'linux-256.png', s:256 },
      { n:'linux-512.png', s:512 },
    ],
    transparent: true,
  },
  favicon: {
    dir: 'favicon',
    sizes: [
      { n:'favicon-16x16.png', s:16 }, { n:'favicon-32x32.png', s:32 },
      { n:'favicon-48x48.png', s:48 }, { n:'favicon-96x96.png', s:96 },
      { n:'favicon-180x180.png', s:180 }, { n:'favicon-192x192.png', s:192 },
      { n:'favicon-512x512.png', s:512 },
    ],
    radius: s => Math.round(s * 0.15),
    bg: '#0d0d0d',
  },
  daw: {
    dir: 'daw',
    sizes: [
      { n:'daw-thumb.png', s:256 }, { n:'daw-thumb-small.png', s:128 },
    ],
    radius: s => Math.round(s * 0.10),
    bg: '#0d0d0d',
  },
  social: {
    dir: 'social',
    sizes: [
      { n:'og-image.png', w:1200, h:630 }, { n:'og-image-square.png', w:1200, h:1200 },
    ],
    bg: '#0d0d0d',
  },
};

function mkdir(d) { if (!fs.existsSync(d)) fs.mkdirSync(d, { recursive: true }); }

function getSvgBase() {
  return fs.readFileSync(SRC);
}

async function build() {
  console.log('\n  ┌──────────────────────────────────────┐');
  console.log('  │   WhyCremisi Icon Builder             │');
  console.log('  └──────────────────────────────────────┘\n');

  mkdir(OUT);
  let total = 0;

  for (const [pf, cfg] of Object.entries(PLATFORMS)) {
    const d = path.join(OUT, cfg.dir);
    mkdir(d);

    for (const s of cfg.sizes) {
      const w = s.w || s.s;
      const h = s.h || s.s;
      const radius = cfg.radius ? cfg.radius(Math.min(w, h)) : 0;
      const dest = path.join(d, s.n);

      try {
        const svgBuf = getSvgBase();
        const mark = sharp(svgBuf)
          .resize(Math.min(w, 1024), Math.min(h, 1024), {
            fit: 'contain',
            background: { r: 0, g: 0, b: 0, alpha: 0 },
          });

        if (cfg.bg) {
          let bgSvg = `<svg width="${w}" height="${h}"><rect width="${w}" height="${h}"`;
          if (radius > 0) bgSvg += ` rx="${radius}" ry="${radius}"`;
          bgSvg += ` fill="${cfg.bg}"/></svg>`;

          const bgBuf = await sharp(Buffer.from(bgSvg)).png().toBuffer();

          await sharp(bgBuf)
            .composite([
              { input: await mark.png().toBuffer(), gravity: 'centre' },
            ])
            .png()
            .toFile(dest);
        } else {
          await mark.png().toFile(dest);
        }

        // macOS Liquid Glass overlay
        if (cfg.glass) {
          const glassSvg = `<svg width="${w}" height="${h}">
            <defs>
              <linearGradient id="g" x1="0" y1="0" x2="0" y2="1">
                <stop offset="0%" stop-color="white" stop-opacity="0.18"/>
                <stop offset="40%" stop-color="white" stop-opacity="0.04"/>
                <stop offset="100%" stop-color="white" stop-opacity="0"/>
              </linearGradient>
              <linearGradient id="ig" x1="0" y1="0" x2="0" y2="1">
                <stop offset="0%" stop-color="#DC143C" stop-opacity="0.25"/>
                <stop offset="100%" stop-color="#DC143C" stop-opacity="0"/>
              </linearGradient>
            </defs>
            <rect x="0" y="0" width="${w}" height="${h}" rx="${radius}" fill="url(#g)"/>
            <rect x="1" y="1" width="${w-2}" height="${h-2}" rx="${Math.max(0, radius-1)}" fill="none" stroke="url(#ig)" stroke-width="1"/>
          </svg>`;

          const glassBuf = await sharp(Buffer.from(glassSvg)).png().toBuffer();
          const baseBuf = await sharp(dest).png().toBuffer();
          await sharp(baseBuf)
            .composite([{ input: glassBuf }])
            .png()
            .toFile(dest);
        }

        process.stdout.write(`  ✓ ${cfg.dir}/${s.n}\n`);
        total++;
      } catch (err) {
        process.stdout.write(`  ✗ ${cfg.dir}/${s.n}: ${err.message}\n`);
      }
    }
  }

  // macOS .iconset
  try {
    const iconset = path.join(OUT, 'WhyCremisi.iconset');
    mkdir(iconset);
    const macDir = path.join(OUT, 'macos');
    const links = ['icon_16x16.png','icon_16x16@2x.png','icon_32x32.png','icon_32x32@2x.png',
      'icon_128x128.png','icon_128x128@2x.png','icon_256x256.png','icon_256x256@2x.png',
      'icon_512x512.png','icon_512x512@2x.png'];
    for (const f of links) {
      const src = path.join(macDir, f);
      if (fs.existsSync(src)) fs.copyFileSync(src, path.join(iconset, f));
    }
    try {
      const { execSync } = require('child_process');
      execSync(`iconutil -c icns "${iconset}" -o "${path.join(OUT, 'WhyCremisi.icns')}"`, { stdio: 'pipe' });
      console.log('  ✓ WhyCremisi.icns');
    } catch {
      console.log('  ~ iconutil skipped (install Xcode CLI tools for .icns)');
    }
  } catch (e) {
    console.log(`  ~ iconset: ${e.message}`);
  }

  // Linux scalable SVG copy
  try {
    fs.copyFileSync(SRC, path.join(OUT, 'linux', 'whycremisi.svg'));
    console.log('  ✓ linux/whycremisi.svg');
  } catch (e) {
    console.log(`  ~ ${e.message}`);
  }

  // site.webmanifest
  const manifest = {
    name: 'WhyCremisi', short_name: 'WhyCremisi',
    icons: [
      { src: '/favicon-192x192.png', sizes: '192x192', type: 'image/png' },
      { src: '/favicon-512x512.png', sizes: '512x512', type: 'image/png' },
    ],
    theme_color: '#DC143C', background_color: '#0d0d0d', display: 'standalone',
  };
  fs.writeFileSync(path.join(OUT, 'site.webmanifest'), JSON.stringify(manifest, null, 2));
  console.log('  ✓ site.webmanifest');

  console.log(`\n  Generated ${total} files → ${OUT}\n`);
}

build().catch(err => { console.error('Fatal:', err); process.exit(1); });
