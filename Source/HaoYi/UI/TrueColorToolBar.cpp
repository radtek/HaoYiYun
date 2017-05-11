
#include "stdafx.h"
#include "TrueColorToolBar.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CTrueColorToolBar::CTrueColorToolBar()
{
}

CTrueColorToolBar::~CTrueColorToolBar()
{
}


BEGIN_MESSAGE_MAP(CTrueColorToolBar, CToolBar)
	ON_WM_SETCURSOR()
	ON_WM_MOUSEMOVE()
END_MESSAGE_MAP()

BOOL CTrueColorToolBar::LoadTrueColorToolBar(int nBtnWidth,
											 UINT uToolBar,
											 UINT uToolBarHot,
											 UINT uToolBarDisabled)
{
	if (!this->SetTrueColorToolBar(TB_SETIMAGELIST, uToolBar, nBtnWidth))
		return FALSE;

	// 少加载一个图片，会少几个GDI...
	//if (!this->SetTrueColorToolBar(TB_SETHOTIMAGELIST, uToolBarHot, nBtnWidth))
	//	return FALSE;

	if ( uToolBarDisabled > 0 ) {
		if (!SetTrueColorToolBar(TB_SETDISABLEDIMAGELIST, uToolBarDisabled, nBtnWidth))
			return FALSE;
	}
	return TRUE;
}
//
// 会自动用第一个像素做为透明色...
BOOL CTrueColorToolBar::SetTrueColorToolBar(UINT uToolBarType, UINT uToolBar, int nBtnWidth)
{
	CImageList	cImageList;
	CBitmap		cBitmap;
	BITMAP		bmBitmap;
	CSize		cSize;
	int			nNbBtn;

	HANDLE hBitmap = ::LoadImage(AfxGetInstanceHandle(), 
								 MAKEINTRESOURCE(uToolBar), IMAGE_BITMAP, 0, 0,
								 LR_DEFAULTSIZE|LR_CREATEDIBSECTION);
	if (!cBitmap.Attach(hBitmap) || !cBitmap.GetBitmap(&bmBitmap))
		return FALSE;

	cSize  = CSize(bmBitmap.bmWidth, bmBitmap.bmHeight); 
	nNbBtn = cSize.cx/nBtnWidth;
	RGBTRIPLE* rgb		= (RGBTRIPLE*)(bmBitmap.bmBits);
	COLORREF   rgbMask	= RGB(rgb[0].rgbtRed, rgb[0].rgbtGreen, rgb[0].rgbtBlue);
	
	// 24位模式下，GDI占用少...
	if (!cImageList.Create(nBtnWidth, cSize.cy, ILC_COLOR24|ILC_MASK, nNbBtn, 0))
		return FALSE;
	
	if (cImageList.Add(&cBitmap, rgbMask) == -1)
		return FALSE;

	/*if (uToolBarType == TB_SETIMAGELIST)
		GetToolBarCtrl().SetImageList(&cImageList);
	else if (uToolBarType == TB_SETHOTIMAGELIST)
		GetToolBarCtrl().SetHotImageList(&cImageList);
	else if (uToolBarType == TB_SETDISABLEDIMAGELIST)
		GetToolBarCtrl().SetDisabledImageList(&cImageList);
	else 
		return FALSE;*/

	// 发送消息...
	this->SendMessage(uToolBarType, 0, (LPARAM)cImageList.m_hImageList);

	// 释放资源，这里不能 cImageList.DeleteImageList 资源...
	cBitmap.DeleteObject();
	cImageList.Detach(); 
	cBitmap.Detach();
	
	return TRUE;
}

BOOL CTrueColorToolBar::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message) 
{
	if( this->GetToolBarCtrl().GetHotItem() >= 0) {
		::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_HAND));
		return true;
	}
	return CToolBar::OnSetCursor(pWnd, nHitTest, message);
}

void CTrueColorToolBar::OnMouseMove(UINT nFlags, CPoint point) 
{
	CToolBarCtrl & oBarCtrl = this->GetToolBarCtrl();
	int nIndex = oBarCtrl.HitTest(&point);
	if( nIndex >= 0 )
	{
		TBBUTTON tbButton = {0};
		if(oBarCtrl.GetButton(nIndex, &tbButton))
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
