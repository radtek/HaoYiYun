
#pragma once

#include "OSMutex.h"
#include "OSThread.h"
#include "mp4v2.h"
#include "myRTSPClient.h"

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

	int             m_nVideoFPS;
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

	int             m_nVideoFPS;
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

	int             m_nVideoFPS;
	int				m_nVideoWidth;
	int				m_nVideoHeight;

	string			m_strSPS;				// SPS
	string			m_strPPS;				// PPS
	string			m_strAACHeader;			// AAC
	string			m_strAVCHeader;			// AVC

	LibRtmp	  *		m_lpRtmp;				// rtmp��������...
};
