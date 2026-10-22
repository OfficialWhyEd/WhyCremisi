# Prepara la cartella da cui l'installer Windows prende i file.
# Uso: powershell -File Tools\installer\prepara-windows.ps1 [-Build build-t2]
param([string]$Build = "build-t2")
$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$stage = Join-Path $root "build-installer\stage"
$bundle = Join-Path $root "$Build\WhyCremisiVSTPlugin_artefacts\Release\VST3\WhyCremisi.vst3"
$dist = Join-Path $root "webview-ui\dist"

if (-not (Test-Path (Join-Path $bundle "Contents\x86_64-win\WhyCremisi.vst3"))) { throw "Plugin non compilato: $bundle" }
if (-not (Test-Path (Join-Path $dist "index.html"))) { throw "UI non compilata: esegui npm run build in webview-ui" }

if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force $stage | Out-Null

# Il plugin, con la UI appena compilata al posto di quella vecchia dentro il bundle
$vst = Join-Path $stage "WhyCremisi.vst3"
New-Item -ItemType Directory -Force (Join-Path $vst "Contents\x86_64-win") | Out-Null
New-Item -ItemType Directory -Force (Join-Path $vst "Contents\Resources") | Out-Null
Copy-Item (Join-Path $bundle "Contents\x86_64-win\WhyCremisi.vst3") (Join-Path $vst "Contents\x86_64-win\") -Force
foreach ($f in "moduleinfo.json", "plugin.png") {
    $p = Join-Path $bundle "Contents\Resources\$f"
    if (Test-Path $p) { Copy-Item $p (Join-Path $vst "Contents\Resources\") -Force }
}
Copy-Item $dist (Join-Path $vst "Contents\Resources\webview-ui") -Recurse -Force

# Immagini del programma di installazione (sfondo cremisi, logo)
Add-Type -AssemblyName System.Drawing
function Nuova-Immagine($w, $h, $logoPath, $logoSize, $out, $allinea) {
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = "AntiAlias"; $g.InterpolationMode = "HighQualityBicubic"
    $r = New-Object System.Drawing.Rectangle 0, 0, $w, $h
    $b = New-Object System.Drawing.Drawing2D.LinearGradientBrush $r, ([System.Drawing.Color]::FromArgb(24, 21, 22)), ([System.Drawing.Color]::FromArgb(120, 10, 30)), 70
    $g.FillRectangle($b, $r)
    $logo = [System.Drawing.Image]::FromFile($logoPath)
    $x = [int](($w - $logoSize) / 2)
    $y = if ($allinea -eq "alto") { [int]($h * 0.12) } else { [int](($h - $logoSize) / 2) }
    $g.DrawImage($logo, $x, $y, $logoSize, $logoSize)
    $logo.Dispose(); $g.Dispose()
    $bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Bmp); $bmp.Dispose()
}
$icon256 = Join-Path $root "Research\logo\platform\windows\win-256.png"
Nuova-Immagine 164 314 $icon256 128 (Join-Path $stage "wizard-grande.bmp") "alto"
Nuova-Immagine 55 55 $icon256 44 (Join-Path $stage "wizard-piccola.bmp") "centro"

# AbletonOSC (MIT, ideoforms) alla stessa versione fissata di Tools\installa-abletonosc.cmd.
# Entra nell'installer ma si copia solo se l'utente spunta la casella.
$sha = "0ca68214bd62c9b5cb641ca34006cfd70ba94430"
$tmp = Join-Path $env:TEMP "abletonosc-$sha"
New-Item -ItemType Directory -Force $tmp | Out-Null
$zip = Join-Path $tmp "a.zip"
if (-not (Test-Path $zip)) { Invoke-WebRequest "https://github.com/ideoforms/AbletonOSC/archive/$sha.zip" -OutFile $zip -UseBasicParsing }
Expand-Archive $zip -DestinationPath $tmp -Force
Copy-Item (Join-Path $tmp "AbletonOSC-$sha") (Join-Path $stage "AbletonOSC") -Recurse -Force

Write-Host "Pronto: $stage"
