
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
	// ��ʼ������״̬...
	m_nCmdState = kCmdSendCreate;
	// ��ʼ��rtp����ͷ�ṹ��...
	memset(&m_rtp_reload, 0, sizeof(m_rtp_reload));
	memset(&m_rtp_detect, 0, sizeof(m_rtp_detect));
	memset(&m_rtp_create, 0, sizeof(m_rtp_create));
	memset(&m_rtp_delete, 0, sizeof(m_rtp_delete));
	memset(&m_rtp_header, 0, sizeof(m_rtp_header));
	memset(&m_rtp_ready, 0, sizeof(m_rtp_ready));
	// ��ʼ�����ζ��У�Ԥ����ռ�...
	circlebuf_init(&m_circle);
	circlebuf_reserve(&m_circle, DEF_CIRCLE_SIZE);
	// �����ն����ͺͽṹ������ => m_rtp_ready => �ȴ��������...
	m_rtp_detect.tm = m_rtp_create.tm = m_rtp_delete.tm = m_rtp_header.tm = TM_TAG_STUDENT;
	m_rtp_detect.id = m_rtp_create.id = m_rtp_delete.id = m_rtp_header.id = ID_TAG_PUSHER;
	m_rtp_detect.pt = PT_TAG_DETECT;
	m_rtp_create.pt = PT_TAG_CREATE;
	m_rtp_delete.pt = PT_TAG_DELETE;
	m_rtp_header.pt = PT_TAG_HEADER;
	// ��䷿��ź�ֱ��ͨ����...
	m_rtp_create.roomID = nDBRoomID;
	m_rtp_create.liveID = nDBCameraID;
	m_rtp_delete.roomID = nDBRoomID;
	m_rtp_delete.liveID = nDBCameraID;
}

CUDPSendThread::~CUDPSendThread()
{
	// δ֪״̬����ֹ��������...
	m_nCmdState = kCmdUnkownState;
	// ֹͣ�̣߳��ȴ��˳�...
	this->StopAndWaitForThread();
	// �ر�UDPSocket����...
	this->CloseSocket();
	// �ͷŻ��ζ��пռ�...
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
	// ������Ƶ��־...
	m_rtp_header.hasVideo = true;
	// ���洫�ݹ����Ĳ�����Ϣ...
	m_rtp_header.fpsNum = nFPS;
	m_rtp_header.picWidth = nWidth;
	m_rtp_header.picHeight = nHeight;
	m_rtp_header.spsSize = inSPS.size();
	m_rtp_header.ppsSize = inPPS.size();
	m_strSPS = inSPS; m_strPPS = inPPS;
	// �߳�û��������ֱ������...
	if( this->GetThreadHandle() != NULL )
		return true;
	return ((this->InitThread() != GM_NoErr) ? false : true);
}

BOOL CUDPSendThread::InitAudio(int nRateIndex, int nChannelNum)
{
	OSMutexLocker theLock(&m_Mutex);
	// ������Ƶ��־...
	m_rtp_header.hasAudio = true;
	// �������������������...
	m_rtp_header.rateIndex = nRateIndex;
	m_rtp_header.channelNum = nChannelNum;
	// �߳�û��������ֱ������...
	if( this->GetThreadHandle() != NULL )
		return true;
	return ((this->InitThread() != GM_NoErr) ? false : true);
}

