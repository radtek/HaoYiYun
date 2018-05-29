
#include "stdafx.h"
#include "HaoYi.h"
#include "MainFrm.h"
#include "MidView.h"
#include "HaoYiView.h"
#include "UtilTool.h"
#include "XmlConfig.h"
#include "FastSession.h"
#include "Ntray.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
	ON_WM_CLOSE()
	ON_WM_TIMER()
	ON_WM_CREATE()
	ON_MESSAGE(WM_TRAY_MENU, OnTrayPopMenu)
	ON_MESSAGE(WM_TRAY_NOTIFY, OnTrayNotify)
END_MESSAGE_MAP()

static UINT indicators[] =
{
	ID_SEPARATOR,           // ״̬��ָʾ��
	ID_INDICATOR_FASTDFS,
	ID_INDICATOR_TRANSMIT,
	ID_INDICATOR_FILE,
	ID_INDICATOR_KBPS,
	ID_INDICATOR_CPU,
	//ID_INDICATOR_CAPS,
	//ID_INDICATOR_NUM,
	//ID_INDICATOR_SCRL,
};

CMainFrame::CMainFrame()
{
	m_pTrayIcon = NULL;
}

CMainFrame::~CMainFrame()
{
}

HMENU CMainFrame::NewDefaultMenu()
{
	m_menuDefault.LoadMenu(IDR_MAINFRAME);
	return (m_menuDefault.Detach());
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1) {
		MsgLogGM(GM_Err_Config);
		return -1;
	}
	// ����������...
	if( !m_wndToolBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | 
		CBRS_ALIGN_TOP | CBRS_TOOLTIPS | CBRS_SIZE_DYNAMIC | CBRS_FLYBY | CBRS_GRIPPER) )
	{
		TRACE0("Failed to create toolbar\n");
		MsgLogGM(GM_Err_Config);
		return -1;      // fail to create
	}
	// ������Ϣ�������...
	/*if (!m_InfoBar.Create(IDD_INFOBAR,this))
	{
		TRACE0("Failed to create toolbar\n");
		MsgLogGM(GM_Err_Config);
		return -1;      // fail to create
	}*/

	// ���Զ���ķ�ʽ���ع�����...
	this->LoadMyToolBar();

	// ���ع��������ɫλͼ�����Զ��õ�һ��������Ϊ͸��ɫ...
	m_wndToolBar.LoadTrueColorToolBar(32, IDB_BITMAP_HOT, IDB_BITMAP_HOT, IDB_BITMAP_DIS);

	// ��ʼ��Tray...RBBS_BREAK||RBBS_FIXEDSIZE|RBBS_FIXEDSIZE
	if (!m_wndReBar.Create(this, RBS_BANDBORDERS|RBS_AUTOSIZE|RBS_DBLCLKTOGGLE , WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS| WS_CLIPCHILDREN|CBRS_TOP) 
		|| !m_wndReBar.AddBar(&m_wndToolBar, RGB(224,224,224), RGB(224,224,224), NULL, RBBS_NOGRIPPER) )
	{
		TRACE0("Failed to create rebar\n");
		MsgLogGM(GM_Err_Config);
		return -1;		// fail to create
	}
	// ����Ϣ�����뵽���ƴ�����...
	/*if (!m_wndReBar.AddBar(&m_InfoBar, RGB(224,224,224), RGB(224,224,224), NULL, RBBS_NOGRIPPER) ){
		TRACE0("Failed to create rebar\n");
		MsgLogGM(GM_Err_Config);
		return -1;		// fail to create
	}*/
	// ����״̬��Ϣ��...
	if( !m_wndStatusBar.Create(this) ||
		!m_wndStatusBar.SetIndicators(indicators, sizeof(indicators)/sizeof(UINT)) )
	{
		TRACE0("Failed to create status bar\n");
		MsgLogGM(GM_Err_Config);
		return -1;      // fail to create
	}
	// �޸�״̬�����...
	int  nWidth = 0;
	UINT nID = 0, nStyle = 0;
	// �洢������...
	m_wndStatusBar.GetPaneInfo(1, nID, nStyle, nWidth);
	m_wndStatusBar.SetPaneInfo(1, nID, nStyle, 120);
	// ��ת������...
	m_wndStatusBar.GetPaneInfo(2, nID, nStyle, nWidth);
	m_wndStatusBar.SetPaneInfo(2, nID, nStyle, 120);
	// �ϴ��ļ���...
	m_wndStatusBar.GetPaneInfo(3, nID, nStyle, nWidth);
	m_wndStatusBar.SetPaneInfo(3, nID, nStyle, 180);
	// �ϴ�������...
	m_wndStatusBar.GetPaneInfo(4, nID, nStyle, nWidth);
	m_wndStatusBar.SetPaneInfo(4, nID, nStyle, 120);
	// CPUʹ����...
	m_wndStatusBar.GetPaneInfo(5, nID, nStyle, nWidth);
	m_wndStatusBar.SetPaneInfo(5, nID, nStyle, 80);

	// ���ȣ����������ļ�...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	if( !theConfig.GMLoadConfig() ) {
		MsgLogGM(GM_Err_Config);
		return -1;
	}
	// ��������ʣ�����Ϊ¼���̷�...
	this->MakeGMSavePath();
	// ͳһ���������ļ�...
	theConfig.GMSaveConfig();
	// ��ʼ��CPU����...
	m_cpu.Initial();
	// ��������ʱ��...
	this->SetTimer(kStatusTimerID, 1000, NULL);
	// ����ϵͳ����Դ...
	this->BuildTrayIcon();
	return 0;
}
//
// ����ʱ�ӹ���...
void CMainFrame::OnTimer(UINT_PTR nIDEvent)
{
	// ����״̬��Ϣ������ʱ��...
	if( nIDEvent == kStatusTimerID ) {
		CString strText;
		CHaoYiView * lpView = (CHaoYiView*)this->GetActiveView();
		if( lpView != NULL ) {
			strText = ((lpView->m_lpTrackerSession != NULL && lpView->m_lpTrackerSession->IsConnected()) ? "�洢������: ����" : "�洢������: ����");
			m_wndStatusBar.SetPaneText(1, strText);
			strText = ((lpView->m_lpRemoteSession != NULL && lpView->m_lpRemoteSession->IsConnected()) ? "��ת������: ����" : "��ת������: ����");
			m_wndStatusBar.SetPaneText(2, strText);
			LPCTSTR lpszName = lpView->GetCurSendFile();
			strText.Format("�ϴ��ļ�: %s", ((lpszName != NULL && strlen(lpszName) > 0 ) ? lpszName : "��"));
			m_wndStatusBar.SetPaneText(3, strText);
			strText.Format("�ϴ�����: %dKbps", lpView->GetCurSendKbps());
			m_wndStatusBar.SetPaneText(4, strText);
		}
		strText.Format("CPUʹ��: %d%%", m_cpu.GetCpuRate());
		m_wndStatusBar.SetPaneText(5, strText);
	}
	CFrameWnd::OnTimer(nIDEvent);
}
//
// ����¼����ͼ����·��...
GM_Error CMainFrame::MakeGMSavePath()
{
	// �鿴����·���Ƿ��Ѿ����ڣ�����·���Ƿ���Ч...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	string strSavePath = theConfig.GetSavePath();
	// ·�����ȱ�����Ч�����һ�Ҫ��֤·���Ѿ�����...
	if( strSavePath.size() > 0 && ::PathFileExists(strSavePath.c_str()) )
		return GM_NoErr;
	// ׼����Ҫ�Ĳ�����Ϣ...
	float	 fSaveFreeGB = 0.0f;
	DWORD	 dwSize = 0;
	TCHAR	 szBuf[MAX_PATH] = {0};
	LPSTR	 lpszData = NULL;
	int		 nDiskType = 0;
	GM_Error theErr = GM_DiskErr;
	// �õ������б�...
	if( (dwSize = ::GetLogicalDriveStrings(MAX_PATH, szBuf)) <= 0 ) {
		MsgLogGM(theErr);
		return theErr;
	}
	// �����߼��ж�...
	lpszData = szBuf; ASSERT(dwSize > 0);
	while( lpszData[0] != NULL ) {
		nDiskType = ::GetDriveType(lpszData);
		do {
			// �����Ǳ��ش���...
			if( nDiskType != DRIVE_FIXED )
				break;
			// �õ���ʱ·��...
			CString strTmpDir;
			strTmpDir.Format("%s%s", lpszData, DEF_SAVE_PATH);
			// �õ���ǰ���������ʣ��ռ���ܿռ�(.2G)
			ULARGE_INTEGER	llRemain, llTotal, llFree;
			if( !GetDiskFreeSpaceEx(lpszData, &llRemain, &llTotal, &llFree) )
				break;
			// ���ֽ�ת����GB...
			BOOL	bIsFixed = ((nDiskType == DRIVE_FIXED) ? true : false);
			float   fFreeGB  = llFree.QuadPart/(1024.0f*1024.0f*1024.0f);
			float   fTotalGB = llTotal.QuadPart/(1024.0f*1024.0f*1024.0f);
			// ʣ����������ģ���������...
			if( fFreeGB >= fSaveFreeGB ) {
				strSavePath = strTmpDir;
				fSaveFreeGB = fFreeGB;
				break;
			}
			ASSERT( fFreeGB < fSaveFreeGB );
		}while( false );
		// �ƶ������б��������ָ��...
		lpszData += (strlen(lpszData) + 1);
	}
	// �Ի�ȡ��·������֮...
	if( strSavePath.size() <= 0 ) {
		strSavePath = "C:\\GMSave";
	}
	// �������·���������ļ�����...
	ASSERT( strSavePath.size() > 0 );
	theConfig.SetSavePath(strSavePath);
	//theConfig.GMSaveConfig();
	// ��������·��Ŀ¼...
	if( !::PathFileExists(strSavePath.c_str()) ) {
		CUtilTool::CreateDir(strSavePath.c_str());
	}
	return GM_NoErr;
}
//
// �޸Ĺ�������Ϣ...
void CMainFrame::LoadMyToolBar()
{
	// ���ù�����λͼ�߿�...
	SIZE sizeButton, sizeImage;
	sizeButton.cx = kToolBarButtonCX;
	sizeButton.cy = kToolBarButtonCY;
	sizeImage.cx = kToolBarImageCX;
	sizeImage.cy = kToolBarImageCX;
	m_wndToolBar.SetSizes(sizeButton, sizeImage);

	//ȡ��ToolBar��CToolBarCtrl
    CToolBarCtrl& oBarCtrl = m_wndToolBar.GetToolBarCtrl();

	// ��������...
	const int nItemCount = kToolBarCount;
	UINT uCtrlID[nItemCount] = {
		ID_VIEW_FULL,
		ID_ADD_DVR, ID_DEL_DVR, ID_MOD_DVR, 
		ID_LOGIN_DVR, ID_LOGOUT_DVR,
		ID_SYS_SET, ID_APP_ABOUT, ID_RECONNECT,
		ID_BIND_MINI, ID_BIND_ROOM,
		ID_PAGE_PREV, ID_PAGE_JUMP, ID_PAGE_NEXT,
	};
	LPCTSTR lpszCtrl[nItemCount] = {
		"ȫ��ģʽ", 
		"���ͨ��", "ɾ��ͨ��", "�޸�ͨ��", 
		"����ͨ��", "ֹͣͨ��", 
		"ϵͳ����", "��ʾ�汾", "�Ͽ�����",
		"��С����", "��ֱ����",
		"��һҳ", "��תҳ", "��һҳ",
	};

	// �ָ�����ť...
	TBBUTTON sepButton = {0};
    sepButton.fsState = TBSTATE_ENABLED;
    sepButton.fsStyle = TBSTYLE_SEP;

	// ���䰴ť...
	TBBUTTON * pTBButtons = new TBBUTTON[nItemCount];
	ASSERT(pTBButtons != NULL);
    for( int nIndex = 0; nIndex < nItemCount; ++nIndex)
    {
		// ���ð�ť����...
        pTBButtons[nIndex].iString = oBarCtrl.AddStrings(lpszCtrl[nIndex]);	// ����ַ���
        pTBButtons[nIndex].iBitmap = nIndex;								// ���Bitmap����
        pTBButtons[nIndex].idCommand = uCtrlID[nIndex];						// ControlID
        pTBButtons[nIndex].fsState = TBSTATE_ENABLED;
        pTBButtons[nIndex].fsStyle = TBSTYLE_BUTTON;
        pTBButtons[nIndex].dwData = 0;
		// ��Ӱ�ť...
		VERIFY( oBarCtrl.AddButtons(1, &pTBButtons[nIndex]) );
		// ��ӿո�...
		if( nIndex == 0 || nIndex == 3 || nIndex == 5 || nIndex == 8 || nIndex == 10 ) {
			VERIFY( oBarCtrl.AddButtons(1, &sepButton) );
		}
    }
	// �ͷŰ�ť...
    delete[] pTBButtons;
	pTBButtons = NULL;
    oBarCtrl.AutoSize();
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if( !CFrameWnd::PreCreateWindow(cs) )
		return FALSE;
	// ȥ�� EX_CLIENT ���Լ���GDI...
	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	return TRUE;
}

