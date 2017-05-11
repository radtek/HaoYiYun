
#pragma once

#include "OSMutex.h"
#include "OSThread.h"
#include "..\myRTSPClient.h"

class LibMP4;
class LibRtmp;
class CRecThread : public OSThread
{
public:
	CRecThread();
	virtual ~CRecThread();
public:
	virtual bool	InitThread(int nTaskID, LPCTSTR lpszURL, LPCTSTR lpszPath) = 0;
	virtual void	doErrNotify() = 0;
	virtual	void	Entry() = 0;
public:
	bool			WriteSample(bool bIsVideo, BYTE * lpFrame, int nSize, DWORD inTimeStamp, bool bIsKeyFrame);
	bool			CreateAudioTrack(int nAudioSampleRate, int nAudioChannel);
	bool			CreateVideoTrack(string & strSPS, string & strPPS);
	float			GetRecSizeM();
protected:
	OSMutex			m_Mutex;			// 互斥对象
	int				m_nTaskID;			// 录像任务编号
	string			m_strURL;			// rtmp拉流地址
	string			m_strMP4Path;		// mp4存盘路径(UTF-8)
	HWND			m_hWndParent;		// 父窗口对象
	LibMP4		*	m_lpLibMP4;			// mp4录像对象...
	DWORD			m_dwWriteSize;		// 写入文件长度...
	DWORD			m_dwWriteRecMS;		// 已经写入的毫秒数...
	DWORD			m_dwRecSliceMS;		// 预算的录像切片毫秒数(0表示不切片)...
};

class CRtmpRecThread : public CRecThread
{
public:
	CRtmpRecThread(HWND hWndParent);
	virtual ~CRtmpRecThread();
public:
	virtual bool	InitThread(int nTaskID, LPCTSTR lpszURL, LPCTSTR lpszPath);
	virtual	void	Entry();
	virtual void	doErrNotify();
private:
	LibRtmp		*	m_lpRtmp;			// rtmp拉流对象
};

class CCamera;
class CRtspRecThread : public CRecThread
{
public:
	CRtspRecThread(HWND hWndParent, CCamera * lpCamera, DWORD dwRecSliceMS);
	virtual ~CRtspRecThread();
public:
	virtual bool	InitThread(int nTaskID, LPCTSTR lpszURL, LPCTSTR lpszPath);
	virtual	void	Entry();
	virtual void	doErrNotify();
public:
	void			ResetEventLoop() { m_rtspEventLoopWatchVariable = 1; }
private:
	CCamera * m_lpCamera;					// IPC对象
	TaskScheduler * m_scheduler_;			// rtsp需要的任务对象
	UsageEnvironment * m_env_;				// rtsp需要的环境
	ourRTSPClient * m_rtspClient_;			// rtsp对象
	char m_rtspEventLoopWatchVariable;		// rtsp退出标志 => 事件循环标志，控制它就可以控制任务线程...
};