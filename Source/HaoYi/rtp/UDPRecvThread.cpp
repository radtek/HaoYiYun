
#include "stdafx.h"
#include "UtilTool.h"
#include "UDPSocket.h"
#include "UDPPlayThread.h"
#include "UDPRecvThread.h"
#include "../PushThread.h"

CUDPRecvThread::CUDPRecvThread(CPushThread * lpPushThread, int nDBRoomID, int nDBCameraID)
  : m_lpPushThread(lpPushThread)
  , m_lpUDPSocket(NULL)
  , m_lpPlaySDL(NULL)
  , m_nMaxPlaySeq(0)
  , m_next_create_ns(-1)
  , m_next_detect_ns(-1)
  , m_next_ready_ns(-1)
  , m_bNeedSleep(false)
  , m_rtt_var_ms(-1)
  , m_rtt_ms(-1)
{
	ASSERT( m_lpPushThread != NULL );
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
	circlebuf_reserve(&m_circle, 512*1024);
	//////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺������ʱģ�⽲ʦ�˹ۿ������ => ����Ӧ��Ϊѧ���ۿ���...
	//////////////////////////////////////////////////////////////////////////////////////
	// �����ն����ͺͽṹ������ => m_rtp_header => �ȴ��������...
	m_rtp_detect.tm = m_rtp_create.tm = m_rtp_delete.tm = m_rtp_supply.tm = TM_TAG_TEACHER; // TM_TAG_STUDENT
	m_rtp_detect.id = m_rtp_create.id = m_rtp_delete.id = m_rtp_supply.id = ID_TAG_LOOKER;
	m_rtp_detect.pt = PT_TAG_DETECT;
	m_rtp_create.pt = PT_TAG_CREATE;
	m_rtp_delete.pt = PT_TAG_DELETE;
	m_rtp_supply.pt = PT_TAG_SUPPLY;
	// ��䷿��ź�ֱ��ͨ����...
	m_rtp_create.roomID = nDBRoomID;
	m_rtp_create.liveID = nDBCameraID;
	m_rtp_delete.roomID = nDBRoomID;
	m_rtp_delete.liveID = nDBCameraID;
}

CUDPRecvThread::~CUDPRecvThread()
{
	// ֹͣ�̣߳��ȴ��˳�...
	this->StopAndWaitForThread();
	// ɾ������Ƶ�����߳�...
	this->ClosePlayer();
	// �ر�UDPSocket����...
	this->CloseSocket();
	// �ͷŻ��ζ��пռ�...
	circlebuf_free(&m_circle);
}

void CUDPRecvThread::ClosePlayer()
{
	if( m_lpPlaySDL != NULL ) {
		delete m_lpPlaySDL;
		m_lpPlaySDL = NULL;
	}
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

void CUDPRecvThread::Entry()
{
	while( !this->IsStopRequested() ) {
		// ������Ϣ��־ => ֻҪ�з������հ��Ͳ�����Ϣ...
		m_bNeedSleep = true;
		// ���ʹ��������ֱ��ͨ�������...
		this->doSendCreateCmd();
		// ����̽�������...
		this->doSendDetectCmd();
		// ����׼�����������...
		this->doSendReadyCmd();
		// ����һ������ķ�����������...
		this->doRecvPacket();
		// ���Ͳ�������...
		this->doSendSupplyCmd();
		// �ӻ��ζ����г�ȡ����һ֡���ݣ����벥����...
		this->doParseFrame();
		// �ȴ����ͻ������һ�����ݰ�...
		this->doSleepTo();
	}
	// ֻ����һ��ɾ�������...
	this->doSendDeleteCmd();
}

void CUDPRecvThread::doSendDeleteCmd()
{
	OSMutexLocker theLock(&m_Mutex);
	GM_Error theErr = GM_NoErr;
	if( m_lpUDPSocket == NULL )
		return;
	// �׽�����Ч��ֱ�ӷ���ɾ������...
	ASSERT( m_lpUDPSocket != NULL );
	theErr = m_lpUDPSocket->SendTo((void*)&m_rtp_delete, sizeof(m_rtp_delete));
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	// ��ӡ�ѷ���ɾ�������...
	uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	TRACE("[Teacher-Looker] Time: %lu ms, Send Delete RoomID: %lu, LiveID: %d\n", now_ms, m_rtp_delete.roomID, m_rtp_delete.liveID);
}

void CUDPRecvThread::doSendCreateCmd()
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
	// ����һ�������������� => �൱�ڵ�¼ע��...
	GM_Error theErr = m_lpUDPSocket->SendTo((void*)&m_rtp_create, sizeof(m_rtp_create));
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	// ��ӡ�ѷ��ʹ�������� => ��һ�����п���û�з��ͳ�ȥ��Ҳ��������...
	uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	TRACE("[Teacher-Looker] Time: %lu ms, Send Create RoomID: %lu, LiveID: %d\n", now_ms, m_rtp_create.roomID, m_rtp_create.liveID);
	// �����´η��ʹ��������ʱ���...
	m_next_create_ns = CUtilTool::os_gettime_ns() + period_ns;
	// �޸���Ϣ״̬ => �Ѿ��з�����������Ϣ...
	m_bNeedSleep = false;
}

