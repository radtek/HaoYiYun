
#pragma once

#include "OSMutex.h"
#include "OSThread.h"
#include "mp4v2.h"
#include "myRTSPClient.h"

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavutil/opt.h"
#include "SDL2/SDL.h"
};

//#define _SAVE_H264_

typedef	map<int64_t, AVPacket>	GM_MapPacket;	// DTS => AVPacket => 解码前的数据帧 => 毫秒 => 1/1000
typedef map<int64_t, AVFrame*>  GM_MapFrame;	// PTS => AVFrame  => 解码后的视频帧 => 纳秒 => 1/1000000000
typedef map<int64_t, string>    GM_MapAudio;	// PTS => string   => 解码后的音频帧 => 纳秒 => 1/1000000000

class CPushThread;
class CPlayThread;
class CRenderWnd;
class CDecoder
{
public:
	CDecoder();
	~CDecoder();
public:
	void		doPushPacket(AVPacket & inPacket);
	int			GetMapPacketSize() { return m_MapPacket.size(); }
protected:
	AVCodec         *	m_lpCodec;
	AVCodecContext  *	m_lpDecoder;
	GM_MapPacket		m_MapPacket;
	GM_MapFrame			m_MapFrame;
};

class CVideoDecoder : public CDecoder
{
public:
	CVideoDecoder();
	~CVideoDecoder();
public:
	BOOL	InitVideo(CRenderWnd * lpRenderWnd, string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS);
	void	doFillPacket(string & inData, int inPTS, bool bIsKeyFrame, int inOffset);
	void	doDecodeFrame(int64_t inSysZeroNS);
	void	doDisplaySDL();
	int64_t	doGetMinNextNS();
private:
	int				m_nFPS;
	int				m_nWidth;
	int				m_nHeight;
	string			m_strSPS;
	string			m_strPPS;
	CRenderWnd	 *	m_lpRenderWnd;
	SDL_Window   *  m_sdlScreen;
	SDL_Renderer *  m_sdlRenderer;
    SDL_Texture  *  m_sdlTexture;
};

class CAudioDecoder : public CDecoder
{
public:
	CAudioDecoder(CPlayThread * lpPlayThread);
	~CAudioDecoder();
public:
	BOOL	InitAudio(int nRateIndex, int nChannelNum);
	void	doFillAudio(Uint8 * inStream, int inLen);
	void	doDecodeFrame(int64_t inSysZeroNS);
	void	doFillPacket(string & inData, int inPTS, bool bIsKeyFrame, int inOffset);
	int64_t	doGetMinNextNS();
private:
	int				m_audio_rate_index;
	int				m_audio_channel_num;
	int             m_audio_sample_rate;

	uint8_t		 *  m_out_buffer;
	int             m_out_buffer_size;
	SwrContext   *  m_au_convert_ctx;

	GM_MapAudio		m_MapAudio;
	CPlayThread  *  m_lpPlayThread;
};

class CPlayThread : public OSThread
{
public:
	CPlayThread(CPushThread * inPushThread);
	~CPlayThread();
public:
	BOOL	InitVideo(CRenderWnd * lpRenderWnd, string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS);
	BOOL	InitAudio(int nRateIndex, int nChannelNum);
	void	PushFrame(FMS_FRAME & inFrame);
public:
	static  void    do_fill_audio(void * udata, Uint8 *stream, int inLen);
private:
	virtual void	Entry();
	void			doFillAudio(Uint8 * inStream, int inLen);
	void			doDecodeVideo();
	void			doDecodeAudio();
	void			doDisplaySDL();
	void			doCalcNextNS();
	void			doSleepTo();
private:
	OSMutex				m_Mutex;
	CPushThread		*	m_lpPushThread;
	CAudioDecoder	*	m_AudioDecoder;
	CVideoDecoder	*	m_VideoDecoder;

	int64_t				m_play_next_ns;		// 下一个要播放帧的系统纳秒值...
	int64_t				m_play_sys_ts;		// 系统计时零点 => 启动时间戳...
	int64_t				m_start_pts;		// 第一帧的PTS时间戳计时起点...
};

