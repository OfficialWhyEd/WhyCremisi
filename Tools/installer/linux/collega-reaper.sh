#!/bin/sh
# Collega Reaper a WhyCremisi: aggiunge in reaper.ini la superficie OSC "WhyCremisi"
# (Reaper ascolta su 8000, risponde al plugin su 9000). Stessa logica di src/daw/ReaperSetup.cpp.
INI="${1:-${XDG_CONFIG_HOME:-$HOME/.config}/REAPER/reaper.ini}"
[ -f "$INI" ] || { echo "Reaper non risulta avviato su questo computer: collegamento saltato."; exit 0; }
if pgrep -x reaper >/dev/null 2>&1; then
  echo "Reaper e' aperto: collegamento saltato. Chiudilo e rilancia questo comando."; exit 0
fi
grep -q 'csurf_[0-9]*=OSC "WhyCremisi"' "$INI" && { echo "Reaper era gia' collegato."; exit 0; }
grep -qi '^\[REAPER\]' "$INI" || { echo "reaper.ini senza sezione [REAPER]: non lo tocco."; exit 0; }
CNT=$(sed -n 's/^csurf_cnt=\([0-9]*\).*/\1/p' "$INI" | head -1)
RIGA="csurf_${CNT:-0}=OSC \"WhyCremisi\" 7 8000 \"127.0.0.1\" 9000 1024 10 \"\""
cp "$INI" "$INI.whycremisi-backup"
TMP=$(mktemp)
if [ -n "$CNT" ]; then
  # aggiorna il contatore e mette la nuova riga subito dopo
  awk -v n=$((CNT+1)) -v r="$RIGA" '
    /^csurf_cnt=/ && !fatto { print "csurf_cnt=" n; print r; fatto=1; next } { print }' "$INI" > "$TMP"
else
  awk -v r="$RIGA" '{ print } /^\[REAPER\]/ && !fatto { print "csurf_cnt=1"; print r; fatto=1 }' "$INI" > "$TMP"
fi
cat "$TMP" > "$INI"; rm -f "$TMP"
echo "Reaper collegato (copia di sicurezza: reaper.ini.whycremisi-backup)."
