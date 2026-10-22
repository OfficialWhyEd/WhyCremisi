#!/bin/sh
# Installer Linux di WhyCremisi. Uso: sh install.sh [--silent] [--no-reaper] [--no-auto] [--abletonosc]
# Un aggiornamento automatico lo lancia con --silent: non fa domande e non cambia le scelte dell'utente.
set -e
QUI=$(cd "$(dirname "$0")" && pwd)
SILENT=0; REAPER=1; AUTO=1; ABLETON=0
for a in "$@"; do
  case "$a" in
    --silent) SILENT=1 ;;
    --no-reaper) REAPER=0 ;;
    --no-auto) AUTO=0 ;;
    --abletonosc) ABLETON=1 ;;
  esac
done
VER=$(cat "$QUI/VERSION" 2>/dev/null || echo "?")
VST="${WHYCREMISI_VST3_DIR:-$HOME/.vst3}"
DATI="${XDG_CONFIG_HOME:-$HOME/.config}/WhyCremisi"
APP="${XDG_DATA_HOME:-$HOME/.local/share}/WhyCremisi"

[ "$SILENT" = 1 ] || echo "WhyCremisi $VER: installazione in $VST"
[ -d "$QUI/WhyCremisi.vst3" ] || { echo "Manca WhyCremisi.vst3 nel pacchetto." >&2; exit 1; }

mkdir -p "$VST" "$APP" "$DATI"
rm -rf "$VST/WhyCremisi.vst3"
cp -r "$QUI/WhyCremisi.vst3" "$VST/"
cp "$QUI/uninstall.sh" "$QUI/collega-reaper.sh" "$QUI/VERSION" "$APP/"
[ -f "$QUI/WhyCremisi.png" ] && cp "$QUI/WhyCremisi.png" "$APP/"
chmod +x "$APP/uninstall.sh" "$APP/collega-reaper.sh"

# Flag aggiornamenti: si scrive solo alla prima installazione
if [ ! -f "$DATI/aggiornamenti.json" ]; then
  if [ "$AUTO" = 1 ]; then V=true; else V=false; fi
  printf '{"auto":%s,"channel":"beta"}' "$V" > "$DATI/aggiornamenti.json"
fi

# Reaper: durante un aggiornamento si ricontrolla, ma solo se e' gia' collegato non cambia nulla
if [ "$REAPER" = 1 ]; then sh "$APP/collega-reaper.sh" || true; fi

if [ "$ABLETON" = 1 ] && [ -d "$QUI/AbletonOSC" ]; then
  # Ableton non gira su Linux: la casella esiste per chi usa Wine/Bitwig no. Si ignora.
  :
fi

[ "$SILENT" = 1 ] || { echo "Fatto. Per toglierlo: sh $APP/uninstall.sh"; echo "Aggiornamenti automatici: $(cat "$DATI/aggiornamenti.json")"; }
