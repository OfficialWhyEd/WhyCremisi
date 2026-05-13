# Paper 04 — Agent Memory System
## OpenClaw Memory: Identity, Persistence, Evolution

```
────────────────────────────────────────────────────────────────
  WHYCREMISI RESEARCH PAPERS — N.04
  Agent AI Memory System

  "It does not reset. It remembers. It grows."
────────────────────────────────────────────────────────────────
```

---

## 1. Why Memory Is Fundamental

An assistant without memory restarts from zero every time. Memory transforms a tool into a **collaborator**.

```
  WITHOUT MEMORY:  Session 100 = Session 1. Generic answers. No growth.

  WITH OPENCLAW:   "I see you're working on something similar to two weeks
                   ago — would you like to use the same processing chain?"
```

---

## 2. OpenClaw Memory Architecture

```
  LAYER 4  Identity (permanent)    — Who the agent is. Core values. Immutable.
  LAYER 3  Personality (slow)      — Communication style. Evolves gradually.
  LAYER 2  User Profile (medium)   — Who the user is. Preferences. Plugins.
  LAYER 1  Session Memory (daily)  — What happened today. Actions, results.
  LAYER 0  Real-Time Context       — BPM, meters, active plugins. Volatile.
```

---

## 3. User Profile Structure

```json
{
  "musical_style": { "genres": ["house","techno"], "bpm_range": [120,140] },
  "plugin_preferences": {
    "eq": "FabFilter Pro-Q3", "compressor": "FabFilter Pro-C2",
    "limiter": "FabFilter Pro-L2"
  },
  "processing_preferences": {
    "kick_style": "dry_tight", "master_lufs_target": -14.0,
    "master_peak_ceiling": -0.3, "compression_style": "transparent"
  },
  "acceptance_rates": {
    "eq_suggestions": 0.78, "compression_suggestions": 0.65
  }
}
```

---

## 4. Learning Cycle

```
  User accepts EQ suggestion  →  increment eq_suggestions acceptance_rate
  User rejects compression    →  lower default compression depth for user
  User manually corrects freq →  update eq_frequency_bias preference
  End of session              →  distil patterns → update user_profile.json
```

---

## 5. Context Injection in Every AI Call

```
  [SESSION CONTEXT]   Date · BPM · Project name
  [LIVE AUDIO STATE]  LUFS · Peak · Stereo width · FFT anomaly
  [USER PROFILE]      Genre · LUFS target · Preferred plugins
  [RELEVANT MEMORY]   Last 3 sessions relevant actions
  [ACTIVE PLUGINS]    Per-track plugin chain
  [AGENT PERSONALITY] Style · Language · Verbosity · Technicality
```

---

## 6. Privacy

All data stored locally at `~/.whycremisi/`. Nothing sent to cloud without explicit opt-in. Full `/forget` and `/reset` commands available. API keys never logged in plain text.

---

*→ Continue: [Paper 05 — Communication Protocol](05-COMMUNICATION-PROTOCOL.md)*