void CUDPRecvThread::doSendDetectCmd()
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
	// �������յ������������...
	m_rtp_detect.maxConSeq = this->doCalcMaxConSeq();
	// ���ýӿڷ���̽�������...
	GM_Error theErr = m_lpUDPSocket->SendTo((void*)&m_rtp_detect, sizeof(m_rtp_detect));
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	// ��ӡ�ѷ���̽�������...
	uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	//TRACE("[Teacher-Looker] Time: %lu ms, Send Detect dtNum: %d, MaxConSeq: %lu\n", now_ms, m_rtp_detect.dtNum, m_rtp_detect.maxConSeq);
	// �����´η���̽�������ʱ���...
	m_next_detect_ns = CUtilTool::os_gettime_ns() + period_ns;
	// �޸���Ϣ״̬ => �Ѿ��з�����������Ϣ...
	m_bNeedSleep = false;
}

uint32_t CUDPRecvThread::doCalcMaxConSeq()
{
	// ���������ļ��� => ������С������ - 1
	if( m_MapLose.size() > 0 ) {
		return (m_MapLose.begin()->first - 1);
	}
	// û�ж��� => ���ζ���Ϊ�� => ������󲥷����к�...
	if(  m_circle.size <= 0  )
		return m_nMaxPlaySeq;
	// û�ж��� => ���յ��������� => ���ζ�����������к�...
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacket[nPerPackSize] = {0};
	circlebuf_peek_back(&m_circle, szPacket, nPerPackSize);
	rtp_hdr_t * lpMaxHeader = (rtp_hdr_t*)szPacket;
	return lpMaxHeader->seq;
}

void CUDPRecvThread::doSendReadyCmd()
{
	OSMutexLocker theLock(&m_Mutex);
	// �������״̬����׼������������������ֱ�ӷ���...
	if( m_nCmdState != kCmdSendReady )
		return;
	// ע�⣺��ʱ����Ƶ����ͷ�Ѿ��������...
	// ÿ��100���뷢�;�������� => ����ת�����з���...
	int64_t cur_time_ns = CUtilTool::os_gettime_ns();
	int64_t period_ns = 100 * 1000000;
	// �������ʱ�仹û����ֱ�ӷ���...
	if( m_next_ready_ns > cur_time_ns )
		return;
	ASSERT( cur_time_ns >= m_next_ready_ns );
	// ׼����ʱ��׼����������ṹ��...
	rtp_ready_t rtpReady = {0};
	rtpReady.tm = TM_TAG_TEACHER;
	rtpReady.id = ID_TAG_LOOKER;
	rtpReady.pt = PT_TAG_READY;
	// ����һ��׼����������� => ֪ͨѧ�������� => ���Կ�ʼ��������Ƶ���ݰ�...
	GM_Error theErr = m_lpUDPSocket->SendTo((void*)&rtpReady, sizeof(rtpReady));
	(theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL;
	// ��ӡ�ѷ���׼�����������...
	uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	TRACE("[Teacher-Looker] Time: %lu ms, Send Ready command\n", now_ms);
	// �����´η��ʹ��������ʱ���...
	m_next_ready_ns = CUtilTool::os_gettime_ns() + period_ns;
	// �޸���Ϣ״̬ => �Ѿ��з�����������Ϣ...
	m_bNeedSleep = false;
}

void CUDPRecvThread::doSendSupplyCmd()
{
	OSMutexLocker theLock(&m_Mutex);
	// ����������϶���Ϊ�գ�ֱ�ӷ���...
	if( m_MapLose.size() <= 0 )
		return;
	ASSERT( m_MapLose.size() > 0 );
	// �������Ĳ���������...
	const int nHeadSize = sizeof(m_rtp_supply);
	const int nPerPackSize = DEF_MTU_SIZE + nHeadSize;
	static char szPacket[nPerPackSize] = {0};
	char * lpData = szPacket + nHeadSize;
	// ��ȡ��ǰʱ��ĺ���ֵ => С�ڻ���ڵ�ǰʱ��Ķ�������Ҫ֪ͨ���Ͷ��ٴη���...
	uint32_t cur_time_ms = (uint32_t)(CUtilTool::os_gettime_ns() / 1000000);
	// ���ò�������Ϊ0 => ���¼�����Ҫ�����ĸ���...
	m_rtp_supply.suSize = 0;
	// �����������У��ҳ���Ҫ�����Ķ������к�...
	GM_MapLose::iterator itorItem = m_MapLose.begin();
	while( itorItem != m_MapLose.end() ) {
		rtp_lose_t & rtpLose = itorItem->second;
		if( rtpLose.resend_time <= cur_time_ms ) {
			// ����������峬���趨�����ֵ������ѭ�� => ��ಹ��200��...
			if( (nHeadSize + m_rtp_supply.suSize) >= nPerPackSize )
				break;
			// �ۼӲ������ȣ������������к�...
			m_rtp_supply.suSize += sizeof(int);
			memcpy(lpData, &rtpLose.lose_seq, sizeof(int));
			// �ۼ�����ָ��...
			lpData += sizeof(int);
			// �ۼ��ط�����...
			++rtpLose.resend_count;
			// ע�⣺���һ������������ʱ��û���յ����������Ҫ�ٴη���������Ĳ�������...
			// ע�⣺����Ҫ���� ���綶��ʱ��� Ϊ��������� => ��û����ɵ�һ��̽������...
			// �����´��ش�ʱ��� => cur_time + rtt => ����ʱ�ĵ�ǰʱ�� + ���������ӳ�ֵ...
			rtpLose.resend_time = (uint32_t)(CUtilTool::os_gettime_ns() / 1000000) + max(m_rtt_ms,0);
			// ���������������1���´β�����Ҫ̫�죬׷��һ����Ϣ����..
			if( rtpLose.resend_count > 1 ) {
				rtpLose.resend_time += MAX_SLEEP_MS;
			}
		}
		// �ۼӶ������Ӷ���...
		++itorItem;
	}
	// ������������Ϊ�գ�ֱ�ӷ���...
	if( m_rtp_supply.suSize <= 0 )
		return;
	// ��ӡ�ѷ��Ͳ�������...
	uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	TRACE("[Teacher-Looker] Time: %lu ms, Supply Send => Count: %d\n", now_ms, m_rtp_supply.suSize / sizeof(int));
	// ���²�������ͷ���ݿ�...
	memcpy(szPacket, &m_rtp_supply, nHeadSize);
	// ����������岻Ϊ�գ��Ž��в��������...
	GM_Error theErr = GM_NoErr;
	int nDataSize = nHeadSize + m_rtp_supply.suSize;
	theErr = m_lpUDPSocket->SendTo(szPacket, nDataSize);
	((theErr != GM_NoErr) ? MsgLogGM(theErr) : NULL);
	// �޸���Ϣ״̬ => �Ѿ��з�����������Ϣ...
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
	case PT_TAG_READY:   this->doProcServerReady(ioBuffer, outRecvLen);  break;
	case PT_TAG_HEADER:	 this->doProcServerHeader(ioBuffer, outRecvLen); break;
	case PT_TAG_RELOAD:  this->doProcServerReload(ioBuffer, outRecvLen); break;

	case PT_TAG_DETECT:	 this->doTagDetectProcess(ioBuffer, outRecvLen); break;
	case PT_TAG_AUDIO:	 this->doTagAVPackProcess(ioBuffer, outRecvLen); break;
	case PT_TAG_VIDEO:	 this->doTagAVPackProcess(ioBuffer, outRecvLen); break;
	}
}

