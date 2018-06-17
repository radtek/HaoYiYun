
#pragma once

#include "OSMutex.h"
#include "OSThread.h"
#include "circlebuf.h"
#include "rtp.h"

class UDPSocket;
class CUDPRecvThread : public OSThread
{
public:
	CUDPRecvThread(int nDBRoomID, int nDBCameraID);
	virtual ~CUDPRecvThread();
	virtual void Entry();
public:
	GM_Error		InitThread();
private:
	void			CloseSocket();
	void			doSendCreateCmd();
	void			doSendDeleteCmd();
	void			doSendDetectCmd();
	void			doSendReadyCmd();
	void			doRecvPacket();
	void			doSleepTo();
	void			doTagDetectProcess(char * lpBuffer, int inRecvLen);
	void			doTagHeaderProcess(char * lpBuffer, int inRecvLen);
	void			doTagAudioProcess(char * lpBuffer, int inRecvLen); 
	void			doTagVideoProcess(char * lpBuffer, int inRecvLen); 
private:
	string			m_strSPS;				// 视频sps
	string			m_strPPS;				// 视频pps

	OSMutex			m_Mutex;				// 互斥对象
	UDPSocket	*	m_lpUDPSocket;			// UDP对象

	bool			m_bNeedSleep;			// 休息标志 => 只要有发包或收包就不能休息...
	bool			m_bSendReady;			// 是否要发送准备就绪命令标志...

	circlebuf		m_circle;				// 环形队列
	rtp_detect_t	m_rtp_detect;			// RTP探测命令结构体
	rtp_create_t	m_rtp_create;			// RTP创建房间和直播结构体
	rtp_delete_t	m_rtp_delete;			// RTP删除房间和直播结构体
	rtp_header_t	m_rtp_header;			// RTP序列头结构体
	rtp_ready_t		m_rtp_ready;			// RTP准备继续结构体

	int64_t			m_next_create_ns;		// 下次发送创建命令时间戳 => 纳秒 => 每隔100毫秒发送一次...
	int64_t			m_next_detect_ns;		// 下次发送探测包的时间戳 => 纳秒 => 每隔1秒发送一次...
	int64_t			m_next_ready_ns;		// 下次发送就绪命令时间戳 => 纳秒 => 每隔100毫秒发送一次...
};