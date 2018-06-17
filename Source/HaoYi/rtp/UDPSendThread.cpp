
#include "stdafx.h"
#include "UtilTool.h"
#include "UDPSocket.h"
#include "UDPSendThread.h"

CUDPSendThread::CUDPSendThread(int nDBRoomID, int nDBCameraID)
  : m_lpUDPSocket(NULL)
  , m_next_create_ns(-1)
  , m_next_detect_ns(-1)
  , m_bNeedSleep(false)
  , m_nCurPackSeq(0)
  , m_nCurSendSeq(0)
{
	// 初始化rtp序列头结构体...
	memset(&m_rtp_detect, 0, sizeof(m_rtp_detect));
	memset(&m_rtp_create, 0, sizeof(m_rtp_create));
	memset(&m_rtp_delete, 0, sizeof(m_rtp_delete));
	memset(&m_rtp_header, 0, sizeof(m_rtp_header));
	memset(&m_rtp_ready, 0, sizeof(m_rtp_ready));
	// 初始化环形队列，预分配空间...
	circlebuf_init(&m_circle);
	circlebuf_reserve(&m_circle, 512*1024);
	// 设置终端类型和结构体类型 => m_rtp_ready => 等待网络填充...
	m_rtp_detect.tm = m_rtp_create.tm = m_rtp_delete.tm = m_rtp_header.tm = TM_TAG_STUDENT;
	m_rtp_detect.id = m_rtp_create.id = m_rtp_delete.id = m_rtp_header.id = ID_TAG_PUSHER;
	m_rtp_detect.pt = PT_TAG_DETECT;
	m_rtp_create.pt = PT_TAG_CREATE;
	m_rtp_delete.pt = PT_TAG_DELETE;
	m_rtp_header.pt = PT_TAG_HEADER;
	// 填充房间号和直播通道号...
	m_rtp_create.roomID = nDBRoomID;
	m_rtp_create.liveID = nDBCameraID;
	m_rtp_delete.roomID = nDBRoomID;
	m_rtp_delete.liveID = nDBCameraID;
}

CUDPSendThread::~CUDPSendThread()
{
	// 停止线程，等待退出...
	this->StopAndWaitForThread();
	// 关闭UDPSocket对象...
	this->CloseSocket();
	// 释放环形队列空间...
	circlebuf_free(&m_circle);
}

void CUDPSendThread::CloseSocket()
{
	if( m_lpUDPSocket != NULL ) {
		m_lpUDPSocket->Close();
		delete m_lpUDPSocket;
		m_lpUDPSocket = NULL;
	}
}

BOOL CUDPSendThread::InitVideo(string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS)
{
	OSMutexLocker theLock(&m_Mutex);
	// 设置视频标志...
	m_rtp_header.hasVideo = true;
	// 保存传递过来的参数信息...
	m_rtp_header.fpsNum = nFPS;
	m_rtp_header.picWidth = nWidth;
	m_rtp_header.picHeight = nHeight;
	m_rtp_header.spsSize = inSPS.size();
	m_rtp_header.ppsSize = inPPS.size();
	m_strSPS = inSPS; m_strPPS = inPPS;
	// 线程没有启动，直接启动...
	if( this->GetThreadHandle() != NULL )
		return true;
	return ((this->InitThread() != GM_NoErr) ? false : true);
}

BOOL CUDPSendThread::InitAudio(int nRateIndex, int nChannelNum)
{
	OSMutexLocker theLock(&m_Mutex);
	// 设置音频标志...
	m_rtp_header.hasAudio = true;
	// 保存采样率索引和声道...
	m_rtp_header.rateIndex = nRateIndex;
	m_rtp_header.channelNum = nChannelNum;
	// 线程没有启动，直接启动...
	if( this->GetThreadHandle() != NULL )
		return true;
	return ((this->InitThread() != GM_NoErr) ? false : true);
}

GM_Error CUDPSendThread::InitThread()
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

