# WhyCremisi VST Bridge AI — Development Plan

**Branch:** `heartbroken-claude`  
**Last Updated:** 2026-05-12 (session 3)  
**Commit:** `96db77e`  
**Platform:** macOS 12.7.6 (Monterey), Intel x86_64, JUCE 8.0.12

---

## How to Use This Plan

Each step is a verifiable atomic unit. After completing a step:
1. Verify it compiles (C++) or passes lint (frontend)
2. Run any relevant tests
3. Mark with `[x]` and commit with the step ID in the message

Legend: `[ ]` = pending, `[~]` = in progress, `[x]` = done, `[-]` = skipped/cancelled

---

## Phase 1: BoxChat Modularization (Steps 1–22)

**Goal:** Break the 500+ line monolithic `BoxChat` function into per-type reusable modules under `webview-ui/src/boxes/`, add multi-box support, improve interactivity.

### Step 1 — Create `boxes/` directory and `boxes/index.js` barrel
- `mkdir webview-ui/src/boxes/`
- Create `index.js` that re-exports all box components
- Create `BoxContext.js` — shared context for meter/transport/personality props

### Step 2 — Extract `VectorscopeBox.jsx`
- Move vectorscope JSX (lines 58–106) to own component
- Import `motion` from framer-motion
- Accept `correlation`, `onDawCmd`, `midSide`, `phaseInvert` props with defaults
- Verify no visual regression

### Step 3 — Extract `StereoscopeBox.jsx`
- Move stereo field box (lines 109–133) to own component
- Accept `meterL`, `meterR`, `midSide`, `onDawCmd` props
- Local state for midSide toggle

### Step 4 — Extract `LoudnessBox.jsx`
- Move loudness analysis box (lines 135–161) to own component
- Accept `lufs`, `peak`, `meterL`, `meterR`, `onDawCmd` props

### Step 5 — Extract `ClippingBox.jsx`
- Move clipping detection box (lines 164–210) to own component
- Accept `peak`, `onDawCmd` props
- Local state for `clipThreshold`

### Step 6 — Extract `EqBox.jsx`
- Move frequency analysis box (lines 212–239) to own component
- Accept `spectrum`, `meterL` props
- Local `avgBands` helper

### Step 7 — Extract `SpectralBox.jsx`
- Move spectral analyzer (lines 242–271) to own component
- Accept `spectrum`, `onDawCmd` props
- Apply requestAnimationFrame-based animation for performance

### Step 8 — Extract `SliderBox.jsx`
- Move volume/drive slider box (lines 273–310) to own component
- Accept `gainDb`, `driveVal`, `onDawCmd` props
- Local gain/drive state with mouse interaction

### Step 9 — Extract `KnobBox.jsx`
- Move pan/stereo knob box (lines 312–334) to own component
- Accept `meterL`, `meterR` props
- SVG arc rendering

### Step 10 — Extract `TransportBox.jsx`
- Move transport display (lines 336–359) to own component
- Accept `transport`, `onDawCmd` props

### Step 11 — Extract `CompressorBox.jsx`
- Move compressor settings (lines 362–403) to own component
- Accept `correlation`, `onDawCmd` props
- Local state for compThresh, compRatio

### Step 12 — Extract `AdvisoryBox.jsx`
- Move advisory card (lines 405–508) to own component
- Accept `suggestion`, `personality`, `onDawCmd`, `onAnalyzeFurther` props
- Local state for advDismissed, advExecuted
- Use `setAdvDismissed` (already fixed in App.jsx)

### Step 13 — Extract `MetricsBox.jsx`
- Move default metrics grid (lines 511–520) to own component
- Accept `meterL`, `meterR`, `lufs`, `peak` props

### Step 14 — Extract shared `MetricBar` subcomponent
- Create `boxes/shared/MetricBar.jsx`
- Pull MetricBar definition from BoxChat (lines 27–40)
- Make it reusable across all box components
- Props: `label`, `val`, `color`, `pct`

### Step 15 — Extract shared `BoxWrapper` subcomponent
- Create `boxes/shared/BoxWrapper.jsx`
- Pull `wrap()` function (lines 42–55)
- Standardize the container, header, and actions pattern
- Props: `label`, `color`, `icon`, `actions`, `children`

