; 脚本用 Inno Setup 脚本向导生成。
; 查阅文档获取创建 INNO SETUP 脚本文件详细资料!

[Setup]
AppName={reg:HKLM\Software\HaoYiYun, AppName|云录播}
AppVerName={reg:HKLM\Software\HaoYiYun, AppVerName|云录播}
;AppPublisher={reg:HKLM\Software\GMBacklot, AppPublisher|北京世纪葵花数字传媒技术有限公司}
;AppPublisherURL={reg:HKLM\Software\GMBacklot, AppPublisherURL|http://www.kuihua.net}

DefaultGroupName={reg:HKLM\Software\HaoYiYun, DefaultGroupName|云录播}
DefaultDirName={reg:HKLM\Software\HaoYiYun, DefaultDirName|{pf}\云录播}

Compression=lzma
SolidCompression=yes
UsePreviousAppDir=no
UsePreviousGroup=no
;UsePreviousLanguage=no
AllowCancelDuringInstall=no
OutputDir=..\Product

VersionInfoVersion=1.0.0.1
OutputBaseFilename=云录播

[Languages]
Name: "chinese"; MessagesFile: "compiler:Default.isl"

[Registry]
Root: HKLM; Subkey: "Software\HaoYiYun"; ValueType: string; ValueName: "DefaultDirName"; ValueData: "{app}"; Flags: uninsdeletekey

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}";

[Files]
Source: "..\Source\bin\HaoYi.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Source\bin\AudioRender.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Source\bin\D3DX9_43.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Source\bin\HCCore.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Source\bin\HCNetSDK.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Source\bin\libcurl.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Source\bin\PlayCtrl.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Source\bin\SuperRender.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Source\bin\zlib1.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Source\bin\HCNetSDKCom\HCPreview.dll"; DestDir: "{app}\HCNetSDKCom"; Flags: ignoreversion
Source: "..\Source\bin\HCNetSDKCom\HCCoreDevCfg.dll"; DestDir: "{app}\HCNetSDKCom"; Flags: ignoreversion
Source: "..\Source\bin\HCNetSDKCom\HCGeneralCfgMgr.dll"; DestDir: "{app}\HCNetSDKCom"; Flags: ignoreversion

[Icons]
Name: "{userdesktop}\云录播"; Filename: "{app}\HaoYi.exe"; Tasks: desktopicon
Name: "{group}\{cm:LaunchProgram,云录播}"; Filename: "{app}\HaoYi.exe"
Name: "{group}\{cm:UninstallProgram,云录播}"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\HaoYi.exe"; Description: "{cm:LaunchProgram,云录播}"; Flags: nowait postinstall skipifsilent

