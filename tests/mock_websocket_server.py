#!/usr/bin/env python3
"""
WhyCremisi VST — Mock WebSocket Server (protocol-json-v1)

Simulates the C++ OscBridge server on ws://localhost:8080
Use for testing the React UI without building the plugin.

Protocol: JSON text frames per protocol-json-v1.md
  - Server broadcasts: daw.transport, daw.meter, ai.response, ai.stream, plugin.init
  - Client sends:      plugin.init, daw.command, daw.request, ai.prompt, config.*
"""

import asyncio
import json
import math
import random
import time
import uuid
import websockets
from datetime import datetime

# ── Simulated DAW state ─────────────────────────────────────────────────────
daw_state = {
    "isPlaying": False,
    "isRecording": False,
    "bpm": 128.0,
    "positionSeconds": 0.0,
    "tracks": {
        0: {"name": "Master",   "volumeDb": 0.0,  "pan": 0.0, "isMuted": False, "isSoloed": False},
        1: {"name": "Kick",     "volumeDb": -3.0, "pan": 0.0, "isMuted": False, "isSoloed": False},
        2: {"name": "Synth",    "volumeDb": -6.0, "pan": -0.3,"isMuted": False, "isSoloed": False},
        3: {"name": "Vocals",   "volumeDb": -9.0, "pan": 0.1, "isMuted": False, "isSoloed": False},
    }
}

connected_clients: set = set()
now_ms = lambda: int(time.time() * 1000)


# ── JSON message helpers ─────────────────────────────────────────────────────
def make_msg(msg_type: str, payload: dict, req_id: str = None) -> str:
    return json.dumps({
        "type": msg_type,
        "id": req_id,
        "timestamp": now_ms(),
        "payload": payload
    })

def transport_msg() -> str:
    return make_msg("daw.transport", {
        "isPlaying": daw_state["isPlaying"],
        "isRecording": daw_state["isRecording"],
        "bpm": daw_state["bpm"],
        "positionSeconds": daw_state["positionSeconds"],
        "timeSignature": {"numerator": 4, "denominator": 4}
    })

def meter_msg(track_id: int, l_db: float, r_db: float) -> str:
    return make_msg("daw.meter", {
        "trackId": track_id,
        "leftDb": round(l_db, 2),
        "rightDb": round(r_db, 2),
        "peakLeftDb": round(l_db + random.uniform(0, 2), 2),
        "peakRightDb": round(r_db + random.uniform(0, 2), 2),
    })


# ── Broadcast helpers ────────────────────────────────────────────────────────
async def broadcast(msg: str):
    if connected_clients:
        await asyncio.gather(
            *[c.send(msg) for c in connected_clients],
            return_exceptions=True
        )


