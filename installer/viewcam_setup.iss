; ViewCam Studio — Windows Installer Script (Inno Setup 6)
; Build: iscc installer\viewcam_setup.iss  (from the project root)

#define AppName        "ViewCam Studio"
#define AppVersion     "1.0.0"
#define AppPublisher   "Sleak Software"
#define AppURL         "https://viewcam.tech"
#define AppExeName     "ViewCam.exe"
#define AppId          "tech.viewcam.studio"
#define SourceDir      "..\bin"
#define OutputDir      "..\installer\output"

[Setup]
AppId={{{#AppId}}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
AllowNoIcons=yes
OutputDir={#OutputDir}
OutputBaseFilename=ViewCamStudio-{#AppVersion}-Setup
SetupIconFile=..\resources\images\viewcam.ico
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#AppExeName}
UninstallDisplayName={#AppName}
VersionInfoVersion={#AppVersion}
VersionInfoCompany={#AppPublisher}
VersionInfoDescription={#AppName} Installer

[Languages]
Name: "english";   MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
; All application files from dist\ (exe, DLLs, Qt runtime, plugins, rcc bundle)
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}";                   Filename: "{app}\{#AppExeName}"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}";             Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Registry]
; App registration for Add/Remove Programs (supplementary to the auto-generated entry)
Root: HKLM; Subkey: "Software\SleakSoftware\ViewCamStudio"; ValueType: string; ValueName: "InstallDir"; ValueData: "{app}"; Flags: uninsdeletekey
Root: HKLM; Subkey: "Software\SleakSoftware\ViewCamStudio"; ValueType: string; ValueName: "Version";    ValueData: "{#AppVersion}"

[Run]
; Register DirectShow virtual camera filter (all Windows versions)
Filename: "{sys}\regsvr32.exe"; Parameters: "/s ""{app}\ViewCamFilter.dll""";   StatusMsg: "Registering virtual camera (DirectShow)...";      Flags: runhidden

; Register MF virtual camera source (Windows 11 virtual camera API)
Filename: "{sys}\regsvr32.exe"; Parameters: "/s ""{app}\ViewCamMFSource.dll"""; StatusMsg: "Registering virtual camera (Media Foundation)..."; Flags: runhidden

; Offer to launch after install
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
; Unregister both camera components before files are removed
Filename: "{sys}\regsvr32.exe"; Parameters: "/s /u ""{app}\ViewCamFilter.dll""";   Flags: runhidden; RunOnceId: "UnregFilter"
Filename: "{sys}\regsvr32.exe"; Parameters: "/s /u ""{app}\ViewCamMFSource.dll"""; Flags: runhidden; RunOnceId: "UnregMFSource"

[Code]
// Ensure the output directory exists before the compiler tries to write to it.
procedure InitializeWizard;
begin
  // nothing needed — Inno Setup creates OutputDir automatically
end;
