#!/usr/bin/env python3
"""
WhyCremisi — Mock AbletonOSC UDP Server

Simulates AbletonOSC responses so you can test the plugin's OSC<->WebSocket
bridge without a real Ableton instance.

AbletonOSC protocol:
  Plugin sends queries to port 9001 (DAW OSC receive)
  This mock listens on 9001, and responds back to plugin on port 9000

Usage:
  python3 tests/mock_ableton_osc.py
  (run alongside the plugin Standalone or VST3 in DAW)
"""

import socket
import struct
import threading
import time
import math
import random

PLUGIN_RECEIVE_PORT = 9000   # plugin listens here (we send to this)
PLUGIN_SEND_PORT    = 9001   # plugin sends here (we listen on this)
HOST                = "127.0.0.1"

# ── DAW state ──────────────────────────────────────────────────────────────
state = {
    "is_playing": 0.0,
    "is_recording": 0.0,
    "tempo": 128.0,
    "current_song_time": 0.0,
    "track_volume": [0.85, 0.75, 0.70, 0.90],   # linear 0-1
    "track_pan":    [0.0,  -0.2,  0.2,  0.0],    # -1 to 1
    "track_mute":   [0.0,   0.0,  0.0,  0.0],
    "track_solo":   [0.0,   0.0,  0.0,  0.0],
}


# ── OSC packet builder (minimal, float-only) ───────────────────────────────
def pad4(s: bytes) -> bytes:
    """Pad bytes to multiple of 4."""
    rem = len(s) % 4
    return s + b'\x00' * ((4 - rem) % 4)

def build_osc(address: str, *args) -> bytes:
    """Build a minimal OSC packet with float32 arguments."""
    addr_bytes = pad4(address.encode() + b'\x00')
    type_tag   = b',' + b'f' * len(args) + b'\x00'
    type_bytes = pad4(type_tag)
    arg_bytes  = b''.join(struct.pack('>f', float(a)) for a in args)
    return addr_bytes + type_bytes + arg_bytes