GM_Error CUDPSendThread::InitThread()
{
	// ���ȣ��ر�socket...
	this->CloseSocket();
	// ���½�socket...
	ASSERT( m_lpUDPSocket == NULL );
	m_lpUDPSocket = new UDPSocket();
	///////////////////////////////////////////////////////
	// ע�⣺OpenĬ���Ѿ��趨Ϊ�첽ģʽ...
	///////////////////////////////////////////////////////
	// ����UDP,��������Ƶ����,����ָ��...
	GM_Error theErr = GM_NoErr;
	theErr = m_lpUDPSocket->Open();
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
		return theErr;
	}
	// �����ظ�ʹ�ö˿�...
	m_lpUDPSocket->ReuseAddr();
	// ���÷��ͺͽ��ջ�����...
	m_lpUDPSocket->SetSocketSendBufSize(128 * 1024);
	m_lpUDPSocket->SetSocketRecvBufSize(128 * 1024);
	// ����TTL���紩Խ��ֵ...
	m_lpUDPSocket->SetTtl(32);
	// ��ȡ��������ַ��Ϣ => ����������Ϣ����һ��IPV4����...
	//LPCTSTR lpszAddr = "192.168.1.70";
	LPCTSTR lpszAddr = DEF_UDP_HOME;
	hostent * lpHost = gethostbyname(lpszAddr);
	if( lpHost != NULL && lpHost->h_addr_list != NULL ) {
		lpszAddr = inet_ntoa(*(in_addr*)lpHost->h_addr_list[0]);
	}
	// �����������ַ����SendTo����......
	m_lpUDPSocket->SetRemoteAddr(lpszAddr, DEF_UDP_PORT);
	// �����鲥�����߳�...
	this->Start();
	// ����ִ�н��...
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
	// �ж��߳��Ƿ��Ѿ��˳� �� ���ζ��л�����Ч...
	if( this->IsStopRequested() || m_circle.capacity <= 0 ) {
		uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
		TRACE("[Student-Pusher] Time: %lu ms, Error => Send Thread has been stoped\n", now_ms);
		return;
	}
	// �������֡�ĳ���Ϊ0����ӡ����ֱ�ӷ���...
	if( inFrame.strData.size() <= 0 ) {
		uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
		TRACE("[Student-Pusher] Time: %lu ms, Error => Input Frame Size is Zero\n", now_ms);
		return;
	}
	/////////////////////////////////////////////////////////////////////
	// ע�⣺ֻ�е����ն�׼����֮�󣬲��ܿ�ʼ��������...
	/////////////////////////////////////////////////////////////////////
	// ������ն�û��׼���ã�ֱ�ӷ��� => �յ�׼����������״̬Ϊ���״̬...
	if( m_nCmdState != kCmdSendAVPack || m_rtp_ready.tm != TM_TAG_TEACHER || m_rtp_ready.recvPort <= 0 )
		return;
	// �������Ƶ����һ֡��������Ƶ�ؼ�֡...
	if( m_rtp_header.hasVideo && m_nCurPackSeq <= 0 ) {
		if( inFrame.typeFlvTag != PT_TAG_VIDEO || !inFrame.is_keyframe )
			return;
		ASSERT( inFrame.typeFlvTag == PT_TAG_VIDEO && inFrame.is_keyframe );
	}
	// ��ӡ��һ֡��Ϣ => ��ʼ������Ƶ���ݰ���...
	/*if( m_nCurPackSeq <= 0 ) {
		uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
		TRACE( "[Student-Pusher] Time: %lu ms, First Frame => Type: %d, KeyFrame: %d, TS: %lu, Size: %d\n", 
				now_ms, inFrame.typeFlvTag, inFrame.is_keyframe, inFrame.dwSendTime, inFrame.strData.size() );
	}*/
	// ��ӡ���е�����Ƶ����֡...
	//uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	//TRACE( "[Student-Pusher] Time: %lu ms, Frame => Type: %d, Key: %d, PTS: %lu, Size: %d\n", 
	//		now_ms, inFrame.typeFlvTag, inFrame.is_keyframe, inFrame.dwSendTime, inFrame.strData.size() );
	//DoSaveSendFile(inFrame.dwSendTime, inFrame.typeFlvTag, inFrame.is_keyframe, inFrame.strData);

	// ����RTP��ͷ�ṹ��...
	rtp_hdr_t rtpHeader = {0};
	rtpHeader.tm  = TM_TAG_STUDENT;
	rtpHeader.id  = ID_TAG_PUSHER;
	rtpHeader.pt  = inFrame.typeFlvTag;
	rtpHeader.pk  = inFrame.is_keyframe;
	rtpHeader.ts  = inFrame.dwSendTime;
	// ������Ҫ��Ƭ�ĸ��� => ��Ҫע���ۼ����һ����Ƭ...
	int nSliceSize = DEF_MTU_SIZE;
	int nFrameSize = inFrame.strData.size();
	int nSliceNum = nFrameSize / DEF_MTU_SIZE;
	char * lpszFramePtr = (char*)inFrame.strData.c_str();
	nSliceNum += ((nFrameSize % DEF_MTU_SIZE) ? 1 : 0);
	int nEndSize = nFrameSize - (nSliceNum - 1) * DEF_MTU_SIZE;
	// ����ѭ��ѹ�뻷�ζ��е���...
	for(int i = 0; i < nSliceNum; ++i) {
		rtpHeader.seq = ++m_nCurPackSeq; // �ۼӴ�����к�...
		rtpHeader.pst = ((i == 0) ? true : false); // �Ƿ��ǵ�һ����Ƭ...
		rtpHeader.ped = ((i+1 == nSliceNum) ? true: false); // �Ƿ������һ����Ƭ...
		rtpHeader.psize = rtpHeader.ped ? nEndSize : DEF_MTU_SIZE; // ��������һ����Ƭ��ȡ����ֵ(����ȡ�����������MTU�����������)������ȡMTUֵ...
		ASSERT( rtpHeader.psize > 0 && rtpHeader.psize <= DEF_MTU_SIZE );
		// ����������ݳ���...
		int nZeroSize = DEF_MTU_SIZE - rtpHeader.psize;
		// �����Ƭ��������ͷָ��...
		LPCTSTR lpszSlicePtr = lpszFramePtr + i * DEF_MTU_SIZE;
		// ���뻷�ζ��е��� => rtp_hdr_t + slice + Zero => 12 + 800 => 812
		circlebuf_push_back(&m_circle, &rtpHeader, sizeof(rtpHeader));
		circlebuf_push_back(&m_circle, lpszSlicePtr, rtpHeader.psize);
		// ��������������ݣ���֤�������Ǳ���һ��MTU��Ԫ��С...
		if( nZeroSize > 0 ) {
			circlebuf_push_back_zero(&m_circle, nZeroSize);
		}
		// ��ӡ������Ϣ...
		//uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
		//TRACE( "[Student-Pusher] Time: %lu ms, Seq: %lu, Type: %d, Key: %d, Size: %d, TS: %lu\n", now_ms, 
		//		rtpHeader.seq, rtpHeader.pt, rtpHeader.pk, rtpHeader.psize, rtpHeader.ts);
		//DoSaveSendSeq(rtpHeader.seq, rtpHeader.psize, rtpHeader.pst, rtpHeader.ped, rtpHeader.ts);
	}
}

