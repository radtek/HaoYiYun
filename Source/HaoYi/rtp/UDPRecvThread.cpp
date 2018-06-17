
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
	//////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺������ʱģ�⽲ʦ�˹ۿ������ => ����Ӧ��Ϊѧ���ۿ���...
	//////////////////////////////////////////////////////////////////////////////////////
	// �����ն����ͺͽṹ������ => m_rtp_header => �ȴ��������...
	m_rtp_detect.tm = m_rtp_create.tm = m_rtp_delete.tm = m_rtp_ready.tm = TM_TAG_TEACHER; // TM_TAG_STUDENT
	m_rtp_detect.id = m_rtp_create.id = m_rtp_delete.id = m_rtp_ready.id = ID_TAG_LOOKER;
	m_rtp_detect.pt = PT_TAG_DETECT;
	m_rtp_create.pt = PT_TAG_CREATE;
	m_rtp_delete.pt = PT_TAG_DELETE;
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
		// �ȴ����ͻ������һ�����ݰ�...
		this->doSleepTo();
	}
	// ����һ��ɾ�������...
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
	// �����´η��ʹ��������ʱ���...
	m_next_ready_ns = CUtilTool::os_gettime_ns() + period_ns;
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
	case PT_TAG_DETECT:	 this->doTagDetectProcess(ioBuffer, outRecvLen); break;
	case PT_TAG_HEADER:	 this->doTagHeaderProcess(ioBuffer, outRecvLen); break;
	case PT_TAG_AUDIO:	 this->doTagAudioProcess(ioBuffer, outRecvLen);  break;
	case PT_TAG_VIDEO:	 this->doTagVideoProcess(ioBuffer, outRecvLen);  break;
	}
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
		int  new_rtt = cur_time_ms - rtpDetect.tsSrc;
		// ��ӡ̽���� => ̽����� | ������ʱ(����)...
		TRACE("[Teacher-Looker] Detect dtNum: %d, rtt: %d ms\n", rtpDetect.dtNum, new_rtt);
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
		TRACE("[RecvThread] Seq Header error, RecvLen: %d\n", inRecvLen);
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
	TRACE("[Teacher-Looker] SPS: %d, PPS: %d\n", m_strSPS.size(), m_strPPS.size());
}

void CUDPRecvThread::doTagAudioProcess(char * lpBuffer, int inRecvLen)
{
	if( lpBuffer == NULL || inRecvLen < 0 || inRecvLen < sizeof(rtp_hdr_t) )
		return;
	// ���ò�Ҫ�ٷ���׼�������������...
	m_bSendReady = false;
}

void CUDPRecvThread::doTagVideoProcess(char * lpBuffer, int inRecvLen)
{
	if( lpBuffer == NULL || inRecvLen < 0 || inRecvLen < sizeof(rtp_hdr_t) )
		return;
	// ���ò�Ҫ�ٷ���׼�������������...
	m_bSendReady = false;
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