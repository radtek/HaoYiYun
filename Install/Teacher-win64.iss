; �ű��� Inno Setup �ű������ɡ�
; �����ĵ���ȡ���� INNO SETUP �ű��ļ���ϸ����!

[Setup]
AppName={reg:HKLM\Software\HaoYiTeacher, AppName|��ʦ��}
AppVerName={reg:HKLM\Software\HaoYiTeacher, AppVerName|��ʦ��}
AppPublisher={reg:HKLM\Software\HaoYiTeacher, AppPublisher|������һ�Ƽ����޹�˾}
AppPublisherURL={reg:HKLM\Software\HaoYiTeacher, AppPublisherURL|https://myhaoyi.com}

DefaultGroupName={reg:HKLM\Software\HaoYiTeacher, DefaultGroupName|��ʦ��}
DefaultDirName={reg:HKLM\Software\HaoYiTeacher, DefaultDirName|{pf}\��ʦ��}

Compression=lzma
SolidCompression=yes
UsePreviousAppDir=no
UsePreviousGroup=noUsePreviousLanguage=no
AllowCancelDuringInstall=no
OutputDir=..\Product

VersionInfoVersion=1.1.0
OutputBaseFilename=cloud-teacher-win64-1.1.0

[Languages]
Name: "chinese"; MessagesFile: "compiler:Default.isl"

[Registry]
Root: HKLM; Subkey: "Software\HaoYiTeacher"; ValueType: string; ValueName: "DefaultDirName"; ValueData: "{app}"; Flags: uninsdeletekey

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}";

[Files]
Source: "E:\obs-studio\vsbuild\rundir\x64\Release\*.*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs

[Icons]
Name: "{userdesktop}\��ʦ��"; Filename: "{app}\bin\64bit\Teacher.exe"; Tasks: desktopicon
Name: "{group}\{cm:LaunchProgram,��ʦ��}"; Filename: "{app}\bin\64bit\Teacher.exe"
Name: "{group}\{cm:UninstallProgram,��ʦ��}"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\bin\64bit\Teacher.exe"; Description: "{cm:LaunchProgram,��ʦ��}"; Flags: nowait postinstall skipifsilent