void CUDPSendThread::Entry()
{
	while( !this->IsStopRequested() ) {
		// ������Ϣ��־ => ֻҪ�з������հ��Ͳ�����Ϣ...
		m_bNeedSleep = true;
		// ���͹ۿ�����Ҫ�Ķ�ʧ���ݰ�...
		this->doSendLosePacket();
		// ���ʹ��������ֱ��ͨ�������...
		this->doSendCreateCmd();
		// ��������ͷ�����...
		this->doSendHeaderCmd();
		// ����̽�������...
		this->doSendDetectCmd();
		// ����һ����װ�õ�RTP���ݰ�...
		this->doSendPacket();
		// ����һ������ķ�����������...
		this->doRecvPacket();
		// �ȴ����ͻ������һ�����ݰ�...
		this->doSleepTo();
	}
	// ֻ����һ��ɾ�������...
	this->doSendDeleteCmd();
}

void CUDPSendThread::doSendDeleteCmd()
{
	OSMutexLocker theLock(&m_Mutex);
	GM_Error theErr = GM_NoErr;
	if( m_lpUDPSocket == NULL )
		return;
	// �׽�����Ч��ֱ�ӷ���ɾ������...
	ASSERT( m_lpUDPSocket != NULL );
	theErr = m_lpUDPSocket->SendTo((void*)&m_rtp_delete, sizeof(m_rtp_delete));
	// ��ӡ�ѷ���ɾ�������...
	uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	TRACE("[Student-Pusher] Time: %lu ms, Send Delete RoomID: %lu, LiveID: %d\n", now_ms, m_rtp_delete.roomID, m_rtp_delete.liveID);
}

void CUDPSendThread::doSendCreateCmd()
{
	OSMutexLocker theLock(&m_Mutex);
	// �������״̬���Ǵ���������������ֱ�ӷ���...
	if( m_nCmdState != kCmdSendCreate )
		return;
	// ÿ��100���뷢�ʹ�������� => ����ת�����з���...
	int64_t cur_time_ns = CUtilTool::os_gettime_ns();
	int64_t period_ns = 100 * 1000000;
	// �������ʱ�仹û����ֱ�ӷ���...
	if( m_next_create_ns > cur_time_ns )
		return;
	ASSERT( cur_time_ns >= m_next_create_ns );
	// ���ȣ�����һ��������������...
	GM_Error theErr = m_lpUDPSocket->SendTo((void*)&m_rtp_create, sizeof(m_rtp_create));
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	// ��ӡ�ѷ��ʹ�������� => ��һ�����п���û�з��ͳ�ȥ��Ҳ��������...
	uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	TRACE("[Student-Pusher] Time: %lu ms, Send Create RoomID: %lu, LiveID: %d\n", now_ms, m_rtp_create.roomID, m_rtp_create.liveID);
	// �����´η��ʹ��������ʱ���...
	m_next_create_ns = CUtilTool::os_gettime_ns() + period_ns;
	// �޸���Ϣ״̬ => �Ѿ��з�����������Ϣ...
	m_bNeedSleep = false;
}

void CUDPSendThread::doSendHeaderCmd()
{
	OSMutexLocker theLock(&m_Mutex);
	// �������״̬��������ͷ������������ֱ�ӷ���...
	if( m_nCmdState != kCmdSendHeader )
		return;
	// ÿ��100���뷢������ͷ����� => ����ת�����з���...
	int64_t cur_time_ns = CUtilTool::os_gettime_ns();
	int64_t period_ns = 100 * 1000000;
	// �������ʱ�仹û����ֱ�ӷ���...
	if( m_next_header_ns > cur_time_ns )
		return;
	ASSERT( cur_time_ns >= m_next_header_ns );
	// Ȼ�󣬷�������ͷ�����...
	string strSeqHeader;
	strSeqHeader.append((char*)&m_rtp_header, sizeof(m_rtp_header));
	// ����SPS����������...
	if( m_strSPS.size() > 0 ) {
		strSeqHeader.append(m_strSPS);
	}
	// ����PPS����������...
	if( m_strPPS.size() > 0 ) {
		strSeqHeader.append(m_strPPS);
	}
	// �����׽��ֽӿڣ�ֱ�ӷ���RTP���ݰ�...
	GM_Error theErr = m_lpUDPSocket->SendTo((void*)strSeqHeader.c_str(), strSeqHeader.size());
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	// ��ӡ�ѷ�������ͷ�����...
	uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	TRACE("[Student-Pusher] Time: %lu ms, Send Header SPS: %lu, PPS: %d\n", now_ms, m_strSPS.size(), m_strPPS.size());
	// �����´η��ʹ��������ʱ���...
	m_next_header_ns = CUtilTool::os_gettime_ns() + period_ns;
	// �޸���Ϣ״̬ => �Ѿ��з�����������Ϣ...
	m_bNeedSleep = false;
}

