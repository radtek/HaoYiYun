
#include "stdafx.h"
#include "HaoYi.h"
#include "HaoYiDoc.h"
#include "HaoYiView.h"
#include "MainFrm.h"
#include "MidView.h"
#include "UtilTool.h"
#include "HKUdpThread.h"
#include "FastSession.h"
#include "FastThread.h"
#include "XmlConfig.h"
#include "DlgSetSys.h"
#include "DlgSetDVR.h"
#include "DlgPushDVR.h"
#include "WebThread.h"
#include "SocketUtils.h"
#include "Camera.h"

#pragma comment(lib, "iphlpapi.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(CHaoYiView, CFormView)

BEGIN_MESSAGE_MAP(CHaoYiView, CFormView)
	ON_WM_SIZE()
	ON_WM_TIMER()
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONDOWN()
	ON_COMMAND(ID_VK_FULL, &CHaoYiView::OnVKFull)
	ON_COMMAND(ID_VIEW_FULL, &CHaoYiView::OnVKFull)
	ON_COMMAND(ID_VK_ESCAPE, &CHaoYiView::OnVKEscape)
	ON_UPDATE_COMMAND_UI(ID_ADD_DVR, &CHaoYiView::OnCmdUpdateAddDVR)
	ON_UPDATE_COMMAND_UI(ID_MOD_DVR, &CHaoYiView::OnCmdUpdateModDVR)
	ON_UPDATE_COMMAND_UI(ID_DEL_DVR, &CHaoYiView::OnCmdUpdateDelDVR)
	ON_UPDATE_COMMAND_UI(ID_LOGIN_DVR, &CHaoYiView::OnCmdUpdateLoginDVR)
	ON_UPDATE_COMMAND_UI(ID_LOGOUT_DVR, &CHaoYiView::OnCmdUpdateLogoutDVR)
	ON_NOTIFY(TVN_SELCHANGED, IDC_TREE_DEVICE, &CHaoYiView::OnSelchangedTreeDevice)
	ON_NOTIFY(TVN_KEYDOWN, IDC_TREE_DEVICE, &CHaoYiView::OnKeydownTreeDevice)
	ON_NOTIFY(NM_RCLICK, IDC_TREE_DEVICE, &CHaoYiView::OnRclickTreeDevice)
	ON_MESSAGE(WM_ADD_BY_TRANSMIT_MSG, &CHaoYiView::OnMsgAddByTransmit)
	ON_MESSAGE(WM_DEL_BY_TRANSMIT_MSG, &CHaoYiView::OnMsgDelByTransmit)
	ON_MESSAGE(WM_WEB_LOAD_RESOURCE,&CHaoYiView::OnMsgWebLoadResource)
	ON_MESSAGE(WM_WEB_AUTH_RESULT,&CHaoYiView::OnMsgWebAuthResult)
	ON_MESSAGE(WM_EVENT_SESSION_MSG, &CHaoYiView::OnMsgEventSession)
	ON_MESSAGE(WM_FIND_HK_CAMERA, &CHaoYiView::OnMsgFindHKCamera)
	ON_MESSAGE(WM_FOCUS_VIDEO, &CHaoYiView::OnMsgFocusVideo)
	ON_MESSAGE(WM_RELOAD_VIEW, &CHaoYiView::OnMsgReloadView)
	ON_COMMAND(ID_LOGIN_DVR, &CHaoYiView::OnLoginDVR)
	ON_COMMAND(ID_LOGOUT_DVR, &CHaoYiView::OnLogoutDVR)
	ON_COMMAND(ID_SYS_SET, &CHaoYiView::OnSysSet)
	ON_COMMAND(ID_ADD_DVR, &CHaoYiView::OnAddDVR)
	ON_COMMAND(ID_MOD_DVR, &CHaoYiView::OnModDVR)
	ON_COMMAND(ID_DEL_DVR, &CHaoYiView::OnDelDVR)
END_MESSAGE_MAP()

CHaoYiView::CHaoYiView()
  : CFormView(CHaoYiView::IDD)
  , m_rcSplitLeftNew(0,0,0,0)
  , m_rcSplitLeftSrc(0,0,0,0)
  , m_rcSplitRightNew(0,0,0,0)
  , m_rcSplitRightSrc(0,0,0,0)
  , m_lpTrackerSession(NULL)
  , m_lpStorageSession(NULL)
  , m_lpRemoteSession(NULL)
  , m_bRightDraging(false)
  , m_bTreeKeydown(false)
  , m_bLeftDraging(false)
  , m_lpWebThread(NULL)
  , m_lpFastThread(NULL)
  , m_lpHKUdpThread(NULL)
  , m_hCurHorSplit(NULL)
  , m_nFocusDBCamera(0)
  , m_nAnimateIndex(0)
  , m_hRootItem(NULL)
  , m_lpMidView(NULL)
  , m_RightView(this)
{
	m_hCurHorSplit = AfxGetApp()->LoadStandardCursor(IDC_SIZEWE);
	ASSERT( m_hCurHorSplit != NULL );
}

CHaoYiView::~CHaoYiView()
{
}

BOOL CHaoYiView::doWebStatCamera(int nDBCamera, int nStatus, int nErrCode/* = 0*/, LPCTSTR lpszErrMsg/* = NULL*/)
{
	if( m_lpWebThread == NULL )
		return false;
	return m_lpWebThread->doWebStatCamera(nDBCamera, nStatus, nErrCode, lpszErrMsg);
}

void CHaoYiView::OnDestroy()
{
	// 通知网站采集端退出...
	this->doGatherLogout();
	// 在窗口关闭之前，释放资源...
	this->DestroyResource();
	// 释放窗口句柄等资源...
	CFormView::OnDestroy();
}

void CHaoYiView::DoDataExchange(CDataExchange* pDX)
{
	CFormView::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_QR_CODE, m_QRStatic);
	DDX_Control(pDX, IDC_TREE_DEVICE, m_DeviceTree);
	DDX_Control(pDX, IDC_RIGHT_VIEW, m_RightView);
}

void CHaoYiView::OnDraw(CDC* pDC)
{
	CHaoYiDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if( pDoc == NULL )
		return;
	/***********************************************************************************************
	//blog.csdn.net/sz76211822/article/details/51507616 => 快速显示动画例子，需要改造透明显示显示
	/***********************************************************************************************
	//TransparentBlt()...
	//尝试显示透明的GIF动画 - 失败！
	//m_Image.Load("c:\\loading.gif");
	//m_Image.Load("c:\\111.gif");
	//COLORREF crOld = m_Image.SetTransparentColor(RGB(255,255,255));
	//#include <GdiPlus.h>
	//#pragma comment(lib, "Gdiplus.lib")
	//using namespace Gdiplus;
	//#include <atlimage.h>
	//CImage m_Image;
	/*CDC * lpMyDC = m_PrevStatic.GetDC();
    Graphics graphics(lpMyDC->m_hDC);
    Image image(L"c:\\111.gif");
    graphics.DrawImage(&image, 0, 0, image.GetWidth(), image.GetHeight());
	m_PrevStatic.ReleaseDC(lpMyDC);*/
	/*if( !m_Image.IsNull() ) {
		CDC * lpMyDC = m_RightView.GetDC();
		m_Image.Draw(lpMyDC->m_hDC, 0, 0);
		m_RightView.ReleaseDC(lpMyDC);
	}*/
}
//
// 得到当前发送码流Kbps...
DWORD CHaoYiView::GetCurSendKbps()
{
	if( m_lpStorageSession == NULL )
		return 0;
	return m_lpStorageSession->GetCurSendKbps();
}
//
// 得到当前发送文件名称...
LPCTSTR CHaoYiView::GetCurSendFile()
{
	if( m_lpStorageSession == NULL )
		return NULL;
	return m_lpStorageSession->GetCurSendFile();
}

void CHaoYiView::OnInitialUpdate()
{
	CFormView::OnInitialUpdate();

	this->BuildResource();
}

#ifdef _DEBUG
void CHaoYiView::AssertValid() const
{
	CFormView::AssertValid();
}

void CHaoYiView::Dump(CDumpContext& dc) const
{
	CFormView::Dump(dc);
}

CHaoYiDoc* CHaoYiView::GetDocument() const // 非调试版本是内联的
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CHaoYiDoc)));
	return (CHaoYiDoc*)m_pDocument;
}
#endif //_DEBUG

