#!/usr/bin/env node
// WhyCremisi — Icon Build System
// Drop source.png (1024×1024+, transparent bg) → all platform icons
//
// Usage: node build-icons.js [source.png | source.svg]

const sharp = require('sharp');
const fs    = require('fs');
const path  = require('path');
const { execSync } = require('child_process');

const ROOT   = __dirname;
const OUT    = path.join(ROOT, 'output');
const SOURCE = process.argv[2] || findSource();

function findSource() {
  for (const name of ['source.png', 'source.svg']) {
    if (fs.existsSync(path.join(ROOT, name))) return path.join(ROOT, name);
  }
  console.error('✗  No source file found. Place source.png or source.svg in logo/');
  process.exit(1);
}

// ── PLATFORM SPECS ────────────────────────────────────────────────────────────

const MACOS_SIZES = [
  { file: 'icon_16x16.png',      size: 16   },
  { file: 'icon_16x16@2x.png',   size: 32   },
  { file: 'icon_32x32.png',      size: 32   },
  { file: 'icon_32x32@2x.png',   size: 64   },
  { file: 'icon_128x128.png',    size: 128  },
  { file: 'icon_128x128@2x.png', size: 256  },
  { file: 'icon_256x256.png',    size: 256  },
  { file: 'icon_256x256@2x.png', size: 512  },
  { file: 'icon_512x512.png',    size: 512  },
  { file: 'icon_512x512@2x.png', size: 1024 },
];

const WINDOWS_SIZES = [
  16, 20, 24, 32, 40, 48, 64, 96, 128, 256,
  // Windows Store extras
  44, 50, 150, 310,
];

const LINUX_SIZES   = [16, 22, 24, 32, 48, 64, 96, 128, 256, 512];
const FAVICON_SIZES = [16, 32, 48, 96, 180, 192, 512];

// ── HELPERS ───────────────────────────────────────────────────────────────────

// Squircle mask for macOS (continuous corner curve)
function squircleSVG(size) {
  const r = Math.round(size * 0.225);
  return Buffer.from(
    `<svg width="${size}" height="${size}" xmlns="http://www.w3.org/2000/svg">
      <rect width="${size}" height="${size}" rx="${r}" ry="${r}" fill="white"/>
    </svg>`
  );
}

// Liquid Glass overlay — highlight at top + subtle inner glow
function liquidGlassOverlay(size) {
  const r = Math.round(size * 0.225);
  const hlH = Math.round(size * 0.38);
  const hlO = 0.13; // highlight opacity
  return Buffer.from(
    `<svg width="${size}" height="${size}" xmlns="http://www.w3.org/2000/svg">
      <defs>
        <linearGradient id="hl" x1="0" y1="0" x2="0" y2="1">
          <stop offset="0%"   stop-color="white" stop-opacity="${hlO}"/>
          <stop offset="100%" stop-color="white" stop-opacity="0"/>
        </linearGradient>
        <clipPath id="clip">
          <rect width="${size}" height="${size}" rx="${r}" ry="${r}"/>
        </clipPath>
      </defs>
      <!-- highlight -->
      <rect x="0" y="0" width="${size}" height="${hlH}"
            fill="url(#hl)" clip-path="url(#clip)"/>
      <!-- inner border -->
      <rect x="0.5" y="0.5" width="${size - 1}" height="${size - 1}"
            rx="${r}" ry="${r}"
            fill="none" stroke="rgba(220,20,60,0.2)" stroke-width="1"/>
    </svg>`
  );
}

// Rounded rect background for Windows (flat, no glass)
function windowsBg(size) {
  const r = Math.round(size * 0.20);
  return Buffer.from(
    `<svg width="${size}" height="${size}" xmlns="http://www.w3.org/2000/svg">
      <rect width="${size}" height="${size}" rx="${r}" ry="${r}" fill="#0d0d0d"/>
    </svg>`
  );
}

// Dark square background for favicon
function faviconBg(size) {
  const r = Math.max(2, Math.round(size * 0.12));
  return Buffer.from(
    `<svg width="${size}" height="${size}" xmlns="http://www.w3.org/2000/svg">
      <rect width="${size}" height="${size}" rx="${r}" ry="${r}" fill="#0d0d0d"/>
    </svg>`
  );
}

