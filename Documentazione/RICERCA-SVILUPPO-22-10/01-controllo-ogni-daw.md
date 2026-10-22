# Controllo di ogni DAW da un plugin (VST3 / AU / CLAP)

Data: 22/10. Solo ricerca web, nessun codice eseguito. Le voci marcate "DA VERIFICARE" vengono da memoria o da fonti non conclusive: prima di usarle, controllare la fonte.

## Tabella per DAW

| DAW | Protocollo / API di controllo | Cosa permette | Auto-installazione (Win / Mac) | Licenza repo utili | Note e limiti |
|---|---|---|---|---|---|
| Ableton Live | Remote Script in Python 3 (API interna) + LOM via OSC con AbletonOSC | Transport, tracce, volume, nomi tracce, parametri, creare clip e note (con repo MCP) | Win: `Documents\Ableton\User Library\Remote Scripts\` / Mac: `Music/Ableton/User Library/Remote Scripts/` ([Ableton help](https://help.ableton.com/hc/en-us/articles/209072009)). Va poi selezionato una volta in Preferenze > MIDI | AbletonOSC: MIT ([ideoforms/AbletonOSC](https://github.com/ideoforms/AbletonOSC)) | Serve un passo manuale: selezionare lo script in Preferenze. Porte OSC 11000 in / 11001 out |
| FL Studio | MIDI scripting Python (`device_<nome>.py`) | Controllo di mixer, transport, eventi MIDI; accesso limitato a parti del progetto | Win: `Documents\Image-Line\FL Studio\Settings\Hardware\<nome>\device_<nome>.py` ([manuale Image-Line](https://www.image-line.com/support/FLHelp/html/midi_scripting.htm)). Mac: da verificare (fonte di terzi indica `$HOME/Documents/Image-Line/FL Studio/Settings/Hardware`) | MCP esistente: karl-andres/fl-studio-mcp, licenza non verificata | Script Python semplice, nessuna installazione extra. Il nome del controller viene dalla prima riga (`#name=`) |
| Logic Pro | Control surface Mackie Control (MCU) integrato | Transport, fader, pan, mute, solo sulla traccia selezionata | Non applicabile: Logic gira solo su macOS. Controllo solo via MCU su porta MIDI virtuale (CoreMIDI, nessun driver) | Documentazione Apple | Nessuna API di scripting pubblica. Scrivere note o clip: non previsto dal protocollo |
| Cubase / Nuendo | MIDI Remote API (script JavaScript `.js` in `Driver Scripts\Local`) | Controllo profondo dei controller: parametri, transport, tracce, eventi | Win e Mac: `Documents\Steinberg\Cubase\MIDI Remote\Driver Scripts\Local\` ([fonte Midikinetics](https://support.midikinetics.com/?p=3007)). Nuendo ha cartella separata | Non verificata (script di terzi) | Serve riavviare o riselezionare il dispositivo. Script `.midiremote` da importare dal MIDI Remote Manager |
| Studio One | Control Link (mappa parametri plugin al telecomando) + UCNET (Studio One Remote) | Parametri plugin, controllo transport | Non trovato | Non trovato | Nessuna API pubblica trovata nella ricerca. Da cercare nella documentazione PreSonus |
| Reaper | ReaScript (Lua, EEL2, Python) + OSC integrato + WebSocket di terzi | Tutto: transport, tracce, volume, parametri FX, creare item e note, leggere nomi | Script in cartella `UserPlugins` di Reaper (Win: `%APPDATA%\REAPER\UserPlugins`, Mac: `~/Library/Application Support/REAPER/UserPlugins`, DA VERIFICARE). Il plugin può scrivere il file da solo | OSC integrato: nessuna licenza separata. Bridge: lucianoiam/reaper-websockets (licenza non verificata). SWS: open source (licenza da verificare) | Il migliore candidato per controllo completo e automatico |
| Bitwig Studio | Controller Extension API (Java, file `.bwextension`) | Transport, tracce, device, parametri, clip | Win: `Documents\Bitwig Studio\Extensions\` (fonte di terzi). Mac: `Documents/Bitwig Studio/Extensions` ([Bitwig community](https://www.bitwig.com/en/community/control_scripts.html)) | Framework Bitwig: dichiarati open source su GitHub. DrivenByMoss: licenza da verificare, probabilmente GPL: NON linkare | Un `.bwextension` precompilato si può distribuire. Mancano una reference API pubblica unica e file di config per estensione |
| Pro Tools | HUI (legacy, non più sviluppato) ed EUCON (proprietario, licenza Avid) | Fader, transport, pan, plugin base via HUI | Non applicabile: nessuno script pubblico. Solo porta MIDI virtuale con HUI | Non applicabile | Nessun MIDI Learn. Avid non supporta nuovo sviluppo HUI dalla 8.0.5. Creare clip e note: non previsto |
| Reason | Protocollo Remote (SDK solo per produttori hardware) | Controllo dei device di Reason tramite protocollo proprietario | Non applicabile | Non applicabile | Nessuna API pubblica per plugin esterni trovata. Da verificare |
| Ardour | Lua scripting interno + OSC integrato | Transport, tracce, azioni, controllo via OSC | Script Lua in cartella utente (DA VERIFICARE per Win e Mac) | Ardour: GPL. Script Lua caricati dentro Ardour non sono codice linkato, ma verificare | Ardour gira su Win e Mac ma è poco diffuso |
| GarageBand | Non trovato | Non trovato | Non applicabile | Non applicabile | GarageBand è solo macOS e non ha API di scripting pubblica nella ricerca. Da trattare come caso Logic |

## Repo e progetti LLM / MCP già esistenti

| Progetto | DAW | Metodo | Licenza (dalla pagina) |
|---|---|---|---|
| [ideoforms/AbletonOSC](https://github.com/ideoforms/AbletonOSC) | Ableton | Remote Script + OSC | MIT |
| [Simon-Kansara/ableton-live-mcp-server](https://github.com/Simon-Kansara/ableton-live-mcp-server) | Ableton | MCP su AbletonOSC | Non verificata |
| [uisato/ableton-mcp-extended](https://glama.ai/mcp/servers/uisato/ableton-mcp-extended) | Ableton | MCP | MIT (da listing) |
| [ahujasid/ableton-mcp](https://github.com/ahujasid/ableton-mcp) (fork itsuzef) | Ableton | MCP, creazione clip e strumenti | Non verificata |
| [opendining/ableton-mcp-server](https://glama.ai/mcp/servers/opendining/ableton-mcp-server) | Ableton | MCP con Remote Script, Live 12 | Non verificata |
| [karl-andres/fl-studio-mcp](https://glama.ai/mcp/servers/@karl-andres/fl-studio-mcp) | FL Studio | MCP con script | Non verificata |
| [lucianoiam/reaper-websockets](https://github.com/lucianoiam/reaper-websockets) | Reaper | Bridge WebSocket su OSC | Non verificata |
| yura9011/scythe_mcp_reaper, itsuzef reaper MCP | Reaper | MCP | Non verificate |
| [git-moss/DrivenByMoss](https://github.com/git-moss/DrivenByMoss) | Bitwig (molti controller) | Controller Extension | DA VERIFICARE: probabilmente GPL, NON linkare |
| [JamesB-VS/AtomSQ_Bitwig](https://github.com/JamesB-VS/AtomSQ_Bitwig), [lplath/Oxygen49_Bitwig_Extension](https://github.com/lplath/Oxygen49_Bitwig_Extension) | Bitwig | Extension di esempio | Non verificate |

## Protocolli universali

- **Mackie Control Universal (MCU)**: fader, transport, pan, mute, solo, display LCD con nomi tracce. Funziona con Logic, Cubase, Pro Tools (anche se HUI è legacy), Reaper, Studio One e altri. Limiti: 8 fader per device (più device XT per più canali), nessuna scrittura di note o clip, nessun parametro plugin profondo.
- **HUI**: più vecchio, usato da Pro Tools. Stessi limiti di fader, legacy.
- **Porta MIDI virtuale**: su Mac, CoreMIDI crea porte virtuali senza installare nulla. Su Windows serve un driver (loopMIDI o Windows MIDI Services loopback): quindi NON è zero-install. Da confermare.
- **OSC**: integrato in Reaper, Ardour, e via script in Ableton (AbletonOSC). Non è universale.

## Raccomandazione

Strato universale + adattatori.

1. Un core unico nel plugin che parla con un bridge locale (WebSocket o OSC su localhost), con un solo modello di comandi: transport, tracce, volume, parametri, creare clip e note.
2. Adattatori per DAW, ognuno con auto-installazione del proprio file nella cartella giusta:
   - Reaper: script ReaScript in UserPlugins, poi OSC/WebSocket. Il più automatico.
   - Ableton: copiare AbletonOSC (MIT) in User Library. Un solo passo manuale: selezionare il controller in Preferenze.
   - FL Studio: script `device_<nome>.py` in Settings\Hardware.
   - Cubase: script `.js` in MIDI Remote\Driver Scripts\Local.
   - Bitwig: `.bwextension` precompilato in Extensions.
3. Fallback per le DAW senza API (Logic, Pro Tools, Studio One, Reason, GarageBand): MCU o HUI su porta MIDI virtuale. Su Mac nessun install, su Windows serve un driver: va dichiarato all'utente.
4. Licenze: il plugin è chiuso. Usare solo codice MIT o equivalente (AbletonOSC, uisato). Evitare DrivenByMoss, Ardour e qualsiasi codice GPL linkato.

Restano da verificare: Studio One (nessuna API trovata), Reason, Mac path di FL Studio, path UserPlugins di Reaper, licenze di SWS, DrivenByMoss e dei repo MCP.
