#define AppName "Lulu Code"
#define AppVersion "0.1.0"

[Setup]
AppId={{B4E0C826-67C1-4B9F-9A5E-E8E8F58E5F61}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=neofetch-tech
DefaultDirName={autopf}\Lulu Code
DefaultGroupName=Lulu Code
UninstallDisplayIcon={app}\lulu_code.exe
OutputDir=..\dist
OutputBaseFilename=lulu-code-setup
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=lowest
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Files]
Source: "..\dist\windows\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Lulu Code"; Filename: "{app}\lulu_code.exe"
Name: "{autodesktop}\Lulu Code"; Filename: "{app}\lulu_code.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"

[Run]
Filename: "{app}\lulu_code.exe"; Description: "Launch Lulu Code"; Flags: nowait postinstall skipifsilent