//
// 右键点击左侧树节点...
void CHaoYiView::OnRclickTreeDevice(NMHDR *pNMHDR, LRESULT *pResult)
{
	*pResult = 0;
}
//
// 左侧树节点发生变化 => 这里处理会发生不断重复循环的问题 => 获取鼠标位置来解决...
void CHaoYiView::OnSelchangedTreeDevice(NMHDR *pNMHDR, LRESULT *pResult)
{
	*pResult = 0;
	HTREEITEM hHitItem = NULL;
	if( m_bTreeKeydown ) {
		// 是键盘方向键引发的变化...
		hHitItem = m_DeviceTree.GetSelectedItem();
		m_bTreeKeydown = false;
	} else {
		// 根据鼠标位置，计算当前点击的节点...
		CPoint ptPoint;
		::GetCursorPos(&ptPoint);
		m_DeviceTree.ScreenToClient(&ptPoint);
		hHitItem = m_DeviceTree.HitTest(ptPoint);
	}
	if( hHitItem == NULL || m_lpMidView == NULL )
		return;
	// 获取该节点的有效监控通道编号...
	ASSERT( hHitItem != NULL && m_lpMidView != NULL );
	int nDBCameraID = m_DeviceTree.GetItemData(hHitItem);
	if( nDBCameraID <= 0 )
		return;
	// 通知中心窗口、右侧窗口，焦点发生了变化...
	// 必须先保存焦点，否则右侧显示混乱...
	m_nFocusDBCamera = nDBCameraID;
	m_lpMidView->doLeftFocus(nDBCameraID);
	m_RightView.doFocusDBCamera(nDBCameraID);
}
//
// 响应键盘事件...
void CHaoYiView::OnKeydownTreeDevice(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMTVKEYDOWN pTVKeyDown = reinterpret_cast<LPNMTVKEYDOWN>(pNMHDR);
	if( pTVKeyDown->wVKey == VK_UP || pTVKeyDown->wVKey == VK_DOWN || 
		pTVKeyDown->wVKey == VK_LEFT || pTVKeyDown->wVKey == VK_RIGHT ) {
		m_bTreeKeydown = true;
	}
	*pResult = 0;
}
//
// 响应监控通道被创建的事件...
void CHaoYiView::OnCreateCamera(int nDBCameraID, CString & strTitle)
{
	ASSERT( m_hRootItem != NULL );
	HTREEITEM hItemTree = NULL;
	// 创建左侧树状节点，并将通道编号记录起来...
	hItemTree = m_DeviceTree.InsertItem(strTitle, 1, 1, m_hRootItem);
	m_DeviceTree.SetItemData(hItemTree, nDBCameraID);
	m_DeviceTree.Expand(m_hRootItem,TVE_EXPAND);
	// 选中节点，保存焦点编号, 通知中间视图、右侧视图焦点变化...
	// 必须先保存焦点，否则右侧窗口状态显示混乱...
	if( m_nFocusDBCamera <= 0 && m_lpMidView != NULL ) {
		m_nFocusDBCamera = nDBCameraID;
		m_DeviceTree.SelectItem(hItemTree);
		m_lpMidView->doLeftFocus(nDBCameraID);
		m_RightView.doFocusDBCamera(nDBCameraID);
	}
}
//
// 响应监控通道窗口的焦点事件...
LRESULT CHaoYiView::OnMsgFocusVideo(WPARAM wParam, LPARAM lParam)
{
	// 通知左侧树状控件焦点变化...
	int nDBFocusID = wParam;
	ASSERT( m_hRootItem != NULL );
	HTREEITEM hChildItem = m_DeviceTree.GetChildItem(m_hRootItem);
	while( hChildItem != NULL ) {
		// 如果编号一致，选中这个节点...
		if( nDBFocusID == m_DeviceTree.GetItemData(hChildItem) ) {
			m_DeviceTree.SelectItem(hChildItem);
			break;
		}
		// 如果不是，继续寻找下一个节点...
		hChildItem = m_DeviceTree.GetNextSiblingItem(hChildItem);
	}
	// 保存视频焦点窗口编号，通知右侧视图焦点变化...
	// 必须先保存焦点，否则右侧显示混乱...
	m_nFocusDBCamera = nDBFocusID;
	m_RightView.doFocusDBCamera(nDBFocusID);
	return S_OK;
}

BOOL CHaoYiView::GetMacIPAddr()
{
	// 这里分配空间很重要，需要多分配一些...
	DWORD buflen = sizeof(IP_ADAPTER_INFO) * 3;
	IP_ADAPTER_INFO * lpAdapter = new IP_ADAPTER_INFO[3];
	DWORD status = GetAdaptersInfo(lpAdapter, &buflen);
	if( status != ERROR_SUCCESS ) {
		delete [] lpAdapter;
		return false;
	}
	// 开始循环遍历网卡节点...
	IP_ADAPTER_INFO * lpInfo = lpAdapter;
	while( lpInfo != NULL ) {
		// 必须是以太网卡，必须是IPV4的网卡，必须是有效的网卡...
		if( lpInfo->Type != MIB_IF_TYPE_ETHERNET || lpInfo->AddressLength != 6 || strcmp(lpInfo->IpAddressList.IpAddress.String, "0.0.0.0") == 0 ) {
			lpInfo = lpInfo->Next;
			continue;
		}
		// 获取MAC地址和IP地址，地址是相互关联的...
		m_strMacAddr.Format("%02X-%02X-%02X-%02X-%02X-%02X", lpInfo->Address[0], lpInfo->Address[1], lpInfo->Address[2], lpInfo->Address[3], lpInfo->Address[4], lpInfo->Address[5]);
		m_strIPAddr.Format("%s", lpInfo->IpAddressList.IpAddress.String);
		lpInfo = lpInfo->Next;
	}
	// 释放分配的缓冲区...
	delete [] lpAdapter;
	lpAdapter = NULL;
	// 另一种获取IP地址的方法 => MAC地址与IP地址不一定相互关联...
	/*if( SocketUtils::GetNumIPAddrs() > 1 ) {
		StrPtrLen * lpAddr = SocketUtils::GetIPAddrStr(0);
		m_strIPAddr = lpAddr->Ptr;
	}*/
	return true;
}

void CHaoYiView::BuildResource()
{
	this->DestroyResource();

	// 获取本机MAC地址...
	if( !this->GetMacIPAddr() ) {
		MsgLogGM(GM_Net_Err);
	}

	// 创建图片列表对象...
	CBitmap bitTemp;
	m_ImageList.Create(25, 23, ILC_COLORDDB |ILC_MASK, 8, 8);
	bitTemp.LoadBitmap(IDB_BITMAP_CAMERA);
	m_ImageList.Add(&bitTemp, RGB(252,2,252));
	bitTemp.DeleteObject();

	// 设置设备树的背景色/文字色/图片对象...
	m_DeviceTree.SetBkColor(WND_BK_COLOR);
	m_DeviceTree.SetTextColor(RGB(255, 255, 255));
	m_DeviceTree.SetImageList(&m_ImageList, LVSIL_NORMAL);

	// 修改设备树的属性...
	m_DeviceTree.ModifyStyle(NULL,  WS_VISIBLE | WS_TABSTOP | WS_CHILD //| WS_BORDER
		| TVS_HASBUTTONS | TVS_LINESATROOT | TVS_HASLINES | TVS_SHOWSELALWAYS   
		| TVS_DISABLEDRAGDROP|TVS_FULLROWSELECT|TVS_TRACKSELECT|TVS_EX_AUTOHSCROLL);

	// 初始化设备树的根节点...
	m_DeviceTree.DeleteAllItems();
	m_hRootItem	= m_DeviceTree.InsertItem("通道设备列表", 0, 0);		
	m_DeviceTree.Expand(m_hRootItem,TVE_EXPAND);
	m_DeviceTree.SelectItem(m_hRootItem);

	// 创建设备树的提示对象...
	//m_tipTree.Create(this);
	//m_tipTree.AddTool(&m_DeviceTree, "点鼠标右键可以弹出操作菜单");
	//m_tipTree.SetDelayTime(TTDT_INITIAL, 100);

	// 创建中间窗口对象...
	ASSERT( m_lpMidView == NULL );
	m_lpMidView = new CMidView(this);
	m_lpMidView->Create(WS_CHILD|WS_VISIBLE, CRect(0, 0, 0, 0), this);
	ASSERT( m_lpMidView->m_hWnd != NULL );

	// 初始化右侧窗口...
	m_RightView.InitButton(m_lpMidView->GetVideoFont());

	// 创建并启动一个网站交互线程...
	GM_Error theErr = GM_NoErr;
	m_lpWebThread = new CWebThread(this);
	theErr = m_lpWebThread->Initialize();
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
		return;
	}
	// 不能直接启动其它资源，必须通过网络交互线程来启动...
}
//
// 网站授权验证结果...
LRESULT CHaoYiView::OnMsgWebAuthResult(WPARAM wParam, LPARAM lParam)
{
	ASSERT( m_lpMidView != NULL );
	m_lpMidView->OnWebAuthResult(wParam, lParam);
	return S_OK;
}
//
// 更新主窗口标题名称...
void CHaoYiView::doUpdateFrameTitle()
{
	CString strTitle, strWebType;
	CMainFrame * lpFrame = (CMainFrame*)AfxGetMainWnd();
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	string & strMainName = theConfig.GetMainName();
	string & strWebName = theConfig.GetWebName();
	switch( theConfig.GetWebType() )
	{
	case kCloudRecorder: strWebType = "云录播";	break;
	case kCloudMonitor:	 strWebType = "云监控";	break;
	default:			 strWebType = "未知";	break;
	}
	strTitle.Format("%s - %s", strWebType, strMainName.c_str());
	lpFrame->SetWindowText(strTitle);
}
//
// 网站线程加载的资源...
LRESULT CHaoYiView::OnMsgWebLoadResource(WPARAM wParam, LPARAM lParam)
{
	// 更新主窗口标题栏名称...
	this->doUpdateFrameTitle();
	// 根据网站配置建立通道...
	ASSERT( m_lpMidView != NULL );
	m_lpMidView->BuildVideoByWeb();
	// 启动频道查询线程...
	GM_Error theErr = GM_NoErr;
	m_lpHKUdpThread = new CHKUdpThread(this);
	theErr = m_lpHKUdpThread->InitMulticast();
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
		return S_OK;
	}
	// 启动fastdfs会话管理线程...
	m_lpFastThread = new CFastThread(this->m_hWnd);
	theErr = m_lpFastThread->Initialize();
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
		return S_OK;
	}
	// 这里需要用一个Timer来处理 => 不断尝试创建Tracker对象，自动链接DVR，自动检测录像课程表...
	this->SetTimer(kTimerCheckFDFS, 5 * 1000, NULL);
	this->SetTimer(kTimerCheckDVR, 2 * 1000, NULL);
	this->SetTimer(kTimerAnimateDVR, 1 * 1000, NULL);
	this->SetTimer(kTimerCheckCourse, 500, NULL);
	this->SetTimer(kTimerWebGatherConfig, 180 * 1000, NULL);
	return S_OK;
}

