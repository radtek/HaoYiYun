
#include "stdafx.h"
#include "MidView.h"
#include "BitItem.h"
#include "UtilTool.h"
#include "base64x.h"
#include "..\Camera.h"
#include "..\resource.h"
#include "..\HaoYiView.h"
#include "..\XmlConfig.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

BEGIN_MESSAGE_MAP(CMidView, CWnd)
	ON_WM_ERASEBKGND()
	ON_WM_CREATE()
	ON_WM_SIZE()
END_MESSAGE_MAP()

CMidView::CMidView(CHaoYiView * lpParent)
  : m_bIsFullScreen(false)
  , m_rcOrigin(0, 0, 0, 0)
  , m_lpParentDlg(lpParent)
{
	ASSERT( m_lpParentDlg != NULL );
	memset(m_BitArray, 0, sizeof(void*) * CVideoWnd::kBitNum);
}

CMidView::~CMidView()
{
	this->DestroyResource();
}
//
// ������Դ...
void CMidView::BuildResource()
{
	// ��������...
	LOGFONT	 lf = {0};
	lf.lfHeight = 14;
	lf.lfWeight = 0;
	strcpy(lf.lfFaceName, "����");
	m_VideoFont.CreateFontIndirect(&lf);
	ASSERT( m_VideoFont.m_hObject != NULL );
	// ����������ť...
	memset(m_BitArray, 0, sizeof(void *) * CVideoWnd::kBitNum);
	m_BitArray[CVideoWnd::kZoomOut] = new CBitItem(IDB_VIDEO_MAX, 18, 18);
	m_BitArray[CVideoWnd::kZoomIn]	= new CBitItem(IDB_VIDEO_MIN, 18, 18);
	m_BitArray[CVideoWnd::kClose]   = new CBitItem(IDB_VIDEO_CLOSE, 18, 18);
	// ����Ĭ�ϵ���ʾ��Ϣ...
	m_strNotice = "����ע��ɼ���...";
}
//
// ������Դ...
void CMidView::DestroyResource()
{
	// ɾ��������Ƶ����...
	GM_MapVideo::iterator itorItem = m_MapVideo.begin();
	for( itorItem = m_MapVideo.begin(); itorItem != m_MapVideo.end(); ++itorItem) {
		CVideoWnd * lpVideo = itorItem->second;
		delete lpVideo;	lpVideo = NULL;
	}
	m_MapVideo.clear();

	// ɾ����ť����...
	for(int i = 0; i < CVideoWnd::kBitNum; i++) {
		if( m_BitArray[i] == NULL )
			continue;
		delete m_BitArray[i];
		m_BitArray[i] = NULL;
	}
	// ɾ���������...
	m_VideoFont.DeleteObject();
	// �Իٴ��ڶ���...
	if( this->m_hWnd != NULL ) {
		this->DestroyWindow();
	}
}
//
// Create a new Left-View window...
BOOL CMidView::Create(UINT wStyle, const CRect & rect, CWnd * pParentWnd)
{
	//
	// 1.0 Register the window class object...
	LPCTSTR	lpWndName = NULL;
	lpWndName = AfxRegisterWndClass(CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW,
									AfxGetApp()->LoadStandardCursor(IDC_ARROW),
									NULL, NULL);//(HBRUSH)GetSysColorBrush(COLOR_BTNFACE), NULL);
	if( lpWndName == NULL )
		return FALSE;
	//
	// 2.0 WS_TABSTOP �ǽ�����̴����л�������...
	wStyle |= WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_TABSTOP;
	ASSERT( m_lpParentDlg == pParentWnd );
	return CWnd::Create(lpWndName, NULL, wStyle, rect, pParentWnd, ID_RIGHT_VIEW_BEGIN);
}

