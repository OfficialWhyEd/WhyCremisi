#!/usr/bin/env node
// WhyCremisi — PDF Build System v3
// Puppeteer + marked + SVG diagrams (no ASCII art)

const puppeteer = require('puppeteer');
const { marked } = require('marked');
const fs   = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const PAPERS = [
  { src: 'italiano/markdown', out: 'italiano/pdf', lang: 'IT' },
  { src: 'inglese/markdown',  out: 'inglese/pdf',  lang: 'EN' },
];

// ── SVG DIAGRAMS ─────────────────────────────────────────────────────────────
// Each returns an HTML string with a real SVG diagram

const DIAGRAMS = {

  architecture: () => `
<div class="diagram">
  <div class="diagram-label">System Architecture</div>
  <svg viewBox="0 0 700 480" xmlns="http://www.w3.org/2000/svg" style="width:100%;max-width:700px;display:block;margin:0 auto">
    <defs>
      <marker id="arr" markerWidth="8" markerHeight="6" refX="8" refY="3" orient="auto">
        <polygon points="0 0,8 3,0 6" fill="#DC143C" opacity="0.7"/>
      </marker>
      <filter id="glow"><feGaussianBlur stdDeviation="2" result="b"/><feMerge><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge></filter>
    </defs>
    <!-- UI Layer -->
    <rect x="30" y="20" width="640" height="70" rx="8" fill="#1c1c1c" stroke="#DC143C" stroke-width="1.5"/>
    <text x="350" y="43" text-anchor="middle" fill="#DC143C" font-family="JetBrains Mono" font-size="9" font-weight="700" letter-spacing="2">UI LAYER</text>
    <text x="350" y="62" text-anchor="middle" fill="#888" font-family="Inter" font-size="10">React 18 · Chat · BoxChat · BotFace · Transport · Telemetry</text>
    <text x="350" y="79" text-anchor="middle" fill="#555" font-family="JetBrains Mono" font-size="8">WebView (WKWebView on macOS)</text>
    <!-- Arrow down -->
    <line x1="350" y1="90" x2="350" y2="118" stroke="#DC143C" stroke-width="1.5" marker-end="url(#arr)" opacity="0.6"/>
    <text x="370" y="108" fill="#555" font-family="JetBrains Mono" font-size="8">WebSocket JSON :8080</text>
    <!-- Bridge Layer -->
    <rect x="30" y="120" width="640" height="60" rx="8" fill="#1c1c1c" stroke="#FFB000" stroke-width="1.5"/>
    <text x="350" y="143" text-anchor="middle" fill="#FFB000" font-family="JetBrains Mono" font-size="9" font-weight="700" letter-spacing="2">BRIDGE LAYER</text>
    <text x="350" y="163" text-anchor="middle" fill="#888" font-family="Inter" font-size="10">WebSocketServer · WebViewBridge · OscBridge · C++ / JUCE 8</text>
    <!-- Arrow down — three branches -->
    <line x1="180" y1="180" x2="180" y2="208" stroke="#FFB000" stroke-width="1.5" marker-end="url(#arr)" opacity="0.5"/>
    <line x1="350" y1="180" x2="350" y2="208" stroke="#FFB000" stroke-width="1.5" marker-end="url(#arr)" opacity="0.5"/>
    <line x1="520" y1="180" x2="520" y2="208" stroke="#FFB000" stroke-width="1.5" marker-end="url(#arr)" opacity="0.5"/>
    <!-- Three engine boxes -->
    <rect x="30" y="210" width="200" height="90" rx="8" fill="#161616" stroke="#333" stroke-width="1"/>
    <text x="130" y="233" text-anchor="middle" fill="#e0e0e0" font-family="JetBrains Mono" font-size="8" font-weight="700">AI ENGINE</text>
    <text x="130" y="252" text-anchor="middle" fill="#666" font-family="Inter" font-size="8.5">Multi-provider</text>
    <text x="130" y="267" text-anchor="middle" fill="#555" font-family="Inter" font-size="8">Ollama · OpenAI</text>
    <text x="130" y="282" text-anchor="middle" fill="#555" font-family="Inter" font-size="8">Claude · Gemini</text>
    <rect x="250" y="210" width="200" height="90" rx="8" fill="#161616" stroke="#333" stroke-width="1"/>
    <text x="350" y="233" text-anchor="middle" fill="#e0e0e0" font-family="JetBrains Mono" font-size="8" font-weight="700">DSP ENGINE</text>
    <text x="350" y="252" text-anchor="middle" fill="#666" font-family="Inter" font-size="8.5">Analyzer · EQ</text>
    <text x="350" y="267" text-anchor="middle" fill="#555" font-family="Inter" font-size="8">Compressor · Limiter</text>
    <text x="350" y="282" text-anchor="middle" fill="#555" font-family="Inter" font-size="8">FFT · LUFS · Meters</text>
    <rect x="470" y="210" width="200" height="90" rx="8" fill="#161616" stroke="#333" stroke-width="1"/>
    <text x="570" y="233" text-anchor="middle" fill="#e0e0e0" font-family="JetBrains Mono" font-size="8" font-weight="700">AGENT WORKSPACE</text>
    <text x="570" y="252" text-anchor="middle" fill="#666" font-family="Inter" font-size="8.5">PersonalityCore</text>
    <text x="570" y="267" text-anchor="middle" fill="#555" font-family="Inter" font-size="8">OpenClaw Memory</text>
    <text x="570" y="282" text-anchor="middle" fill="#555" font-family="Inter" font-size="8">Session Recorder</text>
    <!-- Arrow down merge -->
    <line x1="180" y1="300" x2="180" y2="328" stroke="#444" stroke-width="1.5" marker-end="url(#arr)" opacity="0.5"/>
    <line x1="350" y1="300" x2="350" y2="328" stroke="#444" stroke-width="1.5" marker-end="url(#arr)" opacity="0.5"/>
    <line x1="520" y1="300" x2="520" y2="328" stroke="#444" stroke-width="1.5" marker-end="url(#arr)" opacity="0.5"/>
    <!-- DAW Layer -->
    <rect x="30" y="330" width="640" height="60" rx="8" fill="#1c1c1c" stroke="#555" stroke-width="1"/>
    <text x="350" y="353" text-anchor="middle" fill="#aaa" font-family="JetBrains Mono" font-size="9" font-weight="700" letter-spacing="2">DAW LAYER</text>
    <text x="350" y="373" text-anchor="middle" fill="#666" font-family="Inter" font-size="10">PluginProcessor · ParameterMapper · VST3 Automation / OSC</text>
    <!-- Arrow down -->
    <line x1="350" y1="390" x2="350" y2="418" stroke="#444" stroke-width="1.5" marker-end="url(#arr)" opacity="0.5"/>
    <!-- Host Layer -->
    <rect x="30" y="420" width="640" height="50" rx="8" fill="#0a0a0a" stroke="#2a2a2a" stroke-width="1"/>
    <text x="350" y="441" text-anchor="middle" fill="#666" font-family="JetBrains Mono" font-size="8" font-weight="700" letter-spacing="2">DAW HOST + THIRD-PARTY PLUGINS</text>
    <text x="350" y="458" text-anchor="middle" fill="#444" font-family="Inter" font-size="9">Ableton · Logic · Reaper · FL Studio · FabFilter · iZotope · Waves</text>
  </svg>
</div>`,

  memoryLayers: () => `
<div class="diagram">
  <div class="diagram-label">OpenClaw Memory — 5 Layer Architecture</div>
  <svg viewBox="0 0 640 320" xmlns="http://www.w3.org/2000/svg" style="width:100%;max-width:640px;display:block;margin:0 auto">
    <defs>
      <linearGradient id="l4" x1="0" y1="0" x2="1" y2="0"><stop offset="0%" stop-color="#DC143C" stop-opacity="0.9"/><stop offset="100%" stop-color="#DC143C" stop-opacity="0.5"/></linearGradient>
      <linearGradient id="l3" x1="0" y1="0" x2="1" y2="0"><stop offset="0%" stop-color="#b01030" stop-opacity="0.8"/><stop offset="100%" stop-color="#b01030" stop-opacity="0.4"/></linearGradient>
      <linearGradient id="l2" x1="0" y1="0" x2="1" y2="0"><stop offset="0%" stop-color="#FFB000" stop-opacity="0.7"/><stop offset="100%" stop-color="#FFB000" stop-opacity="0.3"/></linearGradient>
      <linearGradient id="l1" x1="0" y1="0" x2="1" y2="0"><stop offset="0%" stop-color="#22c55e" stop-opacity="0.6"/><stop offset="100%" stop-color="#22c55e" stop-opacity="0.2"/></linearGradient>
      <linearGradient id="l0" x1="0" y1="0" x2="1" y2="0"><stop offset="0%" stop-color="#3b82f6" stop-opacity="0.5"/><stop offset="100%" stop-color="#3b82f6" stop-opacity="0.2"/></linearGradient>
    </defs>
    ${[
      ['LAYER 4','Identity — Permanent','Who the agent is. Core values. Immutable.','l4','#DC143C',16,54],
      ['LAYER 3','Personality — Slow','Communication style. Evolves gradually.','l3','#c01235',70,108],
      ['LAYER 2','User Profile — Medium','Who the user is. Preferences. Plugins.','l2','#FFB000',124,162],
      ['LAYER 1','Session Memory — Daily','What happened today. Actions, results.','l1','#22c55e',178,216],
      ['LAYER 0','Real-Time Context — Volatile','BPM, meters, active plugins. Ephemeral.','l0','#3b82f6',232,270],
    ].map(([badge, title, desc, grad, col, y1, y2]) => `
      <rect x="30" y="${y1}" width="580" height="${y2-y1}" rx="6" fill="url(#${grad})"/>
      <rect x="30" y="${y1}" width="580" height="${y2-y1}" rx="6" fill="none" stroke="${col}" stroke-width="1" opacity="0.4"/>
      <rect x="30" y="${y1}" width="88" height="${y2-y1}" rx="6" fill="${col}" opacity="0.15"/>
      <text x="74" y="${y1+(y2-y1)/2+4}" text-anchor="middle" fill="${col}" font-family="JetBrains Mono" font-size="8" font-weight="700">${badge}</text>
      <text x="130" y="${y1+(y2-y1)/2-5}" fill="#e0e0e0" font-family="Inter" font-size="10" font-weight="600">${title}</text>
      <text x="130" y="${y1+(y2-y1)/2+10}" fill="#888" font-family="Inter" font-size="8.5">${desc}</text>
    `).join('')}
  </svg>
</div>`,

  pluginControl: () => `
<div class="diagram">
  <div class="diagram-label">Plugin Control Flow</div>
  <svg viewBox="0 0 700 200" xmlns="http://www.w3.org/2000/svg" style="width:100%;max-width:700px;display:block;margin:0 auto">
    <defs>
      <marker id="a2" markerWidth="8" markerHeight="6" refX="8" refY="3" orient="auto">
        <polygon points="0 0,8 3,0 6" fill="#FFB000"/>
      </marker>
    </defs>
    ${[
      [30, 'AI ENGINE', 'Generates\ncommand', '#DC143C'],
      [190, 'PARAMETER\nMAPPER', 'Translates &\nvalidates', '#FFB000'],
      [360, 'VST3 HOST\nBRIDGE', 'Applies via\nautomation', '#888'],
      [530, 'PLUGIN\nINSTANCE', 'FabFilter\niZotope etc.', '#555'],
    ].map(([x, title, sub, col]) => `
      <rect x="${x}" y="40" width="130" height="120" rx="8" fill="#161616" stroke="${col}" stroke-width="1.5"/>
      <rect x="${x}" y="40" width="130" height="30" rx="8" fill="${col}" opacity="0.15"/>
      <text x="${x+65}" y="60" text-anchor="middle" fill="${col}" font-family="JetBrains Mono" font-size="8" font-weight="700">${title.split('\n')[0]}</text>
      ${title.split('\n')[1] ? `<text x="${x+65}" y="72" text-anchor="middle" fill="${col}" font-family="JetBrains Mono" font-size="8" font-weight="700">${title.split('\n')[1]}</text>` : ''}
      <text x="${x+65}" y="${sub.split('\n').length > 1 ? 108 : 115}" text-anchor="middle" fill="#666" font-family="Inter" font-size="8.5">${sub.split('\n')[0]}</text>
      ${sub.split('\n')[1] ? `<text x="${x+65}" y="122" text-anchor="middle" fill="#666" font-family="Inter" font-size="8.5">${sub.split('\n')[1]}</text>` : ''}
    `).join('')}
    <line x1="160" y1="100" x2="188" y2="100" stroke="#FFB000" stroke-width="1.5" marker-end="url(#a2)"/>
    <line x1="320" y1="100" x2="358" y2="100" stroke="#FFB000" stroke-width="1.5" marker-end="url(#a2)"/>
    <line x1="490" y1="100" x2="528" y2="100" stroke="#FFB000" stroke-width="1.5" marker-end="url(#a2)"/>
  </svg>
</div>`,

  roadmap: () => `
<div class="diagram">
  <div class="diagram-label">Development Roadmap</div>
  <svg viewBox="0 0 680 280" xmlns="http://www.w3.org/2000/svg" style="width:100%;max-width:680px;display:block;margin:0 auto">
    <line x1="60" y1="50" x2="60" y2="260" stroke="#282828" stroke-width="2"/>
    ${[
      ['Alpha','Foundations','✓ Complete','#22c55e',60,
        ['VST3 + Standalone macOS','WebSocket bridge · AI streaming','BotFace · BoxChat · Memory','DSP Engine · Setup screen']],
      ['Beta','Plugin Control','In Progress','#FFB000',120,
        ['ParameterMapper + FabFilter DB','iZotope Ozone 11 + Neutron 4','Plugin browser UI · Undo stack','Windows & Linux build']],
      ['1.0','Complete Product','Planned','#DC143C',190,
        ['100+ plugins mapped','VST3 auto-discovery','A/B comparison · Reference track','Zero-dependency installer']],
      ['2.0','Ecosystem','Future','#555',250,
        ['Public plugin API','Community database','Multi-agent · Cloud sync','Third-party developers']],
    ].map(([phase, title, status, col, y, items]) => `
      <circle cx="60" cy="${y}" r="10" fill="${col}" opacity="${status==='✓ Complete'?1:status==='In Progress'?0.85:0.5}"/>
      <text x="60" cy="${y}" text-anchor="middle" dominant-baseline="middle" fill="${col==='#22c55e'?'#000':'#fff'}" font-family="JetBrains Mono" font-size="7" font-weight="700" y="${y+1}">▸</text>
      <rect x="82" y="${y-26}" width="570" height="54" rx="6" fill="#161616" stroke="${col}" stroke-width="1" opacity="${status==='In Progress'?1:0.6}"/>
      <text x="96" y="${y-10}" fill="${col}" font-family="JetBrains Mono" font-size="8" font-weight="700">Phase ${phase}</text>
      <text x="180" y="${y-10}" fill="#888" font-family="Inter" font-size="9">${title}</text>
      <rect x="480" y="${y-20}" width="160" height="16" rx="3" fill="${col}" opacity="0.15"/>
      <text x="560" y="${y-10}" text-anchor="middle" fill="${col}" font-family="JetBrains Mono" font-size="7.5" font-weight="700">${status.toUpperCase()}</text>
      <text x="96" y="${y+8}" fill="#555" font-family="Inter" font-size="8">${items[0]} · ${items[1]}</text>
      <text x="96" y="${y+19}" fill="#444" font-family="Inter" font-size="7.5">${items[2]} · ${items[3]}</text>
    `).join('')}
  </svg>
</div>`,

  messageFlow: () => `
<div class="diagram">
  <div class="diagram-label">WebSocket Message Flow</div>
  <svg viewBox="0 0 660 220" xmlns="http://www.w3.org/2000/svg" style="width:100%;max-width:660px;display:block;margin:0 auto">
    <defs>
      <marker id="a3" markerWidth="7" markerHeight="5" refX="7" refY="2.5" orient="auto">
        <polygon points="0 0,7 2.5,0 5" fill="#DC143C" opacity="0.8"/>
      </marker>
      <marker id="a4" markerWidth="7" markerHeight="5" refX="7" refY="2.5" orient="auto">
        <polygon points="0 0,7 2.5,0 5" fill="#FFB000" opacity="0.8"/>
      </marker>
    </defs>
    <!-- React UI box -->
    <rect x="20" y="20" width="160" height="180" rx="8" fill="#1a1a1a" stroke="#DC143C" stroke-width="1.5"/>
    <text x="100" y="44" text-anchor="middle" fill="#DC143C" font-family="JetBrains Mono" font-size="8" font-weight="700">REACT UI</text>
    ${['ai.prompt','daw.command','plugin.control','config.set','plugin.init'].map((m,i)=>`
      <rect x="34" y="${60+i*24}" width="132" height="18" rx="3" fill="#0d0d0d" stroke="#2a2a2a" stroke-width="1"/>
      <text x="100" y="${72+i*24}" text-anchor="middle" fill="#888" font-family="JetBrains Mono" font-size="7.5">${m}</text>
    `).join('')}
    <!-- C++ Plugin box -->
    <rect x="480" y="20" width="160" height="180" rx="8" fill="#1a1a1a" stroke="#FFB000" stroke-width="1.5"/>
    <text x="560" y="44" text-anchor="middle" fill="#FFB000" font-family="JetBrains Mono" font-size="8" font-weight="700">C++ PLUGIN</text>
    ${['ai.stream','ai.response','daw.transport','daw.meter','plugin.state'].map((m,i)=>`
      <rect x="494" y="${60+i*24}" width="132" height="18" rx="3" fill="#0d0d0d" stroke="#2a2a2a" stroke-width="1"/>
      <text x="560" y="${72+i*24}" text-anchor="middle" fill="#666" font-family="JetBrains Mono" font-size="7.5">${m}</text>
    `).join('')}
    <!-- WebSocket channel -->
    <rect x="222" y="80" width="216" height="60" rx="8" fill="#0d0d0d" stroke="#333" stroke-width="1"/>
    <text x="330" y="104" text-anchor="middle" fill="#444" font-family="JetBrains Mono" font-size="7" font-weight="700">WEBSOCKET</text>
    <text x="330" y="118" text-anchor="middle" fill="#333" font-family="JetBrains Mono" font-size="8">ws://localhost:8080</text>
    <text x="330" y="132" text-anchor="middle" fill="#2a2a2a" font-family="JetBrains Mono" font-size="7">JSON · &lt; 5ms RTT</text>
    <!-- arrows -->
    <line x1="180" y1="90" x2="220" y2="100" stroke="#DC143C" stroke-width="1.5" marker-end="url(#a3)" opacity="0.7"/>
    <line x1="438" y1="100" x2="478" y2="90" stroke="#FFB000" stroke-width="1.5" marker-end="url(#a4)" opacity="0.7"/>
  </svg>
</div>`,

  installerBundle: () => `
<div class="diagram">
  <div class="diagram-label">Zero-Dependency Installer Bundle</div>
  <svg viewBox="0 0 660 360" xmlns="http://www.w3.org/2000/svg" style="width:100%;max-width:660px;display:block;margin:0 auto">
    ${[
      ['CORE PLUGIN','#DC143C',20,20,
        ['WhyCremisi.vst3 — macOS / Windows / Linux','WhyCremisi.component — AU (macOS / Logic)','WhyCremisi.app / .exe / standalone']],
      ['LOCAL AI ENGINE','#FFB000',20,140,
        ['Ollama runtime — binary per OS','Base model: llama3.2:3b (~2 GB)','Advanced model: llama3.1:8b (~5 GB) opt-in']],
      ['RUNTIMES + UI','#888',350,20,
        ['WebView2 runtime — Windows embedded','VC++ Redistributable — Windows','React UI bundle — pre-compiled, offline']],
      ['PLUGIN DATABASE','#3b82f6',350,140,
        ['plugin-database.json — 100+ plugins','ai-presets.json — intelligent presets','Parameter maps: FabFilter · iZotope · Waves']],
    ].map(([label, col, x, y, items]) => `
      <rect x="${x}" y="${y}" width="290" height="105" rx="8" fill="#161616" stroke="${col}" stroke-width="1.5" opacity="0.9"/>
      <rect x="${x}" y="${y}" width="290" height="24" rx="8" fill="${col}" opacity="0.18"/>
      <rect x="${x}" y="${y+12}" width="290" height="12" fill="${col}" opacity="0.18"/>
      <text x="${x+14}" y="${y+16}" fill="${col}" font-family="JetBrains Mono" font-size="8" font-weight="700" letter-spacing="1">${label}</text>
      ${items.map((item, i) => `<text x="${x+14}" y="${y+44+i*22}" fill="#888" font-family="Inter" font-size="8.5">· ${item}</text>`).join('')}
    `).join('')}
    <!-- Bottom bar -->
    <rect x="20" y="268" width="620" height="72" rx="8" fill="#0d0d0d" stroke="#22c55e" stroke-width="1.5"/>
    <text x="330" y="291" text-anchor="middle" fill="#22c55e" font-family="JetBrains Mono" font-size="8" font-weight="700" letter-spacing="2">FIRST LAUNCH — READY IN &lt; 30 SECONDS</text>
    <text x="330" y="311" text-anchor="middle" fill="#555" font-family="Inter" font-size="9">No additional downloads · No mandatory registration · Works fully offline</text>
    <text x="330" y="328" text-anchor="middle" fill="#444" font-family="Inter" font-size="8.5">macOS .pkg (signed + notarised) · Windows .exe (NSIS) · Linux .deb / .rpm</text>
  </svg>
</div>`,

};