# ── Message handler ──────────────────────────────────────────────────────────
async def handle_message(ws, raw: str):
    try:
        msg = json.loads(raw)
    except json.JSONDecodeError:
        await ws.send(make_msg("plugin.error", {"code": "PARSE_ERROR", "message": "Invalid JSON", "severity": "error"}))
        return

    msg_type = msg.get("type", "")
    payload  = msg.get("payload", {})
    req_id   = msg.get("id")

    print(f"[{datetime.now().strftime('%H:%M:%S')}] ← {msg_type}  {json.dumps(payload)[:80]}")

    # ── plugin.init ────────────────────────────────────────────────────────
    if msg_type == "plugin.init":
        # Send plugin capabilities
        await ws.send(make_msg("plugin.init", {
            "version": "1.0.0-mock",
            "oscPort": 9000,
            "wsPort": 8080,
            "capabilities": ["widgets", "ai", "osc", "daw"]
        }))
        # Send current transport state
        await ws.send(transport_msg())

    # ── daw.command ────────────────────────────────────────────────────────
    elif msg_type == "daw.command":
        command = payload.get("command", "")

        if command == "play":
            daw_state["isPlaying"] = True
            print(f"  ▶  DAW: PLAY")
        elif command == "stop":
            daw_state["isPlaying"] = False
            daw_state["positionSeconds"] = 0.0
            print(f"  ■  DAW: STOP")
        elif command == "record":
            daw_state["isRecording"] = not daw_state["isRecording"]
            print(f"  ●  DAW: RECORD {'ON' if daw_state['isRecording'] else 'OFF'}")
        elif command == "pause":
            daw_state["isPlaying"] = False
            print(f"  ❙❙ DAW: PAUSE")
        elif command == "setTempo":
            bpm = payload.get("bpm", 128.0)
            daw_state["bpm"] = bpm
            print(f"  ♩  DAW: BPM → {bpm}")
        elif command == "setVolume":
            tid  = payload.get("trackId", 0)
            db   = payload.get("valueDb", 0.0)
            if tid in daw_state["tracks"]:
                daw_state["tracks"][tid]["volumeDb"] = db
            print(f"  🎚  Track {tid} volume → {db:.1f} dB")
        elif command == "setPan":
            tid  = payload.get("trackId", 0)
            pan  = payload.get("value", 0.0)
            if tid in daw_state["tracks"]:
                daw_state["tracks"][tid]["pan"] = pan
        elif command == "muteTrack":
            tid   = payload.get("trackId", 0)
            muted = payload.get("muted", False)
            if tid in daw_state["tracks"]:
                daw_state["tracks"][tid]["isMuted"] = muted
        elif command == "soloTrack":
            tid    = payload.get("trackId", 0)
            soloed = payload.get("soloed", False)
            if tid in daw_state["tracks"]:
                daw_state["tracks"][tid]["isSoloed"] = soloed
        elif command == "setGain":
            print(f"  🎛  Master gain → {payload.get('valueDb', 0):.1f} dB")
        elif command == "setDrive":
            print(f"  🔥 Drive → {payload.get('value', 0):.2f}")
        elif command == "requestTransport":
            pass  # Just broadcast below

        # Always broadcast transport after any command
        await broadcast(transport_msg())

    # ── daw.request ────────────────────────────────────────────────────────
    elif msg_type == "daw.request":
        request = payload.get("request", "")

        if request == "transport":
            await ws.send(make_msg("daw.response", {
                "isPlaying": daw_state["isPlaying"],
                "isRecording": daw_state["isRecording"],
                "bpm": daw_state["bpm"],
                "positionSeconds": daw_state["positionSeconds"]
            }, req_id=req_id))

        elif request == "trackInfo":
            tid = payload.get("trackId", 1)
            track = daw_state["tracks"].get(tid, {"name": f"Track {tid}", "volumeDb": 0, "pan": 0, "isMuted": False, "isSoloed": False})
            await ws.send(make_msg("daw.response", {
                "trackId": tid, **track
            }, req_id=req_id))

        elif request == "allTracks":
            tracks_list = [{"trackId": tid, **info} for tid, info in daw_state["tracks"].items()]
            await ws.send(make_msg("daw.response", {"tracks": tracks_list}, req_id=req_id))

    # ── ai.prompt ──────────────────────────────────────────────────────────
    elif msg_type == "ai.prompt":
        prompt   = payload.get("prompt", "")
        provider = payload.get("provider", "ollama")
        active_id = req_id or str(uuid.uuid4())

        print(f"  🤖 AI prompt: {prompt[:60]}...")

        # Thinking
        await ws.send(make_msg("ai.response", {
            "status": "thinking",
            "provider": provider,
            "content": ""
        }, req_id=active_id))

        # Simulate streaming (chunked response)
        fake_response = (
            f"Analisi del tuo progetto musicale: '{prompt[:40]}...' — "
            "Suggerisco di aumentare la kick drum di 3dB alle basse frequenze, "
            "applicare una compressione 4:1 sulla somma stereo, "
            "e ridurre le frequenze medie del synth pad intorno a 800Hz di 2dB. "
            "Il BPM attuale di {bpm:.0f} BPM è ottimale per questo genere. "
            "Considera anche l'aggiunta di un limiter sul master con threshold a -0.3dBFS."
        ).format(bpm=daw_state["bpm"])

        words = fake_response.split()
        chunk_size = 3
        await asyncio.sleep(0.8)  # thinking delay

        for i in range(0, len(words), chunk_size):
            chunk = " ".join(words[i:i+chunk_size]) + " "
            await ws.send(make_msg("ai.stream", {
                "chunk": chunk,
                "isDone": False
            }, req_id=active_id))
            await asyncio.sleep(0.05)

        # Final done signal
        await ws.send(make_msg("ai.stream", {"chunk": "", "isDone": True}, req_id=active_id))

        # Also send complete response
        await ws.send(make_msg("ai.response", {
            "status": "success",
            "provider": provider,
            "model": "llama3.2-mock",
            "content": fake_response,
            "durationMs": 1200
        }, req_id=active_id))

    # ── osc.send ───────────────────────────────────────────────────────────
    elif msg_type == "osc.send":
        address = payload.get("address", "/unknown")
        value   = payload.get("value", 0.0)
        print(f"  📡 OSC → {address} = {value}")
        # Echo back as osc.message (simulating DAW feedback)
        await asyncio.sleep(0.05)
        await broadcast(make_msg("osc.message", {
            "address": address, "value": value, "valueType": "float"
        }))

    # ── config.* ───────────────────────────────────────────────────────────
    elif msg_type in ("config.get", "config.set"):
        key = payload.get("key", "")
        await ws.send(make_msg("config.response", {
            "key": key,
            "status": "ok",
            "value": payload.get("value"),
            "connected": True,
            "models": ["llama3.2", "llama3.1:8b", "mistral"],
            "provider": "ollama"
        }, req_id=req_id))

    else:
        print(f"  ⚠  Unknown type: {msg_type}")


