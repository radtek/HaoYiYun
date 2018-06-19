
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
	m_rtp_detect.tm = m_rtp_create.tm = m_rtp_delete.tm = m_rtp_ready.tm = m_rtp_supply.tm = TM_TAG_TEACHER; // TM_TAG_STUDENT
	m_rtp_detect.id = m_rtp_create.id = m_rtp_delete.id = m_rtp_ready.id = m_rtp_supply.id = ID_TAG_LOOKER;
	m_rtp_detect.pt = PT_TAG_DETECT;
	m_rtp_create.pt = PT_TAG_CREATE;
	m_rtp_delete.pt = PT_TAG_DELETE;
	m_rtp_supply.pt = PT_TAG_SUPPLY;
	m_rtp_ready.pt  = PT_TAG_READY;
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
	// �ر�UDPSocket����...
	this->CloseSocket();
	// �ͷŻ��ζ��пռ�...
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
	// ��ӡ�ѷ���ɾ�������...
	TRACE("[Teacher-Looker] Send Delete RoomID: %lu, LiveID: %d\n", m_rtp_delete.roomID, m_rtp_delete.liveID);
	return;
}

void CUDPRecvThread::doSendCreateCmd()
{
	OSMutexLocker theLock(&m_Mutex);
	// ����Ѿ��յ���ȷ������ͷ => ����ͷ | ������ | ѧ����...
	if( m_rtp_header.pt == PT_TAG_HEADER && m_rtp_header.id == ID_TAG_PUSHER && m_rtp_header.tm == TM_TAG_STUDENT )
		return;
	// ÿ��100���뷢�ʹ�������� => ����ת�����з���...
	int64_t cur_time_ns = CUtilTool::os_gettime_ns();
	int64_t period_ns = 100000000;
	// �������ʱ�仹û����ֱ�ӷ���...
	if( m_next_create_ns > cur_time_ns )
		return;
	ASSERT( cur_time_ns >= m_next_create_ns );
	// ����һ�������������� => �൱�ڵ�¼ע��...
	GM_Error theErr = m_lpUDPSocket->SendTo((void*)&m_rtp_create, sizeof(m_rtp_create));
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
		return;
	}
	// ��ӡ�ѷ��ʹ��������...
	TRACE("[Teacher-Looker] Send Create RoomID: %lu, LiveID: %d\n", m_rtp_create.roomID, m_rtp_create.liveID);
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