void CUDPRecvThread::doProcServerHeader(char * lpBuffer, int inRecvLen)
{
	// ͨ�� rtp_header_t ��Ϊ���巢�͹����� => ������ֱ��ԭ��ת����ѧ�������˵�����ͷ�ṹ��...
	if( m_lpUDPSocket == NULL || lpBuffer == NULL || inRecvLen < 0 || inRecvLen < sizeof(rtp_header_t) )
		return;
	// ������Ǵ�������״̬�����ߣ��Ѿ��յ�������ͷ��ֱ�ӷ���...
	if( m_nCmdState != kCmdSendCreate || m_rtp_header.pt == PT_TAG_HEADER || m_rtp_header.hasAudio || m_rtp_header.hasVideo )
		return;
    // ͨ����һ���ֽڵĵ�2λ���ж��ն�����...
    uint8_t tmTag = lpBuffer[0] & 0x03;
    // ��ȡ��һ���ֽڵ���2λ���õ��ն����...
    uint8_t idTag = (lpBuffer[0] >> 2) & 0x03;
    // ��ȡ��һ���ֽڵĸ�4λ���õ����ݰ�����...
    uint8_t ptTag = (lpBuffer[0] >> 4) & 0x0F;
	// ��������߲��� ѧ�������� => ֱ�ӷ���...
	if( tmTag != TM_TAG_STUDENT || idTag != ID_TAG_PUSHER )
		return;
	// ������ֳ��Ȳ�����ֱ�ӷ���...
	memcpy(&m_rtp_header, lpBuffer, sizeof(m_rtp_header));
	int nNeedSize = m_rtp_header.spsSize + m_rtp_header.ppsSize + sizeof(m_rtp_header);
	if( nNeedSize != inRecvLen ) {
		TRACE("[Teacher-Looker] Recv Header error, RecvLen: %d\n", inRecvLen);
		memset(&m_rtp_header, 0, sizeof(m_rtp_header));
		return;
	}
	// ��ȡSPS��PPS��ʽͷ��Ϣ...
	char * lpData = lpBuffer + sizeof(m_rtp_header);
	if( m_rtp_header.spsSize > 0 ) {
		m_strSPS.assign(lpData, m_rtp_header.spsSize);
		lpData += m_rtp_header.spsSize;
	}
	// ���� PPS ��ʽͷ...
	if( m_rtp_header.ppsSize > 0 ) {
		m_strPPS.assign(lpData, m_rtp_header.ppsSize);
		lpData += m_rtp_header.ppsSize;
	}
	// �޸�����״̬ => ��ʼ�������׼�����������...
	m_nCmdState = kCmdSendReady;
	// ��ӡ�յ�����ͷ�ṹ����Ϣ...
	uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	TRACE("[Teacher-Looker] Time: %lu ms, Recv Header SPS: %d, PPS: %d\n", now_ms, m_strSPS.size(), m_strPPS.size());
	/////////////////////////////////////////////////////////////////
	// ��ʼ�ؽ����ز���������...
	/////////////////////////////////////////////////////////////////
	// ����������Ѿ�������ֱ�ӷ���...
	if( m_lpPlaySDL != NULL || m_lpPushThread == NULL )
		return;
	// �½�����������ʼ������Ƶ�߳�...
	m_lpPlaySDL = new CPlaySDL();
	// �������Ƶ����ʼ����Ƶ�߳�...
	if( m_rtp_header.hasVideo ) {
		int nPicFPS = m_rtp_header.fpsNum;
		int nPicWidth = m_rtp_header.picWidth;
		int nPicHeight = m_rtp_header.picHeight;
		CRenderWnd * lpRenderWnd = m_lpPushThread->GetRenderWnd();
		m_lpPlaySDL->InitVideo(lpRenderWnd, m_strSPS, m_strPPS, nPicWidth, nPicHeight, nPicFPS);
	} 
	// �������Ƶ����ʼ����Ƶ�߳�...
	if( m_rtp_header.hasAudio ) {
		int nRateIndex = m_rtp_header.rateIndex;
		int nChannelNum = m_rtp_header.channelNum;
		m_lpPlaySDL->InitAudio(nRateIndex, nChannelNum);
	}
}

