
#pragma once

#include "HCNetSDK.h"

class CPushThread;
class CRecThread;
class CVideoWnd;
class CCamera
{
public:
	CCamera(CVideoWnd * lpWndParent);
	~CCamera(void);
public:
	string   &  GetDeviceSN() { return m_strDeviceSN; }
	CString  &	GetLogStatus() { return m_strLogStatus; }
public:
	BOOL		IsLogin();		// 这个状态只能用于显示，不能用于状态判断...
	BOOL		IsPlaying();	// 这个可以用于状态的精确判断...
	BOOL		IsRecording();	// 通道是否正处在录像中...
	BOOL		IsPublishing(); // 通道是否正处在发布中...
	BOOL		IsCameraDevice() { return ((m_nStreamProp == kStreamDevice) ? true : false); }
	int			GetRecCourseID() { return m_nRecCourseID; }
	DWORD		GetHKErrCode() { return m_dwHKErrCode; }
	STREAM_PROP	GetStreamProp() { return m_nStreamProp; }

	int			GetRecvPullKbps();
	int			GetSendPushKbps();
	LPCTSTR		GetStreamPushUrl();

	GM_Error	doStreamLogin();
	GM_Error	doStreamLogout();
	void		doStreamStopLivePush();
	void		doStreamStatus(LPCTSTR lpszStatus);

	BOOL		InitCamera(GM_MapData & inMapLoc);
	GM_Error	ForUDPData(GM_MapData & inNetData);

	void		doRecEnd(int nRecSecond);

	void		doStreamSnapJPG(int nRecSecond);
	DWORD		doDeviceSnapJPG(CString & inJpgName);

	DWORD		doPTZCmd(DWORD dwPTZCmd, BOOL bStop);
	DWORD		doLogin(HWND hWndNotify, LPCTSTR lpIPAddr, int nCmdPort, LPCTSTR lpUser, LPCTSTR lpPass);
	void		doLogout();

	void		UpdateWndTitle(STREAM_PROP inPropType, CString & strTitle);
	void		ForLoginAsync(LONG lUserID, DWORD dwResult, LPNET_DVR_DEVICEINFO_V30 lpDeviceInfo);
	DWORD		onLoginSuccess();

	void		doRecStartCourse(int nCourseID);
	void		doRecStopCourse(int nCourseID);

	int			doStartLivePush(string & strRtmpUrl);
	void		doStopLiveMessage();
	void		doStopLivePush();

	void		doWebStatCamera(int nStatus);
private:
	static void CALLBACK LoginResult(LONG lUserID, DWORD dwResult, LPNET_DVR_DEVICEINFO_V30 lpDeviceInfo, void * pUser);
private:
	void		WaitForExit();						// 等待异步登录退出...
	void		ClearResource();					// 释放建立资源...

//	void		StartRecSlice();					// 启动一个录像切片...
//	void		doRecSlice();
private:
	BOOL					m_bStreamLogin;			// 流转发模式下正在异步登录标志...
	STREAM_PROP				m_nStreamProp;			// 通道流类型...
	int						m_nCameraID;			// 摄像头本地编号...
	HWND					m_hWndNotify;			// 消息通知窗口...
	string					m_strDeviceSN;			// 本地摄像头序列号...
	CAMERA_TYPE				m_nCameraType;			// 网络摄像头类型...
	GM_MapData				m_MapNetConfig;			// 摄像头网络配置信息...
	CVideoWnd		*		m_lpWndParent;			// 父窗口对象...
	CString					m_strLogStatus;			// 登录状态栏...

	CRecThread		*		m_lpRecThread;			// 录像线程对象...
	CString					m_strMP4Name;			// MP4文件名(不带扩展名)...
	CString					m_strJpgName;			// JPG文件名(全路径)...
	int						m_nRecCourseID;			// 正在录像的课程编号...

	CPushThread     *		m_lpPushThread;			// 直播上传线程...
	string					m_strRtmpUrl;			// 直播上传地址...

	CString					m_strLoginUser;			// 记录登录用户名称...
	CString					m_strLoginPass;			// 记录登陆用户密码...
	int						m_nRtspPort;			// IPC的RTSP端口号...

	LONG					m_dwHKErrCode;			// 记录海康错误码...
	BOOL					m_bIsExiting;			// 正在等待退出中...
	BOOL					m_HKLoginIng;			// 正在异步登录中...
	LONG					m_HKPlayID;				// 实时播放编号...
	LONG					m_HKLoginID;			// 海康登录编号...
	NET_DVR_DEVICEINFO_V30	m_HKDeviceInfo;			// 海康设备信息...
};