// ── BUILD FUNCTIONS ──────────────────────────────────────────────────────────

async function buildMacOS(src) {
  const dir = path.join(OUT, 'macos', 'AppIcon.iconset');
  fs.mkdirSync(dir, { recursive: true });

  for (const { file, size } of MACOS_SIZES) {
    const pad    = Math.round(size * 0.12);           // 12% padding
    const inner  = size - pad * 2;
    const mask   = squircleSVG(size);
    const glass  = liquidGlassOverlay(size);

    // 1. Dark bg clipped to squircle
    const bg = await sharp(mask)
      .flatten({ background: { r: 13, g: 13, b: 13 } })
      .png()
      .toBuffer();

    // 2. Logo resized + padded
    const logo = await sharp(src)
      .resize(inner, inner, { fit: 'contain', background: { r: 0, g: 0, b: 0, alpha: 0 } })
      .png()
      .toBuffer();

    // 3. Composite: bg → logo (centered) → glass highlight
    await sharp(bg)
      .composite([
        { input: logo,  top: pad, left: pad },
        { input: glass, top: 0,   left: 0   },
      ])
      .png()
      .toFile(path.join(dir, file));

    process.stdout.write('.');
  }

  // Build .icns with iconutil
  const icnsOut = path.join(OUT, 'macos', 'AppIcon.icns');
  try {
    execSync(`iconutil -c icns "${dir}" -o "${icnsOut}"`);
    console.log(`\n  ✓ AppIcon.icns`);
  } catch {
    console.log('\n  ⚠ iconutil failed — PNG set still generated');
  }
}

async function buildWindows(src) {
  const dir = path.join(OUT, 'windows');
  fs.mkdirSync(dir, { recursive: true });

  for (const size of WINDOWS_SIZES) {
    const pad   = Math.round(size * 0.14);
    const inner = size - pad * 2;
    const bg    = windowsBg(size);

    const logo = await sharp(src)
      .resize(inner, inner, { fit: 'contain', background: { r: 0, g: 0, b: 0, alpha: 0 } })
      .png()
      .toBuffer();

    // Flatten bg with logo
    const flat = await sharp({
      create: { width: size, height: size, channels: 4, background: { r: 0, g: 0, b: 0, alpha: 0 } }
    })
    .composite([
      { input: await sharp(bg).png().toBuffer(), top: 0, left: 0 },
      { input: logo, top: pad, left: pad },
    ])
    .png()
    .toFile(path.join(dir, `win-${size}.png`));

    process.stdout.write('.');
  }

  // .ico via png2ico fallback (sips can't do .ico — use script workaround)
  // Bundle the key sizes into a note for the user
  const icoNote = `# AppIcon.ico
# To generate AppIcon.ico from the PNGs above, use one of:
#   - https://www.icoconverter.com (online, free)
#   - brew install imagemagick → convert win-16.png win-32.png win-48.png win-256.png AppIcon.ico
#   - In Visual Studio: right-click project → Add New Item → Icon
#
# Sizes to bundle: 16, 32, 48, 256
`;
  fs.writeFileSync(path.join(dir, 'HOW-TO-BUILD-ICO.txt'), icoNote);
  console.log(`\n  ✓ Windows PNGs + ICO instructions`);
}

async function buildLinux(src) {
  const dir = path.join(OUT, 'linux');
  fs.mkdirSync(dir, { recursive: true });

  for (const size of LINUX_SIZES) {
    // Linux: transparent bg, minimal radius, mark fills 90% of space
    const inner = Math.round(size * 0.90);
    const pad   = Math.floor((size - inner) / 2);

    await sharp(src)
      .resize(inner, inner, { fit: 'contain', background: { r: 0, g: 0, b: 0, alpha: 0 } })
      .extend({ top: pad, bottom: size - inner - pad, left: pad, right: size - inner - pad,
                background: { r: 0, g: 0, b: 0, alpha: 0 } })
      .png()
      .toFile(path.join(dir, `linux-${size}.png`));

    process.stdout.write('.');
  }
  // Copy 512 as scalable placeholder
  fs.copyFileSync(path.join(dir, 'linux-512.png'), path.join(dir, 'whycremisi.png'));
  console.log(`\n  ✓ Linux PNGs (+ add your SVG as whycremisi.svg for scalable)`);
}

