; Inno Setup script for Riftwalker
; Compile with: iscc installer\Riftwalker.iss
; Result: installer\Output\Riftwalker-v1.0.0-Setup.exe
;
; Inno Setup 6 download: https://jrsoftware.org/isdl.php
;
; Pre-requisite: a Release build must exist at build\Release\.

#define AppName       "Riftwalker"
#define AppVersion    "1.0.0"
#define AppPublisher  "Webmsaster"
#define AppURL        "https://github.com/Webmsaster/Riftwalker"
#define AppExeName    "Riftwalker.exe"

[Setup]
AppId={{A8E2C1F4-9D7B-4E3A-B0C5-7F8E2A1D9C3E}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
AllowNoIcons=yes
DisableProgramGroupPage=auto
OutputDir=Output
OutputBaseFilename=Riftwalker-v{#AppVersion}-Setup
SetupIconFile=
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
LicenseFile=
InfoBeforeFile=
UninstallDisplayName={#AppName} {#AppVersion}
UninstallDisplayIcon={app}\{#AppExeName}
MinVersion=10.0
DisableWelcomePage=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "german";  MessagesFile: "compiler:Languages\German.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Main executable
Source: "..\build\Release\Riftwalker.exe"; DestDir: "{app}"; Flags: ignoreversion

; Bundled DLLs (SDL2 family + dependencies; copy any present)
Source: "..\build\Release\*.dll"; DestDir: "{app}"; Flags: ignoreversion

; Game assets (textures, sounds, music, fonts)
Source: "..\build\Release\assets\*"; DestDir: "{app}\assets"; Flags: ignoreversion recursesubdirs createallsubdirs

; Documentation
Source: "..\README.md";    DestDir: "{app}"; Flags: ignoreversion
Source: "..\CHANGELOG.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}";              Filename: "{app}\{#AppExeName}"
Name: "{group}\Read Me";                 Filename: "{app}\README.md"
Name: "{group}\Changelog";               Filename: "{app}\CHANGELOG.md"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}";        Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; User-generated save files in the app directory — remove on uninstall.
Type: files; Name: "{app}\riftwalker_save.dat"
Type: files; Name: "{app}\riftwalker_save.dat.bak"
Type: files; Name: "{app}\riftwalker_settings.cfg"
Type: files; Name: "{app}\riftwalker_settings.cfg.bak"
Type: files; Name: "{app}\riftwalker_achievements.dat"
Type: files; Name: "{app}\riftwalker_achievements.dat.bak"
Type: files; Name: "{app}\riftwalker_lore.dat"
Type: files; Name: "{app}\riftwalker_lore.dat.bak"
Type: files; Name: "{app}\riftwalker_bindings.cfg"
Type: files; Name: "{app}\riftwalker_bindings.cfg.bak"
Type: files; Name: "{app}\bestiary_save.dat"
Type: files; Name: "{app}\bestiary_save.dat.bak"
Type: files; Name: "{app}\ascension_save.dat"
Type: files; Name: "{app}\ascension_save.dat.bak"
Type: files; Name: "{app}\riftwalker_challenges.dat"
Type: files; Name: "{app}\riftwalker_challenges.dat.bak"
Type: files; Name: "{app}\riftwalker_crash.txt"
Type: dirifempty; Name: "{app}\screenshots"
