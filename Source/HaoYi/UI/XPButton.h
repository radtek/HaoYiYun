
#pragma once

class CXPButton : public CButton
{
public:
	CXPButton();
	virtual ~CXPButton();
protected:
	virtual void PreSubclassWindow();
public:
	virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
	virtual void DoGradientFill(CDC *pDC, CRect* rect);
	virtual void DrawInsideBorder(CDC *pDC, CRect* rect);
protected:
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg LRESULT OnMouseLeave(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnMouseHover(WPARAM wParam, LPARAM lParam);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	DECLARE_MESSAGE_MAP()
protected:
	BOOL  m_bOver;		// 鼠标位于按钮之上时该值为true，反之为flase
	BOOL  m_bTracking;	// 在鼠标按下没有释放时该值为true
	BOOL  m_bSelected;	// 按钮被按下是该值为true
	BOOL  m_bFocus;		// 按钮为当前焦点所在时该值为true
};
