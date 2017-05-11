
#include "stdafx.h"
#include "SFToolBar.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

CSFToolBar::CSFToolBar()
{
}

CSFToolBar::~CSFToolBar()
{
	m_ilToolBarNormal.DeleteImageList();
	m_ilToolBarNormal.Detach();

	m_ilToolBarHot.DeleteImageList();
	m_ilToolBarHot.Detach();

	m_ilToolBarDisable.DeleteImageList();
	m_ilToolBarDisable.Detach();
}


BEGIN_MESSAGE_MAP(CSFToolBar, CToolBar)
	ON_WM_ERASEBKGND()
	ON_WM_SETCURSOR()
	ON_WM_MOUSEMOVE()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSFToolBar message handlers
//------------------------------------------------------
// 设置图标
// nType : 1 表示Normal, 2 表示Hot, 3 表示Disable
//------------------------------------------------------
void CSFToolBar::SFSetImageList(UINT nID, 
								int nType, 
								int cx, 
								int cy, 
								COLORREF crMask )
{
	CBitmap bm;
	bm.LoadBitmap(nID);
	switch(nType)
	{
	case 1:	
		m_ilToolBarNormal.DeleteImageList();
		m_ilToolBarNormal.Detach();
		m_ilToolBarNormal.Create(cx, cy, ILC_COLORDDB | ILC_MASK, 8, 8);
		m_ilToolBarNormal.Add(&bm, crMask);
		this->GetToolBarCtrl().SetImageList(&m_ilToolBarNormal);
		break;
	case 2:
		m_ilToolBarHot.DeleteImageList();
		m_ilToolBarHot.Detach();
		m_ilToolBarHot.Create(cx, cy, ILC_COLORDDB | ILC_MASK, 8, 8);
		m_ilToolBarHot.Add(&bm, crMask);
		this->GetToolBarCtrl().SetHotImageList(&m_ilToolBarHot);
		break;
	case 3:
		m_ilToolBarDisable.DeleteImageList();
		m_ilToolBarDisable.Detach();
		m_ilToolBarDisable.Create(cx, cy, ILC_COLORDDB | ILC_MASK, 8, 8);
		m_ilToolBarDisable.Add(&bm, crMask);
		this->GetToolBarCtrl().SetDisabledImageList(&m_ilToolBarDisable);
		break;
	default:
		break;
	}

	bm.DeleteObject();
	bm.Detach();
	return;
}

BOOL CSFToolBar::OnEraseBkgnd(CDC* pDC) 
{
	RECT rect;
	this->GetClientRect(&rect);
	pDC->FillSolidRect(&rect, RGB(196,196,196));
	return TRUE;
}

BOOL CSFToolBar::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message) 
{
	if( this->GetToolBarCtrl().GetHotItem() >= 0) {
		::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_HAND));
		return true;
	}
	return CToolBar::OnSetCursor(pWnd, nHitTest, message);
}

void CSFToolBar::OnMouseMove(UINT nFlags, CPoint point) 
{
	CToolBarCtrl &barCtrl = this->GetToolBarCtrl();
	int nIndex = barCtrl.HitTest(&point);
	if( nIndex >= 0 )
	{
		TBBUTTON tbButton;
		if(barCtrl.GetButton(nIndex, &tbButton))
		{
			/*CMainFrame *pWnd = (CMainFrame *)AfxGetMainWnd();
			if(pWnd)
			{
				pWnd->SFShowTip(tbButton.idCommand);
			}*/
		}
	}

	CToolBar::OnMouseMove(nFlags, point);
}
