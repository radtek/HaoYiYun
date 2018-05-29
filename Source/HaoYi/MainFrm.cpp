
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
	ID_SEPARATOR,           // 状态行指示器
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
	// 创建工具栏...
	if( !m_wndToolBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | 
		CBRS_ALIGN_TOP | CBRS_TOOLTIPS | CBRS_SIZE_DYNAMIC | CBRS_FLYBY | CBRS_GRIPPER) )
	{
		TRACE0("Failed to create toolbar\n");
		MsgLogGM(GM_Err_Config);
		return -1;      // fail to create
	}
	// 创建信息栏对象框...
	/*if (!m_InfoBar.Create(IDD_INFOBAR,this))
	{
		TRACE0("Failed to create toolbar\n");
		MsgLogGM(GM_Err_Config);
		return -1;      // fail to create
	}*/

	// 以自定义的方式加载工具栏...
	this->LoadMyToolBar();

	// 加载工具栏真彩色位图，会自动用第一个像素做为透明色...
	m_wndToolBar.LoadTrueColorToolBar(32, IDB_BITMAP_HOT, IDB_BITMAP_HOT, IDB_BITMAP_DIS);

	// 初始化Tray...RBBS_BREAK||RBBS_FIXEDSIZE|RBBS_FIXEDSIZE
	if (!m_wndReBar.Create(this, RBS_BANDBORDERS|RBS_AUTOSIZE|RBS_DBLCLKTOGGLE , WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS| WS_CLIPCHILDREN|CBRS_TOP) 
		|| !m_wndReBar.AddBar(&m_wndToolBar, RGB(224,224,224), RGB(224,224,224), NULL, RBBS_NOGRIPPER) )
	{
		TRACE0("Failed to create rebar\n");
		MsgLogGM(GM_Err_Config);
		return -1;		// fail to create
	}
	// 将信息栏加入到控制窗口中...
	/*if (!m_wndReBar.AddBar(&m_InfoBar, RGB(224,224,224), RGB(224,224,224), NULL, RBBS_NOGRIPPER) ){
		TRACE0("Failed to create rebar\n");
		MsgLogGM(GM_Err_Config);
		return -1;		// fail to create
	}*/
	// 创建状态信息栏...
	if( !m_wndStatusBar.Create(this) ||
		!m_wndStatusBar.SetIndicators(indicators, sizeof(indicators)/sizeof(UINT)) )
	{
		TRACE0("Failed to create status bar\n");
		MsgLogGM(GM_Err_Config);
		return -1;      // fail to create
	}
	// 修改状态栏宽度...
	int  nWidth = 0;
	UINT nID = 0, nStyle = 0;
	// 存储服务器...
	m_wndStatusBar.GetPaneInfo(1, nID, nStyle, nWidth);
	m_wndStatusBar.SetPaneInfo(1, nID, nStyle, 120);
	// 中转服务器...
	m_wndStatusBar.GetPaneInfo(2, nID, nStyle, nWidth);
	m_wndStatusBar.SetPaneInfo(2, nID, nStyle, 120);
	// 上传文件区...
	m_wndStatusBar.GetPaneInfo(3, nID, nStyle, nWidth);
	m_wndStatusBar.SetPaneInfo(3, nID, nStyle, 180);
	// 上传码流区...
	m_wndStatusBar.GetPaneInfo(4, nID, nStyle, nWidth);
	m_wndStatusBar.SetPaneInfo(4, nID, nStyle, 120);
	// CPU使用区...
	m_wndStatusBar.GetPaneInfo(5, nID, nStyle, nWidth);
	m_wndStatusBar.SetPaneInfo(5, nID, nStyle, 80);

	// 首先，加载配置文件...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	if( !theConfig.GMLoadConfig() ) {
		MsgLogGM(GM_Err_Config);
		return -1;
	}
	// 设置最大的剩余磁盘为录像盘符...
	this->MakeGMSavePath();
	// 统一保存配置文件...
	theConfig.GMSaveConfig();
	// 初始化CPU对象...
	m_cpu.Initial();
	// 创建更新时钟...
	this->SetTimer(kStatusTimerID, 1000, NULL);
	// 创建系统栏资源...
	this->BuildTrayIcon();
	return 0;
}
//
// 处理时钟过程...
void CMainFrame::OnTimer(UINT_PTR nIDEvent)
{
	// 处理状态信息栏更新时钟...
	if( nIDEvent == kStatusTimerID ) {
		CString strText;
		CHaoYiView * lpView = (CHaoYiView*)this->GetActiveView();
		if( lpView != NULL ) {
			strText = ((lpView->m_lpTrackerSession != NULL && lpView->m_lpTrackerSession->IsConnected()) ? "存储服务器: 在线" : "存储服务器: 离线");
			m_wndStatusBar.SetPaneText(1, strText);
			strText = ((lpView->m_lpRemoteSession != NULL && lpView->m_lpRemoteSession->IsConnected()) ? "中转服务器: 在线" : "中转服务器: 离线");
			m_wndStatusBar.SetPaneText(2, strText);
			LPCTSTR lpszName = lpView->GetCurSendFile();
			strText.Format("上传文件: %s", ((lpszName != NULL && strlen(lpszName) > 0 ) ? lpszName : "无"));
			m_wndStatusBar.SetPaneText(3, strText);
			strText.Format("上传码流: %dKbps", lpView->GetCurSendKbps());
			m_wndStatusBar.SetPaneText(4, strText);
		}
		strText.Format("CPU使用: %d%%", m_cpu.GetCpuRate());
		m_wndStatusBar.SetPaneText(5, strText);
	}
	CFrameWnd::OnTimer(nIDEvent);
}
//
// 创建录像或截图存盘路径...
GM_Error CMainFrame::MakeGMSavePath()
{
	// 查看存盘路径是否已经存在，并且路径是否有效...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	string strSavePath = theConfig.GetSavePath();
	// 路径长度必须有效，而且还要保证路径已经存在...
	if( strSavePath.size() > 0 && ::PathFileExists(strSavePath.c_str()) )
		return GM_NoErr;
	// 准备需要的参数信息...
	float	 fSaveFreeGB = 0.0f;
	DWORD	 dwSize = 0;
	TCHAR	 szBuf[MAX_PATH] = {0};
	LPSTR	 lpszData = NULL;
	int		 nDiskType = 0;
	GM_Error theErr = GM_DiskErr;
	// 得到磁盘列表...
	if( (dwSize = ::GetLogicalDriveStrings(MAX_PATH, szBuf)) <= 0 ) {
		MsgLogGM(theErr);
		return theErr;
	}
	// 进行逻辑判断...
	lpszData = szBuf; ASSERT(dwSize > 0);
	while( lpszData[0] != NULL ) {
		nDiskType = ::GetDriveType(lpszData);
		do {
			// 必须是本地磁盘...
			if( nDiskType != DRIVE_FIXED )
				break;
			// 得到临时路径...
			CString strTmpDir;
			strTmpDir.Format("%s%s", lpszData, DEF_SAVE_PATH);
			// 得到当前磁盘下面的剩余空间和总空间(.2G)
			ULARGE_INTEGER	llRemain, llTotal, llFree;
			if( !GetDiskFreeSpaceEx(lpszData, &llRemain, &llTotal, &llFree) )
				break;
			// 将字节转化成GB...
			BOOL	bIsFixed = ((nDiskType == DRIVE_FIXED) ? true : false);
			float   fFreeGB  = llFree.QuadPart/(1024.0f*1024.0f*1024.0f);
			float   fTotalGB = llTotal.QuadPart/(1024.0f*1024.0f*1024.0f);
			// 剩余磁盘是最大的，保存数据...
			if( fFreeGB >= fSaveFreeGB ) {
				strSavePath = strTmpDir;
				fSaveFreeGB = fFreeGB;
				break;
			}
			ASSERT( fFreeGB < fSaveFreeGB );
		}while( false );
		// 移动磁盘列表的数据区指针...
		lpszData += (strlen(lpszData) + 1);
	}
	// 对获取的路径处理之...
	if( strSavePath.size() <= 0 ) {
		strSavePath = "C:\\GMSave";
	}
	// 保存存盘路径到配置文件当中...
	ASSERT( strSavePath.size() > 0 );
	theConfig.SetSavePath(strSavePath);
	//theConfig.GMSaveConfig();
	// 创建存盘路径目录...
	if( !::PathFileExists(strSavePath.c_str()) ) {
		CUtilTool::CreateDir(strSavePath.c_str());
	}
	return GM_NoErr;
}
//
// 修改工具栏信息...
void CMainFrame::LoadMyToolBar()
{
	// 设置工具栏位图高宽...
	SIZE sizeButton, sizeImage;
	sizeButton.cx = kToolBarButtonCX;
	sizeButton.cy = kToolBarButtonCY;
	sizeImage.cx = kToolBarImageCX;
	sizeImage.cy = kToolBarImageCX;
	m_wndToolBar.SetSizes(sizeButton, sizeImage);

	//取到ToolBar的CToolBarCtrl
    CToolBarCtrl& oBarCtrl = m_wndToolBar.GetToolBarCtrl();

	// 设置命令...
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
		"全屏模式", 
		"添加通道", "删除通道", "修改通道", 
		"启动通道", "停止通道", 
		"系统设置", "显示版本", "断开重连",
		"绑定小程序", "绑定直播间",
		"上一页", "跳转页", "下一页",
	};

	// 分隔符按钮...
	TBBUTTON sepButton = {0};
    sepButton.fsState = TBSTATE_ENABLED;
    sepButton.fsStyle = TBSTYLE_SEP;

	// 分配按钮...
	TBBUTTON * pTBButtons = new TBBUTTON[nItemCount];
	ASSERT(pTBButtons != NULL);
    for( int nIndex = 0; nIndex < nItemCount; ++nIndex)
    {
		// 设置按钮属性...
        pTBButtons[nIndex].iString = oBarCtrl.AddStrings(lpszCtrl[nIndex]);	// 添加字符串
        pTBButtons[nIndex].iBitmap = nIndex;								// 添加Bitmap索引
        pTBButtons[nIndex].idCommand = uCtrlID[nIndex];						// ControlID
        pTBButtons[nIndex].fsState = TBSTATE_ENABLED;
        pTBButtons[nIndex].fsStyle = TBSTYLE_BUTTON;
        pTBButtons[nIndex].dwData = 0;
		// 添加按钮...
		VERIFY( oBarCtrl.AddButtons(1, &pTBButtons[nIndex]) );
		// 添加空格...
		if( nIndex == 0 || nIndex == 3 || nIndex == 5 || nIndex == 8 || nIndex == 10 ) {
			VERIFY( oBarCtrl.AddButtons(1, &sepButton) );
		}
    }
	// 释放按钮...
    delete[] pTBButtons;
	pTBButtons = NULL;
    oBarCtrl.AutoSize();
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if( !CFrameWnd::PreCreateWindow(cs) )
		return FALSE;
	// 去掉 EX_CLIENT 可以减少GDI...
	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	return TRUE;
}

// CMainFrame 诊断
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
	// 退出之前的询问...
	if( ::MessageBox(this->m_hWnd, "确实要退出系统吗？", "确认", MB_OKCANCEL | MB_ICONWARNING) == IDCANCEL )
		return;
	// 释放系统栏资源...
	this->DestoryTrayIcon();
	// 释放框架资源...
	CFrameWnd::OnClose();
}

void CMainFrame::BuildTrayIcon()
{
	// 释放系统栏资源...
	this->DestoryTrayIcon();
	// 创建系统栏对象...
	HICON hTrayIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_pTrayIcon	= new CTrayNotifyIcon();
	m_pTrayIcon->Create(this, IDR_MENU_TRAY, "采集端", hTrayIcon, WM_TRAY_NOTIFY);
	m_TrayMenu.LoadMenu(IDR_MENU_TRAY);
}

void CMainFrame::DestoryTrayIcon()
{
	// 释放系统栏对象...
	if( m_pTrayIcon != NULL ) {
		delete m_pTrayIcon;
		m_pTrayIcon = NULL;
	}
	// 释放系统栏菜单...
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
