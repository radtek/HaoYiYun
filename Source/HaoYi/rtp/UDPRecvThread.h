
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
	void			doSendSupplyCmd();
	void			doSendDetectCmd();
	void			doSendReadyCmd();
	void			doRecvPacket();
	void			doSleepTo();

	void			doTagReloadProcess(char * lpBuffer, int inRecvLen);
	void			doTagDetectProcess(char * lpBuffer, int inRecvLen);
	void			doTagHeaderProcess(char * lpBuffer, int inRecvLen);
	void			doTagAVPackProcess(char * lpBuffer, int inRecvLen);
	void			doEraseLoseSeq(uint32_t inSeqID);
private:
	string			m_strSPS;				// 视频sps
	string			m_strPPS;				// 视频pps

	OSMutex			m_Mutex;				// 互斥对象
	UDPSocket	*	m_lpUDPSocket;			// UDP对象

	bool			m_bNeedSleep;			// 休息标志 => 只要有发包或收包就不能休息...
	bool			m_bSendReady;			// 是否要发送准备就绪命令标志...
	int				m_rtt_ms;				// 网络往返延迟值 => 毫秒
	int				m_rtt_var;				// 网络抖动时间差 => 毫秒

	circlebuf		m_circle;				// 环形队列
	rtp_detect_t	m_rtp_detect;			// RTP探测命令结构体
	rtp_create_t	m_rtp_create;			// RTP创建房间和直播结构体
	rtp_delete_t	m_rtp_delete;			// RTP删除房间和直播结构体
	rtp_supply_t	m_rtp_supply;			// RTP补包命令结构体
	rtp_header_t	m_rtp_header;			// RTP序列头结构体
	rtp_ready_t		m_rtp_ready;			// RTP准备继续结构体
	rtp_reload_t	m_rtp_reload;			// RTP重建命令结构体

	int64_t			m_next_create_ns;		// 下次发送创建命令时间戳 => 纳秒 => 每隔100毫秒发送一次...
	int64_t			m_next_detect_ns;		// 下次发送探测包的时间戳 => 纳秒 => 每隔1秒发送一次...
	int64_t			m_next_ready_ns;		// 下次发送就绪命令时间戳 => 纳秒 => 每隔100毫秒发送一次...

	GM_MapLose		m_MapLose;				// 检测到的丢包集合队列...
};