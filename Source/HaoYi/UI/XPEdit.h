
#pragma once

class CXPEdit : public CEdit
{
public:
	CXPEdit();
	virtual ~CXPEdit();
protected:
    afx_msg void OnNcPaint();
    afx_msg UINT OnGetDlgCode();
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg LRESULT OnMouseLeave(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnMouseHover(WPARAM wParam, LPARAM lParam);
    afx_msg void OnNcCalcSize(BOOL bCalcValidRects, NCCALCSIZE_PARAMS FAR* lpncsp);
	DECLARE_MESSAGE_MAP()
public:
	void	SetVClientHeight(int nVClientHeight) { m_nVClientHeight = nVClientHeight; }
private:
	void	DrawBorder();
protected:
	BOOL    m_bHover;
	BOOL    m_bFocus;
	BOOL    m_bTracking;
    CRect   m_rectNCTop;
    CRect   m_rectNCBottom;
	int     m_nCenterOffSet;
	int     m_nVClientHeight;
};