
#include "stdafx.h"
#include "UtilTool.h"
#include "UDPSocket.h"
#include "UDPSendThread.h"

CUDPSendThread::CUDPSendThread(int nDBRoomID, int nDBCameraID)
  : m_lpUDPSocket(NULL)
  , m_next_create_ns(-1)
  , m_next_header_ns(-1)
  , m_next_detect_ns(-1)
  , m_bNeedSleep(false)
  , m_nCurPackSeq(0)
  , m_nCurSendSeq(0)
  , m_rtt_var_ms(-1)
  , m_rtt_ms(-1)
{
	// 初始化命令状态...
	m_nCmdState = kCmdSendCreate;
	// 初始化rtp序列头结构体...
	memset(&m_rtp_reload, 0, sizeof(m_rtp_reload));
	memset(&m_rtp_detect, 0, sizeof(m_rtp_detect));
	memset(&m_rtp_create, 0, sizeof(m_rtp_create));
	memset(&m_rtp_delete, 0, sizeof(m_rtp_delete));
	memset(&m_rtp_header, 0, sizeof(m_rtp_header));
	memset(&m_rtp_ready, 0, sizeof(m_rtp_ready));
	// 初始化环形队列，预分配空间...
	circlebuf_init(&m_circle);
	circlebuf_reserve(&m_circle, DEF_CIRCLE_SIZE);
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
	// 未知状态，阻止继续塞包...
	m_nCmdState = kCmdUnkownState;
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
	//LPCTSTR lpszAddr = "192.168.1.70";
	LPCTSTR lpszAddr = DEF_UDP_HOME;
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

/*static void DoSaveSendFile(uint32_t inPTS, int inType, bool bIsKeyFrame, string & strFrame)
{
	static char szBuf[MAX_PATH] = {0};
	char * lpszPath = "F:/MP4/Dst/send.txt";
	FILE * pFile = fopen(lpszPath, "a+");
	sprintf(szBuf, "PTS: %lu, Type: %d, Key: %d, Size: %d\n", inPTS, inType, bIsKeyFrame, strFrame.size());
	fwrite(szBuf, 1, strlen(szBuf), pFile);
	fwrite(strFrame.c_str(), 1, strFrame.size(), pFile);
	fclose(pFile);
}

static void DoSaveSendSeq(uint32_t inPSeq, int inPSize, bool inPST, bool inPED, uint32_t inPTS)
{
	static char szBuf[MAX_PATH] = {0};
	char * lpszPath = "F:/MP4/Dst/send_seq.txt";
	FILE * pFile = fopen(lpszPath, "a+");
	sprintf(szBuf, "PSeq: %lu, PSize: %d, PST: %d, PED: %d, PTS: %lu\n", inPSeq, inPSize, inPST, inPED, inPTS);
	fwrite(szBuf, 1, strlen(szBuf), pFile);
	fclose(pFile);
}*/

void CUDPSendThread::PushFrame(FMS_FRAME & inFrame)
{
	OSMutexLocker theLock(&m_Mutex);
	// 判断线程是否已经退出 或 环形队列缓冲无效...
	if( this->IsStopRequested() || m_circle.capacity <= 0 ) {
		uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
		TRACE("[Student-Pusher] Time: %lu ms, Error => Send Thread has been stoped\n", now_ms);
		return;
	}
	// 如果数据帧的长度为0，打印错误，直接返回...
	if( inFrame.strData.size() <= 0 ) {
		uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
		TRACE("[Student-Pusher] Time: %lu ms, Error => Input Frame Size is Zero\n", now_ms);
		return;
	}
	/////////////////////////////////////////////////////////////////////
	// 注意：只有当接收端准备好之后，才能开始推流操作...
	/////////////////////////////////////////////////////////////////////
	// 如果接收端没有准备好，直接返回 => 收到准备继续包，状态为打包状态...
	if( m_nCmdState != kCmdSendAVPack || m_rtp_ready.tm != TM_TAG_TEACHER || m_rtp_ready.recvPort <= 0 )
		return;
	// 如果有视频，第一帧必须是视频关键帧...
	if( m_rtp_header.hasVideo && m_nCurPackSeq <= 0 ) {
		if( inFrame.typeFlvTag != PT_TAG_VIDEO || !inFrame.is_keyframe )
			return;
		ASSERT( inFrame.typeFlvTag == PT_TAG_VIDEO && inFrame.is_keyframe );
	}
	// 打印第一帧信息 => 开始有音视频数据包了...
	/*if( m_nCurPackSeq <= 0 ) {
		uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
		TRACE( "[Student-Pusher] Time: %lu ms, First Frame => Type: %d, KeyFrame: %d, TS: %lu, Size: %d\n", 
				now_ms, inFrame.typeFlvTag, inFrame.is_keyframe, inFrame.dwSendTime, inFrame.strData.size() );
	}*/
	// 打印所有的音视频数据帧...
	//uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	//TRACE( "[Student-Pusher] Time: %lu ms, Frame => Type: %d, Key: %d, PTS: %lu, Size: %d\n", 
	//		now_ms, inFrame.typeFlvTag, inFrame.is_keyframe, inFrame.dwSendTime, inFrame.strData.size() );
	//DoSaveSendFile(inFrame.dwSendTime, inFrame.typeFlvTag, inFrame.is_keyframe, inFrame.strData);

	// 构造RTP包头结构体...
	rtp_hdr_t rtpHeader = {0};
	rtpHeader.tm  = TM_TAG_STUDENT;
	rtpHeader.id  = ID_TAG_PUSHER;
	rtpHeader.pt  = inFrame.typeFlvTag;
	rtpHeader.pk  = inFrame.is_keyframe;
	rtpHeader.ts  = inFrame.dwSendTime;
	// 计算需要分片的个数 => 需要注意累加最后一个分片...
	int nSliceSize = DEF_MTU_SIZE;
	int nFrameSize = inFrame.strData.size();
	int nSliceNum = nFrameSize / DEF_MTU_SIZE;
	char * lpszFramePtr = (char*)inFrame.strData.c_str();
	nSliceNum += ((nFrameSize % DEF_MTU_SIZE) ? 1 : 0);
	int nEndSize = nFrameSize - (nSliceNum - 1) * DEF_MTU_SIZE;
	// 进行循环压入环形队列当中...
	for(int i = 0; i < nSliceNum; ++i) {
		rtpHeader.seq = ++m_nCurPackSeq; // 累加打包序列号...
		rtpHeader.pst = ((i == 0) ? true : false); // 是否是第一个分片...
		rtpHeader.ped = ((i+1 == nSliceNum) ? true: false); // 是否是最后一个分片...
		rtpHeader.psize = rtpHeader.ped ? nEndSize : DEF_MTU_SIZE; // 如果是最后一个分片，取计算值(不能取余数，如果是MTU整数倍会出错)，否则，取MTU值...
		ASSERT( rtpHeader.psize > 0 && rtpHeader.psize <= DEF_MTU_SIZE );
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
		//uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
		//TRACE( "[Student-Pusher] Time: %lu ms, Seq: %lu, Type: %d, Key: %d, Size: %d, TS: %lu\n", now_ms, 
		//		rtpHeader.seq, rtpHeader.pt, rtpHeader.pk, rtpHeader.psize, rtpHeader.ts);
		//DoSaveSendSeq(rtpHeader.seq, rtpHeader.psize, rtpHeader.pst, rtpHeader.ped, rtpHeader.ts);
	}
}

void CUDPSendThread::Entry()
{
	while( !this->IsStopRequested() ) {
		// 设置休息标志 => 只要有发包或收包就不能休息...
		m_bNeedSleep = true;
		// 发送观看端需要的丢失数据包...
		this->doSendLosePacket();
		// 发送创建房间和直播通道命令包...
		this->doSendCreateCmd();
		// 发送序列头命令包...
		this->doSendHeaderCmd();
		// 发送探测命令包...
		this->doSendDetectCmd();
		// 发送一个封装好的RTP数据包...
		this->doSendPacket();
		// 接收一个到达的服务器反馈包...
		this->doRecvPacket();
		// 等待发送或接收下一个数据包...
		this->doSleepTo();
	}
	// 只发送一次删除命令包...
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
	// 打印已发送删除命令包...
	uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	TRACE("[Student-Pusher] Time: %lu ms, Send Delete RoomID: %lu, LiveID: %d\n", now_ms, m_rtp_delete.roomID, m_rtp_delete.liveID);
}

void CUDPSendThread::doSendCreateCmd()
{
	OSMutexLocker theLock(&m_Mutex);
	// 如果命令状态不是创建命令，不发送命令，直接返回...
	if( m_nCmdState != kCmdSendCreate )
		return;
	// 每隔100毫秒发送创建命令包 => 必须转换成有符号...
	int64_t cur_time_ns = CUtilTool::os_gettime_ns();
	int64_t period_ns = 100 * 1000000;
	// 如果发包时间还没到，直接返回...
	if( m_next_create_ns > cur_time_ns )
		return;
	ASSERT( cur_time_ns >= m_next_create_ns );
	// 首先，发送一个创建房间命令...
	GM_Error theErr = m_lpUDPSocket->SendTo((void*)&m_rtp_create, sizeof(m_rtp_create));
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	// 打印已发送创建命令包 => 第一个包有可能没有发送出去，也返回正常...
	uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	TRACE("[Student-Pusher] Time: %lu ms, Send Create RoomID: %lu, LiveID: %d\n", now_ms, m_rtp_create.roomID, m_rtp_create.liveID);
	// 计算下次发送创建命令的时间戳...
	m_next_create_ns = CUtilTool::os_gettime_ns() + period_ns;
	// 修改休息状态 => 已经有发包，不能休息...
	m_bNeedSleep = false;
}

void CUDPSendThread::doSendHeaderCmd()
{
	OSMutexLocker theLock(&m_Mutex);
	// 如果命令状态不是序列头命令，不发送命令，直接返回...
	if( m_nCmdState != kCmdSendHeader )
		return;
	// 每隔100毫秒发送序列头命令包 => 必须转换成有符号...
	int64_t cur_time_ns = CUtilTool::os_gettime_ns();
	int64_t period_ns = 100 * 1000000;
	// 如果发包时间还没到，直接返回...
	if( m_next_header_ns > cur_time_ns )
		return;
	ASSERT( cur_time_ns >= m_next_header_ns );
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
	GM_Error theErr = m_lpUDPSocket->SendTo((void*)strSeqHeader.c_str(), strSeqHeader.size());
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	// 打印已发送序列头命令包...
	uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	TRACE("[Student-Pusher] Time: %lu ms, Send Header SPS: %lu, PPS: %d\n", now_ms, m_strSPS.size(), m_strPPS.size());
	// 计算下次发送创建命令的时间戳...
	m_next_header_ns = CUtilTool::os_gettime_ns() + period_ns;
	// 修改休息状态 => 已经有发包，不能休息...
	m_bNeedSleep = false;
}

void CUDPSendThread::doSendDetectCmd()
{
	OSMutexLocker theLock(&m_Mutex);
	// 每隔1秒发送一个探测命令包 => 必须转换成有符号...
	int64_t cur_time_ns = CUtilTool::os_gettime_ns();
	int64_t period_ns = 1000 * 1000000;
	// 第一个探测包延时1/3秒发送 => 避免第一个探测包先到达，引发服务器发送重建命令...
	if( m_next_detect_ns < 0 ) { 
		m_next_detect_ns = cur_time_ns + period_ns / 3;
	}
	// 如果发包时间还没到，直接返回...
	if( m_next_detect_ns > cur_time_ns )
		return;
	ASSERT( cur_time_ns >= m_next_detect_ns );
	// 将探测起点时间转换成毫秒，累加探测计数器...
	m_rtp_detect.tsSrc  = (uint32_t)(cur_time_ns / 1000000);
	m_rtp_detect.dtNum += 1;
	// 调用接口发送探测命令包...
	GM_Error theErr = m_lpUDPSocket->SendTo((void*)&m_rtp_detect, sizeof(m_rtp_detect));
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	// 打印已发送探测命令包...
	uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	//TRACE("[Student-Pusher] Time: %lu ms, Send Detect dtNum: %d\n", now_ms, m_rtp_detect.dtNum);
	// 计算下次发送探测命令的时间戳...
	m_next_detect_ns = CUtilTool::os_gettime_ns() + period_ns;
	// 修改休息状态 => 已经有发包，不能休息...
	m_bNeedSleep = false;
}

void CUDPSendThread::doSendLosePacket()
{
	OSMutexLocker theLock(&m_Mutex);
	if( m_lpUDPSocket == NULL )
		return;
	// 丢包集合队列为空，直接返回...
	if( m_MapLose.size() <= 0 )
		return;
	// 拿出一个丢包记录，无论是否发送成功，都要删除这个丢包记录...
	// 如果观看端，没有收到这个数据包，会再次发起补包命令...
	GM_MapLose::iterator itorItem = m_MapLose.begin();
	rtp_lose_t rtpLose = itorItem->second;
	m_MapLose.erase(itorItem);
	// 如果环形队列为空，直接返回...
	if( m_circle.size <= 0 )
		return;
	// 先找到环形队列中最前面数据包的头指针 => 最小序号...
	GM_Error    theErr = GM_NoErr;
	rtp_hdr_t * lpFrontHeader = NULL;
	rtp_hdr_t * lpSendHeader = NULL;
	int nSendPos = 0, nSendSize = 0;
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：千万不能在环形队列当中进行指针操作，当start_pos > end_pos时，可能会有越界情况...
	// 所以，一定要用接口读取完整的数据包之后，再进行操作；如果用指针，一旦发生回还，就会错误...
	/////////////////////////////////////////////////////////////////////////////////////////////////
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacketBuffer[nPerPackSize] = {0};
	circlebuf_peek_front(&m_circle, szPacketBuffer, nPerPackSize);
	lpFrontHeader = (rtp_hdr_t*)szPacketBuffer;
	// 如果要补充的数据包序号比最小序号还要小 => 没有找到，直接返回...
	if( rtpLose.lose_seq < lpFrontHeader->seq ) {
		TRACE("[Student-Pusher] Supply Error => lose: %lu, min: %lu\n", rtpLose.lose_seq, lpFrontHeader->seq);
		return;
	}
	ASSERT( rtpLose.lose_seq >= lpFrontHeader->seq );
	// 注意：环形队列当中的序列号一定是连续的...
	// 两者之差就是要发送数据包的头指针位置...
	nSendPos = (rtpLose.lose_seq - lpFrontHeader->seq) * nPerPackSize;
	// 如果补包位置大于或等于环形队列长度 => 补包越界...
	if( nSendPos >= m_circle.size ) {
		TRACE("[Student-Pusher] Supply Error => Position Excessed\n");
		return;
	}
	////////////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：不能用简单的指针操作，环形队列可能会回还，必须用接口 => 从指定相对位置拷贝指定长度数据...
	// 获取将要发送数据包的包头位置和有效数据长度...
	////////////////////////////////////////////////////////////////////////////////////////////////////
	memset(szPacketBuffer, 0, nPerPackSize);
	circlebuf_read(&m_circle, nSendPos, szPacketBuffer, nPerPackSize);
	lpSendHeader = (rtp_hdr_t*)szPacketBuffer;
	// 如果找到的序号位置不对，直接返回...
	if( lpSendHeader->seq != rtpLose.lose_seq ) {
		TRACE("[Student-Pusher] Supply Error => Seq: %lu, Find: %lu\n", rtpLose.lose_seq, lpSendHeader->seq);
		return;
	}
	// 获取有效的数据区长度 => 包头 + 数据...
	nSendSize = sizeof(rtp_hdr_t) + lpSendHeader->psize;
	// 调用套接字接口，直接发送RTP数据包...
	theErr = m_lpUDPSocket->SendTo((void*)lpSendHeader, nSendSize);
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	// 打印已经发送补包信息...
	uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	TRACE("[Student-Pusher] Time: %lu ms, Supply Send => Seq: %lu, TS: %lu, Type: %d, Slice: %d\n", now_ms, lpSendHeader->seq, lpSendHeader->ts, lpSendHeader->pt, lpSendHeader->psize);
}

void CUDPSendThread::doSendPacket()
{
	OSMutexLocker theLock(&m_Mutex);
	// 如果环形队列没有可发送数据，直接返回...
	if( m_circle.size <= 0 || m_lpUDPSocket == NULL )
		return;
	// 如果要发送序列号比打包序列号还要大 => 没有数据包可以发送...
	if( m_nCurSendSeq > m_nCurPackSeq )
		return;
	// 取出最前面的RTP数据包，但不从环形队列中移除 => 目的是给接收端补包用...
	GM_Error    theErr = GM_NoErr;
	rtp_hdr_t * lpFrontHeader = NULL;
	rtp_hdr_t * lpSendHeader = NULL;
	int nSendPos = 0, nSendSize = 0;
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：千万不能在环形队列当中进行指针操作，当start_pos > end_pos时，可能会有越界情况...
	// 所以，一定要用接口读取完整的数据包之后，再进行操作；如果用指针，一旦发生回还，就会错误...
	/////////////////////////////////////////////////////////////////////////////////////////////////
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacketBuffer[nPerPackSize] = {0};
	circlebuf_peek_front(&m_circle, szPacketBuffer, nPerPackSize);
	// 计算环形队列中最前面数据包的头指针 => 最小序号...
	lpFrontHeader = (rtp_hdr_t*)szPacketBuffer;
	// 第一次发包 或 发包序号太小 => 使用最前面包的序列号...
	if((m_nCurSendSeq <= 0) || (m_nCurSendSeq < lpFrontHeader->seq)) {
		m_nCurSendSeq = lpFrontHeader->seq;
	}
	/////////////////////////////////////////////////////////////////////////////////
	// 环形队列最小序号 => min_id => lpFrontHeader->seq
	// 环形队列最大序号 => max_id => m_nCurPackSeq
	/////////////////////////////////////////////////////////////////////////////////
	// 将要发送的数据包序号不能小于最前面包的序列号...
	ASSERT( m_nCurSendSeq >= lpFrontHeader->seq );
	ASSERT( m_nCurSendSeq <= m_nCurPackSeq );
	// 两者之差就是要发送数据包的头指针位置...
	nSendPos = (m_nCurSendSeq - lpFrontHeader->seq) * nPerPackSize;
	////////////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：不能用简单的指针操作，环形队列可能会回还，必须用接口 => 从指定相对位置拷贝指定长度数据...
	// 获取将要发送数据包的包头位置和有效数据长度...
	////////////////////////////////////////////////////////////////////////////////////////////////////
	memset(szPacketBuffer, 0, nPerPackSize);
	circlebuf_read(&m_circle, nSendPos, szPacketBuffer, nPerPackSize);
	lpSendHeader = (rtp_hdr_t*)szPacketBuffer;
	// 如果要发送的数据位置越界或无效，直接返回...
	if( lpSendHeader == NULL || lpSendHeader->seq <= 0 )
		return;
	nSendSize = sizeof(rtp_hdr_t) + lpSendHeader->psize;

	// 不丢包 => 调用套接字接口，直接发送RTP数据包...
	theErr = m_lpUDPSocket->SendTo((void*)lpSendHeader, nSendSize);
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;

	/////////////////////////////////////////////////////////////////////////////////
	// 实验：随机丢包...
	/////////////////////////////////////////////////////////////////////////////////
	/*if( m_nCurSendSeq % 3 != 2 ) {
		// 调用套接字接口，直接发送RTP数据包...
		theErr = m_lpUDPSocket->SendTo((void*)lpSendHeader, nSendSize);
		(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	}*/
	/////////////////////////////////////////////////////////////////////////////////

	// 成功发送数据包 => 累加发送序列号...
	++m_nCurSendSeq;
	// 修改休息状态 => 已经有发包，不能休息...
	m_bNeedSleep = false;

	// 打印调试信息 => 刚刚发送的数据包...
	//int nZeroSize = DEF_MTU_SIZE - lpSendHeader->psize;
	//TRACE("[Student-Pusher] Seq: %lu, TS: %lu, Type: %d, pst: %d, ped: %d, Slice: %d, Zero: %d\n", lpSendHeader->seq, lpSendHeader->ts, lpSendHeader->pt, lpSendHeader->pst, lpSendHeader->ped, lpSendHeader->psize, nZeroSize);
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
	// 注意：这里不用打印错误信息，没有收到数据就立即返回...
	if( theErr != GM_NoErr || outRecvLen <= 0 )
		return;
	// 修改休息状态 => 已经成功收包，不能休息...
	m_bNeedSleep = false;
 
	// 获取第一个字节的高4位，得到数据包类型...
    uint8_t ptTag = (ioBuffer[0] >> 4) & 0x0F;

	// 对收到命令包进行类型分发...
	switch( ptTag )
	{
	case PT_TAG_CREATE:  this->doProcServerCreate(ioBuffer, outRecvLen); break;
	case PT_TAG_HEADER:  this->doProcServerHeader(ioBuffer, outRecvLen); break;
	case PT_TAG_READY:   this->doProcServerReady(ioBuffer, outRecvLen);  break;
	case PT_TAG_RELOAD:  this->doProcServerReload(ioBuffer, outRecvLen); break;

	case PT_TAG_DETECT:	 this->doTagDetectProcess(ioBuffer, outRecvLen); break;
	case PT_TAG_SUPPLY:  this->doTagSupplyProcess(ioBuffer, outRecvLen); break;
	}
}

void CUDPSendThread::doProcServerCreate(char * lpBuffer, int inRecvLen)
{
	// 通过 rtp_hdr_t 做为载体发送过来的...
	if( m_lpUDPSocket == NULL || lpBuffer == NULL || inRecvLen <= 0 || inRecvLen < sizeof(rtp_hdr_t) )
		return;
	// 获取数据包结构体...
	rtp_hdr_t rtpHdr = {0};
	memcpy(&rtpHdr, lpBuffer, sizeof(rtpHdr));
	// 判断数据包的有效性 => 必须是服务器反馈的 Create 命令...
	if( rtpHdr.tm != TM_TAG_SERVER || rtpHdr.id != ID_TAG_SERVER || rtpHdr.pt != PT_TAG_CREATE )
		return;
	// 修改命令状态 => 开始发送序列头...
	m_nCmdState = kCmdSendHeader;
	// 打印收到服务器反馈的创建命令包...
	uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	TRACE("[Student-Pusher] Time: %lu ms, Recv Create from Server\n", now_ms);
}

void CUDPSendThread::doProcServerHeader(char * lpBuffer, int inRecvLen)
{
	// 通过 rtp_hdr_t 做为载体发送过来的...
	if( m_lpUDPSocket == NULL || lpBuffer == NULL || inRecvLen <= 0 || inRecvLen < sizeof(rtp_hdr_t) )
		return;
	// 获取数据包结构体...
	rtp_hdr_t rtpHdr = {0};
	memcpy(&rtpHdr, lpBuffer, sizeof(rtpHdr));
	// 判断数据包的有效性 => 必须是服务器反馈的序列头命令...
	if( rtpHdr.tm != TM_TAG_SERVER || rtpHdr.id != ID_TAG_SERVER || rtpHdr.pt != PT_TAG_HEADER )
		return;
	// 修改命令状态 => 开始接收观看端准备就绪命令...
	m_nCmdState = kCmdWaitReady;
	// 打印收到服务器反馈的序列头命令包...
	uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	TRACE("[Student-Pusher] Time: %lu ms, Recv Header from Server\n", now_ms);
}

void CUDPSendThread::doProcServerReady(char * lpBuffer, int inRecvLen)
{
	if( m_lpUDPSocket == NULL || lpBuffer == NULL || inRecvLen <= 0 || inRecvLen < sizeof(rtp_ready_t) )
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
	// 修改命令状态 => 可以进行音视频打包发送数据了...
	m_nCmdState = kCmdSendAVPack;
	// 保存老师观看端发送的准备就绪数据包内容...
	memcpy(&m_rtp_ready, lpBuffer, sizeof(m_rtp_ready));
	// 打印收到准备就绪命令包...
	uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	TRACE("[Student-Pusher] Time: %lu ms, Recv Ready from %lu:%d\n", now_ms, m_rtp_ready.recvAddr, m_rtp_ready.recvPort);
	// 立即反馈给老师观看者 => 准备就绪包已经收到，不要再发了...
	rtp_ready_t rtpReady = {0};
	rtpReady.tm = TM_TAG_STUDENT;
	rtpReady.id = ID_TAG_PUSHER;
	rtpReady.pt = PT_TAG_READY;
	// 调用套接字接口，直接发送RTP数据包...
	GM_Error theErr = m_lpUDPSocket->SendTo(&rtpReady, sizeof(rtpReady));
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
}
//
// 处理服务器发送过来的重建命令...
void CUDPSendThread::doProcServerReload(char * lpBuffer, int inRecvLen)
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
		uint32_t cur_time_sec = (uint32_t)(CUtilTool::os_gettime_ns()/1000000000);
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
	m_rtp_reload.reload_time = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	++m_rtp_reload.reload_count;
	// 打印收到服务器重建命令...
	uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	TRACE("[Student-Pusher] Time: %lu ms, Server Reload Count: %d\n", now_ms, m_rtp_reload.reload_count);
	// 重置相关命令包...
	memset(&m_rtp_ready, 0, sizeof(m_rtp_ready));
	// 清空补包集合队列...
	m_MapLose.clear();
	// 释放环形队列空间...
	circlebuf_free(&m_circle);
	// 初始化环形队列，预分配空间...
	circlebuf_init(&m_circle);
	circlebuf_reserve(&m_circle, DEF_CIRCLE_SIZE);
	// 重置相关变量...
	m_nCmdState = kCmdSendCreate;
	m_next_create_ns = -1;
	m_next_header_ns = -1;
	m_next_detect_ns = -1;
	m_nCurPackSeq = 0;
	m_nCurSendSeq = 0;
	// 重新开始探测网络...
	m_rtp_detect.tsSrc = 0;
	m_rtp_detect.dtNum = 0;
	m_rtt_var_ms = -1;
	m_rtt_ms = -1;
}