void CUDPRecvThread::doProcServerReady(char * lpBuffer, int inRecvLen)
{
	// ���յ�����ѧ��������Ҫ��ֹͣ����׼�����������...
	if( m_lpUDPSocket == NULL || lpBuffer == NULL || inRecvLen < 0 || inRecvLen < sizeof(rtp_ready_t) )
		return;
	// �������׼����������״̬�����ߣ��Ѿ��յ��������˷�����׼���������ֱ�ӷ���...
	if( m_nCmdState != kCmdSendReady || m_rtp_ready.pt == PT_TAG_READY )
		return;
    // ͨ����һ���ֽڵĵ�2λ���ж��ն�����...
    uint8_t tmTag = lpBuffer[0] & 0x03;
    // ��ȡ��һ���ֽڵ���2λ���õ��ն����...
    uint8_t idTag = (lpBuffer[0] >> 2) & 0x03;
    // ��ȡ��һ���ֽڵĸ�4λ���õ����ݰ�����...
    uint8_t ptTag = (lpBuffer[0] >> 4) & 0x0F;
	// ������� ѧ�������� ������׼����������ֱ�ӷ���...
	if( tmTag != TM_TAG_STUDENT || idTag != ID_TAG_PUSHER )
		return;
	// �޸�����״̬ => ���Խ�������Ƶ���ݰ��Ľ�����...
	m_nCmdState = kCmdRecvAVPak;
	// ����ѧ�������˷��͵�׼���������ݰ�����...
	memcpy(&m_rtp_ready, lpBuffer, sizeof(m_rtp_ready));
	// ��ӡ�յ�׼�����������...
	uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	TRACE("[Teacher-Looker] Time: %lu ms, Recv Ready from %lu:%d\n", now_ms, m_rtp_ready.recvAddr, m_rtp_ready.recvPort);
}
//
// ������������͹������ؽ�����...
void CUDPRecvThread::doProcServerReload(char * lpBuffer, int inRecvLen)
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
		uint32_t load_time_sec = m_rtp_reload.reload_time/1000;
		// ����ؽ�����������20�룬ֱ�ӷ���...
		if( (cur_time_sec - load_time_sec) < RELOAD_TIME_OUT )
			return;
	}
	// ɾ������Ƶ�����߳�...
	this->ClosePlayer();
	// ������������ݵ��ؽ�����...
	m_rtp_reload.tm = tmTag;
	m_rtp_reload.id = idTag;
	m_rtp_reload.pt = ptTag;
	// ��¼�����ؽ���Ϣ...
	m_rtp_reload.reload_time = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	++m_rtp_reload.reload_count;
	// ��ӡ�յ��������ؽ�����...
	uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	TRACE("[Teacher-Looker] Time: %lu ms, Server Reload Count: %d\n", now_ms, m_rtp_reload.reload_count);
	// ������������...
	memset(&m_rtp_header, 0, sizeof(m_rtp_header));
	memset(&m_rtp_ready, 0, sizeof(m_rtp_ready));
	m_rtp_supply.suSize = 0;
	// ��ղ������϶���...
	m_MapLose.clear();
	// �ͷŻ��ζ��пռ�...
	circlebuf_free(&m_circle);
	// ��ʼ�����ζ��У�Ԥ����ռ�...
	circlebuf_init(&m_circle);
	circlebuf_reserve(&m_circle, 512*1024);
	// ������ر���...
	m_nCmdState = kCmdSendCreate;
	m_next_create_ns = -1;
	m_next_detect_ns = -1;
	m_next_ready_ns = -1;
	// ���¿�ʼ̽������...
	m_rtp_detect.tsSrc = 0;
	m_rtp_detect.dtNum = 0;
	m_rtt_var_ms = -1;
	m_rtt_ms = -1;
	// ������Ƶ����ͷ...
	m_strSPS.clear();
	m_strPPS.clear();
}

