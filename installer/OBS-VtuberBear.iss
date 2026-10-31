; Inno Setup script for the Windows installer. CI builds it from the same staged folder the release zip is made from:
;   ISCC.exe /DAppVersion=1.2.3 /DStageDir=<package folder> /DOutputDir=<where to write the setup exe> installer\OBS-VtuberBear.iss
; See .github/workflows/build.yml.

#ifndef AppVersion
  #error Pass /DAppVersion=<version>
#endif
#ifndef StageDir
  #error Pass /DStageDir=<the staged package folder, holding plugins\OBS-VtuberBear, LICENSE and the notices>
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif

#define AppName "OBS-VtuberBear"
#define PluginFolder "OBS-VtuberBear"
; What the plugin's folder and DLL were called before it was renamed
#define OldPluginFolder "CharacterControlOBS"
; Never change AppGuid: it is how a newer installer finds and upgrades an older install
#define AppGuid "B3F1A6C4-7D52-4E0B-9A1F-5C8E2D4F7A19"

[Setup]
AppId={{{#AppGuid}}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=LB-bear-dev
AppPublisherURL=https://github.com/LB-bear-dev/OBS-VtuberBear
AppSupportURL=https://github.com/LB-bear-dev/OBS-VtuberBear/issues
; OBS's shared plugin folder (https://obsproject.com/kb/legacy-plugin-locations), or next to an earlier install, see DefaultPluginDir.
; Portable OBS users can change it on the folder page.
DefaultDirName={code:DefaultPluginDir}
; An earlier install's folder may carry the old name, which OBS would no longer load the plugin from, so DefaultPluginDir decides
UsePreviousAppDir=no
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
LicenseFile={#StageDir}\LICENSE
InfoBeforeFile=notes.txt
OutputDir={#OutputDir}
OutputBaseFilename={#PluginFolder}-{#AppVersion}-windows-x64-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#AppName}
; Offers to close OBS if it is using the plugin's files
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#StageDir}\plugins\{#PluginFolder}\*"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\README.md"; DestDir: "{app}"; Flags: ignoreversion

[InstallDelete]
; Copies from before OBS's plugin folder layout. OBS only treats that location as legacy, and a second copy there is loaded
; as a duplicate module.
Type: files; Name: "{commonpf64}\obs-studio\obs-plugins\64bit\CharacterControlOBS.dll"
Type: files; Name: "{commonpf64}\obs-studio\obs-plugins\64bit\CharacterControlOBS.pdb"
Type: files; Name: "{commonpf64}\obs-studio\obs-plugins\64bit\DiscordBear.dll"
Type: files; Name: "{commonpf64}\obs-studio\obs-plugins\64bit\DiscordBear.pdb"
Type: files; Name: "{commonpf64}\obs-studio\obs-plugins\64bit\cpprest142_2_10.dll"
; Earlier versions shipped cpprest next to the plugin, DiscordBear now uses WinHTTP instead.
Type: files; Name: "{app}\cpprest142_2_10.dll"
; Earlier versions shipped DiscordBear's PDB, it is no longer part of the release.
Type: files; Name: "{app}\DiscordBear.pdb"

[Code]
function ObsFound: Boolean;
begin
  Result := FileExists(ExpandConstant('{commonpf64}\obs-studio\bin\64bit\obs64.exe')) or DirExists(ExpandConstant('{commonappdata}\obs-studio'));
end;

function InitializeSetup: Boolean;
begin
  Result := True;
  if not ObsFound then
  begin
    // A portable OBS has neither, so the answer is the user's. Silent installs carry on.
    Result := SuppressibleMsgBox('OBS Studio does not seem to be installed on this computer.' + #13#10#13#10 +
      'The plugin does nothing without it. Install anyway (for example into a portable OBS)?', mbConfirmation, MB_YESNO, IDYES) = IDYES;
  end;
end;

// Where an earlier version of this installer put the plugin, or '' if it never ran here
function PreviousAppDir: String;
begin
  if not RegQueryStringValue(HKLM, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{{#AppGuid}}_is1', 'Inno Setup: App Path', Result) then
    Result := '';
  Result := RemoveBackslashUnlessRoot(Result);
end;

// An earlier install's folder when it has the plugin's old name, or '' if there is none
function OldPluginDir: String;
begin
  Result := PreviousAppDir;
  if (Result = '') or (CompareText(ExtractFileName(Result), '{#OldPluginFolder}') <> 0) then
    Result := ExpandConstant('{commonappdata}\obs-studio\plugins\{#OldPluginFolder}');
  if not DirExists(Result) then
    Result := '';
end;

// The same plugins folder as the earlier install, under the new name, so a portable OBS's install stays in that OBS
function DefaultPluginDir(Param: String): String;
var
  Previous: String;
begin
  Previous := PreviousAppDir;
  if Previous = '' then
    Result := ExpandConstant('{commonappdata}\obs-studio\plugins\{#PluginFolder}')
  else if CompareText(ExtractFileName(Previous), '{#OldPluginFolder}') = 0 then
    Result := AddBackslash(ExtractFileDir(Previous)) + '{#PluginFolder}'
  else
    Result := Previous;
end;

// Removes the plugin's folder from before the rename. OBS would load the old CharacterControlOBS.dll in it next to the new one, and
// both register the same sources.
procedure CurStepChanged(CurStep: TSetupStep);
var
  OldDir: String;
begin
  if CurStep <> ssInstall then
    Exit;
  OldDir := OldPluginDir;
  if (OldDir <> '') and (CompareText(OldDir, RemoveBackslashUnlessRoot(ExpandConstant('{app}'))) <> 0) then
  begin
    if not DelTree(OldDir, True, True, True) then
      Log('Could not remove the old plugin folder ' + OldDir);
  end;
end;
