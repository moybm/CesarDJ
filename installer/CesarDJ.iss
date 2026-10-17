; Instalador de CesarDJ (Inno Setup 6)
; Se genera con crear_instalador.bat, que pasa la versión desde CMakeLists.txt.
;
; Actualizaciones: el AppId no cambia nunca, así que instalar una versión nueva encima
; sustituye el programa y conserva la biblioteca y los ajustes (%APPDATA%\CesarDJ).

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

#define AppName "CesarDJ"
#define AppExe  "CesarDJ.exe"

[Setup]
AppId={{8E2C7F2A-5B1D-4C3E-9A6F-CE5A2D1B7F30}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppName}
VersionInfoVersion={#AppVersion}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
; Sin administrador por defecto (se instala para el usuario); el asistente permite elegir "para todos"
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir=..\dist
OutputBaseFilename=CesarDJ-Setup-{#AppVersion}
SetupIconFile=..\build\CesarDJ_artefacts\JuceLibraryCode\icon.ico
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName}
WizardStyle=modern
Compression=lzma2/max
SolidCompression=yes
; Si CesarDJ está abierto durante una actualización, se cierra antes de reemplazarlo
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "..\build\CesarDJ_artefacts\Release\{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LEEME.txt"; DestDir: "{app}"; Flags: ignoreversion isreadme
; Modelo de IA para los stems (Demucs v4, ~84 MB): funciona sin internet
Source: "..\build\CesarDJ_artefacts\Release\models\ggml-model-htdemucs-4s-f16.bin"; DestDir: "{app}\models"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

; Al desinstalar NO se borran los datos del usuario (%APPDATA%\CesarDJ): biblioteca, listas, ajustes y MIDI
; se conservan por si se vuelve a instalar.
