
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
	string			m_strAudioAES;			// ��Ƶ��չ����
	int				m_audio_sample_rate;	// ��Ƶ������
	int				m_audio_rate_index;		// ��Ƶ�������
	int				m_audio_channel_num;	// ��Ƶ������
	int				m_nVideoWidth;			// ��Ƶ���
	int				m_nVideoHeight;			// ��Ƶ�߶�
	string			m_strSPS;				// SPS
	string			m_strPPS;				// PPS

	BOOL			m_bFinished;		// �������Ƿ������
	CCamera		*	m_lpCamera;			// IPC�ϲ����
	LibMP4		*	m_lpLibMP4;			// mp4¼�����...
	DWORD			m_dwRecCTime;		// mp4��ʼ¼��ʱ�� => ��λ(��)...
	DWORD			m_dwWriteSize;		// д���ļ�����...
	DWORD			m_dwWriteRecMS;		// �Ѿ�д��ĺ�����...
	string			m_strUTF8MP4;		// mp4����·��(UTF-8)����.tmp��չ��...
	CString			m_strMP4Name;		// MP4�ļ���(������չ��)...
	CString			m_strJpgName;		// JPG�ļ���(ȫ·��)...

	KH_MapFrame		m_MapMonitor;		// �Ƽ����Ƭ��������...
	int				m_nKeyMonitor;		// �Ƽ���ѻ��潻��ؼ�֡����...
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
	string	m_strRtspUrl;					// rtsp���ӵ�ַ
	TaskScheduler * m_scheduler_;			// rtsp��Ҫ���������
	UsageEnvironment * m_env_;				// rtsp��Ҫ�Ļ���
	ourRTSPClient * m_rtspClient_;			// rtsp����
	char m_rtspEventLoopWatchVariable;		// rtsp�˳���־ => �¼�ѭ����־���������Ϳ��Կ��������߳�...
};