
#pragma once

#include "XPEdit.h"
#include "XPButton.h"

class CHaoYiView;
class CRightView : public CStatic
{
	DECLARE_DYNAMIC(CRightView)
public:
	CRightView(CHaoYiView * lpParent);
	virtual ~CRightView();
protected:
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnClickButtonWnd(UINT nItemID);
	afx_msg LRESULT	OnMsgBtnLDownUp(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT	OnMsgDVRLoginResult(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()
public:
	enum {
		kLeftMargin		= 25,
		kGroundHigh		= 30,
		kEditWidth		= 140,
		kEditHigh		= 25,
		kBtnLogWidth	= 80,
		kBtnLogHigh		= 30,
		kBtnYunWidth	= 60,
		kBtnYunHigh		= 20,
		kBtnSetWidth	= 100,
		kBtnSetHigh		= 26,
	};

	enum {
		kButtonLogin       = 10001,
		kButtonLogout      = 10002,

		kButtonYunAuto     = 10003,
		kButtonYunUp	   = 10004,
		kButtonYunReset    = 10005,
		kButtonYunLeft     = 10006,
		kButtonYunDown     = 10007,
		kButtonYunRight    = 10008,

		kButtonJiaoMinus   = 10009,
		kButtonJiaoPlus    = 10010,
		kButtonFangMinus   = 10011,
		kButtonFangPlus    = 10012,
		kButtonQuanMinus   = 10013,
		kButtonQuanPlus    = 10014,

		kButtonSetVPreview = 10015,
		kButtonSetVParam   = 10016,
		kButtonSetRecord   = 10017,
		kButtonSetReview   = 10018,
		kButtonSetAlert    = 10019,
		kButtonSetCamera   = 10020,

		kEditIPAddr	       = 10021,
		kEditIPPort        = 10022,
		kEditUserName      = 10023,
		kEditUserPass      = 10024,
	};
public:
	BOOL	InitButton(CFont * lpFont);
	void	doFocusCamera(int nCameraID);
	void	doAutoCheckDVR();					// 自动连接DVR...
private:
	void	ShowText(CDC * pDC, BOOL bFocus, CString & inLogStatus);
	void	ShowButton(BOOL bDispFlag);			// 显示按钮...
	void	StateButton(BOOL bIsLogin);			// 按钮状态...

	void	doClickBtnLogin();					// 点击登录事件...
	void	doClickBtnLogout();					// 点击注销事件...

	void	doClickBtnPTZ(DWORD dwPTZCmd, BOOL bStop);

	void	doClickSetVPreview();				// 点击视频预览
	void	doClickSetVParam();					// 点击视频参数
	void	doClickSetRecord();					// 点击录像设置
	void	doClickSetReview();					// 点击录像回放
	void	doClickSetAlert();					// 点击报警布防
	void	doClickSetCamera();					// 点击摄像头设置
private:
	int				m_nCurAutoID;				// 当前自动连接DVR编号...

	CHaoYiView	*	m_lpParentDlg;				// 父窗口对象...
	CString			m_strYunTitle;				// 云台标题栏...
	CString			m_strSetTitle;				// 设置标题栏...

	CXPEdit			m_editIPAddr;				// IP地址
	CXPEdit			m_editIPPort;				// 端口
	CXPEdit			m_editUserName;				// 用户名
	CXPEdit			m_editPassWord;				// 密码

	CXPButton		m_btnLogin;					// 登录按钮
	CXPButton		m_btnLogout;				// 注销按钮

	CXPButton		m_btnYunAuto;				// 云台 - 自动
	CXPButton		m_btnYunTop;				// 云台 - 上移
	CXPButton		m_btnYunReset;				// 云台 - 重置
	CXPButton		m_btnYunLeft;				// 云台 - 左移
	CXPButton		m_btnYunBottom;				// 云台 - 下移
	CXPButton		m_btnYunRight;				// 云台 - 右移

	CXPButton		m_btnJiaoMinus;				// 焦距 - 减少
	CXPButton		m_btnJiaoPlus;				// 焦距 - 增加
	CXPButton		m_btnFangMinus;				// 放大 - 减少
	CXPButton		m_btnFangPlus;				// 放大 - 增加
	CXPButton		m_btnQuanMinus;				// 光圈 - 减少
	CXPButton		m_btnQuanPlus;				// 光圈 - 增加

	CXPButton		m_btnSetVPreview;			// 设置 - 视频预览
	CXPButton		m_btnSetVParam;				// 设置 - 视频参数
	CXPButton		m_btnSetRecord;				// 设置 - 录像/截图
	CXPButton		m_btnSetReview;				// 设置 - 回放(录像/截图)
	CXPButton		m_btnSetAlert;				// 设置 - 报警/布防
	CXPButton		m_btnSetCamera;				// 设置 - 硬件设置

	friend class CHaoYiView;
};