void CUDPSendThread::doSendDetectCmd()
{
	OSMutexLocker theLock(&m_Mutex);
	// ÿ��1�뷢��һ��̽������� => ����ת�����з���...
	int64_t cur_time_ns = CUtilTool::os_gettime_ns();
	int64_t period_ns = 1000 * 1000000;
	// ��һ��̽�����ʱ1/3�뷢�� => �����һ��̽����ȵ�����������������ؽ�����...
	if( m_next_detect_ns < 0 ) { 
		m_next_detect_ns = cur_time_ns + period_ns / 3;
	}
	// �������ʱ�仹û����ֱ�ӷ���...
	if( m_next_detect_ns > cur_time_ns )
		return;
	ASSERT( cur_time_ns >= m_next_detect_ns );
	// ��̽�����ʱ��ת���ɺ��룬�ۼ�̽�������...
	m_rtp_detect.tsSrc  = (uint32_t)(cur_time_ns / 1000000);
	m_rtp_detect.dtNum += 1;
	// ���ýӿڷ���̽�������...
	GM_Error theErr = m_lpUDPSocket->SendTo((void*)&m_rtp_detect, sizeof(m_rtp_detect));
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	// ��ӡ�ѷ���̽�������...
	uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	//TRACE("[Student-Pusher] Time: %lu ms, Send Detect dtNum: %d\n", now_ms, m_rtp_detect.dtNum);
	// �����´η���̽�������ʱ���...
	m_next_detect_ns = CUtilTool::os_gettime_ns() + period_ns;
	// �޸���Ϣ״̬ => �Ѿ��з�����������Ϣ...
	m_bNeedSleep = false;
}

void CUDPSendThread::doSendLosePacket()
{
	OSMutexLocker theLock(&m_Mutex);
	if( m_lpUDPSocket == NULL )
		return;
	// �������϶���Ϊ�գ�ֱ�ӷ���...
	if( m_MapLose.size() <= 0 )
		return;
	// �ó�һ��������¼�������Ƿ��ͳɹ�����Ҫɾ�����������¼...
	// ����ۿ��ˣ�û���յ�������ݰ������ٴη��𲹰�����...
	GM_MapLose::iterator itorItem = m_MapLose.begin();
	rtp_lose_t rtpLose = itorItem->second;
	m_MapLose.erase(itorItem);
	// ������ζ���Ϊ�գ�ֱ�ӷ���...
	if( m_circle.size <= 0 )
		return;
	// ���ҵ����ζ�������ǰ�����ݰ���ͷָ�� => ��С���...
	GM_Error    theErr = GM_NoErr;
	rtp_hdr_t * lpFrontHeader = NULL;
	rtp_hdr_t * lpSendHeader = NULL;
	int nSendPos = 0, nSendSize = 0;
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺ǧ�����ڻ��ζ��е��н���ָ���������start_pos > end_posʱ�����ܻ���Խ�����...
	// ���ԣ�һ��Ҫ�ýӿڶ�ȡ���������ݰ�֮���ٽ��в����������ָ�룬һ�������ػ����ͻ����...
	/////////////////////////////////////////////////////////////////////////////////////////////////
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacketBuffer[nPerPackSize] = {0};
	circlebuf_peek_front(&m_circle, szPacketBuffer, nPerPackSize);
	lpFrontHeader = (rtp_hdr_t*)szPacketBuffer;
	// ���Ҫ��������ݰ���ű���С��Ż�ҪС => û���ҵ���ֱ�ӷ���...
	if( rtpLose.lose_seq < lpFrontHeader->seq ) {
		TRACE("[Student-Pusher] Supply Error => lose: %lu, min: %lu\n", rtpLose.lose_seq, lpFrontHeader->seq);
		return;
	}
	ASSERT( rtpLose.lose_seq >= lpFrontHeader->seq );
	// ע�⣺���ζ��е��е����к�һ����������...
	// ����֮�����Ҫ�������ݰ���ͷָ��λ��...
	nSendPos = (rtpLose.lose_seq - lpFrontHeader->seq) * nPerPackSize;
	// �������λ�ô��ڻ���ڻ��ζ��г��� => ����Խ��...
	if( nSendPos >= m_circle.size ) {
		TRACE("[Student-Pusher] Supply Error => Position Excessed\n");
		return;
	}
	////////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺�����ü򵥵�ָ����������ζ��п��ܻ�ػ��������ýӿ� => ��ָ�����λ�ÿ���ָ����������...
	// ��ȡ��Ҫ�������ݰ��İ�ͷλ�ú���Ч���ݳ���...
	////////////////////////////////////////////////////////////////////////////////////////////////////
	memset(szPacketBuffer, 0, nPerPackSize);
	circlebuf_read(&m_circle, nSendPos, szPacketBuffer, nPerPackSize);
	lpSendHeader = (rtp_hdr_t*)szPacketBuffer;
	// ����ҵ������λ�ò��ԣ�ֱ�ӷ���...
	if( lpSendHeader->seq != rtpLose.lose_seq ) {
		TRACE("[Student-Pusher] Supply Error => Seq: %lu, Find: %lu\n", rtpLose.lose_seq, lpSendHeader->seq);
		return;
	}
	// ��ȡ��Ч������������ => ��ͷ + ����...
	nSendSize = sizeof(rtp_hdr_t) + lpSendHeader->psize;
	// �����׽��ֽӿڣ�ֱ�ӷ���RTP���ݰ�...
	theErr = m_lpUDPSocket->SendTo((void*)lpSendHeader, nSendSize);
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	// ��ӡ�Ѿ����Ͳ�����Ϣ...
	uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	TRACE("[Student-Pusher] Time: %lu ms, Supply Send => Seq: %lu, TS: %lu, Type: %d, Slice: %d\n", now_ms, lpSendHeader->seq, lpSendHeader->ts, lpSendHeader->pt, lpSendHeader->psize);
}

