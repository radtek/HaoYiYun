
#pragma once

typedef void* srs_rtmp_t;
class CRtmpThread;
class LibRtmp
{
public:
    LibRtmp(bool isNeedLog, bool isNeedPush, CRtmpThread * lpPullThread);
    ~LibRtmp();

    bool Open(const char* url);
	bool IsClosed();

    void Close();

	bool Read();
    bool Send(const char* data, int size, int type, unsigned int timestamp);
private:
	bool doAudio(DWORD dwTimeStamp, char * lpData, int nSize);
	bool doVideo(DWORD dwTimeStamp, char * lpData, int nSize);
	bool doScript(DWORD dwTimeStamp, char * lpData, int nSize);
	bool doInvoke(DWORD dwTimeStamp, char * lpData, int nSize);

	void ParseAACSequence(char * inBuf, int nSize);
	void ParseAVCSequence(char * inBuf, int nSize);
private:
	srs_rtmp_t	m_lpSRSRtmp;			// SRS��rtmp����...
    bool		m_is_need_push_;		// �Ƿ�����
    string		m_streming_url_;		// ����ַ
    string		m_stream_name_;			// ������

	string		m_strAAC;				// for audio
	string		m_strAVC;				// for video
	string		m_strAES;				// for audio
	string		m_strSPS;				// SPS...
	string		m_strPPS;				// PPS...

	BOOL			  m_bPushStartOK;	// �Ƿ��Ѿ������߳�...
	CRtmpThread	   *  m_lpRtmpThread;	// �ⲿ�̶߳���...
};
