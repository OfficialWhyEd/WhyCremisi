# Test WhyCremisi su Reaper (Linux)

Ramo: `feat/conferma-reale-comandi-daw`. Ultimo test dal vivo: Ableton Live 11 su Windows (volume tracce a -3 e -9 dB, confermato dai fader). Su Linux e su Reaper NON è mai stato provato. Serve a questo il tuo test.

## 1. Compilare (Linux)

```
sudo apt install build-essential cmake ninja-build pkg-config libgtk-3-dev libwebkit2gtk-4.1-dev libasound2-dev libcurl4-openssl-dev libfreetype-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev nodejs npm
git clone https://github.com/OfficialWhyEd/WhyCremisi.git && cd WhyCremisi
git checkout feat/conferma-reale-comandi-daw
(cd webview-ui && npm install && npm run build)
export JUCE_ROOT=/percorso/di/JUCE        # JUCE 8, come nel CMakeLists
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target WhyCremisiVSTPlugin_VST3
cp -r build/WhyCremisiVSTPlugin_artefacts/Release/VST3/WhyCremisi.vst3 ~/.vst3/
```

Prima la UI (`npm run build`), poi il plugin: la UI viene copiata dentro il VST3. Se il plugin si apre vuoto, manca `webview-ui/dist`.

## 2. Reaper: collegare il plugin (una volta)

1. Reaper > Opzioni > Preferenze > Control/OSC/web > Add > OSC (Open Sound Control).
2. Pattern config: Default. Mode: Local port.
3. Local listen port: **8000** (il plugin scrive qui). Local IP e device: 127.0.0.1, device port **9000** (il plugin ascolta qui).
4. Mettere WhyCremisi sul Master.
5. Il plugin ha il preset "reaper" (invia a 8000, ascolta su 9000). Nella UI non c'è ancora un menu per sceglierlo: se non si collega da solo, segnalamelo, lo aggancio subito. Se le porte di Reaper sono diverse da queste, scrivimi quali sono.

## 3. Cosa provare

- "Fai partire", "ferma", "metti il tempo a 100".
- "Metti la traccia 2 a -6 dB", poi "abbassa di 3 dB la traccia 2" (deve fare -9).
- "Muta la traccia 1", "solo sulla 3", "pan della 2 a sinistra".
- L'indicatore "DAW SYNC" deve essere verde solo se Reaper risponde.

## 4. Cosa mi serve da te

- Cosa si muove davvero in Reaper e cosa no.
- Il testo esatto delle risposte del plugin, se sono chiare.
- Il log: `~/.config/WhyCremisi/whycremisi.log` (e `session.json` nella stessa cartella).
- Errori di compilazione, copia e incolla.

Nota onesta: il lato Reaper manda indirizzi OSC standard di Reaper (`/track/N/volume` ecc.) ma la lettura dei valori da Reaper (conferma reale) non è collaudata come per Ableton. Se qualcosa dice "confermato" senza che Reaper si muova, è un bug da segnalare.