void CUDPRecvThread::doTagDetectProcess(char * lpBuffer, int inRecvLen)
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
	// ����� ѧ�������� ������̽��������յ���̽�����ݰ�ԭ�����ظ���������...
	if( tmTag == TM_TAG_STUDENT && idTag == ID_TAG_PUSHER ) {
		theErr = m_lpUDPSocket->SendTo(lpBuffer, inRecvLen);
		return;
	}
	// ����� ��ʦ�ۿ��� �Լ�������̽���������������ʱ...
	if( tmTag == TM_TAG_TEACHER && idTag == ID_TAG_LOOKER ) {
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
		TRACE("[Teacher-Looker] Time: %lu ms, Recv Detect dtNum: %d, rtt: %d ms, rtt_var: %d ms\n", now_ms, rtpDetect.dtNum, m_rtt_ms, m_rtt_var_ms);
	}
}

/*static void DoSaveRecvFile(uint32_t inPTS, int inType, bool bIsKeyFrame, int inSize)
{
	static char szBuf[MAX_PATH] = {0};
	char * lpszPath = "F:/MP4/Dst/recv.txt";
	FILE * pFile = fopen(lpszPath, "a+");
	sprintf(szBuf, "PTS: %lu, Type: %d, Key: %d, Size: %d\n", inPTS, inType, bIsKeyFrame, inSize);
	fwrite(szBuf, 1, strlen(szBuf), pFile);
	fclose(pFile);
}

static void DoSaveRecvSeq(uint32_t inPSeq, int inPSize, bool inPST, bool inPED, uint32_t inPTS)
{
	static char szBuf[MAX_PATH] = {0};
	char * lpszPath = "F:/MP4/Dst/recv_seq.txt";
	FILE * pFile = fopen(lpszPath, "a+");
	sprintf(szBuf, "PSeq: %lu, PSize: %d, PST: %d, PED: %d, PTS: %lu\n", inPSeq, inPSize, inPST, inPED, inPTS);
	fwrite(szBuf, 1, strlen(szBuf), pFile);
	fclose(pFile);
}*/

