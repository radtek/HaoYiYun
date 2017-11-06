
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
	// ֪ͨ��վ�ɼ����˳�...
	this->doGatherLogout();
	// �ڴ��ڹر�֮ǰ���ͷ���Դ...
	this->DestroyResource();
	// �ͷŴ��ھ������Դ...
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
	//blog.csdn.net/sz76211822/article/details/51507616 => ������ʾ�������ӣ���Ҫ����͸����ʾ��ʾ
	/***********************************************************************************************
	//TransparentBlt()...
	//������ʾ͸����GIF���� - ʧ�ܣ�
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
// �õ���ǰ��������Kbps...
DWORD CHaoYiView::GetCurSendKbps()
{
	if( m_lpStorageSession == NULL )
		return 0;
	return m_lpStorageSession->GetCurSendKbps();
}
//
// �õ���ǰ�����ļ�����...
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

CHaoYiDoc* CHaoYiView::GetDocument() const // �ǵ��԰汾��������
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CHaoYiDoc)));
	return (CHaoYiDoc*)m_pDocument;
}
#endif //_DEBUG

//
// �Ҽ����������ڵ�...
void CHaoYiView::OnRclickTreeDevice(NMHDR *pNMHDR, LRESULT *pResult)
{
	*pResult = 0;
}
//
// ������ڵ㷢���仯 => ���ﴦ��ᷢ�������ظ�ѭ�������� => ��ȡ���λ�������...
void CHaoYiView::OnSelchangedTreeDevice(NMHDR *pNMHDR, LRESULT *pResult)
{
	*pResult = 0;
	HTREEITEM hHitItem = NULL;
	if( m_bTreeKeydown ) {
		// �Ǽ��̷���������ı仯...
		hHitItem = m_DeviceTree.GetSelectedItem();
		m_bTreeKeydown = false;
	} else {
		// �������λ�ã����㵱ǰ����Ľڵ�...
		CPoint ptPoint;
		::GetCursorPos(&ptPoint);
		m_DeviceTree.ScreenToClient(&ptPoint);
		hHitItem = m_DeviceTree.HitTest(ptPoint);
	}
	if( hHitItem == NULL || m_lpMidView == NULL )
		return;
	// ��ȡ�ýڵ����Ч���ͨ�����...
	ASSERT( hHitItem != NULL && m_lpMidView != NULL );
	int nDBCameraID = m_DeviceTree.GetItemData(hHitItem);
	if( nDBCameraID <= 0 )
		return;
	// ֪ͨ���Ĵ��ڡ��Ҳര�ڣ����㷢���˱仯...
	// �����ȱ��潹�㣬�����Ҳ���ʾ����...
	m_nFocusDBCamera = nDBCameraID;
	m_lpMidView->doLeftFocus(nDBCameraID);
	m_RightView.doFocusDBCamera(nDBCameraID);
}
//
// ��Ӧ�����¼�...
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
// ��Ӧ���ͨ�����������¼�...
void CHaoYiView::OnCreateCamera(int nDBCameraID, CString & strTitle)
{
	ASSERT( m_hRootItem != NULL );
	HTREEITEM hItemTree = NULL;
	// ���������״�ڵ㣬����ͨ����ż�¼����...
	hItemTree = m_DeviceTree.InsertItem(strTitle, 1, 1, m_hRootItem);
	m_DeviceTree.SetItemData(hItemTree, nDBCameraID);
	m_DeviceTree.Expand(m_hRootItem,TVE_EXPAND);
	// ѡ�нڵ㣬���潹����, ֪ͨ�м���ͼ���Ҳ���ͼ����仯...
	// �����ȱ��潹�㣬�����Ҳര��״̬��ʾ����...
	if( m_nFocusDBCamera <= 0 && m_lpMidView != NULL ) {
		m_nFocusDBCamera = nDBCameraID;
		m_DeviceTree.SelectItem(hItemTree);
		m_lpMidView->doLeftFocus(nDBCameraID);
		m_RightView.doFocusDBCamera(nDBCameraID);
	}
}
//
// ��Ӧ���ͨ�����ڵĽ����¼�...
LRESULT CHaoYiView::OnMsgFocusVideo(WPARAM wParam, LPARAM lParam)
{
	// ֪ͨ�����״�ؼ�����仯...
	int nDBFocusID = wParam;
	ASSERT( m_hRootItem != NULL );
	HTREEITEM hChildItem = m_DeviceTree.GetChildItem(m_hRootItem);
	while( hChildItem != NULL ) {
		// ������һ�£�ѡ������ڵ�...
		if( nDBFocusID == m_DeviceTree.GetItemData(hChildItem) ) {
			m_DeviceTree.SelectItem(hChildItem);
			break;
		}
		// ������ǣ�����Ѱ����һ���ڵ�...
		hChildItem = m_DeviceTree.GetNextSiblingItem(hChildItem);
	}
	// ������Ƶ���㴰�ڱ�ţ�֪ͨ�Ҳ���ͼ����仯...
	// �����ȱ��潹�㣬�����Ҳ���ʾ����...
	m_nFocusDBCamera = nDBFocusID;
	m_RightView.doFocusDBCamera(nDBFocusID);
	return S_OK;
}

BOOL CHaoYiView::GetMacIPAddr()
{
	// �������ռ����Ҫ����Ҫ�����һЩ...
	DWORD buflen = sizeof(IP_ADAPTER_INFO) * 3;
	IP_ADAPTER_INFO * lpAdapter = new IP_ADAPTER_INFO[3];
	DWORD status = GetAdaptersInfo(lpAdapter, &buflen);
	if( status != ERROR_SUCCESS ) {
		delete [] lpAdapter;
		return false;
	}
	// ��ʼѭ�����������ڵ�...
	IP_ADAPTER_INFO * lpInfo = lpAdapter;
	while( lpInfo != NULL ) {
		// ��������̫������������IPV4����������������Ч������...
		if( lpInfo->Type != MIB_IF_TYPE_ETHERNET || lpInfo->AddressLength != 6 || strcmp(lpInfo->IpAddressList.IpAddress.String, "0.0.0.0") == 0 ) {
			lpInfo = lpInfo->Next;
			continue;
		}
		// ��ȡMAC��ַ��IP��ַ����ַ���໥������...
		m_strMacAddr.Format("%02X-%02X-%02X-%02X-%02X-%02X", lpInfo->Address[0], lpInfo->Address[1], lpInfo->Address[2], lpInfo->Address[3], lpInfo->Address[4], lpInfo->Address[5]);
		m_strIPAddr.Format("%s", lpInfo->IpAddressList.IpAddress.String);
		lpInfo = lpInfo->Next;
	}
	// �ͷŷ���Ļ�����...
	delete [] lpAdapter;
	lpAdapter = NULL;
	// ��һ�ֻ�ȡIP��ַ�ķ��� => MAC��ַ��IP��ַ��һ���໥����...
	/*if( SocketUtils::GetNumIPAddrs() > 1 ) {
		StrPtrLen * lpAddr = SocketUtils::GetIPAddrStr(0);
		m_strIPAddr = lpAddr->Ptr;
	}*/
	return true;
}