/*void CUDPSendThread::doFirstPayload()
{
	// 如果是第一帧，填充音视频序列头...
	if( m_nCurPackSeq > 0 )
		return;
	// 累加RTP打包序列号...
	++m_nCurPackSeq;
	// 填充RTP包头结构体...
	rtp_hdr_t rtpHeader = {0};
	rtpHeader.seq = m_nCurPackSeq;
	rtpHeader.ts  = 0;
	rtpHeader.pt  = PT_TAG_HEADER;
	rtpHeader.pk  = true;
	rtpHeader.pst = true;
	rtpHeader.ped = true;
	rtpHeader.psize = sizeof(rtp_seq_t) + m_strSPS.size() + m_strPPS.size();
	// 计算填充数据总长度...
	int nZeroSize = DEF_MTU_SIZE - rtpHeader.psize;
	// 加入环形队列当中 => rtp_hdr_t + rtp_seq_t + sps + pps + Zero => 12 + 800 => 812
	circlebuf_push_back(&m_circle, &rtpHeader, sizeof(rtpHeader));
	circlebuf_push_back(&m_circle, &m_rtp_seq_header, sizeof(m_rtp_seq_header));
	// 加入SPS数据区内容...
	if( m_strSPS.size() > 0 ) {
		circlebuf_push_back(&m_circle, m_strSPS.c_str(), m_strSPS.size());
	}
	// 加入PPS数据区内容...
	if( m_strPPS.size() > 0 ) {
		circlebuf_push_back(&m_circle, m_strPPS.c_str(), m_strPPS.size());
	}
	// 加入填充数据内容，保证数据总是保持一个MTU单元大小...
	if( nZeroSize > 0 ) {
		circlebuf_push_back_zero(&m_circle, nZeroSize);
	}
	// 打印调试信息...
	TRACE("[Pack] Seq: %lu, TS: 0, Type: %d, SPS: %lu, PPS: %lu, Zero: %d\n", m_nCurPackSeq, PT_TAG_HEADER, m_strSPS.size(), m_strPPS.size(), nZeroSize);
}*/

void CUDPSendThread::PushFrame(FMS_FRAME & inFrame)
{
	OSMutexLocker theLock(&m_Mutex);
	/////////////////////////////////////////////////////////////////////
	// 注意：只有当接收端准备好之后，才能开始推流操作...
	/////////////////////////////////////////////////////////////////////
	// 如果接收端没有准备好，直接返回...
	if( m_rtp_ready.tm != TM_TAG_TEACHER || m_rtp_ready.recvPort <= 0 )
		return;
	// 如果有视频，第一帧必须是视频关键帧...
	if( m_rtp_header.hasVideo && m_nCurPackSeq <= 0 ) {
		if( inFrame.typeFlvTag != PT_TAG_VIDEO || !inFrame.is_keyframe )
			return;
		ASSERT( inFrame.typeFlvTag == PT_TAG_VIDEO && inFrame.is_keyframe );
	}
	// 构造RTP包头结构体...
	rtp_hdr_t rtpHeader = {0};
	rtpHeader.tm  = TM_TAG_STUDENT;
	rtpHeader.id  = ID_TAG_PUSHER;
	rtpHeader.pt  = inFrame.typeFlvTag;
	rtpHeader.pk  = inFrame.is_keyframe;
	rtpHeader.ts  = inFrame.dwSendTime;
	// 计算需要分片的个数 => 需要注意累加最后一个分片...
	int nSliceTotalSize = 0;
	int nSliceSize = DEF_MTU_SIZE;
	int nFrameSize = inFrame.strData.size();
	int nSliceNum = nFrameSize / DEF_MTU_SIZE;
	char * lpszFramePtr = (char*)inFrame.strData.c_str();
	nSliceNum += ((nFrameSize % DEF_MTU_SIZE) ? 1 : 0);
	// 进行循环压入环形队列当中...
	for(int i = 0; i < nSliceNum; ++i) {
		rtpHeader.seq = ++m_nCurPackSeq; // 累加打包序列号...
		rtpHeader.pst = ((i == 0) ? true : false); // 是否是第一个分片...
		rtpHeader.ped = ((i+1 == nSliceNum) ? true: false); // 是否是最后一个分片...
		rtpHeader.psize = (rtpHeader.ped ? (nFrameSize % DEF_MTU_SIZE) : DEF_MTU_SIZE); // 如果是最后一个分片，取余数，否则，取MTU值...
		// 计算填充数据长度...
		int nZeroSize = DEF_MTU_SIZE - rtpHeader.psize;
		// 计算分片包的数据头指针...
		LPCTSTR lpszSlicePtr = lpszFramePtr + i * DEF_MTU_SIZE;
		// 加入环形队列当中 => rtp_hdr_t + slice + Zero => 12 + 800 => 812
		circlebuf_push_back(&m_circle, &rtpHeader, sizeof(rtpHeader));
		circlebuf_push_back(&m_circle, lpszSlicePtr, rtpHeader.psize);
		// 加入填充数据内容，保证数据总是保持一个MTU单元大小...
		if( nZeroSize > 0 ) {
			circlebuf_push_back_zero(&m_circle, nZeroSize);
		}
		// 打印调试信息...
		//TRACE("[PUSH] Seq: %lu, TS: %lu, Type: %d, pst: %d, ped: %d, Slice: %d, Zero: %d\n", rtpHeader.seq, rtpHeader.ts, rtpHeader.pt, rtpHeader.pst, rtpHeader.ped, rtpHeader.psize, nZeroSize);
	}
}

