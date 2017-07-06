
#pragma once

#include "OSMutex.h"
#include "OSThread.h"
#include "..\myRTSPClient.h"

class LibMP4;
class LibRtmp;
class CRecThread : public OSThread
{
public:
	CRecThread();
	virtual ~CRecThread();
public:
	virtual bool	InitThread(int nTaskID, LPCTSTR lpszURL, LPCTSTR lpszPath) = 0;
	virtual void	doErrNotify() = 0;
	virtual	void	Entry() = 0;
public:
	bool			WriteSample(bool bIsVideo, BYTE * lpFrame, int nSize, DWORD inTimeStamp, DWORD inRenderOffset, bool bIsKeyFrame);
	bool			CreateAudioTrack(int nAudioSampleRate, int nAudioChannel);
	bool			CreateVideoTrack(string & strSPS, string & strPPS);
	float			GetRecSizeM();
protected:
	OSMutex			m_Mutex;			// �������
	int				m_nTaskID;			// ¼��������
	string			m_strURL;			// rtmp������ַ
	string			m_strMP4Path;		// mp4����·��(UTF-8)
	HWND			m_hWndParent;		// �����ڶ���
	LibMP4		*	m_lpLibMP4;			// mp4¼�����...
	DWORD			m_dwWriteSize;		// д���ļ�����...
	DWORD			m_dwWriteRecMS;		// �Ѿ�д��ĺ�����...
	DWORD			m_dwRecSliceMS;		// Ԥ���¼����Ƭ������(0��ʾ����Ƭ)...
};

class CRtmpRecThread : public CRecThread
{
public:
	CRtmpRecThread(HWND hWndParent);
	virtual ~CRtmpRecThread();
public:
	virtual bool	InitThread(int nTaskID, LPCTSTR lpszURL, LPCTSTR lpszPath);
	virtual	void	Entry();
	virtual void	doErrNotify();
private:
	LibRtmp		*	m_lpRtmp;			// rtmp��������
};

class CCamera;
class CRtspRecThread : public CRecThread
{
public:
	CRtspRecThread(HWND hWndParent, CCamera * lpCamera, DWORD dwRecSliceMS);
	virtual ~CRtspRecThread();
public:
	virtual bool	InitThread(int nTaskID, LPCTSTR lpszURL, LPCTSTR lpszPath);
	virtual	void	Entry();
	virtual void	doErrNotify();
public:
	void			ResetEventLoop() { m_rtspEventLoopWatchVariable = 1; }
private:
	CCamera * m_lpCamera;					// IPC����
	TaskScheduler * m_scheduler_;			// rtsp��Ҫ���������
	UsageEnvironment * m_env_;				// rtsp��Ҫ�Ļ���
	ourRTSPClient * m_rtspClient_;			// rtsp����
	char m_rtspEventLoopWatchVariable;		// rtsp�˳���־ => �¼�ѭ����־���������Ϳ��Կ��������߳�...
};