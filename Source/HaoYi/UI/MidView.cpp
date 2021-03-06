
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
// 创建资源...
void CMidView::BuildResource()
{
	// 创建字体...
	LOGFONT	 lf = {0};
	lf.lfHeight = 14;
	lf.lfWeight = 0;
	strcpy(lf.lfFaceName, "宋体");
	m_VideoFont.CreateFontIndirect(&lf);
	ASSERT( m_VideoFont.m_hObject != NULL );
	// 创建三个按钮...
	memset(m_BitArray, 0, sizeof(void *) * CVideoWnd::kBitNum);
	m_BitArray[CVideoWnd::kZoomOut] = new CBitItem(IDB_VIDEO_MAX, 18, 18);
	m_BitArray[CVideoWnd::kZoomIn]	= new CBitItem(IDB_VIDEO_MIN, 18, 18);
	m_BitArray[CVideoWnd::kClose]   = new CBitItem(IDB_VIDEO_CLOSE, 18, 18);
	// 设置默认的提示信息...
	m_strNotice = "正在注册采集端，请稍等...";
}
//
// 销毁资源...
void CMidView::DestroyResource()
{
	// 删除所有视频窗口 => 这里不能用clear，会出现内存泄漏...
	GM_MapVideo::iterator itorItem = m_MapVideo.begin();
	while( itorItem != m_MapVideo.end() ) {
		CVideoWnd * lpVideo = itorItem->second;
		delete lpVideo;	lpVideo = NULL;
		m_MapVideo.erase(itorItem++);
	}
	// 删除按钮对象...
	for(int i = 0; i < CVideoWnd::kBitNum; i++) {
		if( m_BitArray[i] == NULL )
			continue;
		delete m_BitArray[i];
		m_BitArray[i] = NULL;
	}
	// 删除字体对象...
	m_VideoFont.DeleteObject();
	// 自毁窗口对象...
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
	// 2.0 WS_TABSTOP 是解决键盘窗口切换的问题...
	wStyle |= WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_TABSTOP;
	ASSERT( m_lpParentDlg == pParentWnd );
	return CWnd::Create(lpWndName, NULL, wStyle, rect, pParentWnd, ID_RIGHT_VIEW_BEGIN);
}

