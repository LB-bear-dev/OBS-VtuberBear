#ifndef AppVersion
  #error Pass /DAppVersion=<version>
#endif
#ifndef StageDir
  #error Pass /DStageDir=<the staged package folder>
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif

#define AppName "OBS-VtuberBear"

[Setup]
AppId={{B3F1A6C4-7D52-4E0B-9A1F-5C8E2D4F7A19}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=LB-bear-dev
AppPublisherURL=https://github.com/LB-bear-dev/OBS-VtuberBear
AppSupportURL=https://github.com/LB-bear-dev/OBS-VtuberBear/issues
DefaultDirName={commonappdata}\obs-studio\plugins\{#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
LicenseFile={#StageDir}\LICENSE
InfoBeforeFile=notes.txt
OutputDir={#OutputDir}
OutputBaseFilename={#AppName}-{#AppVersion}-windows-x64-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#AppName}
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#StageDir}\plugins\{#AppName}\*"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\LICENSES\*"; DestDir: "{app}\LICENSES"; Flags: ignoreversion
Source: "{#StageDir}\README.md"; DestDir: "{app}"; Flags: ignoreversion

[Code]
function InitializeSetup: Boolean;
begin
  Result := FileExists(ExpandConstant('{commonpf64}\obs-studio\bin\64bit\obs64.exe')) or DirExists(ExpandConstant('{commonappdata}\obs-studio'));
  if not Result then
    Result := SuppressibleMsgBox('OBS Studio does not seem to be installed on this computer.' + #13#10#13#10 +
      'The plugin does nothing without it. Install anyway (for example into a portable OBS)?', mbConfirmation, MB_YESNO, IDYES) = IDYES;
end;
