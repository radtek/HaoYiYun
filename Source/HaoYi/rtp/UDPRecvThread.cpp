
#include "stdafx.h"
#include "UtilTool.h"
#include "UDPSocket.h"
#include "UDPRecvThread.h"

CUDPRecvThread::CUDPRecvThread(int nDBRoomID, int nDBCameraID)
  : m_lpUDPSocket(NULL)
  , m_next_create_ns(-1)
  , m_next_detect_ns(-1)
  , m_next_ready_ns(-1)
  , m_bNeedSleep(false)
  , m_bSendReady(true)
  , m_rtt_var(-1)
  , m_rtt_ms(-1)
{
	// 初始化rtp序列头结构体...
	memset(&m_rtp_reload, 0, sizeof(m_rtp_reload));
	memset(&m_rtp_detect, 0, sizeof(m_rtp_detect));
	memset(&m_rtp_create, 0, sizeof(m_rtp_create));
	memset(&m_rtp_delete, 0, sizeof(m_rtp_delete));
	memset(&m_rtp_header, 0, sizeof(m_rtp_header));
	memset(&m_rtp_ready, 0, sizeof(m_rtp_ready));
	// 初始化环形队列，预分配空间...
	circlebuf_init(&m_circle);
	circlebuf_reserve(&m_circle, 512*1024);
	//////////////////////////////////////////////////////////////////////////////////////
	// 注意：这里暂时模拟讲师端观看者身份 => 本身应该为学生观看者...
	//////////////////////////////////////////////////////////////////////////////////////
	// 设置终端类型和结构体类型 => m_rtp_header => 等待网络填充...
	m_rtp_detect.tm = m_rtp_create.tm = m_rtp_delete.tm = m_rtp_ready.tm = m_rtp_supply.tm = TM_TAG_TEACHER; // TM_TAG_STUDENT
	m_rtp_detect.id = m_rtp_create.id = m_rtp_delete.id = m_rtp_ready.id = m_rtp_supply.id = ID_TAG_LOOKER;
	m_rtp_detect.pt = PT_TAG_DETECT;
	m_rtp_create.pt = PT_TAG_CREATE;
	m_rtp_delete.pt = PT_TAG_DELETE;
	m_rtp_supply.pt = PT_TAG_SUPPLY;
	m_rtp_ready.pt  = PT_TAG_READY;
	// 填充房间号和直播通道号...
	m_rtp_create.roomID = nDBRoomID;
	m_rtp_create.liveID = nDBCameraID;
	m_rtp_delete.roomID = nDBRoomID;
	m_rtp_delete.liveID = nDBCameraID;
}

CUDPRecvThread::~CUDPRecvThread()
{
	// 停止线程，等待退出...
	this->StopAndWaitForThread();
	// 关闭UDPSocket对象...
	this->CloseSocket();
	// 释放环形队列空间...
	circlebuf_free(&m_circle);
}

void CUDPRecvThread::CloseSocket()
{
	if( m_lpUDPSocket != NULL ) {
		m_lpUDPSocket->Close();
		delete m_lpUDPSocket;
		m_lpUDPSocket = NULL;
	}
}

GM_Error CUDPRecvThread::InitThread()
{
	// 首先，关闭socket...
	this->CloseSocket();
	// 再新建socket...
	ASSERT( m_lpUDPSocket == NULL );
	m_lpUDPSocket = new UDPSocket();
	///////////////////////////////////////////////////////
	// 注意：Open默认已经设定为异步模式...
	///////////////////////////////////////////////////////
	// 建立UDP,发送音视频数据,接收指令...
	GM_Error theErr = GM_NoErr;
	theErr = m_lpUDPSocket->Open();
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
		return theErr;
	}
	// 设置重复使用端口...
	m_lpUDPSocket->ReuseAddr();
	// 设置发送和接收缓冲区...
	m_lpUDPSocket->SetSocketSendBufSize(128 * 1024);
	m_lpUDPSocket->SetSocketRecvBufSize(128 * 1024);
	// 设置TTL网络穿越数值...
	m_lpUDPSocket->SetTtl(32);
	// 获取服务器地址信息 => 假设输入信息就是一个IPV4域名...
	LPCTSTR lpszAddr = "192.168.1.70"; //DEF_UDP_HOME;
	hostent * lpHost = gethostbyname(lpszAddr);
	if( lpHost != NULL && lpHost->h_addr_list != NULL ) {
		lpszAddr = inet_ntoa(*(in_addr*)lpHost->h_addr_list[0]);
	}
	// 保存服务器地址，简化SendTo参数......
	m_lpUDPSocket->SetRemoteAddr(lpszAddr, DEF_UDP_PORT);
	// 启动组播接收线程...
	this->Start();
	// 返回执行结果...
	return theErr;
}

