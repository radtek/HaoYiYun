
#pragma once

class CDlgSetDVR : public CDialogEx
{
	DECLARE_DYNAMIC(CDlgSetDVR)
public:
	CDlgSetDVR(int nDBCameraID, BOOL bIsLogin, CWnd* pParent = NULL);
	virtual ~CDlgSetDVR();
	enum { IDD = IDD_SET_DVR };
protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedOK();
	DECLARE_MESSAGE_MAP()
private:
	int			m_nDBCameraID;		// ͨ�����
	BOOL		m_bIsLogin;			// ͨ���Ƿ��ѵ�¼
	int			m_nDVRPort;			// ��¼�˿�
	BOOL		m_bUseTCP;			// �Ƿ���TCP
	BOOL		m_bOpenOSD;			// �Ƿ���OSD
	BOOL		m_bOpenMirror;		// �Ƿ�������
	CString		m_strDVRName;		// ͨ������
	CString		m_strDVRAddr;		// ͨ����ַ
	CString		m_strLoginUser;		// ��¼�û�
	CString		m_strLoginPass;		// ��¼����

	friend class CHaoYiView;
};
