
#pragma once

#include "OSMutex.h"
#include "OSThread.h"
#include "mp4v2.h"
#include "myRTSPClient.h"

//#define _SAVE_H264_

class LibRtmp;
class CCamera;
class CPushThread;
class CMP4Thread : public OSThread
{
public:
	CMP4Thread();
	~CMP4Thread();
public:
	BOOL			InitMP4(CPushThread * inPushThread, string & strMP4File);
	int				GetVideoWidth(){ return m_nVideoWidth; }
	int				GetVideoHeight() { return m_nVideoHeight; }
	string	 &		GetAVCHeader() { return m_strAVCHeader; }
	string	 &		GetAACHeader() { return m_strAACHeader; }
	bool			IsFinished()   { return m_bFinished; }

	string   &		GetVideoSPS() { return m_strSPS; }
	string   &      GetVideoPPS() { return m_strPPS; }
	int				GetAudioType(){ return m_audio_type; }
	int				GetAudioRateIndex() { return m_audio_rate_index; }
	int				GetAudioChannelNum() { return m_audio_channel_num; }
private:
	virtual void	Entry();
	bool			doPrepareMP4();
	bool			doMP4ParseAV(MP4FileHandle inFile);
	void			WriteAACSequenceHeader();
	void			WriteAVCSequenceHeader();
	bool			ReadOneFrameFromMP4(MP4TrackId tid, uint32_t sid, bool bIsVideo, uint32_t & outSendTime);
private:
	OSMutex			m_Mutex;
	bool			m_bFileLoop;			// 是否循环
	int				m_nLoopCount;			// 已循环次数
	string			m_strMP4File;			// 文件路径
	CPushThread *	m_lpPushThread;			// 父线程对象

	bool			m_bFinished;			// 文件是否读取完毕
	uint32_t		m_dwMP4Duration;		// 文件的持续时间(毫秒)

	MP4FileHandle	m_hMP4Handle;			// MP4文件句柄
	uint32_t		m_iASampleInx;			// 音频帧当前索引
	uint32_t		m_iVSampleInx;			// 视频帧当前索引
	MP4TrackId		m_tidVideo;				// 视频轨道编号
	MP4TrackId		m_tidAudio;				// 音频轨道编号
	bool			m_bAudioComplete;		// 音频完成标志
	bool			m_bVideoComplete;		// 视频完成标志

	int				m_audio_type;
	int				m_audio_rate_index;
	int				m_audio_channel_num;

	int				m_nVideoWidth;
	int				m_nVideoHeight;

	string			m_strSPS;				// SPS
	string			m_strPPS;				// PPS
	string			m_strAES;				// AES
	string			m_strAACHeader;			// AAC
	string			m_strAVCHeader;			// AVC

	DWORD			m_dwStartMS;			// 启动记录时间点
};

class CRtspThread : public OSThread
{
public:
	CRtspThread();
	~CRtspThread();
public:
	void			StartPushThread();
	void			PushFrame(FMS_FRAME & inFrame);
	BOOL			InitRtsp(CPushThread * inPushTread, string & strRtspUrl);
	int				GetVideoWidth(){ return m_nVideoWidth; }
	int				GetVideoHeight() { return m_nVideoHeight; }
	string	 &		GetAVCHeader() { return m_strAVCHeader; }
	string	 &		GetAACHeader() { return m_strAACHeader; }
	bool			IsFinished()   { return m_bFinished; }
	void			WriteAVCSequenceHeader(string & inSPS, string & inPPS);
	void			WriteAACSequenceHeader(int inAudioRate, int inAudioChannel);
	void			ResetEventLoop() { m_rtspEventLoopWatchVariable = 1; }

	string   &		GetVideoSPS() { return m_strSPS; }
	string   &      GetVideoPPS() { return m_strPPS; }
	int				GetAudioRateIndex() { return m_audio_rate_index; }
	int				GetAudioChannelNum() { return m_audio_channel_num; }
private:
	virtual void	Entry();
private:
	OSMutex			m_Mutex;
	string			m_strRtspUrl;
	CPushThread *	m_lpPushThread;
	
	bool			m_bFinished;			// 网络流是否结束了

	int				m_audio_rate_index;
	int				m_audio_channel_num;

	int				m_nVideoWidth;
	int				m_nVideoHeight;

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
	BOOL			InitRtmp(CPushThread * inPushTread, string & strRtmpUrl);
	int				GetVideoWidth(){ return m_nVideoWidth; }
	int				GetVideoHeight() { return m_nVideoHeight; }
	string	 &		GetAVCHeader() { return m_strAVCHeader; }
	string	 &		GetAACHeader() { return m_strAACHeader; }
	bool			IsFinished()   { return m_bFinished; }
	void			WriteAVCSequenceHeader(string & inSPS, string & inPPS);
	void			WriteAACSequenceHeader(int inAudioRate, int inAudioChannel);

