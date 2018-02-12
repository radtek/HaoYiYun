
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
	CComboBox	m_comPage;		// ҳ��ѡ�����...
	int			m_nCurPage;		// ���ݽ����ĵ�ǰҳ��...
	int			m_nMaxPage;		// ���ݽ��������ҳ��...
	int			m_nSelPage;		// ����ѡ���ҳ����...
};
