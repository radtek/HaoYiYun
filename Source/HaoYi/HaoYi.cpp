
#include "stdafx.h"
#include "HaoYi.h"
#include "MainFrm.h"
#include "HaoYiDoc.h"
#include "HaoYiView.h"
#include "UtilTool.h"
#include "XmlConfig.h"

#include "HCNetSDK.h"
#include "OSThread.h"
#include "SocketUtils.h"
#include "PushThread.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CHaoYiApp, CWinAppEx)
	ON_COMMAND(ID_APP_ABOUT, &CHaoYiApp::OnAppAbout)
	ON_COMMAND(ID_FILE_NEW, &CWinApp::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, &CWinApp::OnFileOpen)
END_MESSAGE_MAP()

CHaoYiApp::CHaoYiApp()
{
	// TODO: 将以下应用程序 ID 字符串替换为唯一的 ID 字符串；建议的字符串格式
	//为 CompanyName.ProductName.SubProduct.VersionInformation
	//SetAppID(_T("HaoYi.AppID.NoVersion"));

	// TODO: 在此处添加构造代码，
	// 将所有重要的初始化放置在 InitInstance 中
}

CHaoYiApp theApp;

/*#include "json.h"
void readJson() 
{
	std::string strValue = "{\"name\":\"json\",\"array\":[{\"cpp\":\"jsoncpp\"},{\"java\":\"jsoninjava\"},{\"php\":\"support\"}]}";

	Json::Reader reader;
	Json::Value value,defVal;

	if (reader.parse(strValue, value))
	{
		std::string out = value["name"].asString();
		//std::cout << out << std::endl;
		TRACE("%s\n", out.c_str());
		const Json::Value arrayObj = value["array"];
		for (unsigned int i = 0; i < arrayObj.size(); i++)
		{
			std::string theKey;
			arrayObj[i].get(theKey, defVal);
			if (!arrayObj[i].isMember("cpp")) 
				continue;
			out = arrayObj[i]["cpp"].asString();
			//std::cout << out;
			TRACE("%s\n", out.c_str());
			
			//if (i != (arrayObj.size() - 1))
			//	std::cout << std::endl;
		}
	}
}

//生成JSON格式数据
void writeJson() 
{
	Json::Value root;
	Json::Value arrayObj;
	Json::Value item;

	item["cpp"] = "jsoncpp";
	item["java"] = "jsoninjava";
	item["php"] = "support";
	arrayObj.append(item);

	root["name"] = "json";
	root["array"] = arrayObj;

	root.toStyledString();
	std::string out = root.toStyledString();

	TRACE("%s\n", out.c_str());
	//std::cout << out << std::endl;
}*/

