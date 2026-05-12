#!/usr/bin/env bash
# WhyCremisi — PDF build script
# Generates branded PDFs from all research papers

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RESEARCH_DIR="$(dirname "$SCRIPT_DIR")"
CSS="$SCRIPT_DIR/whycremisi.css"
EXPORT_DIR="$RESEARCH_DIR/export"

mkdir -p "$EXPORT_DIR/IT" "$EXPORT_DIR/EN"

PDF_OPTS='{"printBackground":true,"format":"A4","margin":{"top":"0","bottom":"14mm","left":"0","right":"0"}}'

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  WhyCremisi Research Papers — PDF Build"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# IT papers
echo ""
echo "▸ Italian papers"
for f in "$RESEARCH_DIR/IT/papers/"0*.md "$RESEARCH_DIR/IT/00-INDICE.md"; do
  [ -f "$f" ] || continue
  base=$(basename "$f" .md)
  out="$EXPORT_DIR/IT/${base}.pdf"
  echo "  → $base.pdf"
  npx md-to-pdf "$f" \
    --stylesheet "$CSS" \
    --pdf-options "$PDF_OPTS" \
    2>/dev/null
  # move generated pdf
  src_pdf="${f%.md}.pdf"
  [ -f "$src_pdf" ] && mv "$src_pdf" "$out"
done

# EN papers
echo ""
echo "▸ English papers"
for f in "$RESEARCH_DIR/EN/papers/"0*.md "$RESEARCH_DIR/EN/00-INDEX.md"; do
  [ -f "$f" ] || continue
  base=$(basename "$f" .md)
  out="$EXPORT_DIR/EN/${base}.pdf"
  echo "  → $base.pdf"
  npx md-to-pdf "$f" \
    --stylesheet "$CSS" \
    --pdf-options "$PDF_OPTS" \
    2>/dev/null
  src_pdf="${f%.md}.pdf"
  [ -f "$src_pdf" ] && mv "$src_pdf" "$out"
done

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  Done. PDFs in Research/export/"
ls "$EXPORT_DIR/IT/" | wc -l | xargs -I{} echo "  IT: {} files"
ls "$EXPORT_DIR/EN/" | wc -l | xargs -I{} echo "  EN: {} files"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
