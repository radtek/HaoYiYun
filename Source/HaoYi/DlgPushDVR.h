
#pragma once

class CDlgPushDVR : public CDialogEx
{
	DECLARE_DYNAMIC(CDlgPushDVR)
public:
	CDlgPushDVR(BOOL bIsEdit, int nCameraID, BOOL bIsLogin, CWnd* pParent = NULL);
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
	GM_MapData			m_MapData;		// 存放通道配置信息
	int					m_nCameraID;	// 修改时读取配置
	CString				m_strRtspURL;	// 拉流地址
	CString				m_strMP4File;	// MP4文件
	BOOL				m_bPushAuto;	// 断开自动推送
	BOOL				m_bFileLoop;	// 文件循环标志
	BOOL				m_bFileMode;	// 文件模式或RTSP模式
	BOOL				m_bEditMode;	// 编辑状态
	BOOL				m_bIsLogin;		// 登录标志 => 编辑模式下有效
	CString				m_strDVRName;	// 通道名称

	friend class CHaoYiView;
};