BOOL CHaoYiApp::InitInstance()
{
	//readJson();
	//writeJson();

	// 如果一个运行在 Windows XP 上的应用程序清单指定要
	// 使用 ComCtl32.dll 版本 6 或更高版本来启用可视化方式，
	//则需要 InitCommonControlsEx()。否则，将无法创建窗口。
	//INITCOMMONCONTROLSEX InitCtrls;
	//InitCtrls.dwSize = sizeof(InitCtrls);
	// 将它设置为包括所有要在应用程序中使用的
	// 公共控件类。
	//InitCtrls.dwICC = ICC_WIN95_CLASSES;
	//InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();

	// 初始化海康SDK资源...
    NET_DVR_Init();

	// 初始化网络、线程、套接字...
	WORD	wsVersion = MAKEWORD(2, 2);
	WSADATA	wsData	  = {0};
	(void)::WSAStartup(wsVersion, &wsData);
	OSThread::Initialize();
	SocketUtils::Initialize();
	CPushThread::Initialize();

	//EnableTaskbarInteraction(FALSE);

	// 使用 RichEdit 控件需要  AfxInitRichEdit2()	
	// AfxInitRichEdit2();

	// 标准初始化
	// 如果未使用这些功能并希望减小
	// 最终可执行文件的大小，则应移除下列
	// 不需要的特定初始化例程
	// 更改用于存储设置的注册表项
	// TODO: 应适当修改该字符串，
	// 例如修改为公司或组织名
	//SetRegistryKey(_T("应用程序向导生成的本地应用程序"));
	//LoadStdProfileSettings(4);  // 加载标准 INI 文件选项(包括 MRU)

	m_nCmdShow = SW_SHOWMAXIMIZED; //SW_SHOWNORMAL;

	/*InitContextMenuManager();
	InitKeyboardManager();
	InitTooltipManager();
	CMFCToolTipInfo ttParams;
	ttParams.m_bVislManagerTheme = TRUE;
	theApp.GetTooltipManager()->SetTooltipParams(AFX_TOOLTIP_TYPE_ALL,RUNTIME_CLASS(CMFCToolTipCtrl), &ttParams);*/

	// 注册应用程序的文档模板。文档模板
	// 将用作文档、框架窗口和视图之间的连接
	CSingleDocTemplate* pDocTemplate;
	pDocTemplate = new CSingleDocTemplate(
		IDR_MAINFRAME,
		RUNTIME_CLASS(CHaoYiDoc),
		RUNTIME_CLASS(CMainFrame),       // 主 SDI 框架窗口
		RUNTIME_CLASS(CHaoYiView));
	if (!pDocTemplate)
		return FALSE;
	AddDocTemplate(pDocTemplate);

	// 分析标准 shell 命令、DDE、打开文件操作的命令行
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);

	// 调度在命令行中指定的命令。如果
	// 用 /RegServer、/Register、/Unregserver 或 /Unregister 启动应用程序，则返回 FALSE。
	if (!ProcessShellCommand(cmdInfo))
		return FALSE;
	CMainFrame * pMainFrame = (CMainFrame*)m_pMainWnd;
	pMainFrame->m_hMenuDefault = pMainFrame->NewDefaultMenu();
	pMainFrame->OnUpdateFrameMenu(pMainFrame->m_hMenuDefault);

	// 从配置当中获取标题名称并显示出来...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	string & strMainName = theConfig.GetMainName();
	m_pMainWnd->SetWindowText(strMainName.c_str());
	m_pMainWnd->UpdateWindow();

	//HICON hIcon = (HICON)::LoadImage(AfxGetResourceHandle(), "..\\HaoYi\\res\\P2P.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
	//DWORD dwErr = GetLastError();
	// 设置成false，会少2个GDI对象...
	//m_pMainWnd->SetIcon(hIcon, false);
	// 这里删除操作并没有释放GDI对象...
	//::DeleteObject(hIcon);

	return TRUE;
}

int CHaoYiApp::ExitInstance() 
{
	// 释放海康SDK资源...
	NET_DVR_Cleanup();

	// 释放分配的系统资源...
	CPushThread::UnInitialize();
	SocketUtils::UnInitialize();
	OSThread::UnInitialize();
	//::WSACleanup();

	return CWinApp::ExitInstance();
}

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();
	enum { IDD = IDD_ABOUTBOX };
protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
protected:
	DECLARE_MESSAGE_MAP()
public:
};

CAboutDlg::CAboutDlg() : CDialogEx(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

// 用于运行对话框的应用程序命令
void CHaoYiApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}


BOOL CAboutDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	CString strTitle;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	string & strMainName = theConfig.GetMainName();
	string & strCopyRight = theConfig.GetCopyRight();
	string & strPhone = theConfig.GetPhone();
	string & strWebSite = theConfig.GetWebSite();
	string & strVersion = theConfig.GetVersion();
	string & strAddress = theConfig.GetAddress();
	GetDlgItem(IDC_ABOUT_COPYRIGHT)->SetWindowText(strCopyRight.c_str());
	GetDlgItem(IDC_ABOUT_VERSION)->SetWindowText(strVersion.c_str());
	GetDlgItem(IDC_ABOUT_PHONE)->SetWindowText(strPhone.c_str());
	GetDlgItem(IDC_ABOUT_WEBSITE)->SetWindowText(strWebSite.c_str());
	GetDlgItem(IDC_ABOUT_ADDRESS)->SetWindowText(strAddress.c_str());

	strTitle.Format("关于 - %s", strMainName.c_str());
	this->SetWindowText(strTitle);

	//CString strVersion;
	//strVersion.Format("V%s - Build %s", CUtilTool::GetServerVersion(), __DATE__);
	//GetDlgItem(IDC_STATIC_VER)->SetWindowText(strVersion);

	return TRUE;
}
