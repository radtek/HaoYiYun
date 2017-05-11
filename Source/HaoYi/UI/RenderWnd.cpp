
#include "stdafx.h"
#include "RenderWnd.h"
#include "VideoWnd.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CRenderWnd::CRenderWnd()
  : m_lpParent(NULL),
	m_lpTextFont(NULL),
	m_nRendState(ST_WAIT)
{
}

CRenderWnd::~CRenderWnd()
{
	if( this->m_hWnd != NULL ) {
		this->DestroyWindow();
	}
}

BEGIN_MESSAGE_MAP(CRenderWnd, CWnd)
	ON_WM_SIZE()
	ON_WM_CREATE()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_TIMER()
END_MESSAGE_MAP()

//
// Create a new Left-View window...
BOOL CRenderWnd::Create(UINT wStyle, const CRect & rect, CWnd * pParentWnd, UINT nID)
{
	//
	// 1.0 Register the window class object...
	LPCTSTR	lpWndName = NULL;
	lpWndName = AfxRegisterWndClass(CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW,
									AfxGetApp()->LoadStandardCursor(IDC_ARROW),
									(HBRUSH)GetStockObject(BLACK_BRUSH), NULL);
	if( lpWndName == NULL )
		return FALSE;
	//
	// 2.0 Create the window directly...
	wStyle |= WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
	return CWnd::Create(lpWndName, NULL, wStyle, rect, pParentWnd, nID);
}

int CRenderWnd::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	ASSERT( GetParent() != NULL );
	m_lpParent		= (CVideoWnd *)GetParent();
	m_lpTextFont	= m_lpParent->GetTitleFont();
	m_szTxt.Format("未登录 - %s", m_lpParent->GetTitleText());

	return 0;
}

void CRenderWnd::DrawBackText(CDC * pDC)
{
	CRect  rcRect;
	ASSERT( pDC != NULL );
	this->GetClientRect(rcRect);

	CFont * pOld = NULL;
	CSize   rcSize, rcPos;
	// 填充矩形区，设置透明模式...
	pDC->FillSolidRect(rcRect, _COLOR_KEY);
	pDC->SetBkMode(TRANSPARENT);

	// 计算文字长宽像素值...
	pOld   = pDC->SelectObject(m_lpTextFont);
	rcSize = pDC->GetOutputTextExtent(m_szTxt);

	// 计算显示坐标位置...
	rcPos.cx = (rcRect.Width() - rcSize.cx) / 2;
	rcPos.cy = (rcRect.Height() - rcSize.cy) / 2;

	// 如果是其他模式，则正常显示信息...
	pDC->SetTextColor(RGB(0, 255, 0));
	pDC->TextOut(rcPos.cx, rcPos.cy, m_szTxt);
	pDC->SelectObject(pOld);
}

/*void CRenderWnd::DrawMyKbps()
{
	CSFLiveCastApp * pApp = (CSFLiveCastApp *)AfxGetApp();
	if( pApp->m_stAllInfo.stChannel[m_nIndex-1].nChannelType != channelType_RTMP )
		return;
	if( !pApp->m_stAllInfo.stChannel[m_nIndex-1].bChannelRunning )
		return;
	CString strKbps;
	CMainFrame * pWnd = (CMainFrame *)AfxGetMainWnd();
	KH_MapTask & dbPush = pWnd->GetPushMapTask();
	KH_MapTask::iterator itor = dbPush.find(m_nIndex);
	if( dbPush.end() == itor )
		return;
	CRect		rcRect;
	CSize		rcSize, rcPos;
	CFont	*	pOld = NULL;
	CDC		*	pDC = this->GetDC();

	this->GetClientRect(rcRect);

	pOld   = pDC->SelectObject(m_lpTextFont);
	rcSize = pDC->GetOutputTextExtent(m_szTxt);

	rcPos.cx = (rcRect.Width() - rcSize.cx) / 2;
	rcPos.cy = (rcRect.Height() - rcSize.cy) / 2;

	strKbps.Format("发送码流：%d Kbps", pWnd->GetSendKbps(m_nIndex));

	pDC->SetBkMode(TRANSPARENT);
	pDC->SetTextColor(RGB(20, 220, 20));
	pDC->FillSolidRect(rcRect.left, rcPos.cy + 20, rcRect.Width(), 20, _COLOR_KEY);
	pDC->TextOut(rcRect.left + 90, rcPos.cy + 20, strKbps);
	pDC->SelectObject(pOld);

	this->ReleaseDC(pDC);
}*/