void CUDPSendThread::Entry()
{
	while( !this->IsStopRequested() ) {
		// 设置休息标志 => 只要有发包或收包就不能休息...
		m_bNeedSleep = true;
		// 发送创建房间和直播通道命令包...
		this->doSendCreateCmd();
		// 发送探测命令包...
		this->doSendDetectCmd();
		// 发送一个封装好的RTP数据包...
		this->doSendPacket();
		// 接收一个到达的服务器反馈包...
		this->doRecvPacket();
		// 等待发送或接收下一个数据包...
		this->doSleepTo();
	}
	// 发送一次删除命令包...
	this->doSendDeleteCmd();
}

void CUDPSendThread::doSendDeleteCmd()
{
	OSMutexLocker theLock(&m_Mutex);
	GM_Error theErr = GM_NoErr;
	if( m_lpUDPSocket == NULL )
		return;
	// 套接字有效，直接发送删除命令...
	ASSERT( m_lpUDPSocket != NULL );
	theErr = m_lpUDPSocket->SendTo((void*)&m_rtp_delete, sizeof(m_rtp_delete));
	return;
}

void CUDPSendThread::doSendCreateCmd()
{
	OSMutexLocker theLock(&m_Mutex);
	// 如果已经收到正确的准备就绪包，直接返回...
	if( m_rtp_ready.tm == TM_TAG_TEACHER && m_rtp_ready.recvPort > 0 )
		return;
	// 每隔100毫秒发送创建命令包 => 必须转换成有符号...
	int64_t cur_time_ns = CUtilTool::os_gettime_ns();
	int64_t period_ns = 100000000;
	// 如果发包时间还没到，直接返回...
	if( m_next_create_ns > cur_time_ns )
		return;
	ASSERT( cur_time_ns >= m_next_create_ns );
	// 首先，发送一个创建房间命令...
	GM_Error theErr = m_lpUDPSocket->SendTo((void*)&m_rtp_create, sizeof(m_rtp_create));
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
		return;
	}
	// 然后，发送序列头命令包...
	string strSeqHeader;
	strSeqHeader.append((char*)&m_rtp_header, sizeof(m_rtp_header));
	// 加入SPS数据区内容...
	if( m_strSPS.size() > 0 ) {
		strSeqHeader.append(m_strSPS);
	}
	// 加入PPS数据区内容...
	if( m_strPPS.size() > 0 ) {
		strSeqHeader.append(m_strPPS);
	}
	// 调用套接字接口，直接发送RTP数据包...
	theErr = m_lpUDPSocket->SendTo((void*)strSeqHeader.c_str(), strSeqHeader.size());
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
		return;
	}
	// 计算下次发送创建命令的时间戳...
	m_next_create_ns = CUtilTool::os_gettime_ns() + period_ns;
	// 修改休息状态 => 已经有发包，不能休息...
	m_bNeedSleep = false;
}

