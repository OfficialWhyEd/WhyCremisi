@echo off
setlocal
title Installa AbletonOSC per WhyCremisi

REM ---------------------------------------------------------------
REM  WhyCremisi parla con Ableton Live via OSC, ma Live da solo non
REM  espone nessuna porta OSC: serve AbletonOSC, un Remote Script
REM  di terze parti (MIT, github.com/ideoforms/AbletonOSC).
REM
REM  Questo script lo scarica e lo mette nella User Library, che non
REM  richiede permessi di amministratore. Scarica ed esegue codice
REM  di terzi dentro il tuo DAW: lancialo solo se ti sta bene.
REM ---------------------------------------------------------------

set "DEST=%USERPROFILE%\Documents\Ableton\User Library\Remote Scripts"
set "TMPZIP=%TEMP%\abletonosc.zip"
set "TMPDIR=%TEMP%\abletonosc_extract"

echo.
echo   AbletonOSC verra' installato in:
echo   %DEST%\AbletonOSC
echo.

if exist "%DEST%\AbletonOSC" (
    echo   Risulta gia' installato.
    echo   Per reinstallarlo cancella prima quella cartella.
    echo.
    pause
    exit /b 0
)

echo   Scarico da github.com/ideoforms/AbletonOSC ...
powershell -NoProfile -Command ^
  "try { Invoke-WebRequest -Uri 'https://github.com/ideoforms/AbletonOSC/archive/0ca68214bd62c9b5cb641ca34006cfd70ba94430.zip' -OutFile '%TMPZIP%' -UseBasicParsing; exit 0 } catch { Write-Host $_.Exception.Message; exit 1 }"
if %errorlevel% neq 0 (
    echo   [ERRORE] Download non riuscito. Controlla la connessione.
    pause
    exit /b 1
)

echo   Estraggo...
if exist "%TMPDIR%" rmdir /s /q "%TMPDIR%"
powershell -NoProfile -Command "Expand-Archive -Path '%TMPZIP%' -DestinationPath '%TMPDIR%' -Force"

if not exist "%DEST%" mkdir "%DEST%"
xcopy "%TMPDIR%\AbletonOSC-0ca68214bd62c9b5cb641ca34006cfd70ba94430" "%DEST%\AbletonOSC\" /E /I /Y /Q >nul

del "%TMPZIP%" >nul 2>&1
rmdir /s /q "%TMPDIR%" >nul 2>&1

if not exist "%DEST%\AbletonOSC\__init__.py" (
    echo   [ERRORE] L'installazione non sembra completa.
    pause
    exit /b 1
)

echo.
echo   Installato.
echo.
echo   Ora, in Ableton Live:
echo     1. Preferenze - Link/Tempo/MIDI
echo     2. In Control Surface scegli "AbletonOSC"
echo     3. Riavvia Live
echo.
echo   Se compare nella lista, Live ascolta sulla porta 11000 e
echo   risponde sulla 11001. Nel plugin scegli il preset Ableton.
echo.
pause
