
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
	// ��ʼ��rtp����ͷ�ṹ��...
	memset(&m_rtp_detect, 0, sizeof(m_rtp_detect));
	memset(&m_rtp_create, 0, sizeof(m_rtp_create));
	memset(&m_rtp_delete, 0, sizeof(m_rtp_delete));
	memset(&m_rtp_header, 0, sizeof(m_rtp_header));
	memset(&m_rtp_ready, 0, sizeof(m_rtp_ready));
	// ��ʼ�����ζ��У�Ԥ����ռ�...
	circlebuf_init(&m_circle);
	circlebuf_reserve(&m_circle, 512*1024);
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
	LPCTSTR lpszAddr = "192.168.1.70"; //DEF_UDP_HOME;
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

/*void CUDPSendThread::doFirstPayload()
{
	// ����ǵ�һ֡���������Ƶ����ͷ...
	if( m_nCurPackSeq > 0 )
		return;
	// �ۼ�RTP������к�...
	++m_nCurPackSeq;
	// ���RTP��ͷ�ṹ��...
	rtp_hdr_t rtpHeader = {0};
	rtpHeader.seq = m_nCurPackSeq;
	rtpHeader.ts  = 0;
	rtpHeader.pt  = PT_TAG_HEADER;
	rtpHeader.pk  = true;
	rtpHeader.pst = true;
	rtpHeader.ped = true;
	rtpHeader.psize = sizeof(rtp_seq_t) + m_strSPS.size() + m_strPPS.size();
	// ������������ܳ���...
	int nZeroSize = DEF_MTU_SIZE - rtpHeader.psize;
	// ���뻷�ζ��е��� => rtp_hdr_t + rtp_seq_t + sps + pps + Zero => 12 + 800 => 812
	circlebuf_push_back(&m_circle, &rtpHeader, sizeof(rtpHeader));
	circlebuf_push_back(&m_circle, &m_rtp_seq_header, sizeof(m_rtp_seq_header));
	// ����SPS����������...
	if( m_strSPS.size() > 0 ) {
		circlebuf_push_back(&m_circle, m_strSPS.c_str(), m_strSPS.size());
	}
	// ����PPS����������...
	if( m_strPPS.size() > 0 ) {
		circlebuf_push_back(&m_circle, m_strPPS.c_str(), m_strPPS.size());
	}
	// ��������������ݣ���֤�������Ǳ���һ��MTU��Ԫ��С...
	if( nZeroSize > 0 ) {
		circlebuf_push_back_zero(&m_circle, nZeroSize);
	}
	// ��ӡ������Ϣ...
	TRACE("[Pack] Seq: %lu, TS: 0, Type: %d, SPS: %lu, PPS: %lu, Zero: %d\n", m_nCurPackSeq, PT_TAG_HEADER, m_strSPS.size(), m_strPPS.size(), nZeroSize);
}*/

void CUDPSendThread::PushFrame(FMS_FRAME & inFrame)
{
	OSMutexLocker theLock(&m_Mutex);
	/////////////////////////////////////////////////////////////////////
	// ע�⣺ֻ�е����ն�׼����֮�󣬲��ܿ�ʼ��������...
	/////////////////////////////////////////////////////////////////////
	// ������ն�û��׼���ã�ֱ�ӷ���...
	if( m_rtp_ready.tm != TM_TAG_TEACHER || m_rtp_ready.recvPort <= 0 )
		return;
	// �������Ƶ����һ֡��������Ƶ�ؼ�֡...
	if( m_rtp_header.hasVideo && m_nCurPackSeq <= 0 ) {
		if( inFrame.typeFlvTag != PT_TAG_VIDEO || !inFrame.is_keyframe )
			return;
		ASSERT( inFrame.typeFlvTag == PT_TAG_VIDEO && inFrame.is_keyframe );
	}
	// ����RTP��ͷ�ṹ��...
	rtp_hdr_t rtpHeader = {0};
	rtpHeader.tm  = TM_TAG_STUDENT;
	rtpHeader.id  = ID_TAG_PUSHER;
	rtpHeader.pt  = inFrame.typeFlvTag;
	rtpHeader.pk  = inFrame.is_keyframe;
	rtpHeader.ts  = inFrame.dwSendTime;
	// ������Ҫ��Ƭ�ĸ��� => ��Ҫע���ۼ����һ����Ƭ...
	int nSliceTotalSize = 0;
	int nSliceSize = DEF_MTU_SIZE;
	int nFrameSize = inFrame.strData.size();
	int nSliceNum = nFrameSize / DEF_MTU_SIZE;
	char * lpszFramePtr = (char*)inFrame.strData.c_str();
	nSliceNum += ((nFrameSize % DEF_MTU_SIZE) ? 1 : 0);
	// ����ѭ��ѹ�뻷�ζ��е���...
	for(int i = 0; i < nSliceNum; ++i) {
		rtpHeader.seq = ++m_nCurPackSeq; // �ۼӴ�����к�...
		rtpHeader.pst = ((i == 0) ? true : false); // �Ƿ��ǵ�һ����Ƭ...
		rtpHeader.ped = ((i+1 == nSliceNum) ? true: false); // �Ƿ������һ����Ƭ...
		rtpHeader.psize = (rtpHeader.ped ? (nFrameSize % DEF_MTU_SIZE) : DEF_MTU_SIZE); // ��������һ����Ƭ��ȡ����������ȡMTUֵ...
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
		//TRACE("[PUSH] Seq: %lu, TS: %lu, Type: %d, pst: %d, ped: %d, Slice: %d, Zero: %d\n", rtpHeader.seq, rtpHeader.ts, rtpHeader.pt, rtpHeader.pst, rtpHeader.ped, rtpHeader.psize, nZeroSize);
	}
}