/*class CVideoThread : public OSThread
{
public:
	CVideoThread(string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS);
	~CVideoThread();
public:
	BOOL	InitThread(CPushThread * inPushThread, CRenderWnd * lpRenderWnd);
	void	PushFrame(string & inFrame, bool bIsKeyFrame);
private:
	virtual void	Entry();
	int     DecodeFrame();
	void    DisplaySDL();
private:
	int				m_nFPS;
	int				m_nWidth;
	int				m_nHeight;
	OSMutex			m_Mutex;
	string			m_strSPS;
	string			m_strPPS;
	string			m_strFrame;
	CRenderWnd	 *	m_lpRenderWnd;
	CPushThread  *  m_lpPushThread;
	SDL_Window   *  m_sdlScreen;
	SDL_Renderer *  m_sdlRenderer;
    SDL_Texture  *  m_sdlTexture;

	AVCodecParserContext * m_lpSrcCodecParserCtx;
    AVCodecContext * m_lpSrcCodecCtx;
	AVCodec * m_lpSrcCodec;
	AVFrame	* m_lpSrcFrame;
};

class CAudioThread : public OSThread
{
public:
	CAudioThread(int nRateIndex, int nChannelNum);
	~CAudioThread();
public:
	BOOL	InitThread(CPushThread * inPushThread);
	void    DisplaySDL(Uint8 * inStream, int inLen);
	void	PushFrame(string & inFrame);
private:
	virtual void	Entry();
	int     DecodeFrame();
private:
	OSMutex			m_Mutex;
	string			m_strFrame;
	CPushThread  *  m_lpPushThread;
	int				m_audio_rate_index;
	int				m_audio_channel_num;
	int             m_audio_sample_rate;

	string			m_strPCM;
	uint8_t		 *  m_out_buffer;
	SwrContext   *  m_au_convert_ctx;
	int             m_out_buffer_size;

	AVCodecParserContext * m_lpSrcCodecParserCtx;
    AVCodecContext * m_lpSrcCodecCtx;
	AVCodec * m_lpSrcCodec;
	AVFrame	* m_lpSrcFrame;
};*/

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
	uint32_t		m_dwMP4Duration;		// 文件的总毫秒数 = (总刻度数/每秒刻度数)*1000

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

	int             m_nVideoFPS;
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
	BOOL			InitRtsp(BOOL bUsingTCP, CPushThread * inPushTread, string & strRtspUrl);
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

	int             m_nVideoFPS;
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

	int             m_nVideoFPS;
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
	BOOL			StreamInitThread(BOOL bFileMode, BOOL bUsingTCP, string & strStreamUrl, string & strStreamMP4);
	BOOL			StreamStartLivePush(string & strRtmpUrl);
	BOOL			StreamStopLivePush();
	
	void			SetStreamPlaying(BOOL bFlag);
	void			SetStreamPublish(BOOL bFlag);

	BOOL			StreamBeginRecord();
	BOOL			StreamWriteRecord(FMS_FRAME & inFrame);
	BOOL			StreamEndRecord();

	DWORD			GetStreamRecSize() { return m_dwWriteSize; }
	DWORD			GetStreamRecSec() { return m_dwWriteSec; }
	BOOL			IsStreamPlaying() { return m_bStreamPlaying; }
	BOOL			IsPublishing() { return m_bIsPublishing; }
	int				GetSendKbps() { return m_nSendKbps; }
	int				GetRecvKbps();
	BOOL			IsRecording();

	int				PushFrame(FMS_FRAME & inFrame);
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

	//CVideoThread  * m_lpVideoThread;	// 视频播放线程...
	//CAudioThread  * m_lpAudioThread;  // 音频播放线程...
	CPlayThread   *  m_lpPlayThread;	// 播放线程...

#ifdef _SAVE_H264_
	bool			m_bSave_sps;
	FILE		*	m_lpH264File;
#endif

	friend class CCamera;
};
