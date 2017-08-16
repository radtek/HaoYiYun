
#include "stdafx.h"
#include "XPButton.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CXPButton::CXPButton()
{
	m_bOver = m_bSelected = m_bTracking = m_bFocus = FALSE;
}

CXPButton::~CXPButton()
{
}

BEGIN_MESSAGE_MAP(CXPButton, CButton)
	ON_WM_ERASEBKGND()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_WM_LBUTTONDOWN()
	ON_MESSAGE(WM_MOUSELEAVE, OnMouseLeave)
	ON_MESSAGE(WM_MOUSEHOVER, OnMouseHover)
END_MESSAGE_MAP()

void CXPButton::OnLButtonDown(UINT nFlags, CPoint point) 
{
	::PostMessage(this->GetParent()->m_hWnd, WM_DEVICE_BUTTON_LDOWNUP, this->GetDlgCtrlID(), false);
	CButton::OnLButtonDown(nFlags, point);
}

void CXPButton::OnLButtonUp(UINT nFlags, CPoint point) 
{
	::PostMessage(this->GetParent()->m_hWnd, WM_DEVICE_BUTTON_LDOWNUP, this->GetDlgCtrlID(), true);
	CButton::OnLButtonUp(nFlags, point);
}

// ��� Owner Draw ����...
void CXPButton::PreSubclassWindow() 
{
	CButton::PreSubclassWindow();
	this->ModifyStyle(0, BS_OWNERDRAW);
}

// ��Ҫ���Ʊ�������...
BOOL CXPButton::OnEraseBkgnd(CDC* pDC) 
{
	return TRUE;
}

void CXPButton::OnMouseMove(UINT nFlags, CPoint point) 
{
	if( !m_bTracking ) {
		TRACKMOUSEEVENT tme = {0};
		tme.cbSize = sizeof(tme);
		tme.hwndTrack = m_hWnd;
		tme.dwFlags = TME_LEAVE | TME_HOVER;
		tme.dwHoverTime = 1;
		m_bTracking = _TrackMouseEvent(&tme);
	}
	CButton::OnMouseMove(nFlags, point);
}

LRESULT CXPButton::OnMouseLeave(WPARAM wParam, LPARAM lParam)
{
	m_bOver = FALSE;
	m_bTracking = FALSE;
	this->InvalidateRect(NULL, FALSE);
	return S_OK;
}

LRESULT CXPButton::OnMouseHover(WPARAM wParam, LPARAM lParam)
{
	m_bOver = TRUE;
	this->InvalidateRect(NULL);
	return S_OK;
}

void CXPButton::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	// ��lpDrawItemStruct��ȡ�ؼ��������Ϣ...
	POINT pt = {0};
	CPen * hOldPen = NULL;
	CRect rect = lpDrawItemStruct->rcItem;
	CDC * pDC = CDC::FromHandle(lpDrawItemStruct->hDC);
	UINT state = lpDrawItemStruct->itemState;
	int  nSaveDC = pDC->SaveDC();
	TCHAR strText[MAX_PATH + 1] = {0};
	::GetWindowText(this->m_hWnd, strText, MAX_PATH);

	// ��ť����߿�...
	CPen penBoundry;
	if( state & ODS_DISABLED ) {
		penBoundry.CreatePen(PS_INSIDEFRAME | PS_SOLID, 1, RGB(128, 128, 128));
	} else {
		penBoundry.CreatePen(PS_INSIDEFRAME | PS_SOLID, 1, RGB(66, 120, 255));
	}
	// ����ť����߿�����һ���뾶Ϊ5��Բ�Ǿ���
	pt.x = 5; pt.y = 5;
	hOldPen = pDC->SelectObject(&penBoundry);
	pDC->RoundRect(&rect, pt);
	
	// ��ȡ��ť��״̬...
	if( state & ODS_FOCUS ) {
		m_bFocus = TRUE;
		m_bSelected = TRUE;
	} else {
		m_bFocus = FALSE;
		m_bSelected = FALSE;
	}
	if( state & ODS_SELECTED || state & ODS_DEFAULT ) {
		m_bFocus = TRUE;
	}
	// ѡ�����ʣ�ɾ��֮...
	pDC->SelectObject(hOldPen);
	penBoundry.DeleteObject();

	// ƽ�滯...
	rect.DeflateRect(CSize(GetSystemMetrics(SM_CXEDGE), GetSystemMetrics(SM_CYEDGE)));
	
	// ��䰴ť�ĵ�ɫ...
	this->DoGradientFill(pDC, &rect);
	
	// ���ݰ�ť��״̬�����ڱ߿�...
	if( m_bOver || m_bSelected ) {
		this->DrawInsideBorder(pDC, &rect);
	}
	
	// ��ʾ��ť���ı�...
	if( strlen(strText) > 0 ) {
		CFont * hFont = this->GetFont();
		CFont * hOldFont = pDC->SelectObject(hFont);
		CSize  szExtent = pDC->GetTextExtent(strText, lstrlen(strText));
		CPoint pt( rect.CenterPoint().x - szExtent.cx / 2, rect.CenterPoint().y - szExtent.cy / 2);
		if( state & ODS_SELECTED ) { 
			pt.Offset(1, 1);
		}
		int nMode = pDC->SetBkMode(TRANSPARENT);
		int nDSS  = ((state & ODS_DISABLED) ? DSS_DISABLED : DSS_NORMAL);
		pDC->DrawState(pt, szExtent, strText, nDSS, TRUE, 0, (HBRUSH)NULL);
		pDC->SelectObject(hOldFont);
		pDC->SetBkMode(nMode);
	}
	pDC->RestoreDC(nSaveDC);
}

