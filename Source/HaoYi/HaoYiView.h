
#pragma once

#include "resource.h"
#include "OSMutex.h"
#include "QRStatic.h"
#include "RightView.h"

class CCamera;
class CMidView;
class CHaoYiDoc;
class CWebThread;
class CFastThread;
class CHKUdpThread;
class CFastSession;
class CRemoteSession;
class CTrackerSession;
class CStorageSession;

typedef	list<GM_MapData>	GM_ListData;

class CHaoYiView : public CFormView
{
	DECLARE_DYNCREATE(CHaoYiView)
protected:
	CHaoYiView();
	virtual ~CHaoYiView();
public:
	enum { IDD = IDD_HYVIEW_DLG };
public:
	CHaoYiDoc* GetDocument() const;
public:
	virtual void OnDraw(CDC* pDC);
	virtual void OnInitialUpdate();
	virtual void DoDataExchange(CDataExchange * pDX);
	virtual BOOL PreCreateWindow(CREATESTRUCT & cs);
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif
protected:
	afx_msg void OnVKFull();
	afx_msg void OnVKEscape();
	afx_msg void OnDVRSet();
	afx_msg void OnSysSet();
	afx_msg void OnLoginDVR();
	afx_msg void OnLogoutDVR();
	afx_msg void OnDestroy();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnCmdUpdateSetDVR(CCmdUI *pCmdUI);
	afx_msg void OnCmdUpdateLoginDVR(CCmdUI *pCmdUI);
	afx_msg void OnCmdUpdateLogoutDVR(CCmdUI *pCmdUI);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnRclickTreeDevice(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnSelchangedTreeDevice(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnKeydownTreeDevice(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg LRESULT	OnMsgFindHKCamera(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT	OnMsgFocusVideo(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT	OnMsgEventSession(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnMsgWebLoadResource(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()
public:
	enum {
		kAddCourse		= 1,							// 添加课表
		kModCourse		= 2,							// 修改课表
		kDelCourse		= 3,							// 删除课表
	};
	enum {
		kNoneRepeat		= 0,							// 无
		kDayRepeat		= 1,							// 每天
		kWeekRepeat		= 2,							// 每周
	};
	enum {
		kTimerCheckFDFS   = 1,							// 每隔5秒单独检测...
		kTimerCheckDVR	  = 2,							// 每隔2秒轮询检测 => 跟实际配置的DVR数量有关...
		kTimerAnimateDVR  = 3,							// 每隔1秒显示DVR动画...
		kTimerCheckCourse = 4,							// 每隔半秒检测课程表...
	};
	enum {
		kSplitterWidth	= 3,							// 拖拉条宽度...
	};
public:
	DWORD			GetCurSendKbps();					// 得到当前发送码流Kbps...
	LPCTSTR			GetCurSendFile();					// 得到当前发送文件名称...
	void			DrawSplitBar(LPRECT lpRect);		// 绘制分隔符...
	void			OnLeftSplitEnd(int nOffset);		// 拖拽分隔符停止...
	BOOL			IsLeftCanSplit(CPoint & point);		// 是否还能拖动...

	void			OnRightSplitEnd(int nOffset);		// 拖拽分隔符停止...
	BOOL			IsRightCanSplit(CPoint & point);	// 是否还能拖动...

	CCamera		*	FindCameraByID(int nCameraID);		// 查找摄像头对象...
	void			OnCreateCamera(int nCameraID, CString & strTitle);
	void			doCameraUDPData(GM_MapData & inNetData, CAMERA_TYPE inType);
	BOOL			doWebRegCamera(GM_MapData & inData);// 向网站注册摄像头...

	void			doChangeAutoDVR(DWORD dwErrCode);
	int				GetCameraStatusByDBID(int nDBCameraID);
	void			UpdateFocusTitle(int nLocalID, CString & strTitle);
	void			doCourseChanged(int nOperateID, int nLocalID, GM_MapData & inData);
	GM_Error		doTransmitPlayer(int nPlayerSock, string & strRtmpUrl, GM_Error inErr);
public:
	CMidView    *	GetMidView()     { return m_lpMidView; }
	CTreeCtrl   &	GetTreeCtrl()	 { return m_DeviceTree; }
	CRightView  &	GetRightView()	 { return m_RightView; }
	int				GetFocusCamera() { return m_nFocusCamera; }
private:
	BOOL			GetMacIPAddr();
	void			BuildResource();					// 创建资源...
	void			DestroyResource();					// 销毁资源...

	void			ClearFastThreads();					// 删除所有的fastthread...
	void			ClearFastSession();					// 删除所有的fastsession...

	GM_Error		DelByEventThread(CFastSession * lpSession);
	GM_Error		AddToEventThread(CFastSession * lpSession);
	void			OnOptDelSession(ULONG inUniqueID);

	void			doRecStartCourse(int nCameraID, int nCourseID);
	void			doRecStopCourse(int nCameraID, int nCourseID);

	void			doCheckCourse();
	void			doAnimateDVR();
	void			doCheckFDFS();
	void			doCheckDVR();

	void			doCheckTracker();
	void			doCheckStorage();
	void			doCheckTransmit();

	BOOL			IsHasUploadFile();					// 是否有需要上传文件...
private:
	CString			m_strMacAddr;						// 本机MAC地址...
	CString			m_strIPAddr;						// 本机IP地址...
	CWebThread  *   m_lpWebThread;						// 网站相关线程...

	BOOL			m_bLeftDraging;						// Flag for Middle Split-Bar...
	CRect			m_rcSplitLeftNew;					// 分隔符的新矩形区...
	CRect			m_rcSplitLeftSrc;					// 分隔符的原矩形区...

	BOOL			m_bRightDraging;					// Flag for Middle Split-Bar...
	CRect			m_rcSplitRightNew;					// 分隔符的新矩形区...
	CRect			m_rcSplitRightSrc;					// 分隔符的原矩形区...

	HCURSOR			m_hCurHorSplit;						// 分隔符需要的水平鼠标...

	CTreeCtrl		m_DeviceTree;						// 设备树对象...
	CImageList		m_ImageList;						// 设备树图标...
	HTREEITEM		m_hRootItem;						// 根节点...
	BOOL			m_bTreeKeydown;						// 树的键盘事件...
	int				m_nAnimateIndex;					// 设备动画索引...

	CQRStatic		m_QRStatic;							// 二维码显示区...
	CRightView		m_RightView;						// 右侧显示区...
	CMidView	*	m_lpMidView;						// 视频窗口管理器...
	int				m_nFocusCamera;						// 视频焦点窗口编号...

	OSMutex			m_Mutex;							// 互斥对象
	GM_ListData		m_HKListData;						// 海康网络数据队列...
	CHKUdpThread *	m_lpHKUdpThread;					// 海康自动搜索线程(不隔15发送UDP命令)...

	CFastThread      *  m_lpFastThread;					// fastdfs会话管理线程...
	CTrackerSession  *  m_lpTrackerSession;				// 链接Tracker，获取Storage的配置信息...
	CStorageSession  *  m_lpStorageSession;				// 链接Storage，发送上传指令，发送上传文件，接收反馈信息...
	CRemoteSession   *  m_lpRemoteSession;				// 链接命令中转服务器，获取微信或网站发出的远程操作指令...

	friend class CWebThread;
	friend class CRemoteSession;
};

#ifndef _DEBUG  // HaoYiView.cpp 中的调试版本
inline CHaoYiDoc* CHaoYiView::GetDocument() const
   { return reinterpret_cast<CHaoYiDoc*>(m_pDocument); }
#endif