void CHaoYiView::OnTimer(UINT_PTR nIDEvent)
{
	switch( nIDEvent )
	{
	case kTimerCheckFDFS:		this->doCheckFDFS();		break;
	case kTimerCheckDVR:		this->doCheckDVR();			break;
	case kTimerAnimateDVR:		this->doAnimateDVR();		break;
	case kTimerCheckCourse:		this->doCheckCourse();		break;
	case kTimerWebGatherConfig: this->doWebGatherConfig();	break;
	default:					/*-- do nothing --*/		break;
	}
	CFormView::OnTimer(nIDEvent);
}
//
// 处理有关课程记录发生变化的情况...
// 都是单个记录 => 添加(1) | 修改(2) | 删除(3)...
void CHaoYiView::doCourseChanged(int nOperateID, int nDBCameraID, GM_MapData & inData)
{
	// 首先，进行数据的有效性验证...
	if( nOperateID < kAddCourse || nOperateID > kDelCourse )
		return;
	if( inData.find("course_id") == inData.end() )
		return;
	int nCourseID = atoi(inData["course_id"].c_str());
	// 从系统配置对象中获取已有的录像课程表，获取引用，需要进行互斥保护...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	OSMutexLocker theLock(&theConfig.m_MutexCourse);
	// 这里直接用本地编号得到课表集合，如果没有会自动创建一个新的...
	GM_MapCourse & theCourse = theConfig.m_MapNodeCourse[nDBCameraID];
	if( nOperateID == kDelCourse ) {
		// 先对课表的录像任务进行停止，再删除 => 内部自动判断是否能够停止录像...
		if( theCourse.find(nCourseID) != theCourse.end() ) {
			this->doRecStopCourse(nDBCameraID, nCourseID);
			theCourse.erase(nCourseID);
		}
	} else {
		// 添加和修改都是需要直接存入集合当中 => 这里不需要判断是否需要停止录像，下面的任务调度会自动处理...
		ASSERT((nOperateID == kAddCourse) || (nOperateID == kModCourse));
		theCourse[nCourseID] = inData;
	}
}
//
// 获取只有时间的秒数，不含日期...
time_t CHaoYiView::GetTimeSecond(time_t inTime)
{
	CTime theCTime(inTime);
	return (theCTime.GetHour()*3600 + theCTime.GetMinute()*60 + theCTime.GetSecond());
}
//
// 每隔半秒检测录像课程表...
void CHaoYiView::doCheckCourse()
{
	// 获取当前日期是星期几...
	int nCurDayOfWeek = CTime::GetCurrentTime().GetDayOfWeek() - 1;
	// 从系统配置对象中获取已有的录像课程表，获取引用，需要进行互斥保护...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	OSMutexLocker theLock(&theConfig.m_MutexCourse);
	// 获取课程列表的引用对象，可能会修改课程内容...
	GM_MapNodeCourse & theNodeCourse = theConfig.GetNodeCourse();
	GM_MapNodeCourse::iterator itorNode;
	// 开始遍历以摄像头(通道)为基准的列表...
	for(itorNode = theNodeCourse.begin(); itorNode != theNodeCourse.end(); ++itorNode) {
		int nCameraID = itorNode->first;
		GM_MapCourse & theCourse = itorNode->second;
		GM_MapCourse::iterator itorCourse;
		// 遍历该通道下的课程记录...
		for(itorCourse = theCourse.begin(); itorCourse != theCourse.end(); ++itorCourse) {
			int nCourseID = itorCourse->first;
			GM_MapData & theData = itorCourse->second;
			time_t  tNow	 = this->GetTimeSecond(::time(NULL));
			time_t  tStart	 = this->GetTimeSecond((time_t)atoi(theData["start_time"].c_str()));
			time_t  tEnd	 = this->GetTimeSecond((time_t)atoi(theData["end_time"].c_str()));
			time_t  tDur	 = (time_t)atoi(theData["elapse_sec"].c_str());
			int		nWeekID  = atoi(theData["week_id"].c_str());
			int     nRepMode = atoi(theData["repeat_id"].c_str());
			ASSERT( nCourseID == atoi(theData["course_id"].c_str()) );
			//ASSERT( nRepMode == kWeekRepeat );
			//ASSERT( tEnd == (tStart + tDur) );
			// 当前星期与记录里的星期不一致 => 用当前记录编号去停止任务记录，避免0点跳变...
			if( nWeekID != nCurDayOfWeek ) {
				this->doRecStopCourse(nCameraID, nCourseID);
				continue;
			}
			// 当前时间 < 开始时间 => 时间未到，直接停止正在录像的当前任务记录，因为有可能是修改过时间的记录...
			if( tNow < tStart ) {
				this->doRecStopCourse(nCameraID, nCourseID);
				continue;
			}
			// 当前时间 >= 结束时间 => 通知任务结束，如果当前运行记录不是它，不处理，直接返回...
			if( tNow >= tEnd ) {
				// 停止当前通道中正在录像的当前任务记录...
				this->doRecStopCourse(nCameraID, nCourseID);
				///////////////////////////////////////////////////////////////////////////////
				// 2017.07.28 - by jackey => 固定模式每周重复，而且只比较时间，所以不用处理了...
				///////////////////////////////////////////////////////////////////////////////
				// 无重复模式 => 直接检测下一条记录 => 不要进行删除操作，因为可能会进行修改...
				/*if( nRepMode == kNoneRepeat )
					continue;
				ASSERT( nRepMode == kDayRepeat || nRepMode == kWeekRepeat );
				// 每天重复 => 累加1*24小时...
				if( nRepMode == kDayRepeat ) {
					tStart += 1 * 24 * 3600;
					tEnd    = tStart + tDur;
				} else if( nRepMode == kWeekRepeat ) {
					// 每周重复 => 累加7*24小时...
					tStart += 7 * 24 * 3600;
					tEnd    = tStart + tDur;
				}
				// 转换开始时间...
				TCHAR szBuf[MAX_PATH/2] = {0};
				sprintf(szBuf, "%lu", tStart);
				theData["start_time"] = szBuf;
				// 转换结束时间...
				sprintf(szBuf, "%lu", tEnd);
				theData["end_time"] = szBuf;*/
				// 继续下一个任务...
				continue;
			}
			// 当前时间一定大于开始时间，立即开始录像...
			ASSERT(tNow >= tStart);
			// 开启当前通道的任务录像 => 内部会自动判断当前记录编号是否合法有效...
			this->doRecStartCourse(nCameraID, nCourseID);
		}
	}
}
//
// 启动摄像头的课程任务录像...
void CHaoYiView::doRecStartCourse(int nDBCameraID, int nCourseID)
{
	CCamera * lpCamera = this->FindDBCameraByID(nDBCameraID);
	if( lpCamera == NULL )
		return;
	// 如果当前通道正在录像的记录与当前记录一致，直接返回...
	if( lpCamera->GetRecCourseID() == nCourseID )
		return;
	lpCamera->doRecStartCourse(nCourseID);
}
//
// 停止摄像头的课程任务录像...
void CHaoYiView::doRecStopCourse(int nDBCameraID, int nCourseID)
{
	CCamera * lpCamera = this->FindDBCameraByID(nDBCameraID);
	if( lpCamera == NULL )
		return;
	// 如果当前通道正在录像的记录与当前记录不一致，直接返回...
	if( lpCamera->GetRecCourseID() != nCourseID )
		return;
	lpCamera->doRecStopCourse(nCourseID);
}
//
// 每隔1秒显示DVR动画信息...
void CHaoYiView::doAnimateDVR()
{
	// 遍历所有的子节点 => DVR通道节点...
	HTREEITEM hChildItem = m_DeviceTree.GetChildItem(m_hRootItem);
	while( hChildItem != NULL ) {
		int nDBCameraID = m_DeviceTree.GetItemData(hChildItem);
		CCamera * lpCamera = this->FindDBCameraByID(nDBCameraID);
		// 通道有效，并且登录成功，设置动画图标...
		if( lpCamera != NULL && lpCamera->IsLogin() ) {
			m_DeviceTree.SetItemImage(hChildItem, m_nAnimateIndex + 2, m_nAnimateIndex + 2);
		} else {
			// 通道没有登录，恢复通道图标状态...
			int nImage = 0, nSelImage = 0;
			m_DeviceTree.GetItemImage(hChildItem, nImage, nSelImage);
			if( nImage != 1 ) {
				m_DeviceTree.SetItemImage(hChildItem, 1, 1);
			}
		}
		// 遍历下一个通道节点...
		hChildItem = m_DeviceTree.GetNextSiblingItem(hChildItem);
	}
	// 总共四个动画，动画编号累加...
	m_nAnimateIndex = (++m_nAnimateIndex) % 4;
}
//
// 通过数据库编号获取摄像头状态信息...
int CHaoYiView::GetDBCameraStatusByID(int nDBCameraID)
{
	// 通过本地编号查找摄像头对象...
	int nStatus = kCameraWait;
	CCamera * lpCamera = this->FindDBCameraByID(nDBCameraID);
	if( lpCamera == NULL )
		return nStatus;
	// 获取摄像机的当前运行状态...
	if( lpCamera->IsRecording() ) {
		nStatus = kCameraRec;
	} else if( lpCamera->IsPlaying() ) {
		nStatus = kCameraRun;
	}
	// 最后返回运行状态...
	return nStatus;
}
//
// 每隔5秒检测一次Tracker和Storage对象是否被创建...
void CHaoYiView::doCheckFDFS()
{
	this->doCheckTracker();
	this->doCheckStorage();
	this->doCheckTransmit();
}
//
// 自动检测并创建RemoteSession...
void CHaoYiView::doCheckTransmit()
{
	if( m_lpRemoteSession != NULL )
		return;
	ASSERT( m_lpRemoteSession == NULL );
	// 检查中转服务器地址是否有效...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	string & strRemoteAddr = theConfig.GetRemoteAddr();
	int nRemotePort = theConfig.GetRemotePort();
	if( strRemoteAddr.size() <= 0 || nRemotePort <= 0 )
		return;
	// 获取Transmit配置，创建Remote对象...
	ASSERT( strRemoteAddr.size() > 0 && nRemotePort > 0 );
	m_lpRemoteSession = new CRemoteSession(this);
	GM_Error theErr = GM_NoErr;
	do {
		// 创建Remote会话对象...
		theErr = m_lpRemoteSession->InitSession(strRemoteAddr.c_str(), nRemotePort);
		if( theErr != GM_NoErr ) {
			MsgLogGM(theErr);
			break;
		}
		// 加入到事件等待线程当中...
		theErr = this->AddToEventThread(m_lpRemoteSession);
		if( theErr != GM_NoErr ) {
			MsgLogGM(theErr);
			break;
		}
	}while( false );
	// 正确执行，直接返回...
	if( theErr == GM_NoErr )
		return;
	// 执行错误，删除，等待重建...
	ASSERT( theErr != GM_NoErr );
	if( m_lpRemoteSession != NULL ) {
		delete m_lpRemoteSession;
		m_lpRemoteSession = NULL;
	}
}
//
// 转发延时命令到指定的直播播放器链接...
/*GM_Error CHaoYiView::doTransmitPlayer(int nPlayerSock, string & strRtmpUrl, GM_Error inErr)
{
	if( m_lpRemoteSession == NULL )
		return GM_NoErr;
	return m_lpRemoteSession->doTransmitPlayer(nPlayerSock, strRtmpUrl, inErr);
}*/
//
// 自动检测并创建TrackerSession...
void CHaoYiView::doCheckTracker()
{
	if( m_lpTrackerSession != NULL )
		return;
	ASSERT( m_lpTrackerSession == NULL );
	// 检查是否允许自动连接Tracker服务器...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	string & strAddr = theConfig.GetTrackerAddr();
	int nPort = theConfig.GetTrackerPort();
	if( !theConfig.GetAutoLinkFDFS() || strAddr.size() <= 0 || nPort <= 0 )
		return;
	// 获取Tracker配置，创建Tracker对象...
	ASSERT( strAddr.size() > 0 && nPort > 0 );
	ASSERT( theConfig.GetAutoLinkFDFS() );
	m_lpTrackerSession = new CTrackerSession();
	GM_Error theErr = GM_NoErr;
	do {
		// 创建Tracker会话对象，获取Storage地址...
		theErr = m_lpTrackerSession->InitSession(strAddr.c_str(), nPort);
		if( theErr != GM_NoErr ) {
			MsgLogGM(theErr);
			break;
		}
		// 加入到事件等待线程当中...
		theErr = this->AddToEventThread(m_lpTrackerSession);
		if( theErr != GM_NoErr ) {
			MsgLogGM(theErr);
			break;
		}
	}while( false );
	// 正确执行，直接返回...
	if( theErr == GM_NoErr )
		return;
	// 执行错误，删除，等待重建...
	ASSERT( theErr != GM_NoErr );
	if( m_lpTrackerSession != NULL ) {
		delete m_lpTrackerSession;
		m_lpTrackerSession = NULL;
	}
}
//
// 自动检测并创建StorageSession...
void CHaoYiView::doCheckStorage()
{
	// 首先判断一些参数的有效性...
	if( m_lpTrackerSession == NULL || m_lpStorageSession != NULL )
		return;
	ASSERT( m_lpTrackerSession != NULL && m_lpStorageSession == NULL );
	StorageServer & theStorage = m_lpTrackerSession->GetStorageServer();
	if( !m_lpTrackerSession->IsConnected() || theStorage.port <= 0 )
		return;
	ASSERT( m_lpTrackerSession->IsConnected() && theStorage.port > 0 );
	// 如果录像目录下没有需要上传的文件，直接返回，继续等待...
	if( !this->IsHasUploadFile() )
		return;
	// 创建Storage会话对象，根据Storage地址...
	GM_Error theErr = GM_NoErr;
	m_lpStorageSession = new CStorageSession(&theStorage);
	do {
		// 创建Storage对象会话...
		theErr = m_lpStorageSession->InitSession(theStorage.ip_addr, theStorage.port);
		if( theErr != GM_NoErr ) {
			MsgLogGM(theErr);
			break;
		}
		// 加入到事件等待线程当中...
		theErr = this->AddToEventThread(m_lpStorageSession);
		if( theErr != GM_NoErr ) {
			MsgLogGM(theErr);
			break;
		}
	}while( false );
	// 正确执行，直接返回...
	if( theErr == GM_NoErr )
		return;
	// 执行错误，删除，等待重建...
	ASSERT( theErr != GM_NoErr );
	if( m_lpStorageSession != NULL ) {
		delete m_lpStorageSession;
		m_lpStorageSession = NULL;
	}
}
//
// 是否有需要上传文件...
BOOL CHaoYiView::IsHasUploadFile()
{
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	string & strSavePath = theConfig.GetSavePath();
	string   strRootPath = strSavePath;
	if( strRootPath.size() <= 0 )
		return false;
	// 路径是否需要加上反斜杠...
	BOOL bHasSlash = false;
	bHasSlash = ((strRootPath.at(strRootPath.size()-1) == '\\') ? true : false);
	strRootPath += (bHasSlash ? "*.*" : "\\*.*");
	// 准备查找需要的变量...
	GM_Error theErr = GM_NoErr;
	WIN32_FIND_DATA theFileData = {0};
	HANDLE	hFind = INVALID_HANDLE_VALUE;
	LPCTSTR	lpszExt = NULL;
	BOOL	bIsOK = true;
	CString strFileName;
	// 查找第一个文件或目录...
	hFind = ::FindFirstFile(strRootPath.c_str(), &theFileData);
	if( hFind != INVALID_HANDLE_VALUE ) {
		while( bIsOK ) {
			if(	(theFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||	// 目录
				(theFileData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) ||		// 系统
				(strcmp(theFileData.cFileName, ".") == 0) ||					// 当前
				(strcmp(theFileData.cFileName, "..") == 0) )					// 上级
			{
				bIsOK = ::FindNextFile(hFind, &theFileData);
				continue;
			}
			// 判断扩展名是否有效 => .jpg .jpeg .mp4 有效...
			lpszExt = ::PathFindExtension(theFileData.cFileName);
			if( (stricmp(lpszExt, ".jpg") != 0) && (stricmp(lpszExt, ".jpeg") != 0) && (stricmp(lpszExt, ".mp4") != 0) ) {
				bIsOK = ::FindNextFile(hFind, &theFileData);
				continue;
			}
			// 首先，组合有效的文件全路径，获取文件长度...
			strFileName.Format(bHasSlash ? "%s%s" : "%s\\%s", strSavePath.c_str(), theFileData.cFileName);
			ULONGLONG llSize = CUtilTool::GetFileFullSize(strFileName);
			// 文件长度为0，查找下一个文件...
			if( llSize <= 0 ) {
				bIsOK = ::FindNextFile(hFind, &theFileData);
				continue;
			}
			// 文件类型和长度都是有效的，关闭句柄，直接返回...
			::FindClose(hFind);
			return true;
		}
	}
	// 如果查找句柄不为空，直接关闭之...
	if( hFind != INVALID_HANDLE_VALUE ) {
		::FindClose(hFind);
	}
	return false;
}
//
// 每隔一秒钟尝试连接DVR...
void CHaoYiView::doCheckDVR()
{
	// 检查是否允许自动连接DVR服务器...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	if( !theConfig.GetAutoLinkDVR() )
		return;
	ASSERT( theConfig.GetAutoLinkDVR() );
	// 直接调用中间视图，进行自动重连...
	if( m_RightView.m_hWnd != NULL ) {
		m_RightView.doAutoCheckDVR();
	}
}
//
// 得到下一个窗口编号...
int CHaoYiView::GetNextAutoID(int nCurDBCameraID)
{
	return ((m_lpMidView != NULL) ? m_lpMidView->GetNextAutoID(nCurDBCameraID) : DEF_CAMERA_START_ID);
}
//
// 改变自动连接DVR的时间周期...
/*void CHaoYiView::doChangeAutoDVR(DWORD dwErrCode)
{
	// 检查是否允许自动连接DVR服务器...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	if( !theConfig.GetAutoLinkDVR() )
		return;
	ASSERT( theConfig.GetAutoLinkDVR() );
	this->KillTimer(kTimerCheckDVR);
	// 如果是密码错误，或者登录被锁定，直接返回...
	if( dwErrCode == NET_DVR_PASSWORD_ERROR || dwErrCode == NET_DVR_USER_LOCKED )
		return;
	// 其它错误，增大自动连接周期...
	this->SetTimer(kTimerCheckDVR, 2 * 10 * 1000, NULL);
}*/

void CHaoYiView::DestroyResource()
{
	// 停止自动登录时钟...
	this->KillTimer(kTimerCheckFDFS);
	this->KillTimer(kTimerCheckDVR);
	this->KillTimer(kTimerCheckCourse);
	this->KillTimer(kTimerAnimateDVR);
	this->KillTimer(kTimerWebGatherConfig);
	// 释放web线程...
	if( m_lpWebThread != NULL ) {
		delete m_lpWebThread;
		m_lpWebThread = NULL;
	}
	// 删除中间视图...
	if( m_lpMidView != NULL ) {
		delete m_lpMidView;
		m_lpMidView = NULL;
	}
	// 销毁右侧窗口里的按钮对象...
	m_RightView.DestoryAllButton();
	// 删除左侧节点内容...
	m_DeviceTree.DeleteAllItems();
	m_ImageList.DeleteImageList();
	// 释放频道查询线程...
	if( m_lpHKUdpThread != NULL ) {
		delete m_lpHKUdpThread;
		m_lpHKUdpThread = NULL;
	}
	// 首先，释放fastdfs会话管理线程...
	this->ClearFastThreads();
	// 然后，释放所有的会话对象资源...
	this->ClearFastSession();
	// 重置一些全局变量...
	m_bTreeKeydown = false;
	m_nFocusDBCamera = 0;
	m_nAnimateIndex = 0;
	m_hRootItem = NULL;
}
//
// 释放会话管理线程...
void CHaoYiView::ClearFastThreads()
{
	// 释放FastDFS会话管理线程...
	if( m_lpFastThread != NULL ) {
		delete m_lpFastThread;
		m_lpFastThread = NULL;
	}
}
//
// 释放所有的会员资源...
void CHaoYiView::ClearFastSession()
{
	// 释放远程会话对象...
	if( m_lpRemoteSession != NULL ) {
		delete m_lpRemoteSession;
		m_lpRemoteSession = NULL;
	}
	// 释放Tracker会话对象...
	if( m_lpTrackerSession != NULL ) {
		delete m_lpTrackerSession;
		m_lpTrackerSession = NULL;
	}
	// 释放Storage会话对象...
	if( m_lpStorageSession != NULL ) {
		delete m_lpStorageSession;
		m_lpStorageSession = NULL;
	}
}
//
// 将事件会话加入到事件线程中进行管理...
GM_Error CHaoYiView::AddToEventThread(CFastSession * lpSession)
{
	if( m_lpFastThread == NULL || lpSession == NULL )
		return GM_NoErr;
	// 将会话加入到事件线程当中...
	ASSERT( m_lpFastThread != NULL && lpSession != NULL );
	return m_lpFastThread->AddSession(lpSession);
}
//
// 通过线程接口来删除会话对象,避免资源冲突...
GM_Error CHaoYiView::DelByEventThread(CFastSession * lpSession)
{
	if( m_lpFastThread == NULL || lpSession == NULL )
		return GM_NoErr;
	// 直接调用线程接口，删除指定的会话对象...
	ASSERT( m_lpFastThread != NULL && lpSession != NULL );
	return m_lpFastThread->DelSession(lpSession);
}
//
// 响应事件会话发送的通知消息...
LRESULT	CHaoYiView::OnMsgEventSession(WPARAM wParam, LPARAM lParam)
{
	switch(wParam)
	{
	case OPT_DelSession:  this->OnOptDelSession(lParam);  break;
	default:			  /*-- do nothing --*/			  break;
	}	
	return S_OK;
}
//
// 响应删除操作 => CFastSession * 
void CHaoYiView::OnOptDelSession(ULONG inUniqueID)
{
	if( inUniqueID == NULL )
		return;
	ASSERT( inUniqueID > 0 );
	// 获取会话的强制转换指针对象...
	CFastSession * lpSession = (CFastSession *)inUniqueID;
	GM_Error theErr = lpSession->GetErrorCode();
	MsgLogGM(theErr);
	// 会话已经在线程中被注销掉了，可以直接被删除...
	if( lpSession == m_lpTrackerSession ) {
		delete m_lpTrackerSession;
		m_lpTrackerSession = NULL;
	} else if( lpSession == m_lpStorageSession ) {
		delete m_lpStorageSession;
		m_lpStorageSession = NULL;
	} else if( lpSession == m_lpRemoteSession ) {
		delete m_lpRemoteSession;
		m_lpRemoteSession = NULL;
	}
}

void CHaoYiView::OnSize(UINT nType, int cx, int cy)
{
	CFormView::OnSize(nType, cx, cy);

	// 对控件进行位置移动操作...
	if( m_DeviceTree.m_hWnd == NULL || m_QRStatic.m_hWnd == NULL ) 
		return;
	// 显示二维码的模式...
	//m_DeviceTree.SetWindowPos(this, 0, 0, QR_CODE_CX, cy - QR_CODE_CY, SWP_NOZORDER | SWP_NOACTIVATE);
	//m_QRStatic.SetWindowPos(this, 0, cy - QR_CODE_CY, QR_CODE_CX, QR_CODE_CY, SWP_NOZORDER | SWP_NOACTIVATE);
	// 不显示二维码的模式...
	m_DeviceTree.SetWindowPos(this, 0, 0, QR_CODE_CX, cy - 0, SWP_NOZORDER | SWP_NOACTIVATE);
	m_QRStatic.SetWindowPos(this, 0, cy - 0, QR_CODE_CX, 0, SWP_NOZORDER | SWP_NOACTIVATE);
	m_RightView.SetWindowPos(this, cx - QR_CODE_CX, 0, QR_CODE_CX, cy, SWP_NOZORDER | SWP_NOACTIVATE);
	
	// 调整中间窗口的大小...
	if( m_lpMidView != NULL && m_lpMidView->m_hWnd != NULL ) {
		int nMidWidth = cx - QR_CODE_CX * 2 - kSplitterWidth * 2;
		CRect rectT(QR_CODE_CX + kSplitterWidth, 0, QR_CODE_CX + kSplitterWidth + nMidWidth, cy);
		m_lpMidView->doMoveWindow(rectT);
	}

	// 设置左侧拖动条位置...
	m_rcSplitLeftSrc.SetRect(QR_CODE_CX, 0, QR_CODE_CX + kSplitterWidth, cy);
	m_rcSplitLeftNew.SetRect(QR_CODE_CX, 0, QR_CODE_CX + kSplitterWidth, cy);

	// 设置右侧拖动条位置...
	m_rcSplitRightSrc.SetRect(cx - QR_CODE_CX - kSplitterWidth, 0, cx - QR_CODE_CX, cy);
	m_rcSplitRightNew.SetRect(cx - QR_CODE_CX - kSplitterWidth, 0, cx - QR_CODE_CX, cy);
}

BOOL CHaoYiView::OnEraseBkgnd(CDC* pDC)
{
	//return CFormView::OnEraseBkgnd(pDC);

	// 绘制分隔符...
	pDC->FillSolidRect(&m_rcSplitLeftSrc, RGB(224,224,224));
	pDC->FillSolidRect(&m_rcSplitRightSrc, RGB(224,224,224));

	return true;
}
//
// 绘制分割线..
void CHaoYiView::DrawSplitBar(LPRECT lpRect)
{
	CDC dc;
	dc.Attach(::GetDC(NULL));
	ASSERT( dc.GetSafeHdc() );
	
	CRect	rect(lpRect);
	this->ClientToScreen(rect);
	do {
		//TRACE("draw.left = %lu, draw.right = %lu\n", rect.left, rect.right);
		dc.DrawFocusRect(rect);
		rect.DeflateRect(1, 1);
	}while (rect.Size().cx > 0 && rect.Size().cy > 0);
	
	HDC hDC = dc.Detach();
	::ReleaseDC(NULL, hDC);
}
//
// 左侧分割结束事件...
void CHaoYiView::OnLeftSplitEnd(int nOffset)
{
	CRect	rectTemp, rcMain;

	// 移动设备树位置...
	m_DeviceTree.GetWindowRect(rectTemp);
	this->ScreenToClient(rectTemp);
	m_DeviceTree.SetWindowPos(NULL, rectTemp.left, rectTemp.top, rectTemp.Width() + nOffset, rectTemp.Height(), SWP_NOZORDER);
	m_DeviceTree.Invalidate(true);

	// 移动二维码位置...
	//m_QRStatic.GetWindowRect(rectTemp);
	//this->ScreenToClient(rectTemp);
	//m_QRStatic.SetWindowPos(NULL, rectTemp.left, rectTemp.top, rectTemp.Width() + nOffset, rectTemp.Height(), SWP_NOZORDER);
	//m_QRStatic.Invalidate(true);

	// 设置新的分割条位置...
	this->GetClientRect(rcMain);
	m_DeviceTree.GetWindowRect(rectTemp);
	this->ScreenToClient(rectTemp);
	
	m_rcSplitLeftSrc.SetRect(rectTemp.right, rectTemp.top, rectTemp.right + kSplitterWidth, rcMain.Height());
	m_rcSplitLeftNew.SetRect(rectTemp.right, rectTemp.top, rectTemp.right + kSplitterWidth, rcMain.Height());

	// 移动中间矩形位置...
	if( m_lpMidView != NULL && m_lpMidView->m_hWnd != NULL ) {
		CRect rcView(m_rcSplitLeftSrc.right, m_rcSplitLeftSrc.top, m_rcSplitRightSrc.left, m_rcSplitLeftSrc.bottom);
		m_lpMidView->doMoveWindow(rcView);
	}
}
//
// 左侧分割条能否移动...
BOOL CHaoYiView::IsLeftCanSplit(CPoint & point)
{
	return (( point.x < QR_CODE_CY || point.x > QR_CODE_CX * 2 ) ? false : true);
}
//
// 右侧分割条结束事件...
void CHaoYiView::OnRightSplitEnd(int nOffset)
{
	CRect	rectTemp, rcMain;

	// 移动右侧显示区...
	m_RightView.GetWindowRect(rectTemp);
	this->ScreenToClient(rectTemp);
	m_RightView.SetWindowPos(NULL, rectTemp.left + nOffset, rectTemp.top, rectTemp.Width() - nOffset, rectTemp.Height(), SWP_NOZORDER);
	m_RightView.Invalidate(true);

	// 计算右侧分割条位置...
	this->GetClientRect(rcMain);
	m_RightView.GetWindowRect(rectTemp);
	this->ScreenToClient(rectTemp);

	m_rcSplitRightSrc.SetRect(rectTemp.left - kSplitterWidth, rectTemp.top, rectTemp.left, rcMain.Height());
	m_rcSplitRightNew.SetRect(rectTemp.left - kSplitterWidth, rectTemp.top, rectTemp.left, rcMain.Height());

	// 移动中间显示区...
	if( m_lpMidView != NULL && m_lpMidView->m_hWnd != NULL ) {
		CRect rcView(m_rcSplitLeftSrc.right, m_rcSplitLeftSrc.top, m_rcSplitRightSrc.left, m_rcSplitLeftSrc.bottom);
		m_lpMidView->doMoveWindow(rcView);
	}
}
//
// 右侧分割条能否移动...
BOOL CHaoYiView::IsRightCanSplit(CPoint & point)
{
	CRect rcMain;
	this->GetClientRect(rcMain);
	return (( point.x > (rcMain.right - QR_CODE_CY) || point.x < (rcMain.right - QR_CODE_CX * 2) ) ? false : true);
}

void CHaoYiView::OnLButtonUp(UINT nFlags, CPoint point)
{
	if( m_bLeftDraging ) {
		::ReleaseCapture();
		this->DrawSplitBar(m_rcSplitLeftNew);
		this->DrawSplitBar(m_rcSplitLeftNew);
		this->OnLeftSplitEnd(m_rcSplitLeftNew.right - m_rcSplitLeftSrc.right);
		m_bLeftDraging = FALSE;
	}
	if( m_bRightDraging ) {
		::ReleaseCapture();
		this->DrawSplitBar(m_rcSplitRightNew);
		this->DrawSplitBar(m_rcSplitRightNew);
		this->OnRightSplitEnd(m_rcSplitRightNew.right - m_rcSplitRightSrc.right);
		m_bRightDraging = FALSE;
	}
	CFormView::OnLButtonUp(nFlags, point);
}

void CHaoYiView::OnLButtonDown(UINT nFlags, CPoint point)
{
	if( m_rcSplitLeftNew.PtInRect(point) ) {
		this->SetCapture();
		m_bLeftDraging = TRUE;
		::SetCursor(m_hCurHorSplit);
	}
	if( m_rcSplitRightNew.PtInRect(point) ) {
		this->SetCapture();
		m_bRightDraging = TRUE;
		::SetCursor(m_hCurHorSplit);
	}
	CFormView::OnLButtonDown(nFlags, point);
}

void CHaoYiView::OnMouseMove(UINT nFlags, CPoint point)
{
	// 左侧，是否需要设置拖动图标...
	if( m_rcSplitLeftNew.PtInRect(point) || m_bLeftDraging ) {
		::SetCursor(m_hCurHorSplit);
	}
	// 左侧，绘制虚线...
	if( m_bLeftDraging ) {
		if( !this->IsLeftCanSplit(point) )
			return;
		CRect rcDraw = m_rcSplitLeftNew;
		rcDraw.right = point.x + m_rcSplitLeftNew.Width()/2;
		rcDraw.left	 = rcDraw.right - m_rcSplitLeftNew.Width();
		this->DrawSplitBar(m_rcSplitLeftNew);
		this->DrawSplitBar(rcDraw);
		m_rcSplitLeftNew = rcDraw;
	}
	// 右侧，是否需要设置拖动图标...
	if( m_rcSplitRightNew.PtInRect(point) || m_bRightDraging ) {
		::SetCursor(m_hCurHorSplit);
	}
	// 右侧，绘制虚线...
	if( m_bRightDraging ) {
		if( !this->IsRightCanSplit(point) )
			return;
		CRect rcDraw = m_rcSplitRightNew;
		rcDraw.right = point.x - m_rcSplitRightNew.Width()/2;
		rcDraw.left	 = rcDraw.right - m_rcSplitRightNew.Width();
		this->DrawSplitBar(m_rcSplitRightNew);
		this->DrawSplitBar(rcDraw);
		m_rcSplitRightNew = rcDraw;
	}
	CFormView::OnMouseMove(nFlags, point);
}
//
// 查找视频通道...
CCamera * CHaoYiView::FindDBCameraByID(int nDBCameraID)
{
	return ((m_lpMidView != NULL ) ? m_lpMidView->FindDBCameraByID(nDBCameraID) : NULL);
}
//
// 处理发现摄像头的命令...
void CHaoYiView::doMulticastData(GM_MapData & inNetData, CAMERA_TYPE inType)
{
	// 数据互斥，并加数据加入队列，为了避免堵死...
	OSMutexLocker theLocker(&m_Mutex);
	if( inType == kCameraHK ) {
		// 处理发现海康摄像头的处理过程...
		m_HKListData.push_back(inNetData);
		this->PostMessage(WM_FIND_HK_CAMERA);
	} else if( inType == kCameraDH ) {
		// 处理发现大华摄像头的处理过程...
		//m_DHListData.push_back(inNetData);
		//this->PostMessage(WM_FIND_DH_CAMERA);
	}
}
//
// 响应找到海康的消息事件...
LRESULT	CHaoYiView::OnMsgFindHKCamera(WPARAM wParam, LPARAM lParam)
{
	// 数据互斥，取出一个数据内容...
	OSMutexLocker theLocker(&m_Mutex);
	GM_Error theErr = GM_NoErr;
	if( m_HKListData.size() < 0 || m_lpMidView == NULL )
		return S_OK;
	// 对数据进行处理...
	ASSERT( m_HKListData.size() > 0 && m_lpMidView != NULL );
	int outDBCameraID = 0;
	GM_MapData & theNetData = m_HKListData.front();
	theErr = m_lpMidView->doCameraUDPData(theNetData, kCameraHK, outDBCameraID);
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
	}
	// 将数据从队列中移除来...
	m_HKListData.pop_front();
	return S_OK;
}
//
// 向网站获取采集端配置...
BOOL CHaoYiView::doWebGatherConfig()
{
	if( m_lpWebThread == NULL )
		return false;
	return m_lpWebThread->doWebGatherConfig();
}
//
// 向网站注册摄像头...
BOOL CHaoYiView::doWebRegCamera(GM_MapData & inData)
{
	if( m_lpWebThread == NULL )
		return false;
	return m_lpWebThread->doWebRegCamera(inData);
}
//
// 向网站删除摄像头...
BOOL CHaoYiView::doWebDelCamera(string & inDeviceSN)
{
	if( m_lpWebThread == NULL )
		return false;
	return m_lpWebThread->doWebDelCamera(inDeviceSN);
}
//
// 响应 VK_Escape 系统快捷键...
void CHaoYiView::OnVKEscape()
{
	if( m_lpMidView != NULL ) {
		m_lpMidView->ChangeFullScreen(false);
	}
}
//
// 响应 VK_F 系统快捷键...
void CHaoYiView::OnVKFull()
{
	if( m_lpMidView != NULL ) {
		m_lpMidView->ChangeFullScreen(!m_lpMidView->IsFullScreen());
	}
}
//
// 更新 添加通道 菜单状态...
void CHaoYiView::OnCmdUpdateAddDVR(CCmdUI *pCmdUI)
{
	// 已经在网站注册成功之后才有效...
	pCmdUI->Enable(m_lpFastThread != NULL ? true : false);
}
//
// 更新 修改通道 菜单状态...
void CHaoYiView::OnCmdUpdateModDVR(CCmdUI *pCmdUI)
{
	// 通过当前焦点编号查找DVR对象...
	CCamera * lpCamera = this->FindDBCameraByID(m_nFocusDBCamera);
	if( lpCamera == NULL ) {
		pCmdUI->Enable(false);
		return;
	}
	// 设置菜单状态...
	pCmdUI->Enable(true);
}
//
// 更新 删除通道 菜单状态...
void CHaoYiView::OnCmdUpdateDelDVR(CCmdUI *pCmdUI)
{
	// 通过当前焦点编号查找DVR对象...
	CCamera * lpCamera = this->FindDBCameraByID(m_nFocusDBCamera);
	//if( lpCamera == NULL || lpCamera->IsCameraDevice() ) {
	//2017.10.31 - 所有类型都可以被删除...
	if( lpCamera == NULL ) {
		pCmdUI->Enable(false);
		return;
	}
	// 设置菜单状态 => 一定是流转发类型...
	//ASSERT( !lpCamera->IsCameraDevice() );
	pCmdUI->Enable(true);
}
//
// 更新 登录通道 菜单状态...
void CHaoYiView::OnCmdUpdateLoginDVR(CCmdUI *pCmdUI)
{
	// 通过当前焦点编号查找DVR对象...
	CCamera * lpCamera = this->FindDBCameraByID(m_nFocusDBCamera);
	if( lpCamera == NULL ) {
		pCmdUI->Enable(false);
		return;
	}
	// 设置菜单状态 => 与登录状态相反...
	pCmdUI->Enable(!lpCamera->IsLogin());
}
//
// 更新 注销通道 菜单状态...
void CHaoYiView::OnCmdUpdateLogoutDVR(CCmdUI *pCmdUI)
{
	// 通过当前焦点编号查找DVR对象...
	CCamera * lpCamera = this->FindDBCameraByID(m_nFocusDBCamera);
	if( lpCamera == NULL ) {
		pCmdUI->Enable(false);
		return;
	}
	// 设置菜单状态 => 与登录状态相同...
	pCmdUI->Enable(lpCamera->IsLogin());
}
//
// 点击菜单 => 登录通道...
void CHaoYiView::OnLoginDVR()
{
	// 根据焦点获取对象...
	CCamera * lpCamera = this->FindDBCameraByID(m_nFocusDBCamera);
	if( lpCamera == NULL || lpCamera->IsLogin() )
		return;
	// 处理流转发模式的登录...
	GM_Error theErr = GM_NoErr;
	if( !lpCamera->IsCameraDevice() ) {
		theErr = lpCamera->doStreamLogin();
		if( theErr != GM_NoErr ) {
			MsgLogGM(theErr);
		}
		return;
	}
	// 处理摄像头模式的登录...
	ASSERT( lpCamera->IsCameraDevice() );
	if( m_RightView.m_hWnd != NULL ) {
		m_RightView.doDeviceLogin();
	}
}
//
// 点击菜单 => 注销通道...
void CHaoYiView::OnLogoutDVR()
{
	// 根据焦点获取对象...
	CCamera * lpCamera = this->FindDBCameraByID(m_nFocusDBCamera);
	if( lpCamera == NULL )
		return;
	// 处理流转发模式的注销...
	GM_Error theErr = GM_NoErr;
	if( !lpCamera->IsCameraDevice() ) {
		theErr = lpCamera->doStreamLogout();
		if( theErr != GM_NoErr ) {
			MsgLogGM(theErr);
		}
		return;
	}
	// 处理摄像头模式的登录...
	ASSERT( lpCamera->IsCameraDevice() );
	if( m_RightView.m_hWnd != NULL ) {
		m_RightView.doDeviceLogout();
	}
}
//
// 点击菜单 => 添加通道...
void CHaoYiView::OnAddDVR()
{
	// 需要设置默认的通道名称...
	CDlgPushDVR	dlg(false, -1, false, this);
	dlg.m_strDVRName = m_lpMidView->GetDefaultCameraName().c_str();
	// 显示添加对话框 => 流转发模式...
	if( IDOK != dlg.DoModal() )
		return;
	// 注意：这里跟硬件的过程一致...
	// 添加操作成功，开始创建新的通道...
	int outDBCameraID = 0;
	GM_Error theErr = GM_NoErr;
	GM_MapData & theData = dlg.GetPushData();
	theErr = m_lpMidView->doCameraUDPData(theData, kCameraNO, outDBCameraID);
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
		return;
	}
	// 判断通道编号是否正确建立...
	if( outDBCameraID <= 0 ) {
		MsgLogGM(theErr);
		return;
	}
	// 必须先保存焦点，否则会发生定位错误...
	m_nFocusDBCamera = outDBCameraID;
	m_lpMidView->doLeftFocus(outDBCameraID);
	m_RightView.doFocusDBCamera(outDBCameraID);
	// 添加成功，直接运行...
	this->OnLoginDVR();
}
//
// 网络命令发起的添加操作...
LRESULT CHaoYiView::OnMsgAddByTransmit(WPARAM wParam, LPARAM lParam)
{
	//////////////////////////////////////////////////////
	// 注意：由于是网站发起的指令，不用向网站汇报了...
	//////////////////////////////////////////////////////
	int nDBCameraID = wParam;
	GM_MapData theWebData;
	CCamera * lpCamera = NULL;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetCamera(nDBCameraID, theWebData);
	lpCamera = m_lpMidView->BuildWebCamera(theWebData);
	if( lpCamera == NULL )
		return S_OK;
	// 必须先保存焦点，否则会发生定位错误...
	m_nFocusDBCamera = nDBCameraID;
	m_lpMidView->doLeftFocus(nDBCameraID);
	m_RightView.doFocusDBCamera(nDBCameraID);
	// 添加成功，直接运行...
	this->OnLoginDVR();
	return S_OK;
}
//
// 网络命令发起的删除操作...
LRESULT CHaoYiView::OnMsgDelByTransmit(WPARAM wParam, LPARAM lParam)
{
	//////////////////////////////////////////////////////
	// 注意：由于是网站发起的指令，不用向网站汇报了...
	//////////////////////////////////////////////////////
	int nDBCameraID = wParam;
	// 左侧窗口发起删除操作...
	this->doDelTreeFocus(nDBCameraID);
	// 中间窗口发起删除操作...
	m_lpMidView->doDelDVR(nDBCameraID);
	// 配置文件发起删除操作...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.doDelDVR(nDBCameraID);
	return S_OK;
}
//
// 点击菜单 => 删除通道...
void CHaoYiView::OnDelDVR()
{
	// 判断当前通道是否有效...
	CCamera * lpCamera = this->FindDBCameraByID(m_nFocusDBCamera);
	//if( lpCamera == NULL || lpCamera->IsCameraDevice() )
	//2017.10.31 - 可以删除摄像头设备...
	if( lpCamera == NULL )
		return;
	// 得到要删除窗口的序列号，通知网站时使用，不能用引用，因为会被删除...
	int nDBCameraID = m_nFocusDBCamera;
	string strDeviceSN = lpCamera->GetDeviceSN();
	// 删除之前的询问...
	if( ::MessageBox(this->m_hWnd, "确实要删除当前选中的通道吗？", "确认", MB_OKCANCEL | MB_ICONWARNING) == IDCANCEL )
		return;
	// 左侧窗口发起删除操作...
	this->doDelTreeFocus(nDBCameraID);
	// 中间窗口发起删除操作...
	m_lpMidView->doDelDVR(nDBCameraID);
	// 通知网站发起删除操作...
	this->doWebDelCamera(strDeviceSN);
	// 配置文件发起删除操作...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.doDelDVR(nDBCameraID);
}
//
// 删除左侧树形焦点节点...
void CHaoYiView::doDelTreeFocus(int nDBCameraID)
{
	HTREEITEM hChildItem = m_DeviceTree.GetChildItem(m_hRootItem);
	while( hChildItem != NULL ) {
		// 如果编号一致，直接删除这个节点...
		if( nDBCameraID == m_DeviceTree.GetItemData(hChildItem) ) {
			m_DeviceTree.DeleteItem(hChildItem);
			break;
		}
		// 如果不是，继续寻找下一个节点...
		hChildItem = m_DeviceTree.GetNextSiblingItem(hChildItem);
	}
}
//
// 点击菜单 => 修改通道...
void CHaoYiView::OnModDVR()
{
	CCamera * lpCamera = this->FindDBCameraByID(m_nFocusDBCamera);
	if( m_nFocusDBCamera <= 0 || lpCamera == NULL )
		return;
	ASSERT( m_nFocusDBCamera > 0 && lpCamera != NULL );
	STREAM_PROP theProp = lpCamera->GetStreamProp();
	if( lpCamera->IsCameraDevice() ) {
		// 针对设备通道的修改操作 => 摄像头设备只能修改...
		CDlgSetDVR dlg(m_nFocusDBCamera, lpCamera->IsLogin(), this);
		if( IDOK != dlg.DoModal() )
			return;
		// 对话框返回之前，修改信息已经保存到集合...
	} else {
		// 针对流转发通道的修改操作...
		CDlgPushDVR dlg(true, m_nFocusDBCamera, lpCamera->IsLogin(), this);
		if( IDOK != dlg.DoModal() )
			return;
		// 对话框返回之前，修改信息已经保存到集合...
		theProp = (dlg.m_bFileMode ? kStreamMP4File : kStreamUrlLink);
	}
	// 这里需要将修改信息，通知网站更新数据库...
	GM_MapData theMapWeb;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetCamera(m_nFocusDBCamera, theMapWeb);
	this->doWebRegCamera(theMapWeb);
	// 将设置的通道信息应用到中间界面当中...
	CString strDVRName = theConfig.GetDBCameraTitle(m_nFocusDBCamera);
	lpCamera->UpdateWndTitle(theProp, strDVRName);
	// 将设置的通道信息应用到右侧界面当中...
	m_RightView.doFocusDBCamera(m_nFocusDBCamera);
	// 获取左侧选中的节点...
	HTREEITEM hSelItem = m_DeviceTree.GetSelectedItem();
	if( hSelItem == NULL || m_DeviceTree.GetItemData(hSelItem) != m_nFocusDBCamera )
		return;
	// 修改左侧Tree的标题名称...
	m_DeviceTree.SetItemText(hSelItem, strDVRName);
	// 修改完毕，直接运行该通道...
	this->OnLoginDVR();
}
//
// 响应 doSetCameraName 发生的摄像头变化事件...
void CHaoYiView::UpdateFocusTitle(int nDBCameraID, CString & strTitle)
{
	// 如果当前焦点窗口编号与正在设置的本地编号一致，进行焦点设置...
	if( m_nFocusDBCamera == nDBCameraID ) {
		m_RightView.doFocusDBCamera(nDBCameraID);
	}
	// 更新左侧节点的名称...
	HTREEITEM hChildItem = m_DeviceTree.GetChildItem(m_hRootItem);
	while( hChildItem != NULL ) {
		// 如果编号一致，修改这个节点的标题名称...
		if( nDBCameraID == m_DeviceTree.GetItemData(hChildItem) ) {
			m_DeviceTree.SetItemText(hChildItem, strTitle);
			break;
		}
		// 如果不是，继续寻找下一个节点...
		hChildItem = m_DeviceTree.GetNextSiblingItem(hChildItem);
	}
}
//
// 点击菜单 => 系统设置...
void CHaoYiView::OnSysSet()
{
	CDlgSetSys dlg(this);
	if( IDOK != dlg.DoModal() )
		return;
	// 更新标题栏名称...
	this->doUpdateFrameTitle();
	// 不连接存储服务器，但是会话有效，则立即删除会话对象...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	if( !theConfig.GetAutoLinkFDFS() ) {
		this->DelByEventThread(m_lpTrackerSession);
		m_lpTrackerSession = NULL;
		this->DelByEventThread(m_lpStorageSession);
		m_lpStorageSession = NULL;
	}
	// 如果网站地址或端口发生变化，需要向本窗口发送重建消息...
	if( dlg.IsWebChange() ) {
		this->PostMessage(WM_RELOAD_VIEW);
	}
	// 其它参数会自动通过调用更新...
}
//
// 处理视图销毁事件...
void CHaoYiView::doGatherLogout()
{
	// 通知网站采集端退出...
	if( m_lpWebThread == NULL )
		return;
	ASSERT( m_lpWebThread != NULL );
	m_lpWebThread->doWebGatherLogout();
}
//
// 响应整个视图窗口的重建消息事件...
LRESULT CHaoYiView::OnMsgReloadView(WPARAM wParam, LPARAM lParam)
{
	CRect rectMid;
	// 获取中间视图的矩形区...
	if( m_lpMidView != NULL ) {
		m_lpMidView->GetWindowRect(rectMid);
		this->ScreenToClient(rectMid);
	}
	// 重建所有的资源数据对象...
	this->BuildResource();
	// 调整中间视图的矩形位置...
	if( m_lpMidView != NULL ) {
		m_lpMidView->doMoveWindow(rectMid);
	}
	return S_OK;
}
//
// 创建窗口之前的操作 => 修改窗口属性...
BOOL CHaoYiView::PreCreateWindow(CREATESTRUCT& cs)
{
	if( !CFormView::PreCreateWindow(cs) )
		return FALSE;
	//cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	return TRUE;
}
