; 脚本用 Inno Setup 脚本向导生成。
; 查阅文档获取创建 INNO SETUP 脚本文件详细资料!

[Setup]
AppName={reg:HKLM\Software\HaoYiYun, AppName|采集端}
AppVerName={reg:HKLM\Software\HaoYiYun, AppVerName|采集端}
AppPublisher={reg:HKLM\Software\HaoYiYun, AppPublisher|北京浩一科技有限公司}
AppPublisherURL={reg:HKLM\Software\HaoYiYun, AppPublisherURL|https://myhaoyi.com}

DefaultGroupName={reg:HKLM\Software\HaoYiYun, DefaultGroupName|采集端}
DefaultDirName={reg:HKLM\Software\HaoYiYun, DefaultDirName|{pf}\采集端}

Compression=lzma
SolidCompression=yes
UsePreviousAppDir=no
UsePreviousGroup=noUsePreviousLanguage=no
AllowCancelDuringInstall=no
OutputDir=..\Product

VersionInfoVersion=1.3.4
OutputBaseFilename=cloud-gather-nodelay-1.3.4

[Languages]
Name: "chinese"; MessagesFile: "compiler:Default.isl"

[Registry]
Root: HKLM; Subkey: "Software\HaoYiYun"; ValueType: string; ValueName: "DefaultDirName"; ValueData: "{app}"; Flags: uninsdeletekey

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}";

[Files]
Source: "..\Source\bin\SDL2.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Source\bin\swscale-2.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Source\bin\swresample-0.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Source\bin\HaoYi.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Source\bin\msvcr100.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Source\bin\mplayer.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Source\bin\avcodec-55.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Source\bin\avformat-55.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Source\bin\avutil-52.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Source\bin\AudioRender.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Source\bin\D3DX9_43.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Source\bin\HCCore.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Source\bin\HCNetSDK.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Source\bin\libcurl.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Source\bin\libeay32.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Source\bin\ssleay32.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Source\bin\zlib1.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Source\bin\PlayCtrl.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Source\bin\SuperRender.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Source\bin\HCNetSDKCom\HCPreview.dll"; DestDir: "{app}\HCNetSDKCom"; Flags: ignoreversion
Source: "..\Source\bin\HCNetSDKCom\HCCoreDevCfg.dll"; DestDir: "{app}\HCNetSDKCom"; Flags: ignoreversion
Source: "..\Source\bin\HCNetSDKCom\HCGeneralCfgMgr.dll"; DestDir: "{app}\HCNetSDKCom"; Flags: ignoreversion

[Icons]
Name: "{userdesktop}\采集端"; Filename: "{app}\HaoYi.exe"; Tasks: desktopicon
Name: "{group}\{cm:LaunchProgram,采集端}"; Filename: "{app}\HaoYi.exe"
Name: "{group}\{cm:UninstallProgram,采集端}"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\HaoYi.exe"; Description: "{cm:LaunchProgram,采集端}"; Flags: nowait postinstall skipifsilent

