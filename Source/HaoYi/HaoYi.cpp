
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
	// TODO: ������Ӧ�ó��� ID �ַ����滻ΪΨһ�� ID �ַ�����������ַ�����ʽ
	//Ϊ CompanyName.ProductName.SubProduct.VersionInformation
	//SetAppID(_T("HaoYi.AppID.NoVersion"));

	// TODO: �ڴ˴���ӹ�����룬
	// ��������Ҫ�ĳ�ʼ�������� InitInstance ��
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

//����JSON��ʽ����
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

	// ���һ�������� Windows XP �ϵ�Ӧ�ó����嵥ָ��Ҫ
	// ʹ�� ComCtl32.dll �汾 6 ����߰汾�����ÿ��ӻ���ʽ��
	//����Ҫ InitCommonControlsEx()�����򣬽��޷��������ڡ�
	//INITCOMMONCONTROLSEX InitCtrls;
	//InitCtrls.dwSize = sizeof(InitCtrls);
	// ��������Ϊ��������Ҫ��Ӧ�ó�����ʹ�õ�
	// �����ؼ��ࡣ
	//InitCtrls.dwICC = ICC_WIN95_CLASSES;
	//InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();

	// ��ʼ������SDK��Դ...
    NET_DVR_Init();

	// ��ʼ�����硢�̡߳��׽���...
	WORD	wsVersion = MAKEWORD(2, 2);
	WSADATA	wsData	  = {0};
	(void)::WSAStartup(wsVersion, &wsData);
	OSThread::Initialize();
	SocketUtils::Initialize();
	CPushThread::Initialize();

	//EnableTaskbarInteraction(FALSE);

	// ʹ�� RichEdit �ؼ���Ҫ  AfxInitRichEdit2()	
	// AfxInitRichEdit2();

	// ��׼��ʼ��
	// ���δʹ����Щ���ܲ�ϣ����С
	// ���տ�ִ���ļ��Ĵ�С����Ӧ�Ƴ�����
	// ����Ҫ���ض���ʼ������
	// �������ڴ洢���õ�ע�����
	// TODO: Ӧ�ʵ��޸ĸ��ַ�����
	// �����޸�Ϊ��˾����֯��
	//SetRegistryKey(_T("Ӧ�ó��������ɵı���Ӧ�ó���"));
	//LoadStdProfileSettings(4);  // ���ر�׼ INI �ļ�ѡ��(���� MRU)

	m_nCmdShow = SW_SHOWMAXIMIZED; //SW_SHOWNORMAL;

	/*InitContextMenuManager();
	InitKeyboardManager();
	InitTooltipManager();
	CMFCToolTipInfo ttParams;
	ttParams.m_bVislManagerTheme = TRUE;
	theApp.GetTooltipManager()->SetTooltipParams(AFX_TOOLTIP_TYPE_ALL,RUNTIME_CLASS(CMFCToolTipCtrl), &ttParams);*/

	// ע��Ӧ�ó�����ĵ�ģ�塣�ĵ�ģ��
	// �������ĵ�����ܴ��ں���ͼ֮�������
	CSingleDocTemplate* pDocTemplate;
	pDocTemplate = new CSingleDocTemplate(
		IDR_MAINFRAME,
		RUNTIME_CLASS(CHaoYiDoc),
		RUNTIME_CLASS(CMainFrame),       // �� SDI ��ܴ���
		RUNTIME_CLASS(CHaoYiView));
	if (!pDocTemplate)
		return FALSE;
	AddDocTemplate(pDocTemplate);

	// ������׼ shell ���DDE�����ļ�������������
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);

	// ��������������ָ����������
	// �� /RegServer��/Register��/Unregserver �� /Unregister ����Ӧ�ó����򷵻� FALSE��
	if (!ProcessShellCommand(cmdInfo))
		return FALSE;
	CMainFrame * pMainFrame = (CMainFrame*)m_pMainWnd;
	pMainFrame->m_hMenuDefault = pMainFrame->NewDefaultMenu();
	pMainFrame->OnUpdateFrameMenu(pMainFrame->m_hMenuDefault);

	// �����õ��л�ȡ�������Ʋ���ʾ����...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	string & strMainName = theConfig.GetMainName();
	m_pMainWnd->SetWindowText(strMainName.c_str());
	m_pMainWnd->UpdateWindow();

	//HICON hIcon = (HICON)::LoadImage(AfxGetResourceHandle(), "..\\HaoYi\\res\\P2P.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
	//DWORD dwErr = GetLastError();
	// ���ó�false������2��GDI����...
	//m_pMainWnd->SetIcon(hIcon, false);
	// ����ɾ��������û���ͷ�GDI����...
	//::DeleteObject(hIcon);

	return TRUE;
}

int CHaoYiApp::ExitInstance() 
{
	// �ͷź���SDK��Դ...
	NET_DVR_Cleanup();

	// �ͷŷ����ϵͳ��Դ...
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

// �������жԻ����Ӧ�ó�������
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

	strTitle.Format("���� - %s", strMainName.c_str());
	this->SetWindowText(strTitle);

	//CString strVersion;
	//strVersion.Format("V%s - Build %s", CUtilTool::GetServerVersion(), __DATE__);
	//GetDlgItem(IDC_STATIC_VER)->SetWindowText(strVersion);

	return TRUE;
}
