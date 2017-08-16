
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
	afx_msg LRESULT	OnMsgDeviceBtnLDownUp(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT	OnMsgDeviceLoginResult(WPARAM wParam, LPARAM lParam);
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
	void	doAutoCheckDVR();					// �Զ�����DVR...
private:
	void	ShowText(CDC * pDC, BOOL bFocus, CString & inLogStatus);
	void	ShowButton(BOOL bDispFlag);			// ��ʾ��ť...
	void	StateButton(BOOL bIsLogin);			// ��ť״̬...
	void	DrawStreamText(CDC * pDC);

	void	doDeviceLogin();					// �����¼�¼�...
	void	doDeviceLogout();					// ���ע���¼�...

	void	doClickBtnPTZ(DWORD dwPTZCmd, BOOL bStop);

	void	doClickSetVPreview();				// �����ƵԤ��
	void	doClickSetVParam();					// �����Ƶ����
	void	doClickSetRecord();					// ���¼������
	void	doClickSetReview();					// ���¼��ط�
	void	doClickSetAlert();					// �����������
	void	doClickSetCamera();					// �������ͷ����
private:
	int				m_nCurAutoID;				// ��ǰ�Զ�����DVR���...

	CHaoYiView	*	m_lpParentDlg;				// �����ڶ���...
	CString			m_strYunTitle;				// ��̨������...
	CString			m_strSetTitle;				// ���ñ�����...

	CXPEdit			m_editIPAddr;				// IP��ַ
	CXPEdit			m_editIPPort;				// �˿�
	CXPEdit			m_editUserName;				// �û���
	CXPEdit			m_editPassWord;				// ����

	CXPButton		m_btnLogin;					// ��¼��ť
	CXPButton		m_btnLogout;				// ע����ť

	CXPButton		m_btnYunAuto;				// ��̨ - �Զ�
	CXPButton		m_btnYunTop;				// ��̨ - ����
	CXPButton		m_btnYunReset;				// ��̨ - ����
	CXPButton		m_btnYunLeft;				// ��̨ - ����
	CXPButton		m_btnYunBottom;				// ��̨ - ����
	CXPButton		m_btnYunRight;				// ��̨ - ����

	CXPButton		m_btnJiaoMinus;				// ���� - ����
	CXPButton		m_btnJiaoPlus;				// ���� - ����
	CXPButton		m_btnFangMinus;				// �Ŵ� - ����
	CXPButton		m_btnFangPlus;				// �Ŵ� - ����
	CXPButton		m_btnQuanMinus;				// ��Ȧ - ����
	CXPButton		m_btnQuanPlus;				// ��Ȧ - ����

	CXPButton		m_btnSetVPreview;			// ���� - ��ƵԤ��
	CXPButton		m_btnSetVParam;				// ���� - ��Ƶ����
	CXPButton		m_btnSetRecord;				// ���� - ¼��/��ͼ
	CXPButton		m_btnSetReview;				// ���� - �ط�(¼��/��ͼ)
	CXPButton		m_btnSetAlert;				// ���� - ����/����
	CXPButton		m_btnSetCamera;				// ���� - Ӳ������

	friend class CHaoYiView;
};