void CUDPRecvThread::Entry()
{
	while( !this->IsStopRequested() ) {
		// 设置休息标志 => 只要有发包或收包就不能休息...
		m_bNeedSleep = true;
		// 发送创建房间和直播通道命令包...
		this->doSendCreateCmd();
		// 发送探测命令包...
		this->doSendDetectCmd();
		// 发送准备就绪命令包...
		this->doSendReadyCmd();
		// 接收一个到达的服务器反馈包...
		this->doRecvPacket();
		// 发送补包命令...
		this->doSendSupplyCmd();
		// 等待发送或接收下一个数据包...
		this->doSleepTo();
	}
	// 只发送一次删除命令包...
	this->doSendDeleteCmd();
}

void CUDPRecvThread::doSendDeleteCmd()
{
	OSMutexLocker theLock(&m_Mutex);
	GM_Error theErr = GM_NoErr;
	if( m_lpUDPSocket == NULL )
		return;
	// 套接字有效，直接发送删除命令...
	ASSERT( m_lpUDPSocket != NULL );
	theErr = m_lpUDPSocket->SendTo((void*)&m_rtp_delete, sizeof(m_rtp_delete));
	// 打印已发送删除命令包...
	TRACE("[Teacher-Looker] Send Delete RoomID: %lu, LiveID: %d\n", m_rtp_delete.roomID, m_rtp_delete.liveID);
	return;
}

void CUDPRecvThread::doSendCreateCmd()
{
	OSMutexLocker theLock(&m_Mutex);
	// 如果已经收到正确的序列头 => 序列头 | 推流者 | 学生端...
	if( m_rtp_header.pt == PT_TAG_HEADER && m_rtp_header.id == ID_TAG_PUSHER && m_rtp_header.tm == TM_TAG_STUDENT )
		return;
	// 每隔100毫秒发送创建命令包 => 必须转换成有符号...
	int64_t cur_time_ns = CUtilTool::os_gettime_ns();
	int64_t period_ns = 100000000;
	// 如果发包时间还没到，直接返回...
	if( m_next_create_ns > cur_time_ns )
		return;
	ASSERT( cur_time_ns >= m_next_create_ns );
	// 发送一个创建房间命令 => 相当于登录注册...
	GM_Error theErr = m_lpUDPSocket->SendTo((void*)&m_rtp_create, sizeof(m_rtp_create));
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
		return;
	}
	// 打印已发送创建命令包...
	TRACE("[Teacher-Looker] Send Create RoomID: %lu, LiveID: %d\n", m_rtp_create.roomID, m_rtp_create.liveID);
	// 计算下次发送创建命令的时间戳...
	m_next_create_ns = CUtilTool::os_gettime_ns() + period_ns;
	// 修改休息状态 => 已经有发包，不能休息...
	m_bNeedSleep = false;
}

void CUDPRecvThread::doSendDetectCmd()
{
	OSMutexLocker theLock(&m_Mutex);
	// 每隔1秒发送一个探测命令包 => 必须转换成有符号...
	int64_t cur_time_ns = CUtilTool::os_gettime_ns();
	int64_t period_ns = 1000000000;
	// 如果发包时间还没到，直接返回...
	if( m_next_detect_ns > cur_time_ns )
		return;
	ASSERT( cur_time_ns >= m_next_detect_ns );
	// 将探测起点时间转换成毫秒，累加探测计数器...
	m_rtp_detect.tsSrc  = (uint32_t)(cur_time_ns / 1000000);
	m_rtp_detect.dtNum += 1;
	// 调用接口发送探测命令包...
	GM_Error theErr = m_lpUDPSocket->SendTo((void*)&m_rtp_detect, sizeof(m_rtp_detect));
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
		return;
	}
	// 计算下次发送探测命令的时间戳...
	m_next_detect_ns = CUtilTool::os_gettime_ns() + period_ns;
	// 修改休息状态 => 已经有发包，不能休息...
	m_bNeedSleep = false;
}