// ── DETECT & REPLACE ASCII DIAGRAMS ──────────────────────────────────────────

function injectDiagrams(html, filename) {
  // Replace ASCII art pre/code blocks with real SVG diagrams based on content
  html = html.replace(/<pre><code>([\s\S]*?)<\/code><\/pre>/g, (match, content) => {
    const t = content;
    // Architecture diagram
    if (/WHYCREMISI ARCHITECTURE|UI LAYER.*React|BRIDGE LAYER.*JUCE|DAW LAYER.*PluginProcessor/.test(t))
      return DIAGRAMS.architecture();
    // Memory layers
    if (/LAYER 4.*Identity|LAYER 3.*Personality|LAYER 2.*User Profile|LAYER 0.*Real.Time/.test(t))
      return DIAGRAMS.memoryLayers();
    // Plugin control
    if (/ParameterMapper.*conceptual|setPluginParameter|lookupParamIndex/.test(t))
      return match; // keep code blocks that are actual code
    // Roadmap phases
    if (/Phase Alpha|Phase Beta|Phase 1\.0|Phase 2\.0|VST3.*Standalone.*macOS.*WebSocket/.test(t))
      return DIAGRAMS.roadmap();
    // Installer bundle
    if (/CORE PLUGIN|LOCAL AI ENGINE|Ollama runtime|plugin-database\.json|INSTALLER BUNDLE/.test(t))
      return DIAGRAMS.installerBundle();
    // Message flow / WebSocket
    if (/ai\.prompt.*provider|plugin\.control.*trackId|daw\.transport.*isPlaying/.test(t))
      return match; // keep message tables as-is (rendered as code)
    // Paper header box (remove completely — shown in cover)
    if (/WHYCREMISI RESEARCH PAPERS|─{20,}/.test(t) && t.length < 300)
      return '';
    return match;
  });

  // Inject architecture diagram after h1 if paper 02
  if (/02-/.test(filename) && !html.includes('class="diagram"')) {
    html = html.replace(/(<h2[^>]*>)/i, DIAGRAMS.architecture() + '$1');
  }
  // Memory layers paper 04
  if (/04-/.test(filename) && !html.includes('class="diagram"')) {
    html = html.replace(/(<h2[^>]*>System Architecture|<h2[^>]*>Architettura|<h2[^>]*>OpenClaw|<h2[^>]*>Architettura OpenClaw)/i,
      DIAGRAMS.memoryLayers() + '$1');
  }
  // Roadmap paper 06
  if (/06-/.test(filename)) {
    html = html.replace(/(<h2[^>]*>Phase|<h2[^>]*>Fase)/i, DIAGRAMS.roadmap() + '$1');
  }
  // Plugin control paper 03
  if (/03-/.test(filename)) {
    html = html.replace(/(<h2[^>]*>Control|<h2[^>]*>Controllo)/i, DIAGRAMS.pluginControl() + '$1');
  }
  // Communication paper 05
  if (/05-/.test(filename)) {
    html = html.replace(/(<h2[^>]*>UI →|<h2[^>]*>Message|<h2[^>]*>Messaggi)/i, DIAGRAMS.messageFlow() + '$1');
  }

  return html;
}

