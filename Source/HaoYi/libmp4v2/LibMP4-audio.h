
#pragma once

#include "mp4v2.h"

typedef struct RTMPFrame {
	string		m_strData;
	bool		m_bKeyFrame;
	uint32_t	m_nTimeStamp;
}RTMPFrame;

typedef	deque<RTMPFrame>	KH_DeqFrame;
class LibMP4
{
public:
	LibMP4(void);
	~LibMP4(void);
public:
	MP4Duration GetDuration();

	DWORD GetWriteSize()  { return m_dwWriteSize; }
	DWORD GetWriteRecMS() { return m_dwWriteRecMS; }
	bool IsVideoCreated() { return ((m_videoID == MP4_INVALID_TRACK_ID) ? false : true); }
	bool IsAudioCreated() { return ((m_audioID == MP4_INVALID_TRACK_ID) ? false : true); }

	bool CreateAudioTrack(const char* lpUTF8Name, int nAudioSampleRate, string & strAES);
	bool CreateVideoTrack(const char* lpUTF8Name, int nVideoTimeScale, int width, int height, string & strSPS, string & strPPS);

	bool WriteSample(bool bIsVideo, BYTE * lpFrame, int nSize, uint32_t inTimeStamp, bool bIsKeyFrame);

	bool Close();
private:
	MP4FileHandle	m_hFileHandle;		// �ļ����
	MP4TrackId		m_videoID;			// ��ƵID
	MP4TrackId		m_audioID;			// ��ƵID
	RTMPFrame		m_VLastFrame;		// ��һ֡��Ƶ����Ƶ�ǹ̶��ģ����ü�¼��һ֡����
	RTMPFrame		m_ALastFrame;		// ��һ֡��Ƶ...
	KH_DeqFrame		m_deqAudio;			// ��Ƶ��������֡
	DWORD			m_dwWriteSize;		// д���ļ�����...
	DWORD			m_dwWriteRecMS;		// �Ѿ�д��ĺ�����...
	DWORD			m_dwFirstStamp;		// ��ʼд�̵ĵ�һ֡ʱ���...
	bool			m_bFirstFrame;		// ��һ֡����д��Ƶ�Ĺؼ�֡,֮ǰ����Ƶ����
	int				m_nVideoTimeScale;	// Ŀǰ�ǹ̶�����ֵ�����ϲ������Ʋ�����
	int				m_nAudioSampleRate;	// ��Ƶ��TimeScale...
};

