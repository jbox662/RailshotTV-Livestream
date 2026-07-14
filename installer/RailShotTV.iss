#define MyAppName "RailShot TV Broadcaster"
#ifndef MyAppVersion
#define MyAppVersion "1.0.0"
#endif
#define MyAppPublisher "RailShot TV"
#define MyAppExeName "RailShotBroadcaster.exe"

[Setup]
AppId={{73E927F5-26AC-4C0A-9E78-E9B718C20C6B}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
VersionInfoVersion={#MyAppVersion}.0
VersionInfoProductVersion={#MyAppVersion}.0
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription={#MyAppName} Installer
DefaultDirName={autopf}\RailShot TV
DefaultGroupName=RailShot TV
DisableProgramGroupPage=yes
OutputDir=..\dist\installer
OutputBaseFilename=RailShotTV-Windows-x64-Setup
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName={#MyAppName}
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Shortcuts:"
Name: "virtualcam"; Description: "Register the RailShot Virtual Camera"; GroupDescription: "Components:"; Flags: checkedonce

[Files]
Source: "..\dist\RailShotTV\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\RailShot TV Broadcaster"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\RailShot TV Broadcaster"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{sys}\regsvr32.exe"; Parameters: "/s ""{app}\railshot-virtualcam64.dll"""; StatusMsg: "Registering RailShot Virtual Camera…"; Flags: runhidden waituntilterminated; Tasks: virtualcam
Filename: "https://go.microsoft.com/fwlink/p/?LinkId=2124703"; Description: "Download Microsoft Edge WebView2 Runtime (required for browser sources)"; Flags: postinstall shellexec skipifsilent unchecked; Check: not WebView2Installed
Filename: "{app}\{#MyAppExeName}"; Description: "Launch RailShot TV Broadcaster"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{sys}\regsvr32.exe"; Parameters: "/u /s ""{app}\railshot-virtualcam64.dll"""; Flags: runhidden waituntilterminated; RunOnceId: "UnregisterRailShotVirtualCam"

[Code]
const
  WebView2ClientGuid = '{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}';

function WebView2Installed: Boolean;
var
  Version: string;
begin
  Version := '';
  if IsWin64 then
    Result :=
      RegQueryStringValue(
        HKLM32,
        'SOFTWARE\Microsoft\EdgeUpdate\Clients\' + WebView2ClientGuid,
        'pv',
        Version)
      or RegQueryStringValue(
        HKCU,
        'Software\Microsoft\EdgeUpdate\Clients\' + WebView2ClientGuid,
        'pv',
        Version)
  else
    Result :=
      RegQueryStringValue(
        HKLM,
        'SOFTWARE\Microsoft\EdgeUpdate\Clients\' + WebView2ClientGuid,
        'pv',
        Version)
      or RegQueryStringValue(
        HKCU,
        'Software\Microsoft\EdgeUpdate\Clients\' + WebView2ClientGuid,
        'pv',
        Version);

  Result := Result and (Version <> '') and (Version <> '0.0.0.0');
end;
