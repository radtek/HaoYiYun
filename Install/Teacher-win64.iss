; 脚本用 Inno Setup 脚本向导生成。
; 查阅文档获取创建 INNO SETUP 脚本文件详细资料!

[Setup]
AppName={reg:HKLM\Software\HaoYiTeacher, AppName|讲师端}
AppVerName={reg:HKLM\Software\HaoYiTeacher, AppVerName|讲师端}
AppPublisher={reg:HKLM\Software\HaoYiTeacher, AppPublisher|北京浩一科技有限公司}
AppPublisherURL={reg:HKLM\Software\HaoYiTeacher, AppPublisherURL|https://myhaoyi.com}

DefaultGroupName={reg:HKLM\Software\HaoYiTeacher, DefaultGroupName|讲师端}
DefaultDirName={reg:HKLM\Software\HaoYiTeacher, DefaultDirName|{pf}\讲师端}

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
Name: "{userdesktop}\讲师端"; Filename: "{app}\bin\64bit\Teacher.exe"; Tasks: desktopicon
Name: "{group}\{cm:LaunchProgram,讲师端}"; Filename: "{app}\bin\64bit\Teacher.exe"
Name: "{group}\{cm:UninstallProgram,讲师端}"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\bin\64bit\Teacher.exe"; Description: "{cm:LaunchProgram,讲师端}"; Flags: nowait postinstall skipifsilent