void CUDPSendThread::doSendPacket()
{
	OSMutexLocker theLock(&m_Mutex);
	// ������ζ���û�пɷ������ݣ�ֱ�ӷ���...
	if( m_circle.size <= 0 || m_lpUDPSocket == NULL )
		return;
	// ���Ҫ�������кűȴ�����кŻ�Ҫ�� => û�����ݰ����Է���...
	if( m_nCurSendSeq > m_nCurPackSeq )
		return;
	// ȡ����ǰ���RTP���ݰ��������ӻ��ζ������Ƴ� => Ŀ���Ǹ����ն˲�����...
	GM_Error    theErr = GM_NoErr;
	rtp_hdr_t * lpFrontHeader = NULL;
	rtp_hdr_t * lpSendHeader = NULL;
	int nSendPos = 0, nSendSize = 0;
	/////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺ǧ�����ڻ��ζ��е��н���ָ���������start_pos > end_posʱ�����ܻ���Խ�����...
	// ���ԣ�һ��Ҫ�ýӿڶ�ȡ���������ݰ�֮���ٽ��в����������ָ�룬һ�������ػ����ͻ����...
	/////////////////////////////////////////////////////////////////////////////////////////////////
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacketBuffer[nPerPackSize] = {0};
	circlebuf_peek_front(&m_circle, szPacketBuffer, nPerPackSize);
	// ���㻷�ζ�������ǰ�����ݰ���ͷָ�� => ��С���...
	lpFrontHeader = (rtp_hdr_t*)szPacketBuffer;
	// ��һ�η��� �� �������̫С => ʹ����ǰ��������к�...
	if((m_nCurSendSeq <= 0) || (m_nCurSendSeq < lpFrontHeader->seq)) {
		m_nCurSendSeq = lpFrontHeader->seq;
	}
	/////////////////////////////////////////////////////////////////////////////////
	// ���ζ�����С��� => min_id => lpFrontHeader->seq
	// ���ζ��������� => max_id => m_nCurPackSeq
	/////////////////////////////////////////////////////////////////////////////////
	// ��Ҫ���͵����ݰ���Ų���С����ǰ��������к�...
	ASSERT( m_nCurSendSeq >= lpFrontHeader->seq );
	ASSERT( m_nCurSendSeq <= m_nCurPackSeq );
	// ����֮�����Ҫ�������ݰ���ͷָ��λ��...
	nSendPos = (m_nCurSendSeq - lpFrontHeader->seq) * nPerPackSize;
	////////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺�����ü򵥵�ָ����������ζ��п��ܻ�ػ��������ýӿ� => ��ָ�����λ�ÿ���ָ����������...
	// ��ȡ��Ҫ�������ݰ��İ�ͷλ�ú���Ч���ݳ���...
	////////////////////////////////////////////////////////////////////////////////////////////////////
	memset(szPacketBuffer, 0, nPerPackSize);
	circlebuf_read(&m_circle, nSendPos, szPacketBuffer, nPerPackSize);
	lpSendHeader = (rtp_hdr_t*)szPacketBuffer;
	// ���Ҫ���͵�����λ��Խ�����Ч��ֱ�ӷ���...
	if( lpSendHeader == NULL || lpSendHeader->seq <= 0 )
		return;
	nSendSize = sizeof(rtp_hdr_t) + lpSendHeader->psize;

	// ������ => �����׽��ֽӿڣ�ֱ�ӷ���RTP���ݰ�...
	theErr = m_lpUDPSocket->SendTo((void*)lpSendHeader, nSendSize);
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;

	/////////////////////////////////////////////////////////////////////////////////
	// ʵ�飺�������...
	/////////////////////////////////////////////////////////////////////////////////
	/*if( m_nCurSendSeq % 3 != 2 ) {
		// �����׽��ֽӿڣ�ֱ�ӷ���RTP���ݰ�...
		theErr = m_lpUDPSocket->SendTo((void*)lpSendHeader, nSendSize);
		(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	}*/
	/////////////////////////////////////////////////////////////////////////////////

	// �ɹ��������ݰ� => �ۼӷ������к�...
	++m_nCurSendSeq;
	// �޸���Ϣ״̬ => �Ѿ��з�����������Ϣ...
	m_bNeedSleep = false;

	// ��ӡ������Ϣ => �ոշ��͵����ݰ�...
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
	// ���ýӿڴ�������ȡ���ݰ� => �������첽�׽��֣����������� => ���ܴ���...
	theErr = m_lpUDPSocket->RecvFrom(&outRemoteAddr, &outRemotePort, ioBuffer, inBufLen, &outRecvLen);
	// ע�⣺���ﲻ�ô�ӡ������Ϣ��û���յ����ݾ���������...
	if( theErr != GM_NoErr || outRecvLen <= 0 )
		return;
	// �޸���Ϣ״̬ => �Ѿ��ɹ��հ���������Ϣ...
	m_bNeedSleep = false;
 
	// ��ȡ��һ���ֽڵĸ�4λ���õ����ݰ�����...
    uint8_t ptTag = (ioBuffer[0] >> 4) & 0x0F;

	// ���յ�������������ͷַ�...
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
	// ͨ�� rtp_hdr_t ��Ϊ���巢�͹�����...
	if( m_lpUDPSocket == NULL || lpBuffer == NULL || inRecvLen <= 0 || inRecvLen < sizeof(rtp_hdr_t) )
		return;
	// ��ȡ���ݰ��ṹ��...
	rtp_hdr_t rtpHdr = {0};
	memcpy(&rtpHdr, lpBuffer, sizeof(rtpHdr));
	// �ж����ݰ�����Ч�� => �����Ƿ����������� Create ����...
	if( rtpHdr.tm != TM_TAG_SERVER || rtpHdr.id != ID_TAG_SERVER || rtpHdr.pt != PT_TAG_CREATE )
		return;
	// �޸�����״̬ => ��ʼ��������ͷ...
	m_nCmdState = kCmdSendHeader;
	// ��ӡ�յ������������Ĵ��������...
	uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	TRACE("[Student-Pusher] Time: %lu ms, Recv Create from Server\n", now_ms);
}

