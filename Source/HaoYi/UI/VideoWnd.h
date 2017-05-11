
#pragma once

#include "RenderWnd.h"
#include "BitWnd.h"

class CCamera;
class CBitItem;
class CVideoWnd;

class CAutoVideo
{
public:
	CAutoVideo(CVideoWnd * inWnd);
	~CAutoVideo();
private:
	CVideoWnd	*	m_lpWnd;
};

class CVideoWnd : public CWnd
{
public:
	CVideoWnd(CBitItem ** lppBit, UINT inVideoWndID);
	virtual ~CVideoWnd();
public:
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnGetMinMaxInfo(MINMAXINFO FAR* lpMMI);
	afx_msg void OnClickItemWnd(UINT nItem);
	afx_msg LRESULT	OnMsgRecSlice(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnMsgTaskErr(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnMsgPushErr(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()
public:
	enum {
		kSnapTimerID    = 1,
		kStatusTimerID	= 2,
	};
	enum {
		kZoomOut		= 0,			// Zoom out area
		kZoomIn			= 1,			// Zoom in area
		kClose			= 2,			// Close area
		kBitNum			= 3,			// Bit number
	};
	enum WND_STATE {
		kFixState		= 0,			// Fixed state
		kFlyState		= 1,			// Flying state
		kFullState		= 2,			// Full state
	};
	enum {
		kFocusColor		= RGB(255, 255,   0),
		kNormalColor	= RGB( 96, 123, 189),
	};
public:
	WND_STATE		GetWndState()							{ return m_nWndState; }
	CWnd		*	GetRealParent()							{ return m_lpParent; }
	CRenderWnd	*	GetRenderWnd()							{ return m_lpRenderWnd; }
	CCamera		*	GetCamera()								{ return m_lpCamera; }
	int				GetCameraID()							{ return m_nWndIndex; }

	CString		&	GetTitleText()							{ return m_strTitle; }
	CFont		*	GetTitleFont()							{ return m_lpTitleFont; }
	void			SetTitleFont(CFont * inFont)			{ m_lpTitleFont = inFont; }
	void			SetTitleText(CString & szTitle);

	void			ReleaseFocus();
	void			OnFixToFlyState();
	void			OnFlyToFixState();
	
	BOOL			Create(UINT wStyle, const CRect & rect, CWnd * pParentWnd);
	BOOL			BuildCamera(GM_MapData & inMapLoc);
private:
	void			doFocusAction();
	void			doEarseRecStatus();
	void			doDrawRecStatus();
	void			doEarseLiveStatus();
	void			doDrawLiveStatus();
	void			DrawFocus(COLORREF inColor);
	void			OnZoomInEvent();
	void			OnZoomOutEvent();
	void			OnZoomCloseEvent();

	void			OnFlyToZoomIn();
	void			OnFlyToZoomOut();
	void			OnFullScreen();
	void			OnRestorScreen();
	void			OnMoveToCenter(int nWidth, int nHeight);

	void			EnableDrag();
	void			DisableDrag();
	void			OnMouseDragEvent();
	
	void			BuildItemWnd();
	void			BuildRenderWnd(int inRenderWndID);
	void			ClearResource();
private:
	int				m_nWndIndex;
	UINT			m_nVideoWndID;
	BOOL			m_bDraging;
	CRect			m_rcWinPos;
	CPoint			m_ptMouse;
	WND_STATE		m_nWndState;
	BOOL			m_bFocus;
	
	CCamera		 *  m_lpCamera;
	CWnd		 *	m_lpParent;
	CFont		 *	m_lpTitleFont;
	CString			m_strTitle;
	BOOL			m_bDrawRec;
	BOOL			m_bDrawLive;

	CBitWnd			m_ItemWnd[kBitNum];
	CBitItem	 *	m_BitArray[kBitNum];
	CRenderWnd	 *	m_lpRenderWnd;
	WINDOWPLACEMENT	m_wPrevious;
private:
	friend class CRenderWnd;
	friend class CMidView;
};