void CUDPRecvThread::doSendReadyCmd()
{
	OSMutexLocker theLock(&m_Mutex);
	// 还没有收到序列头命令，直接返回 => 序列头 | 推流者 | 学生端 ...
	if( m_rtp_header.pt != PT_TAG_HEADER || m_rtp_header.id != ID_TAG_PUSHER || m_rtp_header.tm != TM_TAG_STUDENT  )
		return;
	ASSERT( m_rtp_header.pt == PT_TAG_HEADER && m_rtp_header.id == ID_TAG_PUSHER && m_rtp_header.tm == TM_TAG_STUDENT );
	// 如果已经收到了音视频数据包，直接返回...
	if( !m_bSendReady )
		return;
	// 每隔100毫秒发送就绪命令包 => 必须转换成有符号...
	int64_t cur_time_ns = CUtilTool::os_gettime_ns();
	int64_t period_ns = 100000000;
	// 如果发包时间还没到，直接返回...
	if( m_next_ready_ns > cur_time_ns )
		return;
	ASSERT( cur_time_ns >= m_next_ready_ns );
	// 发送一个准备就绪命令包 => 通知学生端开始发送音视频数据包...
	GM_Error theErr = m_lpUDPSocket->SendTo((void*)&m_rtp_ready, sizeof(m_rtp_ready));
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
		return;
	}
	// 打印已发送准备就绪命令包...
	TRACE("[Teacher-Looker] Send Ready command\n");
	// 计算下次发送创建命令的时间戳...
	m_next_ready_ns = CUtilTool::os_gettime_ns() + period_ns;
	// 修改休息状态 => 已经有发包，不能休息...
	m_bNeedSleep = false;
}

