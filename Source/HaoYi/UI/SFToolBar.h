
#pragma once

class CSFToolBar : public CToolBar
{
public:
	CSFToolBar();
	virtual ~CSFToolBar();
protected:
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	DECLARE_MESSAGE_MAP()
public:
	void	SFSetImageList(UINT nID, int nType, int cx, int cy, COLORREF crMask = RGB(255,0,255));
private:
	CImageList		m_ilToolBarDisable;
	CImageList		m_ilToolBarNormal;
	CImageList		m_ilToolBarHot;
};
