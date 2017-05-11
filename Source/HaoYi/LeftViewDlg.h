
#pragma once

#include "QRStatic.h"

class CLeftViewDlg : public CFormView
{
	DECLARE_DYNCREATE(CLeftViewDlg)
protected:
	CLeftViewDlg();
	virtual ~CLeftViewDlg();
public:
	enum { IDD = IDD_LEFTVIEW_DLG };

#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	virtual void OnInitialUpdate();
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL PreTranslateMessage(MSG* pMsg);

	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnRclickTreeDevice(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnSelchangedTreeDevice(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);

	DECLARE_MESSAGE_MAP()
public:
	CTreeCtrl   &   GetTreeCtrl() { return m_DeviceTree; }
protected:
	CTreeCtrl		m_DeviceTree;
	CImageList		m_ImageList;
	CToolTipCtrl	m_tipTree;
	HTREEITEM		m_hRootItem;

	CQRStatic		m_QRStatic;
};