async function buildFavicon(src) {
  const dir = path.join(OUT, 'favicon');
  fs.mkdirSync(dir, { recursive: true });

  for (const size of FAVICON_SIZES) {
    const pad   = Math.round(size * 0.10);
    const inner = size - pad * 2;
    const bg    = faviconBg(size);

    const logo = await sharp(src)
      .resize(inner, inner, { fit: 'contain', background: { r: 0, g: 0, b: 0, alpha: 0 } })
      .png()
      .toBuffer();

    await sharp({
      create: { width: size, height: size, channels: 4, background: { r: 0, g: 0, b: 0, alpha: 0 } }
    })
    .composite([
      { input: await sharp(bg).png().toBuffer(), top: 0, left: 0 },
      { input: logo, top: pad, left: pad },
    ])
    .png()
    .toFile(path.join(dir, `favicon-${size}x${size}.png`));

    process.stdout.write('.');
  }

  // Apple touch icon (180)
  fs.copyFileSync(
    path.join(dir, 'favicon-180x180.png'),
    path.join(dir, 'apple-touch-icon.png')
  );

  // site.webmanifest
  const manifest = {
    name: "WhyCremisi",
    short_name: "WhyCremisi",
    description: "AI Mix Assistant",
    icons: [
      { src: "/favicon-192x192.png", sizes: "192x192", type: "image/png" },
      { src: "/favicon-512x512.png", sizes: "512x512", type: "image/png", purpose: "maskable" }
    ],
    theme_color: "#DC143C",
    background_color: "#0d0d0d",
    display: "standalone",
    start_url: "/"
  };
  fs.writeFileSync(path.join(dir, 'site.webmanifest'), JSON.stringify(manifest, null, 2));

  // favicon.ico note
  const icoNote = `# favicon.ico
# Bundle favicon-16x16.png + favicon-32x32.png + favicon-48x48.png
# Online: https://favicon.io  or  imagemagick:
#   convert favicon-16x16.png favicon-32x32.png favicon-48x48.png favicon.ico
`;
  fs.writeFileSync(path.join(dir, 'HOW-TO-BUILD-ICO.txt'), icoNote);
  console.log(`\n  ✓ Favicons + apple-touch-icon + site.webmanifest`);
}

