# 🧠 AI Integration Stress Test: WhyCremisi Pro

**Goal:** Ensure the AI engine is stable, responsive, and handles failures gracefully, matching the reliability standards of high-end VST developers.

---

## 🧪 Test Case A: Local Model (Ollama) Latency & Load
- **Objective**: Measure the overhead of the local HTTP bridge.
- **Method**: Send 10 consecutive prompts and measure:
  1. Time to First Byte (TTFB).
  2. Total completion time.
  3. CPU spike during processing.
- **Success Criteria**: TTFB < 200ms (excluding model inference).

## 🧪 Test Case B: Network Resilience (Cloud API)
- **Objective**: Verify handling of API timeouts and SSL errors.
- **Method**: 
  1. Simulate a DNS failure (invalid URL).
  2. Simulate a Timeout (set timeout to 100ms).
  3. Simulate an Invalid API Key.
- **Success Criteria**: The plugin must return a clear `[ERROR]` message to the UI within the timeout period without freezing the GUI.

## 🧪 Test Case C: Message Thread Blocking
- **Objective**: Ensure the `OscBridge` remains responsive while the AI is thinking.
- **Method**: 
  1. Trigger a slow AI prompt (10+ seconds).
  2. While the AI is thinking, send a "DAW Play" command from the UI.
- **Success Criteria**: The DAW should start playing *instantly*, even if the AI is still processing. 

---

## 📊 Observations & Risk Analysis

| Risk | Impact | Mitigation |
| :--- | :--- | :--- |
| **Sync Blocking** | High (GUI Freeze) | Move `sendPrompt` to a real worker thread (`juce::Thread`). |
| **JSON Malformation** | Medium | Strict schema validation before parsing. |
| **Port Conflict** | Low | Implement port scanning or dynamic port allocation. |

---

> "Stability is the most important feature of an AI plugin." — *Antigravity Research*
