# WhyCremisi su Reaper: guida per Carlo e per i suoi agenti

Versione 0.2.0, canale beta. Leggi tutto prima di partire. Gli agenti: seguite i passi in ordine e restituite quello che è scritto in fondo.

## Cosa è provato e cosa no (onestà)

- Provato: Ableton Live 11 su Windows (volume tracce confermato dai fader).
- Provato: installer Windows (installa, aggiorna, disinstalla) e script di collegamento Reaper su copie di `reaper.ini`.
- NON provato: Reaper dal vivo, Linux dal vivo, l'aggiornamento automatico contro una release vera pubblicata.
- Se qualcosa dice "confermato" senza che Reaper si muova, è un bug: segnalalo.

## 1. Installare

Windows:
1. Scarica `WhyCremisi-Setup-<versione>.exe` dalla release "beta" su https://github.com/OfficialWhyEd/WhyCremisi/releases/tag/beta
2. Avvialo. Lascia spuntate "Collega Reaper" e "Aggiornamenti automatici". Chiudi Reaper prima.

Linux:
1. Scarica `WhyCremisi-linux-<versione>.tar.gz` dalla stessa release (se non c'è ancora, vedi "Se il pacchetto non c'è").
2. `tar -xzf WhyCremisi-linux-*.tar.gz && sh WhyCremisi-linux-*/install.sh`
3. Opzioni: `--no-reaper` (non toccare Reaper), `--no-auto` (aggiornamenti spenti).

Il plugin finisce in `~/.vst3` (Linux) o `C:\Program Files\Common Files\VST3` (Windows). Non serve altro.

Se il pacchetto Linux non c'è: compila come prima (`apt install` delle librerie JUCE, `npm run build` in `webview-ui`, cmake+ninja, `JUCE_ROOT` verso JUCE 8), poi `sh Tools/installer/pacchetto-linux.sh build` e installa il tar.gz creato.

## 2. Collegare Reaper

Lo fa l'installer, con Reaper chiuso. Se era aperto: chiudilo e rilancia `sh ~/.local/share/WhyCremisi/collega-reaper.sh` (Linux) o riesegui l'installer (Windows).
Aggiunge a `reaper.ini` la superficie OSC "WhyCremisi": Reaper ascolta su 8000, il plugin ascolta su 9000. Fa prima una copia: `reaper.ini.whycremisi-backup`.
Controllo: Reaper > Opzioni > Preferenze > Control/OSC/web deve mostrare "WhyCremisi". Poi metti WhyCremisi sul Master.

## 3. Aggiornamenti

- All'apertura il plugin controlla se c'è una versione nuova e te lo dice in un banner. Installa quando chiudi la DAW. Non riscarichi tutto a mano.
- Spegnerli: nel banner togli la spunta "Aggiornamenti automatici", oppure metti `{"auto":false,"channel":"beta"}` in `%APPDATA%\WhyCremisi\aggiornamenti.json` (Windows) o `~/.config/WhyCremisi/aggiornamenti.json` (Linux), oppure la variabile `WHYCREMISI_NO_UPDATE=1`.
- "Controlla ora" nel banner forza il controllo.
- Gli aggiornamenti non cambiano le tue scelte e non fanno domande.

## 4. Disinstallare

- Windows: Impostazioni > App > WhyCremisi, oppure menu Start > WhyCremisi > Disinstalla. Chiede se cancellare anche impostazioni e memoria (di default No).
- Linux: `sh ~/.local/share/WhyCremisi/uninstall.sh` (aggiungi `--dati` per cancellare anche impostazioni e memoria).

## 5. Cosa provare

- "Fai partire", "ferma", "metti il tempo a 100".
- "Metti la traccia 2 a -6 dB", poi "abbassa di 3 dB la traccia 2" (deve fare -9).
- "Muta la traccia 1", "solo sulla 3", "pan della 2 a sinistra".
- L'indicatore "DAW SYNC" è verde solo se Reaper risponde davvero.
- Chiudi e riapri Reaper: il plugin si ricorda la sessione?
- Prova l'accesso (Google o email e password) se il servizio è attivo; se dice che non è ancora attivo, è normale in questa beta.

## 6. Cosa restituire

- Cosa si muove davvero in Reaper e cosa no.
- Il testo esatto delle risposte del plugin quando non sono chiare.
- Il log: `%APPDATA%\WhyCremisi\whycremisi.log` (Windows) o `~/.config/WhyCremisi/whycremisi.log` (Linux), più `session.json` nella stessa cartella.
- Versione installata (`cat ~/.local/share/WhyCremisi/VERSION`).
- Errori di installazione o compilazione, copiati interi.
- Se l'aggiornamento automatico è partito e se la versione è cambiata.
