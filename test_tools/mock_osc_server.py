#!/usr/bin/env python3
"""
Mock Server OSC — Simula un DAW per testare il plugin

Cosa fa questo programma:
- Invia messaggi OSC falsi che sembrano venire da Reaper/Ableton
- Permette a Heartbroken di testare la UI React senza avere il plugin C++ pronto
- Permette a Edo di vedere cosa succede senza aprire Ableton

Come usarlo:
1. Apri terminale
2. python3 mock_osc_server.py
3. Il programma invia messaggi OSC automaticamente
4. Il plugin (o la UI di test) riceve i messaggi come se venissero dal DAW vero

Nota: questo è un FINTO DAW, serve solo per testare.
"""

import time
import random
from pythonosc import udp_client

def main():
    # Configurazione
    IP = "127.0.0.1"      # localhost (questo computer)
    PORT = 9000           # porta dove il plugin ascolta
    
    print("🎵 Mock DAW Server avviato")
    print(f"Invio messaggi OSC falsi a {IP}:{PORT}")
    print("Come fermare: premi CTRL+C")
    print("")
    
    # Crea il client OSC
    client = udp_client.SimpleUDPClient(IP, PORT)
    
    # Contatore per i messaggi
    msg_count = 0
    
    try:
        while True:
            # Simula cambiamento volume su Track 1
            volume = round(random.uniform(0.0, 1.0), 2)
            client.send_message("/track/1/volume", volume)
            print(f"📤 Messaggio {msg_count}: Volume Track 1 = {volume}")
            msg_count += 1
            time.sleep(2)
            
            # Simula cambiamento pan su Track 1
            pan = round(random.uniform(-1.0, 1.0), 2)
            client.send_message("/track/1/pan", pan)
            print(f"📤 Messaggio {msg_count}: Pan Track 1 = {pan}")
            msg_count += 1
            time.sleep(2)
            
            # Simula trasport (play/stop) ogni tanto
            if random.random() > 0.7:
                client.send_message("/play", 1)
                print(f"📤 Messaggio {msg_count}: ▶️ Play")
                msg_count += 1
                time.sleep(1)
            
    except KeyboardInterrupt:
        print("\n👋 Mock server fermato")

if __name__ == "__main__":
    # Controlla se python-osc è installato
    try:
        from pythonosc import udp_client
    except ImportError:
        print("❌ Errore: python-osc non installato")
        print("Installa con: pip install python-osc")
        exit(1)
    
    main()
