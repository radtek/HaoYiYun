; 脚本用 Inno Setup 脚本向导生成。
; 查阅文档获取创建 INNO SETUP 脚本文件详细资料!

[Setup]
AppName={reg:HKLM\Software\HaoYiStudent, AppName|学生端}
AppVerName={reg:HKLM\Software\HaoYiStudent, AppVerName|学生端}
AppPublisher={reg:HKLM\Software\HaoYiStudent, AppPublisher|北京浩一科技有限公司}
AppPublisherURL={reg:HKLM\Software\HaoYiStudent, AppPublisherURL|https://myhaoyi.com}

DefaultGroupName={reg:HKLM\Software\HaoYiStudent, DefaultGroupName|学生端}
DefaultDirName={reg:HKLM\Software\HaoYiStudent, DefaultDirName|{pf}\学生端}

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
Name: "{userdesktop}\学生端"; Filename: "{app}\bin\32bit\Student.exe"; Tasks: desktopicon
Name: "{group}\{cm:LaunchProgram,学生端}"; Filename: "{app}\bin\32bit\Student.exe"
Name: "{group}\{cm:UninstallProgram,学生端}"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\bin\32bit\Student.exe"; Description: "{cm:LaunchProgram,学生端}"; Flags: nowait postinstall skipifsilent

