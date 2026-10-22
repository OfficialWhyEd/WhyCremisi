#!/bin/sh
# Disinstalla WhyCremisi. Uso: sh uninstall.sh [--silent] [--dati]
# I dati (impostazioni, memoria, log) restano, a meno di --dati.
VST="${WHYCREMISI_VST3_DIR:-$HOME/.vst3}"
DATI="${XDG_CONFIG_HOME:-$HOME/.config}/WhyCremisi"
APP="${XDG_DATA_HOME:-$HOME/.local/share}/WhyCremisi"
CANC=0
for a in "$@"; do [ "$a" = "--dati" ] && CANC=1; done
rm -rf "$VST/WhyCremisi.vst3"
if [ "$CANC" = 1 ]; then rm -rf "$DATI"; fi
rm -rf "$APP"
echo "WhyCremisi disinstallato."
[ "$CANC" = 1 ] || echo "Le tue impostazioni sono rimaste in $DATI (per cancellarle: --dati)."