void CHaoYiView::BuildResource()
{
	this->DestroyResource();

	// ��ȡ����MAC��ַ...
	if( !this->GetMacIPAddr() ) {
		MsgLogGM(GM_Net_Err);
	}

	// ����ͼƬ�б����...
	CBitmap bitTemp;
	m_ImageList.Create(25, 23, ILC_COLORDDB |ILC_MASK, 8, 8);
	bitTemp.LoadBitmap(IDB_BITMAP_CAMERA);
	m_ImageList.Add(&bitTemp, RGB(252,2,252));
	bitTemp.DeleteObject();

	// �����豸���ı���ɫ/����ɫ/ͼƬ����...
	m_DeviceTree.SetBkColor(WND_BK_COLOR);
	m_DeviceTree.SetTextColor(RGB(255, 255, 255));
	m_DeviceTree.SetImageList(&m_ImageList, LVSIL_NORMAL);

	// �޸��豸��������...
	m_DeviceTree.ModifyStyle(NULL,  WS_VISIBLE | WS_TABSTOP | WS_CHILD //| WS_BORDER
		| TVS_HASBUTTONS | TVS_LINESATROOT | TVS_HASLINES | TVS_SHOWSELALWAYS   
		| TVS_DISABLEDRAGDROP|TVS_FULLROWSELECT|TVS_TRACKSELECT|TVS_EX_AUTOHSCROLL);

	// ��ʼ���豸���ĸ��ڵ�...
	m_DeviceTree.DeleteAllItems();
	m_hRootItem	= m_DeviceTree.InsertItem("ͨ���豸�б�", 0, 0);		
	m_DeviceTree.Expand(m_hRootItem,TVE_EXPAND);
	m_DeviceTree.SelectItem(m_hRootItem);

	// �����豸������ʾ����...
	//m_tipTree.Create(this);
	//m_tipTree.AddTool(&m_DeviceTree, "������Ҽ����Ե��������˵�");
	//m_tipTree.SetDelayTime(TTDT_INITIAL, 100);

	// �����м䴰�ڶ���...
	ASSERT( m_lpMidView == NULL );
	m_lpMidView = new CMidView(this);
	m_lpMidView->Create(WS_CHILD|WS_VISIBLE, CRect(0, 0, 0, 0), this);
	ASSERT( m_lpMidView->m_hWnd != NULL );

	// ��ʼ���Ҳര��...
	m_RightView.InitButton(m_lpMidView->GetVideoFont());

	// ����������һ����վ�����߳�...
	GM_Error theErr = GM_NoErr;
	m_lpWebThread = new CWebThread(this);
	theErr = m_lpWebThread->Initialize();
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
		return;
	}
	// ����ֱ������������Դ������ͨ�����罻���߳�������...
}
//
// ��վ��Ȩ��֤���...
LRESULT CHaoYiView::OnMsgWebAuthResult(WPARAM wParam, LPARAM lParam)
{
	ASSERT( m_lpMidView != NULL );
	m_lpMidView->OnWebAuthResult(wParam, lParam);
	return S_OK;
}
//
// ���������ڱ�������...
void CHaoYiView::doUpdateFrameTitle()
{
	CString strTitle, strWebType;
	CMainFrame * lpFrame = (CMainFrame*)AfxGetMainWnd();
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	string & strMainName = theConfig.GetMainName();
	string & strWebName = theConfig.GetWebName();
	switch( theConfig.GetWebType() )
	{
	case kCloudRecorder: strWebType = "��¼��";	break;
	case kCloudMonitor:	 strWebType = "�Ƽ��";	break;
	default:			 strWebType = "δ֪";	break;
	}
	strTitle.Format("%s - %s", strWebType, strMainName.c_str());
	lpFrame->SetWindowText(strTitle);
}
//
// ��վ�̼߳��ص���Դ...
LRESULT CHaoYiView::OnMsgWebLoadResource(WPARAM wParam, LPARAM lParam)
{
	// ���������ڱ���������...
	this->doUpdateFrameTitle();
	// ������վ���ý���ͨ��...
	ASSERT( m_lpMidView != NULL );
	m_lpMidView->BuildVideoByWeb();
	// ����Ƶ����ѯ�߳�...
	GM_Error theErr = GM_NoErr;
	m_lpHKUdpThread = new CHKUdpThread(this);
	theErr = m_lpHKUdpThread->InitMulticast();
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
		return S_OK;
	}
	// ����fastdfs�Ự�����߳�...
	m_lpFastThread = new CFastThread(this->m_hWnd);
	theErr = m_lpFastThread->Initialize();
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
		return S_OK;
	}
	// ������Ҫ��һ��Timer������ => ���ϳ��Դ���Tracker�����Զ�����DVR���Զ����¼��γ̱�...
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
// �����йؿγ̼�¼�����仯�����...
// ���ǵ�����¼ => ���(1) | �޸�(2) | ɾ��(3)...
void CHaoYiView::doCourseChanged(int nOperateID, int nDBCameraID, GM_MapData & inData)
{
	// ���ȣ��������ݵ���Ч����֤...
	if( nOperateID < kAddCourse || nOperateID > kDelCourse )
		return;
	if( inData.find("course_id") == inData.end() )
		return;
	int nCourseID = atoi(inData["course_id"].c_str());
	// ��ϵͳ���ö����л�ȡ���е�¼��γ̱���ȡ���ã���Ҫ���л��Ᵽ��...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	OSMutexLocker theLock(&theConfig.m_MutexCourse);
	// ����ֱ���ñ��ر�ŵõ��α��ϣ����û�л��Զ�����һ���µ�...
	GM_MapCourse & theCourse = theConfig.m_MapNodeCourse[nDBCameraID];
	if( nOperateID == kDelCourse ) {
		// �ȶԿα��¼���������ֹͣ����ɾ�� => �ڲ��Զ��ж��Ƿ��ܹ�ֹͣ¼��...
		if( theCourse.find(nCourseID) != theCourse.end() ) {
			this->doRecStopCourse(nDBCameraID, nCourseID);
			theCourse.erase(nCourseID);
		}
	} else {
		// ��Ӻ��޸Ķ�����Ҫֱ�Ӵ��뼯�ϵ��� => ���ﲻ��Ҫ�ж��Ƿ���Ҫֹͣ¼�������������Ȼ��Զ�����...
		ASSERT((nOperateID == kAddCourse) || (nOperateID == kModCourse));
		theCourse[nCourseID] = inData;
	}
}
//
// ��ȡֻ��ʱ�����������������...
time_t CHaoYiView::GetTimeSecond(time_t inTime)
{
	CTime theCTime(inTime);
	return (theCTime.GetHour()*3600 + theCTime.GetMinute()*60 + theCTime.GetSecond());
}
//
// ÿ��������¼��γ̱�...
void CHaoYiView::doCheckCourse()
{
	// ��ȡ��ǰ���������ڼ�...
	int nCurDayOfWeek = CTime::GetCurrentTime().GetDayOfWeek() - 1;
	// ��ϵͳ���ö����л�ȡ���е�¼��γ̱���ȡ���ã���Ҫ���л��Ᵽ��...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	OSMutexLocker theLock(&theConfig.m_MutexCourse);
	// ��ȡ�γ��б�����ö��󣬿��ܻ��޸Ŀγ�����...
	GM_MapNodeCourse & theNodeCourse = theConfig.GetNodeCourse();
	GM_MapNodeCourse::iterator itorNode;
	// ��ʼ����������ͷ(ͨ��)Ϊ��׼���б�...
	for(itorNode = theNodeCourse.begin(); itorNode != theNodeCourse.end(); ++itorNode) {
		int nCameraID = itorNode->first;
		GM_MapCourse & theCourse = itorNode->second;
		GM_MapCourse::iterator itorCourse;
		// ������ͨ���µĿγ̼�¼...
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
			// ��ǰ�������¼������ڲ�һ�� => �õ�ǰ��¼���ȥֹͣ�����¼������0������...
			if( nWeekID != nCurDayOfWeek ) {
				this->doRecStopCourse(nCameraID, nCourseID);
				continue;
			}
			// ��ǰʱ�� < ��ʼʱ�� => ʱ��δ����ֱ��ֹͣ����¼��ĵ�ǰ�����¼����Ϊ�п������޸Ĺ�ʱ��ļ�¼...
			if( tNow < tStart ) {
				this->doRecStopCourse(nCameraID, nCourseID);
				continue;
			}
			// ��ǰʱ�� >= ����ʱ�� => ֪ͨ��������������ǰ���м�¼��������������ֱ�ӷ���...
			if( tNow >= tEnd ) {
				// ֹͣ��ǰͨ��������¼��ĵ�ǰ�����¼...
				this->doRecStopCourse(nCameraID, nCourseID);
				///////////////////////////////////////////////////////////////////////////////
				// 2017.07.28 - by jackey => �̶�ģʽÿ���ظ�������ֻ�Ƚ�ʱ�䣬���Բ��ô�����...
				///////////////////////////////////////////////////////////////////////////////
				// ���ظ�ģʽ => ֱ�Ӽ����һ����¼ => ��Ҫ����ɾ����������Ϊ���ܻ�����޸�...
				/*if( nRepMode == kNoneRepeat )
					continue;
				ASSERT( nRepMode == kDayRepeat || nRepMode == kWeekRepeat );
				// ÿ���ظ� => �ۼ�1*24Сʱ...
				if( nRepMode == kDayRepeat ) {
					tStart += 1 * 24 * 3600;
					tEnd    = tStart + tDur;
				} else if( nRepMode == kWeekRepeat ) {
					// ÿ���ظ� => �ۼ�7*24Сʱ...
					tStart += 7 * 24 * 3600;
					tEnd    = tStart + tDur;
				}
				// ת����ʼʱ��...
				TCHAR szBuf[MAX_PATH/2] = {0};
				sprintf(szBuf, "%lu", tStart);
				theData["start_time"] = szBuf;
				// ת������ʱ��...
				sprintf(szBuf, "%lu", tEnd);
				theData["end_time"] = szBuf;*/
				// ������һ������...
				continue;
			}
			// ��ǰʱ��һ�����ڿ�ʼʱ�䣬������ʼ¼��...
			ASSERT(tNow >= tStart);
			// ������ǰͨ��������¼�� => �ڲ����Զ��жϵ�ǰ��¼����Ƿ�Ϸ���Ч...
			this->doRecStartCourse(nCameraID, nCourseID);
		}
	}
}
//
// ��������ͷ�Ŀγ�����¼��...
void CHaoYiView::doRecStartCourse(int nDBCameraID, int nCourseID)
{
	CCamera * lpCamera = this->FindDBCameraByID(nDBCameraID);
	if( lpCamera == NULL )
		return;
	// �����ǰͨ������¼��ļ�¼�뵱ǰ��¼һ�£�ֱ�ӷ���...
	if( lpCamera->GetRecCourseID() == nCourseID )
		return;
	lpCamera->doRecStartCourse(nCourseID);
}
//
// ֹͣ����ͷ�Ŀγ�����¼��...
void CHaoYiView::doRecStopCourse(int nDBCameraID, int nCourseID)
{
	CCamera * lpCamera = this->FindDBCameraByID(nDBCameraID);
	if( lpCamera == NULL )
		return;
	// �����ǰͨ������¼��ļ�¼�뵱ǰ��¼��һ�£�ֱ�ӷ���...
	if( lpCamera->GetRecCourseID() != nCourseID )
		return;
	lpCamera->doRecStopCourse(nCourseID);
}
//
// ÿ��1����ʾDVR������Ϣ...
void CHaoYiView::doAnimateDVR()
{
	// �������е��ӽڵ� => DVRͨ���ڵ�...
	HTREEITEM hChildItem = m_DeviceTree.GetChildItem(m_hRootItem);
	while( hChildItem != NULL ) {
		int nDBCameraID = m_DeviceTree.GetItemData(hChildItem);
		CCamera * lpCamera = this->FindDBCameraByID(nDBCameraID);
		// ͨ����Ч�����ҵ�¼�ɹ������ö���ͼ��...
		if( lpCamera != NULL && lpCamera->IsLogin() ) {
			m_DeviceTree.SetItemImage(hChildItem, m_nAnimateIndex + 2, m_nAnimateIndex + 2);
		} else {
			// ͨ��û�е�¼���ָ�ͨ��ͼ��״̬...
			int nImage = 0, nSelImage = 0;
			m_DeviceTree.GetItemImage(hChildItem, nImage, nSelImage);
			if( nImage != 1 ) {
				m_DeviceTree.SetItemImage(hChildItem, 1, 1);
			}
		}
		// ������һ��ͨ���ڵ�...
		hChildItem = m_DeviceTree.GetNextSiblingItem(hChildItem);
	}
	// �ܹ��ĸ���������������ۼ�...
	m_nAnimateIndex = (++m_nAnimateIndex) % 4;
}
//
// ͨ�����ݿ��Ż�ȡ����ͷ״̬��Ϣ...
int CHaoYiView::GetDBCameraStatusByID(int nDBCameraID)
{
	// ͨ�����ر�Ų�������ͷ����...
	int nStatus = kCameraWait;
	CCamera * lpCamera = this->FindDBCameraByID(nDBCameraID);
	if( lpCamera == NULL )
		return nStatus;
	// ��ȡ������ĵ�ǰ����״̬...
	if( lpCamera->IsRecording() ) {
		nStatus = kCameraRec;
	} else if( lpCamera->IsPlaying() ) {
		nStatus = kCameraRun;
	}
	// ��󷵻�����״̬...
	return nStatus;
}
//
// ÿ��5����һ��Tracker��Storage�����Ƿ񱻴���...
void CHaoYiView::doCheckFDFS()
{
	this->doCheckTracker();
	this->doCheckStorage();
	this->doCheckTransmit();
}
//
// �Զ���Ⲣ����RemoteSession...
void CHaoYiView::doCheckTransmit()
{
	if( m_lpRemoteSession != NULL )
		return;
	ASSERT( m_lpRemoteSession == NULL );
	// �����ת��������ַ�Ƿ���Ч...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	string & strRemoteAddr = theConfig.GetRemoteAddr();
	int nRemotePort = theConfig.GetRemotePort();
	if( strRemoteAddr.size() <= 0 || nRemotePort <= 0 )
		return;
	// ��ȡTransmit���ã�����Remote����...
	ASSERT( strRemoteAddr.size() > 0 && nRemotePort > 0 );
	m_lpRemoteSession = new CRemoteSession(this);
	GM_Error theErr = GM_NoErr;
	do {
		// ����Remote�Ự����...
		theErr = m_lpRemoteSession->InitSession(strRemoteAddr.c_str(), nRemotePort);
		if( theErr != GM_NoErr ) {
			MsgLogGM(theErr);
			break;
		}
		// ���뵽�¼��ȴ��̵߳���...
		theErr = this->AddToEventThread(m_lpRemoteSession);
		if( theErr != GM_NoErr ) {
			MsgLogGM(theErr);
			break;
		}
	}while( false );
	// ��ȷִ�У�ֱ�ӷ���...
	if( theErr == GM_NoErr )
		return;
	// ִ�д���ɾ�����ȴ��ؽ�...
	ASSERT( theErr != GM_NoErr );
	if( m_lpRemoteSession != NULL ) {
		delete m_lpRemoteSession;
		m_lpRemoteSession = NULL;
	}
}
//
// ת����ʱ���ָ����ֱ������������...
/*GM_Error CHaoYiView::doTransmitPlayer(int nPlayerSock, string & strRtmpUrl, GM_Error inErr)
{
	if( m_lpRemoteSession == NULL )
		return GM_NoErr;
	return m_lpRemoteSession->doTransmitPlayer(nPlayerSock, strRtmpUrl, inErr);
}*/
//
// �Զ���Ⲣ����TrackerSession...
void CHaoYiView::doCheckTracker()
{
	if( m_lpTrackerSession != NULL )
		return;
	ASSERT( m_lpTrackerSession == NULL );
	// ����Ƿ������Զ�����Tracker������...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	string & strAddr = theConfig.GetTrackerAddr();
	int nPort = theConfig.GetTrackerPort();
	if( !theConfig.GetAutoLinkFDFS() || strAddr.size() <= 0 || nPort <= 0 )
		return;
	// ��ȡTracker���ã�����Tracker����...
	ASSERT( strAddr.size() > 0 && nPort > 0 );
	ASSERT( theConfig.GetAutoLinkFDFS() );
	m_lpTrackerSession = new CTrackerSession();
	GM_Error theErr = GM_NoErr;
	do {
		// ����Tracker�Ự���󣬻�ȡStorage��ַ...
		theErr = m_lpTrackerSession->InitSession(strAddr.c_str(), nPort);
		if( theErr != GM_NoErr ) {
			MsgLogGM(theErr);
			break;
		}
		// ���뵽�¼��ȴ��̵߳���...
		theErr = this->AddToEventThread(m_lpTrackerSession);
		if( theErr != GM_NoErr ) {
			MsgLogGM(theErr);
			break;
		}
	}while( false );
	// ��ȷִ�У�ֱ�ӷ���...
	if( theErr == GM_NoErr )
		return;
	// ִ�д���ɾ�����ȴ��ؽ�...
	ASSERT( theErr != GM_NoErr );
	if( m_lpTrackerSession != NULL ) {
		delete m_lpTrackerSession;
		m_lpTrackerSession = NULL;
	}
}
//
// �Զ���Ⲣ����StorageSession...
void CHaoYiView::doCheckStorage()
{
	// �����ж�һЩ��������Ч��...
	if( m_lpTrackerSession == NULL || m_lpStorageSession != NULL )
		return;
	ASSERT( m_lpTrackerSession != NULL && m_lpStorageSession == NULL );
	StorageServer & theStorage = m_lpTrackerSession->GetStorageServer();
	if( !m_lpTrackerSession->IsConnected() || theStorage.port <= 0 )
		return;
	ASSERT( m_lpTrackerSession->IsConnected() && theStorage.port > 0 );
	// ���¼��Ŀ¼��û����Ҫ�ϴ����ļ���ֱ�ӷ��أ������ȴ�...
	if( !this->IsHasUploadFile() )
		return;
	// ����Storage�Ự���󣬸���Storage��ַ...
	GM_Error theErr = GM_NoErr;
	m_lpStorageSession = new CStorageSession(&theStorage);
	do {
		// ����Storage����Ự...
		theErr = m_lpStorageSession->InitSession(theStorage.ip_addr, theStorage.port);
		if( theErr != GM_NoErr ) {
			MsgLogGM(theErr);
			break;
		}
		// ���뵽�¼��ȴ��̵߳���...
		theErr = this->AddToEventThread(m_lpStorageSession);
		if( theErr != GM_NoErr ) {
			MsgLogGM(theErr);
			break;
		}
	}while( false );
	// ��ȷִ�У�ֱ�ӷ���...
	if( theErr == GM_NoErr )
		return;
	// ִ�д���ɾ�����ȴ��ؽ�...
	ASSERT( theErr != GM_NoErr );
	if( m_lpStorageSession != NULL ) {
		delete m_lpStorageSession;
		m_lpStorageSession = NULL;
	}
}
//
// �Ƿ�����Ҫ�ϴ��ļ�...
BOOL CHaoYiView::IsHasUploadFile()
{
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	string & strSavePath = theConfig.GetSavePath();
	string   strRootPath = strSavePath;
	if( strRootPath.size() <= 0 )
		return false;
	// ·���Ƿ���Ҫ���Ϸ�б��...
	BOOL bHasSlash = false;
	bHasSlash = ((strRootPath.at(strRootPath.size()-1) == '\\') ? true : false);
	strRootPath += (bHasSlash ? "*.*" : "\\*.*");
	// ׼��������Ҫ�ı���...
	GM_Error theErr = GM_NoErr;
	WIN32_FIND_DATA theFileData = {0};
	HANDLE	hFind = INVALID_HANDLE_VALUE;
	LPCTSTR	lpszExt = NULL;
	BOOL	bIsOK = true;
	CString strFileName;
	// ���ҵ�һ���ļ���Ŀ¼...
	hFind = ::FindFirstFile(strRootPath.c_str(), &theFileData);
	if( hFind != INVALID_HANDLE_VALUE ) {
		while( bIsOK ) {
			if(	(theFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||	// Ŀ¼
				(theFileData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) ||		// ϵͳ
				(strcmp(theFileData.cFileName, ".") == 0) ||					// ��ǰ
				(strcmp(theFileData.cFileName, "..") == 0) )					// �ϼ�
			{
				bIsOK = ::FindNextFile(hFind, &theFileData);
				continue;
			}
			// �ж���չ���Ƿ���Ч => .jpg .jpeg .mp4 ��Ч...
			lpszExt = ::PathFindExtension(theFileData.cFileName);
			if( (stricmp(lpszExt, ".jpg") != 0) && (stricmp(lpszExt, ".jpeg") != 0) && (stricmp(lpszExt, ".mp4") != 0) ) {
				bIsOK = ::FindNextFile(hFind, &theFileData);
				continue;
			}
			// ���ȣ������Ч���ļ�ȫ·������ȡ�ļ�����...
			strFileName.Format(bHasSlash ? "%s%s" : "%s\\%s", strSavePath.c_str(), theFileData.cFileName);
			ULONGLONG llSize = CUtilTool::GetFileFullSize(strFileName);
			// �ļ�����Ϊ0��������һ���ļ�...
			if( llSize <= 0 ) {
				bIsOK = ::FindNextFile(hFind, &theFileData);
				continue;
			}
			// �ļ����ͺͳ��ȶ�����Ч�ģ��رվ����ֱ�ӷ���...
			::FindClose(hFind);
			return true;
		}
	}
	// ������Ҿ����Ϊ�գ�ֱ�ӹر�֮...
	if( hFind != INVALID_HANDLE_VALUE ) {
		::FindClose(hFind);
	}
	return false;
}
//
// ÿ��һ���ӳ�������DVR...
void CHaoYiView::doCheckDVR()
{
	// ����Ƿ������Զ�����DVR������...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	if( !theConfig.GetAutoLinkDVR() )
		return;
	ASSERT( theConfig.GetAutoLinkDVR() );
	// ֱ�ӵ����м���ͼ�������Զ�����...
	if( m_RightView.m_hWnd != NULL ) {
		m_RightView.doAutoCheckDVR();
	}
}
//
// �õ���һ�����ڱ��...
int CHaoYiView::GetNextAutoID(int nCurDBCameraID)
{
	return ((m_lpMidView != NULL) ? m_lpMidView->GetNextAutoID(nCurDBCameraID) : DEF_CAMERA_START_ID);
}
//
// �ı��Զ�����DVR��ʱ������...
/*void CHaoYiView::doChangeAutoDVR(DWORD dwErrCode)
{
	// ����Ƿ������Զ�����DVR������...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	if( !theConfig.GetAutoLinkDVR() )
		return;
	ASSERT( theConfig.GetAutoLinkDVR() );
	this->KillTimer(kTimerCheckDVR);
	// �����������󣬻��ߵ�¼��������ֱ�ӷ���...
	if( dwErrCode == NET_DVR_PASSWORD_ERROR || dwErrCode == NET_DVR_USER_LOCKED )
		return;
	// �������������Զ���������...
	this->SetTimer(kTimerCheckDVR, 2 * 10 * 1000, NULL);
}*/

void CHaoYiView::DestroyResource()
{
	// ֹͣ�Զ���¼ʱ��...
	this->KillTimer(kTimerCheckFDFS);
	this->KillTimer(kTimerCheckDVR);
	this->KillTimer(kTimerCheckCourse);
	this->KillTimer(kTimerAnimateDVR);
	this->KillTimer(kTimerWebGatherConfig);
	// �ͷ�web�߳�...
	if( m_lpWebThread != NULL ) {
		delete m_lpWebThread;
		m_lpWebThread = NULL;
	}
	// ɾ���м���ͼ...
	if( m_lpMidView != NULL ) {
		delete m_lpMidView;
		m_lpMidView = NULL;
	}
	// �����Ҳര����İ�ť����...
	m_RightView.DestoryAllButton();
	// ɾ�����ڵ�����...
	m_DeviceTree.DeleteAllItems();
	m_ImageList.DeleteImageList();
	// �ͷ�Ƶ����ѯ�߳�...
	if( m_lpHKUdpThread != NULL ) {
		delete m_lpHKUdpThread;
		m_lpHKUdpThread = NULL;
	}
	// ���ȣ��ͷ�fastdfs�Ự�����߳�...
	this->ClearFastThreads();
	// Ȼ���ͷ����еĻỰ������Դ...
	this->ClearFastSession();
	// ����һЩȫ�ֱ���...
	m_bTreeKeydown = false;
	m_nFocusDBCamera = 0;
	m_nAnimateIndex = 0;
	m_hRootItem = NULL;
}
//
// �ͷŻỰ�����߳�...
void CHaoYiView::ClearFastThreads()
{
	// �ͷ�FastDFS�Ự�����߳�...
	if( m_lpFastThread != NULL ) {
		delete m_lpFastThread;
		m_lpFastThread = NULL;
	}
}
//
// �ͷ����еĻ�Ա��Դ...
void CHaoYiView::ClearFastSession()
{
	// �ͷ�Զ�̻Ự����...
	if( m_lpRemoteSession != NULL ) {
		delete m_lpRemoteSession;
		m_lpRemoteSession = NULL;
	}
	// �ͷ�Tracker�Ự����...
	if( m_lpTrackerSession != NULL ) {
		delete m_lpTrackerSession;
		m_lpTrackerSession = NULL;
	}
	// �ͷ�Storage�Ự����...
	if( m_lpStorageSession != NULL ) {
		delete m_lpStorageSession;
		m_lpStorageSession = NULL;
	}
}
//
// ���¼��Ự���뵽�¼��߳��н��й���...
GM_Error CHaoYiView::AddToEventThread(CFastSession * lpSession)
{
	if( m_lpFastThread == NULL || lpSession == NULL )
		return GM_NoErr;
	// ���Ự���뵽�¼��̵߳���...
	ASSERT( m_lpFastThread != NULL && lpSession != NULL );
	return m_lpFastThread->AddSession(lpSession);
}
//
// ͨ���߳̽ӿ���ɾ���Ự����,������Դ��ͻ...
GM_Error CHaoYiView::DelByEventThread(CFastSession * lpSession)
{
	if( m_lpFastThread == NULL || lpSession == NULL )
		return GM_NoErr;
	// ֱ�ӵ����߳̽ӿڣ�ɾ��ָ���ĻỰ����...
	ASSERT( m_lpFastThread != NULL && lpSession != NULL );
	return m_lpFastThread->DelSession(lpSession);
}
//
// ��Ӧ�¼��Ự���͵�֪ͨ��Ϣ...
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
// ��Ӧɾ������ => CFastSession * 
void CHaoYiView::OnOptDelSession(ULONG inUniqueID)
{
	if( inUniqueID == NULL )
		return;
	ASSERT( inUniqueID > 0 );
	// ��ȡ�Ự��ǿ��ת��ָ�����...
	CFastSession * lpSession = (CFastSession *)inUniqueID;
	GM_Error theErr = lpSession->GetErrorCode();
	MsgLogGM(theErr);
	// �Ự�Ѿ����߳��б�ע�����ˣ�����ֱ�ӱ�ɾ��...
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

	// �Կؼ�����λ���ƶ�����...
	if( m_DeviceTree.m_hWnd == NULL || m_QRStatic.m_hWnd == NULL ) 
		return;
	// ��ʾ��ά���ģʽ...
	//m_DeviceTree.SetWindowPos(this, 0, 0, QR_CODE_CX, cy - QR_CODE_CY, SWP_NOZORDER | SWP_NOACTIVATE);
	//m_QRStatic.SetWindowPos(this, 0, cy - QR_CODE_CY, QR_CODE_CX, QR_CODE_CY, SWP_NOZORDER | SWP_NOACTIVATE);
	// ����ʾ��ά���ģʽ...
	m_DeviceTree.SetWindowPos(this, 0, 0, QR_CODE_CX, cy - 0, SWP_NOZORDER | SWP_NOACTIVATE);
	m_QRStatic.SetWindowPos(this, 0, cy - 0, QR_CODE_CX, 0, SWP_NOZORDER | SWP_NOACTIVATE);
	m_RightView.SetWindowPos(this, cx - QR_CODE_CX, 0, QR_CODE_CX, cy, SWP_NOZORDER | SWP_NOACTIVATE);
	
	// �����м䴰�ڵĴ�С...
	if( m_lpMidView != NULL && m_lpMidView->m_hWnd != NULL ) {
		int nMidWidth = cx - QR_CODE_CX * 2 - kSplitterWidth * 2;
		CRect rectT(QR_CODE_CX + kSplitterWidth, 0, QR_CODE_CX + kSplitterWidth + nMidWidth, cy);
		m_lpMidView->doMoveWindow(rectT);
	}

	// ��������϶���λ��...
	m_rcSplitLeftSrc.SetRect(QR_CODE_CX, 0, QR_CODE_CX + kSplitterWidth, cy);
	m_rcSplitLeftNew.SetRect(QR_CODE_CX, 0, QR_CODE_CX + kSplitterWidth, cy);

	// �����Ҳ��϶���λ��...
	m_rcSplitRightSrc.SetRect(cx - QR_CODE_CX - kSplitterWidth, 0, cx - QR_CODE_CX, cy);
	m_rcSplitRightNew.SetRect(cx - QR_CODE_CX - kSplitterWidth, 0, cx - QR_CODE_CX, cy);
}

BOOL CHaoYiView::OnEraseBkgnd(CDC* pDC)
{
	//return CFormView::OnEraseBkgnd(pDC);

	// ���Ʒָ���...
	pDC->FillSolidRect(&m_rcSplitLeftSrc, RGB(224,224,224));
	pDC->FillSolidRect(&m_rcSplitRightSrc, RGB(224,224,224));

	return true;
}
//
// ���Ʒָ���..
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
// ���ָ�����¼�...
void CHaoYiView::OnLeftSplitEnd(int nOffset)
{
	CRect	rectTemp, rcMain;

	// �ƶ��豸��λ��...
	m_DeviceTree.GetWindowRect(rectTemp);
	this->ScreenToClient(rectTemp);
	m_DeviceTree.SetWindowPos(NULL, rectTemp.left, rectTemp.top, rectTemp.Width() + nOffset, rectTemp.Height(), SWP_NOZORDER);
	m_DeviceTree.Invalidate(true);

	// �ƶ���ά��λ��...
	//m_QRStatic.GetWindowRect(rectTemp);
	//this->ScreenToClient(rectTemp);
	//m_QRStatic.SetWindowPos(NULL, rectTemp.left, rectTemp.top, rectTemp.Width() + nOffset, rectTemp.Height(), SWP_NOZORDER);
	//m_QRStatic.Invalidate(true);

	// �����µķָ���λ��...
	this->GetClientRect(rcMain);
	m_DeviceTree.GetWindowRect(rectTemp);
	this->ScreenToClient(rectTemp);
	
	m_rcSplitLeftSrc.SetRect(rectTemp.right, rectTemp.top, rectTemp.right + kSplitterWidth, rcMain.Height());
	m_rcSplitLeftNew.SetRect(rectTemp.right, rectTemp.top, rectTemp.right + kSplitterWidth, rcMain.Height());

	// �ƶ��м����λ��...
	if( m_lpMidView != NULL && m_lpMidView->m_hWnd != NULL ) {
		CRect rcView(m_rcSplitLeftSrc.right, m_rcSplitLeftSrc.top, m_rcSplitRightSrc.left, m_rcSplitLeftSrc.bottom);
		m_lpMidView->doMoveWindow(rcView);
	}
}
//
// ���ָ����ܷ��ƶ�...
BOOL CHaoYiView::IsLeftCanSplit(CPoint & point)
{
	return (( point.x < QR_CODE_CY || point.x > QR_CODE_CX * 2 ) ? false : true);
}
//
// �Ҳ�ָ��������¼�...
void CHaoYiView::OnRightSplitEnd(int nOffset)
{
	CRect	rectTemp, rcMain;

	// �ƶ��Ҳ���ʾ��...
	m_RightView.GetWindowRect(rectTemp);
	this->ScreenToClient(rectTemp);
	m_RightView.SetWindowPos(NULL, rectTemp.left + nOffset, rectTemp.top, rectTemp.Width() - nOffset, rectTemp.Height(), SWP_NOZORDER);
	m_RightView.Invalidate(true);

	// �����Ҳ�ָ���λ��...
	this->GetClientRect(rcMain);
	m_RightView.GetWindowRect(rectTemp);
	this->ScreenToClient(rectTemp);

	m_rcSplitRightSrc.SetRect(rectTemp.left - kSplitterWidth, rectTemp.top, rectTemp.left, rcMain.Height());
	m_rcSplitRightNew.SetRect(rectTemp.left - kSplitterWidth, rectTemp.top, rectTemp.left, rcMain.Height());

	// �ƶ��м���ʾ��...
	if( m_lpMidView != NULL && m_lpMidView->m_hWnd != NULL ) {
		CRect rcView(m_rcSplitLeftSrc.right, m_rcSplitLeftSrc.top, m_rcSplitRightSrc.left, m_rcSplitLeftSrc.bottom);
		m_lpMidView->doMoveWindow(rcView);
	}
}
//
// �Ҳ�ָ����ܷ��ƶ�...
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
	// ��࣬�Ƿ���Ҫ�����϶�ͼ��...
	if( m_rcSplitLeftNew.PtInRect(point) || m_bLeftDraging ) {
		::SetCursor(m_hCurHorSplit);
	}
	// ��࣬��������...
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
	// �Ҳ࣬�Ƿ���Ҫ�����϶�ͼ��...
	if( m_rcSplitRightNew.PtInRect(point) || m_bRightDraging ) {
		::SetCursor(m_hCurHorSplit);
	}
	// �Ҳ࣬��������...
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
// ������Ƶͨ��...
CCamera * CHaoYiView::FindDBCameraByID(int nDBCameraID)
{
	return ((m_lpMidView != NULL ) ? m_lpMidView->FindDBCameraByID(nDBCameraID) : NULL);
}
//
// ����������ͷ������...
void CHaoYiView::doMulticastData(GM_MapData & inNetData, CAMERA_TYPE inType)
{
	// ���ݻ��⣬�������ݼ�����У�Ϊ�˱������...
	OSMutexLocker theLocker(&m_Mutex);
	if( inType == kCameraHK ) {
		// �����ֺ�������ͷ�Ĵ������...
		m_HKListData.push_back(inNetData);
		this->PostMessage(WM_FIND_HK_CAMERA);
	} else if( inType == kCameraDH ) {
		// �����ִ�����ͷ�Ĵ������...
		//m_DHListData.push_back(inNetData);
		//this->PostMessage(WM_FIND_DH_CAMERA);
	}
}
//
// ��Ӧ�ҵ���������Ϣ�¼�...
LRESULT	CHaoYiView::OnMsgFindHKCamera(WPARAM wParam, LPARAM lParam)
{
	// ���ݻ��⣬ȡ��һ����������...
	OSMutexLocker theLocker(&m_Mutex);
	GM_Error theErr = GM_NoErr;
	if( m_HKListData.size() < 0 || m_lpMidView == NULL )
		return S_OK;
	// �����ݽ��д���...
	ASSERT( m_HKListData.size() > 0 && m_lpMidView != NULL );
	int outDBCameraID = 0;
	GM_MapData & theNetData = m_HKListData.front();
	theErr = m_lpMidView->doCameraUDPData(theNetData, kCameraHK, outDBCameraID);
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
	}
	// �����ݴӶ������Ƴ���...
	m_HKListData.pop_front();
	return S_OK;
}
//
// ����վ��ȡ�ɼ�������...
BOOL CHaoYiView::doWebGatherConfig()
{
	if( m_lpWebThread == NULL )
		return false;
	return m_lpWebThread->doWebGatherConfig();
}
//
// ����վע������ͷ...
BOOL CHaoYiView::doWebRegCamera(GM_MapData & inData)
{
	if( m_lpWebThread == NULL )
		return false;
	return m_lpWebThread->doWebRegCamera(inData);
}
//
// ����վɾ������ͷ...
BOOL CHaoYiView::doWebDelCamera(string & inDeviceSN)
{
	if( m_lpWebThread == NULL )
		return false;
	return m_lpWebThread->doWebDelCamera(inDeviceSN);
}
//
// ��Ӧ VK_Escape ϵͳ��ݼ�...
void CHaoYiView::OnVKEscape()
{
	if( m_lpMidView != NULL ) {
		m_lpMidView->ChangeFullScreen(false);
	}
}
//
// ��Ӧ VK_F ϵͳ��ݼ�...
void CHaoYiView::OnVKFull()
{
	if( m_lpMidView != NULL ) {
		m_lpMidView->ChangeFullScreen(!m_lpMidView->IsFullScreen());
	}
}
//
// ���� ���ͨ�� �˵�״̬...
void CHaoYiView::OnCmdUpdateAddDVR(CCmdUI *pCmdUI)
{
	// �Ѿ�����վע��ɹ�֮�����Ч...
	pCmdUI->Enable(m_lpFastThread != NULL ? true : false);
}
//
// ���� �޸�ͨ�� �˵�״̬...
void CHaoYiView::OnCmdUpdateModDVR(CCmdUI *pCmdUI)
{
	// ͨ����ǰ�����Ų���DVR����...
	CCamera * lpCamera = this->FindDBCameraByID(m_nFocusDBCamera);
	if( lpCamera == NULL ) {
		pCmdUI->Enable(false);
		return;
	}
	// ���ò˵�״̬...
	pCmdUI->Enable(true);
}
//
// ���� ɾ��ͨ�� �˵�״̬...
void CHaoYiView::OnCmdUpdateDelDVR(CCmdUI *pCmdUI)
{
	// ͨ����ǰ�����Ų���DVR����...
	CCamera * lpCamera = this->FindDBCameraByID(m_nFocusDBCamera);
	//if( lpCamera == NULL || lpCamera->IsCameraDevice() ) {
	//2017.10.31 - �������Ͷ����Ա�ɾ��...
	if( lpCamera == NULL ) {
		pCmdUI->Enable(false);
		return;
	}
	// ���ò˵�״̬ => һ������ת������...
	//ASSERT( !lpCamera->IsCameraDevice() );
	pCmdUI->Enable(true);
}
//
// ���� ��¼ͨ�� �˵�״̬...
void CHaoYiView::OnCmdUpdateLoginDVR(CCmdUI *pCmdUI)
{
	// ͨ����ǰ�����Ų���DVR����...
	CCamera * lpCamera = this->FindDBCameraByID(m_nFocusDBCamera);
	if( lpCamera == NULL ) {
		pCmdUI->Enable(false);
		return;
	}
	// ���ò˵�״̬ => ���¼״̬�෴...
	pCmdUI->Enable(!lpCamera->IsLogin());
}
//
// ���� ע��ͨ�� �˵�״̬...
void CHaoYiView::OnCmdUpdateLogoutDVR(CCmdUI *pCmdUI)
{
	// ͨ����ǰ�����Ų���DVR����...
	CCamera * lpCamera = this->FindDBCameraByID(m_nFocusDBCamera);
	if( lpCamera == NULL ) {
		pCmdUI->Enable(false);
		return;
	}
	// ���ò˵�״̬ => ���¼״̬��ͬ...
	pCmdUI->Enable(lpCamera->IsLogin());
}
//
// ����˵� => ��¼ͨ��...
void CHaoYiView::OnLoginDVR()
{
	// ���ݽ����ȡ����...
	CCamera * lpCamera = this->FindDBCameraByID(m_nFocusDBCamera);
	if( lpCamera == NULL || lpCamera->IsLogin() )
		return;
	// ������ת��ģʽ�ĵ�¼...
	GM_Error theErr = GM_NoErr;
	if( !lpCamera->IsCameraDevice() ) {
		theErr = lpCamera->doStreamLogin();
		if( theErr != GM_NoErr ) {
			MsgLogGM(theErr);
		}
		return;
	}
	// ��������ͷģʽ�ĵ�¼...
	ASSERT( lpCamera->IsCameraDevice() );
	if( m_RightView.m_hWnd != NULL ) {
		m_RightView.doDeviceLogin();
	}
}
//
// ����˵� => ע��ͨ��...
void CHaoYiView::OnLogoutDVR()
{
	// ���ݽ����ȡ����...
	CCamera * lpCamera = this->FindDBCameraByID(m_nFocusDBCamera);
	if( lpCamera == NULL )
		return;
	// ������ת��ģʽ��ע��...
	GM_Error theErr = GM_NoErr;
	if( !lpCamera->IsCameraDevice() ) {
		theErr = lpCamera->doStreamLogout();
		if( theErr != GM_NoErr ) {
			MsgLogGM(theErr);
		}
		return;
	}
	// ��������ͷģʽ�ĵ�¼...
	ASSERT( lpCamera->IsCameraDevice() );
	if( m_RightView.m_hWnd != NULL ) {
		m_RightView.doDeviceLogout();
	}
}
//
// ����˵� => ���ͨ��...
void CHaoYiView::OnAddDVR()
{
	// ��Ҫ����Ĭ�ϵ�ͨ������...
	CDlgPushDVR	dlg(false, -1, false, this);
	dlg.m_strDVRName = m_lpMidView->GetDefaultCameraName().c_str();
	// ��ʾ��ӶԻ��� => ��ת��ģʽ...
	if( IDOK != dlg.DoModal() )
		return;
	// ע�⣺�����Ӳ���Ĺ���һ��...
	// ��Ӳ����ɹ�����ʼ�����µ�ͨ��...
	int outDBCameraID = 0;
	GM_Error theErr = GM_NoErr;
	GM_MapData & theData = dlg.GetPushData();
	theErr = m_lpMidView->doCameraUDPData(theData, kCameraNO, outDBCameraID);
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
		return;
	}
	// �ж�ͨ������Ƿ���ȷ����...
	if( outDBCameraID <= 0 ) {
		MsgLogGM(theErr);
		return;
	}
	// �����ȱ��潹�㣬����ᷢ����λ����...
	m_nFocusDBCamera = outDBCameraID;
	m_lpMidView->doLeftFocus(outDBCameraID);
	m_RightView.doFocusDBCamera(outDBCameraID);
	// ��ӳɹ���ֱ������...
	this->OnLoginDVR();
}
//
// ������������Ӳ���...
LRESULT CHaoYiView::OnMsgAddByTransmit(WPARAM wParam, LPARAM lParam)
{
	//////////////////////////////////////////////////////
	// ע�⣺��������վ�����ָ���������վ�㱨��...
	//////////////////////////////////////////////////////
	int nDBCameraID = wParam;
	GM_MapData theWebData;
	CCamera * lpCamera = NULL;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetCamera(nDBCameraID, theWebData);
	lpCamera = m_lpMidView->BuildWebCamera(theWebData);
	if( lpCamera == NULL )
		return S_OK;
	// �����ȱ��潹�㣬����ᷢ����λ����...
	m_nFocusDBCamera = nDBCameraID;
	m_lpMidView->doLeftFocus(nDBCameraID);
	m_RightView.doFocusDBCamera(nDBCameraID);
	// ��ӳɹ���ֱ������...
	this->OnLoginDVR();
	return S_OK;
}
//
// ����������ɾ������...
LRESULT CHaoYiView::OnMsgDelByTransmit(WPARAM wParam, LPARAM lParam)
{
	//////////////////////////////////////////////////////
	// ע�⣺��������վ�����ָ���������վ�㱨��...
	//////////////////////////////////////////////////////
	int nDBCameraID = wParam;
	// ��ര�ڷ���ɾ������...
	this->doDelTreeFocus(nDBCameraID);
	// �м䴰�ڷ���ɾ������...
	m_lpMidView->doDelDVR(nDBCameraID);
	// �����ļ�����ɾ������...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.doDelDVR(nDBCameraID);
	return S_OK;
}
//
// ����˵� => ɾ��ͨ��...
void CHaoYiView::OnDelDVR()
{
	// �жϵ�ǰͨ���Ƿ���Ч...
	CCamera * lpCamera = this->FindDBCameraByID(m_nFocusDBCamera);
	//if( lpCamera == NULL || lpCamera->IsCameraDevice() )
	//2017.10.31 - ����ɾ������ͷ�豸...
	if( lpCamera == NULL )
		return;
	// �õ�Ҫɾ�����ڵ����кţ�֪ͨ��վʱʹ�ã����������ã���Ϊ�ᱻɾ��...
	int nDBCameraID = m_nFocusDBCamera;
	string strDeviceSN = lpCamera->GetDeviceSN();
	// ɾ��֮ǰ��ѯ��...
	if( ::MessageBox(this->m_hWnd, "ȷʵҪɾ����ǰѡ�е�ͨ����", "ȷ��", MB_OKCANCEL | MB_ICONWARNING) == IDCANCEL )
		return;
	// ��ര�ڷ���ɾ������...
	this->doDelTreeFocus(nDBCameraID);
	// �м䴰�ڷ���ɾ������...
	m_lpMidView->doDelDVR(nDBCameraID);
	// ֪ͨ��վ����ɾ������...
	this->doWebDelCamera(strDeviceSN);
	// �����ļ�����ɾ������...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.doDelDVR(nDBCameraID);
}
//
// ɾ��������ν���ڵ�...
void CHaoYiView::doDelTreeFocus(int nDBCameraID)
{
	HTREEITEM hChildItem = m_DeviceTree.GetChildItem(m_hRootItem);
	while( hChildItem != NULL ) {
		// ������һ�£�ֱ��ɾ������ڵ�...
		if( nDBCameraID == m_DeviceTree.GetItemData(hChildItem) ) {
			m_DeviceTree.DeleteItem(hChildItem);
			break;
		}
		// ������ǣ�����Ѱ����һ���ڵ�...
		hChildItem = m_DeviceTree.GetNextSiblingItem(hChildItem);
	}
}
//
// ����˵� => �޸�ͨ��...
void CHaoYiView::OnModDVR()
{
	CCamera * lpCamera = this->FindDBCameraByID(m_nFocusDBCamera);
	if( m_nFocusDBCamera <= 0 || lpCamera == NULL )
		return;
	ASSERT( m_nFocusDBCamera > 0 && lpCamera != NULL );
	STREAM_PROP theProp = lpCamera->GetStreamProp();
	if( lpCamera->IsCameraDevice() ) {
		// ����豸ͨ�����޸Ĳ��� => ����ͷ�豸ֻ���޸�...
		CDlgSetDVR dlg(m_nFocusDBCamera, lpCamera->IsLogin(), this);
		if( IDOK != dlg.DoModal() )
			return;
		// �Ի��򷵻�֮ǰ���޸���Ϣ�Ѿ����浽����...
	} else {
		// �����ת��ͨ�����޸Ĳ���...
		CDlgPushDVR dlg(true, m_nFocusDBCamera, lpCamera->IsLogin(), this);
		if( IDOK != dlg.DoModal() )
			return;
		// �Ի��򷵻�֮ǰ���޸���Ϣ�Ѿ����浽����...
		theProp = (dlg.m_bFileMode ? kStreamMP4File : kStreamUrlLink);
	}
	// ������Ҫ���޸���Ϣ��֪ͨ��վ�������ݿ�...
	GM_MapData theMapWeb;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetCamera(m_nFocusDBCamera, theMapWeb);
	this->doWebRegCamera(theMapWeb);
	// �����õ�ͨ����ϢӦ�õ��м���浱��...
	CString strDVRName = theConfig.GetDBCameraTitle(m_nFocusDBCamera);
	lpCamera->UpdateWndTitle(theProp, strDVRName);
	// �����õ�ͨ����ϢӦ�õ��Ҳ���浱��...
	m_RightView.doFocusDBCamera(m_nFocusDBCamera);
	// ��ȡ���ѡ�еĽڵ�...
	HTREEITEM hSelItem = m_DeviceTree.GetSelectedItem();
	if( hSelItem == NULL || m_DeviceTree.GetItemData(hSelItem) != m_nFocusDBCamera )
		return;
	// �޸����Tree�ı�������...
	m_DeviceTree.SetItemText(hSelItem, strDVRName);
	// �޸���ϣ�ֱ�����и�ͨ��...
	this->OnLoginDVR();
}
//
// ��Ӧ doSetCameraName ����������ͷ�仯�¼�...
void CHaoYiView::UpdateFocusTitle(int nDBCameraID, CString & strTitle)
{
	// �����ǰ���㴰�ڱ�����������õı��ر��һ�£����н�������...
	if( m_nFocusDBCamera == nDBCameraID ) {
		m_RightView.doFocusDBCamera(nDBCameraID);
	}
	// �������ڵ������...
	HTREEITEM hChildItem = m_DeviceTree.GetChildItem(m_hRootItem);
	while( hChildItem != NULL ) {
		// ������һ�£��޸�����ڵ�ı�������...
		if( nDBCameraID == m_DeviceTree.GetItemData(hChildItem) ) {
			m_DeviceTree.SetItemText(hChildItem, strTitle);
			break;
		}
		// ������ǣ�����Ѱ����һ���ڵ�...
		hChildItem = m_DeviceTree.GetNextSiblingItem(hChildItem);
	}
}
//
// ����˵� => ϵͳ����...
void CHaoYiView::OnSysSet()
{
	CDlgSetSys dlg(this);
	if( IDOK != dlg.DoModal() )
		return;
	// ���±���������...
	this->doUpdateFrameTitle();
	// �����Ӵ洢�����������ǻỰ��Ч��������ɾ���Ự����...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	if( !theConfig.GetAutoLinkFDFS() ) {
		this->DelByEventThread(m_lpTrackerSession);
		m_lpTrackerSession = NULL;
		this->DelByEventThread(m_lpStorageSession);
		m_lpStorageSession = NULL;
	}
	// �����վ��ַ��˿ڷ����仯����Ҫ�򱾴��ڷ����ؽ���Ϣ...
	if( dlg.IsWebChange() ) {
		this->PostMessage(WM_RELOAD_VIEW);
	}
	// �����������Զ�ͨ�����ø���...
}
//
// ������ͼ�����¼�...
void CHaoYiView::doGatherLogout()
{
	// ֪ͨ��վ�ɼ����˳�...
	if( m_lpWebThread == NULL )
		return;
	ASSERT( m_lpWebThread != NULL );
	m_lpWebThread->doWebGatherLogout();
}
//
// ��Ӧ������ͼ���ڵ��ؽ���Ϣ�¼�...
LRESULT CHaoYiView::OnMsgReloadView(WPARAM wParam, LPARAM lParam)
{
	CRect rectMid;
	// ��ȡ�м���ͼ�ľ�����...
	if( m_lpMidView != NULL ) {
		m_lpMidView->GetWindowRect(rectMid);
		this->ScreenToClient(rectMid);
	}
	// �ؽ����е���Դ���ݶ���...
	this->BuildResource();
	// �����м���ͼ�ľ���λ��...
	if( m_lpMidView != NULL ) {
		m_lpMidView->doMoveWindow(rectMid);
	}
	return S_OK;
}
//
// ��������֮ǰ�Ĳ��� => �޸Ĵ�������...
BOOL CHaoYiView::PreCreateWindow(CREATESTRUCT& cs)
{
	if( !CFormView::PreCreateWindow(cs) )
		return FALSE;
	//cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	return TRUE;
}
