
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
	m_PosRecvKbps.cx = m_PosRecvKbps.cy = 0;
	m_PosSendKbps.cx = m_PosSendKbps.cy = 0;
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
	m_szTxt.Format("δ��¼ - %s", m_lpParent->GetWebTitleText());
	m_strStreamStatus = "δ����...";

	this->SetTimer(kFlowTimerID, 1000, NULL);

	return 0;
}

void CRenderWnd::DrawBackText(CDC * pDC)
{
	CRect  rcRect;
	ASSERT( pDC != NULL );
	this->GetClientRect(rcRect);

	CFont * pOld = NULL;
	CSize   rcSize, rcPos;
	// ��������������͸��ģʽ...
	pDC->FillSolidRect(rcRect, _COLOR_KEY);
	pDC->SetBkMode(TRANSPARENT);

	// �������ֳ�������ֵ...
	pOld   = pDC->SelectObject(m_lpTextFont);
	rcSize = pDC->GetOutputTextExtent(m_szTxt);

	// ������ʾ����λ��...
	rcPos.cx = (rcRect.Width() - rcSize.cx) / 2;
	rcPos.cy = (rcRect.Height() - rcSize.cy) / 2;

	if( m_lpParent->IsCameraDevice() && m_lpParent->IsDeviceStatus() ) {
		// ���������ͷ�豸ģʽ����������Ԥ�����棬ֻ��ʾ����...
		CRect rcDraw;
		pDC->SetTextColor(RGB(0, 255, 0));
		rcSize = pDC->GetOutputTextExtent(m_szTxt);
		if( rcSize.cx < rcRect.Width() ) {
			// ���ֳ��ȱȴ���С��ֱ��TextOut...
			pDC->TextOut(rcPos.cx, rcPos.cy, m_szTxt);
		} else {
			// ���ֳ��ȱȴ��ڴ�ʹ��DrawText...
			rcDraw.left = 20;
			rcDraw.right = rcRect.Width() - 20 * 2;
			rcDraw.top = rcPos.cy - 30;
			rcDraw.bottom = rcDraw.top + 80;
			pDC->DrawText(m_szTxt, rcDraw, DT_CENTER | DT_WORDBREAK | DT_EDITCONTROL | DT_EXTERNALLEADING);
		}
		pDC->SelectObject(pOld);
	} else {
		// �������ת��ģʽ����ʾ������Ϣ...
		CRect rcPull;
		CString strPushUrl, strPushKbps, strPullKbps;
		CString strPullUrl, strStatus, outPullUrl;
		LPCTSTR lpszPush = m_lpParent->GetStreamPushUrl();
		m_lpParent->GetStreamPullUrl(outPullUrl);
		strStatus.Format("ͨ��״̬��%s", m_strStreamStatus);
		strPushUrl.Format("������ַ��%s", ((lpszPush != NULL) ? lpszPush : "��"));
		strPullKbps.Format("�������ʣ�%d Kbps", m_lpParent->GetRecvPullKbps());
		strPushKbps.Format("�������ʣ�%d Kbps", m_lpParent->GetSendPushKbps());
		pDC->SetTextColor(RGB(20, 220, 20));
		pDC->TextOut(rcRect.left + 50, rcPos.cy - 60, strStatus);

		strPullUrl = "������ַ��";
		rcSize = pDC->GetOutputTextExtent(strPullUrl);
		pDC->TextOut(rcRect.left + 50, rcPos.cy - 36, strPullUrl);

		rcPull.left = rcRect.left + 50 + rcSize.cx;
		rcPull.top  = rcPos.cy - 36;
		rcPull.right = rcRect.Width() - 30;
		rcPull.bottom = rcPull.top + 20 + 20;
		pDC->DrawText(outPullUrl, rcPull, DT_LEFT | DT_WORDBREAK | DT_EDITCONTROL | DT_EXTERNALLEADING);

		m_PosRecvKbps.cy = rcPos.cy + 0;
		m_PosRecvKbps.cx = rcRect.left + 50;
		pDC->TextOut(m_PosRecvKbps.cx, m_PosRecvKbps.cy, strPullKbps);

		pDC->TextOut(rcRect.left + 50, rcPos.cy + 20, strPushUrl);

		m_PosSendKbps.cy = rcPos.cy + 40;
		m_PosSendKbps.cx = rcRect.left + 50;
		pDC->TextOut(m_PosSendKbps.cx, m_PosSendKbps.cy, strPushKbps);

		pDC->SelectObject(pOld);
	}
}

void CRenderWnd::DrawFlowKbps()
{
	// ֻҪ���ӳɹ��Ϳ��ԣ�Ϊ�˴�����ʱ...
	if( !m_lpParent->IsStreamLogin() )
		return;
	ASSERT( m_lpParent->IsStreamLogin() );
	// �ֱ��ȡ���պͷ��͵�������Ϣ => ������ʱ...
	int nRecvKbps = m_lpParent->GetRecvPullKbps();
	int nSendKbps = m_lpParent->GetSendPushKbps();
	// ���������ͷ�豸����������Ԥ�����棬ֻ��⣬����ʾ�������Դ�����ʱ...
	if( m_lpParent->IsCameraDevice() && m_lpParent->IsDeviceStatus() )
		return;
	// ��ȡ�������ʺ���������...
	CRect	rcRect;
	CFont *	pOld = NULL;
	CDC	* pDC = this->GetDC();
	CString strPushKbps, strPullKbps;
	strPullKbps.Format("�������ʣ�%d Kbps", nRecvKbps);
	strPushKbps.Format("�������ʣ�%d Kbps", nSendKbps);

	this->GetClientRect(rcRect);
	pDC->SetBkMode(TRANSPARENT);
	pDC->SetTextColor(RGB(20, 220, 20));
	pOld = pDC->SelectObject(m_lpTextFont);

	pDC->FillSolidRect(rcRect.left, m_PosRecvKbps.cy, rcRect.Width(), 20, _COLOR_KEY);
	pDC->TextOut(m_PosRecvKbps.cx, m_PosRecvKbps.cy, strPullKbps);

	pDC->FillSolidRect(rcRect.left, m_PosSendKbps.cy, rcRect.Width(), 20, _COLOR_KEY);
	pDC->TextOut(m_PosSendKbps.cx, m_PosSendKbps.cy, strPushKbps);

	pDC->SelectObject(pOld);
	this->ReleaseDC(pDC);
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

	strKbps.Format("����������%d Kbps", pWnd->GetSendKbps(m_nIndex));

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

void CRenderWnd::SetStreamStatus(CString & strStatus)
{
	m_strStreamStatus = strStatus;
	this->Invalidate();
}

void CRenderWnd::OnTimer(UINT_PTR nIDEvent) 
{
	if( nIDEvent == kFlowTimerID ) {
		this->DrawFlowKbps();
	}
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