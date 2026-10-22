#!/bin/sh
# Crea WhyCremisi-linux-<versione>.tar.gz dal plugin compilato su Linux.
# Uso: sh Tools/installer/pacchetto-linux.sh [cartella-build]   (default: build)
set -e
RADICE=$(cd "$(dirname "$0")/../.." && pwd)
BUILD="${1:-build}"
VER=$(cat "$RADICE/VERSION")
BUNDLE="$RADICE/$BUILD/WhyCremisiVSTPlugin_artefacts/Release/VST3/WhyCremisi.vst3"
[ -d "$BUNDLE" ] || { echo "Plugin non compilato: $BUNDLE" >&2; exit 1; }
[ -f "$RADICE/webview-ui/dist/index.html" ] || { echo "UI non compilata: npm run build in webview-ui" >&2; exit 1; }
OUT="$RADICE/build-installer/out"; PKG="$RADICE/build-installer/linux/WhyCremisi-linux-$VER"
rm -rf "$PKG"; mkdir -p "$PKG" "$OUT"
cp -r "$BUNDLE" "$PKG/WhyCremisi.vst3"
# UI fresca al posto di quella eventualmente vecchia dentro il bundle
rm -rf "$PKG/WhyCremisi.vst3/Contents/Resources/webview-ui"
mkdir -p "$PKG/WhyCremisi.vst3/Contents/Resources"
cp -r "$RADICE/webview-ui/dist" "$PKG/WhyCremisi.vst3/Contents/Resources/webview-ui"
cp "$RADICE"/Tools/installer/linux/*.sh "$PKG/"
cp "$RADICE/VERSION" "$PKG/"
cp "$RADICE/Research/logo/platform/linux/linux-256.png" "$PKG/WhyCremisi.png" 2>/dev/null || true
tar -czf "$OUT/WhyCremisi-linux-$VER.tar.gz" -C "$RADICE/build-installer/linux" "WhyCremisi-linux-$VER"
sha256sum "$OUT/WhyCremisi-linux-$VER.tar.gz"
