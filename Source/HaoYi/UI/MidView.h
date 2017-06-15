
#pragma once

#include "VideoWnd.h"

class CBitItem;
class CHaoYiView;

typedef	map<int, CVideoWnd*>	GM_MapVideo;

class CMidView : public CWnd
{
public:
	CMidView(CHaoYiView * lpParent);
	virtual ~CMidView();
protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	DECLARE_MESSAGE_MAP()
public:
	void			OnAuthResult(int nType, BOOL bAuthOK);
	CFont    *		GetVideoFont() { return &m_VideoFont; }
	BOOL			IsFullScreen() { return m_bIsFullScreen; }
	BOOL			Create(UINT wStyle, const CRect & rect, CWnd * pParentWnd);
public:
	void			ReleaseFocus();						// 释放焦点...
	void			BuildVideoByXml();					// 通过配置创建窗口...
	void			doDelDVR(int nCameraID);			// 删除指定摄像头...
	void			doLeftFocus(int nCameraID);			// 左侧点击...
	void			doMoveWindow(CRect & rcArea);		// 移动窗口...
	void			LayoutVideoWnd(int cx, int cy);		// 排列窗口...
	int				GetNextAutoID(int nCurCameraID);	// 得到下一个窗口编号...
	BOOL			ChangeFullScreen(BOOL bFullScreen);	// 设置全屏模式...
	CCamera     *	FindCameraByID(int nCameraID);
	CCamera		*	FindCameraBySN(string & inDeviceSN);
	GM_Error		AddNewCamera(GM_MapData & inNetData, CAMERA_TYPE inType);
	GM_Error		doCameraUDPData(GM_MapData & inNetData, CAMERA_TYPE inType);
	BOOL			doWebStatCamera(int nDBCamera, int nStatus);
private:
	void			FixVideoWnd();						// 锁定窗口...
	void			BuildResource();					// 创建资源...
	void			DestroyResource();					// 销毁资源...
	void			DrawNotice(CDC * pDC);				// 打印警告...
	void			DrawBackLine(CDC * pDC);			// 绘制线框...
	CCamera		*	BuildXmlCamera(GM_MapData & inXmlData);
private:
	CRect			m_rcOrigin;							// 原始矩形区
	BOOL			m_bIsFullScreen;					// 全屏标志
	CString			m_strNotice;						// 提示信息

	CBitItem	 *	m_BitArray[CVideoWnd::kBitNum];		// 按钮列表...
	CHaoYiView	 *	m_lpParentDlg;						// 父窗口对象...
	CFont			m_VideoFont;						// 字体对象...

	GM_MapVideo		m_MapVideo;							// 视频窗口集合 => VIndex => CVideoWnd
};