### Step 16 — Rewrite `BoxChat.jsx` as thin orchestrator
- Move BoxChat from App.jsx to its own `boxes/BoxChat.jsx`
- Import all box type components from `boxes/`
- Switch on `boxType` string to render the correct box
- If `boxType` is empty/null, render nothing
- Accept same props as before, pass them down to component

### Step 17 — Make `detectBoxType()` return array of types
- Modify `detectBoxType` (line 1004) to return `string[]` instead of `string`
- Match multiple box types from prompt+response
- Ensure backward compat: if only one match, return `[type]`
- Update call sites in App.jsx

### Step 18 — Multi-box rendering in BoxChat
- When `boxType` is an array, render all matched boxes
- Wrap each in its own container with sequential animation delay
- Add visual separator between multiple boxes

### Step 19 — Add `combineBoxes` user prop
- Allow user to request specific box combinations via chat
- E.g. "show me eq and spectral" → returns `['eq', 'spectral']`
- Parse user intent for explicit box type requests in detectBoxType

### Step 20 — Hover persistence for interactive boxes
- While user is interacting (mouse down on slider/knob/comp), keep box visible
- Add `forceVisible` prop to BoxChat when interaction occurs
- Timeout-based auto-hide after 3s of inactivity for non-interactive boxes

### Step 21 — Add resize/minimize capabilities per box
- Action buttons in `BoxWrapper`: minimize (collapse), close (dismiss)
- Minimized boxes show only the header bar
- State managed in BoxChat or parent App

### Step 22 — Box layout modes (inline, grid, popover)
- `layout` prop: `'inline'` (default, stacked), `'grid'` (2-column), `'popover'` (floating)
- Grid layout uses CSS grid with auto-fill columns
- Popover renders above chat in absolute position

---

## Phase 2: Backend DSP & OSC (Steps 23–38)

**Goal:** Improve DSP quality, reorganize OSC handler, add FFT/loudness features, DAW support.

### Step 23 — Reorganize OscHandler methods into logical groups
- Group: transport commands, mixer commands, query commands, feedback handlers
- Prefix private methods with `_`
- Add `// MARK:` style comments for IDE navigation

### Step 24 — Consolidate log messages in OscHandler
- Remove duplicate log prefixes, use consistent format
- Add thread-safe log queue instead of direct `DBG()` calls
- Push logs to WebSocket for frontend display

### Step 25 — Add FFT spectrum via real-time averaging
- In DSPEngine Analyzer, implement proper FFT with windowing (Hann/Blackman)
- Average spectrum over configurable time window (50ms–500ms)
- Output `daw.analyzer` OSC with `spectrum`, `correlation`, `phases`

### Step 26 — Add loudness metering (momentary, short-term, integrated)
- Implement EBU R128 loudness measurement
- LUFS momentary (400ms window), short-term (3s), integrated (full session)
- Output as `daw.loudness` OSC event

### Step 27 — Add true peak detection
- Oversample by 2x or 4x for true peak estimation
- Report dBTP alongside regular peak
- Include in clipping detection box

### Step 28 — Add stereo correlation with history
- Calculate real-time correlation coefficient
- Track min/max/avg correlation over session
- Detect phase issues automatically

### Step 29 — Add clipping history/count per session
- Track number of clipping events, max duration, max overshoot
- Reset on play/stop or manual reset
- Display in ClippingBox as `CLIPS: N` counter

### Step 30 — Improve OSC parameter mapping for Reaper
- Full parameter list for Reaper: volume, pan, mute, solo, record arm, track FX params
- Bidirectional sync (reaper → plugin → UI)
- Use `/track/N/param/M` addresses

### Step 31 — Add VST3 program/preset management
- Program list, current program, program change via OSC
- Preset save/load from UI
- Forward to DAW

### Step 32 — Add AU parameter automation support
- Expose all parameters to AU host for automation
- Automation read/write/latch/touch modes
- Parameter value smoothing for automation

### Step 33 — Add Ableton Live parameter discovery
- Detect Ableton via OSC handshake (/live/... endpoints)
- Map Ableton track parameters (volume, pan, sends, devices)
- Handle Ableton's `/live/api/v1/...` REST API if available

### Step 34 — Consolidate Reaper + Ableton into generic DAW abstraction
- Abstract DAW interface: `IDawHandler` with methods for transport, mixer, feedback
- Reaper and Ableton specific implementations
- Auto-detect DAW type on connection

