
#pragma once

#include "CPU.h"
#include "BCMenu.h"
#include "InfoBar.h"
#include "TrueColorToolBar.h"

class CMainFrame : public CFrameWnd
{
protected:
	CMainFrame();
	DECLARE_DYNCREATE(CMainFrame)
public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext);
public:
	virtual ~CMainFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif
protected:
	afx_msg void OnClose();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
	DECLARE_MESSAGE_MAP()
private:
	enum {
		kStatusTimerID	 =  1,
	};
private:
	void		LoadMyToolBar();
	HMENU		NewDefaultMenu();
	GM_Error	MakeGMSavePath();
private:
	//CInfoBar			m_InfoBar;
	CReBar				m_wndReBar;
	BCMenu				m_menuDefault;
	CStatusBar			m_wndStatusBar;
	CTrueColorToolBar   m_wndToolBar;

	CCpuMemRate			m_cpu;

	friend class CHaoYiApp;
};