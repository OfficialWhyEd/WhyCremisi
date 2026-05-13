#!/usr/bin/env python3
"""
OpenClaw VST Bridge AI - Protocol JSON Test Suite
Testa il protocollo JSON C++ ↔ JavaScript senza bisogno del plugin compilato

Autore: Aura
Data: 2026-04-12
"""

import json
import sys
from datetime import datetime
from typing import Dict, Any, Optional
from dataclasses import dataclass, asdict
from enum import Enum

class MessageType(Enum):
    """Tipi di messaggi supportati"""
    # C++ → JS
    DAW_TRANSPORT = "daw.transport"
    DAW_TRACK = "daw.track"
    DAW_METER = "daw.meter"
    DAW_CLIP = "daw.clip"
    OSC_MESSAGE = "osc.message"
    AI_RESPONSE = "ai.response"
    AI_STREAM = "ai.stream"
    UI_WIDGET_CREATE = "ui.widget.create"
    UI_WIDGET_UPDATE = "ui.widget.update"
    UI_WIDGET_REMOVE = "ui.widget.remove"
    PLUGIN_ERROR = "plugin.error"
    
    # JS → C++
    PLUGIN_INIT = "plugin.init"
    DAW_COMMAND = "daw.command"
    DAW_REQUEST = "daw.request"
    AI_PROMPT = "ai.prompt"
    WIDGET_VALUE_CHANGE = "widget.valueChange"
    OSC_SEND = "osc.send"
    CONFIG_GET = "config.get"
    CONFIG_SET = "config.set"


@dataclass
class OpenClawMessage:
    """Classe base per messaggi OpenClaw"""
    type: str
    timestamp: int
    id: Optional[str] = None
    payload: Optional[Dict[str, Any]] = None
    
    def to_json(self) -> str:
        """Serializza in JSON"""
        data = {
            "type": self.type,
            "timestamp": self.timestamp,
            "id": self.id if self.id else None,
            "payload": self.payload if self.payload else {}
        }
        return json.dumps(data, indent=2)
    
    @classmethod
    def from_json(cls, json_str: str) -> 'OpenClawMessage':
        """Deserializza da JSON"""
        data = json.loads(json_str)
        return cls(
            type=data.get("type", ""),
            timestamp=data.get("timestamp", int(datetime.now().timestamp() * 1000)),
            id=data.get("id"),
            payload=data.get("payload", {})
        )


