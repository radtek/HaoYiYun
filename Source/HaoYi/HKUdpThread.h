
#pragma once

#include "OSThread.h"
#include "UDPSocket.h"

#define DEF_MCAST_ADDRV4	"239.255.255.250"	// 组播地址
#define	DEF_MCAST_PORT		37020				// 组播端口
#define	DEF_MCAST_STEP		30					// 间隔时间(秒)
#define DEF_MCAST_TIME		5000				// 默认组播事件线程等待时间(毫秒)

class CHaoYiView;
class CHKUdpThread : public OSThread
{
public:
	CHKUdpThread(CHaoYiView * lpView);
	~CHKUdpThread();
public:
	GM_Error		InitMulticast();
private:
	virtual void	Entry();
private:
	GM_Error		ForRead();						// 响应网络读取事件
	GM_Error		SendCmdQuiry();					// 发送查询指令 - 组播命令
	GM_Error		WaitAndProcMulticast();			// 等待并处理组播数据
	GM_Error		ProcessEvent(int eventBits);	// 处理网络事件
private:
	UDPSocket		m_UDPSocket;					// UDP对象
	CHaoYiView	*	m_lpHaoYiView;					// 视图指针
};
