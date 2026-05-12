#!/usr/bin/env node
// WhyCremisi Research Papers — PDF Build System
// Puppeteer + marked → branded PDFs with SVG diagrams

const puppeteer = require('puppeteer');
const { marked }  = require('marked');
const fs   = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');

const PAPERS = [
  { src: 'IT/markdown', out: 'IT/pdf', lang: 'IT' },
  { src: 'EN/markdown', out: 'EN/pdf', lang: 'EN' },
];

// ── HTML TEMPLATE ────────────────────────────────────────────────────────────
function buildHTML(paperNumber, title, subtitle, lang, bodyHtml) {
  return `<!DOCTYPE html>
<html lang="${lang.toLowerCase()}">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width">
<title>${title}</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700;900&family=JetBrains+Mono:ital,wght@0,400;0,500;0,700;1,400&display=swap" rel="stylesheet">
<style>
/* ── RESET & BASE ───────────────────────────────────────── */
*, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

:root {
  --cr:    #DC143C;
  --amber: #FFB000;
  --bg:    #0d0d0d;
  --panel: #161616;
  --card:  #1c1c1c;
  --edge:  #282828;
  --edge2: #333333;
  --text:  #d8d8d8;
  --muted: #777777;
  --dim:   #444444;
  --white: #ffffff;
  --green: #22c55e;
}

@page {
  size: A4;
  margin: 0;
}

html { background: var(--bg); }

body {
  font-family: 'Inter', sans-serif;
  font-size: 9.5pt;
  line-height: 1.75;
  color: var(--text);
  background: var(--bg);
  width: 210mm;
  min-height: 297mm;
}

/* ── COVER ──────────────────────────────────────────────── */
.cover {
  width: 100%;
  background: var(--cr);
  padding: 18mm 22mm 0 22mm;
  position: relative;
  overflow: hidden;
}

.cover-noise {
  position: absolute;
  inset: 0;
  background-image:
    radial-gradient(circle at 80% 20%, rgba(255,255,255,0.06) 0%, transparent 50%),
    radial-gradient(circle at 10% 80%, rgba(0,0,0,0.2) 0%, transparent 40%);
  pointer-events: none;
}

.cover-grid {
  position: absolute;
  inset: 0;
  background-image:
    linear-gradient(rgba(255,255,255,0.04) 1px, transparent 1px),
    linear-gradient(90deg, rgba(255,255,255,0.04) 1px, transparent 1px);
  background-size: 20px 20px;
  pointer-events: none;
}

.cover-meta {
  font-family: 'JetBrains Mono', monospace;
  font-size: 7pt;
  font-weight: 500;
  color: rgba(255,255,255,0.55);
  letter-spacing: 0.18em;
  text-transform: uppercase;
  margin-bottom: 10px;
  position: relative;
  z-index: 1;
  display: flex;
  align-items: center;
  gap: 14px;
}
.cover-meta::before {
  content: '';
  display: inline-block;
  width: 24px;
  height: 2px;
  background: var(--amber);
  flex-shrink: 0;
}

.cover-title {
  font-size: 28pt;
  font-weight: 900;
  color: var(--white);
  line-height: 1.1;
  letter-spacing: -0.03em;
  margin-bottom: 8px;
  position: relative;
  z-index: 1;
  max-width: 85%;
}

.cover-subtitle {
  font-size: 11pt;
  font-weight: 300;
  color: rgba(255,255,255,0.65);
  font-style: italic;
  margin-bottom: 18mm;
  position: relative;
  z-index: 1;
}

.cover-stripe {
  height: 4px;
  background: linear-gradient(90deg, var(--amber) 0%, rgba(255,176,0,0.3) 100%);
  width: 100%;
  position: relative;
  z-index: 1;
}

/* ── BODY CONTENT ───────────────────────────────────────── */
.content {
  padding: 12mm 22mm 18mm 22mm;
}

/* ── HEADINGS ───────────────────────────────────────────── */
h1 { display: none; } /* handled by .cover */

h2 {
  font-size: 12pt;
  font-weight: 700;
  color: var(--white);
  margin: 28px 0 10px;
  padding: 9px 14px 9px 16px;
  background: var(--panel);
  border-left: 3px solid var(--cr);
  border-radius: 0 4px 4px 0;
  break-after: avoid;
  display: flex;
  align-items: center;
  gap: 10px;
}
h2::before {
  content: '';
  display: inline-block;
  width: 6px;
  height: 6px;
  background: var(--cr);
  border-radius: 50%;
  flex-shrink: 0;
}

h3 {
  font-size: 9.5pt;
  font-weight: 700;
  color: var(--amber);
  text-transform: uppercase;
  letter-spacing: 0.1em;
  margin: 20px 0 6px;
  display: flex;
  align-items: center;
  gap: 8px;
  break-after: avoid;
}
h3::before {
  content: '';
  display: inline-block;
  width: 14px;
  height: 1.5px;
  background: var(--amber);
  flex-shrink: 0;
}

h4 {
  font-size: 9pt;
  font-weight: 600;
  color: #999;
  margin: 14px 0 4px;
  break-after: avoid;
}

/* ── PARAGRAPHS ─────────────────────────────────────────── */
p {
  margin-bottom: 10px;
  color: var(--text);
  text-align: justify;
  hyphens: auto;
}

strong { color: var(--white); font-weight: 700; }
em     { color: var(--amber); font-style: italic; }

/* ── CODE BLOCKS ────────────────────────────────────────── */
pre {
  background: var(--panel);
  border: 1px solid var(--edge);
  border-left: 3px solid var(--cr);
  border-radius: 0 6px 6px 0;
  padding: 14px 16px;
  margin: 14px 0;
  overflow-x: auto;
  break-inside: avoid;
  position: relative;
}

pre::before {
  content: '';
  position: absolute;
  top: 10px;
  right: 14px;
  width: 8px;
  height: 8px;
  background: var(--cr);
  border-radius: 50%;
  opacity: 0.6;
  box-shadow: -12px 0 0 rgba(255,176,0,0.5), -24px 0 0 rgba(34,197,94,0.4);
}

pre code {
  font-family: 'JetBrains Mono', monospace;
  font-size: 7.5pt;
  color: #cdd6f4;
  background: transparent;
  padding: 0;
  border: none;
  line-height: 1.7;
  display: block;
  white-space: pre;
}

code {
  font-family: 'JetBrains Mono', monospace;
  font-size: 8pt;
  color: var(--amber);
  background: rgba(255,176,0,0.08);
  padding: 1px 5px;
  border-radius: 3px;
  border: 1px solid rgba(255,176,0,0.15);
}

/* ── TABLES ─────────────────────────────────────────────── */
table {
  width: 100%;
  border-collapse: separate;
  border-spacing: 0;
  margin: 16px 0;
  font-size: 8.5pt;
  break-inside: avoid;
  border-radius: 8px;
  overflow: hidden;
  border: 1px solid var(--edge);
  box-shadow: 0 4px 20px rgba(0,0,0,0.4);
}

thead tr {
  background: linear-gradient(135deg, var(--cr) 0%, #a00028 100%);
}
thead th {
  color: var(--white);
  font-family: 'JetBrains Mono', monospace;
  font-size: 7pt;
  font-weight: 700;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  padding: 10px 14px;
  text-align: left;
  border: none;
}
thead th:first-child { border-radius: 0; }

tbody tr:nth-child(odd)  { background: var(--panel); }
tbody tr:nth-child(even) { background: var(--card); }

tbody td {
  color: var(--text);
  padding: 8px 14px;
  border: none;
  border-bottom: 1px solid var(--edge);
  vertical-align: top;
}
tbody td:first-child {
  color: var(--amber);
  font-family: 'JetBrains Mono', monospace;
  font-size: 8pt;
  font-weight: 500;
}
tbody tr:last-child td { border-bottom: none; }

/* ── BLOCKQUOTE ─────────────────────────────────────────── */
blockquote {
  border-left: 3px solid var(--amber);
  background: linear-gradient(90deg, rgba(255,176,0,0.06) 0%, transparent 100%);
  padding: 12px 16px;
  margin: 16px 0;
  border-radius: 0 6px 6px 0;
  break-inside: avoid;
  position: relative;
}
blockquote::before {
  content: '"';
  font-size: 48pt;
  line-height: 0;
  color: rgba(255,176,0,0.15);
  font-family: Georgia, serif;
  position: absolute;
  top: 32px;
  left: 10px;
}
blockquote p {
  color: #ccc;
  font-style: italic;
  margin: 0;
  padding-left: 20px;
}

/* ── LISTS ──────────────────────────────────────────────── */
ul, ol { padding-left: 8px; margin: 8px 0 12px 0; }

ul li {
  color: var(--text);
  margin-bottom: 5px;
  list-style: none;
  padding-left: 16px;
  position: relative;
}
ul li::before {
  content: '▸';
  color: var(--cr);
  position: absolute;
  left: 0;
  font-size: 8pt;
  line-height: 1.75;
}
ul ul li::before { content: '◦'; color: var(--amber); }
ul ul ul li::before { content: '–'; color: var(--dim); }

ol { padding-left: 20px; }
ol li { color: var(--text); margin-bottom: 5px; }
ol li::marker { color: var(--cr); font-weight: 700; }

/* ── HR ─────────────────────────────────────────────────── */
hr {
  border: none;
  height: 1px;
  background: linear-gradient(90deg, var(--cr), var(--amber), transparent);
  margin: 24px 0;
  opacity: 0.4;
}

/* ── LINKS ──────────────────────────────────────────────── */
a { color: var(--amber); text-decoration: none; }

/* ── PAGE FOOTER ────────────────────────────────────────── */
.page-footer {
  position: fixed;
  bottom: 0;
  left: 0;
  right: 0;
  height: 14mm;
  background: var(--panel);
  border-top: 1px solid var(--edge);
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0 22mm;
}
.page-footer-left {
  font-family: 'JetBrains Mono', monospace;
  font-size: 6.5pt;
  color: var(--dim);
  letter-spacing: 0.08em;
  text-transform: uppercase;
}
.page-footer-center {
  display: flex;
  align-items: center;
  gap: 6px;
}
.footer-dot {
  width: 6px;
  height: 6px;
  background: var(--cr);
  border-radius: 50%;
}
.footer-name {
  font-family: 'JetBrains Mono', monospace;
  font-size: 7pt;
  font-weight: 700;
  color: var(--cr);
  letter-spacing: 0.12em;
}
.page-footer-right {
  font-family: 'JetBrains Mono', monospace;
  font-size: 6.5pt;
  color: var(--dim);
}

/* ── DIAGRAM BOXES ──────────────────────────────────────── */
.diagram-box {
  background: var(--panel);
  border: 1px solid var(--edge2);
  border-radius: 8px;
  padding: 20px;
  margin: 16px 0;
  break-inside: avoid;
}
.diagram-title {
  font-family: 'JetBrains Mono', monospace;
  font-size: 7pt;
  color: var(--cr);
  text-transform: uppercase;
  letter-spacing: 0.14em;
  margin-bottom: 14px;
  padding-bottom: 8px;
  border-bottom: 1px solid var(--edge);
  display: flex;
  align-items: center;
  gap: 8px;
}
.diagram-title::before {
  content: '◈';
  color: var(--amber);
}

/* ── CALLOUT ────────────────────────────────────────────── */
.callout {
  background: rgba(220,20,60,0.08);
  border: 1px solid rgba(220,20,60,0.25);
  border-radius: 6px;
  padding: 12px 16px;
  margin: 14px 0;
}
.callout-label {
  font-family: 'JetBrains Mono', monospace;
  font-size: 7pt;
  font-weight: 700;
  color: var(--cr);
  text-transform: uppercase;
  letter-spacing: 0.1em;
  margin-bottom: 4px;
}

/* ── PRINT ──────────────────────────────────────────────── */
@media print {
  body { margin-bottom: 14mm; }
  h2, h3, h4 { break-after: avoid; }
  pre, table, blockquote { break-inside: avoid; }
}
</style>
</head>
<body>

<div class="cover">
  <div class="cover-noise"></div>
  <div class="cover-grid"></div>
  <div class="cover-meta">WHYCREMISI · AI MIX ASSISTANT &nbsp;·&nbsp; PAPER ${paperNumber} &nbsp;·&nbsp; ${lang}</div>
  <div class="cover-title">${title}</div>
  <div class="cover-subtitle">${subtitle}</div>
  <div class="cover-stripe"></div>
</div>

<div class="content">
${bodyHtml}
</div>

<div class="page-footer">
  <div class="page-footer-left">WhyCremisi Research Papers</div>
  <div class="page-footer-center">
    <div class="footer-dot"></div>
    <div class="footer-name">WHYCREMISI</div>
    <div class="footer-dot"></div>
  </div>
  <div class="page-footer-right">Paper ${paperNumber} · ${lang}</div>
</div>

</body>
</html>`;
}

