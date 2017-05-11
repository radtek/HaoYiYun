
#pragma once

class CDlgSetDVR : public CDialogEx
{
	DECLARE_DYNAMIC(CDlgSetDVR)
public:
	CDlgSetDVR(int nCameraID, BOOL bIsLogin, CWnd* pParent = NULL);
	virtual ~CDlgSetDVR();
	enum { IDD = IDD_SET_DVR };
protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedOK();
	DECLARE_MESSAGE_MAP()
private:
	string		m_strDBCameraID;	// 数据库中的摄像头编号
	BOOL		m_bNameChanged;		// 名称是否发生变化
	BOOL		m_bIsLogin;			// 通道是否已登录
	int			m_nDVRID;			// 通道编号
	int			m_nDVRPort;			// 登录端口
	BOOL		m_bOpenOSD;			// 是否开启OSD
	BOOL		m_bOpenMirror;		// 是否开启镜像
	CString		m_strDVRName;		// 通道名称
	CString		m_strDVRAddr;		// 通道地址
	CString		m_strLoginUser;		// 登录用户
	CString		m_strLoginPass;		// 登录密码

	friend class CHaoYiView;
};