void CUDPSendThread::doProcServerHeader(char * lpBuffer, int inRecvLen)
{
	// ͨ�� rtp_hdr_t ��Ϊ���巢�͹�����...
	if( m_lpUDPSocket == NULL || lpBuffer == NULL || inRecvLen <= 0 || inRecvLen < sizeof(rtp_hdr_t) )
		return;
	// ��ȡ���ݰ��ṹ��...
	rtp_hdr_t rtpHdr = {0};
	memcpy(&rtpHdr, lpBuffer, sizeof(rtpHdr));
	// �ж����ݰ�����Ч�� => �����Ƿ���������������ͷ����...
	if( rtpHdr.tm != TM_TAG_SERVER || rtpHdr.id != ID_TAG_SERVER || rtpHdr.pt != PT_TAG_HEADER )
		return;
	// �޸�����״̬ => ��ʼ���չۿ���׼����������...
	m_nCmdState = kCmdWaitReady;
	// ��ӡ�յ�����������������ͷ�����...
	uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	TRACE("[Student-Pusher] Time: %lu ms, Recv Header from Server\n", now_ms);
}

void CUDPSendThread::doProcServerReady(char * lpBuffer, int inRecvLen)
{
	if( m_lpUDPSocket == NULL || lpBuffer == NULL || inRecvLen <= 0 || inRecvLen < sizeof(rtp_ready_t) )
		return;
    // ͨ����һ���ֽڵĵ�2λ���ж��ն�����...
    uint8_t tmTag = lpBuffer[0] & 0x03;
    // ��ȡ��һ���ֽڵ���2λ���õ��ն����...
    uint8_t idTag = (lpBuffer[0] >> 2) & 0x03;
    // ��ȡ��һ���ֽڵĸ�4λ���õ����ݰ�����...
    uint8_t ptTag = (lpBuffer[0] >> 4) & 0x0F;
	// ������� ��ʦ�ۿ��� ������׼����������ֱ�ӷ���...
	if( tmTag != TM_TAG_TEACHER || idTag != ID_TAG_LOOKER )
		return;
	// �޸�����״̬ => ���Խ�������Ƶ�������������...
	m_nCmdState = kCmdSendAVPack;
	// ������ʦ�ۿ��˷��͵�׼���������ݰ�����...
	memcpy(&m_rtp_ready, lpBuffer, sizeof(m_rtp_ready));
	// ��ӡ�յ�׼�����������...
	uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	TRACE("[Student-Pusher] Time: %lu ms, Recv Ready from %lu:%d\n", now_ms, m_rtp_ready.recvAddr, m_rtp_ready.recvPort);
	// ������������ʦ�ۿ��� => ׼���������Ѿ��յ�����Ҫ�ٷ���...
	rtp_ready_t rtpReady = {0};
	rtpReady.tm = TM_TAG_STUDENT;
	rtpReady.id = ID_TAG_PUSHER;
	rtpReady.pt = PT_TAG_READY;
	// �����׽��ֽӿڣ�ֱ�ӷ���RTP���ݰ�...
	GM_Error theErr = m_lpUDPSocket->SendTo(&rtpReady, sizeof(rtpReady));
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
}
//
// ������������͹������ؽ�����...
void CUDPSendThread::doProcServerReload(char * lpBuffer, int inRecvLen)
{
	if( m_lpUDPSocket == NULL || lpBuffer == NULL || inRecvLen <= 0 || inRecvLen < sizeof(rtp_reload_t) )
		return;
    // ͨ����һ���ֽڵĵ�2λ���ж��ն�����...
    uint8_t tmTag = lpBuffer[0] & 0x03;
    // ��ȡ��һ���ֽڵ���2λ���õ��ն����...
    uint8_t idTag = (lpBuffer[0] >> 2) & 0x03;
    // ��ȡ��һ���ֽڵĸ�4λ���õ����ݰ�����...
    uint8_t ptTag = (lpBuffer[0] >> 4) & 0x0F;
	// ������Ƿ������˷��͵��ؽ����ֱ�ӷ���...
	if( tmTag != TM_TAG_SERVER || idTag != ID_TAG_SERVER )
		return;
	// ������ǵ�һ���ؽ����ؽ����������20������...
	if( m_rtp_reload.reload_time > 0 ) {
		uint32_t cur_time_sec = (uint32_t)(CUtilTool::os_gettime_ns()/1000000000);
		uint32_t load_time_sec = m_rtp_reload.reload_time / 1000;
		// ����ؽ�����������20�룬ֱ�ӷ���...
		if( (cur_time_sec - load_time_sec) < RELOAD_TIME_OUT )
			return;
	}
	// ������������ݵ��ؽ�����...
	m_rtp_reload.tm = tmTag;
	m_rtp_reload.id = idTag;
	m_rtp_reload.pt = ptTag;
	// ��¼�����ؽ���Ϣ...
	m_rtp_reload.reload_time = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	++m_rtp_reload.reload_count;
	// ��ӡ�յ��������ؽ�����...
	uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	TRACE("[Student-Pusher] Time: %lu ms, Server Reload Count: %d\n", now_ms, m_rtp_reload.reload_count);
	// ������������...
	memset(&m_rtp_ready, 0, sizeof(m_rtp_ready));
	// ��ղ������϶���...
	m_MapLose.clear();
	// �ͷŻ��ζ��пռ�...
	circlebuf_free(&m_circle);
	// ��ʼ�����ζ��У�Ԥ����ռ�...
	circlebuf_init(&m_circle);
	circlebuf_reserve(&m_circle, DEF_CIRCLE_SIZE);
	// ������ر���...
	m_nCmdState = kCmdSendCreate;
	m_next_create_ns = -1;
	m_next_header_ns = -1;
	m_next_detect_ns = -1;
	m_nCurPackSeq = 0;
	m_nCurSendSeq = 0;
	// ���¿�ʼ̽������...
	m_rtp_detect.tsSrc = 0;
	m_rtp_detect.dtNum = 0;
	m_rtt_var_ms = -1;
	m_rtt_ms = -1;
}