void CUDPRecvThread::doSendSupplyCmd()
{
	OSMutexLocker theLock(&m_Mutex);
	// 如果丢包集合队列为空，直接返回...
	if( m_MapLose.size() <= 0 )
		return;
	ASSERT( m_MapLose.size() > 0 );
	// 定义最大的补包缓冲区...
	const int nHeadSize = sizeof(m_rtp_supply);
	const int nPackSize = DEF_MTU_SIZE + nHeadSize;
	static char szPacket[nPackSize] = {0};
	char * lpData = szPacket + nHeadSize;
	// 获取当前时间的毫秒值 => 小于或等于当前时间的丢包都需要通知发送端再次发送...
	uint32_t cur_time_ms = (uint32_t)(CUtilTool::os_gettime_ns() / 1000000);
	// 重置补报长度为0 => 重新计算需要补包的个数...
	m_rtp_supply.suSize = 0;
	// 遍历丢包队列，找出需要补包的丢包序列号...
	GM_MapLose::iterator itorItem = m_MapLose.begin();
	while( itorItem != m_MapLose.end() ) {
		rtp_lose_t & rtpLose = itorItem->second;
		if( rtpLose.resend_time <= cur_time_ms ) {
			// 如果补包缓冲超过设定的最大值，跳出循环 => 最多补包200个...
			if( (nHeadSize + m_rtp_supply.suSize) >= nPackSize )
				break;
			// 累加补包长度，拷贝补包序列号...
			m_rtp_supply.suSize += sizeof(int);
			memcpy(lpData, &rtpLose.lose_seq, sizeof(int));
			// 累加数据指针...
			lpData += sizeof(int);
			// 累加重发次数...
			++rtpLose.resend_count;
			// 修正下次重传时间点 => cur_time + rtt_var => 丢包时的当前时间 + 丢包时的网络抖动时间差
			rtpLose.resend_time = (uint32_t)(CUtilTool::os_gettime_ns() / 1000000) + 10;//m_rtt_var;
		}
		// 累加丢包算子对象...
		++itorItem;
	}
	// 如果补充包缓冲为空，直接返回...
	if( m_rtp_supply.suSize <= 0 )
		return;
	// 打印已发送补包命令...
	TRACE("[Teacher-Looker] Supply Send => Count: %d\n", m_rtp_supply.suSize / sizeof(int));
	// 更新补包命令头内容块...
	memcpy(szPacket, &m_rtp_supply, nHeadSize);
	// 如果补包缓冲不为空，才进行补包命令发送...
	GM_Error theErr = GM_NoErr;
	int nDataSize = nHeadSize + m_rtp_supply.suSize;
	theErr = m_lpUDPSocket->SendTo(szPacket, nDataSize);
	((theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL);
	// 修改休息状态 => 已经有发包，不能休息...
	m_bNeedSleep = false;
}

void CUDPRecvThread::doRecvPacket()
{
	OSMutexLocker theLock(&m_Mutex);
	if( m_lpUDPSocket == NULL )
		return;
	GM_Error theErr = GM_NoErr;
	UInt32   outRecvLen = 0;
	UInt32   outRemoteAddr = 0;
	UInt16   outRemotePort = 0;
	UInt32   inBufLen = MAX_BUFF_LEN;
	char     ioBuffer[MAX_BUFF_LEN] = {0};
	// 调用接口从网络层获取数据包 => 这里是异步套接字，会立即返回 => 不管错误...
	theErr = m_lpUDPSocket->RecvFrom(&outRemoteAddr, &outRemotePort, ioBuffer, inBufLen, &outRecvLen);
	if( theErr != GM_NoErr )
		return;
	// 修改休息状态 => 已经成功收包，不能休息...
	m_bNeedSleep = false;
	
    // 获取第一个字节的高4位，得到数据包类型...
    uint8_t ptTag = (ioBuffer[0] >> 4) & 0x0F;
	
	// 对收到命令包进行类型分发...
	switch( ptTag )
	{
	case PT_TAG_RELOAD:  this->doTagReloadProcess(ioBuffer, outRecvLen); break;
	case PT_TAG_DETECT:	 this->doTagDetectProcess(ioBuffer, outRecvLen); break;
	case PT_TAG_HEADER:	 this->doTagHeaderProcess(ioBuffer, outRecvLen); break;
	case PT_TAG_AUDIO:	 this->doTagAVPackProcess(ioBuffer, outRecvLen); break;
	case PT_TAG_VIDEO:	 this->doTagAVPackProcess(ioBuffer, outRecvLen); break;
	}
}
//
// 处理服务器发送过来的重建命令...
void CUDPRecvThread::doTagReloadProcess(char * lpBuffer, int inRecvLen)
{
	if( m_lpUDPSocket == NULL || lpBuffer == NULL || inRecvLen <= 0 || inRecvLen < sizeof(rtp_reload_t) )
		return;
    // 通过第一个字节的低2位，判断终端类型...
    uint8_t tmTag = lpBuffer[0] & 0x03;
    // 获取第一个字节的中2位，得到终端身份...
    uint8_t idTag = (lpBuffer[0] >> 2) & 0x03;
    // 获取第一个字节的高4位，得到数据包类型...
    uint8_t ptTag = (lpBuffer[0] >> 4) & 0x0F;
	// 如果不是服务器端发送的重建命令，直接返回...
	if( tmTag != TM_TAG_SERVER || idTag != ID_TAG_SERVER )
		return;
	// 如果不是第一次重建，重建命令必须间隔20秒以上...
	if( m_rtp_reload.reload_time > 0 ) {
		uint32_t cur_time_sec = (uint32_t)(CUtilTool::os_gettime_ns() / 1000000000);
		uint32_t load_time_sec = m_rtp_reload.reload_time / 1000;
		// 如果重建命令间隔不到20秒，直接返回...
		if( (cur_time_sec - load_time_sec) < RELOAD_TIME_OUT )
			return;
	}
	// 保存服务器传递的重建命令...
	m_rtp_reload.tm = tmTag;
	m_rtp_reload.id = idTag;
	m_rtp_reload.pt = ptTag;
	// 记录本地重建信息...
	m_rtp_reload.reload_time = (uint32_t)(CUtilTool::os_gettime_ns() / 1000000);
	++m_rtp_reload.reload_count;
	// 打印收到服务器重建命令...
	TRACE("[Teacher-Looker] Server Reload Count: %d\n", m_rtp_reload.reload_count);
	// 重置相关命令包...
	memset(&m_rtp_header, 0, sizeof(m_rtp_header));
	// 清空补包集合队列...
	m_MapLose.clear();
	// 释放环形队列空间...
	circlebuf_free(&m_circle);
	// 初始化环形队列，预分配空间...
	circlebuf_init(&m_circle);
	circlebuf_reserve(&m_circle, 512*1024);
	// 重置相关变量...
	m_next_create_ns = -1;
	m_next_detect_ns = -1;
	m_next_ready_ns = -1;
	m_bSendReady = true;
	// 重新开始探测网络...
	m_rtp_detect.tsSrc = 0;
	m_rtp_detect.dtNum = 0;
	m_rtt_var = -1;
	m_rtt_ms = -1;
	// 重置视频序列头...
	m_strSPS.clear();
	m_strPPS.clear();
}

void CUDPRecvThread::doTagDetectProcess(char * lpBuffer, int inRecvLen)
{
	GM_Error theErr = GM_NoErr;
	if( m_lpUDPSocket == NULL || lpBuffer == NULL || inRecvLen <= 0 || inRecvLen < sizeof(rtp_detect_t) )
		return;
    // 通过第一个字节的低2位，判断终端类型...
    uint8_t tmTag = lpBuffer[0] & 0x03;
    // 获取第一个字节的中2位，得到终端身份...
    uint8_t idTag = (lpBuffer[0] >> 2) & 0x03;
    // 获取第一个字节的高4位，得到数据包类型...
    uint8_t ptTag = (lpBuffer[0] >> 4) & 0x0F;
	// 如果是 学生推流端 发出的探测包，将收到的探测数据包原样返回给服务器端...
	if( tmTag == TM_TAG_STUDENT && idTag == ID_TAG_PUSHER ) {
		theErr = m_lpUDPSocket->SendTo(lpBuffer, inRecvLen);
		return;
	}
	// 如果是 老师观看端 自己发出的探测包，计算网络延时...
	if( tmTag == TM_TAG_TEACHER && idTag == ID_TAG_LOOKER ) {
		// 获取收到的探测数据包...
		rtp_detect_t rtpDetect = {0};
		memcpy(&rtpDetect, lpBuffer, sizeof(rtpDetect));
		// 当前时间转换成毫秒，计算网络延时 => 当前时间 - 探测时间...
		uint32_t cur_time_ms = (uint32_t)(CUtilTool::os_gettime_ns() / 1000000);
		int keep_rtt = cur_time_ms - rtpDetect.tsSrc;
		// 防止网络突发延迟增大, 借鉴 TCP 的 RTT 遗忘衰减的算法...
		if( m_rtt_ms < 0 ) { m_rtt_ms = keep_rtt; }
		else { m_rtt_ms = (7 * m_rtt_ms + keep_rtt) / 8; }
		// 计算网络抖动的时间差值 => RTT的修正值...
		if( m_rtt_var < 0 ) { m_rtt_var = m_rtt_ms; }
		else { m_rtt_var = (m_rtt_var * 3 + abs(m_rtt_ms - keep_rtt)) / 4; }
		// 打印探测结果 => 探测序号 | 网络延时(毫秒)...
		TRACE("[Teacher-Looker] Recv Detect dtNum: %d, rtt: %d ms, rtt_var: %d ms\n", rtpDetect.dtNum, m_rtt_ms, m_rtt_var);
	}
}

void CUDPRecvThread::doTagHeaderProcess(char * lpBuffer, int inRecvLen)
{
	if( lpBuffer == NULL || inRecvLen < 0 || inRecvLen < sizeof(rtp_header_t) )
		return;
	// 如果已经收到了序列头，直接返回...
	if( m_rtp_header.pt == PT_TAG_HEADER || m_rtp_header.hasAudio || m_rtp_header.hasVideo )
		return;
    // 通过第一个字节的低2位，判断终端类型...
    uint8_t tmTag = lpBuffer[0] & 0x03;
    // 获取第一个字节的中2位，得到终端身份...
    uint8_t idTag = (lpBuffer[0] >> 2) & 0x03;
    // 获取第一个字节的高4位，得到数据包类型...
    uint8_t ptTag = (lpBuffer[0] >> 4) & 0x0F;
	// 如果发送者不是 学生推流端 => 直接返回...
	if( tmTag != TM_TAG_STUDENT || idTag != ID_TAG_PUSHER )
		return;
	// 如果发现长度不够，直接返回...
	memcpy(&m_rtp_header, lpBuffer, sizeof(m_rtp_header));
	int nNeedSize = m_rtp_header.spsSize + m_rtp_header.ppsSize + sizeof(m_rtp_header);
	if( nNeedSize != inRecvLen ) {
		TRACE("[Teacher-Looker] Recv Header error, RecvLen: %d\n", inRecvLen);
		memset(&m_rtp_header, 0, sizeof(m_rtp_header));
		return;
	}
	// 读取SPS和PPS格式头信息...
	char * lpData = lpBuffer + sizeof(m_rtp_header);
	if( m_rtp_header.spsSize > 0 ) {
		m_strSPS.assign(lpData, m_rtp_header.spsSize);
		lpData += m_rtp_header.spsSize;
	}
	// 保存 PPS 格式头...
	if( m_rtp_header.ppsSize > 0 ) {
		m_strPPS.assign(lpData, m_rtp_header.ppsSize);
		lpData += m_rtp_header.ppsSize;
	}
	// 打印收到序列头结构体信息...
	TRACE("[Teacher-Looker] Recv Header SPS: %d, PPS: %d\n", m_strSPS.size(), m_strPPS.size());
}

void CUDPRecvThread::doEraseLoseSeq(uint32_t inSeqID)
{
	// 如果没有找到指定的序列号，直接返回...
	GM_MapLose::iterator itorItem = m_MapLose.find(inSeqID);
	if( itorItem == m_MapLose.end() )
		return;
	// 删除检测到的丢包节点...
	rtp_lose_t & rtpLose = itorItem->second;
	uint32_t nResendCount = rtpLose.resend_count;
	m_MapLose.erase(itorItem);
	// 打印已收到的补包信息，还剩下的未补包个数...
	TRACE("[Teacher-Looker] Supply Erase => LoseSeq: %lu, ResendCount: %lu, LoseSize: %lu\n", inSeqID, nResendCount, m_MapLose.size());
}

void CUDPRecvThread::doTagAVPackProcess(char * lpBuffer, int inRecvLen)
{
	if( lpBuffer == NULL || inRecvLen < 0 || inRecvLen < sizeof(rtp_hdr_t) )
		return;
	// 如果没有收到了序列头，直接返回...
	if( m_rtp_header.pt != PT_TAG_HEADER )
		return;
	// 既没有音频，也没有视频，直接返回...
	if( !m_rtp_header.hasAudio && !m_rtp_header.hasVideo )
		return;
	// 设置不要再发送准备就绪命令包了...
	m_bSendReady = false;
	// 如果收到的缓冲区长度不够 或 填充量为负数，直接丢弃...
	rtp_hdr_t * lpNewHeader = (rtp_hdr_t*)lpBuffer;
	int nDataSize = lpNewHeader->psize + sizeof(rtp_hdr_t);
	int nZeroSize = DEF_MTU_SIZE - lpNewHeader->psize;
	uint32_t new_id = lpNewHeader->seq;
	uint32_t max_id = new_id;
	uint32_t min_id = new_id;
	if( inRecvLen != nDataSize || nZeroSize < 0 ) {
		TRACE("[Teacher-Looker] Discard => Seq: %lu, TS: %lu, Type: %d, Slice: %d, ZeroSize: %d\n", lpNewHeader->seq, lpNewHeader->ts, lpNewHeader->pt, lpNewHeader->psize, nZeroSize);
		return;
	}
	// 打印收到的音频数据包信息 => 包括缓冲区填充量 => 每个数据包都是统一大小 => rtp_hdr_t + slice + Zero => 812
	//TRACE("[Teacher-Looker] Seq: %lu, TS: %lu, Type: %d, pst: %d, ped: %d, Slice: %d, ZeroSize: %d\n", lpNewHeader->seq, lpNewHeader->ts, lpNewHeader->pt, lpNewHeader->pst, lpNewHeader->ped, lpNewHeader->psize, nZeroSize);
	// 首先，将当前包序列号从丢包队列当中删除...
	this->doEraseLoseSeq(new_id);
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：每个环形队列中的数据包大小是一样的 => rtp_hdr_t + slice + Zero => 12 + 800 => 812
	//////////////////////////////////////////////////////////////////////////////////////////////////
	const int nPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacket[nPackSize] = {0};
	// 如果环形队列为空 => 直接将数据加入到环形队列当中...
	if( m_circle.size < nPackSize ) {
		// 先加入包头和数据内容 => 
		circlebuf_push_back(&m_circle, lpBuffer, inRecvLen);
		// 再加入填充数据内容，保证数据总是保持一个MTU单元大小...
		if( nZeroSize > 0 ) {
			circlebuf_push_back_zero(&m_circle, nZeroSize);
		}
		return;
	}
	// 环形队列中至少要有一个数据包...
	ASSERT( m_circle.size >= nPackSize );
	// 获取环形队列中最小序列号...
	circlebuf_peek_front(&m_circle, szPacket, nPackSize);
	rtp_hdr_t * lpMinHeader = (rtp_hdr_t*)szPacket;
	min_id = lpMinHeader->seq;
	// 获取环形队列中最大序列号...
	circlebuf_peek_back(&m_circle, szPacket, nPackSize);
	rtp_hdr_t * lpMaxHeader = (rtp_hdr_t*)szPacket;
	max_id = lpMaxHeader->seq;
	// 发生丢包条件 => max_id + 1 < new_id
	if( max_id + 1 < new_id ) {
		// 丢包区间 => [max_id + 1, new_id - 1];
		uint32_t sup_id = max_id + 1;
		rtp_hdr_t rtpDis = {0};
		rtpDis.pt = PT_TAG_LOSE;
		while( sup_id < new_id ) {
			// 给当前丢包预留缓冲区...
			rtpDis.seq = sup_id;
			circlebuf_push_back(&m_circle, &rtpDis, sizeof(rtpDis));
			circlebuf_push_back_zero(&m_circle, DEF_MTU_SIZE);
			// 将丢包序号加入丢包队列当中 => 毫秒时刻点...
			rtp_lose_t rtpLose = {0};
			rtpLose.resend_count = 0;
			rtpLose.lose_seq = sup_id;
			// 重发时间点 => cur_time + rtt_var => 丢包时的当前时间 + 丢包时的网络抖动时间差
			rtpLose.resend_time = (uint32_t)(CUtilTool::os_gettime_ns() / 1000000) + 10;//m_rtt_var;
			m_MapLose[sup_id] = rtpLose;
			// 打印已丢包信息，丢包队列长度...
			TRACE("[Teacher-Looker] Lose Seq: %lu, LoseSize: %lu\n", sup_id, m_MapLose.size());
			// 累加当前丢包序列号...
			++sup_id;
		}
	}
	// 如果是丢包或正常序号包，放入环形队列，返回...
	if( max_id + 1 <= new_id ) {
		// 先加入包头和数据内容...
		circlebuf_push_back(&m_circle, lpBuffer, inRecvLen);
		// 再加入填充数据内容，保证数据总是保持一个MTU单元大小...
		if( nZeroSize > 0 ) {
			circlebuf_push_back_zero(&m_circle, nZeroSize);
		}
		// 打印新加入的最大序号包...
		//TRACE("[Teacher-Looker] Max Seq: %lu\n", new_id);
		return;
	}
	// 如果是丢包后的补充包 => max_id > new_id
	if( max_id > new_id ) {
		// 如果最小序号大于丢包序号 => 打印错误，直接丢掉这个补充包...
		if( min_id > new_id ) {
			TRACE("[Teacher-Looker] Supply Discard => Seq: %lu, Min-Max: [%lu, %lu]\n", new_id, min_id, max_id);
			return;
		}
		// 最小序号不能比丢包序号小...
		ASSERT( min_id <= new_id );
		// 计算缓冲区更新位置...
		uint32_t nPosition = (new_id - min_id) * nPackSize;
		// 将获取的数据内容更新到指定位置...
		circlebuf_place(&m_circle, nPosition, lpBuffer, inRecvLen);
		// 打印补充包信息...
		TRACE("[Teacher-Looker] Supply Success => Seq: %lu, Min-Max: [%lu, %lu]\n", new_id, min_id, max_id);
		return;
	}
	// 如果是其它未知包，打印信息...
	TRACE("[Teacher-Looker] Supply Unknown => Seq: %lu, Slice: %d, Min-Max: [%lu, %lu]\n", new_id, lpNewHeader->psize, min_id, max_id);
}
///////////////////////////////////////////////////////
// 注意：没有发包，也没有收包，需要进行休息...
///////////////////////////////////////////////////////
void CUDPRecvThread::doSleepTo()
{
	// 如果不能休息，直接返回...
	if( !m_bNeedSleep )
		return;
	// 计算要休息的时间 => 休息5毫秒...
	uint64_t delta_ns = 5 * 1000000;
	uint64_t cur_time_ns = CUtilTool::os_gettime_ns();
	// 调用系统工具函数，进行sleep休息...
	CUtilTool::os_sleepto_ns(cur_time_ns + delta_ns);
}