int CMidView::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	// 创建需要窗口的资源...
	this->BuildResource();
	
	return 0;
}
//
// 通过网站配置创建所有窗口...
void CMidView::BuildVideoByWeb()
{
	// 读取网站配置里的通道列表 => 本地不再存储配置...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	GM_MapNodeCamera & theMapCamera = theConfig.GetNodeCamera();
	GM_MapNodeCamera::iterator itorItem;
	// 创建网站配置通道列表...
	for(itorItem = theMapCamera.begin(); itorItem != theMapCamera.end(); ++itorItem) {
		GM_MapData & theData = itorItem->second;
		this->BuildWebCamera(theData);
	}
}
//
// 通过设备序列号查找摄像头...
CCamera	* CMidView::FindDBCameraBySN(string & inDeviceSN)
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
		// 比对设备序列号是否一致...
		string & strCSN = lpCamera->GetDeviceSN();
		if( strCSN == inDeviceSN )
			return lpCamera;
	}
	return NULL;
}
//
// 通过摄像头编号查找视频对象...
CCamera * CMidView::FindDBCameraByID(int nDBCameraID)
{
	GM_MapVideo::iterator itorItem = m_MapVideo.find(nDBCameraID);
	if( itorItem == m_MapVideo.end() )
		return NULL;
	ASSERT( itorItem != m_MapVideo.end() );
	ASSERT( itorItem->first == nDBCameraID );
	return itorItem->second->GetCamera();
}
//
// 处理网络摄像头配置数据包...
GM_Error CMidView::doCameraUDPData(GM_MapData & inNetData, CAMERA_TYPE inType, int & outDBCameraID)
{
	// 查看是否存在设备序列号字段...
	GM_Error theErr = GM_No_Xml_Node;
	GM_MapData::iterator itorData;
	switch( inType ) {
	case kCameraNO: itorData = inNetData.find("device_sn"); break;
	case kCameraHK: itorData = inNetData.find("DeviceSN"); break;
	case kCameraDH: itorData = inNetData.end(); break;
	default:		itorData = inNetData.end(); break;
	}
	// 字段无效，直接返回...
	if( itorData == inNetData.end() ) {
		MsgLogGM(theErr);
		return theErr;
	}
	// 获取监控通道唯一标识信息 => 设备序号...
	string & strDeviceSN = itorData->second;
	CCamera * lpCamera = this->FindDBCameraBySN(strDeviceSN);
	// 找到对应的摄像头，直接返回...
	if( lpCamera != NULL )
		return GM_NoErr;
	// 如果摄像头对象为空，判断是否超过了设备支持上限...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	if( m_MapVideo.size() >= theConfig.GetMaxCamera() )
		return GM_NoErr;
	// 如果摄像头对象为空，建立一个新的窗口对象...
	return this->AddNewCamera(inNetData, inType, outDBCameraID);
}
//
// 删除指定摄像头...
void CMidView::doDelDVR(int nCameraID)
{
	GM_MapVideo::iterator itorItem = m_MapVideo.find(nCameraID);
	if( itorItem == m_MapVideo.end() )
		return;
	// 删除对应的视频窗口对象...
	CVideoWnd * lpVideo = itorItem->second;
	delete lpVideo; lpVideo = NULL;
	m_MapVideo.erase(itorItem);
	// 对子窗口进行布局重绘操作...
	CRect rcRect;
	this->GetClientRect(rcRect);
	if( rcRect.Width() > 0 && rcRect.Height() > 0 ) {
		this->LayoutVideoWnd(rcRect.Width(), rcRect.Height());
		this->Invalidate(true);
	}
	// 窗口列表不为空，找第一个焦点...
	if( m_MapVideo.size() <= 0 ) {
		::PostMessage(m_lpParentDlg->m_hWnd, WM_FOCUS_VIDEO, -1, NULL);
		return;
	}
	// 找到当前第一个窗口进行焦点事件操作...
	itorItem = m_MapVideo.begin();
	lpVideo = itorItem->second;
	lpVideo->doFocusAction();
}
//
// 得到下一个窗口编号...
int CMidView::GetNextAutoID(int nCurDBCameraID)
{
	GM_MapVideo::iterator itorFirst = m_MapVideo.begin();
	// 如果没有节点，直接返回开始编号...
	if( itorFirst == m_MapVideo.end() ) {
		return DEF_CAMERA_START_ID;
	}
	ASSERT( itorFirst != m_MapVideo.end() );
	// 如果输入的节点是无效的，返回第一个有效节点...
	GM_MapVideo::iterator itorItem = m_MapVideo.find(nCurDBCameraID);
	if( itorItem == m_MapVideo.end() ) {
		return itorFirst->first;
	}
	// 如果下一个节点是无效的，回滚到第一个节点...
	if((++itorItem) == m_MapVideo.end() ) {
		return itorFirst->first;
	}
	// 下一个节点有效，返回对应的编号...
	ASSERT( itorItem != m_MapVideo.end() );
	return itorItem->first;
}