# ── Connection handler ───────────────────────────────────────────────────────
async def handle_client(websocket):
    client_id = id(websocket) % 10000
    print(f"\n[{datetime.now().strftime('%H:%M:%S')}] ✅ Client {client_id} connected")
    connected_clients.add(websocket)

    try:
        async for message in websocket:
            await handle_message(websocket, message)
    except websockets.exceptions.ConnectionClosed:
        pass
    finally:
        connected_clients.discard(websocket)
        print(f"[{datetime.now().strftime('%H:%M:%S')}] ❌ Client {client_id} disconnected")


# ── Background simulators ────────────────────────────────────────────────────
async def simulate_transport():
    """Advance position and broadcast transport every second while playing."""
    while True:
        await asyncio.sleep(1.0)
        if connected_clients and daw_state["isPlaying"]:
            daw_state["positionSeconds"] += 1.0
            await broadcast(transport_msg())


async def simulate_meters():
    """Broadcast fake meter levels at ~30fps while playing, 2fps otherwise."""
    phase = 0.0
    while True:
        playing = daw_state["isPlaying"]
        await asyncio.sleep(0.033 if playing else 0.5)

        if not connected_clients:
            continue

        phase += 0.15
        if playing:
            # Realistic dancing meters
            base_l = -18.0 + 12.0 * abs(math.sin(phase))
            base_r = -18.0 + 10.0 * abs(math.sin(phase + 0.3))
            noise  = random.uniform(-2, 2)
        else:
            base_l = base_r = -60.0
            noise  = 0.0

        await broadcast(meter_msg(-1, base_l + noise, base_r + noise))


async def main():
    print("=" * 62)
    print("  WhyCremisi VST — Mock WebSocket Server (protocol-json-v1)")
    print("=" * 62)
    print(f"  WebSocket : ws://localhost:8080")
    print(f"  Protocol  : protocol-json-v1")
    print(f"  Press Ctrl+C to stop")
    print("=" * 62)

    async with websockets.serve(handle_client, "localhost", 8080):
        await asyncio.gather(
            simulate_transport(),
            simulate_meters(),
        )


if __name__ == "__main__":
    asyncio.run(main())
