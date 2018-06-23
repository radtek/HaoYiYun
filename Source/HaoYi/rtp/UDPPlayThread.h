
#pragma once

#include "OSMutex.h"
#include "OSThread.h"

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavutil/error.h"
#include "libavutil/opt.h"
#include "SDL2/SDL.h"
};

typedef	map<int64_t, AVPacket>		GM_MapPacket;	// DTS => AVPacket  => 解码前的数据帧 => 毫秒 => 1/1000
typedef map<int64_t, AVFrame*>		GM_MapFrame;	// PTS => AVFrame   => 解码后的视频帧 => 毫秒 => 1/1000
typedef map<int64_t, string>		GM_MapAudio;	// PTS => string    => 解码后的音频帧 => 毫秒 => 1/1000

// 第三版：音视频线程分开播放...
class CRenderWnd;
class CPlaySDL;
class CDecoder
{
public:
	CDecoder();
	~CDecoder();
public:
	void		doSleepTo();
	void		doPushPacket(AVPacket & inPacket);
	int			GetMapPacketSize() { return m_MapPacket.size(); }
protected:
	AVCodec         *   m_lpCodec;			// 解码器...
	AVFrame         *   m_lpDFrame;			// 解码结构体...
	AVCodecContext  *   m_lpDecoder;		// 解码器描述...
	GM_MapPacket		m_MapPacket;		// 解码前的数据帧...
	GM_MapFrame			m_MapFrame;			// 解码后的数据帧....
	int64_t				m_play_next_ns;		// 下一个要播放帧的系统纳秒值...
	bool				m_bNeedSleep;		// 休息标志 => 只要有解码或播放就不能休息...
};

class CVideoThread : public CDecoder, public OSThread
{
public:
	CVideoThread(CPlaySDL * lpPlaySDL);
	virtual ~CVideoThread();
	virtual void Entry();
public:
	BOOL	InitVideo(CRenderWnd * lpRenderWnd, string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS);
	void	doFillPacket(string & inData, int inPTS, bool bIsKeyFrame, int inOffset);
private:
	void	doDecodeFrame();
	void	doDisplaySDL();
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

	OSMutex			m_Mutex;
	CPlaySDL	 *  m_lpPlaySDL;
};

class CAudioThread : public CDecoder, public OSThread
{
public:
	CAudioThread(CPlaySDL * lpPlaySDL);
	virtual ~CAudioThread();
	virtual void Entry();
public:
	BOOL	InitAudio(int nRateIndex, int nChannelNum);
	void	doFillPacket(string & inData, int inPTS, bool bIsKeyFrame, int inOffset);
private:
	void	doDecodeFrame();
	void	doDisplaySDL();
private:
	int					m_audio_rate_index;
	int					m_audio_channel_num;
	int				    m_audio_sample_rate;
	int					m_nSampleDuration;

	uint8_t		 *		m_out_buffer;
	int					m_out_buffer_size;
	SwrContext   *		m_au_convert_ctx;

	GM_MapAudio			m_MapAudio;
	SDL_AudioDeviceID	m_nDeviceID;

	OSMutex				m_Mutex;
	CPlaySDL	 *		m_lpPlaySDL;
};

class CPlaySDL
{
public:
	CPlaySDL();
	~CPlaySDL();
public:
	void		PushFrame(string & inData, int inTypeTag, bool bIsKeyFrame, uint32_t inSendTime);
	BOOL		InitVideo(CRenderWnd * lpRenderWnd, string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS);
	BOOL		InitAudio(int nRateIndex, int nChannelNum);
	int64_t		GetSysZeroNS() { return m_sys_zero_ns; }
	int64_t		GetStartPtsMS() { return m_start_pts_ms; }
private:
	int64_t				m_sys_zero_ns;		// 系统计时零点 => 启动时间戳 => 纳秒...
	int64_t				m_start_pts_ms;		// 第一帧的PTS时间戳计时起点 => 毫秒...

	CVideoThread    *   m_lpVideoThread;	// 视频线程...
	CAudioThread    *   m_lpAudioThread;	// 音频线程...
};

// 第二版：音视频线程合并播放...
/*class CPlayThread;
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
};*/

// 第一版：音视频线程分开播放...
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