void CUDPSendThread::doSendDetectCmd()
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

void CUDPSendThread::doSendPacket()
{
	OSMutexLocker theLock(&m_Mutex);
	if( m_circle.size <= 0 || m_lpUDPSocket == NULL )
		return;
	// 取出最前面的RTP数据包，但不从环形队列中移除 => 目的是给接收端补包用...
	rtp_hdr_t * lpFrontHeader = NULL;
	rtp_hdr_t * lpSendHeader = NULL;
	int nSendPos = 0, nSendSize = 0;
	int nPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	// 计算环形队列中最前面数据包的头指针 => 最小序号...
	lpFrontHeader = (rtp_hdr_t*)circlebuf_data(&m_circle, 0);
	// 第一次发包 或 发包序号太小 => 使用最前面包的序列号...
	if((m_nCurSendSeq <= 0) || (m_nCurSendSeq < lpFrontHeader->seq)) {
		m_nCurSendSeq = lpFrontHeader->seq;
	}
	// 将要发送的数据包序号不能小于最前面包的序列号...
	ASSERT( m_nCurSendSeq >= lpFrontHeader->seq );
	// 两者之差就是要发送数据包的头指针位置...
	nSendPos = (m_nCurSendSeq - lpFrontHeader->seq) * nPackSize;
	// 如果要发包位置超过了队列有效长度，说明数据包已经发送完毕...
	if( nSendPos < 0 || nSendPos >= m_circle.size )
		return;
	// 获取发送数据包的包头位置和有效数据长度...
	lpSendHeader = (rtp_hdr_t*)circlebuf_data(&m_circle, nSendPos);
	nSendSize = sizeof(rtp_hdr_t) + lpSendHeader->psize;
	// 调用套接字接口，直接发送RTP数据包...
	GM_Error theErr = m_lpUDPSocket->SendTo((void*)lpSendHeader, nSendSize);
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
		return;
	}
	// 成功发送数据包 => 累加发送序列号...
	++m_nCurSendSeq;
	// 修改休息状态 => 已经有发包，不能休息...
	m_bNeedSleep = false;

	// 打印调试信息 => 刚刚发送的数据包...
	int nZeroSize = DEF_MTU_SIZE - lpSendHeader->psize;
	TRACE("[Send ] Seq: %lu, TS: %lu, Type: %d, pst: %d, ped: %d, Slice: %d, Zero: %d\n", lpSendHeader->seq, lpSendHeader->ts, lpSendHeader->pt, lpSendHeader->pst, lpSendHeader->ped, lpSendHeader->psize, nZeroSize);

	// 实验：移除最前面的数据包...
	string strPacket;
	strPacket.resize(nPackSize);
	circlebuf_pop_front(&m_circle, (void*)strPacket.c_str(), nPackSize);

	/*string	strRtpData;
	int         nZeroSize = 0;
	rtp_hdr_t	rtpHeader = {0};
	// 获取RTP包头结构体...
	circlebuf_pop_front(&m_circle, &rtpHeader, sizeof(rtpHeader));
	// 分配RTP数据包大小，计算填充区大小...
	strRtpData.resize(rtpHeader.psize);
	nZeroSize = DEF_MTU_SIZE - rtpHeader.psize;
	// 弹出有效的RTP数据包...
	circlebuf_pop_front(&m_circle, (void*)strRtpData.c_str(), rtpHeader.psize);
	// 弹出RTP填充数据内容...
	if( nZeroSize > 0 ) {
		circlebuf_pop_front(&m_circle, NULL, nZeroSize);
	}
	// 获取RTP数据包头结构体指针...
	LPCTSTR lpszData = strRtpData.c_str();
	// 如果是第一个RTP数据包，获取序列头结构体，SPS和PPS数据...
	if( rtpHeader.pt == PT_TAG_HEADER ) {
		string strSPS, strPPS;
		// 读取序列头结构体...
		rtp_seq_t * lpRtpSeq = (rtp_seq_t*)lpszData;
		// 读取SPS内容信息...
		lpszData += sizeof(rtp_seq_t);
		if( lpRtpSeq->spsSize > 0 ) {
			strSPS.assign(lpszData, lpRtpSeq->spsSize);
		}
		// 读取PPS内容信息...
		lpszData += lpRtpSeq->spsSize;
		if( lpRtpSeq->ppsSize > 0 ) {
			strPPS.assign(lpszData, lpRtpSeq->ppsSize);
		}
		// 打印调试信息...
		TRACE("[POP ] Seq: %lu, TS: 0, Type: %d, SPS: %lu, PPS: %lu, Zero: %d\n", rtpHeader.seq, rtpHeader.pt, strSPS.size(), strPPS.size(), nZeroSize);
	} else {
		// 处理音视频RTP数据包...

		// 打印调试信息...
		TRACE("[POP ] Seq: %lu, TS: %lu, Type: %d, pst: %d, ped: %d, Slice: %d, Zero: %d\n", rtpHeader.seq, rtpHeader.ts, rtpHeader.pt, rtpHeader.pst, rtpHeader.ped, rtpHeader.psize, nZeroSize);
	}*/
}