async function buildDAW(src) {
  const dir = path.join(OUT, 'daw');
  fs.mkdirSync(dir, { recursive: true });

  // DAW thumbnails — square, padded, dark bg
  for (const [name, size] of [['daw-thumb', 256], ['daw-thumb-small', 128]]) {
    const pad   = Math.round(size * 0.12);
    const inner = size - pad * 2;
    await sharp(src)
      .resize(inner, inner, { fit: 'contain', background: { r: 0, g: 0, b: 0, alpha: 0 } })
      .extend({ top: pad, bottom: pad, left: pad, right: pad,
                background: { r: 13, g: 13, b: 13, alpha: 255 } })
      .png()
      .toFile(path.join(dir, `${name}.png`));
    process.stdout.write('.');
  }

  // og-image 1200×630 — logo centered on dark bg with grid
  const logoSize = 280;
  const ogW = 1200, ogH = 630;
  const logoResized = await sharp(src)
    .resize(logoSize, logoSize, { fit: 'contain', background: { r: 0, g: 0, b: 0, alpha: 0 } })
    .png()
    .toBuffer();

  const gridSVG = Buffer.from(`<svg width="${ogW}" height="${ogH}" xmlns="http://www.w3.org/2000/svg">
    <rect width="${ogW}" height="${ogH}" fill="#0d0d0d"/>
    <defs>
      <pattern id="g" width="40" height="40" patternUnits="userSpaceOnUse">
        <path d="M 40 0 L 0 0 0 40" fill="none" stroke="rgba(255,255,255,0.03)" stroke-width="1"/>
      </pattern>
    </defs>
    <rect width="${ogW}" height="${ogH}" fill="url(#g)"/>
    <rect x="0" y="0" width="6" height="${ogH}" fill="#DC143C"/>
    <rect x="0" y="${ogH - 4}" width="${ogW}" height="4" fill="#FFB000" opacity="0.6"/>
  </svg>`);

  await sharp(gridSVG)
    .composite([{
      input: logoResized,
      top:  Math.round((ogH - logoSize) / 2),
      left: Math.round((ogW - logoSize) / 2),
    }])
    .png()
    .toFile(path.join(dir, 'og-image.png'));

  // og-image square 1200×1200
  const logoSq = 420;
  const logoSqBuf = await sharp(src)
    .resize(logoSq, logoSq, { fit: 'contain', background: { r: 0, g: 0, b: 0, alpha: 0 } })
    .png()
    .toBuffer();
  const gridSq = Buffer.from(`<svg width="1200" height="1200" xmlns="http://www.w3.org/2000/svg">
    <rect width="1200" height="1200" fill="#0d0d0d"/>
    <defs>
      <pattern id="g" width="40" height="40" patternUnits="userSpaceOnUse">
        <path d="M 40 0 L 0 0 0 40" fill="none" stroke="rgba(255,255,255,0.03)" stroke-width="1"/>
      </pattern>
    </defs>
    <rect width="1200" height="1200" fill="url(#g)"/>
    <rect x="0" y="0" width="6" height="1200" fill="#DC143C"/>
  </svg>`);
  await sharp(gridSq)
    .composite([{ input: logoSqBuf, top: Math.round((1200 - logoSq) / 2), left: Math.round((1200 - logoSq) / 2) }])
    .png()
    .toFile(path.join(dir, 'og-image-square.png'));

  process.stdout.write('.');
  console.log(`\n  ✓ DAW thumbs + og-image (1200×630) + og-image-square (1200×1200)`);
}

// ── MAIN ─────────────────────────────────────────────────────────────────────

async function main() {
  console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
  console.log('  WhyCremisi — Icon Build System');
  console.log(`  Source: ${path.basename(SOURCE)}`);
  console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');

  if (!fs.existsSync(SOURCE)) {
    console.error(`✗  Source not found: ${SOURCE}`);
    process.exit(1);
  }

  fs.mkdirSync(OUT, { recursive: true });

  // Get source metadata
  const meta = await sharp(SOURCE).metadata();
  console.log(`  Size: ${meta.width}×${meta.height} · Format: ${meta.format}`);
  if (meta.width < 512 || meta.height < 512) {
    console.warn('  ⚠ Warning: source is smaller than 512px — quality may be reduced');
  }

  console.log('\n▸ macOS (Liquid Glass)');
  await buildMacOS(SOURCE);

  console.log('\n▸ Windows (Fluent flat)');
  await buildWindows(SOURCE);

  console.log('\n▸ Linux (transparent, clean)');
  await buildLinux(SOURCE);

  console.log('\n▸ Favicon & Web');
  await buildFavicon(SOURCE);

  console.log('\n▸ DAW & Social');
  await buildDAW(SOURCE);

  // Report
  const allFiles = [];
  function countFiles(dir) {
    if (!fs.existsSync(dir)) return;
    for (const f of fs.readdirSync(dir, { withFileTypes: true })) {
      if (f.isDirectory()) countFiles(path.join(dir, f.name));
      else allFiles.push(path.join(dir, f.name));
    }
  }
  countFiles(OUT);

  console.log('\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
  console.log(`  ✓ Done — ${allFiles.length} files generated in logo/output/`);
  console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
  console.log('');
  console.log('  output/');
  console.log('    macos/     AppIcon.iconset/ + AppIcon.icns');
  console.log('    windows/   win-{size}.png (16→310)');
  console.log('    linux/     linux-{size}.png + whycremisi.svg placeholder');
  console.log('    favicon/   favicon-{size}.png + apple-touch-icon + manifest');
  console.log('    daw/       daw-thumb.png + og-image.png + og-image-square.png');
}

main().catch(err => { console.error(err); process.exit(1); });