// CMainFrame ���
#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
	CFrameWnd::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
	CFrameWnd::Dump(dc);
}
#endif //_DEBUG

BOOL CMainFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext)
{
	return CFrameWnd::OnCreateClient(lpcs, pContext);
}

void CMainFrame::OnClose()
{
	// �˳�֮ǰ��ѯ��...
	if( ::MessageBox(this->m_hWnd, "ȷʵҪ�˳�ϵͳ��", "ȷ��", MB_OKCANCEL | MB_ICONWARNING) == IDCANCEL )
		return;
	// �ͷ�ϵͳ����Դ...
	this->DestoryTrayIcon();
	// �ͷſ����Դ...
	CFrameWnd::OnClose();
}

void CMainFrame::BuildTrayIcon()
{
	// �ͷ�ϵͳ����Դ...
	this->DestoryTrayIcon();
	// ����ϵͳ������...
	HICON hTrayIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_pTrayIcon	= new CTrayNotifyIcon();
	m_pTrayIcon->Create(this, IDR_MENU_TRAY, "�ɼ���", hTrayIcon, WM_TRAY_NOTIFY);
	m_TrayMenu.LoadMenu(IDR_MENU_TRAY);
}

void CMainFrame::DestoryTrayIcon()
{
	// �ͷ�ϵͳ������...
	if( m_pTrayIcon != NULL ) {
		delete m_pTrayIcon;
		m_pTrayIcon = NULL;
	}
	// �ͷ�ϵͳ���˵�...
	m_TrayMenu.DestroyMenu();
}

LRESULT CMainFrame::OnTrayNotify(WPARAM wParam, LPARAM lParam)
{
	if( m_pTrayIcon == NULL )
		return S_OK;
	return m_pTrayIcon->OnTrayNotification(wParam, lParam);
}

LRESULT CMainFrame::OnTrayPopMenu(WPARAM wParam, LPARAM lParam)
{
	CPoint	posCursor;
	::GetCursorPos(&posCursor);
	CMenu * pSubMenu = m_TrayMenu.GetSubMenu(0);	
	if( pSubMenu != NULL ) {
		pSubMenu->TrackPopupMenu (TPM_LEFTALIGN|TPM_RIGHTBUTTON, posCursor.x, posCursor.y, this);
	}
	return S_OK;
}