### Step 35 — Add DAW-specific OSC commands (transport, markers, tempo)
- Transport: play, stop, record, pause, FF, REW, goto marker
- Markers: list, add, delete, goto
- Tempo: get/set, tap tempo

### Step 36 — Add audio playback position sync
- Send position updates at 10Hz while playing
- Display in transport box as timecode
- Support for time signature changes

### Step 37 — Add `PluginStats` to `daw.analyzer` message
- Include sample rate, buffer size, latency, CPU usage
- Send on connect and periodically

### Step 38 — Add audio CPU usage monitoring
- Track processing time per callback
- Report as percentage of available CPU
- Detect and warn on buffer underruns

---

## Phase 3: AI Engine Enhancements (Steps 39–50)

**Goal:** Make AI responses faster, smarter, and more interactive.

### Step 39 — Add streaming token-by-token output
- Implement SSE-style streaming for all providers
- Each chunk triggers `ai.stream` event to frontend
- Frontend appends chunks in real-time with cursor blink

### Step 40 — Add model list retrieval endpoint
- `/api/models` for OpenAI, Anthropic, Groq, Ollama
- Cache model list for 5 minutes
- Display in setup UI

### Step 41 — Add context window management (trim history)
- Track total tokens per conversation
- When approaching limit, summarize older messages
- Keep system prompt + last N exchanges intact

### Step 42 — Add tool/function calling for DAW commands
- Define functions: `setGain`, `applyEQ`, `setPan`, `play`, `stop`, `record`
- AI can call these functions directly (OpenAI/Anthropic tool use)
- Results fed back to AI for multi-step reasoning

### Step 43 — Add personality-based prompt tuning
- System prompt varies by personality style (direct, consultative, analytical, creative, warm)
- Personality prefix injected at runtime based on PersonalityCore state
- Tone, verbosity, and recommendation style match personality

### Step 44 — Add JSON mode for structured AI responses
- Force JSON output for meter analysis and suggestion generation
- Parse JSON in `detectBoxType` for precise box type selection
- Schema: `{ boxes: string[], suggestion?: { freq, gain, ... } }`

### Step 45 — Add response caching for repeating queries
- Hash prompt + context + personality → cache key
- Cache valid for 30s, stored in LRU (max 100 entries)
- Cache hit returns instantly with `fromCache: true` flag

### Step 46 — Add conversation branching (alternate suggestions)
- User can request alternate suggestions
- Branch from last AI message with modified prompt
- Display as numbered alternatives in chat

### Step 47 — Add DAW-aware persona descriptions
- Personality includes DAW-specific terminology (Reaper/Ableton)
- Tooltips and descriptions adapt based on detected DAW
- Learning mode: personality updates based on DAW usage patterns

### Step 48 — Add plugin-aware system prompt generation
- System prompt includes current plugin chain state
- AI knows what plugins are in the chain and their parameters
- Recommendations consider available plugins

### Step 49 — Add connection health monitoring
- Track WebSocket latency, message loss, reconnection count
- Display in header as connection quality indicator
- Alert on persistent issues

### Step 50 — Add offline mode improvements
- Better fallback responses with cached AI data
- Local keyword-based suggestions
- Queue commands for execution when online

---

## Phase 4: UI Polish (Steps 51–62)

**Goal:** Professional finish, accessibility, theming, and UX improvements.

### Step 51 — Improve dark theme consistency
- Audit all colors against design system tokens
- Ensure no hardcoded colors outside theme variables
- Add CSS custom properties for all theme colors

### Step 52 — Add light theme option
- Light theme variant in `index.css` using `[data-theme="light"]`
- Toggle in settings header icon
- Persist preference in localStorage

### Step 53 — Improve mobile/touch support (WebView scaling)
- Viewport meta tag for proper mobile scaling
- Responsive breakpoints for smaller screens
- Touch-friendly widget sizes (min 44px tap targets)

### Step 54 — Add keyboard shortcuts
- `Cmd/Ctrl+Enter` = send message
- `Escape` = clear input
- `Cmd/Ctrl+K` = focus command input
- `Cmd/Ctrl+Z` = undo AI action
- `Shift+Cmd/Ctrl+Z` = redo AI action
- `Space` = toggle play/stop

