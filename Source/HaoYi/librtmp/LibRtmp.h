
#pragma once

#include "rtmp.h"
#include "log.h"

class CRtmpRecThread;
class CRtmpThread;
class LibRtmp
{
public:
    LibRtmp(bool isNeedLog, bool isNeedPush, CRtmpThread * lpPullThread, CRtmpRecThread * lpRecThread);

    ~LibRtmp();

    bool Open(const char* url);
	bool IsClosed();

    void Close();

	bool Read();
    bool Send(const char* buf, int bufLen, int type, unsigned int timestamp);

    void SendSetChunkSize(unsigned int chunkSize);

    void CreateSharedObject();

    void SetSharedObject(const std::string& objName, bool isSet);

    void SendSharedObject(const std::string& objName, int val);
private:
	bool doAudio(RTMPPacket *packet);
	bool doVideo(RTMPPacket *packet);
	bool doInvoke(RTMPPacket *packet);
	void ParseAACSequence(char * inBuf, int nSize);
	void ParseAVCSequence(char * inBuf, int nSize);
private:
    RTMP	*	m_rtmp_;				// rtmp对象
    char	*	m_streming_url_;		// 流地址
    FILE	*	m_flog_;				// 日志句柄
    bool		m_is_need_push_;		// 是否推送
    string		m_stream_name_;			// 流名称

	string		m_strAAC;				// for audio
	string		m_strAVC;				// for video
	string		m_strAES;				// for audio
	string		m_strSPS;				// SPS...
	string		m_strPPS;				// PPS...

	BOOL			  m_bPushStartOK;	// 是否已经启动线程...
	CRtmpThread	   *  m_lpRtmpThread;	// 外部线程对象...
	CRtmpRecThread *  m_lpRecThread;	// 外部录像线程对象...
};
