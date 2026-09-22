#ifndef MyAppVersion
#define MyAppVersion "0.1.0"
#endif

#define MyAppName "VidView"
#define MyAppExeName "VidView.exe"
#define MyAppProgId "VidView.Video.1"

[Setup]
AppId={{6F4E5A76-3B1B-4E8B-8C91-4C3ECA7DA5A0}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}

AppPublisher=VidView

DefaultDirName={localappdata}\Programs\VidView
DefaultGroupName=VidView

DisableProgramGroupPage=yes

PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible

OutputDir=..\dist
OutputBaseFilename=VidView-Setup-{#MyAppVersion}

SetupIconFile=..\resources\VidView.ico
UninstallDisplayIcon={app}\VidView.exe

Compression=lzma2
SolidCompression=yes
WizardStyle=modern

ChangesAssociations=yes

VersionInfoVersion={#MyAppVersion}
VersionInfoProductName=VidView
VersionInfoDescription=VidView Installer

CloseApplications=yes
RestartApplications=no

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "..\dist\VidView\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\VidView"; Filename: "{app}\VidView.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\VidView"; Filename: "{app}\VidView.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Registry]

; VidView ProgID
Root: HKA; Subkey: "Software\Classes\{#MyAppProgId}"; ValueType: string; ValueName: ""; ValueData: "MP4 Video"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\{#MyAppProgId}\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: """{app}\VidView.exe"",0"
Root: HKA; Subkey: "Software\Classes\{#MyAppProgId}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\VidView.exe"" ""%1"""

; Open With support
Root: HKA; Subkey: "Software\Classes\.mp4\OpenWithProgids"; ValueType: string; ValueName: "{#MyAppProgId}"; ValueData: ""; Flags: uninsdeletevalue

; Register VidView.exe with Explorer
Root: HKA; Subkey: "Software\Classes\Applications\VidView.exe"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "VidView"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Applications\VidView.exe\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: """{app}\VidView.exe"",0"
Root: HKA; Subkey: "Software\Classes\Applications\VidView.exe\SupportedTypes"; ValueType: string; ValueName: ".mp4"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\Applications\VidView.exe\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\VidView.exe"" ""%1"""

; Default Apps registration
Root: HKA; Subkey: "Software\VidView\Capabilities"; ValueType: string; ValueName: "ApplicationName"; ValueData: "VidView"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\VidView\Capabilities"; ValueType: string; ValueName: "ApplicationDescription"; ValueData: "Fast native video inspection and frame analysis."
Root: HKA; Subkey: "Software\VidView\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mp4"; ValueData: "{#MyAppProgId}"

Root: HKA; Subkey: "Software\RegisteredApplications"; ValueType: string; ValueName: "VidView"; ValueData: "Software\VidView\Capabilities"; Flags: uninsdeletevalue

; Explicit context menu entry
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.mp4\shell\VidView"; ValueType: string; ValueName: "MUIVerb"; ValueData: "Open in VidView"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.mp4\shell\VidView"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\VidView.exe"",0"
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.mp4\shell\VidView\command"; ValueType: string; ValueName: ""; ValueData: """{app}\VidView.exe"" ""%1"""

[Run]
Filename: "{app}\VidView.exe"; Description: "Launch VidView"; Flags: nowait postinstall skipifsilent