// ── METADATA PER PAPER ────────────────────────────────────────────────────────
function extractMeta(filename, content) {
  const lines = content.split('\n');
  const h1 = lines.find(l => l.startsWith('# '))?.replace(/^# /, '') || filename;
  const h2 = lines.find(l => l.startsWith('## '))?.replace(/^## /, '') || '';
  const num = filename.match(/^(\d+)/)?.[1] || '00';
  // Extract title (everything after "Paper NN — " if present)
  const titleClean = h1.replace(/^Paper \d+ — /, '').replace(/^Paper \d+.*?—\s*/, '');
  return { title: titleClean || h1, subtitle: h2, num };
}

// ── PROCESS MARKDOWN ──────────────────────────────────────────────────────────
function processMarkdown(content) {
  // Remove the first h1 (shown in cover) and first h2 (shown as subtitle)
  let md = content
    .replace(/^# .+\n/, '')
    .replace(/^## .+\n/, '');

  // Remove the code fences wrapping the paper header box
  md = md.replace(/^```\n─+[\s\S]*?─+\n```\n/m, '');

  // Remove navigation links at bottom
  md = md.replace(/^\*→ Continue:.*\*$/m, '');
  md = md.replace(/^\*→ End of.*\*$/m, '');
  md = md.replace(/^\*→ Return to.*\*$/m, '');

  return marked.parse(md);
}

// ── MAIN ──────────────────────────────────────────────────────────────────────
async function main() {
  console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
  console.log('  WhyCremisi — PDF Build (Puppeteer)');
  console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');

  const browser = await puppeteer.launch({
    headless: true,
    args: ['--no-sandbox', '--disable-setuid-sandbox'],
  });

  for (const { src, out, lang } of PAPERS) {
    const srcDir = path.join(ROOT, src);
    const outDir = path.join(ROOT, out);
    fs.mkdirSync(outDir, { recursive: true });

    const files = fs.readdirSync(srcDir)
      .filter(f => f.endsWith('.md'))
      .sort();

    console.log(`\n▸ ${lang} — ${files.length} papers`);

    for (const file of files) {
      const base    = file.replace('.md', '');
      const content = fs.readFileSync(path.join(srcDir, file), 'utf8');
      const { title, subtitle, num } = extractMeta(base, content);
      const bodyHtml = processMarkdown(content);
      const html = buildHTML(num, title, subtitle, lang, bodyHtml);

      const outFile = path.join(outDir, base + '.pdf');
      console.log(`  → ${base}.pdf`);

      const page = await browser.newPage();
      await page.setContent(html, { waitUntil: 'networkidle0', timeout: 60000 });
      await page.pdf({
        path: outFile,
        format: 'A4',
        printBackground: true,
        margin: { top: '0', bottom: '14mm', left: '0', right: '0' },
      });
      await page.close();
    }
  }

  await browser.close();
  console.log('\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
  console.log('  Done.');
}

main().catch(err => { console.error(err); process.exit(1); });
