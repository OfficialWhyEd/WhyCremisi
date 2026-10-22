# Prepara (e con -Pubblica carica) la release "beta" di WhyCremisi: installer + latest.json.
# Uso:  powershell -File Tools\pubblica-beta.ps1            (prepara soltanto, non carica nulla)
#       powershell -File Tools\pubblica-beta.ps1 -Pubblica  (carica su GitHub: serve l'OK di Whyed)
# Prima: npm run build in webview-ui e plugin compilato in build-t2.
param([switch]$Pubblica, [string]$Build = "build-t2", [string]$Note = "")
$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$ver = (Get-Content (Join-Path $root "VERSION") -Raw).Trim()
$out = Join-Path $root "build-installer\out"
$iscc = "C:\Users\$env:USERNAME\AppData\Local\Programs\Inno Setup 6\ISCC.exe"
if (-not (Test-Path $iscc)) { $iscc = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" }
if (-not (Test-Path $iscc)) { throw "Inno Setup 6 non trovato (winget install JRSoftware.InnoSetup)" }

& powershell -NoProfile -File (Join-Path $root "Tools\installer\prepara-windows.ps1") -Build $Build
if ($LASTEXITCODE) { throw "prepara-windows fallito" }
& $iscc (Join-Path $root "Tools\installer\WhyCremisi.iss")
if ($LASTEXITCODE) { throw "ISCC fallito" }

$exe = Join-Path $out "WhyCremisi-Setup-$ver.exe"
$hash = (Get-FileHash $exe -Algorithm SHA256).Hash.ToLower()
$base = "https://github.com/OfficialWhyEd/WhyCremisi/releases/download/beta"
$man = [ordered]@{ version = $ver; notes = $Note
    windows = [ordered]@{ url = "$base/WhyCremisi-Setup-$ver.exe"; sha256 = $hash; size = (Get-Item $exe).Length } }
# Linux: si aggiunge solo se il pacchetto e' stato fatto (sh Tools/installer/pacchetto-linux.sh su Linux)
$tgz = Join-Path $out "WhyCremisi-linux-$ver.tar.gz"
if (Test-Path $tgz) {
    $man.linux = [ordered]@{ url = "$base/WhyCremisi-linux-$ver.tar.gz"; sha256 = (Get-FileHash $tgz -Algorithm SHA256).Hash.ToLower(); size = (Get-Item $tgz).Length }
}
$json = Join-Path $out "latest.json"
[IO.File]::WriteAllText($json, ($man | ConvertTo-Json -Depth 4), (New-Object Text.UTF8Encoding $false))
# Firma Ed25519 del manifest: la chiave privata resta sul PC di Whyed, mai su GitHub
$sig = "$json.sig"
if (Test-Path $sig) { Remove-Item $sig -Force }
& node (Join-Path $root "Toolsirmairma.mjs") $json
$firmato = Test-Path $sig
if (-not $firmato) { Write-Host "ATTENZIONE: latest.json NON firmato (manca la chiave: node Toolsirmagenera-chiave.mjs)." }
Write-Host "Pronto in $out : $(Split-Path $exe -Leaf), latest.json (windows sha256 $hash)"
if (Test-Path $tgz) { Write-Host "Linux incluso." } else { Write-Host "Linux NON incluso: i Linux non vedranno questo aggiornamento." }

if (-not $Pubblica) { Write-Host "Non ho caricato nulla (manca -Pubblica)."; exit 0 }
$repo = "OfficialWhyEd/WhyCremisi"
& gh release view beta --repo $repo *> $null
if ($LASTEXITCODE) { & gh release create beta --repo $repo --prerelease --title "WhyCremisi beta" --notes "Canale beta: gli aggiornamenti automatici leggono latest.json da qui." }
if (-not $firmato) { throw "Senza firma non pubblico: gli aggiornamenti verrebbero rifiutati. Crea la chiave con genera-chiave.mjs e ricompila il plugin." }
$files = @($exe, $json, $sig); if (Test-Path $tgz) { $files += $tgz }
& gh release upload beta @files --repo $repo --clobber
if ($LASTEXITCODE) { throw "caricamento fallito" }
Write-Host "Pubblicato su https://github.com/$repo/releases/tag/beta"
