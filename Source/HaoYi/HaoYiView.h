
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
	afx_msg void OnAddDVR();
	afx_msg void OnModDVR();
	afx_msg void OnDelDVR();
	afx_msg void OnVKFull();
	afx_msg void OnVKEscape();
	afx_msg void OnSysSet();
	afx_msg void OnLoginDVR();
	afx_msg void OnLogoutDVR();
	afx_msg void OnDestroy();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnCmdUpdateAddDVR(CCmdUI *pCmdUI);
	afx_msg void OnCmdUpdateModDVR(CCmdUI *pCmdUI);
	afx_msg void OnCmdUpdateDelDVR(CCmdUI *pCmdUI);
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
	afx_msg LRESULT OnMsgWebAuthResult(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnMsgReloadView(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnMsgDelByTransmit(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnMsgAddByTransmit(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnMsgSysConfig(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()
public:
	enum {
		kAddCourse		= 1,							// ��ӿα�
		kModCourse		= 2,							// �޸Ŀα�
		kDelCourse		= 3,							// ɾ���α�
	};
	enum {
		kNoneRepeat		= 0,							// ��
		kDayRepeat		= 1,							// ÿ��
		kWeekRepeat		= 2,							// ÿ��
	};
	enum {
		kTimerCheckFDFS		  = 1,						// ÿ��5�뵥�����...
		kTimerCheckDVR		  = 2,						// ÿ��2����ѯ��� => ��ʵ�����õ�DVR�����й�...
		kTimerAnimateDVR	  = 3,						// ÿ��1����ʾDVR����...
		kTimerCheckCourse	  = 4,						// ÿ��������γ̱�...
	};
	enum {
		kSplitterWidth	= 3,							// ���������...
	};
public:
	void			doGatherLogout();					// ����ɼ����˳��¼�...
	void			doUpdateFrameTitle();				// ���������ڱ�������...
	DWORD			GetCurSendKbps();					// �õ���ǰ��������Kbps...
	LPCTSTR			GetCurSendFile();					// �õ���ǰ�����ļ�����...
	void			DrawSplitBar(LPRECT lpRect);		// ���Ʒָ���...
	void			OnLeftSplitEnd(int nOffset);		// ��ק�ָ���ֹͣ...
	BOOL			IsLeftCanSplit(CPoint & point);		// �Ƿ����϶�...

	void			OnRightSplitEnd(int nOffset);		// ��ק�ָ���ֹͣ...
	BOOL			IsRightCanSplit(CPoint & point);	// �Ƿ����϶�...

	CCamera		*	FindDBCameraByID(int nDBCameraID);	// ��������ͷ����...
	void			OnCreateCamera(int nDBCameraID, CString & strTitle);
	void			doMulticastData(GM_MapData & inNetData, CAMERA_TYPE inType);
	BOOL			doWebRegCamera(GM_MapData & inData);// ����վע������ͷ...
	BOOL			doWebDelCamera(string & inDeviceSN);// ����վɾ������ͷ...
	BOOL			doWebStatCamera(int nDBCamera, int nStatus, int nErrCode = 0, LPCTSTR lpszErrMsg = NULL);

	int				GetNextAutoID(int nCurDBCameraID);	// �õ���һ�����ڱ��...
	int				GetDBCameraStatusByID(int nDBCameraID);
	void			UpdateFocusTitle(int nDBCameraID, CString & strTitle);
	void			doCourseChanged(int nOperateID, int nDBCameraID, GM_MapData & inData);
public:
	CMidView    *	GetMidView()     { return m_lpMidView; }
	CTreeCtrl   &	GetTreeCtrl()	 { return m_DeviceTree; }
	CRightView  &	GetRightView()	 { return m_RightView; }
	int				GetFocusDBCamera() { return m_nFocusDBCamera; }
private:
	BOOL			GetMacIPAddr();
	void			BuildResource();					// ������Դ...
	void			DestroyResource();					// ������Դ...

	void			ClearFastThreads();					// ɾ�����е�fastthread...
	void			ClearFastSession();					// ɾ�����е�fastsession...

	GM_Error		DelByEventThread(CFastSession * lpSession);
	GM_Error		AddToEventThread(CFastSession * lpSession);
	void			OnOptDelSession(ULONG inUniqueID);
	void			doDelTreeFocus(int nDBCameraID);
	void			doSysConfigChanged();

	void			doRecStartCourse(int nDBCameraID, int nCourseID);
	void			doRecStopCourse(int nDBCameraID, int nCourseID);

	void			doCheckCourse();
	void			doAnimateDVR();
	void			doCheckFDFS();
	void			doCheckDVR();

	void			doCheckTracker();
	void			doCheckStorage();
	void			doCheckTransmit();

	BOOL			IsHasUploadFile();					// �Ƿ�����Ҫ�ϴ��ļ�...
	time_t			GetTimeSecond(time_t inTime);		// ��ȡֻ��ʱ�����������������...
private:
	CString			m_strMacAddr;						// ����MAC��ַ...
	CString			m_strIPAddr;						// ����IP��ַ...
	CWebThread  *   m_lpWebThread;						// ��վ����߳�...

	BOOL			m_bLeftDraging;						// Flag for Middle Split-Bar...
	CRect			m_rcSplitLeftNew;					// �ָ������¾�����...
	CRect			m_rcSplitLeftSrc;					// �ָ�����ԭ������...

	BOOL			m_bRightDraging;					// Flag for Middle Split-Bar...
	CRect			m_rcSplitRightNew;					// �ָ������¾�����...
	CRect			m_rcSplitRightSrc;					// �ָ�����ԭ������...

	HCURSOR			m_hCurHorSplit;						// �ָ�����Ҫ��ˮƽ���...

	CTreeCtrl		m_DeviceTree;						// �豸������...
	CImageList		m_ImageList;						// �豸��ͼ��...
	HTREEITEM		m_hRootItem;						// ���ڵ�...
	BOOL			m_bTreeKeydown;						// ���ļ����¼�...
	int				m_nAnimateIndex;					// �豸��������...

	CQRStatic		m_QRStatic;							// ��ά����ʾ��...
	CRightView		m_RightView;						// �Ҳ���ʾ��...
	CMidView	*	m_lpMidView;						// ��Ƶ���ڹ�����...
	int				m_nFocusDBCamera;					// ��Ƶ���㴰�ڱ��...

	OSMutex			m_Mutex;							// �������
	GM_ListData		m_HKListData;						// �����������ݶ���...
	CHKUdpThread *	m_lpHKUdpThread;					// �����Զ������߳�(����15����UDP����)...

	CFastThread      *  m_lpFastThread;					// fastdfs�Ự�����߳�...
	CTrackerSession  *  m_lpTrackerSession;				// ����Tracker����ȡStorage��������Ϣ...
	CStorageSession  *  m_lpStorageSession;				// ����Storage�������ϴ�ָ������ϴ��ļ������շ�����Ϣ...
	CRemoteSession   *  m_lpRemoteSession;				// ����������ת����������ȡ΢�Ż���վ������Զ�̲���ָ��...

	friend class CMainFrame;
	friend class CWebThread;
	friend class CRemoteSession;
};

#ifndef _DEBUG  // HaoYiView.cpp �еĵ��԰汾
inline CHaoYiDoc* CHaoYiView::GetDocument() const
   { return reinterpret_cast<CHaoYiDoc*>(m_pDocument); }
#endif