// ���ư�ť�ĵ�ɫ...
void CXPButton::DoGradientFill(CDC *pDC, CRect* rect)
{
	CRect rcNew;
	COLORREF brColor[64] = {0};
	int nWidth = rect->Width();	
	int nHeight = rect->Height();
	// ׼����ɫ�б�...
	for(int i = 0; i < 64; ++i) {
		if( m_bOver ) {
			brColor[i] = m_bFocus ? RGB(255 - (i / 4), 255 - (i / 4), 255 - (i / 3)) : RGB(255 - (i / 4), 255 - (i / 4), 255 - (i / 5));
		} else {
			brColor[i] = m_bFocus ? RGB(255 - (i / 3), 255 - (i / 3), 255 - (i / 4)) : RGB(255 - (i / 3), 255 - (i / 3), 255 - (i / 5));
		}
	}
	// ���ƽ����...
	for(int i = rect->top; i <= nHeight; ++i) {
		rcNew.SetRect(rect->left, i, nWidth + 2, i + 1);
		pDC->FillSolidRect(&rcNew, brColor[((i * 63) / nHeight)]);
	}
}

// ���ư�ť���ڱ߿�..
void CXPButton::DrawInsideBorder(CDC *pDC, CRect* rect)
{
	CPen pLeft, pRight, pTop, pBottom;
	
	if( m_bSelected && !m_bOver ) {
		// ���ָ�����ڰ�ť֮��ʱ��ť���ڱ߿�
		pLeft.CreatePen(PS_INSIDEFRAME | PS_SOLID, 3, RGB(153, 198, 252)); 
		pTop.CreatePen(PS_INSIDEFRAME | PS_SOLID, 2, RGB(162, 201, 255));
		pRight.CreatePen(PS_INSIDEFRAME | PS_SOLID, 3, RGB(162, 189, 252));
		pBottom.CreatePen(PS_INSIDEFRAME | PS_SOLID, 2, RGB(162, 201, 255));
	} else {
		// ��ť��ý���ʱ��ť���ڱ߿�
		pLeft.CreatePen(PS_INSIDEFRAME | PS_SOLID, 3, RGB(250, 196, 88)); 
		pRight.CreatePen(PS_INSIDEFRAME | PS_SOLID, 3, RGB(251, 202, 106));
		pTop.CreatePen(PS_INSIDEFRAME | PS_SOLID, 2, RGB(252, 210, 121));
		pBottom.CreatePen(PS_INSIDEFRAME | PS_SOLID, 2, RGB(229, 151, 0));
	}
	
	CPoint oldPoint = pDC->MoveTo(rect->left, rect->bottom - 1);
	CPen* pOldPen = pDC->SelectObject(&pLeft);
	pDC->LineTo(rect->left, rect->top + 1);
	pDC->SelectObject(&pRight);
	pDC->MoveTo(rect->right - 1, rect->bottom - 1);
	pDC->LineTo(rect->right - 1, rect->top);
	pDC->SelectObject(&pTop);
	pDC->MoveTo(rect->left - 1, rect->top);
	pDC->LineTo(rect->right - 1, rect->top);
	pDC->SelectObject(&pBottom);
	pDC->MoveTo(rect->left, rect->bottom);
	pDC->LineTo(rect->right - 1, rect->bottom);
	pDC->SelectObject(pOldPen);
	pDC->MoveTo(oldPoint);

	//if( m_bSelected && !m_bOver ) {
	//	::DrawFocusRect(pDC->m_hDC,rect);
	//}

	pLeft.DeleteObject();
	pRight.DeleteObject();
	pTop.DeleteObject();
	pBottom.DeleteObject();
}


