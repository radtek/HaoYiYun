
#pragma once
#include "afxwin.h"

class CDlgJump : public CDialogEx
{
	DECLARE_DYNAMIC(CDlgJump)
public:
	CDlgJump(int inCurPage, int inMaxPage, CWnd* pParent = NULL);
	virtual ~CDlgJump();
	enum { IDD = IDD_PAGE_JUMP };
	int GetCurSelPage() { return (m_nSelPage + 1); }
protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	DECLARE_MESSAGE_MAP()
private:
	CComboBox	m_comPage;		// 页码选择对象...
	int			m_nCurPage;		// 传递进来的当前页码...
	int			m_nMaxPage;		// 传递进来的最大页码...
	int			m_nSelPage;		// 最终选择的页码编号...
};