void CUDPRecvThread::doParseFrame()
{
	/*////////////////////////////////////////////////////////////////////////////
	// ע�⣺���ζ�������Ҫ��һ�����ݰ����ڣ������ڷ�������ʱ���޷�����...
	// ���� => ��ӡ�յ�����Ч����ţ����ӻ��ζ��е���ɾ��...
	////////////////////////////////////////////////////////////////////////////
	int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	if( m_circle.size <= nPerPackSize )
		return;
	rtp_hdr_t * lpFirstHeader = (rtp_hdr_t*)circlebuf_data(&m_circle, 0);
	if( lpFirstHeader->pt == PT_TAG_LOSE )
		return;
	//uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	//TRACE( "[Teacher-Looker] Time: %lu ms, Seq: %lu, Type: %d, Key: %d, Size: %d, TS: %lu\n", now_ms, 
	//		lpFirstHeader->seq, lpFirstHeader->pt, lpFirstHeader->pk, lpFirstHeader->psize, lpFirstHeader->ts);
	// ����յ�����Ч��Ų��������ģ���ӡ����...
	if( (m_nMaxPlaySeq + 1) != lpFirstHeader->seq ) {
		TRACE("[Teacher-Looker] Error => PlaySeq: %lu, CurSeq: %lu\n", m_nMaxPlaySeq, lpFirstHeader->seq);
	}
	// ������ǰ������ţ��Ƴ����ζ���...
	m_nMaxPlaySeq = lpFirstHeader->seq;
	circlebuf_pop_front(&m_circle, NULL, nPerPackSize);*/

	// ��֤�Ƿ񶪰���ʵ��...
	/*int nPerTestSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	if( m_circle.size <= nPerTestSize )
		return;
	rtp_hdr_t * lpTestHeader = (rtp_hdr_t*)circlebuf_data(&m_circle, 0);
	if( lpTestHeader->pt == PT_TAG_LOSE )
		return;
	DoSaveRecvSeq(lpTestHeader->seq, lpTestHeader->psize, lpTestHeader->pst, lpTestHeader->ped, lpTestHeader->ts);
	circlebuf_pop_front(&m_circle, NULL, nPerTestSize);
	return;*/

	/////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺���ζ�������Ҫ��һ�����ݰ����ڣ������ڷ�������ʱ���޷�����...
	// ������ζ���Ϊ�ջ򲥷���������Ч��ֱ�ӷ���...
	/////////////////////////////////////////////////////////////////////////////////////
	int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	rtp_hdr_t * lpFrontHeader = NULL;
	if( m_circle.size <= nPerPackSize )
		return;
	// ���ҵ����ζ�������ǰ�����ݰ���ͷָ�� => ��С���...
	lpFrontHeader = (rtp_hdr_t*)circlebuf_data(&m_circle, 0);
	// �����С��Ű�����Ҫ����Ķ��� => ������Ϣ�ȴ�...
	if( lpFrontHeader->pt == PT_TAG_LOSE )
		return;
	// �����С��Ű���������Ƶ����֡�Ŀ�ʼ�� => ɾ��������ݰ���������...
	if( lpFrontHeader->pst <= 0 ) {
		// ���µ�ǰ��󲥷����кŲ���������...
		m_nMaxPlaySeq = lpFrontHeader->seq;
		// ɾ��������ݰ������ز���Ϣ��������...
		//DoSaveRecvSeq(lpFrontHeader->seq, lpFrontHeader->psize, lpFrontHeader->pst, lpFrontHeader->ped, lpFrontHeader->ts);
		circlebuf_pop_front(&m_circle, NULL, nPerPackSize);
		// �޸���Ϣ״̬ => �Ѿ���ȡ���ݰ���������Ϣ...
		m_bNeedSleep = false;
		// ��ӡ��֡ʧ����Ϣ => û���ҵ�����֡�Ŀ�ʼ���...
		uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
		TRACE( "[Teacher-Looker] Time: %lu ms, Error => Frame start code not find, Seq: %lu, Type: %d, Key: %d, PTS: %lu\n", 
				now_ms, lpFrontHeader->seq, lpFrontHeader->pt, lpFrontHeader->pk, lpFrontHeader->ts );
		return;
	}
	ASSERT( lpFrontHeader->pst );
	// ��ʼ��ʽ�ӻ��ζ����г�ȡ����Ƶ����֡...
	int         pt_type = lpFrontHeader->pt;
	bool        is_key = lpFrontHeader->pk;
	uint32_t    ts_ms = lpFrontHeader->ts;
	uint32_t    min_seq = lpFrontHeader->seq;
	uint32_t    cur_seq = lpFrontHeader->seq;
	rtp_hdr_t * lpCurHeader = lpFrontHeader;
	uint32_t    nConsumeSize = nPerPackSize;
	string      strFrame;
	// ��������֡ => �����������pst��ʼ����ped����...
	while( true ) {
		// �����ݰ�����Ч�غɱ�������...
		char * lpData = (char*)lpCurHeader + sizeof(rtp_hdr_t);
		strFrame.append(lpData, lpCurHeader->psize);
		// ������ݰ���֡�Ľ����� => ����֡������...
		if( lpCurHeader->ped > 0 )
			break;
		// ������ݰ�����֡�Ľ����� => ����Ѱ��...
		ASSERT( lpCurHeader->ped <= 0 );
		// �ۼӰ����кţ���ͨ�����к��ҵ���ͷλ��...
		uint32_t nPosition = (++cur_seq - min_seq) * nPerPackSize;
		// �����ͷ��λλ�ó����˻��ζ����ܳ��� => ˵���Ѿ����ﻷ�ζ���ĩβ => ֱ�ӷ��أ���Ϣ�ȴ�...
		if( nPosition >= m_circle.size )
			return;
		// �ҵ�ָ����ͷλ�õ�ͷָ��ṹ��...
		lpCurHeader = (rtp_hdr_t*)circlebuf_data(&m_circle, nPosition);
		// ����µ����ݰ�������Ч����Ƶ���ݰ� => ���صȴ�����...
		if( lpCurHeader->pt == PT_TAG_LOSE )
			return;
		ASSERT( lpCurHeader->pt != PT_TAG_LOSE );
		// ����µ����ݰ�����������Ű� => ���صȴ�...
		if( cur_seq != lpCurHeader->seq )
			return;
		ASSERT( cur_seq == lpCurHeader->seq );
		// ����ַ�����֡��ʼ��� => ����ѽ�������֡ => �������֡����������Ҫ����...
		// ͬʱ����Ҫ������ʱ��ŵ�����֡�����Ϣ�����¿�ʼ��֡...
		if( lpCurHeader->pst > 0 ) {
			pt_type = lpCurHeader->pt;
			is_key = lpCurHeader->pk;
			ts_ms = lpCurHeader->ts;
			strFrame.clear();
		}
		// �ۼ��ѽ��������ݰ��ܳ���...
		nConsumeSize += nPerPackSize;
	}
	// ���û�н���������֡ => ��ӡ������Ϣ...
	if( strFrame.size() <= 0 ) {
		uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
		TRACE("[Teacher-Looker] Time: %lu ms, Error => Frame size is Zero, PlaySeq: %lu, Type: %d, Key: %d\n", now_ms, m_nMaxPlaySeq, pt_type, is_key);
		return;
	}
	// ������ζ��б�ȫ����ɣ�ֱ�ӷ���...
	if( nConsumeSize >= m_circle.size )
		return;
	// ��ǰ�ѽ��������кű���Ϊ��ǰ��󲥷����к�...
	m_nMaxPlaySeq = cur_seq;
	
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// ���Դ��� => ��Ҫɾ�������ݰ���Ϣ���浽�ļ�����...
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/*uint32_t nTestSeq = min_seq;
	while( nTestSeq <= cur_seq ) {
		DoSaveRecvSeq(lpFrontHeader->seq, lpFrontHeader->psize, lpFrontHeader->pst, lpFrontHeader->ped, lpFrontHeader->ts);
		uint32_t nPosition = (++nTestSeq - min_seq) * nPerPackSize;
		if( nPosition >= m_circle.size ) break;
		lpFrontHeader = (rtp_hdr_t*)circlebuf_data(&m_circle, nPosition);
	}*/

	// ɾ���ѽ�����ϵĻ��ζ������ݰ� => ���ջ�����...
	circlebuf_pop_front(&m_circle, NULL, nConsumeSize);
	// ������������Ч����֡���벥�Ŷ�����...
	if( m_lpPlaySDL != NULL ) {
		m_lpPlaySDL->PushFrame(strFrame, pt_type, is_key, ts_ms);
	}
	// ��ӡ��Ͷ�ݵ���������֡��Ϣ...
	//uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	//TRACE( "[Teacher-Looker] Time: %lu ms, Frame => Type: %d, Key: %d, PTS: %lu, Size: %d, PlaySeq: %lu, CircleSize: %d\n", 
	//		now_ms, pt_type, is_key, ts_ms, strFrame.size(), m_nMaxPlaySeq, m_circle.size/nPerPackSize );
	//DoSaveRecvFile(ts_ms, pt_type, is_key, strFrame.size());
	// �޸���Ϣ״̬ => �Ѿ���ȡ��������Ƶ����֡��������Ϣ...
	m_bNeedSleep = false;
}