def parse_osc(data: bytes):
    """Parse OSC packet → (address, [float args])."""
    try:
        null = data.index(b'\x00')
        address = data[:null].decode('ascii', errors='replace')
        # Skip to next 4-byte boundary
        i = (null // 4 + 1) * 4
        if i >= len(data):
            return address, []
        # Type tag
        if data[i:i+1] == b',':
            ttag_end = data.index(b'\x00', i)
            type_tag = data[i+1:ttag_end].decode('ascii', errors='replace')
            i = (ttag_end // 4 + 1) * 4
            args = []
            for t in type_tag:
                if t == 'f' and i + 4 <= len(data):
                    args.append(struct.unpack('>f', data[i:i+4])[0])
                    i += 4
                elif t == 'i' and i + 4 <= len(data):
                    args.append(struct.unpack('>i', data[i:i+4])[0])
                    i += 4
            return address, args
        return address, []
    except Exception as e:
        return None, []


# ── Response dispatcher ────────────────────────────────────────────────────
def respond(sock, address: str, args: list):
    """Send OSC response back to the plugin."""

    def send(resp_addr, *vals):
        pkt = build_osc(resp_addr, *vals)
        sock.sendto(pkt, (HOST, PLUGIN_RECEIVE_PORT))
        print(f"  → {resp_addr} = {vals}")

    addr = address.lower()

    if addr in ("/live/song/get/is_playing", "/live/song/get/tempo",
                "/live/song/get/current_song_time", "/live/song/get/is_recording"):
        key = addr.split("/get/")[-1]
        send(f"/live/song/get/{key}", state[key])

    elif addr == "/live/song/start_playing":
        state["is_playing"] = 1.0
        send("/live/song/get/is_playing", 1.0)

    elif addr == "/live/song/stop_playing":
        state["is_playing"] = 0.0
        state["current_song_time"] = 0.0
        send("/live/song/get/is_playing", 0.0)

    elif addr == "/live/song/record":
        state["is_recording"] = 1.0 - state["is_recording"]
        send("/live/song/get/is_recording", state["is_recording"])

    elif addr == "/live/song/set/tempo" and args:
        state["tempo"] = args[0]
        send("/live/song/get/tempo", state["tempo"])

    elif "/live/track/" in addr and "/set/volume" in addr and args:
        # Extract track index from path: /live/track/N/set/volume
        parts = addr.split("/")
        try:
            idx = int(parts[3])
            if 0 <= idx < len(state["track_volume"]):
                state["track_volume"][idx] = args[0]
                send(f"/live/track/{idx}/get/volume", args[0])
        except (IndexError, ValueError):
            pass

    elif "/live/track/" in addr and "/set/panning" in addr and args:
        parts = addr.split("/")
        try:
            idx = int(parts[3])
            if 0 <= idx < len(state["track_pan"]):
                state["track_pan"][idx] = args[0]
                send(f"/live/track/{idx}/get/panning", args[0])
        except (IndexError, ValueError):
            pass

    elif "/live/track/" in addr and "/set/mute" in addr and args:
        parts = addr.split("/")
        try:
            idx = int(parts[3])
            if 0 <= idx < len(state["track_mute"]):
                state["track_mute"][idx] = args[0]
                send(f"/live/track/{idx}/get/mute", args[0])
        except (IndexError, ValueError):
            pass

    elif "/live/master_track/set/volume" in addr and args:
        print(f"  🎚 Master volume → {args[0]:.3f}")

    elif addr in ("/play", "/stop", "/record", "/pause"):
        # Generic REAPER-style fallback
        if addr == "/play":
            state["is_playing"] = 1.0
            send("/live/song/get/is_playing", 1.0)
        elif addr == "/stop":
            state["is_playing"] = 0.0
            send("/live/song/get/is_playing", 0.0)
    else:
        print(f"  ⚠  Unhandled: {address} {args}")


# ── Position ticker ────────────────────────────────────────────────────────
def position_ticker(sock):
    """Advance song position while playing and push updates to plugin."""
    while True:
        time.sleep(1.0)
        if state["is_playing"] > 0.5:
            state["current_song_time"] += 1.0
            pkt = build_osc("/live/song/get/current_song_time", state["current_song_time"])
            sock.sendto(pkt, (HOST, PLUGIN_RECEIVE_PORT))


# ── Main listener ──────────────────────────────────────────────────────────
def main():
    print("=" * 60)
    print("  WhyCremisi — Mock AbletonOSC UDP Server")
    print(f"  Listening on UDP {HOST}:{PLUGIN_SEND_PORT}")
    print(f"  Responding to plugin on UDP {HOST}:{PLUGIN_RECEIVE_PORT}")
    print(f"  Press Ctrl+C to stop")
    print("=" * 60)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((HOST, PLUGIN_SEND_PORT))
    sock.settimeout(1.0)

    # Push initial state to plugin
    time.sleep(0.5)
    for key, addr in [
        ("is_playing", "/live/song/get/is_playing"),
        ("tempo",      "/live/song/get/tempo"),
        ("current_song_time", "/live/song/get/current_song_time"),
    ]:
        pkt = build_osc(addr, state[key])
        sock.sendto(pkt, (HOST, PLUGIN_RECEIVE_PORT))
        print(f"  ← Initial push: {addr} = {state[key]}")

    threading.Thread(target=position_ticker, args=(sock,), daemon=True).start()

    print("\nListening for OSC from plugin...\n")
    while True:
        try:
            data, addr_from = sock.recvfrom(4096)
            address, args = parse_osc(data)
            if address:
                print(f"[← {addr_from[0]}:{addr_from[1]}] {address} {args}")
                respond(sock, address, args)
        except socket.timeout:
            continue
        except KeyboardInterrupt:
            print("\nStopped.")
            break


if __name__ == "__main__":
    main()
