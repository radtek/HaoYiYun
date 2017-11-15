
#pragma once

#include "OSMutex.h"
#include "OSThread.h"
#include "..\myRTSPClient.h"

class LibMP4;
class CCamera;
class CRecThread : public OSThread
{
public:
	CRecThread();
	virtual ~CRecThread();
public:
	virtual BOOL	InitThread(LPCTSTR lpszRtspUrl) = 0;
public:
	BOOL			IsRecFinished() { return m_bFinished; }
	void			StoreVideoHeader(string & inSPS, string & inPPS);
	void			StoreAudioHeader(int inAudioRate, int inAudioChannel);
	BOOL			StreamBeginRecord();
	BOOL			StreamWriteRecord(FMS_FRAME & inFrame);
	BOOL			StreamEndRecord();
protected:
	BOOL			MP4CreateVideoTrack();
	BOOL			MP4CreateAudioTrack();
	void			doStreamSnapJPG(int nRecSecond);
	void			dropSliceKeyFrame();
	void			doSaveInterFrame();
	BOOL			BeginRecSlice();
	BOOL			EndRecSlice();
protected:
	string			m_strAudioAES;			// 音频扩展缓存
	int				m_audio_sample_rate;	// 音频采样率
	int				m_audio_rate_index;		// 音频索引编号
	int				m_audio_channel_num;	// 音频声道数
	int				m_nVideoWidth;			// 视频宽度
	int				m_nVideoHeight;			// 视频高度
	string			m_strSPS;				// SPS
	string			m_strPPS;				// PPS

	BOOL			m_bFinished;		// 网络流是否结束了
	CCamera		*	m_lpCamera;			// IPC上层对象
	LibMP4		*	m_lpLibMP4;			// mp4录像对象...
	DWORD			m_dwRecCTime;		// mp4开始录像时间 => 单位(秒)...
	DWORD			m_dwWriteSize;		// 写入文件长度...
	DWORD			m_dwWriteRecMS;		// 已经写入的毫秒数...
	string			m_strUTF8MP4;		// mp4存盘路径(UTF-8)，带.tmp扩展名...
	CString			m_strMP4Name;		// MP4文件名(不带扩展名)...
	CString			m_strJpgName;		// JPG文件名(全路径)...

	KH_MapFrame		m_MapMonitor;		// 云监控切片交错缓存区...
	int				m_nKeyMonitor;		// 云监控已缓存交错关键帧个数...
};

class CRtspRecThread : public CRecThread
{
public:
	CRtspRecThread(CCamera * lpCamera);
	virtual ~CRtspRecThread();
public:
	void			ResetEventLoop() { m_rtspEventLoopWatchVariable = 1; }
public:
	virtual BOOL	InitThread(LPCTSTR lpszRtspUrl);
	virtual	void	Entry();
private:
	string	m_strRtspUrl;					// rtsp连接地址
	TaskScheduler * m_scheduler_;			// rtsp需要的任务对象
	UsageEnvironment * m_env_;				// rtsp需要的环境
	ourRTSPClient * m_rtspClient_;			// rtsp对象
	char m_rtspEventLoopWatchVariable;		// rtsp退出标志 => 事件循环标志，控制它就可以控制任务线程...
};