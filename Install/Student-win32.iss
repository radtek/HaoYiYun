; �ű��� Inno Setup �ű������ɡ�
; �����ĵ���ȡ���� INNO SETUP �ű��ļ���ϸ����!

[Setup]
AppName={reg:HKLM\Software\HaoYiStudent, AppName|ѧ����}
AppVerName={reg:HKLM\Software\HaoYiStudent, AppVerName|ѧ����}
AppPublisher={reg:HKLM\Software\HaoYiStudent, AppPublisher|������һ�Ƽ����޹�˾}
AppPublisherURL={reg:HKLM\Software\HaoYiStudent, AppPublisherURL|https://myhaoyi.com}

DefaultGroupName={reg:HKLM\Software\HaoYiStudent, DefaultGroupName|ѧ����}
DefaultDirName={reg:HKLM\Software\HaoYiStudent, DefaultDirName|{pf}\ѧ����}

Compression=lzma
SolidCompression=yes
UsePreviousAppDir=no
UsePreviousGroup=noUsePreviousLanguage=no
AllowCancelDuringInstall=no
OutputDir=..\Product

VersionInfoVersion=1.1.0
OutputBaseFilename=cloud-student-win32-1.1.0

[Languages]
Name: "chinese"; MessagesFile: "compiler:Default.isl"

[Registry]
Root: HKLM; Subkey: "Software\HaoYiStudent"; ValueType: string; ValueName: "DefaultDirName"; ValueData: "{app}"; Flags: uninsdeletekey

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}";

[Files]
Source: "E:\obs-studio\vsbuild\student\Release\*.*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs

[Icons]
Name: "{userdesktop}\ѧ����"; Filename: "{app}\bin\32bit\Student.exe"; Tasks: desktopicon
Name: "{group}\{cm:LaunchProgram,ѧ����}"; Filename: "{app}\bin\32bit\Student.exe"
Name: "{group}\{cm:UninstallProgram,ѧ����}"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\bin\32bit\Student.exe"; Description: "{cm:LaunchProgram,ѧ����}"; Flags: nowait postinstall skipifsilent