class MessageFactory:
    """Factory per creare messaggi validi"""
    
    @staticmethod
    def create_transport(is_playing: bool = False, 
                        is_recording: bool = False,
                        bpm: float = 120.0,
                        position_seconds: float = 0.0) -> OpenClawMessage:
        """Crea messaggio daw.transport"""
        return OpenClawMessage(
            type=MessageType.DAW_TRANSPORT.value,
            timestamp=int(datetime.now().timestamp() * 1000),
            payload={
                "isPlaying": is_playing,
                "isRecording": is_recording,
                "bpm": bpm,
                "positionSeconds": position_seconds,
                "positionBars": position_seconds * bpm / 60.0 / 4.0,
                "timeSignature": {"numerator": 4, "denominator": 4}
            }
        )
    
    @staticmethod
    def create_track(track_id: int, name: str, volume_db: float = 0.0, 
                     pan: float = 0.0) -> OpenClawMessage:
        """Crea messaggio daw.track"""
        import random
        colors = ["#FF6B6B", "#4ECDC4", "#45B7D1", "#96CEB4", "#FECA57", "#FF9FF3"]
        return OpenClawMessage(
            type=MessageType.DAW_TRACK.value,
            timestamp=int(datetime.now().timestamp() * 1000),
            payload={
                "trackId": track_id,
                "name": name,
                "color": colors[track_id % len(colors)],
                "volume": 10 ** (volume_db / 20),  # dB to linear
                "volumeDb": volume_db,
                "pan": pan,
                "isMuted": False,
                "isSoloed": False,
                "isArmed": False
            }
        )
    
    @staticmethod
    def create_meter(track_id: int, left_db: float, right_db: float) -> OpenClawMessage:
        """Crea messaggio daw.meter"""
        return OpenClawMessage(
            type=MessageType.DAW_METER.value,
            timestamp=int(datetime.now().timestamp() * 1000),
            payload={
                "trackId": track_id,
                "leftDb": left_db,
                "rightDb": right_db,
                "peakLeftDb": left_db + 3.0,
                "peakRightDb": right_db + 3.0
            }
        )
    
    @staticmethod
    def create_osc_message(address: str, value: float) -> OpenClawMessage:
        """Crea messaggio osc.message"""
        return OpenClawMessage(
            type=MessageType.OSC_MESSAGE.value,
            timestamp=int(datetime.now().timestamp() * 1000),
            payload={
                "address": address,
                "value": value,
                "valueType": "float"
            }
        )
    
    @staticmethod
    def create_ai_response(request_id: str, content: str, 
                          provider: str = "ollama") -> OpenClawMessage:
        """Crea messaggio ai.response"""
        return OpenClawMessage(
            type=MessageType.AI_RESPONSE.value,
            timestamp=int(datetime.now().timestamp() * 1000),
            id=request_id,
            payload={
                "status": "success",
                "provider": provider,
                "model": "llama3.2",
                "content": content,
                "tokensUsed": len(content.split()),
                "latencyMs": 850
            }
        )
    
    @staticmethod
    def create_widget_create(widget_id: str, widget_type: str, 
                             title: str, **config) -> OpenClawMessage:
        """Crea messaggio ui.widget.create"""
        return OpenClawMessage(
            type=MessageType.UI_WIDGET_CREATE.value,
            timestamp=int(datetime.now().timestamp() * 1000),
            id=widget_id,
            payload={
                "widgetType": widget_type,
                "title": title,
                **config
            }
        )
    
    @staticmethod
    def create_plugin_init(version: str = "1.0.0") -> OpenClawMessage:
        """Crea messaggio plugin.init (JS → C++)"""
        return OpenClawMessage(
            type=MessageType.PLUGIN_INIT.value,
            timestamp=int(datetime.now().timestamp() * 1000),
            id="init-001",
            payload={
                "version": version,
                "capabilities": ["widgets", "ai", "osc"]
            }
        )
    
    @staticmethod
    def create_daw_command(command: str, **params) -> OpenClawMessage:
        """Crea messaggio daw.command (JS → C++)"""
        return OpenClawMessage(
            type=MessageType.DAW_COMMAND.value,
            timestamp=int(datetime.now().timestamp() * 1000),
            id=f"cmd-{command}",
            payload={
                "command": command,
                **params
            }
        )
    
    @staticmethod
    def create_ai_prompt(prompt: str, request_id: str = None) -> OpenClawMessage:
        """Crea messaggio ai.prompt (JS → C++)"""
        import uuid
        return OpenClawMessage(
            type=MessageType.AI_PROMPT.value,
            timestamp=int(datetime.now().timestamp() * 1000),
            id=request_id or str(uuid.uuid4()),
            payload={
                "prompt": prompt,
                "provider": "ollama",
                "model": "llama3.2",
                "stream": False,
                "context": {}
            }
        )


class ProtocolValidator:
    """Valida messaggi secondo il protocollo v1.0"""
    
    REQUIRED_FIELDS = ["type", "timestamp", "payload"]
    VALID_TYPES = [t.value for t in MessageType]
    
    @classmethod
    def validate(cls, message: OpenClawMessage) -> tuple[bool, str]:
        """Valida un messaggio. Ritorna (is_valid, error_message)"""
        
        # Check campi richiesti
        if not message.type:
            return False, "Campo 'type' mancante"
        
        if message.type not in cls.VALID_TYPES:
            return False, f"Tipo '{message.type}' non valido. Validi: {cls.VALID_TYPES}"
        
        if not isinstance(message.timestamp, int):
            return False, f"Campo 'timestamp' deve essere int, trovato: {type(message.timestamp)}"
        
        if message.payload is None:
            return False, "Campo 'payload' mancante"
        
        if not isinstance(message.payload, dict):
            return False, f"Campo 'payload' deve essere object, trovato: {type(message.payload)}"
        
        # Validazione specifica per tipo
        return cls._validate_type_specific(message)
    
    @classmethod
    def _validate_type_specific(cls, message: OpenClawMessage) -> tuple[bool, str]:
        """Validazione specifica per tipo di messaggio"""
        msg_type = message.type
        payload = message.payload
        
        if msg_type == MessageType.DAW_TRANSPORT.value:
            required = ["isPlaying", "isRecording", "bpm"]
            for field in required:
                if field not in payload:
                    return False, f"Campo '{field}' mancante in daw.transport"
        
        elif msg_type == MessageType.DAW_TRACK.value:
            required = ["trackId", "name"]
            for field in required:
                if field not in payload:
                    return False, f"Campo '{field}' mancante in daw.track"
        
        elif msg_type == MessageType.AI_RESPONSE.value:
            if not message.id:
                return False, "Campo 'id' richiesto per ai.response"
            if "content" not in payload:
                return False, "Campo 'content' mancante in ai.response"
        
        elif msg_type == MessageType.UI_WIDGET_CREATE.value:
            if not message.id:
                return False, "Campo 'id' richiesto per ui.widget.create"
            if "widgetType" not in payload:
                return False, "Campo 'widgetType' mancante"
            if "title" not in payload:
                return False, "Campo 'title' mancante"
        
        elif msg_type == MessageType.AI_PROMPT.value:
            if not message.id:
                return False, "Campo 'id' richiesto per ai.prompt"
            if "prompt" not in payload:
                return False, "Campo 'prompt' mancante"
        
        return True, "OK"


