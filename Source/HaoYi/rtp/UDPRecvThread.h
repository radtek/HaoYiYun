
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

	void			doProcServerReady(char * lpBuffer, int inRecvLen);
	void			doProcServerHeader(char * lpBuffer, int inRecvLen);
	void			doProcServerReload(char * lpBuffer, int inRecvLen);

	void			doTagDetectProcess(char * lpBuffer, int inRecvLen);
	void			doTagHeaderProcess(char * lpBuffer, int inRecvLen);
	void			doTagAVPackProcess(char * lpBuffer, int inRecvLen);
	void			doEraseLoseSeq(uint32_t inSeqID);
private:
	enum {
		kCmdSendCreate	= 0,				// 开始发送 => 创建命令状态
		kCmdSendReady	= 1,				// 开始发送 => 准备就绪命令状态
		kCmdRecvAVPak	= 2,				// 开始接收 => 音视频数据包接收状态
	} m_nCmdState;							// 命令状态变量...

	string			m_strSPS;				// 视频sps
	string			m_strPPS;				// 视频pps

	OSMutex			m_Mutex;				// 互斥对象
	UDPSocket	*	m_lpUDPSocket;			// UDP对象

	bool			m_bNeedSleep;			// 休息标志 => 只要有发包或收包就不能休息...
	int				m_rtt_ms;				// 网络往返延迟值 => 毫秒
	int				m_rtt_var_ms;			// 网络抖动时间差 => 毫秒

	circlebuf		m_circle;				// 环形队列
	rtp_detect_t	m_rtp_detect;			// RTP探测命令结构体
	rtp_create_t	m_rtp_create;			// RTP创建房间和直播结构体
	rtp_delete_t	m_rtp_delete;			// RTP删除房间和直播结构体
	rtp_supply_t	m_rtp_supply;			// RTP补包命令结构体

	rtp_header_t	m_rtp_header;			// RTP序列头结构体   => 接收 => 来自推流端...
	rtp_ready_t		m_rtp_ready;			// RTP准备继续结构体 => 接收 => 来自推流端...
	rtp_reload_t	m_rtp_reload;			// RTP重建命令结构体 => 接收 => 来自服务器...

	int64_t			m_next_create_ns;		// 下次发送创建命令时间戳 => 纳秒 => 每隔100毫秒发送一次...
	int64_t			m_next_detect_ns;		// 下次发送探测包的时间戳 => 纳秒 => 每隔1秒发送一次...
	int64_t			m_next_ready_ns;		// 下次发送就绪命令时间戳 => 纳秒 => 每隔100毫秒发送一次...

	GM_MapLose		m_MapLose;				// 检测到的丢包集合队列...
};