// ── HTML TEMPLATE ─────────────────────────────────────────────────────────────

function buildHTML(num, title, subtitle, lang, bodyHtml) {
  return `<!DOCTYPE html>
<html lang="${lang.toLowerCase()}">
<head>
<meta charset="UTF-8">
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700;900&family=JetBrains+Mono:wght@400;500;700&display=swap" rel="stylesheet">
<style>
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
:root{--cr:#DC143C;--am:#FFB000;--bg:#0d0d0d;--pn:#161616;--cd:#1c1c1c;--eg:#282828;--tx:#d8d8d8;--mt:#777;--wh:#fff}
@page{size:A4;margin:0}
html{background:var(--bg)}
body{font-family:'Inter',sans-serif;font-size:9.5pt;line-height:1.75;color:var(--tx);background:var(--bg);width:210mm;min-height:297mm}

/* COVER */
.cover{width:100%;background:var(--cr);padding:18mm 22mm 0;position:relative;overflow:hidden}
.cover-grid{position:absolute;inset:0;background-image:linear-gradient(rgba(255,255,255,.04) 1px,transparent 1px),linear-gradient(90deg,rgba(255,255,255,.04) 1px,transparent 1px);background-size:22px 22px;pointer-events:none}
.cover-glow{position:absolute;top:-40px;right:-40px;width:300px;height:300px;background:radial-gradient(circle,rgba(255,255,255,.1) 0%,transparent 70%);pointer-events:none}
.cover-meta{font-family:'JetBrains Mono',monospace;font-size:7pt;color:rgba(255,255,255,.5);letter-spacing:.18em;text-transform:uppercase;margin-bottom:10px;position:relative;z-index:1;display:flex;align-items:center;gap:12px}
.cover-meta::before{content:'';display:inline-block;width:28px;height:2px;background:var(--am);flex-shrink:0}
.cover-title{font-size:27pt;font-weight:900;color:#fff;line-height:1.1;letter-spacing:-.03em;margin-bottom:8px;position:relative;z-index:1;max-width:90%}
.cover-sub{font-size:11pt;font-weight:300;color:rgba(255,255,255,.6);font-style:italic;margin-bottom:16mm;position:relative;z-index:1}
.cover-stripe{height:4px;background:linear-gradient(90deg,var(--am) 0%,rgba(255,176,0,.2) 100%);width:100%;position:relative;z-index:1}

/* CONTENT */
.content{padding:12mm 22mm 22mm}
h1{display:none}
h2{font-size:11.5pt;font-weight:700;color:#fff;margin:26px 0 10px;padding:9px 14px 9px 16px;background:var(--pn);border-left:3px solid var(--cr);border-radius:0 5px 5px 0;break-after:avoid;display:flex;align-items:center;gap:8px}
h2::before{content:'';width:5px;height:5px;background:var(--cr);border-radius:50%;flex-shrink:0}
h3{font-size:9.5pt;font-weight:700;color:var(--am);text-transform:uppercase;letter-spacing:.1em;margin:20px 0 6px;display:flex;align-items:center;gap:8px;break-after:avoid}
h3::before{content:'';width:14px;height:1.5px;background:var(--am);flex-shrink:0}
h4{font-size:9pt;font-weight:600;color:#888;margin:14px 0 4px;break-after:avoid}
p{margin-bottom:10px;color:var(--tx);text-align:justify;hyphens:auto}
strong{color:#fff;font-weight:700}
em{color:var(--am);font-style:italic}

/* CODE */
pre{background:var(--pn);border:1px solid var(--eg);border-left:3px solid var(--cr);border-radius:0 6px 6px 0;padding:12px 16px;margin:12px 0;overflow-x:auto;break-inside:avoid;position:relative}
pre::before{content:'';position:absolute;top:9px;right:12px;width:7px;height:7px;background:var(--cr);border-radius:50%;opacity:.5;box-shadow:-11px 0 0 rgba(255,176,0,.4),-22px 0 0 rgba(34,197,94,.35)}
pre code{font-family:'JetBrains Mono',monospace;font-size:7.5pt;color:#cdd6f4;background:transparent;padding:0;border:none;line-height:1.7;display:block;white-space:pre}
code{font-family:'JetBrains Mono',monospace;font-size:8pt;color:var(--am);background:rgba(255,176,0,.08);padding:1px 5px;border-radius:3px;border:1px solid rgba(255,176,0,.15)}

/* TABLES */
table{width:100%;border-collapse:separate;border-spacing:0;margin:14px 0;font-size:8.5pt;break-inside:avoid;border-radius:8px;overflow:hidden;border:1px solid var(--eg);box-shadow:0 4px 18px rgba(0,0,0,.4)}
thead tr{background:linear-gradient(135deg,var(--cr) 0%,#a0001e 100%)}
thead th{color:#fff;font-family:'JetBrains Mono',monospace;font-size:7pt;font-weight:700;text-transform:uppercase;letter-spacing:.08em;padding:9px 13px;text-align:left;border:none}
tbody tr:nth-child(odd){background:var(--pn)}
tbody tr:nth-child(even){background:var(--cd)}
tbody td{color:var(--tx);padding:7px 13px;border:none;border-bottom:1px solid var(--eg);vertical-align:top}
tbody td:first-child{color:var(--am);font-family:'JetBrains Mono',monospace;font-size:8pt;font-weight:500}
tbody tr:last-child td{border-bottom:none}

/* BLOCKQUOTE */
blockquote{border-left:3px solid var(--am);background:linear-gradient(90deg,rgba(255,176,0,.06) 0%,transparent 100%);padding:12px 16px;margin:14px 0;border-radius:0 6px 6px 0;break-inside:avoid}
blockquote p{color:#ccc;font-style:italic;margin:0}

/* LISTS */
ul,ol{padding-left:8px;margin:6px 0 10px}
ul li{color:var(--tx);margin-bottom:4px;list-style:none;padding-left:16px;position:relative}
ul li::before{content:'▸';color:var(--cr);position:absolute;left:0;font-size:8pt;line-height:1.75}
ul ul li::before{content:'◦';color:var(--am)}
ol{padding-left:20px}
ol li{color:var(--tx);margin-bottom:4px}
ol li::marker{color:var(--cr);font-weight:700}

hr{border:none;height:1px;background:linear-gradient(90deg,var(--cr),var(--am),transparent);margin:22px 0;opacity:.35}
a{color:var(--am);text-decoration:none}

/* DIAGRAMS */
.diagram{background:var(--pn);border:1px solid var(--eg);border-radius:10px;padding:18px 14px 14px;margin:16px 0;break-inside:avoid}
.diagram-label{font-family:'JetBrains Mono',monospace;font-size:7pt;color:var(--cr);text-transform:uppercase;letter-spacing:.14em;margin-bottom:14px;padding-bottom:8px;border-bottom:1px solid var(--eg);display:flex;align-items:center;gap:8px}
.diagram-label::before{content:'◈';color:var(--am);font-size:9pt}

/* FOOTER */
.footer{position:fixed;bottom:0;left:0;right:0;height:13mm;background:var(--pn);border-top:1px solid var(--eg);display:flex;align-items:center;justify-content:space-between;padding:0 22mm}
.footer span{font-family:'JetBrains Mono',monospace;font-size:6.5pt;color:#3a3a3a;letter-spacing:.06em}
.footer .brand{color:var(--cr);font-weight:700;display:flex;align-items:center;gap:5px}
.footer .brand::before,.footer .brand::after{content:'·';color:#333}

@media print{body{margin-bottom:13mm}h2,h3,h4{break-after:avoid}pre,table,blockquote,.diagram{break-inside:avoid}}
</style>
</head>
<body>
<div class="cover">
  <div class="cover-grid"></div>
  <div class="cover-glow"></div>
  <div class="cover-meta">WhyCremisi · AI Mix Assistant &nbsp;·&nbsp; Paper ${num} &nbsp;·&nbsp; ${lang}</div>
  <div class="cover-title">${title}</div>
  <div class="cover-sub">${subtitle}</div>
  <div class="cover-stripe"></div>
</div>
<div class="content">${bodyHtml}</div>
<div class="footer">
  <span>WhyCremisi Research Papers</span>
  <span class="brand">WHYCREMISI</span>
  <span>Paper ${num} · ${lang}</span>
</div>
</body></html>`;
}

