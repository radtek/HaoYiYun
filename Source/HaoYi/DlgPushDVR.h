
#pragma once

class CDlgPushDVR : public CDialogEx
{
	DECLARE_DYNAMIC(CDlgPushDVR)
public:
	CDlgPushDVR(BOOL bIsEdit, int nDBCameraID, BOOL bIsLogin, CWnd* pParent = NULL);
	virtual ~CDlgPushDVR();
	enum { IDD = IDD_PUSH_DVR };
protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	afx_msg void OnBtnChoose();
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedRadioRtsp();
	afx_msg void OnBnClickedRadioFile();
	DECLARE_MESSAGE_MAP()
public:
	GM_MapData & GetPushData()	{ return m_MapData; }
private:
	void		FillEditValue();
	void		BuildPushRecord();
	BOOL		CheckRtspURL(LPCTSTR lpszFullURL);
private:
	GM_MapData			m_MapData;		// ���ͨ��������Ϣ
	int					m_nDBCameraID;	// �޸�ʱ��ȡ����
	CString				m_strRtspURL;	// ������ַ
	CString				m_strMP4File;	// MP4�ļ�
	BOOL				m_bPushAuto;	// �Ͽ��Զ�����
	BOOL				m_bFileLoop;	// �ļ�ѭ����־
	BOOL				m_bFileMode;	// �ļ�ģʽ��RTSPģʽ
	BOOL				m_bEditMode;	// �༭״̬
	BOOL				m_bIsLogin;		// ��¼��־ => �༭ģʽ����Ч
	CString				m_strDVRName;	// ͨ������

	friend class CHaoYiView;
};
