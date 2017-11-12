
#pragma once

class CHaoYiView;
class CDlgSetSys : public CDialogEx
{
	DECLARE_DYNAMIC(CDlgSetSys)
public:
	CDlgSetSys(CHaoYiView * pHaoYiView);
	virtual ~CDlgSetSys();
	enum { IDD = IDD_SET_SYS };
protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedOK();
	DECLARE_MESSAGE_MAP()
public:
	BOOL	IsWebChange() { return m_bWebChange; }
private:
	CHaoYiView * m_lpHaoYiVew;		// �����ڶ���...
	CString		m_strMainName;		// ���ڱ�������
	CString		m_strSavePath;		// ¼����·��
	int			m_nRecRate;			// ¼������ => Kbps
	int			m_nLiveRate;		// ֱ������ => Kbps
	int			m_nSliceVal;		// ��Ƭʱ���� => 0,30
	int			m_nInterVal;		// ��Ƭ����֡ => 0,3
	BOOL		m_bAutoLinkDVR;		// �Զ�����DVR�豸 => Ĭ�Ϲر�
	BOOL		m_bAutoLinkFDFS;	// �Զ�����FDFS������ => Ĭ�Ϲر�
	int			m_nWebPort;			// Web�������˿�
	CString		m_strWebAddr;		// Web��������ַ
	BOOL		m_bWebChange;		// Web��ַ�����仯
};
