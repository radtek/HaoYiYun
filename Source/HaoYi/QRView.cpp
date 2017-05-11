
#include "stdafx.h"
#include "HaoYi.h"
#include "QRView.h"

IMPLEMENT_DYNCREATE(CQRView, CHtmlView)

CQRView::CQRView()
{
}

CQRView::~CQRView()
{
}

void CQRView::DoDataExchange(CDataExchange* pDX)
{
	CHtmlView::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CQRView, CHtmlView)
	ON_WM_DESTROY()
	ON_WM_MOUSEACTIVATE()
END_MESSAGE_MAP()

#ifdef _DEBUG
void CQRView::AssertValid() const
{
	CHtmlView::AssertValid();
}

void CQRView::Dump(CDumpContext& dc) const
{
	CHtmlView::Dump(dc);
}
#endif //_DEBUG

void CQRView::OnDestroy()
{
	CHtmlView::OnDestroy();
}

void CQRView::OnBeforeNavigate2(LPCTSTR lpszURL, DWORD nFlags, LPCTSTR lpszTargetFrameName, CByteArray& baPostedData, LPCTSTR lpszHeaders, BOOL* pbCancel)
{
	CHtmlView::OnBeforeNavigate2(lpszURL, nFlags, lpszTargetFrameName, baPostedData, lpszHeaders, pbCancel);
}

int CQRView::OnMouseActivate(CWnd* pDesktopWnd, UINT nHitTest, UINT message)
{
	return CHtmlView::OnMouseActivate(pDesktopWnd, nHitTest, message);
}

/*BOOL CQRView::CreateFromStatic(UINT nID, CWnd* pParent)
{
	CStatic wndStatic;
	if( !wndStatic.SubclassDlgItem(nID, pParent) )
		return FALSE;
	// Get static control rect, convert to parent's client coords.
	CRect rc;
	wndStatic.GetWindowRect(&rc);
	pParent->ScreenToClient(&rc);
	wndStatic.DestroyWindow();

	// create HTML control (CHtmlView)
	return Create(NULL,						 // class name
		NULL,								 // title
		(WS_CHILD | WS_VISIBLE ),			 // style
		rc,									 // rectangle
		pParent,							 // parent
		nID,								 // control ID
		NULL);								 // frame/doc context not used
}*/
