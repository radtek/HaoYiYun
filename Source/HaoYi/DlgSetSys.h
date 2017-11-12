
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
	CHaoYiView * m_lpHaoYiVew;		// 父窗口对象...
	CString		m_strMainName;		// 窗口标题名称
	CString		m_strSavePath;		// 录像存放路径
	int			m_nRecRate;			// 录像码流 => Kbps
	int			m_nLiveRate;		// 直播码流 => Kbps
	int			m_nSliceVal;		// 切片时间间隔 => 0,30
	int			m_nInterVal;		// 切片交错帧 => 0,3
	BOOL		m_bAutoLinkDVR;		// 自动连接DVR设备 => 默认关闭
	BOOL		m_bAutoLinkFDFS;	// 自动连接FDFS服务器 => 默认关闭
	int			m_nWebPort;			// Web服务器端口
	CString		m_strWebAddr;		// Web服务器地址
	BOOL		m_bWebChange;		// Web地址发生变化
};
