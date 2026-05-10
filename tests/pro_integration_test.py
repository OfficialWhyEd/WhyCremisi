import asyncio
import json
import uuid
import time
import websockets
import sys
from statistics import mean

WS_URL = "ws://localhost:8080"

class ProTester:
    def __init__(self):
        self.results = {
            "connection": False,
            "config_sync": False,
            "ai_latency_ttfb": [],
            "ai_latency_total": [],
            "concurrency_ok": False,
            "stress_test_ok": False
        }

    async def run_all(self):
        print("\n" + "="*60)
        print(" 🛡️  WHYCREMISI PRO: ADVANCED INTEGRATION TEST SUITE")
        print("="*60)

        # Check for environment variable first
        import os, sys
        real_key = os.environ.get("GEMINI_API_KEY")
        use_real = bool(real_key)

        if not use_real and sys.stdin.isatty():
            ans = input("\n [?] Vuoi inserire una API KEY reale per il test? (y/n): ").lower()
            if ans == 'y':
                real_key = input(" [>] Inserisci la tua Gemini API Key: ")
                use_real = bool(real_key)

        if not use_real:
            print(" [INFO] Nessuna API KEY — test in modalità MOCK (Ollama/local)")

        try:
            async with websockets.connect(WS_URL) as ws:
                self.results["connection"] = True
                print(" [OK] WebSocket Bridge Connected.")

                # 1. SETUP & CONFIG SYNC
                await self.test_config_sync(ws, real_key if use_real else "MOCK_KEY")

                # 2. AI LATENCY & STREAMING
                await self.test_ai_latency(ws)

                # 3. CONCURRENCY (DAW during AI)
                await self.test_concurrency(ws)

                # 4. STRESS TEST (Rapid Config)
                await self.test_stress(ws)

                self.print_report()

        except Exception as e:
            print(f"\n [ERROR] Test Sequence Aborted: {e}")
            print(" Is the Mock Server (mock_websocket_server.py) or Plugin running?")

    async def test_config_sync(self, ws, api_key):
        print("\n [1/4] Testing Configuration Sync...")
        test_id = str(uuid.uuid4())
        
        # Send API Key
        msg = {
            "type": "config.set",
            "id": test_id,
            "payload": {"key": "ai.apiKey", "value": api_key, "provider": "gemini"}
        }
        await ws.send(json.dumps(msg))
        
        # Also set provider
        await ws.send(json.dumps({
            "type": "config.set",
            "payload": {"key": "ai.provider", "value": "gemini"}
        }))
        
        # Wait for response
        start = time.time()
        while time.time() - start < 2:
            resp = await ws.recv()
            data = json.loads(resp)
            if data.get("type") == "config.response" and data.get("payload", {}).get("key") == "ai.provider":
                print(f"  ← Received config.response: {data['payload']['status']}")
                self.results["config_sync"] = (data["payload"]["status"] == "ok")
                return
        print("  [!] Timeout waiting for config response")

    async def test_ai_latency(self, ws):
        print("\n [2/4] Testing AI Latency & Streaming...")
        for i in range(3):
            prompt_id = str(uuid.uuid4())
            prompt = f"Latency test prompt {i+1}"

            start_time = time.time()
            ttfb = None

            await ws.send(json.dumps({
                "type": "ai.prompt",
                "id": prompt_id,
                "payload": {"prompt": prompt, "provider": "gemini"}
            }))

            try:
                deadline = time.time() + 8  # 8s timeout per prompt
                while time.time() < deadline:
                    try:
                        resp = await asyncio.wait_for(ws.recv(), timeout=1.0)
                    except asyncio.TimeoutError:
                        continue
                    data = json.loads(resp)

                    if data.get("id") != prompt_id: continue

                    if ttfb is None and data.get("type") == "ai.stream":
                        ttfb = (time.time() - start_time) * 1000
                        self.results["ai_latency_ttfb"].append(ttfb)

                    if data.get("type") == "ai.response" and data["payload"].get("status") == "success":
                        total = (time.time() - start_time) * 1000
                        self.results["ai_latency_total"].append(total)
                        print(f"  → Sample {i+1}: TTFB={ttfb:.1f}ms, Total={total:.1f}ms")
                        break
                else:
                    print(f"  [!] Sample {i+1}: timeout — AI backend not available")
                    break
            except Exception as e:
                print(f"  [!] Sample {i+1} error: {e}")
                break

    async def test_concurrency(self, ws):
        print("\n [3/4] Testing DAW/AI Concurrency...")
        prompt_id = str(uuid.uuid4())
        
        # Start a slow AI prompt
        await ws.send(json.dumps({
            "type": "ai.prompt",
            "id": prompt_id,
            "payload": {"prompt": "Analyze the mix deeply", "provider": "ollama"}
        }))
        
        # Immediately send a DAW command
        await asyncio.sleep(0.1)
        await ws.send(json.dumps({
            "type": "daw.command",
            "payload": {"command": "play"}
        }))
        print("  → Sent AI Prompt followed by DAW Play")

        transport_confirmed = False
        ai_confirmed = False
        
        start = time.time()
        while time.time() - start < 5:
            resp = await ws.recv()
            data = json.loads(resp)
            
            if data.get("type") == "daw.transport" and data["payload"].get("isPlaying"):
                if not transport_confirmed:
                    print(f"  ← [SUCCESS] Transport Play received while AI thinking.")
                    transport_confirmed = True
            
            if data.get("type") == "ai.response" and data.get("id") == prompt_id:
                ai_confirmed = True
                if transport_confirmed: break

        self.results["concurrency_ok"] = transport_confirmed and ai_confirmed

    async def test_stress(self, ws):
        print("\n [4/4] Stress Testing (Rapid Config)...")
        success_count = 0
        total_attempts = 10
        
        for i in range(total_attempts):
            val = f"key_{i}"
            await ws.send(json.dumps({
                "type": "config.set",
                "payload": {"key": "stress.test", "value": val}
            }))
            await asyncio.sleep(0.05) # 50ms interval
        
        # Drain responses — keep polling until 2-second window expires
        start = time.time()
        while time.time() - start < 2:
            try:
                resp = await asyncio.wait_for(ws.recv(), timeout=0.15)
                data = json.loads(resp)
                if data.get("type") == "config.response":
                    success_count += 1
            except asyncio.TimeoutError:
                continue  # keep draining until 2s window
        
        print(f"  → Processed {success_count}/{total_attempts} rapid config changes.")
        self.results["stress_test_ok"] = (success_count >= total_attempts * 0.8)

    def print_report(self):
        print("\n" + "="*60)
        print(" 📊 WHYCREMISI PRO: VALIDATION REPORT")
        print("="*60)
        
        res = self.results
        avg_ttfb = mean(res["ai_latency_ttfb"]) if res["ai_latency_ttfb"] else 0
        avg_total = mean(res["ai_latency_total"]) if res["ai_latency_total"] else 0
        
        print(f" {'Connection':<25} | {'[ PASS ]' if res['connection'] else '[ FAIL ]'}")
        print(f" {'Config Sync':<25} | {'[ PASS ]' if res['config_sync'] else '[ FAIL ]'}")
        print(f" {'Concurrency (DAW+AI)':<25} | {'[ PASS ]' if res['concurrency_ok'] else '[ FAIL ]'}")
        print(f" {'Stress Test':<25} | {'[ PASS ]' if res['stress_test_ok'] else '[ FAIL ]'}")
        print("-" * 60)
        print(f" {'Avg AI TTFB':<25} | {avg_ttfb:.2f} ms")
        print(f" {'Avg AI Total':<25} | {avg_total:.2f} ms")
        print("-" * 60)
        
        status = "✅ COMPLIANT" if all([res['connection'], res['config_sync'], res['concurrency_ok'], res['stress_test_ok']]) else "❌ NON-COMPLIANT"
        print(f" FINAL VERDICT: {status}")
        print("="*60 + "\n")

if __name__ == "__main__":
    tester = ProTester()
    asyncio.run(tester.run_all())
