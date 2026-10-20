; Subside — Inno Setup installer

#define MyAppName "Subside"
#define MyAppVersion "1.0.5"
#define MyAppPublisher "Dewpoint AV"
#define MyAppURL "https://dewpoint.live"

[Setup]
AppId={{7C3D9A51-2B4E-4F6A-8C1D-6E9F2A3B4C5E}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\Dewpoint AV\{#MyAppName}
DefaultGroupName=Dewpoint AV\{#MyAppName}
DisableProgramGroupPage=yes
DisableDirPage=yes
OutputDir=output
OutputBaseFilename={#MyAppName}-Setup-{#MyAppVersion}
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
SetupLogging=yes
UninstallDisplayName={#MyAppName} (Dewpoint AV)

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "..\build\dist\Subside.vst3\*"; DestDir: "{commoncf}\VST3\{#MyAppName}.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\build\dist\Subside.clap"; DestDir: "{commoncf}\CLAP"; Flags: ignoreversion

[InstallDelete]
Type: filesandordirs; Name: "{commoncf}\VST3\Subside.vst3"
Type: files; Name: "{commoncf}\CLAP\Subside.clap"

[Icons]
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"

[Messages]
WelcomeLabel1=Subside Sidechain Ducker
WelcomeLabel2=This will install Subside (VST3 + CLAP) on your computer.%n%nClick Next to continue.
