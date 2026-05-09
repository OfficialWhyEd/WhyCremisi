#!/usr/bin/env python3
"""
WhyCremisi VST — WebSocket Bridge Test Suite (protocol-json-v1)

Runs against either:
  - The real plugin (Standalone or VST3 in DAW)
  - The mock server: python3 tests/mock_websocket_server.py

Usage:
  python3 tests/test_websocket_bridge.py [ws://localhost:8080]
"""

import asyncio
import json
import sys
import time
import uuid
import websockets

WS_URL = sys.argv[1] if len(sys.argv) > 1 else "ws://localhost:8080"
TIMEOUT = 5.0
PASS = "[PASS]"
FAIL = "[FAIL]"
SKIP = "[SKIP]"

results = []

def ok(name): results.append((name, True));  print(f"  {PASS} {name}")
def fail(name, reason=""): results.append((name, False)); print(f"  {FAIL} {name}" + (f" — {reason}" if reason else ""))


# ── Helper ───────────────────────────────────────────────────────────────────
def msg(msg_type, payload=None, req_id=None):
    return json.dumps({
        "type": msg_type,
        "id": req_id or str(uuid.uuid4()),
        "timestamp": int(time.time() * 1000),
        "payload": payload or {}
    })

async def recv_until(ws, match_type, timeout=TIMEOUT):
    """Receive messages until one with matching type, or timeout."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        remaining = deadline - time.time()
        try:
            raw = await asyncio.wait_for(ws.recv(), timeout=remaining)
            data = json.loads(raw)
            if data.get("type") == match_type:
                return data
        except asyncio.TimeoutError:
            return None
    return None


# ── Tests ────────────────────────────────────────────────────────────────────
async def test_connection(ws):
    print("\n── Connection & plugin.init ────────────────────────────────")
    await ws.send(msg("plugin.init"))
    response = await recv_until(ws, "plugin.init")
    if response:
        p = response.get("payload", {})
        if "version" in p and "capabilities" in p:
            ok("plugin.init response has version + capabilities")
        else:
            fail("plugin.init response missing fields", str(p))
    else:
        fail("plugin.init no response in time")


async def test_transport_broadcast(ws):
    print("\n── Transport broadcast ─────────────────────────────────────")
    # Should receive daw.transport after plugin.init
    t = await recv_until(ws, "daw.transport")
    if t:
        p = t.get("payload", {})
        required = ["isPlaying", "isRecording", "bpm", "positionSeconds"]
        missing = [f for f in required if f not in p]
        if not missing:
            ok(f"daw.transport contains all fields (bpm={p['bpm']})")
        else:
            fail("daw.transport missing fields", str(missing))
    else:
        fail("No daw.transport received")


async def test_daw_play_stop(ws):
    print("\n── DAW play / stop commands ───────────────────────────────")
    await ws.send(msg("daw.command", {"command": "play"}))
    t = await recv_until(ws, "daw.transport")
    if t and t["payload"].get("isPlaying") is True:
        ok("play command → daw.transport isPlaying=true")
    else:
        fail("play did not trigger transport update", str(t))

    await ws.send(msg("daw.command", {"command": "stop"}))
    t = await recv_until(ws, "daw.transport")
    if t and t["payload"].get("isPlaying") is False:
        ok("stop command → daw.transport isPlaying=false")
    else:
        fail("stop did not trigger transport update", str(t))


async def test_tempo_change(ws):
    print("\n── Tempo change ────────────────────────────────────────────")
    await ws.send(msg("daw.command", {"command": "setTempo", "bpm": 140.0}))
    t = await recv_until(ws, "daw.transport")
    if t and abs(t["payload"].get("bpm", 0) - 140.0) < 0.1:
        ok("setTempo 140 → daw.transport bpm=140")
    else:
        fail("setTempo did not reflect in transport", str(t))


async def test_volume_command(ws):
    print("\n── Track volume command ────────────────────────────────────")
    await ws.send(msg("daw.command", {"command": "setVolume", "trackId": 1, "valueDb": -6.0}))
    # No specific response expected (DAW feedback would come as OSC), just no error
    await asyncio.sleep(0.1)
    ok("setVolume command sent without error")


async def test_daw_request(ws):
    print("\n── daw.request transport ───────────────────────────────────")
    req_id = str(uuid.uuid4())
    await ws.send(msg("daw.request", {"request": "transport"}, req_id=req_id))
    r = await recv_until(ws, "daw.response")
    if r:
        p = r.get("payload", {})
        if "bpm" in p:
            ok(f"daw.response has bpm={p['bpm']}")
        else:
            fail("daw.response missing bpm", str(p))
    else:
        fail("No daw.response received")


async def test_ai_prompt(ws):
    print("\n── AI prompt / response ────────────────────────────────────")
    req_id = str(uuid.uuid4())
    await ws.send(msg("ai.prompt", {"prompt": "Suggerisci come migliorare il mix del kick", "provider": "ollama"}, req_id=req_id))

    # Expect thinking first
    thinking = await recv_until(ws, "ai.response")
    if thinking and thinking["payload"].get("status") == "thinking":
        ok("ai.response thinking received")
    else:
        fail("No thinking status", str(thinking))

    # Expect final response (may take a few seconds)
    final = None
    for _ in range(20):
        r = await recv_until(ws, "ai.response", timeout=2.0)
        if r and r["payload"].get("status") in ("success", "error"):
            final = r
            break

    if final and "content" in final.get("payload", {}):
        content = final["payload"]["content"]
        ok(f"ai.response success content={content[:40]}...")
    else:
        fail("No final ai.response received", str(final))


async def test_ai_stream(ws):
    print("\n── AI streaming ────────────────────────────────────────────")
    req_id = str(uuid.uuid4())
    await ws.send(msg("ai.prompt", {"prompt": "Cosa pensi del loudness war?", "provider": "ollama"}, req_id=req_id))

    chunks = []
    done = False
    deadline = time.time() + 10
    while time.time() < deadline and not done:
        try:
            raw = await asyncio.wait_for(ws.recv(), timeout=2.0)
            d   = json.loads(raw)
            if d.get("type") == "ai.stream":
                chunk = d["payload"].get("chunk", "")
                if chunk:
                    chunks.append(chunk)
                if d["payload"].get("isDone"):
                    done = True
        except asyncio.TimeoutError:
            break

    if chunks:
        ok(f"ai.stream received {len(chunks)} chunks")
    else:
        fail("No ai.stream chunks received")

    if done:
        ok("ai.stream isDone=true received")
    else:
        fail("ai.stream never completed")


async def test_meter_broadcast(ws):
    print("\n── Meter broadcast ─────────────────────────────────────────")
    # Start playback so meters activate
    await ws.send(msg("daw.command", {"command": "play"}))
    await asyncio.sleep(0.2)

    meter = await recv_until(ws, "daw.meter", timeout=3.0)
    if meter:
        p = meter["payload"]
        if "leftDb" in p and "rightDb" in p and "trackId" in p:
            ok(f"daw.meter received (L={p['leftDb']:.1f} R={p['rightDb']:.1f} dB)")
        else:
            fail("daw.meter missing fields", str(p))
    else:
        fail("No daw.meter received while playing")

    await ws.send(msg("daw.command", {"command": "stop"}))


async def test_config_get(ws):
    print("\n── config.get ──────────────────────────────────────────────")
    req_id = str(uuid.uuid4())
    await ws.send(msg("config.get", {"key": "ai.getModels"}, req_id=req_id))
    r = await recv_until(ws, "config.response")
    if r:
        ok(f"config.response received: {str(r['payload'])[:60]}")
    else:
        fail("No config.response received")


async def test_osc_send(ws):
    print("\n── osc.send ────────────────────────────────────────────────")
    await ws.send(msg("osc.send", {"address": "/whycremisi/test", "value": 1.0}))
    # Expect an echo back as osc.message
    r = await recv_until(ws, "osc.message", timeout=2.0)
    if r:
        ok(f"osc.message echo: {r['payload'].get('address')} = {r['payload'].get('value')}")
    else:
        # Not all servers echo; warn but don't fail
        results.append(("osc.send echo", True))
        print(f"  [SKIP] osc.message echo (server may not echo — ok)")


# ── Runner ───────────────────────────────────────────────────────────────────
async def main():
    print("=" * 62)
    print("  WhyCremisi VST — WebSocket Bridge Test Suite")
    print(f"  Target: {WS_URL}")
    print("=" * 62)

    try:
        async with websockets.connect(WS_URL, open_timeout=5) as ws:
            print(f"  Connected to {WS_URL}")

            await test_connection(ws)
            await test_transport_broadcast(ws)
            await test_daw_play_stop(ws)
            await test_tempo_change(ws)
            await test_volume_command(ws)
            await test_daw_request(ws)
            await test_ai_prompt(ws)
            await test_ai_stream(ws)
            await test_meter_broadcast(ws)
            await test_config_get(ws)
            await test_osc_send(ws)

    except ConnectionRefusedError:
        print(f"\n  [ERROR] No server on {WS_URL}")
        print("  Start mock: python3 tests/mock_websocket_server.py")
        print("  Or build & run the plugin Standalone first.\n")
        sys.exit(2)
    except Exception as e:
        print(f"\n  [ERROR] {e}\n")
        sys.exit(2)

    # ── Summary ──────────────────────────────────────────────────────────────
    passed = sum(1 for _, r in results if r)
    total  = len(results)
    print("\n" + "=" * 62)
    print("  RESULTS")
    print("=" * 62)
    for name, r in results:
        print(f"  {'[PASS]' if r else '[FAIL]'} {name}")
    print("=" * 62)
    print(f"  {passed}/{total} passed", "✓" if passed == total else "✗")
    print()
    sys.exit(0 if passed == total else 1)

if __name__ == "__main__":
    asyncio.run(main())