void CUDPSendThread::Entry()
{
	while( !this->IsStopRequested() ) {
		// ������Ϣ��־ => ֻҪ�з������հ��Ͳ�����Ϣ...
		m_bNeedSleep = true;
		// ���ʹ��������ֱ��ͨ�������...
		this->doSendCreateCmd();
		// ����̽�������...
		this->doSendDetectCmd();
		// ����һ����װ�õ�RTP���ݰ�...
		this->doSendPacket();
		// ����һ������ķ�����������...
		this->doRecvPacket();
		// �ȴ����ͻ������һ�����ݰ�...
		this->doSleepTo();
	}
	// ����һ��ɾ�������...
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
	return;
}

void CUDPSendThread::doSendCreateCmd()
{
	OSMutexLocker theLock(&m_Mutex);
	// ����Ѿ��յ���ȷ��׼����������ֱ�ӷ���...
	if( m_rtp_ready.tm == TM_TAG_TEACHER && m_rtp_ready.recvPort > 0 )
		return;
	// ÿ��100���뷢�ʹ�������� => ����ת�����з���...
	int64_t cur_time_ns = CUtilTool::os_gettime_ns();
	int64_t period_ns = 100000000;
	// �������ʱ�仹û����ֱ�ӷ���...
	if( m_next_create_ns > cur_time_ns )
		return;
	ASSERT( cur_time_ns >= m_next_create_ns );
	// ���ȣ�����һ��������������...
	GM_Error theErr = m_lpUDPSocket->SendTo((void*)&m_rtp_create, sizeof(m_rtp_create));
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
		return;
	}
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
	theErr = m_lpUDPSocket->SendTo((void*)strSeqHeader.c_str(), strSeqHeader.size());
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
		return;
	}
	// �����´η��ʹ��������ʱ���...
	m_next_create_ns = CUtilTool::os_gettime_ns() + period_ns;
	// �޸���Ϣ״̬ => �Ѿ��з�����������Ϣ...
	m_bNeedSleep = false;
}

void CUDPSendThread::doSendDetectCmd()
{
	OSMutexLocker theLock(&m_Mutex);
	// ÿ��1�뷢��һ��̽������� => ����ת�����з���...
	int64_t cur_time_ns = CUtilTool::os_gettime_ns();
	int64_t period_ns = 1000000000;
	// �������ʱ�仹û����ֱ�ӷ���...
	if( m_next_detect_ns > cur_time_ns )
		return;
	ASSERT( cur_time_ns >= m_next_detect_ns );
	// ��̽�����ʱ��ת���ɺ��룬�ۼ�̽�������...
	m_rtp_detect.tsSrc  = (uint32_t)(cur_time_ns / 1000000);
	m_rtp_detect.dtNum += 1;
	// ���ýӿڷ���̽�������...
	GM_Error theErr = m_lpUDPSocket->SendTo((void*)&m_rtp_detect, sizeof(m_rtp_detect));
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
		return;
	}
	// �����´η���̽�������ʱ���...
	m_next_detect_ns = CUtilTool::os_gettime_ns() + period_ns;
	// �޸���Ϣ״̬ => �Ѿ��з�����������Ϣ...
	m_bNeedSleep = false;
}