void CUDPRecvThread::doSendReadyCmd()
{
	OSMutexLocker theLock(&m_Mutex);
	// ��û���յ�����ͷ���ֱ�ӷ��� => ����ͷ | ������ | ѧ���� ...
	if( m_rtp_header.pt != PT_TAG_HEADER || m_rtp_header.id != ID_TAG_PUSHER || m_rtp_header.tm != TM_TAG_STUDENT  )
		return;
	ASSERT( m_rtp_header.pt == PT_TAG_HEADER && m_rtp_header.id == ID_TAG_PUSHER && m_rtp_header.tm == TM_TAG_STUDENT );
	// ����Ѿ��յ�������Ƶ���ݰ���ֱ�ӷ���...
	if( !m_bSendReady )
		return;
	// ÿ��100���뷢�;�������� => ����ת�����з���...
	int64_t cur_time_ns = CUtilTool::os_gettime_ns();
	int64_t period_ns = 100000000;
	// �������ʱ�仹û����ֱ�ӷ���...
	if( m_next_ready_ns > cur_time_ns )
		return;
	ASSERT( cur_time_ns >= m_next_ready_ns );
	// ����һ��׼����������� => ֪ͨѧ���˿�ʼ��������Ƶ���ݰ�...
	GM_Error theErr = m_lpUDPSocket->SendTo((void*)&m_rtp_ready, sizeof(m_rtp_ready));
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
		return;
	}
	// ��ӡ�ѷ���׼�����������...
	TRACE("[Teacher-Looker] Send Ready command\n");
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
	const int nPackSize = DEF_MTU_SIZE + nHeadSize;
	static char szPacket[nPackSize] = {0};
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
			if( (nHeadSize + m_rtp_supply.suSize) >= nPackSize )
				break;
			// �ۼӲ������ȣ������������к�...
			m_rtp_supply.suSize += sizeof(int);
			memcpy(lpData, &rtpLose.lose_seq, sizeof(int));
			// �ۼ�����ָ��...
			lpData += sizeof(int);
			// �ۼ��ط�����...
			++rtpLose.resend_count;
			// �����´��ش�ʱ��� => cur_time + rtt_var => ����ʱ�ĵ�ǰʱ�� + ����ʱ�����綶��ʱ���
			rtpLose.resend_time = (uint32_t)(CUtilTool::os_gettime_ns() / 1000000) + 10;//m_rtt_var;
		}
		// �ۼӶ������Ӷ���...
		++itorItem;
	}
	// ������������Ϊ�գ�ֱ�ӷ���...
	if( m_rtp_supply.suSize <= 0 )
		return;
	// ��ӡ�ѷ��Ͳ�������...
	TRACE("[Teacher-Looker] Supply Send => Count: %d\n", m_rtp_supply.suSize / sizeof(int));
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
	if( theErr != GM_NoErr )
		return;
	// �޸���Ϣ״̬ => �Ѿ��ɹ��հ���������Ϣ...
	m_bNeedSleep = false;
	
    // ��ȡ��һ���ֽڵĸ�4λ���õ����ݰ�����...
    uint8_t ptTag = (ioBuffer[0] >> 4) & 0x0F;
	
	// ���յ�������������ͷַ�...
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
// ������������͹������ؽ�����...
void CUDPRecvThread::doTagReloadProcess(char * lpBuffer, int inRecvLen)
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
		uint32_t cur_time_sec = (uint32_t)(CUtilTool::os_gettime_ns() / 1000000000);
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
	m_rtp_reload.reload_time = (uint32_t)(CUtilTool::os_gettime_ns() / 1000000);
	++m_rtp_reload.reload_count;
	// ��ӡ�յ��������ؽ�����...
	TRACE("[Teacher-Looker] Server Reload Count: %d\n", m_rtp_reload.reload_count);
	// ������������...
	memset(&m_rtp_header, 0, sizeof(m_rtp_header));
	// ��ղ������϶���...
	m_MapLose.clear();
	// �ͷŻ��ζ��пռ�...
	circlebuf_free(&m_circle);
	// ��ʼ�����ζ��У�Ԥ����ռ�...
	circlebuf_init(&m_circle);
	circlebuf_reserve(&m_circle, 512*1024);
	// ������ر���...
	m_next_create_ns = -1;
	m_next_detect_ns = -1;
	m_next_ready_ns = -1;
	m_bSendReady = true;
	// ���¿�ʼ̽������...
	m_rtp_detect.tsSrc = 0;
	m_rtp_detect.dtNum = 0;
	m_rtt_var = -1;
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
		if( m_rtt_var < 0 ) { m_rtt_var = m_rtt_ms; }
		else { m_rtt_var = (m_rtt_var * 3 + abs(m_rtt_ms - keep_rtt)) / 4; }
		// ��ӡ̽���� => ̽����� | ������ʱ(����)...
		TRACE("[Teacher-Looker] Recv Detect dtNum: %d, rtt: %d ms, rtt_var: %d ms\n", rtpDetect.dtNum, m_rtt_ms, m_rtt_var);
	}
}

void CUDPRecvThread::doTagHeaderProcess(char * lpBuffer, int inRecvLen)
{
	if( lpBuffer == NULL || inRecvLen < 0 || inRecvLen < sizeof(rtp_header_t) )
		return;
	// ����Ѿ��յ�������ͷ��ֱ�ӷ���...
	if( m_rtp_header.pt == PT_TAG_HEADER || m_rtp_header.hasAudio || m_rtp_header.hasVideo )
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
	// ��ӡ�յ�����ͷ�ṹ����Ϣ...
	TRACE("[Teacher-Looker] Recv Header SPS: %d, PPS: %d\n", m_strSPS.size(), m_strPPS.size());
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
	TRACE("[Teacher-Looker] Supply Erase => LoseSeq: %lu, ResendCount: %lu, LoseSize: %lu\n", inSeqID, nResendCount, m_MapLose.size());
}