// ── HELPERS ───────────────────────────────────────────────────────────────────

function extractMeta(filename, content) {
  const lines = content.split('\n');
  const h1 = lines.find(l => l.startsWith('# '))?.replace(/^# /, '') || filename;
  const h2 = lines.find(l => l.startsWith('## '))?.replace(/^## /, '') || '';
  const num = filename.match(/^(\d+)/)?.[1] || '00';
  const title = h1.replace(/^Paper \d+\s*[—-]\s*/, '');
  return { title: title || h1, subtitle: h2, num };
}

function processMarkdown(content, filename) {
  let md = content
    .replace(/^# .+\n/, '')          // remove h1 (in cover)
    .replace(/^\*→.*\*\s*$/gm, ''); // remove nav links
  const html = marked.parse(md);
  return injectDiagrams(html, filename);
}

// ── MAIN ──────────────────────────────────────────────────────────────────────

async function main() {
  console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
  console.log('  WhyCremisi — PDF Build v3 (SVG Diagrams)');
  console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');

  const browser = await puppeteer.launch({
    headless: true,
    args: ['--no-sandbox', '--disable-setuid-sandbox'],
  });

  for (const { src, out, lang } of PAPERS) {
    const srcDir = path.join(ROOT, src);
    const outDir = path.join(ROOT, out);
    fs.mkdirSync(outDir, { recursive: true });

    const files = fs.readdirSync(srcDir).filter(f => f.endsWith('.md')).sort();
    console.log(`\n▸ ${lang} — ${files.length} papers`);

    for (const file of files) {
      const base = file.replace('.md', '');
      const content = fs.readFileSync(path.join(srcDir, file), 'utf8');
      const { title, subtitle, num } = extractMeta(base, content);
      const bodyHtml = processMarkdown(content, file);
      const html = buildHTML(num, title, subtitle, lang, bodyHtml);

      console.log(`  → ${base}.pdf`);
      const page = await browser.newPage();
      await page.setContent(html, { waitUntil: 'networkidle0', timeout: 60000 });
      await page.pdf({
        path: path.join(outDir, base + '.pdf'),
        format: 'A4',
        printBackground: true,
        margin: { top: '0', bottom: '13mm', left: '0', right: '0' },
      });
      await page.close();
    }
  }

  await browser.close();
  console.log('\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
  console.log('  Done.');
}

main().catch(err => { console.error(err); process.exit(1); });
