
#include "stdafx.h"
#include "BitItem.h"
#include "VideoWnd.h"
#include "MidView.h"
#include "..\Camera.h"
#include "..\Resource.h"
#include "..\XmlConfig.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CAutoVideo::CAutoVideo(CVideoWnd * inWnd)
  : m_lpWnd(inWnd)
{
	ASSERT( m_lpWnd != NULL );
	if( m_lpWnd->GetWndState() != CVideoWnd::kFlyState )
		return;
	::SetWindowPos(m_lpWnd->m_hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
}

CAutoVideo::~CAutoVideo()
{
	ASSERT( m_lpWnd != NULL );
	if( m_lpWnd->GetWndState() != CVideoWnd::kFlyState )
		return;
	::SetWindowPos(m_lpWnd->m_hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
}

CVideoWnd::CVideoWnd(CBitItem ** lppBit, UINT inVideoWndID)
  : m_lpTitleFont(NULL),
    m_bFocus(false),
	m_bDrawRec(true),
	m_bDrawLive(true),
	m_bDraging(false),
	m_lpRenderWnd(NULL),
	m_ptMouse(0, 0),
	m_lpCamera(NULL),
	m_lpParent(NULL),
	m_nWndState(kFixState),
	m_nVideoWndID(inVideoWndID),
	m_nWndIndex(inVideoWndID - ID_VIDEO_WND_BEGIN)
{
	ASSERT( lppBit != NULL );
	ASSERT( m_nVideoWndID > 0 && m_nWndIndex > 0 );
	
	memset(&m_wPrevious, 0, sizeof(m_wPrevious));
	memcpy(m_BitArray, lppBit, sizeof(void *) * kBitNum);

	//m_hDesk	 = (HBRUSH)GetSysColorBrush(COLOR_DESKTOP);
	//ASSERT( m_hDesk != NULL );
}

CVideoWnd::~CVideoWnd()
{
	this->ClearResource();
}

BEGIN_MESSAGE_MAP(CVideoWnd, CWnd)
	ON_WM_SIZE()
	ON_WM_TIMER()
	ON_WM_CREATE()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONDOWN()
	//ON_WM_NCCALCSIZE()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_GETMINMAXINFO()
	ON_MESSAGE(WM_ERR_TASK_MSG, &CVideoWnd::OnMsgTaskErr)
	ON_MESSAGE(WM_ERR_PUSH_MSG, &CVideoWnd::OnMsgPushErr)
	ON_MESSAGE(WM_REC_SLICE_MSG, &CVideoWnd::OnMsgRecSlice)
	ON_MESSAGE(WM_STOP_STREAM_MSG, &CVideoWnd::OnMsgStopStream)
	ON_COMMAND_RANGE(ID_RENDER_WND_BEGIN, ID_RENDER_WND_BEGIN + kBitNum, &CVideoWnd::OnClickItemWnd)
END_MESSAGE_MAP()
//
// �Ƿ�������ͷ�豸ģʽ...
BOOL CVideoWnd::IsCameraDevice()
{
	if( m_lpCamera == NULL )
		return false;
	return m_lpCamera->IsCameraDevice();
}
//
// �Ƿ�����ת��ģʽ���ڲ�����...
BOOL CVideoWnd::IsStreamPlaying()
{
	if( m_lpCamera == NULL || m_lpCamera->IsCameraDevice() )
		return false;
	ASSERT( !m_lpCamera->IsCameraDevice() );
	return m_lpCamera->IsPlaying();
}
//
// �Ƿ����������ڷ�����...
BOOL CVideoWnd::IsStreamPublish()
{
	if( m_lpCamera == NULL || m_lpCamera->IsCameraDevice() )
		return false;
	ASSERT( !m_lpCamera->IsCameraDevice() );
	return m_lpCamera->IsPublishing();
}
//
// �Ƿ����������Ѿ����ӳɹ�...
BOOL CVideoWnd::IsStreamLogin()
{
	if( m_lpCamera == NULL || m_lpCamera->IsCameraDevice() )
		return false;
	ASSERT( !m_lpCamera->IsCameraDevice() );
	return m_lpCamera->IsLogin();
}

int CVideoWnd::GetRecvPullKbps()
{
	if( m_lpCamera == NULL )
		return 0;
	ASSERT( m_lpCamera != NULL );
	// ��ȡ�������ݵ�������Ϣ...
	int nRecvKbps = m_lpCamera->GetRecvPullKbps();
	// ������ʱ��ֱ�ӵ��ýӿ�ֹͣ��������ͨ���˳�...
	if( nRecvKbps < 0 ) {
		m_lpCamera->doStreamLogout();
		return 0;
	}
	// �������������Ч��ֱ�ӷ���...
	ASSERT( nRecvKbps >= 0 );
	return nRecvKbps;
}

int	CVideoWnd::GetSendPushKbps()
{
	return ((m_lpCamera == NULL) ? 0 : m_lpCamera->GetSendPushKbps());
}

LPCTSTR	CVideoWnd::GetStreamPushUrl()
{
	return ((m_lpCamera == NULL) ? NULL : m_lpCamera->GetStreamPushUrl());
}

void CVideoWnd::GetStreamPullUrl(CString & outPullUrl)
{
	if( m_lpCamera == NULL || m_lpCamera->IsCameraDevice() )
		return;
	GM_MapData theMapLoc;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetCamera(this->GetCameraID(), theMapLoc);
	string & strStreamMP4 = theMapLoc["StreamMP4"];
	string & strStreamUrl = theMapLoc["StreamUrl"];
	if( m_lpCamera->GetStreamProp() == kStreamMP4File ) {
		outPullUrl = strStreamMP4.c_str();
	} else {
		outPullUrl = strStreamUrl.c_str();
	}
}
//
// ¼����Ƭ֪ͨ...
LRESULT	CVideoWnd::OnMsgRecSlice(WPARAM wParam, LPARAM lParam)
{
	/*if( m_lpCamera != NULL ) {
		m_lpCamera->doRecSlice();
	}*/
	return S_OK;
}
//
// ��Ӧ����¼�������Ϣ...
LRESULT CVideoWnd::OnMsgTaskErr(WPARAM wParam, LPARAM lParam)
{
	// ֱ�ӵ���ֹͣ¼��ӿ� => Ŀǰ��ʱ������ => ���ܻ�������������������...
	//if( m_lpCamera != NULL ) {
	//	m_lpCamera->doRecStopCourse(lParam);
	//}
	return S_OK;
}
//
// ��Ӧ�����ϴ�������Ϣ...
LRESULT CVideoWnd::OnMsgPushErr(WPARAM wParam, LPARAM lParam)
{
	// ֱ�ӵ���ֹͣ�ϴ��ӿ�...
	if( m_lpCamera != NULL ) {
		m_lpCamera->doStopLivePush();
	}
	return S_OK;
}
//
// ��Ӧֹͣ�����ϴ���Ϣ...
LRESULT CVideoWnd::OnMsgStopStream(WPARAM wParam, LPARAM lParam)
{
	// ֱ�ӵ���ֹͣ�ϴ��ӿ�...
	if( m_lpCamera != NULL ) {
		m_lpCamera->doStreamStopLivePush();
	}
	return S_OK;
}

void CVideoWnd::OnTimer(UINT_PTR nIDEvent) 
{
	// �����ͼʱ���¼�...
	/*if( nIDEvent == kSnapTimerID ) {
		if( m_lpCamera != NULL ) {
			//m_lpCamera->doSnapJPG();
		}
	}*/
	// ����¼��״̬��˸�¼�...
	if( nIDEvent == kStatusTimerID ) {
		this->doDrawRecStatus();
		this->doDrawLiveStatus();
	}
	CWnd::OnTimer(nIDEvent);
}

void CVideoWnd::doDrawLiveStatus()
{
	if( m_lpCamera == NULL )
		return;
	if( !m_lpCamera->IsPublishing() ) {
		this->doEarseLiveStatus();
		return;
	}
	// ���û���״̬��־...
	m_bDrawLive = false;

	CRect   rcRect;
	CFont   fontLogo;
	CFont * pOldFont = NULL;
	CString strText = "LIVE";
	// �������壬��ȡ������ʾ����...
	fontLogo.CreateFont(16, 0, 0, 0, FW_BOLD, false, false,0,0,0,0,0,0, "����");
	this->GetClientRect(&rcRect);
	rcRect.DeflateRect(1, 1);
	rcRect.bottom = rcRect.top + 20;
	rcRect.right -= ((m_nWndState == kFixState) ? 40 : 90);
	rcRect.left   = rcRect.right - 50;
	// ���ƾ�������...
	CDC * pDC = this->GetDC();
	pOldFont = pDC->SelectObject(&fontLogo);
	pDC->SetTextColor( RGB(255, 128, 0) );
	pDC->SetBkMode(TRANSPARENT);
	pDC->DrawText( strText, rcRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
	// ��������...
	pDC->SelectObject(pOldFont);
	fontLogo.DeleteObject();
	fontLogo.Detach();
	this->ReleaseDC(pDC);
}

void CVideoWnd::doEarseLiveStatus()
{
	// ����LIVE״̬��־...
	if( m_bDrawLive )
		return;
	m_bDrawLive = true;
	// ������ƾ�����...
	CRect	rcRect;
	CDC   * pDC = this->GetDC();
	this->GetClientRect(rcRect);
	rcRect.DeflateRect(1, 1);
	rcRect.bottom = rcRect.top + 20;
	// ���¼���־��Ϣ...
	rcRect.right -= ((m_nWndState == kFixState) ? 40 : 90);
	rcRect.left   = rcRect.right - 50;
	pDC->FillSolidRect(rcRect, kNormalColor);
	this->ReleaseDC(pDC);
}

void CVideoWnd::doEarseRecStatus()
{
	// ����¼��״̬��־...
	if( m_bDrawRec )
		return;
	m_bDrawRec = true;
	ASSERT( m_lpCamera != NULL );
	ASSERT( !m_lpCamera->IsRecording() );
	// ������ƾ�����...
	CRect	rcRect;
	CDC   * pDC = this->GetDC();
	this->GetClientRect(rcRect);
	rcRect.DeflateRect(1, 1);
	rcRect.bottom = rcRect.top + 20;
	// ���¼���־��Ϣ...
	rcRect.right -= ((m_nWndState == kFixState) ? 20 : 70);
	rcRect.left   = rcRect.right - 20;
	pDC->FillSolidRect(rcRect, kNormalColor);
	this->ReleaseDC(pDC);
}

void CVideoWnd::doDrawRecStatus()
{
	// DVR������Ч...
	if( m_lpCamera == NULL )
		return;
	ASSERT( m_lpCamera != NULL );
	// ���DVR����¼���У���Ҫ����һ��¼���־...
	if( !m_lpCamera->IsRecording() ) {
		this->doEarseRecStatus();
		return;
	}
	// ������ƾ�����...
	CRect	rcRect;
	CDC   * pDC = this->GetDC();
	this->GetClientRect(rcRect);
	rcRect.DeflateRect(1, 1);
	rcRect.bottom = rcRect.top + 20;
	// ����¼��״̬��(��˸)...
	rcRect.right -= ((m_nWndState == kFixState) ? 20 : 70);
	rcRect.left   = rcRect.right - 20;
	if( m_bDrawRec ) {
		CBrush   theBrush(RGB(255, 0, 0));
		CPen     thePen(PS_SOLID, 1, RGB(255, 0, 0));
		CPen   * lpOldPen = pDC->SelectObject(&thePen);
		CBrush * lpOldBrush = pDC->SelectObject(&theBrush);
		rcRect.DeflateRect(2, 2);
		pDC->Ellipse(rcRect);
		pDC->SelectObject(lpOldPen);
		pDC->SelectObject(lpOldBrush);
		m_bDrawRec = false;
	} else {
		m_bDrawRec = true;
		pDC->FillSolidRect(rcRect, kNormalColor);
	}
	this->ReleaseDC(pDC);
}

BOOL CVideoWnd::BuildCamera(GM_MapData & inMapLoc)
{
	// ����һ����ͼʱ��...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	int nSnapStep = theConfig.GetSnapStep();

	//this->SetTimer(kSnapTimerID, nSnapStep * 1000, NULL);
	this->SetTimer(kStatusTimerID, 1 * 1000, NULL);

	// ����һ��DVRͨ��...
	ASSERT( m_lpCamera == NULL );
	m_lpCamera = new CCamera(this);
	return m_lpCamera->InitCamera(inMapLoc);
}

BOOL CVideoWnd::Create(UINT wStyle, const CRect & rect, CWnd * pParentWnd)
{
	//
	// 1.0 Register the window class object...
	LPCTSTR	lpWndName = NULL;
	lpWndName = AfxRegisterWndClass(CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW,
									AfxGetApp()->LoadStandardCursor(IDC_ARROW),
									NULL, NULL);
	if( lpWndName == NULL )
		return FALSE;
	//
	// 2.0 Create the window directly...
	wStyle |= WS_CLIPSIBLINGS;
	return CWnd::Create(lpWndName, NULL, wStyle, rect, pParentWnd, m_nVideoWndID);
}

int CVideoWnd::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	m_lpParent = this->GetParent();
	ASSERT( m_lpParent != NULL );

	this->BuildItemWnd();
	this->BuildRenderWnd(m_nWndIndex);

	return 0;
}

BOOL CVideoWnd::OnEraseBkgnd(CDC* pDC) 
{
	//if( m_nWndState == kFixState ) {
	//	return TRUE;
	//}

	// ��ȡ���ڵľ�����...
	CRect		rcRect;
	COLORREF	clrValue = (m_bFocus ? kFocusColor : kNormalColor);
	this->GetClientRect(rcRect);
	// ׼����ˢ�����Ʊ߿�...
	pDC->Draw3dRect(rcRect, clrValue, clrValue);
	rcRect.DeflateRect(1, 1);
	pDC->Draw3dRect(rcRect, clrValue, clrValue);
	// ���Ʊ���������ɫ...
	rcRect.bottom = rcRect.top + 20;
	pDC->FillSolidRect(rcRect, kNormalColor);

	// �ڱ��������Ʊ�������...
	CFont  * lpOld = NULL;
	lpOld = pDC->SelectObject(m_lpTitleFont);
	pDC->SetTextColor(RGB(255, 255, 255));
	pDC->SetBkMode(TRANSPARENT);
	pDC->TextOut(10, 4, m_strTitle);
	pDC->SelectObject(lpOld);

	// �������� ��Ⱦ���� ռ�ݣ����û���...

	return TRUE;
}

void CVideoWnd::OnSize(UINT nType, int cx, int cy) 
{
	CWnd::OnSize(nType, cx, cy);

	if( cx <= 0 || cy <= 0)
		return;
	POINT	point	= {0, 0};
	int		nTitle	= 20;
	int		nBit	= 18;
	int		nStep	= (nTitle - nBit) / 2;
	int		nPos	= cx - kBitNum * (nBit + 1);
	ASSERT( cx > 0 && nStep > 0 );
	for(int i = 0; i < kBitNum; i++)
	{
		point.x = nPos + i * (nBit + 1);
		point.y = nStep;
		if( m_ItemWnd[i].m_hWnd == NULL )
			continue;
		m_ItemWnd[i].ShowWindow((m_nWndState == kFixState) ? SW_HIDE : SW_SHOW);
		m_ItemWnd[i].SetWindowPos(NULL, point.x, point.y, 0, 0, SWP_NOSIZE);
	}
	// ������Ҫ�����߿�...
	int nReal = nTitle;
	//int nReal = (m_nWndState == kFixState) ? 0 : nTitle;
	(m_lpRenderWnd != NULL) ? m_lpRenderWnd->MoveWindow(1, nReal + 1, cx - 2, cy - nReal - 2) : NULL;
}

void CVideoWnd::BuildRenderWnd(int inRenderWndID)
{
	ASSERT( m_lpRenderWnd == NULL );

	m_lpRenderWnd = new CRenderWnd();
	ASSERT( m_lpRenderWnd != NULL );
	m_lpRenderWnd->Create(WS_CHILD | WS_VISIBLE,
						  CRect(0, 0, 0, 0), 
						  this, inRenderWndID);
	ASSERT( m_lpRenderWnd->m_hWnd != NULL );
}

void CVideoWnd::ClearResource()
{
	if( m_lpRenderWnd != NULL ) {
		delete m_lpRenderWnd;
		m_lpRenderWnd = NULL;
	}
	if( m_lpCamera != NULL ) {
		delete m_lpCamera;
		m_lpCamera = NULL;
	}
	if( this->m_hWnd != NULL ) {
		this->DestroyWindow();
	}
}

void CVideoWnd::OnMouseMove(UINT nFlags, CPoint point) 
{
	if( m_nWndState == kFixState )
		return;
	this->OnMouseDragEvent();
	CWnd::OnMouseMove(nFlags, point);
}

void CVideoWnd::OnMouseDragEvent()
{
	if( !m_bDraging )
		return;
	CRect			rcRect;
	CPoint			ptNewPos;
	WINDOWPLACEMENT wPlace = {0};
	if( !this->GetWindowPlacement(&wPlace) )
		return;
	rcRect = wPlace.rcNormalPosition;
	::GetCursorPos(&ptNewPos);
	rcRect.OffsetRect(ptNewPos - m_ptMouse);
	this->MoveWindow(rcRect);
	m_ptMouse  = ptNewPos;
}

void CVideoWnd::EnableDrag()
{
	if( m_nWndState != kFlyState )
		return;
	this->SetCapture();
	m_bDraging = TRUE;
	::GetCursorPos(&m_ptMouse);
}

void CVideoWnd::DisableDrag()
{
	m_bDraging = FALSE;
	::ReleaseCapture();
}

void CVideoWnd::ReleaseFocus()
{
	// ������ǽ��㣬ֱ�ӷ���...
	if( !m_bFocus )
		return;
	ASSERT( m_bFocus );
	// �ͷŽ��㣬�ػ�߿�...
	this->DrawFocus(kNormalColor);
	m_bFocus = false;
}

void CVideoWnd::DrawFocus(COLORREF inColor)
{
	// ���»��ƽ�������...
	CDC    *    pDC = this->GetDC();
	CRect		rcRect;
	COLORREF	clrValue = inColor;
	this->GetClientRect(rcRect);
	// ���Ʊ߿�...
	pDC->Draw3dRect(rcRect, clrValue, clrValue);
	this->ReleaseDC(pDC);
}

void CVideoWnd::OnLButtonDown(UINT nFlags, CPoint point) 
{
	if( m_nWndState == kFixState ) {
		// �������¼�����...
		this->doFocusAction();
	}
	this->EnableDrag();
	if( m_nWndState == kFixState )
		return;
	this->BringWindowToTop();
	CWnd::OnLButtonDown(nFlags, point);
}

void CVideoWnd::OnLButtonUp(UINT nFlags, CPoint point) 
{
	this->DisableDrag();
	if( m_nWndState == kFixState )
		return;
	CWnd::OnLButtonUp(nFlags, point);
}

void CVideoWnd::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
	switch( m_nWndState )
	{
	case kFixState:	  this->OnFixToFlyState();	break;
	case kFlyState:	  this->OnFullScreen();		break;
	case kFullState:  this->OnRestorScreen();	break;
	default:		  ASSERT( FALSE );			break;
	}
	CWnd::OnLButtonDblClk(nFlags, point);
}
//
// Process the click event...
void CVideoWnd::OnClickItemWnd(UINT nItem)
{
	switch( nItem - ID_RENDER_WND_BEGIN )
	{
	case kZoomOut:	this->OnZoomOutEvent();		break;
	case kZoomIn:	this->OnZoomInEvent();		break;
	case kClose:	this->OnZoomCloseEvent();	break;
	default:		ASSERT( FALSE );			break;
	}
}

void CVideoWnd::OnZoomOutEvent()
{
	switch( m_nWndState )
	{
	case kFixState:		this->OnFixToFlyState();	break;
	case kFlyState:		this->OnFlyToZoomOut();		break;
	case kFullState:								break;
	default:			ASSERT( FALSE );			break;
	}
}

void CVideoWnd::OnFixToFlyState()
{
	this->ShowWindow(SW_HIDE);

	ASSERT( m_lpParent != NULL );
	m_nWndState = kFlyState;
	m_bFocus = false;
	
	this->ModifyStyle(0, WS_THICKFRAME);
	this->ModifyStyleEx(0, WS_EX_TOOLWINDOW);
	this->SetParent(NULL);

	this->OnMoveToCenter(KHSS_LEFT_VIEW_WIDTH, KHSS_LEFT_VIEW_WIDTH / 4 * 3);

	this->ShowWindow(SW_SHOW);
}

void CVideoWnd::OnMoveToCenter(int nWidth, int nHeight)
{
	CWnd * lpDesk = GetDesktopWindow();
	if( nWidth <= 0 || nHeight <= 0 || lpDesk == NULL )
		return;
	int		xLeft = 0;
	int		xTop  = 0;
	CRect	rcArea;
	lpDesk->GetClientRect(rcArea);
	xLeft = (rcArea.Width() - nWidth) / 2;
	xTop  = (rcArea.Height() - nHeight) / 2;
	ASSERT( xLeft >= 0 && xTop >= 0 );

	this->SetWindowPos(&wndTopMost, xLeft, xTop, nWidth, nHeight, SWP_NOACTIVATE);
}

void CVideoWnd::OnFlyToFixState()
{
//	CWnd * lpFrame = AfxGetMainWnd();
//	if( !lpFrame->IsWindowVisible() || lpFrame->IsIconic() )
//		return;
	this->ShowWindow(SW_HIDE);

	CRect	rcRect = m_rcWinPos;
	ASSERT( m_lpParent != NULL );
	m_nWndState = kFixState;
	this->SetParent(m_lpParent);

	ModifyStyle(WS_THICKFRAME, 0, SWP_FRAMECHANGED);
	ModifyStyleEx(WS_EX_TOOLWINDOW, 0, SWP_FRAMECHANGED);
	
	m_lpParent->ScreenToClient(rcRect);
	this->MoveWindow(rcRect);
	this->ShowWindow(SW_SHOW);

	// �������¼�����...
	this->doFocusAction();
}
//
// �ϲ��������¼�����...
void CVideoWnd::doFocusAction()
{
	// ���õ�ǰ����Ϊ���㴰��...
	this->SetFocus();
	// ֪ͨ�������ͷ�ԭ�������д��ڵĽ���״̬...
	ASSERT( m_lpParent != NULL );
	((CMidView *)m_lpParent)->ReleaseFocus();
	// ���õ�ǰ����Ϊ���㴰��...
	this->DrawFocus(kFocusColor);
	m_bFocus = true;
	// ֪ͨ��ͼ���ڵ�ǰ��Ƶ���ڳ�Ϊ�˽���...
	::PostMessage(m_lpParent->GetParent()->m_hWnd, WM_FOCUS_VIDEO, m_nWndIndex, NULL);
}

void CVideoWnd::OnFlyToZoomIn()
{
	m_nWndState = kFlyState;

	CRect	rcRect;
	int		nCX = 0;
	int		nCY	= 0;
	this->GetClientRect(rcRect);
	nCX = (rcRect.Width() - KHSS_VIDEO_STEP < KHSS_LEFT_VIEW_WIDTH - 50) ? 0 : (rcRect.Width() - KHSS_VIDEO_STEP);
	nCY	= (rcRect.Height() - KHSS_VIDEO_STEP/4*3 < KHSS_LEFT_VIEW_WIDTH/4*3 - 50) ? 0 : (rcRect.Height() - KHSS_VIDEO_STEP/4*3);
	if( nCX <= 0 || nCY <= 0 )
	{
		this->OnFlyToFixState();
		return;
	}
	this->ShowWindow(SW_HIDE);

	this->ModifyStyle(0, WS_THICKFRAME, SWP_FRAMECHANGED);
	this->ModifyStyleEx(0, WS_EX_TOOLWINDOW, SWP_FRAMECHANGED);
	this->SetParent(NULL);

	this->OnMoveToCenter(nCX, nCY);
	this->ShowWindow(SW_SHOW);
}

void CVideoWnd::OnFlyToZoomOut()
{
	ASSERT( m_nWndState == kFlyState );
	CRect	rcRect;
	int		nCX = ::GetSystemMetrics(SM_CXSCREEN) - 50;
	int		nCY	= ::GetSystemMetrics(SM_CYSCREEN) - 50;
	this->GetClientRect(rcRect);
	nCX = (rcRect.Width() + KHSS_VIDEO_STEP >= nCX) ? 0 : (rcRect.Width() + KHSS_VIDEO_STEP);
	nCY = (rcRect.Height() + KHSS_VIDEO_STEP/4*3 >= nCY) ? 0 : (rcRect.Height() + KHSS_VIDEO_STEP/4*3);

	if( nCX <= 0 || nCY <= 0 )
	{
		this->OnFullScreen();
		return;
	}
	this->ShowWindow(SW_HIDE);

	this->ModifyStyle(0, WS_THICKFRAME, SWP_FRAMECHANGED);
	this->ModifyStyleEx(0, WS_EX_TOOLWINDOW, SWP_FRAMECHANGED);
	this->SetParent(NULL);

	this->OnMoveToCenter(nCX, nCY);
	this->ShowWindow(SW_SHOW);
}

void CVideoWnd::OnFullScreen()
{
	if( m_nWndState == kFullState )
		return;
	m_nWndState = kFullState;
	
	WINDOWPLACEMENT wpNew = {0};
	GetWindowPlacement(&m_wPrevious);
	memcpy(&wpNew, &m_wPrevious, sizeof(wpNew));
	
	ModifyStyle(WS_THICKFRAME, 0, SWP_FRAMECHANGED);
	ModifyStyleEx(0, WS_EX_TOOLWINDOW, SWP_FRAMECHANGED);

	SetParent(NULL);
	SetWindowPos(&wndTopMost, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
	
	wpNew.showCmd = SW_NORMAL;
	wpNew.rcNormalPosition.top		= 0;
	wpNew.rcNormalPosition.bottom	= GetSystemMetrics(SM_CYSCREEN);
	wpNew.rcNormalPosition.left		= 0;
	wpNew.rcNormalPosition.right	= GetSystemMetrics(SM_CXSCREEN);

	SetWindowPlacement(&wpNew);
}

void CVideoWnd::OnRestorScreen()
{
	m_nWndState = kFlyState;

	ModifyStyle(0, WS_THICKFRAME, SWP_FRAMECHANGED);
	ModifyStyleEx(0, WS_EX_TOOLWINDOW, SWP_FRAMECHANGED);
	SetWindowPlacement(&m_wPrevious);
}

void CVideoWnd::OnZoomInEvent()
{
	if( m_nWndState == kFixState )
		return;
	this->OnFlyToZoomIn();
}

void CVideoWnd::OnZoomCloseEvent()
{
	if( m_nWndState == kFixState )
		return;
	this->OnFlyToFixState();
}

void CVideoWnd::SetTitleText(CString & szTitle)
{
	m_strTitle = szTitle;
	(this->m_hWnd != NULL) ? this->Invalidate() : NULL;
}

/*void CVideoWnd::OnActivateApp(BOOL bActive, DWORD hTask) 
{
	CWnd::OnActivateApp(bActive, hTask);

	//TRACE("Video ID = %lu, bActive = %lu\r\n", GetDlgCtrlID(), bActive);
	//SetWindowPos(&wndTopMost, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
	//SetWindowPos(bActive ? &wndTopMost : &wndNoTopMost, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
}*/

void CVideoWnd::BuildItemWnd()
{
	for(int i = 0; i < kBitNum; i++)
	{
		m_ItemWnd[i].Create(WS_CHILD | WS_VISIBLE, this, ID_RENDER_WND_BEGIN + i);
		m_ItemWnd[i].SetBitItem(m_BitArray[i]);
	}
}

void CVideoWnd::OnGetMinMaxInfo(MINMAXINFO FAR* lpMMI) 
{
	lpMMI->ptMinTrackSize.x = KHSS_MIN_VIDEO_WIDTH;
	lpMMI->ptMinTrackSize.y = KHSS_MIN_VIDEO_HEIGHT;
	CWnd::OnGetMinMaxInfo(lpMMI);
}

/*void CVideoWnd::OnNcCalcSize(BOOL bCalcValidRects, NCCALCSIZE_PARAMS* lpncsp)
{
	if( bCalcValidRects ) {
		lpncsp->rgrc[2] = lpncsp->rgrc[1];
		lpncsp->rgrc[1] = lpncsp->rgrc[0];
	}
	//CWnd::OnNcCalcSize(bCalcValidRects, lpncsp);
}*/