void CUDPSendThread::doRecvPacket()
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
	case PT_TAG_DETECT:	 this->doTagDetectProcess(ioBuffer, outRecvLen); break;
	case PT_TAG_READY:	 this->doTagReadyProcess(ioBuffer, outRecvLen);   break;
	}
}

void CUDPSendThread::doTagDetectProcess(char * lpBuffer, int inRecvLen)
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
	// 如果是 老师观看者 发出的探测包，将收到的探测数据包原样返回给服务器端...
	if( tmTag == TM_TAG_TEACHER && idTag == ID_TAG_LOOKER ) {
		theErr = m_lpUDPSocket->SendTo(lpBuffer, inRecvLen);
		return;
	}
	// 如果是 学生推流端 自己发出的探测包，计算网络延时...
	if( tmTag == TM_TAG_STUDENT && idTag == ID_TAG_PUSHER ) {
		// 获取收到的探测数据包...
		rtp_detect_t rtpDetect = {0};
		memcpy(&rtpDetect, lpBuffer, sizeof(rtpDetect));
		// 当前时间转换成毫秒，计算网络延时 => 当前时间 - 探测时间...
		uint32_t cur_time_ms = (uint32_t)(CUtilTool::os_gettime_ns() / 1000000);
		int  new_rtt = cur_time_ms - rtpDetect.tsSrc;
		// 打印探测结果 => 探测序号 | 网络延时(毫秒)...
		TRACE("[Student-Pusher] Detect dtNum: %d, rtt: %d ms\n", rtpDetect.dtNum, new_rtt);
	}
}

void CUDPSendThread::doTagReadyProcess(char * lpBuffer, int inRecvLen)
{
	if( lpBuffer == NULL || inRecvLen <= 0 || inRecvLen < sizeof(m_rtp_ready) )
		return;
    // 通过第一个字节的低2位，判断终端类型...
    uint8_t tmTag = lpBuffer[0] & 0x03;
    // 获取第一个字节的中2位，得到终端身份...
    uint8_t idTag = (lpBuffer[0] >> 2) & 0x03;
    // 获取第一个字节的高4位，得到数据包类型...
    uint8_t ptTag = (lpBuffer[0] >> 4) & 0x0F;
	// 如果不是 老师观看端 发出的准备就绪包，直接返回...
	if( tmTag != TM_TAG_TEACHER || idTag != ID_TAG_LOOKER )
		return;
	// 保存老师观看端发送的准备就绪数据包内容...
	memcpy(&m_rtp_ready, lpBuffer, sizeof(m_rtp_ready));
}
///////////////////////////////////////////////////////
// 注意：没有发包，也没有收包，需要进行休息...
///////////////////////////////////////////////////////
void CUDPSendThread::doSleepTo()
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