void CUDPRecvThread::doEraseLoseSeq(uint32_t inSeqID)
{
	// ���û���ҵ�ָ�������кţ�ֱ�ӷ���...
	GM_MapLose::iterator itorItem = m_MapLose.find(inSeqID);
	if( itorItem == m_MapLose.end() )
		return;
	// ɾ����⵽�Ķ����ڵ�...
	rtp_lose_t & rtpLose = itorItem->second;
	uint32_t nResendCount = rtpLose.resend_count;
	m_MapLose.erase(itorItem);
	// ��ӡ���յ��Ĳ�����Ϣ����ʣ�µ�δ��������...
	uint32_t now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	TRACE("[Teacher-Looker] Time: %lu ms, Supply Erase => LoseSeq: %lu, ResendCount: %lu, LoseSize: %lu\n", now_ms, inSeqID, nResendCount, m_MapLose.size());
}
//
// ����ȡ������Ƶ���ݰ����뻷�ζ��е���...
void CUDPRecvThread::doTagAVPackProcess(char * lpBuffer, int inRecvLen)
{
	// �ж��������ݰ�����Ч��...
	if( lpBuffer == NULL || inRecvLen < 0 || inRecvLen < sizeof(rtp_hdr_t) )
		return;
	/////////////////////////////////////////////////////////////////////////////////
	// ע�⣺����ֻ��Ҫ�ж�����ͷ�Ƿ񵽴�����ж������˵�׼���������Ƿ񵽴�...
	// ע�⣺��Ϊ��������Ƶ���ݰ�������׼����������������Ļ��ͻ���ɶ���...
	/////////////////////////////////////////////////////////////////////////////////
	// ���û���յ�������ͷ��ֱ�ӷ���...
	if( m_rtp_header.pt != PT_TAG_HEADER )
		return;
	// ��û����Ƶ��Ҳû����Ƶ��ֱ�ӷ���...
	if( !m_rtp_header.hasAudio && !m_rtp_header.hasVideo )
		return;
	// ����յ��Ļ��������Ȳ��� �� �����Ϊ������ֱ�Ӷ���...
	rtp_hdr_t * lpNewHeader = (rtp_hdr_t*)lpBuffer;
	int nDataSize = lpNewHeader->psize + sizeof(rtp_hdr_t);
	int nZeroSize = DEF_MTU_SIZE - lpNewHeader->psize;
	uint32_t new_id = lpNewHeader->seq;
	uint32_t max_id = new_id;
	uint32_t min_id = new_id;
	uint32_t now_ms = 0;
	if( inRecvLen != nDataSize || nZeroSize < 0 ) {
		now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
		TRACE("[Teacher-Looker] Time: %lu ms, Discard => Seq: %lu, TS: %lu, Type: %d, Slice: %d, ZeroSize: %d\n", now_ms, lpNewHeader->seq, lpNewHeader->ts, lpNewHeader->pt, lpNewHeader->psize, nZeroSize);
		return;
	}
	// ����յ��Ĳ�����ȵ�ǰ��󲥷Ű���ҪС => ˵���Ƕ�β������������ֱ���ӵ�...
	// ע�⣺��ʹ���ҲҪ�ӵ�����Ϊ��󲥷���Ű������Ѿ�Ͷ�ݵ��˲��Ų㣬�Ѿ���ɾ����...
	if( new_id <= m_nMaxPlaySeq ) {
		now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
		TRACE("[Teacher-Looker] Time: %lu ms, Supply Discard => Seq: %lu, MaxPlaySeq: %lu\n", now_ms, new_id, m_nMaxPlaySeq);
		return;
	}
	// ��ӡ�յ�����Ƶ���ݰ���Ϣ => ��������������� => ÿ�����ݰ�����ͳһ��С => rtp_hdr_t + slice + Zero => 812
	//TRACE("[Teacher-Looker] Seq: %lu, TS: %lu, Type: %d, pst: %d, ped: %d, Slice: %d, ZeroSize: %d\n", lpNewHeader->seq, lpNewHeader->ts, lpNewHeader->pt, lpNewHeader->pst, lpNewHeader->ped, lpNewHeader->psize, nZeroSize);
	// ���ȣ�����ǰ�����кŴӶ������е���ɾ��...
	this->doEraseLoseSeq(new_id);
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺ÿ�����ζ����е����ݰ���С��һ���� => rtp_hdr_t + slice + Zero => 12 + 800 => 812
	//////////////////////////////////////////////////////////////////////////////////////////////////
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacket[nPerPackSize] = {0};
	// ������ζ���Ϊ�� => ֱ�ӽ����ݼ��뵽���ζ��е���...
	if( m_circle.size < nPerPackSize ) {
		// �ȼ����ͷ���������� => 
		circlebuf_push_back(&m_circle, lpBuffer, inRecvLen);
		// �ټ�������������ݣ���֤�������Ǳ���һ��MTU��Ԫ��С...
		if( nZeroSize > 0 ) {
			circlebuf_push_back_zero(&m_circle, nZeroSize);
		}
		return;
	}
	// ���ζ���������Ҫ��һ�����ݰ�...
	ASSERT( m_circle.size >= nPerPackSize );
	// ��ȡ���ζ�������С���к�...
	circlebuf_peek_front(&m_circle, szPacket, nPerPackSize);
	rtp_hdr_t * lpMinHeader = (rtp_hdr_t*)szPacket;
	min_id = lpMinHeader->seq;
	// ��ȡ���ζ�����������к�...
	circlebuf_peek_back(&m_circle, szPacket, nPerPackSize);
	rtp_hdr_t * lpMaxHeader = (rtp_hdr_t*)szPacket;
	max_id = lpMaxHeader->seq;
	// ������������ => max_id + 1 < new_id
	if( max_id + 1 < new_id ) {
		// �������� => [max_id + 1, new_id - 1];
		uint32_t sup_id = max_id + 1;
		rtp_hdr_t rtpDis = {0};
		rtpDis.pt = PT_TAG_LOSE;
		while( sup_id < new_id ) {
			// ����ǰ����Ԥ��������...
			rtpDis.seq = sup_id;
			circlebuf_push_back(&m_circle, &rtpDis, sizeof(rtpDis));
			circlebuf_push_back_zero(&m_circle, DEF_MTU_SIZE);
			// ��������ż��붪�����е��� => ����ʱ�̵�...
			rtp_lose_t rtpLose = {0};
			rtpLose.resend_count = 0;
			rtpLose.lose_seq = sup_id;
			// ע�⣺����Ҫ���� ���綶��ʱ��� Ϊ��������� => ��û����ɵ�һ��̽������...
			// �ط�ʱ��� => cur_time + rtt_var => ����ʱ�ĵ�ǰʱ�� + ����ʱ�����綶��ʱ���
			rtpLose.resend_time = (uint32_t)(CUtilTool::os_gettime_ns() / 1000000) + max(m_rtt_var_ms,0);
			m_MapLose[sup_id] = rtpLose;
			// ��ӡ�Ѷ�����Ϣ���������г���...
			now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
			TRACE("[Teacher-Looker] Time: %lu ms, Lose Seq: %lu, LoseSize: %lu\n", now_ms, sup_id, m_MapLose.size());
			// �ۼӵ�ǰ�������к�...
			++sup_id;
		}
	}
	// ����Ƕ�����������Ű������뻷�ζ��У�����...
	if( max_id + 1 <= new_id ) {
		// �ȼ����ͷ����������...
		circlebuf_push_back(&m_circle, lpBuffer, inRecvLen);
		// �ټ�������������ݣ���֤�������Ǳ���һ��MTU��Ԫ��С...
		if( nZeroSize > 0 ) {
			circlebuf_push_back_zero(&m_circle, nZeroSize);
		}
		// ��ӡ�¼���������Ű�...
		//TRACE("[Teacher-Looker] Max Seq: %lu\n", new_id);
		return;
	}
	// ����Ƕ�����Ĳ���� => max_id > new_id
	if( max_id > new_id ) {
		// �����С��Ŵ��ڶ������ => ��ӡ����ֱ�Ӷ�����������...
		if( min_id > new_id ) {
			now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
			TRACE("[Teacher-Looker] Time: %lu ms, Supply Discard => Seq: %lu, Min-Max: [%lu, %lu]\n", now_ms, new_id, min_id, max_id);
			return;
		}
		// ��С��Ų��ܱȶ������С...
		ASSERT( min_id <= new_id );
		// ���㻺��������λ��...
		uint32_t nPosition = (new_id - min_id) * nPerPackSize;
		// ����ȡ���������ݸ��µ�ָ��λ��...
		circlebuf_place(&m_circle, nPosition, lpBuffer, inRecvLen);
		// ��ӡ�������Ϣ...
		now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
		TRACE("[Teacher-Looker] Time: %lu ms, Supply Success => Seq: %lu, Min-Max: [%lu, %lu]\n", now_ms, new_id, min_id, max_id);
		return;
	}
	// ���������δ֪������ӡ��Ϣ...
	now_ms = (uint32_t)(CUtilTool::os_gettime_ns()/1000000);
	TRACE("[Teacher-Looker] Time: %lu ms, Supply Unknown => Seq: %lu, Slice: %d, Min-Max: [%lu, %lu]\n", now_ms, new_id, lpNewHeader->psize, min_id, max_id);
}
///////////////////////////////////////////////////////
// ע�⣺û�з�����Ҳû���հ�����Ҫ������Ϣ...
///////////////////////////////////////////////////////
void CUDPRecvThread::doSleepTo()
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