string CMidView::GetDefaultCameraName()
{
	string strDefName;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	switch( theConfig.GetWebType() )
	{
	case kCloudRecorder: strDefName = "一班"; break;
	case kCloudMonitor:	 strDefName = "监控通道"; break;
	default:			 strDefName = "直播通道"; break;
	}
	return strDefName;
}
//
// 添加一个视频窗口...
GM_Error CMidView::AddNewCamera(GM_MapData & inNetData, CAMERA_TYPE inType, int & outDBCameraID)
{
	// 注意：通道编号由数据库产生，而不是本地产生...
	// 先从已有集合中查找最大编号的窗口...
	/*int nCameraID = DEF_CAMERA_START_ID;
	GM_MapVideo::reverse_iterator itorEnd = m_MapVideo.rbegin();
	if( itorEnd != m_MapVideo.rend() ) {
		nCameraID = itorEnd->first + 1;
	}*/
	// 定义需要的变量...
	GM_MapData theMapLoc;
	CString strType, strStreamProp;
	// 构造摄像头类型信息...
	strType.Format("%d", inType);
	theMapLoc["camera_type"] = strType;
	// 获取默认的通道名称，通道编号由数据库产生...
	if( inNetData.find("camera_name") != inNetData.end() ) {
		theMapLoc["camera_name"] = inNetData["camera_name"];
	} else {
		theMapLoc["camera_name"] = this->GetDefaultCameraName();
	}
	// 如果是摄像头类型，提取有用的字段组合成需要的配置信息...
	if( inType == kCameraNO ) {
		// 如果是流转发数据，提取需要的字段组起来...
		theMapLoc["stream_prop"] = inNetData["stream_prop"];
		theMapLoc["stream_url"] = inNetData["stream_url"];
		theMapLoc["stream_mp4"] = inNetData["stream_mp4"];
		theMapLoc["stream_loop"] = inNetData["stream_loop"];
		theMapLoc["stream_auto"] = inNetData["stream_auto"];
		theMapLoc["device_sn"] = inNetData["device_sn"];
	} else {
		// 如果是摄像头类型，提取有用的字段组合成需要的配置信息...
		ASSERT( inType == kCameraHK || inType == kCameraDH );
		strStreamProp.Format("%d", kStreamDevice);
		theMapLoc["stream_prop"] = strStreamProp;
		theMapLoc["device_type"] = inNetData["DeviceType"];
		theMapLoc["device_desc"] = inNetData["DeviceDescription"];
		theMapLoc["device_sn"] = inNetData["DeviceSN"];
		theMapLoc["device_cmd_port"] = inNetData["CommandPort"];
		theMapLoc["device_http_port"] = inNetData["HttpPort"];
		theMapLoc["device_mac"] = inNetData["MAC"];
		theMapLoc["device_ip"] = inNetData["IPv4Address"];
		theMapLoc["device_boot"] = inNetData["BootTime"];
		// 去掉硬件的通道汇报，换成了device_twice模式...
		//theMapLoc["device_channel"] = inNetData["DigitalChannelNum"];
		// 设置默认的用户名和密码(base64)...
		TCHAR szEncode[MAX_PATH] = {0};
		int nEncLen = Base64encode(szEncode, DEF_LOGIN_PASS_HK, strlen(DEF_LOGIN_PASS_HK));
		theMapLoc["device_user"] = DEF_LOGIN_USER_HK;
		theMapLoc["device_pass"] = szEncode;
		// 默认开启OSD、关闭镜像、关闭双流模式...
		theMapLoc["device_osd"] = "1";
		theMapLoc["device_mirror"] = "0";
		theMapLoc["device_twice"] = "0";
	}
	// 向网站注册摄像头，注册成功才能创建...
	// 注意：注册之后，会将camera_id更新...
	ASSERT( m_lpParentDlg != NULL );
	GM_Error  theErr = GM_NotImplement;
	if( !m_lpParentDlg->doWebRegCamera(theMapLoc) ) {
		MsgLogGM(theErr);
		return theErr;
	}
	// 直接创建一个新的视频窗口...
	CCamera * lpCamera = this->BuildWebCamera(theMapLoc);
	if( lpCamera == NULL ) {
		MsgLogGM(theErr);
		return theErr;
	}
	// 注意：配置信息已经更新到内存当中了...
	// 获取通道编号，直接输出返回...
	outDBCameraID = lpCamera->GetDBCameraID();
	return GM_NoErr;
	// 放弃了以前的更新网络配置的方式...
	// 如果是流转发模式，直接返回...
	/*if( inType == kCameraNO )
		return GM_NoErr;
	// 如果是硬件设备，更新监控通道的网络配置...
	ASSERT( inType == kCameraHK || inType == kCameraDH );
	return lpCamera->ForDeviceUDPData(inNetData);*/
}
//
// 根据网络配置数据创建监控通道 => 本地不再存放通道配置...
CCamera * CMidView::BuildWebCamera(GM_MapData & inWebData)
{
	GM_MapData::iterator itorID, itorName;
	itorID = inWebData.find("camera_id");
	itorName = inWebData.find("camera_name");
	if((itorID == inWebData.end()) || (itorName == inWebData.end())) {
		MsgLogGM(GM_No_Xml_Node);
		return NULL;
	}
	// 计算视频窗口编号...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	int nDBCameraID = atoi(itorID->second.c_str());
	int nVideoWndID = ID_VIDEO_WND_BEGIN + nDBCameraID;
	CString strTitle = theConfig.GetDBCameraTitle(nDBCameraID);
	// 创建一个新的视频窗口...
	ASSERT( m_BitArray != NULL && nDBCameraID > 0 );
	CVideoWnd * lpVideo = new CVideoWnd(m_BitArray, nVideoWndID);
	lpVideo->SetTitleFont(&m_VideoFont);
	lpVideo->SetWebTitleText(strTitle);
	// 创建窗口并保存到列表当中...
	lpVideo->Create(WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
	ASSERT( lpVideo->m_hWnd != NULL );
	m_MapVideo[nDBCameraID] = lpVideo;
	// 对子窗口进行布局重绘操作...
	CRect rcRect;
	this->GetClientRect(rcRect);
	if( rcRect.Width() > 0 && rcRect.Height() > 0 ) {
		this->LayoutVideoWnd(rcRect.Width(), rcRect.Height());
		this->Invalidate(true);
	}
	// 根据配置的数据，创建摄像头...
	lpVideo->BuildCamera(inWebData);
	// 2017.08.06 - by jackey => 对象创建完毕，再设置窗口标题...
	lpVideo->SetDispTitleText(strTitle);
	// 需要通知父窗口有新的通道被创建了...
	ASSERT( m_lpParentDlg != NULL );
	m_lpParentDlg->OnCreateCamera(nDBCameraID, strTitle);
	// 返回该窗口对应的摄像头...
	return lpVideo->GetCamera();
}
//
// 网站授权验证结果处理...
void CMidView::OnWebAuthResult(int nType, BOOL bAuthOK)
{
	// 根据不同状态，显示不同的信息...
	if( nType == kAuthRegister ) {
		m_strNotice = bAuthOK ? "正在验证网站授权，请稍等..." : "节点网站注册失败，请检查网站链接...";
	} else if( nType == kAuthExpired ) {
		m_strNotice = bAuthOK ? "正在搜索网络摄像机..." : "采集端授权已过期，请联系供应商重新授权！";
	}
	// 重新更新窗口背景...
	this->Invalidate(true);
}
//
// 处理背景更新事件...
BOOL CMidView::OnEraseBkgnd(CDC* pDC) 
{
	// 获取矩形区域...
	CRect rcRect;
	this->GetClientRect(rcRect);
	// 填充背景色...
	pDC->FillSolidRect(rcRect, RGB(32,32,32));
	// 绘制边框...
	//rcRect.DeflateRect(1, 1);
	//pDC->Draw3dRect(rcRect, ::GetSysColor(COLOR_BTNHIGHLIGHT), ::GetSysColor(COLOR_BTNSHADOW) );
	// 绘制窗口背景线条...
	this->DrawBackLine(pDC);
	// 打印警告信息...
	this->DrawNotice(pDC);
	return TRUE;
}
//
// 打印警告信息...
void CMidView::DrawNotice(CDC * pDC)
{
	CRect   rcRect;
	CFont   fontLogo;
	CFont * pOldFont = NULL;
	// 只要有子窗口存在就不绘制...
	if( m_MapVideo.size() > 0 )
		return;
	ASSERT( m_MapVideo.size() <= 0 );
	// 创建字体，获取矩形显示区域...
	fontLogo.CreateFont(40, 0, 0, 0, FW_LIGHT, false, false,0,0,0,0,0,0, "宋体");
	this->GetClientRect(&rcRect);
	// 绘制警告文字...
	pOldFont = pDC->SelectObject(&fontLogo);
	pDC->SetTextColor( RGB(255, 255, 0) );
	pDC->DrawText(m_strNotice, rcRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
	// 销毁字体...
	pDC->SelectObject(pOldFont);
	fontLogo.DeleteObject();
	fontLogo.Detach();
}
//
// 绘制背景蓝色线框，用计算出来的位置...
void CMidView::DrawBackLine(CDC * pDC)
{
	// 窗口列表必须大于0...
	GM_MapVideo::iterator itorItem = m_MapVideo.begin();
	if( itorItem == m_MapVideo.end() )
		return;
	// 获取客户区矩形...
	CRect rcRect;
	this->GetClientRect(rcRect);
	// 首先，计算需要的行、列数字...
	float fData = sqrt(m_MapVideo.size() * 1.0f);
	int nCol  = (int)fData + (((fData - (int)fData) == 0.0f) ? 0 : 1);
	int nRow  = nCol;
	int	nWidth	= (rcRect.Width() - 1) / nCol - 1;
	int	nHigh	= (rcRect.Height() - 1) / nRow - 1;
	// 创建画刷对象...
	HBRUSH hBrush = ::CreateSolidBrush(RGB(89, 147, 200));
	// 开始进行窗口排列操作...
	for(int i = 0; i < nRow; ++i) {
		for(int j = 0; j < nCol; ++j) {
			// 窗口有效，计算并保存位置，这里是Screen坐标...
			CRect rcWinPos;
			int	nLeft = j * (nWidth + 1) + 1;
			int	nTop  = i * (nHigh  + 1) + 1;
			rcWinPos.SetRect(nLeft, nTop, nLeft + nWidth, nTop + nHigh);
			::FrameRect(pDC->m_hDC, rcWinPos, hBrush);
			this->ClientToScreen(rcWinPos);
			// 如果窗口已经没有了...
			if( itorItem == m_MapVideo.end() )
				continue;
			ASSERT( itorItem != m_MapVideo.end() );
			// 窗口指针无效，继续下一个...
			CVideoWnd * lpVideo = itorItem->second;
			if( lpVideo != NULL ) {
				lpVideo->m_rcWinPos = rcWinPos;
			}
			// 移动到下一个窗口...
			++itorItem;
		}
	}
	// 删除画刷对象...
	::DeleteObject(hBrush);
}
//
// 处理 WM_SIZE 消息...
void CMidView::OnSize(UINT nType, int cx, int cy) 
{
	CWnd::OnSize(nType, cx, cy);
	if( cx <= 0 || cy <= 0 )
		return;
	this->LayoutVideoWnd(cx, cy);
}
//
// 排列视频窗口...
void CMidView::LayoutVideoWnd(int cx, int cy)
{
	// 窗口列表必须大于0...
	GM_MapVideo::iterator itorItem = m_MapVideo.begin();
	if( itorItem == m_MapVideo.end() )
		return;
	// 首先，计算需要的行、列数字...
	float fData = sqrt(m_MapVideo.size() * 1.0f);
	int nCol  = (int)fData + (((fData - (int)fData) == 0.0f) ? 0 : 1);
	int nRow  = nCol;
	int	nWidth	= (cx - 1) / nCol - 1;
	int	nHigh	= (cy - 1) / nRow - 1;
	// 开始进行窗口排列操作...
	for(int i = 0; i < nRow; ++i) {
		for(int j = 0; j < nCol; ++j) {
			if( itorItem == m_MapVideo.end() )
				return;
			// 视频窗口无效，继续下一个窗口...
			CVideoWnd * lpVideo = itorItem->second;
			if( lpVideo == NULL ) {
				++itorItem;
				continue;
			}
			int	nLeft = j * (nWidth + 1) + 1;
			int	nTop  = i * (nHigh  + 1) + 1;
			// Fix状态，才需要移动窗口位置...
			if( lpVideo->GetWndState() == CVideoWnd::kFixState ) {
				lpVideo->MoveWindow(nLeft, nTop, nWidth, nHigh);
			}
			// 然后，检测下一个窗口...
			++itorItem;
		}
	}
}
//
// 将所有窗口都固定，不要漂浮...
void CMidView::FixVideoWnd()
{
	// 遍历视频窗口，根据窗口状态处理...
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
// 释放焦点窗口...
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
// 响应左侧树焦点事件...
void CMidView::doLeftFocus(int nCameraID)
{
	// 首先，查找对应的视频窗口对象...
	GM_MapVideo::iterator itorItem;
	itorItem = m_MapVideo.find(nCameraID);
	if( itorItem == m_MapVideo.end() )
		return;
	// 窗口对象无效，直接返回...
	CVideoWnd * lpVideo = itorItem->second;
	if( lpVideo == NULL )
		return;
	// 释放所有的焦点窗口，设置当前窗口为焦点...
	this->ReleaseFocus();
	lpVideo->m_bFocus = true;
	lpVideo->DrawFocus(CVideoWnd::kFocusColor);
}
//
// 移动窗口，会触发 OnSize 事件...
void CMidView::doMoveWindow(CRect & rcArea)
{
	m_rcOrigin = rcArea;
	this->MoveWindow(&m_rcOrigin);
}
//
// 全屏显示窗口...
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

BOOL CMidView::doWebStatCamera(int nDBCamera, int nStatus, int nErrCode, LPCTSTR lpszErrMsg)
{
	if( m_lpParentDlg == NULL ) 
		return false;
	return m_lpParentDlg->doWebStatCamera(nDBCamera, nStatus, nErrCode, lpszErrMsg);
}