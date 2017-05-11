
#pragma once

#include "OSMutex.h"
#include "OSThread.h"
#include "mp4v2.h"
#include "myRTSPClient.h"

typedef struct
{
    int			typeFlvTag;			// FLV_TAG_TYPE_AUDIO or FLV_TAG_TYPE_VIDEO
    bool		is_keyframe;		// 是否是关键帧
	uint32_t	dwSendTime;			// 发送时间(毫秒)
	string		strData;			// 帧数据
}FMS_FRAME;

// 定义按时间排序的可以重复的帧队列(音视频混合队列)...
typedef	multimap<uint32_t, FMS_FRAME>	KH_MapFrame;

class LibRtmp;
class CCamera;
class CPushThread;
class CRtspThread : public OSThread
{
public:
	CRtspThread();
	~CRtspThread();
public:
	void			StartPushThread();
	void			PushFrame(FMS_FRAME & inFrame);
	BOOL			InitRtsp(CPushThread * inPushTread, CString & strRtspUrl);
	string	 &		GetAVCHeader() { return m_strAVCHeader; }
	string	 &		GetAACHeader() { return m_strAACHeader; }
	bool			IsFinished()   { return m_bFinished; }
	void			WriteAVCSequenceHeader(string & inSPS, string & inPPS);
	void			WriteAACSequenceHeader(int inAudioRate, int inAudioChannel);
	void			ResetEventLoop() { m_rtspEventLoopWatchVariable = 1; }
private:
	virtual void	Entry();
private:
	OSMutex			m_Mutex;
	string			m_strRtspUrl;
	CPushThread *	m_lpPushThread;
	
	bool			m_bFinished;			// 网络流是否结束了

	int				m_audio_rate_index;
	int				m_audio_channel_num;

	string			m_strSPS;				// SPS
	string			m_strPPS;				// PPS
	string			m_strAACHeader;			// AAC
	string			m_strAVCHeader;			// AVC

	TaskScheduler * m_scheduler_;			// rtsp需要的任务对象
	UsageEnvironment * m_env_;				// rtsp需要的环境
	ourRTSPClient * m_rtspClient_;			// rtsp对象
	char m_rtspEventLoopWatchVariable;		// rtsp退出标志 => 事件循环标志，控制它就可以控制任务线程...
};

class CRtmpThread : public OSThread
{
public:
	CRtmpThread();
	~CRtmpThread();
public:
	void			StartPushThread();
	int				PushFrame(FMS_FRAME & inFrame);
	BOOL			InitRtmp(CPushThread * inPushTread);
	string	 &		GetAVCHeader() { return m_strAVCHeader; }
	string	 &		GetAACHeader() { return m_strAACHeader; }
	bool			IsFinished()   { return m_bFinished; }
	void			WriteAVCSequenceHeader(string & inSPS, string & inPPS);
	void			WriteAACSequenceHeader(int inAudioRate, int inAudioChannel);
private:
	virtual void	Entry();
private:
	OSMutex			m_Mutex;
	string			m_strRtmpUrl;
	CPushThread *	m_lpPushThread;
	
	bool			m_bFinished;			// 网络流是否结束了

	int				m_audio_rate_index;
	int				m_audio_channel_num;

	string			m_strSPS;				// SPS
	string			m_strPPS;				// PPS
	string			m_strAACHeader;			// AAC
	string			m_strAVCHeader;			// AVC

	LibRtmp	  *		m_lpRtmp;				// rtmp拉流对象...
};

class CPushThread : public OSThread
{
public:
	CPushThread(HWND hWndParent);
	~CPushThread();
public:
	BOOL			InitThread(CCamera * lpCamera, CString & strRtspUrl, string & strRtmpUrl);
	int				PushFrame(FMS_FRAME & inFrame);
	int				GetSendKbps() { return m_nSendKbps; }
	void			doErrNotify();
private:
	virtual	void	Entry();

	BOOL			OpenRtmpUrl();
	int				SendOneDataPacket();
    BOOL			SendVideoDataPacket(FMS_FRAME & inFrame);
    BOOL			SendAudioDataPacket(FMS_FRAME & inFrame);
	BOOL			SendAACSequenceHeaderPacket();
	BOOL			SendAVCSequenceHeaderPacket();
	char		*	WriteMetadata(char * buf);
	int				SendMetadataPacket();

	bool			IsDataFinished();
	BOOL			IsFrameTimeout();
private:
	OSMutex			m_Mutex;
	bool			m_bFileMode;
	string			m_strRtmpUrl;
	uint32_t		m_nSendKbps;		// 发送码流
	uint32_t		m_nCurSendByte;		// 当前已发送字节数
	DWORD			m_dwTimeOutMS;		// 超时计时点
	HWND			m_hWndParent;		// 父窗口对象
	bool			m_bDeleteFlag;		// 已删除标志
	
	CCamera		*	m_lpCamera;			// 摄像头对象...
	CRtmpThread	*	m_lpRtmpThread;		// rtmp线程...
	CRtspThread	*	m_lpRtspThread;		// rtsp线程...

	LibRtmp		*	m_lpRtmp;			// rtmp发送对象...
	KH_MapFrame		m_MapFrame;			// 按时间排序的帧数据队列

	friend class CCamera;
};