	string   &		GetVideoSPS() { return m_strSPS; }
	string   &      GetVideoPPS() { return m_strPPS; }
	int				GetAudioRateIndex() { return m_audio_rate_index; }
	int				GetAudioChannelNum() { return m_audio_channel_num; }
private:
	virtual void	Entry();
private:
	OSMutex			m_Mutex;
	string			m_strRtmpUrl;
	CPushThread *	m_lpPushThread;

	bool			m_bFinished;			// 网络流是否结束了

	int				m_audio_rate_index;
	int				m_audio_channel_num;

	int				m_nVideoWidth;
	int				m_nVideoHeight;

	string			m_strSPS;				// SPS
	string			m_strPPS;				// PPS
	string			m_strAACHeader;			// AAC
	string			m_strAVCHeader;			// AVC

	LibRtmp	  *		m_lpRtmp;				// rtmp拉流对象...
};

class LibMP4;
class CPushThread : public OSThread
{
public:
	CPushThread(HWND hWndVideo, CCamera * lpCamera);
	~CPushThread();
public:
	static void Initialize();
	static void UnInitialize();
public:
	BOOL			StreamInitThread(BOOL bFileMode, string & strStreamUrl, string & strStreamMP4);
	BOOL			StreamStartLivePush(string & strRtmpUrl);
	BOOL			StreamStopLivePush();
	
	void			SetStreamPlaying(BOOL bFlag);
	void			SetStreamPublish(BOOL bFlag);

	BOOL			StreamBeginRecord();
	BOOL			StreamWriteRecord(FMS_FRAME & inFrame);
	BOOL			StreamEndRecord();

	DWORD			GetStreamRecSize() { return m_dwWriteSize; }
	DWORD			GetStreamRecMS() { return m_dwWriteRecMS; }
	BOOL			IsStreamPlaying() { return m_bStreamPlaying; }
	BOOL			IsPublishing() { return m_bIsPublishing; }
	int				GetSendKbps() { return m_nSendKbps; }
	int				GetRecvKbps();
	BOOL			IsRecording();

	int				PushFrame(FMS_FRAME & inFrame);
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
	void			BeginSendPacket();
	void			EndSendPacket();
	void			dropToKeyFrame();
	void			doErrPushNotify();

	void			doStreamSnapJPG(int nRecSecond);
	void			dropSliceKeyFrame();
	void			doSaveInterFrame();
	BOOL			BeginRecSlice();
	BOOL			EndRecSlice();
	
	BOOL			MP4CreateVideoTrack();
	BOOL			MP4CreateAudioTrack();
private:
	OSMutex			m_Mutex;			// 互斥对象
	string			m_strRtmpUrl;		// 推流地址
	DWORD			m_dwTimeOutMS;		// 超时计时点
	HWND			m_hWndVideo;		// 父窗口对象
	bool			m_bDeleteFlag;		// 已删除标志

	BOOL			m_bIsPublishing;	// 是否处于直播发布状态...
	BOOL			m_bStreamPlaying;	// 是否处于流数据正常状态...

	DWORD			m_dwRecvTimeMS;		// 接收计时时间
	uint32_t		m_nRecvKbps;		// 接收码流
	uint32_t		m_nCurRecvByte;		// 当前已接收字节数
	uint32_t		m_nSendKbps;		// 发送码流
	uint32_t		m_nCurSendByte;		// 当前已发送字节数
	
	CCamera		*	m_lpCamera;			// 摄像头对象...
	CRtmpThread	*	m_lpRtmpThread;		// rtmp线程...
	CRtspThread	*	m_lpRtspThread;		// rtsp线程...
	CMP4Thread  *   m_lpMP4Thread;		// mp4线程...

	LibMP4		*	m_lpRecMP4;			// mp4录像对象...
	DWORD			m_dwRecCTime;		// mp4开始录像时间 => 单位(秒)...
	DWORD			m_dwWriteSize;		// 写入文件长度...
	DWORD			m_dwWriteRecMS;		// 已经写入的毫秒数...
	string			m_strUTF8MP4;		// mp4存盘路径(UTF-8)，带.tmp扩展名...
	CString			m_strMP4Name;		// MP4文件名(不带扩展名)...
	CString			m_strJpgName;		// JPG文件名(全路径)...

	LibRtmp		*	m_lpRtmpPush;		// rtmp发送对象...
	KH_MapFrame		m_MapFrame;			// 按时间排序的帧数据队列 => 所有音视频数据...
	KH_MapFrame		m_MapStream;		// 流转发模式缓存数据区   => 只有一个视频关键帧的数据区间...
	int				m_nKeyFrame;		// 已经缓存的关键帧计数器
	DWORD			m_dwFirstSendTime;	// 第一个数据包的时间戳

	KH_MapFrame		m_MapMonitor;		// 云监控切片交错缓存区...
	int				m_nKeyMonitor;		// 云监控已缓存交错关键帧个数...

#ifdef _SAVE_H264_
	bool			m_bSave_sps;
	FILE		*	m_lpH264File;
#endif

	friend class CCamera;
};