void CUDPSendThread::doTagSupplyProcess(char * lpBuffer, int inRecvLen)
{
	if( m_lpUDPSocket == NULL || lpBuffer == NULL || inRecvLen <= 0 || inRecvLen < sizeof(rtp_supply_t) )
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
	// 获取补包命令包内容...
	rtp_supply_t rtpSupply = {0};
	int nHeadSize = sizeof(rtp_supply_t);
	memcpy(&rtpSupply, lpBuffer, nHeadSize);
	if( (rtpSupply.suSize <= 0) || ((nHeadSize + rtpSupply.suSize) != inRecvLen) )
		return;
	// 获取需要补包的序列号，加入到补包队列当中...
	char * lpDataPtr = lpBuffer + nHeadSize;
	int    nDataSize = rtpSupply.suSize;
	while( nDataSize > 0 ) {
		uint32_t   nLoseSeq = 0;
		rtp_lose_t rtpLose = {0};
		// 获取补包序列号...
		memcpy(&nLoseSeq, lpDataPtr, sizeof(int));
		// 如果序列号已经存在，增加补包次数，不存在，增加新记录...
		if( m_MapLose.find(nLoseSeq) != m_MapLose.end() ) {
			rtp_lose_t & theFind = m_MapLose[nLoseSeq];
			theFind.lose_seq = nLoseSeq;
			++theFind.resend_count;
		} else {
			rtpLose.lose_seq = nLoseSeq;
			rtpLose.resend_time = (uint32_t)(CUtilTool::os_gettime_ns() / 1000000);
			m_MapLose[rtpLose.lose_seq] = rtpLose;
		}
		// 移动数据区指针位置...
		lpDataPtr += sizeof(int);
		nDataSize -= sizeof(int);
	}
	// 打印已收到补包命令...
	uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	TRACE("[Student-Pusher] Time: %lu ms, Supply Recv => Count: %d\n", now_ms, rtpSupply.suSize / sizeof(int));
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
		// 先将收到探测数据包原样返回给服务器端...
		theErr = m_lpUDPSocket->SendTo(lpBuffer, inRecvLen);
		// 再处理观看端已经收到的最大连续包号...
		rtp_detect_t * lpDetect = (rtp_detect_t*)lpBuffer;
		if( lpDetect->maxConSeq <= 0 || m_circle.size <= 0 )
			return;
		// 先找到环形队列中最前面数据包的头指针 => 最小序号...
		rtp_hdr_t * lpFrontHeader = NULL;
		/////////////////////////////////////////////////////////////////////////////////////////////////
		// 注意：千万不能在环形队列当中进行指针操作，当start_pos > end_pos时，可能会有越界情况...
		// 所以，一定要用接口读取完整的数据包之后，再进行操作；如果用指针，一旦发生回还，就会错误...
		/////////////////////////////////////////////////////////////////////////////////////////////////
		const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
		static char szPacketBuffer[nPerPackSize] = {0};
		circlebuf_peek_front(&m_circle, szPacketBuffer, nPerPackSize);
		lpFrontHeader = (rtp_hdr_t*)szPacketBuffer;
		// 如果要删除的数据包序号比最小序号还要小 => 数据已经删除了，直接返回...
		if( lpDetect->maxConSeq < lpFrontHeader->seq )
			return;
		// 注意：当前已发送包号是一个超前包号，是指下一个要发送的包号...
		// 观看端收到的最大连续包号相等或大于当前已发送包号 => 直接返回...
		if( lpDetect->maxConSeq >= m_nCurSendSeq )
			return;
		// 注意：环形队列当中的序列号一定是连续的...
		// 注意：观看端收到的最大连续包号一定比当前已发送包号小...
		// 两者之差加1就是要删除的数据长度 => 要包含最大连续包本身的删除...
		uint32_t nPopSize = (lpDetect->maxConSeq - lpFrontHeader->seq + 1) * nPerPackSize;
		circlebuf_pop_front(&m_circle, NULL, nPopSize);
		// 注意：环形队列当中的数据块大小是连续的，是一样大的...
		// 打印环形队列删除结果，计算环形队列剩余的数据包个数...
		//uint32_t nRemainCount = m_circle.size / nPackSize;
		//uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
		//TRACE( "[Student-Pusher] Time: %lu ms, Recv Detect MaxConSeq: %lu, MinSeq: %lu, CurSendSeq: %lu, CurPackSeq: %lu, Circle: %lu\n", 
		//		now_ms, lpDetect->maxConSeq, lpFrontHeader->seq, m_nCurSendSeq, m_nCurPackSeq, nRemainCount );
		return;
	}
	// 如果是 学生推流端 自己发出的探测包，计算网络延时...
	if( tmTag == TM_TAG_STUDENT && idTag == ID_TAG_PUSHER ) {
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
		if( m_rtt_var_ms < 0 ) { m_rtt_var_ms = abs(m_rtt_ms - keep_rtt); }
		else { m_rtt_var_ms = (m_rtt_var_ms * 3 + abs(m_rtt_ms - keep_rtt)) / 4; }
		// 打印探测结果 => 探测序号 | 网络延时(毫秒)...
		uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
		//TRACE("[Student-Pusher] Time: %lu ms, Recv Detect dtNum: %d, rtt: %d ms, rtt_var: %d ms\n", now_ms, rtpDetect.dtNum, m_rtt_ms, m_rtt_var_ms);
	}
}
///////////////////////////////////////////////////////
// 注意：没有发包，也没有收包，需要进行休息...
///////////////////////////////////////////////////////
void CUDPSendThread::doSleepTo()
{
	// 如果不能休息，直接返回...
	if( !m_bNeedSleep )
		return;
	// 计算要休息的时间 => 最大休息毫秒数...
	uint64_t delta_ns = MAX_SLEEP_MS * 1000000;
	uint64_t cur_time_ns = CUtilTool::os_gettime_ns();
	// 调用系统工具函数，进行sleep休息...
	CUtilTool::os_sleepto_ns(cur_time_ns + delta_ns);
}