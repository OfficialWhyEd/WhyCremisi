# Collega Reaper a WhyCremisi: aggiunge in reaper.ini la superficie OSC "WhyCremisi"
# (Reaper ascolta su 8000, risponde al plugin su 9000). Stessa logica di src/daw/ReaperSetup.cpp.
# Non scrive se Reaper e' aperto (lo riscriverebbe alla chiusura) e fa una copia di sicurezza.
param([string]$Ini = (Join-Path $env:APPDATA "REAPER\reaper.ini"))
$ErrorActionPreference = "Stop"

if (-not (Test-Path $Ini)) { Write-Host "Reaper non risulta avviato su questo computer: collegamento saltato."; exit 0 }
if (Get-Process -Name reaper -ErrorAction SilentlyContinue) { Write-Host "Reaper e' aperto: collegamento saltato. Chiudilo e premi Collega Reaper dentro WhyCremisi."; exit 0 }

$testo = [IO.File]::ReadAllText($Ini)
if ($testo -match 'csurf_\d+=OSC "WhyCremisi"') { Write-Host "Reaper era gia' collegato."; exit 0 }

$crlf = $testo.Contains("`r`n")
$righe = New-Object System.Collections.Generic.List[string]
foreach ($r in ($testo -split "`r?`n")) { $righe.Add($r) }
if ($righe.Count -gt 0 -and $righe[$righe.Count - 1] -eq "") { $righe.RemoveAt($righe.Count - 1) }

$inizio = -1; $fine = $righe.Count; $posCnt = -1; $cnt = 0; $ultimo = -1
for ($i = 0; $i -lt $righe.Count; $i++) {
    $r = $righe[$i].Trim()
    if ($r.StartsWith("[") -and $r.EndsWith("]")) {
        if ($r -ieq "[REAPER]") { $inizio = $i } elseif ($inizio -ge 0) { $fine = $i; break }
    }
    if ($inizio -ge 0) {
        if ($r.StartsWith("csurf_cnt=")) { $cnt = [int]($r.Split("=")[1]); $posCnt = $i }
        elseif ($r.StartsWith("csurf_") -and $r.Contains("=") -and -not $r.StartsWith("csurfrate")) { $ultimo = $i }
    }
}
if ($inizio -lt 0) { Write-Host "reaper.ini senza sezione [REAPER]: non lo tocco."; exit 0 }

$riga = "csurf_$cnt=OSC `"WhyCremisi`" 7 8000 `"127.0.0.1`" 9000 1024 10 `"`""
if ($posCnt -ge 0) {
    $righe[$posCnt] = "csurf_cnt=$($cnt + 1)"
    $dopo = [Math]::Max($ultimo, $posCnt)
    $righe.Insert($dopo + 1, $riga)
} else {
    $righe.Insert($fine, $riga)
    $righe.Insert($fine, "csurf_cnt=1")
}
Copy-Item $Ini "$Ini.whycremisi-backup" -Force
$nl = if ($crlf) { "`r`n" } else { "`n" }
[IO.File]::WriteAllText($Ini, ($righe -join $nl) + $nl)
Write-Host "Reaper collegato (copia di sicurezza: reaper.ini.whycremisi-backup)."
