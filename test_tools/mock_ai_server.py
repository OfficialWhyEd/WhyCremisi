#!/usr/bin/env python3
"""
Mock AI Server — Simula risposte da Ollama/OpenAI/Anthropic

Cosa fa:
- Riceve richieste JSON dal plugin
- Risponde con risposte AI "finte" ma realistiche
- Permette di testare la UI senza avere AI vera configurata

Come usarlo:
1. python3 mock_ai_server.py
2. Plugin invia richiesta a http://localhost:8080/ai/request
3. Server risponde con JSON tipo AI (proposte widget, analisi, ecc.)
"""

import json
import random
import time
from datetime import datetime
from http.server import HTTPServer, BaseHTTPRequestHandler

class MockAIHandler(BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path == '/ai/request':
            content_length = int(self.headers.get('Content-Length', 0))
            post_data = self.rfile.read(content_length)
            
            try:
                request = json.loads(post_data.decode('utf-8'))
                response = self.generate_ai_response(request)
                
                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps(response).encode())
                
                print(f"[{datetime.now().strftime('%H:%M:%S')}] Richiesta: {request.get('message_type')}")
                print(f"                      Risposta: {response['status']}")
                
            except json.JSONDecodeError:
                self.send_error(400, 'Invalid JSON')
        else:
            self.send_error(404)
    
    def log_message(self, format, *args):
        # Silenzia log HTTP standard
        pass
    
    def generate_ai_response(self, request):
        """Genera risposta AI basata sul tipo di richiesta"""
        
        msg_type = request.get('current_message', {}).get('content', '')
        
        # Simula latenza
        time.sleep(random.uniform(0.5, 1.5))
        
        if 'volume' in msg_type.lower():
            return {
                "message_type": "ai_response",
                "timestamp": time.time(),
                "request_id": request.get('request_id', 'mock-id'),
                "status": "success",
                "provider_used": "mock-ollama",
                "model_used": "mock-llama3",
                "response": {
                    "content": "Ho notato che il volume viene modificato frequentemente. Suggerisco di aggiungere uno slider per controllarlo facilmente.",
                    "tool_calls": [{
                        "name": "suggest_widget",
                        "arguments": {
                            "widget_type": "slider",
                            "label": "Track Volume Control",
                            "reasoning": "Valore modificato frequentemente, range 0-1"
                        }
                    }]
                },
                "usage": {
                    "input_tokens": 45,
                    "output_tokens": 78,
                    "total_tokens": 123,
                    "cost_usd": 0.0
                },
                "latency_ms": 850
            }
        
        elif 'pan' in msg_type.lower():
            return {
                "message_type": "ai_response",
                "timestamp": time.time(),
                "request_id": request.get('request_id', 'mock-id'),
                "status": "success",
                "provider_used": "mock-ollama",
                "model_used": "mock-llama3",
                "response": {
                    "content": "Il panning è un parametro importante per la spatializzazione. Un knob circolare sarebbe ideale.",
                    "tool_calls": [{
                        "name": "suggest_widget",
                        "arguments": {
                            "widget_type": "knob",
                            "label": "Track Pan",
                            "reasoning": "Range -1 a 1, ideale per knob"
                        }
                    }]
                },
                "usage": {
                    "input_tokens": 38,
                    "output_tokens": 65,
                    "total_tokens": 103,
                    "cost_usd": 0.0
                },
                "latency_ms": 720
            }
        
        elif 'transport' in msg_type.lower() or 'play' in msg_type.lower():
            return {
                "message_type": "ai_response",
                "timestamp": time.time(),
                "request_id": request.get('request_id', 'mock-id'),
                "status": "success",
                "provider_used": "mock-ollama",
                "model_used": "mock-llama3",
                "response": {
                    "content": "Rilevato controllo trasporto. Aggiungo un pulsante per play/stop.",
                    "tool_calls": [{
                        "name": "suggest_widget",
                        "arguments": {
                            "widget_type": "button",
                            "label": "Transport Control",
                            "reasoning": "Controllo trasporto essenziale"
                        }
                    }]
                },
                "usage": {
                    "input_tokens": 28,
                    "output_tokens": 52,
                    "total_tokens": 80,
                    "cost_usd": 0.0
                },
                "latency_ms": 610
            }
        
        else:
            return {
                "message_type": "ai_response",
                "timestamp": time.time(),
                "request_id": request.get('request_id', 'mock-id'),
                "status": "success",
                "provider_used": "mock-ollama",
                "model_used": "mock-llama3",
                "response": {
                    "content": "Evento ricevuto. Analisi completata. Nessuna azione suggerita.",
                    "tool_calls": []
                },
                "usage": {
                    "input_tokens": 20,
                    "output_tokens": 35,
                    "total_tokens": 55,
                    "cost_usd": 0.0
                },
                "latency_ms": 450
            }


def main():
    PORT = 8080
    server = HTTPServer(('localhost', PORT), MockAIHandler)
    
    print(f"🤖 Mock AI Server avviato su http://localhost:{PORT}")
    print("Endpoint: POST /ai/request")
    print("Come fermare: CTRL+C")
    print("")
    
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n👋 Mock AI Server fermato")
        server.shutdown()


if __name__ == '__main__':
    main()
