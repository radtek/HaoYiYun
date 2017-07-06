
#pragma once

#include "OSMutex.h"
#include "OSThread.h"
#include "mp4v2.h"
#include "myRTSPClient.h"

typedef struct
{
	string		strData;			// ֡����
    int			typeFlvTag;			// FLV_TAG_TYPE_AUDIO or FLV_TAG_TYPE_VIDEO
    bool		is_keyframe;		// �Ƿ��ǹؼ�֡
	uint32_t	dwSendTime;			// ����ʱ��(����)
	uint32_t	dwRenderOffset;		// ʱ���ƫ��ֵ
}FMS_FRAME;

// ���尴ʱ������Ŀ����ظ���֡����(����Ƶ��϶���)...
typedef	multimap<uint32_t, FMS_FRAME>	KH_MapFrame;

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
	uint32_t		m_dwMP4Duration;		// �ļ��ĳ���ʱ��(����)

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
	CPushThread(HWND hWndParent);
	~CPushThread();
public:
	static void Initialize();
	static void UnInitialize();
public:
	BOOL			StreamInitThread(CCamera * lpCamera, BOOL bFileMode, string & strStreamUrl, string & strStreamMP4);
	BOOL			StreamStartLivePush(CCamera * lpCamera, string & strRtmpUrl);
	BOOL			StreamStopLivePush(CCamera * lpCamera);
	void			SetStreamPlaying(BOOL bFlag);
	void			SetStreamPublish(BOOL bFlag);
	BOOL			StreamBeginRecord(LPCTSTR lpszPathMP4);
	BOOL			StreamWriteRecord(FMS_FRAME & inFrame);
	BOOL			StreamEndRecord();

	BOOL			DeviceInitThread(CCamera * lpCamera, CString & strRtspUrl, string & strRtmpUrl);

	DWORD			GetStreamRecSize() { return m_dwWriteSize; }
	DWORD			GetStreamRecMS() { return m_dwWriteRecMS; }
	BOOL			IsStreamPlaying() { return m_bStreamPlaying; }
	BOOL			IsCameraDevice() { return ((m_nStreamProp == kStreamDevice) ? true : false); }
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
	void			doErrNotify();

	BOOL			MP4CreateVideoTrack();
	BOOL			MP4CreateAudioTrack();
private:
	STREAM_PROP		m_nStreamProp;		// ͨ��������...
	OSMutex			m_Mutex;			// �������
	string			m_strRtmpUrl;		// ������ַ
	DWORD			m_dwTimeOutMS;		// ��ʱ��ʱ��
	HWND			m_hWndParent;		// �����ڶ���
	bool			m_bDeleteFlag;		// ��ɾ����־

	BOOL			m_bIsPublishing;	// �Ƿ���ֱ������״̬...
	BOOL			m_bStreamPlaying;	// �Ƿ�������������״̬...

	DWORD			m_dwRecvTimeMS;		// ���ռ�ʱʱ��
	uint32_t		m_nRecvKbps;		// ��������
	uint32_t		m_nCurRecvByte;		// ��ǰ�ѽ����ֽ���
	uint32_t		m_nSendKbps;		// ��������
	uint32_t		m_nCurSendByte;		// ��ǰ�ѷ����ֽ���
	
	CCamera		*	m_lpCamera;			// ����ͷ����...
	CRtmpThread	*	m_lpRtmpThread;		// rtmp�߳�...
	CRtspThread	*	m_lpRtspThread;		// rtsp�߳�...
	CMP4Thread  *   m_lpMP4Thread;		// mp4�߳�...

	LibMP4		*	m_lpRecMP4;			// mp4¼�����...
	string			m_strUTF8MP4;		// mp4����·��(UTF-8)
	DWORD			m_dwWriteSize;		// д���ļ�����...
	DWORD			m_dwWriteRecMS;		// �Ѿ�д��ĺ�����...

	LibRtmp		*	m_lpRtmpPush;		// rtmp���Ͷ���...
	KH_MapFrame		m_MapFrame;			// ��ʱ�������֡���ݶ���
	KH_MapFrame		m_MapStream;		// ��ת��ģʽ����������
	int				m_nKeyFrame;		// �Ѿ�����Ĺؼ�֡������
	DWORD			m_dwFirstSendTime;	// ��һ�����ݰ���ʱ���

	friend class CCamera;
};
