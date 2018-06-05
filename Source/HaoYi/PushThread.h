
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

class CRenderWnd;
class CPushThread;
class CVideoThread : public OSThread
{
public:
	CVideoThread(string & inSPS, string & inPPS, int nWidth, int nHeight);
	~CVideoThread();
public:
	BOOL	InitThread(CPushThread * inPushThread, CRenderWnd * lpRenderWnd);
	void	PushFrame(string & inFrame, bool bIsKeyFrame);
private:
	virtual void	Entry();
	int     DecodeFrame();
	void    DisplaySDL();
private:
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
	int             m_out_buffer_size;

	uint8_t		 *  m_out_buffer;
	SwrContext   *  m_au_convert_ctx;
	SDL_AudioDeviceID m_AudioDevID;

	AVCodecParserContext * m_lpSrcCodecParserCtx;
    AVCodecContext * m_lpSrcCodecCtx;
	AVCodec * m_lpSrcCodec;
	AVFrame	* m_lpSrcFrame;

	Uint8  * m_audio_chunk; 
	Uint32   m_audio_len; 
	Uint8  * m_audio_pos; 

};

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
	bool			m_bFileLoop;			// �Ƿ�ѭ��
	int				m_nLoopCount;			// ��ѭ������
	string			m_strMP4File;			// �ļ�·��
	CPushThread *	m_lpPushThread;			// ���̶߳���

	bool			m_bFinished;			// �ļ��Ƿ��ȡ���
	uint32_t		m_dwMP4Duration;		// �ļ����ܺ����� = (�̶ܿ���/ÿ��̶���)*1000

	MP4FileHandle	m_hMP4Handle;			// MP4�ļ����
	uint32_t		m_iASampleInx;			// ��Ƶ֡��ǰ����
	uint32_t		m_iVSampleInx;			// ��Ƶ֡��ǰ����
	MP4TrackId		m_tidVideo;				// ��Ƶ������
	MP4TrackId		m_tidAudio;				// ��Ƶ������
	bool			m_bAudioComplete;		// ��Ƶ��ɱ�־
	bool			m_bVideoComplete;		// ��Ƶ��ɱ�־

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

	DWORD			m_dwStartMS;			// ������¼ʱ���
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
	
	bool			m_bFinished;			// �������Ƿ������

	int				m_audio_rate_index;
	int				m_audio_channel_num;

	int				m_nVideoWidth;
	int				m_nVideoHeight;

	string			m_strSPS;				// SPS
	string			m_strPPS;				// PPS
	string			m_strAACHeader;			// AAC
	string			m_strAVCHeader;			// AVC

	TaskScheduler * m_scheduler_;			// rtsp��Ҫ���������
	UsageEnvironment * m_env_;				// rtsp��Ҫ�Ļ���
	ourRTSPClient * m_rtspClient_;			// rtsp����
	char m_rtspEventLoopWatchVariable;		// rtsp�˳���־ => �¼�ѭ����־���������Ϳ��Կ��������߳�...
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

	bool			m_bFinished;			// �������Ƿ������

	int				m_audio_rate_index;
	int				m_audio_channel_num;

	int				m_nVideoWidth;
	int				m_nVideoHeight;

	string			m_strSPS;				// SPS
	string			m_strPPS;				// PPS
	string			m_strAACHeader;			// AAC
	string			m_strAVCHeader;			// AVC

	LibRtmp	  *		m_lpRtmp;				// rtmp��������...
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
	void			StartAudioThread(int nRateIndex, int nChannelNum);
	void			StartVideoThread(string & inSPS, string & inPPS, int nWidth, int nHeight);
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
	OSMutex			m_Mutex;			// �������
	string			m_strRtmpUrl;		// ������ַ
	DWORD			m_dwTimeOutMS;		// ��ʱ��ʱ��
	HWND			m_hWndVideo;		// �����ڶ���
	bool			m_bDeleteFlag;		// ��ɾ����־

	BOOL			m_bIsPublishing;	// �Ƿ���ֱ������״̬...
	BOOL			m_bStreamPlaying;	// �Ƿ�������������״̬...

	string			m_strSnapFrame;		// �Ѿ���ŵĽ�ͼ��Ƶ������ => �ؼ�֡��ͷ���ۼӣ�ֱ���¹ؼ�֡����...
	DWORD			m_dwSnapTimeMS;		// ͨ����ͼʱ�� => ��λ�����룩=> ��������ؼ�֡��գ�Ŀǰ��ŵ���һ��...

	DWORD			m_dwRecvTimeMS;		// ���ռ�ʱʱ�� => ��λ�����룩
	uint32_t		m_nRecvKbps;		// ��������
	uint32_t		m_nCurRecvByte;		// ��ǰ�ѽ����ֽ���
	uint32_t		m_nSendKbps;		// ��������
	uint32_t		m_nCurSendByte;		// ��ǰ�ѷ����ֽ���
	
	CCamera		*	m_lpCamera;			// ����ͷ����...
	CRtmpThread	*	m_lpRtmpThread;		// rtmp�߳�...
	CRtspThread	*	m_lpRtspThread;		// rtsp�߳�...
	CMP4Thread  *   m_lpMP4Thread;		// mp4�߳�...

	LibMP4		*	m_lpRecMP4;			// mp4¼�����...
	DWORD			m_dwRecCTime;		// mp4��ʼ¼��ʱ�� => ��λ(��)...
	DWORD			m_dwWriteSize;		// д���ļ�����...
	DWORD			m_dwWriteSec;		// �Ѿ�д�������...
	string			m_strUTF8MP4;		// mp4����·��(UTF-8)����.tmp��չ��...
	CString			m_strMP4Name;		// MP4�ļ���(������չ��)...
	CString			m_strJpgName;		// JPG�ļ���(ȫ·��)...

	CString			m_strSliceID;		// ��ƬΨһ��־...
	int				m_nSliceInx;		// ��Ƭ�������...

	LibRtmp		*	m_lpRtmpPush;		// rtmp���Ͷ���...
	KH_MapFrame		m_MapFrame;			// ��ʱ�������֡���ݶ��� => ��������Ƶ����...
	KH_MapFrame		m_MapStream;		// ��ת��ģʽ����������   => ֻ��һ����Ƶ�ؼ�֡����������...
	int				m_nKeyFrame;		// �Ѿ�����Ĺؼ�֡������
	DWORD			m_dwFirstSendTime;	// ��һ�����ݰ���ʱ���

	KH_MapFrame		m_MapMonitor;		// �Ƽ����Ƭ��������...
	int				m_nKeyMonitor;		// �Ƽ���ѻ��潻��ؼ�֡����...

	CVideoThread  * m_lpVideoThread;	// ��Ƶ�����߳�...
	CAudioThread  * m_lpAudioThread;    // ��Ƶ�����߳�...

#ifdef _SAVE_H264_
	bool			m_bSave_sps;
	FILE		*	m_lpH264File;
#endif

	friend class CCamera;
};