void CUDPSendThread::doTagSupplyProcess(char * lpBuffer, int inRecvLen)
{
	if( m_lpUDPSocket == NULL || lpBuffer == NULL || inRecvLen <= 0 || inRecvLen < sizeof(rtp_supply_t) )
		return;
    // ͨ����һ���ֽڵĵ�2λ���ж��ն�����...
    uint8_t tmTag = lpBuffer[0] & 0x03;
    // ��ȡ��һ���ֽڵ���2λ���õ��ն����...
    uint8_t idTag = (lpBuffer[0] >> 2) & 0x03;
    // ��ȡ��һ���ֽڵĸ�4λ���õ����ݰ�����...
    uint8_t ptTag = (lpBuffer[0] >> 4) & 0x0F;
	// ������� ��ʦ�ۿ��� ������׼����������ֱ�ӷ���...
	if( tmTag != TM_TAG_TEACHER || idTag != ID_TAG_LOOKER )
		return;
	// ��ȡ�������������...
	rtp_supply_t rtpSupply = {0};
	int nHeadSize = sizeof(rtp_supply_t);
	memcpy(&rtpSupply, lpBuffer, nHeadSize);
	if( (rtpSupply.suSize <= 0) || ((nHeadSize + rtpSupply.suSize) != inRecvLen) )
		return;
	// ��ȡ��Ҫ���������кţ����뵽�������е���...
	char * lpDataPtr = lpBuffer + nHeadSize;
	int    nDataSize = rtpSupply.suSize;
	while( nDataSize > 0 ) {
		uint32_t   nLoseSeq = 0;
		rtp_lose_t rtpLose = {0};
		// ��ȡ�������к�...
		memcpy(&nLoseSeq, lpDataPtr, sizeof(int));
		// ������к��Ѿ����ڣ����Ӳ��������������ڣ������¼�¼...
		if( m_MapLose.find(nLoseSeq) != m_MapLose.end() ) {
			rtp_lose_t & theFind = m_MapLose[nLoseSeq];
			theFind.lose_seq = nLoseSeq;
			++theFind.resend_count;
		} else {
			rtpLose.lose_seq = nLoseSeq;
			rtpLose.resend_time = (uint32_t)(CUtilTool::os_gettime_ns() / 1000000);
			m_MapLose[rtpLose.lose_seq] = rtpLose;
		}
		// �ƶ�������ָ��λ��...
		lpDataPtr += sizeof(int);
		nDataSize -= sizeof(int);
	}
	// ��ӡ���յ���������...
	uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	TRACE("[Student-Pusher] Time: %lu ms, Supply Recv => Count: %d\n", now_ms, rtpSupply.suSize / sizeof(int));
}

