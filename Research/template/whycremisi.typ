// WhyCremisi Research Papers — Typst Template
// Brand: Cremisi #DC143C · Amber #FFB000 · Deep Black #0d0d0d

#let cremisi = rgb("#DC143C")
#let amber   = rgb("#FFB000")
#let black   = rgb("#0d0d0d")
#let dark    = rgb("#1a1a1a")
#let mid     = rgb("#2a2a2a")
#let light   = rgb("#e0e0e0")
#let muted   = rgb("#888888")
#let white   = rgb("#ffffff")

#let paper(
  title: "",
  subtitle: "",
  number: "",
  lang: "EN",
  body
) = {

  set document(title: title)
  set page(
    paper: "a4",
    margin: (top: 0mm, bottom: 20mm, left: 0mm, right: 0mm),
    background: rect(fill: black, width: 100%, height: 100%),
    footer: context {
      set text(fill: muted, size: 7pt, font: "JetBrains Mono")
      pad(x: 20mm, y: 4mm)[
        #grid(
          columns: (1fr, 1fr, 1fr),
          align(left)[WHYCREMISI · AI MIX ASSISTANT],
          align(center)[#title.slice(0, calc.min(40, title.len()))],
          align(right)[#counter(page).display() / #counter(page).final().first()]
        )
      ]
    }
  )

  // ── COVER HEADER ──────────────────────────────────────────────
  block(
    width: 100%,
    fill: cremisi,
    inset: 0pt,
  )[
    pad(x: 20mm, top: 18mm, bottom: 14mm)[
      // Paper number badge
      text(fill: white, size: 8pt, weight: "bold", font: "JetBrains Mono")[
        PAPER N.#number · #lang
      ]
      v(6pt)
      // Main title
      text(fill: white, size: 26pt, weight: "black")[
        #title
      ]
      v(4pt)
      // Subtitle
      text(fill: rgba(255,255,255,0.75), size: 12pt, style: "italic")[
        #subtitle
      ]
    ]
  ]

  // ── AMBER ACCENT LINE ─────────────────────────────────────────
  block(width: 100%, height: 3pt, fill: amber)

  // ── BODY ─────────────────────────────────────────────────────
  pad(x: 20mm, top: 12mm)[
    set text(fill: light, size: 10pt, font: "Inter", lang: lang.slice(0,2).to-lowercase())
    set par(justify: true, leading: 1.5em)
    set heading(numbering: none)

    show heading.where(level: 1): h => {
      v(14pt)
      block(
        width: 100%,
        fill: dark,
        inset: (x: 12pt, y: 8pt),
        radius: 3pt,
      )[
        text(fill: cremisi, size: 11pt, weight: "bold", font: "JetBrains Mono")[
          #h.body
        ]
      ]
      v(6pt)
    }

    show heading.where(level: 2): h => {
      v(10pt)
      stack(
        dir: ltr,
        spacing: 8pt,
        rect(width: 2pt, height: 14pt, fill: amber, radius: 1pt),
        text(fill: amber, size: 10pt, weight: "bold")[#h.body]
      )
      v(4pt)
    }

    show heading.where(level: 3): h => {
      v(6pt)
      text(fill: muted, size: 9pt, weight: "bold")[#upper(h.body)]
      v(2pt)
    }

    show raw.where(block: true): r => {
      block(
        width: 100%,
        fill: dark,
        inset: (x: 14pt, y: 10pt),
        radius: 4pt,
        stroke: (left: (thickness: 2pt, paint: cremisi))
      )[
        text(fill: rgb("#d4d4d4"), size: 8pt, font: "JetBrains Mono")[#r]
      ]
    }

    show raw.where(block: false): r => {
      box(
        fill: dark,
        inset: (x: 4pt, y: 2pt),
        radius: 2pt,
      )[
        text(fill: amber, size: 8.5pt, font: "JetBrains Mono")[#r]
      ]
    }

    show table: t => {
      set table(
        fill: (col, row) => if row == 0 { cremisi } else if calc.odd(row) { dark } else { mid },
        stroke: none,
        inset: (x: 10pt, y: 6pt),
      )
      set text(size: 9pt)
      block(width: 100%, radius: 4pt, clip: true)[#t]
    }

    show table.cell.where(y: 0): c => {
      text(fill: white, weight: "bold", font: "JetBrains Mono", size: 8pt)[#upper(c.body)]
    }

    show link: l => {
      text(fill: amber, style: "italic")[#l]
    }

    show strong: s => text(fill: white, weight: "bold")[#s]

    show emph: e => text(fill: amber, style: "italic")[#e]

    show list: l => {
      set list(marker: (
        text(fill: cremisi)[▸],
        text(fill: amber)[◦],
        text(fill: muted)[–],
      ))
      l
    }

    body
  ]
}
