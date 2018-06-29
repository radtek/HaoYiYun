
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

	CUDPSendThread  * m_lpUDPSendThread;// UDP�����߳�...
	CUDPRecvThread  * m_lpUDPRecvThread;// UDP�����߳�...

	CPlaySDL        * m_lpPlaySDL;      // SDL���Ź�����...

#ifdef _SAVE_H264_
	bool			m_bSave_sps;
	FILE		*	m_lpH264File;
#endif

	friend class CCamera;
};