void CUDPSendThread::doTagDetectProcess(char * lpBuffer, int inRecvLen)
{
	GM_Error theErr = GM_NoErr;
	if( m_lpUDPSocket == NULL || lpBuffer == NULL || inRecvLen <= 0 || inRecvLen < sizeof(rtp_detect_t) )
		return;
    // ͨ����һ���ֽڵĵ�2λ���ж��ն�����...
    uint8_t tmTag = lpBuffer[0] & 0x03;
    // ��ȡ��һ���ֽڵ���2λ���õ��ն����...
    uint8_t idTag = (lpBuffer[0] >> 2) & 0x03;
    // ��ȡ��һ���ֽڵĸ�4λ���õ����ݰ�����...
    uint8_t ptTag = (lpBuffer[0] >> 4) & 0x0F;
	// ����� ��ʦ�ۿ��� ������̽��������յ���̽�����ݰ�ԭ�����ظ���������...
	if( tmTag == TM_TAG_TEACHER && idTag == ID_TAG_LOOKER ) {
		// �Ƚ��յ�̽�����ݰ�ԭ�����ظ���������...
		theErr = m_lpUDPSocket->SendTo(lpBuffer, inRecvLen);
		// �ٴ���ۿ����Ѿ��յ��������������...
		rtp_detect_t * lpDetect = (rtp_detect_t*)lpBuffer;
		if( lpDetect->maxConSeq <= 0 || m_circle.size <= 0 )
			return;
		// ���ҵ����ζ�������ǰ�����ݰ���ͷָ�� => ��С���...
		rtp_hdr_t * lpFrontHeader = NULL;
		/////////////////////////////////////////////////////////////////////////////////////////////////
		// ע�⣺ǧ�����ڻ��ζ��е��н���ָ���������start_pos > end_posʱ�����ܻ���Խ�����...
		// ���ԣ�һ��Ҫ�ýӿڶ�ȡ���������ݰ�֮���ٽ��в����������ָ�룬һ�������ػ����ͻ����...
		/////////////////////////////////////////////////////////////////////////////////////////////////
		const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
		static char szPacketBuffer[nPerPackSize] = {0};
		circlebuf_peek_front(&m_circle, szPacketBuffer, nPerPackSize);
		lpFrontHeader = (rtp_hdr_t*)szPacketBuffer;
		// ���Ҫɾ�������ݰ���ű���С��Ż�ҪС => �����Ѿ�ɾ���ˣ�ֱ�ӷ���...
		if( lpDetect->maxConSeq < lpFrontHeader->seq )
			return;
		// ע�⣺��ǰ�ѷ��Ͱ�����һ����ǰ���ţ���ָ��һ��Ҫ���͵İ���...
		// �ۿ����յ����������������Ȼ���ڵ�ǰ�ѷ��Ͱ��� => ֱ�ӷ���...
		if( lpDetect->maxConSeq >= m_nCurSendSeq )
			return;
		// ע�⣺���ζ��е��е����к�һ����������...
		// ע�⣺�ۿ����յ��������������һ���ȵ�ǰ�ѷ��Ͱ���С...
		// ����֮���1����Ҫɾ�������ݳ��� => Ҫ������������������ɾ��...
		uint32_t nPopSize = (lpDetect->maxConSeq - lpFrontHeader->seq + 1) * nPerPackSize;
		circlebuf_pop_front(&m_circle, NULL, nPopSize);
		// ע�⣺���ζ��е��е����ݿ��С�������ģ���һ�����...
		// ��ӡ���ζ���ɾ����������㻷�ζ���ʣ������ݰ�����...
		//uint32_t nRemainCount = m_circle.size / nPackSize;
		//uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
		//TRACE( "[Student-Pusher] Time: %lu ms, Recv Detect MaxConSeq: %lu, MinSeq: %lu, CurSendSeq: %lu, CurPackSeq: %lu, Circle: %lu\n", 
		//		now_ms, lpDetect->maxConSeq, lpFrontHeader->seq, m_nCurSendSeq, m_nCurPackSeq, nRemainCount );
		return;
	}
	// ����� ѧ�������� �Լ�������̽���������������ʱ...
	if( tmTag == TM_TAG_STUDENT && idTag == ID_TAG_PUSHER ) {
		// ��ȡ�յ���̽�����ݰ�...
		rtp_detect_t rtpDetect = {0};
		memcpy(&rtpDetect, lpBuffer, sizeof(rtpDetect));
		// ��ǰʱ��ת���ɺ��룬����������ʱ => ��ǰʱ�� - ̽��ʱ��...
		uint32_t cur_time_ms = (uint32_t)(CUtilTool::os_gettime_ns() / 1000000);
		int keep_rtt = cur_time_ms - rtpDetect.tsSrc;
		// ��ֹ����ͻ���ӳ�����, ��� TCP �� RTT ����˥�����㷨...
		if( m_rtt_ms < 0 ) { m_rtt_ms = keep_rtt; }
		else { m_rtt_ms = (7 * m_rtt_ms + keep_rtt) / 8; }
		// �������綶����ʱ���ֵ => RTT������ֵ...
		if( m_rtt_var_ms < 0 ) { m_rtt_var_ms = abs(m_rtt_ms - keep_rtt); }
		else { m_rtt_var_ms = (m_rtt_var_ms * 3 + abs(m_rtt_ms - keep_rtt)) / 4; }
		// ��ӡ̽���� => ̽����� | ������ʱ(����)...
		uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
		//TRACE("[Student-Pusher] Time: %lu ms, Recv Detect dtNum: %d, rtt: %d ms, rtt_var: %d ms\n", now_ms, rtpDetect.dtNum, m_rtt_ms, m_rtt_var_ms);
	}
}
///////////////////////////////////////////////////////
// ע�⣺û�з�����Ҳû���հ�����Ҫ������Ϣ...
///////////////////////////////////////////////////////
void CUDPSendThread::doSleepTo()
{
	// ���������Ϣ��ֱ�ӷ���...
	if( !m_bNeedSleep )
		return;
	// ����Ҫ��Ϣ��ʱ�� => �����Ϣ������...
	uint64_t delta_ns = MAX_SLEEP_MS * 1000000;
	uint64_t cur_time_ns = CUtilTool::os_gettime_ns();
	// ����ϵͳ���ߺ���������sleep��Ϣ...
	CUtilTool::os_sleepto_ns(cur_time_ns + delta_ns);
}