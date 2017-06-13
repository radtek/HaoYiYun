
#pragma once

class CVideoWnd;
class CRenderWnd : public CWnd
{
public:
	CRenderWnd();
	virtual ~CRenderWnd();
public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	DECLARE_MESSAGE_MAP()
public:
	enum {
		ST_WAIT			=	0,					// Wait state...
		ST_RENDER		=	1					// Render state...
	};
	enum {
		kFlowTimerID    = 1,
	};
public:
	CVideoWnd	*	GetVideoWnd() { return m_lpParent; }
	int				GetRenderState() { return m_nRendState; }
	void			SetRenderState(int nState);
	void			SetRenderText(CString & strText);
	void			SetStreamStatus(CString & strStatus);

	BOOL			Create(UINT wStyle, const CRect & rect, CWnd * pParentWnd, UINT nID);
private:
	void			DrawFlowKbps();
	void			DrawBackText(CDC * pDC);
private:
	int				m_nRendState;
	CFont		*	m_lpTextFont;				// Store the video-font...
	CSize			m_PosRecvKbps;				// 接收码流位置
	CSize			m_PosSendKbps;				// 发送码流位置
	CString			m_strStreamStatus;			// 流转发数据状态信息...
	CString			m_szTxt;					// Store the text string...
	CVideoWnd	 *	m_lpParent;					// Store the Parent window...
};