### Step 55 — Add chat message search
- Search input above chat messages
- Filter messages by text content
- Highlight matches with amber background
- Count results, navigate with up/down arrows

### Step 56 — Add session renaming
- Click on session timestamp to rename
- Inline edit with Enter to confirm
- Store in session metadata

### Step 57 — Add export/import session data
- Export: download JSON of all messages + context + actions
- Import: load JSON into existing session
- Format: `{ version, timestamp, messages, personality, pluginChain, midiMappings }`

### Step 58 — Improve accessibility (aria labels, tab order)
- Add `aria-label` on all interactive elements
- Logical tab order through header → sidebar → chat → rack
- Focus ring styles for keyboard navigation
- Screen reader announcements for state changes

### Step 59 — Add onboarding tour for new users
- 5-step tour: Setup, Command Console, Mastering Rack, Telemetry, Settings
- Overlay with highlighted elements and explanatory text
- Can dismiss permanently or skip

### Step 60 — Add loading skeleton for slow AI responses
- Skeleton placeholder while AI is thinking
- Animated pulse effect matching chat message layout
- Replaced with actual content when response arrives

### Step 61 — Improve advisory card animations
- Entry: staggered reveal of elements (priority bar → text → actions)
- Reduced motion preference support (`prefers-reduced-motion`)
- Pulse indicator for confidence level

### Step 62 — Add undo/redo stack UI in actions panel
- Visual list of AI actions with undo/redo buttons
- Keyboard shortcut integration
- Action grouping (coalesce rapid changes)

---

## Phase 5: Stability & Performance (Steps 63–74)

**Goal:** Robust error handling, memory management, performance profiling.

### Step 63 — Comprehensive error recovery in WebSocket
- Handle all close codes (1000–1015) with appropriate action
- Graceful degradation on message parse failure
- Connection timeout with configurable limit

### Step 64 — Auto-reconnect with exponential backoff
- Initial delay: 500ms, max delay: 30s
- Jitter: ±25% random
- Reset on successful connection
- Display reconnection count in header

### Step 65 — Message queue with retry
- Queue messages sent while disconnected
- Send in order on reconnection
- Max queue size: 100, oldest dropped
- Retry failed sends up to 3 times

### Step 66 — Performance profiling (FPS, latency)
- Monitor UI frame rate with `requestAnimationFrame` counter
- Track WebSocket round-trip latency
- Display in debug overlay (triggered by `Cmd/Ctrl+Shift+D`)

### Step 67 — Memory management for long sessions
- Limit chat messages to 500, oldest archived
- Periodic cleanup of stale animation frames
- Unsubscribe from all events on unmount (already partially done)

### Step 68 — CPU throttle detection
- Monitor `setInterval` drift (expected vs actual delay)
- Detect when browser throttles timers (background tab)
- Adjust animation quality accordingly

### Step 69 — Crash recovery (state persistence)
- Save critical state to localStorage every 30s
- On reload, restore last session (messages, personality, settings)
- Version-stamped to handle schema migrations

### Step 70 — Log rotation for long runs
- Frontend log buffer capped at 1000 entries
- Rotate OSC/system logs, keep last 200 per type
- Exportable log dump for debugging

### Step 71 — Config validation on save
- Validate provider URL format before saving
- Check API key length > 0 for non-Ollama providers
- Model name must match known models or pass regex

### Step 72 — Version migration for config changes
- Config schema version field
- Migration functions for each version increment
- Handle missing keys with sensible defaults

### Step 73 — Add JSON schema validation for OSC messages
- Define JSON schema for each message type
- Validate incoming messages, log warnings on mismatch
- Reject malformed messages gracefully

### Step 74 — Add `juce::AsyncUpdater` for thread-safe OSC dispatch
- Replace raw pointers with AsyncUpdater for main thread dispatch
- Eliminate potential race conditions in parameter updates
- Verify with AddressSanitizer

---

## Phase 6: Testing & Documentation (Steps 75–85)

**Goal:** Automated tests, comprehensive docs, CI setup.

### Step 75 — Add C++ unit tests (Google Test) for OscHandler
- Test parsing of all OSC address patterns
- Test message formatting and padding
- Test edge cases: empty string, max values, special chars

### Step 76 — Add C++ unit tests for PersonalityCore
- Test style detection from user input
- Test confidence calculations
- Test experience level progression

