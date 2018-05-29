
#pragma once

class CWebThread;
class CDlgRoom : public CDialogEx
{
	DECLARE_DYNAMIC(CDlgRoom)
public:
	CDlgRoom(CWnd* pParent = NULL);   // 标准构造函数
	virtual ~CDlgRoom();
	enum { IDD = IDD_BIND_ROOM };
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持
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
		kStateInit		= 0,		// 初始化
		kStateWait		= 1,		// 正在处理...
		kStateError		= 2,		// 发生错误...
		kStateFinish	= 3,		// 执行完毕...
	} m_nState;
	enum {
		kTimerLiveRoom	= 1,		// 获取直播间时钟...
	};
	enum {
		kRoomID			= 0,		// 直播间号码
		kRoomLesson		= 1,		// 直播间课程
		kRoomTeacher	= 2,		// 直播间老师
		kRoomStartTime	= 3,		// 直播开始时间
		kRoomEndTime	= 4,		// 直播结束时间
	};
	enum {
		kImageWait		= 0,		// 等待中
		kImageCheck		= 1,		// 已选中
	};
	int				m_nSelRoomID;	// 当前选中直播间号
	CListCtrl		m_ListRoom;		// 直播间列表对象
	CImageList		m_RoomImage;	// 直播间图标对象
	CWebThread  *   m_lpRoomThread;	// 交互线程对象

	friend class CHaoYiView;
};