int CMidView::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	// ������Ҫ���ڵ���Դ...
	this->BuildResource();
	
	return 0;
}
//
// ͨ�������ļ��������д���...
void CMidView::BuildVideoByXml()
{
	// ��ȡxml�����ļ���ļ���б�...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	GM_MapNodeCamera & theMapCamera = theConfig.GetNodeCamera();
	GM_MapNodeCamera::iterator itorItem;
	// �������ͨ���б�...
	for(itorItem = theMapCamera.begin(); itorItem != theMapCamera.end(); ++itorItem) {
		GM_MapData & theData = itorItem->second;
		this->BuildXmlCamera(theData);
	}
}
//
// ͨ���豸���кŲ�������ͷ...
CCamera	* CMidView::FindCameraBySN(string & inDeviceSN)
{
	CCamera   * lpCamera = NULL;
	CVideoWnd * lpVideo = NULL;
	GM_MapVideo::iterator  itorItem;
	for(itorItem = m_MapVideo.begin(); itorItem != m_MapVideo.end(); ++itorItem) {
		lpVideo = itorItem->second;
		if( lpVideo == NULL )
			continue;
		ASSERT( lpVideo != NULL );
		lpCamera = lpVideo->GetCamera();
		if( lpCamera == NULL )
			continue;
		// �ȶ��豸���к��Ƿ�һ��...
		string & strCSN = lpCamera->GetDeviceSN();
		if( strCSN == inDeviceSN )
			return lpCamera;
	}
	return NULL;
}
//
// ͨ������ͷ��Ų�����Ƶ����...
CCamera * CMidView::FindCameraByID(int nCameraID)
{
	GM_MapVideo::iterator itorItem = m_MapVideo.find(nCameraID);
	if( itorItem == m_MapVideo.end() )
		return NULL;
	ASSERT( itorItem != m_MapVideo.end() );
	ASSERT( itorItem->first == nCameraID );
	return itorItem->second->GetCamera();
}
//
// ������������ͷ�������ݰ�...
GM_Error CMidView::doCameraUDPData(GM_MapData & inNetData, CAMERA_TYPE inType)
{
	// �鿴�Ƿ�����豸���к��ֶ�...
	GM_Error theErr = GM_No_Xml_Node;
	GM_MapData::iterator itorData = inNetData.find("DeviceSN");
	if( itorData == inNetData.end() ) {
		MsgLogGM(theErr);
		return theErr;
	}
	// ��ȡ���ͨ��Ψһ��ʶ��Ϣ => �豸���...
	string & strDeviceSN = itorData->second;
	CCamera * lpCamera = this->FindCameraBySN(strDeviceSN);
	// �ҵ���Ӧ������ͷ������������Ϣ...
	if( lpCamera != NULL ) {
		if( inType == kCameraNO ) {
			MsgLogGM(theErr);
			return theErr;
		}
		// ����������ͷ����...
		ASSERT( inType != kCameraNO );
		return lpCamera->ForUDPData(inNetData);
	}
	// �������ͷ����Ϊ�գ��ж��Ƿ񳬹����豸֧������...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	if( m_MapVideo.size() >= theConfig.GetMaxCamera() )
		return GM_NoErr;
	// �������ͷ����Ϊ�գ�����һ���µĴ��ڶ���...
	return this->AddNewCamera(inNetData, inType);
}
//
// ɾ��ָ������ͷ...
void CMidView::doDelDVR(int nCameraID)
{
	GM_MapVideo::iterator itorItem = m_MapVideo.find(nCameraID);
	if( itorItem == m_MapVideo.end() )
		return;
	// ɾ����Ӧ����Ƶ���ڶ���...
	CVideoWnd * lpVideo = itorItem->second;
	delete lpVideo; lpVideo = NULL;
	m_MapVideo.erase(itorItem);
	// ���Ӵ��ڽ��в����ػ����...
	CRect rcRect;
	this->GetClientRect(rcRect);
	if( rcRect.Width() > 0 && rcRect.Height() > 0 ) {
		this->LayoutVideoWnd(rcRect.Width(), rcRect.Height());
		this->Invalidate(true);
	}
	// �ҵ���ǰ��һ�����ڽ��н����¼�����...
	if( m_MapVideo.size() <= 0 )
		return;
	itorItem = m_MapVideo.begin();
	lpVideo = itorItem->second;
	lpVideo->doFocusAction();
}
//
// �õ���һ�����ڱ��...
int CMidView::GetNextAutoID(int nCurCameraID)
{
	GM_MapVideo::iterator itorFirst = m_MapVideo.begin();
	// ���û�нڵ㣬ֱ�ӷ��ؿ�ʼ���...
	if( itorFirst == m_MapVideo.end() ) {
		return DEF_CAMERA_START_ID;
	}
	ASSERT( itorFirst != m_MapVideo.end() );
	// �������Ľڵ�����Ч�ģ����ص�һ����Ч�ڵ�...
	GM_MapVideo::iterator itorItem = m_MapVideo.find(nCurCameraID);
	if( itorItem == m_MapVideo.end() ) {
		return itorFirst->first;
	}
	// �����һ���ڵ�����Ч�ģ��ع�����һ���ڵ�...
	if((++itorItem) == m_MapVideo.end() ) {
		return itorFirst->first;
	}
	// ��һ���ڵ���Ч�����ض�Ӧ�ı��...
	ASSERT( itorItem != m_MapVideo.end() );
	return itorItem->first;
}
string CMidView::GetDefaultCameraName()
{
	string strDefName;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	switch( theConfig.GetWebType() )
	{
	case kCloudRecorder: strDefName = "ֱ��ͨ��"; break;
	case kCloudMonitor:	 strDefName = "���ͨ��"; break;
	default:			 strDefName = "ֱ��ͨ��"; break;
	}
	return strDefName;
}
//
// ���һ����Ƶ����...
GM_Error CMidView::AddNewCamera(GM_MapData & inNetData, CAMERA_TYPE inType)
{
	// �ȴ����м����в�������ŵĴ���...
	int nCameraID = DEF_CAMERA_START_ID;
	GM_MapVideo::reverse_iterator itorEnd = m_MapVideo.rbegin();
	if( itorEnd != m_MapVideo.rend() ) {
		nCameraID = itorEnd->first + 1;
	}
	GM_MapData theMapLoc;
	// ���ñ�����Ϣ��������ͷ���ͣ��ڴ����ǲ�ת����UTF8...
	CString strTitle, strID, strType, strStreamProp;
	string strDefName = this->GetDefaultCameraName();
	strID.Format("%d", nCameraID);
	theMapLoc["ID"] = strID;
	strTitle.Format("%s - %d", strDefName.c_str(), nCameraID);
	theMapLoc["Name"] = strTitle.GetString();
	strType.Format("%d", inType);
	theMapLoc["CameraType"] = strType;
	if( inType == kCameraNO ) {
		// �������ת�����ݣ���ȡ��Ҫ���ֶ�������...
		theMapLoc["StreamProp"] = inNetData["StreamProp"];
		theMapLoc["StreamUrl"] = inNetData["StreamUrl"];
		theMapLoc["StreamMP4"] = inNetData["StreamMP4"];
		theMapLoc["StreamLoop"] = inNetData["StreamLoop"];
		theMapLoc["StreamAuto"] = inNetData["StreamAuto"];
		theMapLoc["DeviceSN"] = inNetData["DeviceSN"];
	} else {
		// ���������ͷ���ͣ���ȡ���õ��ֶ���ϳ���Ҫ��������Ϣ...
		ASSERT( inType == kCameraHK || inType == kCameraDH );
		strStreamProp.Format("%d", kStreamDevice);
		theMapLoc["StreamProp"] = strStreamProp;
		theMapLoc["DeviceType"] = inNetData["DeviceType"];
		theMapLoc["DeviceDescription"] = inNetData["DeviceDescription"];
		theMapLoc["DeviceSN"] = inNetData["DeviceSN"];
		theMapLoc["CommandPort"] = inNetData["CommandPort"];
		theMapLoc["HttpPort"] = inNetData["HttpPort"];
		theMapLoc["MAC"] = inNetData["MAC"];
		theMapLoc["IPv4Address"] = inNetData["IPv4Address"];
		theMapLoc["BootTime"] = inNetData["BootTime"];
		theMapLoc["DigitalChannelNum"] = inNetData["DigitalChannelNum"];
		theMapLoc["DiskNumber"] = inNetData["DiskNumber"];
		// ����Ĭ�ϵ��û���������(base64)...
		TCHAR szEncode[MAX_PATH] = {0};
		int nEncLen = Base64encode(szEncode, DEF_LOGIN_PASS_HK, strlen(DEF_LOGIN_PASS_HK));
		theMapLoc["LoginUser"] = DEF_LOGIN_USER_HK;
		theMapLoc["LoginPass"] = szEncode;
		// ���ÿ���OSD�Ϳ�������...
		theMapLoc["OpenOSD"] = "1";
		theMapLoc["OpenMirror"] = "0";
	}
	// ����վע������ͷ��ע��ɹ����ܴ���...
	ASSERT( m_lpParentDlg != NULL );
	GM_Error  theErr = GM_NotImplement;
	if( !m_lpParentDlg->doWebRegCamera(theMapLoc) ) {
		MsgLogGM(theErr);
		return theErr;
	}
	// ֱ�Ӵ���һ���µ���Ƶ����...
	CCamera * lpCamera = this->BuildXmlCamera(theMapLoc);
	if( lpCamera == NULL ) {
		MsgLogGM(theErr);
		return theErr;
	}
	// �������ת��ģʽ��ֱ�Ӵ��̷���...
	if( inType == kCameraNO ) {
		CXmlConfig & theConfig = CXmlConfig::GMInstance();
		theConfig.SetCamera(nCameraID, theMapLoc);
		theConfig.GMSaveConfig();
		return GM_NoErr;
	}
	// ���¼��ͨ������������...
	ASSERT( inType == kCameraHK || inType == kCameraDH );
	return lpCamera->ForUDPData(inNetData);
}
//
// ����XML�������ݴ������ͨ��...
CCamera * CMidView::BuildXmlCamera(GM_MapData & inXmlData)
{
	GM_MapData::iterator itorID, itorName;
	itorID = inXmlData.find("ID");
	itorName = inXmlData.find("Name");
	if((itorID == inXmlData.end()) || (itorName == inXmlData.end())) {
		MsgLogGM(GM_No_Xml_Node);
		return NULL;
	}
	// ������Ƶ���ڱ��...
	int nCameraID = atoi(itorID->second.c_str());
	int nVideoWndID = ID_VIDEO_WND_BEGIN + nCameraID;
	CString strTitle = itorName->second.c_str();
	// ����һ���µ���Ƶ����...
	ASSERT( m_BitArray != NULL && nCameraID > 0 );
	CVideoWnd * lpVideo = new CVideoWnd(m_BitArray, nVideoWndID);
	lpVideo->SetTitleFont(&m_VideoFont);
	lpVideo->SetWebTitleText(strTitle);
	// �������ڲ����浽�б���...
	lpVideo->Create(WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
	ASSERT( lpVideo->m_hWnd != NULL );
	m_MapVideo[nCameraID] = lpVideo;
	// ���Ӵ��ڽ��в����ػ����...
	CRect rcRect;
	this->GetClientRect(rcRect);
	if( rcRect.Width() > 0 && rcRect.Height() > 0 ) {
		this->LayoutVideoWnd(rcRect.Width(), rcRect.Height());
		this->Invalidate(true);
	}
	// �������õ����ݣ���������ͷ...
	lpVideo->BuildCamera(inXmlData);
	// 2017.08.06 - by jackey => ���󴴽���ϣ������ô��ڱ���...
	lpVideo->SetDispTitleText(strTitle);
	// ��Ҫ֪ͨ���������µ�ͨ����������...
	ASSERT( m_lpParentDlg != NULL );
	m_lpParentDlg->OnCreateCamera(nCameraID, strTitle);
	// ���ظô��ڶ�Ӧ������ͷ...
	return lpVideo->GetCamera();
}
//
// ��վ��Ȩ��֤�������...
void CMidView::OnWebAuthResult(int nType, BOOL bAuthOK)
{
	// ���ݲ�ͬ״̬����ʾ��ͬ����Ϣ...
	if( nType == kAuthRegiter ) {
		m_strNotice = bAuthOK ? "������֤��վ��Ȩ..." : "��վע��ʧ�ܣ�������վ����...";
	} else if( nType == kAuthExpired ) {
		m_strNotice = bAuthOK ? "�����������������..." : "��վ��Ȩ���ڣ���֤ʧ��...";
	}
	// ���¸��´��ڱ���...
	this->Invalidate(true);
}
//
// �����������¼�...
BOOL CMidView::OnEraseBkgnd(CDC* pDC) 
{
	// ��ȡ��������...
	CRect rcRect;
	this->GetClientRect(rcRect);
	// ��䱳��ɫ...
	pDC->FillSolidRect(rcRect, RGB(32,32,32));
	// ���Ʊ߿�...
	//rcRect.DeflateRect(1, 1);
	//pDC->Draw3dRect(rcRect, ::GetSysColor(COLOR_BTNHIGHLIGHT), ::GetSysColor(COLOR_BTNSHADOW) );
	// ���ƴ��ڱ�������...
	this->DrawBackLine(pDC);
	// ��ӡ������Ϣ...
	this->DrawNotice(pDC);
	return TRUE;
}
//
// ��ӡ������Ϣ...
void CMidView::DrawNotice(CDC * pDC)
{
	CRect   rcRect;
	CFont   fontLogo;
	CFont * pOldFont = NULL;
	if( m_MapVideo.size() > 0 )
		return;
	ASSERT( m_MapVideo.size() <= 0 );
	// �������壬��ȡ������ʾ����...
	fontLogo.CreateFont(40, 0, 0, 0, FW_LIGHT, false, false,0,0,0,0,0,0, "����");
	this->GetClientRect(&rcRect);
	// ���ƾ�������...
	pOldFont = pDC->SelectObject(&fontLogo);
	pDC->SetTextColor( RGB(255, 255, 0) );
	pDC->DrawText(m_strNotice, rcRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
	// ��������...
	pDC->SelectObject(pOldFont);
	fontLogo.DeleteObject();
	fontLogo.Detach();
}
//
// ���Ʊ�����ɫ�߿��ü��������λ��...
void CMidView::DrawBackLine(CDC * pDC)
{
	// �����б�������0...
	GM_MapVideo::iterator itorItem = m_MapVideo.begin();
	if( itorItem == m_MapVideo.end() )
		return;
	// ��ȡ�ͻ�������...
	CRect rcRect;
	this->GetClientRect(rcRect);
	// ���ȣ�������Ҫ���С�������...
	float fData = sqrt(m_MapVideo.size() * 1.0f);
	int nCol  = (int)fData + (((fData - (int)fData) == 0.0f) ? 0 : 1);
	int nRow  = nCol;
	int	nWidth	= (rcRect.Width() - 1) / nCol - 1;
	int	nHigh	= (rcRect.Height() - 1) / nRow - 1;
	// ������ˢ����...
	HBRUSH hBrush = ::CreateSolidBrush(RGB(89, 147, 200));
	// ��ʼ���д������в���...
	for(int i = 0; i < nRow; ++i) {
		for(int j = 0; j < nCol; ++j) {
			// ������Ч�����㲢����λ�ã�������Screen����...
			CRect rcWinPos;
			int	nLeft = j * (nWidth + 1) + 1;
			int	nTop  = i * (nHigh  + 1) + 1;
			rcWinPos.SetRect(nLeft, nTop, nLeft + nWidth, nTop + nHigh);
			::FrameRect(pDC->m_hDC, rcWinPos, hBrush);
			this->ClientToScreen(rcWinPos);
			// ��������Ѿ�û����...
			if( itorItem == m_MapVideo.end() )
				continue;
			ASSERT( itorItem != m_MapVideo.end() );
			// ����ָ����Ч��������һ��...
			CVideoWnd * lpVideo = itorItem->second;
			if( lpVideo != NULL ) {
				lpVideo->m_rcWinPos = rcWinPos;
			}
			// �ƶ�����һ������...
			++itorItem;
		}
	}
	// ɾ����ˢ����...
	::DeleteObject(hBrush);
}
//
// ���� WM_SIZE ��Ϣ...
void CMidView::OnSize(UINT nType, int cx, int cy) 
{
	CWnd::OnSize(nType, cx, cy);
	if( cx <= 0 || cy <= 0 )
		return;
	this->LayoutVideoWnd(cx, cy);
}
//
// ������Ƶ����...
void CMidView::LayoutVideoWnd(int cx, int cy)
{
	// �����б�������0...
	GM_MapVideo::iterator itorItem = m_MapVideo.begin();
	if( itorItem == m_MapVideo.end() )
		return;
	// ���ȣ�������Ҫ���С�������...
	float fData = sqrt(m_MapVideo.size() * 1.0f);
	int nCol  = (int)fData + (((fData - (int)fData) == 0.0f) ? 0 : 1);
	int nRow  = nCol;
	int	nWidth	= (cx - 1) / nCol - 1;
	int	nHigh	= (cy - 1) / nRow - 1;
	// ��ʼ���д������в���...
	for(int i = 0; i < nRow; ++i) {
		for(int j = 0; j < nCol; ++j) {
			if( itorItem == m_MapVideo.end() )
				return;
			// ��Ƶ������Ч��������һ������...
			CVideoWnd * lpVideo = itorItem->second;
			if( lpVideo == NULL ) {
				++itorItem;
				continue;
			}
			int	nLeft = j * (nWidth + 1) + 1;
			int	nTop  = i * (nHigh  + 1) + 1;
			// Fix״̬������Ҫ�ƶ�����λ��...
			if( lpVideo->GetWndState() == CVideoWnd::kFixState ) {
				lpVideo->MoveWindow(nLeft, nTop, nWidth, nHigh);
			}
			// Ȼ�󣬼����һ������...
			++itorItem;
		}
	}
}
//
// �����д��ڶ��̶�����ҪƯ��...
void CMidView::FixVideoWnd()
{
	// ������Ƶ���ڣ����ݴ���״̬����...
	GM_MapVideo::iterator itorItem;
	for( itorItem = m_MapVideo.begin(); itorItem != m_MapVideo.end(); ++itorItem) {
		CVideoWnd * lpVideo = itorItem->second;
		if( lpVideo == NULL )
			continue;
		CVideoWnd::WND_STATE theState = lpVideo->GetWndState();
		if( theState == CVideoWnd::kFlyState || theState == CVideoWnd::kFullState ) {
			lpVideo->OnFlyToFixState();
		}
	}
}
//
// �ͷŽ��㴰��...
void CMidView::ReleaseFocus()
{
	GM_MapVideo::iterator itorItem;
	for( itorItem = m_MapVideo.begin(); itorItem != m_MapVideo.end(); ++itorItem) {
		CVideoWnd * lpVideo = itorItem->second;
		if( lpVideo == NULL )
			continue;
		lpVideo->ReleaseFocus();
	}
}
//
// ��Ӧ����������¼�...
void CMidView::doLeftFocus(int nCameraID)
{
	// ���ȣ����Ҷ�Ӧ����Ƶ���ڶ���...
	GM_MapVideo::iterator itorItem;
	itorItem = m_MapVideo.find(nCameraID);
	if( itorItem == m_MapVideo.end() )
		return;
	// ���ڶ�����Ч��ֱ�ӷ���...
	CVideoWnd * lpVideo = itorItem->second;
	if( lpVideo == NULL )
		return;
	// �ͷ����еĽ��㴰�ڣ����õ�ǰ����Ϊ����...
	this->ReleaseFocus();
	lpVideo->m_bFocus = true;
	lpVideo->DrawFocus(CVideoWnd::kFocusColor);
}
//
// �ƶ����ڣ��ᴥ�� OnSize �¼�...
void CMidView::doMoveWindow(CRect & rcArea)
{
	m_rcOrigin = rcArea;
	this->MoveWindow(&m_rcOrigin);
}
//
// ȫ����ʾ����...
BOOL CMidView::ChangeFullScreen( BOOL bFullScreen )
{
	if( m_lpParentDlg == NULL )
		return FALSE;	
	this->FixVideoWnd();
	CRect rcArea(0, 0, 0, 0);
	if( !bFullScreen ) {	
		this->ModifyStyleEx(WS_EX_TOOLWINDOW, 0);
		this->SetParent(m_lpParentDlg);		
		rcArea = m_rcOrigin;
		this->MoveWindow(&rcArea);
		m_bIsFullScreen = false;
		
		::SetWindowPos(m_hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
		//::PostMessage( m_lpParent->m_hWnd,WM_CMDWND_SHOWDOCK,TRUE,TRUE);
	} else {
		ASSERT( bFullScreen );
		this->ModifyStyleEx(0, WS_EX_TOOLWINDOW); 
		this->SetParent(NULL);
		rcArea.left	  = 0; rcArea.top = 0;
		rcArea.right  = GetSystemMetrics(SM_CXSCREEN);
		rcArea.bottom = GetSystemMetrics(SM_CYSCREEN);
		this->MoveWindow(&rcArea);
		m_bIsFullScreen = true;
		
		::SetWindowPos(m_hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);

// 		CRect rcClient;
// 		CDCT<false>	dc(this->GetDC());
// 		this->GetClientRect(&rcClient);
// 		dc.FillSolidRect(&rcClient, kBackColor);
// 		this->ReleaseDC(dc);
	}
	return TRUE;
}

BOOL CMidView::doWebStatCamera(int nDBCamera, int nStatus)
{
	if( m_lpParentDlg == NULL ) 
		return false;
	return m_lpParentDlg->doWebStatCamera(nDBCamera, nStatus);
}