void CUDPSendThread::doSendPacket()
{
	OSMutexLocker theLock(&m_Mutex);
	if( m_circle.size <= 0 || m_lpUDPSocket == NULL )
		return;
	// ȡ����ǰ���RTP���ݰ��������ӻ��ζ������Ƴ� => Ŀ���Ǹ����ն˲�����...
	rtp_hdr_t * lpFrontHeader = NULL;
	rtp_hdr_t * lpSendHeader = NULL;
	int nSendPos = 0, nSendSize = 0;
	int nPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	// ���㻷�ζ�������ǰ�����ݰ���ͷָ�� => ��С���...
	lpFrontHeader = (rtp_hdr_t*)circlebuf_data(&m_circle, 0);
	// ��һ�η��� �� �������̫С => ʹ����ǰ��������к�...
	if((m_nCurSendSeq <= 0) || (m_nCurSendSeq < lpFrontHeader->seq)) {
		m_nCurSendSeq = lpFrontHeader->seq;
	}
	// ��Ҫ���͵����ݰ���Ų���С����ǰ��������к�...
	ASSERT( m_nCurSendSeq >= lpFrontHeader->seq );
	// ����֮�����Ҫ�������ݰ���ͷָ��λ��...
	nSendPos = (m_nCurSendSeq - lpFrontHeader->seq) * nPackSize;
	// ���Ҫ����λ�ó����˶�����Ч���ȣ�˵�����ݰ��Ѿ��������...
	if( nSendPos < 0 || nSendPos >= m_circle.size )
		return;
	// ��ȡ�������ݰ��İ�ͷλ�ú���Ч���ݳ���...
	lpSendHeader = (rtp_hdr_t*)circlebuf_data(&m_circle, nSendPos);
	nSendSize = sizeof(rtp_hdr_t) + lpSendHeader->psize;
	// �����׽��ֽӿڣ�ֱ�ӷ���RTP���ݰ�...
	GM_Error theErr = m_lpUDPSocket->SendTo((void*)lpSendHeader, nSendSize);
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
		return;
	}
	// �ɹ��������ݰ� => �ۼӷ������к�...
	++m_nCurSendSeq;
	// �޸���Ϣ״̬ => �Ѿ��з�����������Ϣ...
	m_bNeedSleep = false;

	// ��ӡ������Ϣ => �ոշ��͵����ݰ�...
	int nZeroSize = DEF_MTU_SIZE - lpSendHeader->psize;
	TRACE("[Send ] Seq: %lu, TS: %lu, Type: %d, pst: %d, ped: %d, Slice: %d, Zero: %d\n", lpSendHeader->seq, lpSendHeader->ts, lpSendHeader->pt, lpSendHeader->pst, lpSendHeader->ped, lpSendHeader->psize, nZeroSize);

	// ʵ�飺�Ƴ���ǰ������ݰ�...
	string strPacket;
	strPacket.resize(nPackSize);
	circlebuf_pop_front(&m_circle, (void*)strPacket.c_str(), nPackSize);

	/*string	strRtpData;
	int         nZeroSize = 0;
	rtp_hdr_t	rtpHeader = {0};
	// ��ȡRTP��ͷ�ṹ��...
	circlebuf_pop_front(&m_circle, &rtpHeader, sizeof(rtpHeader));
	// ����RTP���ݰ���С�������������С...
	strRtpData.resize(rtpHeader.psize);
	nZeroSize = DEF_MTU_SIZE - rtpHeader.psize;
	// ������Ч��RTP���ݰ�...
	circlebuf_pop_front(&m_circle, (void*)strRtpData.c_str(), rtpHeader.psize);
	// ����RTP�����������...
	if( nZeroSize > 0 ) {
		circlebuf_pop_front(&m_circle, NULL, nZeroSize);
	}
	// ��ȡRTP���ݰ�ͷ�ṹ��ָ��...
	LPCTSTR lpszData = strRtpData.c_str();
	// ����ǵ�һ��RTP���ݰ�����ȡ����ͷ�ṹ�壬SPS��PPS����...
	if( rtpHeader.pt == PT_TAG_HEADER ) {
		string strSPS, strPPS;
		// ��ȡ����ͷ�ṹ��...
		rtp_seq_t * lpRtpSeq = (rtp_seq_t*)lpszData;
		// ��ȡSPS������Ϣ...
		lpszData += sizeof(rtp_seq_t);
		if( lpRtpSeq->spsSize > 0 ) {
			strSPS.assign(lpszData, lpRtpSeq->spsSize);
		}
		// ��ȡPPS������Ϣ...
		lpszData += lpRtpSeq->spsSize;
		if( lpRtpSeq->ppsSize > 0 ) {
			strPPS.assign(lpszData, lpRtpSeq->ppsSize);
		}
		// ��ӡ������Ϣ...
		TRACE("[POP ] Seq: %lu, TS: 0, Type: %d, SPS: %lu, PPS: %lu, Zero: %d\n", rtpHeader.seq, rtpHeader.pt, strSPS.size(), strPPS.size(), nZeroSize);
	} else {
		// ��������ƵRTP���ݰ�...

		// ��ӡ������Ϣ...
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
	// ���ýӿڴ�������ȡ���ݰ� => �������첽�׽��֣����������� => ���ܴ���...
	theErr = m_lpUDPSocket->RecvFrom(&outRemoteAddr, &outRemotePort, ioBuffer, inBufLen, &outRecvLen);
	if( theErr != GM_NoErr )
		return;
	// �޸���Ϣ״̬ => �Ѿ��ɹ��հ���������Ϣ...
	m_bNeedSleep = false;
 
	// ��ȡ��һ���ֽڵĸ�4λ���õ����ݰ�����...
    uint8_t ptTag = (ioBuffer[0] >> 4) & 0x0F;

	// ���յ�������������ͷַ�...
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
    // ͨ����һ���ֽڵĵ�2λ���ж��ն�����...
    uint8_t tmTag = lpBuffer[0] & 0x03;
    // ��ȡ��һ���ֽڵ���2λ���õ��ն����...
    uint8_t idTag = (lpBuffer[0] >> 2) & 0x03;
    // ��ȡ��һ���ֽڵĸ�4λ���õ����ݰ�����...
    uint8_t ptTag = (lpBuffer[0] >> 4) & 0x0F;
	// ����� ��ʦ�ۿ��� ������̽��������յ���̽�����ݰ�ԭ�����ظ���������...
	if( tmTag == TM_TAG_TEACHER && idTag == ID_TAG_LOOKER ) {
		theErr = m_lpUDPSocket->SendTo(lpBuffer, inRecvLen);
		return;
	}
	// ����� ѧ�������� �Լ�������̽���������������ʱ...
	if( tmTag == TM_TAG_STUDENT && idTag == ID_TAG_PUSHER ) {
		// ��ȡ�յ���̽�����ݰ�...
		rtp_detect_t rtpDetect = {0};
		memcpy(&rtpDetect, lpBuffer, sizeof(rtpDetect));
		// ��ǰʱ��ת���ɺ��룬����������ʱ => ��ǰʱ�� - ̽��ʱ��...
		uint32_t cur_time_ms = (uint32_t)(CUtilTool::os_gettime_ns() / 1000000);
		int  new_rtt = cur_time_ms - rtpDetect.tsSrc;
		// ��ӡ̽���� => ̽����� | ������ʱ(����)...
		TRACE("[Student-Pusher] Detect dtNum: %d, rtt: %d ms\n", rtpDetect.dtNum, new_rtt);
	}
}

void CUDPSendThread::doTagReadyProcess(char * lpBuffer, int inRecvLen)
{
	if( lpBuffer == NULL || inRecvLen <= 0 || inRecvLen < sizeof(m_rtp_ready) )
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
	// ������ʦ�ۿ��˷��͵�׼���������ݰ�����...
	memcpy(&m_rtp_ready, lpBuffer, sizeof(m_rtp_ready));
}
///////////////////////////////////////////////////////
// ע�⣺û�з�����Ҳû���հ�����Ҫ������Ϣ...
///////////////////////////////////////////////////////
void CUDPSendThread::doSleepTo()
{
	// ���������Ϣ��ֱ�ӷ���...
	if( !m_bNeedSleep )
		return;
	// ����Ҫ��Ϣ��ʱ�� => ��Ϣ5����...
	uint64_t delta_ns = 5 * 1000000;
	uint64_t cur_time_ns = CUtilTool::os_gettime_ns();
	// ����ϵͳ���ߺ���������sleep��Ϣ...
	CUtilTool::os_sleepto_ns(cur_time_ns + delta_ns);
}