
#pragma once

class CWebThread;
class CDlgRoom : public CDialogEx
{
	DECLARE_DYNAMIC(CDlgRoom)
public:
	CDlgRoom(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CDlgRoom();
	enum { IDD = IDD_BIND_ROOM };
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��
	virtual BOOL OnInitDialog();
	afx_msg void OnBtnClickedOk();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnDblclkListRoom(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnItemchangedListRoom(NMHDR *pNMHDR, LRESULT *pResult);
	DECLARE_MESSAGE_MAP()
private:
	void ShowText(CDC * pDC, CString & inTxtData);
private:
	enum {
		kStateInit		= 0,		// ��ʼ��
		kStateWait		= 1,		// ���ڴ���...
		kStateError		= 2,		// ��������...
		kStateFinish	= 3,		// ִ�����...
	} m_nState;
	enum {
		kTimerLiveRoom	= 1,		// ��ȡֱ����ʱ��...
	};
	enum {
		kRoomID			= 0,		// ֱ�������
		kRoomLesson		= 1,		// ֱ����γ�
		kRoomTeacher	= 2,		// ֱ������ʦ
		kRoomStartTime	= 3,		// ֱ����ʼʱ��
		kRoomEndTime	= 4,		// ֱ������ʱ��
	};
	enum {
		kImageWait		= 0,		// �ȴ���
		kImageCheck		= 1,		// ��ѡ��
	};
	int				m_nSelRoomID;	// ��ǰѡ��ֱ�����
	CListCtrl		m_ListRoom;		// ֱ�����б����
	CImageList		m_RoomImage;	// ֱ����ͼ�����
	CWebThread  *   m_lpRoomThread;	// �����̶߳���

	friend class CHaoYiView;
};