BOOL CRenderWnd::OnEraseBkgnd(CDC* pDC) 
{
	if( m_nRendState != ST_WAIT )
		return TRUE;
	ASSERT( m_nRendState == ST_WAIT);
	this->DrawBackText(pDC);
	return TRUE;
}

void CRenderWnd::OnSize(UINT nType, int cx, int cy) 
{
	CWnd::OnSize(nType, cx, cy);

	/*if( cx <= 0 || cy <= 0 || m_lpParent == NULL )// || m_nRendState == ST_WAIT )
		return;
	CWnd * pWnd = m_lpParent->GetRealParent();
	if( pWnd == NULL || pWnd->m_hWnd == NULL )
		return;
	NMHDR	nmHdr	= {0};
	nmHdr.code		= WM_SIZE;
	nmHdr.hwndFrom	= m_hWnd;
	pWnd->SendMessage(WM_NOTIFY, GetDlgCtrlID(), (LPARAM)&nmHdr);*/
}

void CRenderWnd::OnLButtonDown(UINT nFlags, CPoint point) 
{
/*	if( !m_bLocalFlag && m_lpParent->GetWndState() == CVideoWnd::kFixState )
		return;
	if( this->OnLDownLocalArea(point) )
		return;
	if( m_bLocalFlag && m_lpParent->GetWndState() == CVideoWnd::kFixState )
		return;*/
	if( m_lpParent != NULL ) {
		this->ClientToScreen(&point);
		m_lpParent->ScreenToClient(&point);
		m_lpParent->OnLButtonDown(nFlags, point);
	}
}

void CRenderWnd::OnLButtonUp(UINT nFlags, CPoint point) 
{
	//if( m_lpParent->GetWndState() == CVideoWnd::kFixState )
	//	return;
	if( m_lpParent != NULL ) {
		this->ClientToScreen(&point);
		m_lpParent->ScreenToClient(&point);
		m_lpParent->OnLButtonUp(nFlags, point);
	}
}

void CRenderWnd::OnMouseMove(UINT nFlags, CPoint point) 
{
	/*if( !m_bLocalFlag && m_lpParent->GetWndState() == CVideoWnd::kFixState )
		return;
	if( this->OnMoveLocalArea(point) )
		return;
	if( m_bLocalFlag )
		return;*/
	if( m_lpParent != NULL ) {
		this->ClientToScreen(&point);
		m_lpParent->ScreenToClient(&point);
		m_lpParent->OnMouseMove(nFlags, point);
	}
}

void CRenderWnd::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
	if( m_lpParent != NULL ) {
		this->ClientToScreen(&point);
		m_lpParent->ScreenToClient(&point);
		m_lpParent->OnLButtonDblClk(nFlags, point);
	}
}

void CRenderWnd::OnRButtonDown(UINT nFlags, CPoint point) 
{
	/*CWnd * pWnd = m_lpParent->GetRealParent();
	if( pWnd == NULL || pWnd->m_hWnd == NULL )
		return;
	NMHDR	nmHdr	= {0};
	nmHdr.code		= WM_RBUTTONDOWN;
	nmHdr.hwndFrom	= m_hWnd;
	pWnd->SendMessage(WM_NOTIFY, GetDlgCtrlID(), (LPARAM)&nmHdr);*/
	
	//AfxGetMainWnd()->SendMessage(WM_SELECT_CHANNEL, m_nIndex);
	
	CWnd::OnRButtonDown(nFlags, point);
}

void CRenderWnd::OnRButtonUp(UINT nFlags, CPoint point) 
{
	AfxGetMainWnd()->SendMessage(WM_POPUP_CHANNEL_MENU);
	
	CWnd::OnRButtonUp(nFlags, point);
}

void CRenderWnd::SetRenderState(int nState)
{ 
	m_nRendState = nState; 
	this->Invalidate();
}

void CRenderWnd::SetRenderText(CString & strText)
{
	m_szTxt = strText;
	this->Invalidate();
}

void CRenderWnd::OnTimer(UINT nIDEvent) 
{
	CWnd::OnTimer(nIDEvent);
}

BOOL CRenderWnd::PreTranslateMessage(MSG* pMsg) 
{
// 	if( pMsg->message == WM_KEYDOWN ) 
// 	{
// 		if ( pMsg->wParam == VK_ESCAPE)
// 		{
// 			MessageBox("4ESC");
// 		}
// 	}
	return CWnd::PreTranslateMessage(pMsg);
}