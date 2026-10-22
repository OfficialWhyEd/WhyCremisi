; Installer Windows di WhyCremisi (Inno Setup 6).
; Compilare con:  ISCC.exe Tools\installer\WhyCremisi.iss   (dopo prepara-windows.ps1)
; Aggiornamento automatico: l'app lancia questo stesso installer con /SILENT /FROMUPDATE
; quando la DAW si chiude. Un aggiornamento non fa domande e non tocca le scelte dell'utente.

#define NomeApp "WhyCremisi"
#define Versione Trim(FileRead(FileOpen(SourcePath + "\..\..\VERSION")))
#define Radice SourcePath + "\..\.."

[Setup]
; Questo GUID non si cambia mai: e' l'identita' del programma (aggiornamenti e disinstallazione)
#ifdef Prova
; Versione di collaudo: senza amministratore e senza toccare il plugin vero (ISCC /DProva=1)
AppId={{8F3A5C2E-6B1D-4E7A-9C40-2D5B7E1A93F7}
PrivilegesRequired=lowest
DefaultDirName={localappdata}\WhyCremisiProva
OutputBaseFilename=WhyCremisi-Prova
#else
AppId={{8F3A5C2E-6B1D-4E7A-9C40-2D5B7E1A93F6}
PrivilegesRequired=admin
DefaultDirName={autopf}\WhyCremisi
OutputBaseFilename=WhyCremisi-Setup-{#Versione}
#endif
UsedUserAreasWarning=no
AppName={#NomeApp}
AppVersion={#Versione}
AppVerName={#NomeApp} {#Versione}
AppPublisher=WhyEd
AppPublisherURL=https://github.com/OfficialWhyEd/WhyCremisi
AppSupportURL=https://github.com/OfficialWhyEd/WhyCremisi/issues
VersionInfoVersion={#Versione}
VersionInfoProductName={#NomeApp}
DisableDirPage=yes
DefaultGroupName={#NomeApp}
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#Radice}\build-installer\out
SetupIconFile={#Radice}\Research\logo\platform\windows\WhyCremisi.ico
UninstallDisplayIcon={app}\WhyCremisi.ico
WizardImageFile={#Radice}\build-installer\stage\wizard-grande.bmp
WizardSmallImageFile={#Radice}\build-installer\stage\wizard-piccola.bmp
WizardStyle=modern
Compression=lzma2/ultra
SolidCompression=yes
CloseApplications=no
RestartIfNeededByRun=no
UsePreviousLanguage=no
ShowLanguageDialog=auto
UninstallDisplayName={#NomeApp}

[Languages]
Name: "it"; MessagesFile: "compiler:Languages\Italian.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
it.TaskReaper=Collega Reaper (aggiunge la superficie di controllo OSC; Reaper deve essere chiuso)
en.TaskReaper=Connect Reaper (adds the OSC control surface; Reaper must be closed)
it.TaskAbleton=Installa lo script AbletonOSC per Ableton Live (codice di terzi, licenza MIT)
en.TaskAbleton=Install the AbletonOSC script for Ableton Live (third-party code, MIT license)
it.TaskAuto=Cerca e installa da solo gli aggiornamenti di WhyCremisi (quando chiudi la DAW)
en.TaskAuto=Check for and install WhyCremisi updates automatically (when you close the DAW)
it.GruppoDaw=Collegamento alla DAW:
en.GruppoDaw=DAW connection:
it.GruppoAgg=Aggiornamenti:
en.GruppoAgg=Updates:
it.CancellaDati=Vuoi cancellare anche le tue impostazioni, la memoria e i log di WhyCremisi?%n%nSe pensi di reinstallarlo, scegli No.
en.CancellaDati=Do you also want to delete your WhyCremisi settings, memory and logs?%n%nIf you plan to reinstall it, choose No.

[Tasks]
Name: "reaper"; Description: "{cm:TaskReaper}"; GroupDescription: "{cm:GruppoDaw}"
Name: "abletonosc"; Description: "{cm:TaskAbleton}"; GroupDescription: "{cm:GruppoDaw}"; Flags: unchecked
Name: "autoupdate"; Description: "{cm:TaskAuto}"; GroupDescription: "{cm:GruppoAgg}"

[Files]
; Il plugin. Se una DAW lo tiene aperto il file si sostituisce al riavvio, senza perdere nulla.
Source: "{#Radice}\build-installer\stage\WhyCremisi.vst3\*"; DestDir: "{code:CartellaVst3}\WhyCremisi.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs restartreplace uninsrestartdelete
Source: "{#Radice}\Research\logo\platform\windows\WhyCremisi.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Radice}\Tools\installer\collega-reaper.ps1"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Radice}\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Radice}\VERSION"; DestDir: "{app}"; Flags: ignoreversion
; AbletonOSC (MIT): copiato solo se spunti la casella
Source: "{#Radice}\build-installer\stage\AbletonOSC\*"; DestDir: "{userdocs}\Ableton\User Library\Remote Scripts\AbletonOSC"; Flags: ignoreversion recursesubdirs createallsubdirs; Tasks: abletonosc

[Icons]
Name: "{autoprograms}\WhyCremisi\Disinstalla WhyCremisi"; Filename: "{uninstallexe}"; IconFilename: "{app}\WhyCremisi.ico"

[Run]
Filename: "powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\collega-reaper.ps1"""; Flags: runhidden; Tasks: reaper

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Code]
function FromUpdate: Boolean;
var
  i: Integer;
begin
  Result := False;
  for i := 1 to ParamCount do
    if CompareText(ParamStr(i), '/FROMUPDATE') = 0 then Result := True;
end;

function CartellaVst3(Param: String): String;
begin
#ifdef Prova
  Result := ExpandConstant('{app}') + '\vst3-prova';
#else
  Result := ExpandConstant('{commoncf64}') + '\VST3';
#endif
end;

function CartellaDati: String;
begin
  Result := ExpandConstant('{userappdata}\WhyCremisi');
end;

// Scrive il flag degli aggiornamenti solo alla prima installazione: un aggiornamento non cambia le scelte.
procedure CurStepChanged(CurStep: TSetupStep);
var
  File: String;
  Valore: String;
begin
  if CurStep = ssPostInstall then
  begin
    File := CartellaDati + '\aggiornamenti.json';
    if not FileExists(File) then
    begin
      ForceDirectories(CartellaDati);
      if WizardIsTaskSelected('autoupdate') then Valore := 'true' else Valore := 'false';
      SaveStringToFile(File, '{"auto":' + Valore + ',"channel":"beta"}', False);
    end;
  end;
end;

// Non mostra le caselle durante un aggiornamento automatico (e' silenzioso, ma per sicurezza)
function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := FromUpdate and (PageID = wpSelectTasks);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
  begin
    // Disinstallazione silenziosa: i dati dell'utente restano sempre
    if (not UninstallSilent) and DirExists(CartellaDati) then
      if MsgBox(CustomMessage('CancellaDati'), mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES then
        DelTree(CartellaDati, True, True, True);
  end;
end;