void CUDPRecvThread::doTagAVPackProcess(char * lpBuffer, int inRecvLen)
{
	if( lpBuffer == NULL || inRecvLen < 0 || inRecvLen < sizeof(rtp_hdr_t) )
		return;
	// ���û���յ�������ͷ��ֱ�ӷ���...
	if( m_rtp_header.pt != PT_TAG_HEADER )
		return;
	// ��û����Ƶ��Ҳû����Ƶ��ֱ�ӷ���...
	if( !m_rtp_header.hasAudio && !m_rtp_header.hasVideo )
		return;
	// ���ò�Ҫ�ٷ���׼�������������...
	m_bSendReady = false;
	// ����յ��Ļ��������Ȳ��� �� �����Ϊ������ֱ�Ӷ���...
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
	// ��ӡ�յ�����Ƶ���ݰ���Ϣ => ��������������� => ÿ�����ݰ�����ͳһ��С => rtp_hdr_t + slice + Zero => 812
	//TRACE("[Teacher-Looker] Seq: %lu, TS: %lu, Type: %d, pst: %d, ped: %d, Slice: %d, ZeroSize: %d\n", lpNewHeader->seq, lpNewHeader->ts, lpNewHeader->pt, lpNewHeader->pst, lpNewHeader->ped, lpNewHeader->psize, nZeroSize);
	// ���ȣ�����ǰ�����кŴӶ������е���ɾ��...
	this->doEraseLoseSeq(new_id);
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺ÿ�����ζ����е����ݰ���С��һ���� => rtp_hdr_t + slice + Zero => 12 + 800 => 812
	//////////////////////////////////////////////////////////////////////////////////////////////////
	const int nPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacket[nPackSize] = {0};
	// ������ζ���Ϊ�� => ֱ�ӽ����ݼ��뵽���ζ��е���...
	if( m_circle.size < nPackSize ) {
		// �ȼ����ͷ���������� => 
		circlebuf_push_back(&m_circle, lpBuffer, inRecvLen);
		// �ټ�������������ݣ���֤�������Ǳ���һ��MTU��Ԫ��С...
		if( nZeroSize > 0 ) {
			circlebuf_push_back_zero(&m_circle, nZeroSize);
		}
		return;
	}
	// ���ζ���������Ҫ��һ�����ݰ�...
	ASSERT( m_circle.size >= nPackSize );
	// ��ȡ���ζ�������С���к�...
	circlebuf_peek_front(&m_circle, szPacket, nPackSize);
	rtp_hdr_t * lpMinHeader = (rtp_hdr_t*)szPacket;
	min_id = lpMinHeader->seq;
	// ��ȡ���ζ�����������к�...
	circlebuf_peek_back(&m_circle, szPacket, nPackSize);
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
			// �ط�ʱ��� => cur_time + rtt_var => ����ʱ�ĵ�ǰʱ�� + ����ʱ�����綶��ʱ���
			rtpLose.resend_time = (uint32_t)(CUtilTool::os_gettime_ns() / 1000000) + 10;//m_rtt_var;
			m_MapLose[sup_id] = rtpLose;
			// ��ӡ�Ѷ�����Ϣ���������г���...
			TRACE("[Teacher-Looker] Lose Seq: %lu, LoseSize: %lu\n", sup_id, m_MapLose.size());
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
			TRACE("[Teacher-Looker] Supply Discard => Seq: %lu, Min-Max: [%lu, %lu]\n", new_id, min_id, max_id);
			return;
		}
		// ��С��Ų��ܱȶ������С...
		ASSERT( min_id <= new_id );
		// ���㻺��������λ��...
		uint32_t nPosition = (new_id - min_id) * nPackSize;
		// ����ȡ���������ݸ��µ�ָ��λ��...
		circlebuf_place(&m_circle, nPosition, lpBuffer, inRecvLen);
		// ��ӡ�������Ϣ...
		TRACE("[Teacher-Looker] Supply Success => Seq: %lu, Min-Max: [%lu, %lu]\n", new_id, min_id, max_id);
		return;
	}
	// ���������δ֪������ӡ��Ϣ...
	TRACE("[Teacher-Looker] Supply Unknown => Seq: %lu, Slice: %d, Min-Max: [%lu, %lu]\n", new_id, lpNewHeader->psize, min_id, max_id);
}
///////////////////////////////////////////////////////
// ע�⣺û�з�����Ҳû���հ�����Ҫ������Ϣ...
///////////////////////////////////////////////////////
void CUDPRecvThread::doSleepTo()
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