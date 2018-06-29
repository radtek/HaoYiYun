
#pragma once

#include "OSMutex.h"
#include "OSThread.h"

//#define _SAVE_H264_

class CPlaySDL;
class LibMP4;
class LibRtmp;
class CCamera;
class CRenderWnd;
class CMP4Thread;
class CRtmpThread;
class CRtspThread;
class CUDPSendThread;
class CUDPRecvThread;
class CPushThread : public OSThread
{
public:
	CPushThread(HWND hWndVideo, CCamera * lpCamera);
	~CPushThread();
public:
	static void Initialize();
	static void UnInitialize();
public:
	BOOL			StreamInitThread(BOOL bFileMode, BOOL bUsingTCP, string & strStreamUrl, string & strStreamMP4);
	BOOL			StreamStartLivePush(string & strRtmpUrl);
	BOOL			StreamStopLivePush();
	
	void			SetStreamPlaying(BOOL bFlag);
	void			SetStreamPublish(BOOL bFlag);

	BOOL			StreamBeginRecord();
	BOOL			StreamWriteRecord(FMS_FRAME & inFrame);
	BOOL			StreamEndRecord();

	CRenderWnd  *   GetRenderWnd();

	DWORD			GetStreamRecSize() { return m_dwWriteSize; }
	DWORD			GetStreamRecSec() { return m_dwWriteSec; }
	BOOL			IsStreamPlaying() { return m_bStreamPlaying; }
	BOOL			IsPublishing() { return m_bIsPublishing; }
	int				GetSendKbps() { return m_nSendKbps; }
	int				GetRecvKbps();
	BOOL			IsRecording();

	int				PushFrame(FMS_FRAME & inFrame);

	void			StartUDPRecvThread();
	void			StartUDPSendThread();
private:
	void			StartSendByAudio(int nRateIndex, int nChannelNum);
	void			StartSendByVideo(string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS);
	void			StartPlayByAudio(int nRateIndex, int nChannelNum);
	void			StartPlayByVideo(string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS);
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

	void			doFFmpegSnapJPG();
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

	string			m_strSnapFrame;		// 已经存放的截图视频缓冲区 => 关键帧开头，累加，直到新关键帧出现...
	DWORD			m_dwSnapTimeMS;		// 通道截图时间 => 单位（毫秒）=> 存放两个关键帧最保险，目前存放的是一个...

	DWORD			m_dwRecvTimeMS;		// 接收计时时间 => 单位（毫秒）
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
	DWORD			m_dwWriteSec;		// 已经写入的秒数...
	string			m_strUTF8MP4;		// mp4存盘路径(UTF-8)，带.tmp扩展名...
	CString			m_strMP4Name;		// MP4文件名(不带扩展名)...
	CString			m_strJpgName;		// JPG文件名(全路径)...

	CString			m_strSliceID;		// 切片唯一标志...
	int				m_nSliceInx;		// 切片索引编号...

	LibRtmp		*	m_lpRtmpPush;		// rtmp发送对象...
	KH_MapFrame		m_MapFrame;			// 按时间排序的帧数据队列 => 所有音视频数据...
	KH_MapFrame		m_MapStream;		// 流转发模式缓存数据区   => 只有一个视频关键帧的数据区间...
	int				m_nKeyFrame;		// 已经缓存的关键帧计数器
	DWORD			m_dwFirstSendTime;	// 第一个数据包的时间戳

	KH_MapFrame		m_MapMonitor;		// 云监控切片交错缓存区...
	int				m_nKeyMonitor;		// 云监控已缓存交错关键帧个数...

	CUDPSendThread  * m_lpUDPSendThread;// UDP发送线程...
	CUDPRecvThread  * m_lpUDPRecvThread;// UDP接收线程...

	CPlaySDL        * m_lpPlaySDL;      // SDL播放管理器...

#ifdef _SAVE_H264_
	bool			m_bSave_sps;
	FILE		*	m_lpH264File;
#endif

	friend class CCamera;
};
