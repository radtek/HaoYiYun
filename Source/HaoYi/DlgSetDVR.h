
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
	int			m_nDBCameraID;		// 通道编号
	BOOL		m_bIsLogin;			// 通道是否已登录
	int			m_nDVRPort;			// 登录端口
	BOOL		m_bUseTCP;			// 是否开启TCP
	BOOL		m_bOpenOSD;			// 是否开启OSD
	BOOL		m_bOpenMirror;		// 是否开启镜像
	CString		m_strDVRName;		// 通道名称
	CString		m_strDVRAddr;		// 通道地址
	CString		m_strLoginUser;		// 登录用户
	CString		m_strLoginPass;		// 登录密码

	friend class CHaoYiView;
};