### Step 77 — Add C++ unit tests for AiEngine
- Test provider routing (OpenAI, Anthropic, Groq, Ollama)
- Test system prompt generation with personality
- Test context window trimming

### Step 78 — Add React component tests (Vitest + Testing Library)
- Test BoxChat renders correct component per boxType
- Test MetricBar renders values correctly
- Test AdvisoryBox dismiss and execute flows
- Test SliderBox mouse interaction

### Step 79 — Add end-to-end integration test
- Script: launch standalone → connect WebSocket → send OSC → verify UI updates
- Use `juce::UnitTest` for C++ side
- Use Playwright or Cypress for frontend side

### Step 80 — Add build scripts for CI
- GitHub Actions workflow: build C++, build frontend, run tests
- Matrix: macOS (x86_64 + arm64), Windows, Linux
- Lint + typecheck on every push

### Step 81 — Add CHANGELOG.md
- Semantic versioning based on current state
- Categorize: Features, Fixes, Performance, Documentation
- Link to relevant commits/issues

### Step 82 — Add API documentation for OSC commands
- Document all OSC addresses and parameters
- Format: `[ADDRESS] (direction) — description`
- Include examples for Reaper and Ableton

### Step 83 — Add user manual / README updates
- Installation guide (VST3, AU, Standalone)
- Configuration (API keys, model selection)
- DAW setup (Reaper OSC, Ableton Link)
- Troubleshooting FAQ

### Step 84 — Add contribution guide (CONTRIBUTING.md)
- Code style (C++17, React patterns)
- PR process and review guidelines
- Development environment setup
- Testing requirements

### Step 85 — Add architecture documentation
- High-level component diagram (C++ modules)
- Data flow: Audio → DSP → OSC → WebSocket → WebView
- React component tree
- Event/message catalog

---

## Phase 7: Advanced Features (Steps 86–100)

**Goal:** Cutting-edge capabilities that differentiate WhyCremisi.

### Step 86 — Real-time audio analysis presets
- Genre-based analysis profiles (EDM, rock, jazz, classical, podcast)
- Target loudness, EQ curve, dynamic range by genre
- Preset switchable from UI or AI suggestion

### Step 87 — Stem separation integration
- Use Spleeter/Demucs via local HTTP server
- Separate vocals, drums, bass, other
- Per-stem analysis and processing
- Requires external Python server or ONNX model

### Step 88 — AI mix comparison (A/B)
- Save current mix state as snapshot
- Apply AI suggestion, compare with original
- Visual diff: before/after spectrum overlay, loudness change
- Seamless toggle between snapshots

### Step 89 — Spectral editing (click removal, de-esser, de-hum)
- Intelligent spectral repair for clicks/pops
- De-esser: detect sibilance range automatically
- De-hum: notch filter suggestion based on spectrum peaks

### Step 90 — LUFS target auto-match (streaming platform presets)
- Presets: Spotify (-14 LUFS), YouTube (-13 LUFS), Apple Music (-16 LUFS), podcast (-16 LUFS)
- Auto-adjust gain + limiting to hit target
- Measure integrated LUFS over full track

### Step 91 — Loudness normalization curve analysis
- Analyze dynamic contour over time
- Plot loudness vs time graph
- Suggest compression/limiting to match reference

### Step 92 — Phase correlation metering with history graph
- Line chart of correlation over last 10s
- Detect and mark phase issues in timeline
- Correlation heatmap per frequency band

### Step 93 — Multi-channel support (5.1, 7.1, Atmos)
- Extend meter display for surround channels
- Per-channel correlation and phase analysis
- Downmix check (Lt/Rt compatibility)

### Step 94 — AI mastering chain presets (warm, clean, aggressive, transparent)
- Preset: full mastering chain with configured plugins
- Personality-aware default presets
- User-customizable and sharable presets

### Step 95 — Session statistics dashboard
- Track: AI queries, actions taken, DAW commands sent, clip events
- Session duration, peak CPU, memory usage
- Export stats as CSV

### Step 96 — External control surface integration (Mackie MCU, etc.)
- Map controls to physical faders/knobs
- Transport and automation control via surface
- Custom mapping editor in UI

### Step 97 — Plugin hosting (load VST3 within WhyCremisi)
- Use JUCE's AudioPluginFormatManager
- Host external VST3 plugins in chain
- Forward UI parameters to hosted plugins