def run_tests():
    """Esegue suite di test"""
    print("=" * 70)
    print("OpenClaw VST Bridge AI - Protocol JSON Test Suite v1.0")
    print("=" * 70)
    print()
    
    factory = MessageFactory()
    validator = ProtocolValidator()
    
    tests = [
        ("DAW Transport (play)", factory.create_transport(is_playing=True, bpm=128.0)),
        ("DAW Transport (stop)", factory.create_transport(is_playing=False, bpm=120.0)),
        ("DAW Track", factory.create_track(1, "Lead Vocal", volume_db=-2.5, pan=0.2)),
        ("DAW Meter", factory.create_meter(1, -12.5, -11.8)),
        ("OSC Message", factory.create_osc_message("/track/1/volume", 0.75)),
        ("AI Response", factory.create_ai_response("req-001", "Suggerimento: alza i 3kHz")),
        ("Widget Create (slider)", factory.create_widget_create(
            "widget-001", "slider", "EQ High", 
            minValue=-15.0, maxValue=15.0, defaultValue=0.0, step=0.1, unit="dB"
        )),
        ("Plugin Init", factory.create_plugin_init()),
        ("DAW Command (play)", factory.create_daw_command("play")),
        ("DAW Command (setVolume)", factory.create_daw_command("setVolume", trackId=1, valueDb=-3.0)),
        ("AI Prompt", factory.create_ai_prompt("Analizza la traccia 1")),
    ]
    
    passed = 0
    failed = 0
    
    for test_name, message in tests:
        # Serializza
        json_str = message.to_json()
        
        # Deserializza
        try:
            restored = OpenClawMessage.from_json(json_str)
        except json.JSONDecodeError as e:
            print(f"❌ {test_name}: JSON decode error - {e}")
            failed += 1
            continue
        
        # Valida
        is_valid, error_msg = validator.validate(restored)
        
        if is_valid:
            print(f"✅ {test_name}: OK")
            passed += 1
        else:
            print(f"❌ {test_name}: {error_msg}")
            failed += 1
        
        # Stampa JSON (primi 200 char)
        json_preview = json_str.replace('\n', ' ')[:200]
        print(f"   JSON: {json_preview}...")
        print()
    
    # Test errori
    print("-" * 70)
    print("Test error handling:")
    print("-" * 70)
    
    # Messaggio senza tipo
    invalid_msg = OpenClawMessage(type="", timestamp=123, payload={})
    is_valid, error = validator.validate(invalid_msg)
    print(f"{'❌' if not is_valid else '✅'} Messaggio senza tipo: {error}")
    
    # Tipo non valido
    invalid_msg = OpenClawMessage(type="invalid.type", timestamp=123, payload={})
    is_valid, error = validator.validate(invalid_msg)
    print(f"{'❌' if not is_valid else '✅'} Tipo non valido: {error}")
    
    # Payload mancante
    invalid_msg = OpenClawMessage(type="daw.transport", timestamp=123, payload=None)
    is_valid, error = validator.validate(invalid_msg)
    print(f"{'❌' if not is_valid else '✅'} Payload mancante: {error}")
    
    # daw.track senza trackId
    invalid_track = OpenClawMessage(
        type="daw.track",
        timestamp=123,
        payload={"name": "Test"}  # manca trackId
    )
    is_valid, error = validator.validate(invalid_track)
    print(f"{'❌' if not is_valid else '✅'} daw.track senza trackId: {error}")
    
    print()
    print("=" * 70)
    print(f"Risultati: {passed} passati, {failed} falliti")
    print("=" * 70)
    
    return failed == 0


if __name__ == "__main__":
    success = run_tests()
    sys.exit(0 if success else 1)