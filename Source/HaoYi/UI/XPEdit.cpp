
#include "stdafx.h"
#include "XPEdit.h"

CXPEdit::CXPEdit()
  : m_rectNCBottom(0, 0, 0, 0)
  , m_rectNCTop(0, 0, 0, 0)
  , m_nVClientHeight(0)
  , m_nCenterOffSet(0)
  , m_bTracking(false)
  , m_bFocus(false)
  , m_bHover(false)
{

}

CXPEdit::~CXPEdit()
{
}

BEGIN_MESSAGE_MAP(CXPEdit, CEdit)
    ON_WM_NCPAINT()
	ON_WM_SETFOCUS()
	ON_WM_KILLFOCUS()
	ON_WM_MOUSEMOVE()
    ON_WM_GETDLGCODE()
    ON_WM_NCCALCSIZE()
	ON_MESSAGE(WM_MOUSELEAVE, OnMouseLeave)
	ON_MESSAGE(WM_MOUSEHOVER, OnMouseHover)
END_MESSAGE_MAP()

void CXPEdit::OnMouseMove(UINT nFlags, CPoint point) 
{
	if( !m_bTracking ) {
		TRACKMOUSEEVENT tme = {0};
		tme.cbSize = sizeof(tme);
		tme.hwndTrack = m_hWnd;
		tme.dwFlags = TME_LEAVE | TME_HOVER;
		tme.dwHoverTime = 1;
		m_bTracking = _TrackMouseEvent(&tme);
	}
	CEdit::OnMouseMove(nFlags, point);
}

LRESULT CXPEdit::OnMouseLeave(WPARAM wParam, LPARAM lParam)
{
	m_bHover = FALSE;
	m_bTracking = FALSE;
    this->DrawBorder();
	return S_OK;
}

LRESULT CXPEdit::OnMouseHover(WPARAM wParam, LPARAM lParam)
{
	m_bHover = TRUE;
    this->DrawBorder();
	return S_OK;
}

void CXPEdit::OnSetFocus(CWnd* pOldWnd)
{
    CEdit::OnSetFocus(pOldWnd);
    m_bFocus = TRUE;
    this->DrawBorder();
}
 
void CXPEdit::OnKillFocus(CWnd* pNewWnd)
{
    CEdit::OnKillFocus(pNewWnd);
	m_bFocus = FALSE;
    this->DrawBorder();
}

void CXPEdit::DrawBorder()
{
	CRect rcWin, rcRect;
	CDC * lpDC = this->GetDC();
	this->GetWindowRect(rcWin);
	rcRect = CRect(0, 0, rcWin.Width(), rcWin.Height());
	if( !m_rectNCTop.IsRectEmpty() ) {
		rcRect.top -= m_nCenterOffSet;
		rcRect.bottom -= m_nCenterOffSet;
		rcRect.left  -= 2;  // 勉强解决边框重绘...
		rcRect.right += 2;  // 勉强解决边框重绘...
	}
	if( m_bHover ) {
		lpDC->Draw3dRect(rcRect, 0xFDC860, 0xFDC860);
		rcRect.DeflateRect(1, 1);
		lpDC->Draw3dRect(rcRect, 0xE7AC49, 0xE7AC49);
	} else if( m_bFocus ) {
		lpDC->Draw3dRect(rcRect, GetSysColor(COLOR_BTNFACE), GetSysColor(COLOR_BTNFACE));
		rcRect.DeflateRect(1, 1);
		lpDC->Draw3dRect(rcRect, 0xE7AC49, 0xE7AC49);
	} else {
		lpDC->Draw3dRect(rcRect, GetSysColor(COLOR_WINDOW), GetSysColor(COLOR_WINDOW) );
		rcRect.DeflateRect(1, 1);
		lpDC->Draw3dRect(rcRect, GetSysColor(COLOR_WINDOW), GetSysColor(COLOR_WINDOW));
	}
	this->ReleaseDC(lpDC);
}

void CXPEdit::OnNcCalcSize(BOOL bCalcValidRects, NCCALCSIZE_PARAMS FAR* lpncsp) 
{
	// 计算需要的矩形区域...
    CRect rectWnd, rectClient;
    this->GetWindowRect(rectWnd);
    this->GetClientRect(rectClient);
	if( rectClient.IsRectEmpty() ) {
		rectClient.SetRect(0, 0, rectWnd.Width(), rectWnd.Height());
	}
    this->ClientToScreen(rectClient);
	
    UINT uiCenterOffset = (rectClient.Height() - m_nVClientHeight) / 2;
    UINT uiCY = (rectWnd.Height() - rectClient.Height()) / 2;
    UINT uiCX = (rectWnd.Width() - rectClient.Width()) / 2;
 
    rectWnd.OffsetRect(-rectWnd.left, -rectWnd.top);

	m_rectNCTop = rectWnd;
    m_rectNCTop.DeflateRect(uiCX, uiCY, uiCX, uiCenterOffset + m_nVClientHeight + uiCY);
    m_rectNCBottom = rectWnd;
    m_rectNCBottom.DeflateRect(uiCX, uiCenterOffset + m_nVClientHeight + uiCY, uiCX, uiCY);
 
    lpncsp->rgrc[0].top += uiCenterOffset;
    lpncsp->rgrc[0].bottom -= uiCenterOffset;
	m_nCenterOffSet = uiCenterOffset;
 
    lpncsp->rgrc[0].left  += uiCX;
    lpncsp->rgrc[0].right -= uiCX;
}

void CXPEdit::OnNcPaint() 
{
    this->Default();
	CWindowDC dc(this);
	BOOL bReadOnly = ((this->GetStyle() & ES_READONLY) ? true : false);
	COLORREF clrRect = GetSysColor((!this->IsWindowEnabled() || bReadOnly) ? COLOR_BTNFACE : COLOR_WINDOW);
	dc.FillSolidRect(m_rectNCBottom, clrRect);
	dc.FillSolidRect(m_rectNCTop, clrRect);
	//TRACE("Enabled = %d, ReadOnly = %d, color = %lu \n", this->IsWindowEnabled(), bReadOnly, clrRect);
}
 
UINT CXPEdit::OnGetDlgCode() 
{
    if( m_rectNCTop.IsRectEmpty() ) {
        SetWindowPos(NULL, 0, 0, 0, 0, SWP_NOOWNERZORDER | SWP_NOSIZE | SWP_NOMOVE | SWP_FRAMECHANGED | SWP_NOREDRAW);
    }
    return CEdit::OnGetDlgCode();
}