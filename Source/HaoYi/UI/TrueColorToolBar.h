
#pragma once

class CTrueColorToolBar : public CToolBar
{
public:
	CTrueColorToolBar();
	virtual ~CTrueColorToolBar();
protected:
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	DECLARE_MESSAGE_MAP()
public:
	BOOL LoadTrueColorToolBar(int nBtnWidth,
							  UINT uToolBar,
							  UINT uToolBarHot,
							  UINT uToolBarDisabled = 0);
private:
	BOOL SetTrueColorToolBar(UINT uToolBarType,
		                     UINT uToolBar,
						     int nBtnWidth);
};