### Step 98 — Collaborative session sharing
- Share UI session via WebRTC or WebSocket relay
- Remote AI assistant: one person controls, others view
- Chat between collaborators

### Step 99 — MIDI generation from AI (drum patterns, bass lines)
- AI generates MIDI patterns based on analysis
- Output MIDI to DAW via OSC/MIDI
- Style and complexity controlled by personality

### Step 100 — Public API for third-party integration
- REST API for external apps to query state
- WebSocket subscription API for real-time data
- Authentication via API tokens
- Rate limiting and usage tracking

---

## Execution Log

| Step | Date | Author | Status | Commit |
|------|------|--------|--------|--------|
| 1–22 | 2026-05-12 | Claude | [x] | heartbroken-claude |
| 23–28 | 2026-05-12 | Claude | [x] | heartbroken-claude |
| 29 | 2026-05-12 | Claude | [x] | heartbroken-claude (clipping count) |
| 30 | 2026-05-12 | Claude | [x] | heartbroken-claude (DAW cmd additions) |
| 31 | 2026-05-12 | Claude | [x] | heartbroken-claude (VST3 program/preset) |
| 32 | 2026-05-12 | Claude | [x] | heartbroken-claude (AU auto smoothing) |
| 33 | 2026-05-12 | Claude | [x] | heartbroken-claude (Ableton discovery) |
| 34 | 2026-05-12 | Claude | [x] | heartbroken-claude (IDawHandler) |
| 35 | 2026-05-12 | Claude | [x] | heartbroken-claude (marker/track/FX cmd) |
| 36 | 2026-05-12 | Claude | [x] | heartbroken-claude (10Hz position sync) |
| 37 | 2026-05-12 | Claude | [x] | heartbroken-claude (stats in analyzer) |
| 38 | 2026-05-12 | Claude | [x] | heartbroken-claude (CPU monitoring) |
| 39 | 2026-05-12 | Claude | [x] | heartbroken-claude (logo SVG + icon builder) |
| 40 | 2026-05-12 | Claude | [x] | heartbroken-claude (papers 08–15 IT+EN ×32 files) |
| 41 | 2026-05-12 | Claude | [x] | heartbroken-claude (PDFs ×32, 117 research files) |
| 42 | 2026-05-12 | Claude | [x] | heartbroken-claude (Research/{IT,EN} alignment) |
| 43 | 2026-05-12 | Claude | [x] | heartbroken-claude (AIProvider abstraction + streaming) |
| 44 | 2026-05-12 | Claude | [x] | heartbroken-claude (personality-based prompt tuning) |
| 45 | 2026-05-12 | Claude | [x] | heartbroken-claude (ToolRegistry + function calling) |
| 46 | 2026-05-12 | Claude | [x] | heartbroken-claude (ContextManager token budget) |
| 47 | 2026-05-12 | Claude | [x] | heartbroken-claude (dark/light theme toggle) |
| 48 | 2026-05-12 | Claude | [x] | heartbroken-claude (personality style picker UI) |
| 49 | 2026-05-13 | Claude | [x] | heartbroken-claude (connection health monitoring) |
| 54 | 2026-05-13 | Claude | [x] | heartbroken-claude (keyboard shortcuts: Cmd+K/Esc///↑↓) |
| 60 | 2026-05-13 | Claude | [x] | heartbroken-claude (loading skeleton for slow AI) |
| 63 | 2026-05-13 | Claude | [x] | heartbroken-claude (WebSocket error recovery + ping) |
| 64 | 2026-05-13 | Claude | [x] | heartbroken-claude (exponential backoff + jitter reconnect) |
| 65 | 2026-05-13 | Claude | [x] | heartbroken-claude (message queue with retry) |
| 52 | 2026-05-13 | Claude | [x] | heartbroken-claude (hardcoded→CSS var light theme ×120+) |
| 55 | 2026-05-13 | Claude | [x] | heartbroken-claude (chat message search ⌘F) |
| 57 | 2026-05-13 | Claude | [x] | heartbroken-claude (session export/import JSON) |
| 58 | 2026-05-13 | Claude | [x] | heartbroken-claude (aria-labels, accessibility) |
| 66–100 | TBD | Claude | [ ] | — |

---

*Plan created 2026-05-12 — Claude session "heartbroken-claude"*
