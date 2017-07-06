
#include "stdafx.h"
#include "LibRtmp.h"
#include "LibMP4.h"
#include "UtilTool.h"
#include "RecThread.h"
#include "..\Camera.h"
#include "..\ReadSPS.h"
#include "..\librtmp\BitWritter.h"

CRecThread::CRecThread()
{
	m_nTaskID = -1;
	m_strURL = "";
	m_strMP4Path = "";
	m_lpLibMP4 = NULL;
	m_hWndParent = NULL;
	m_dwWriteSize = 0;
	m_dwWriteRecMS = 0;
	m_dwRecSliceMS = 0;
}

CRecThread::~CRecThread()
{
}

bool CRecThread::CreateAudioTrack(int nAudioSampleRate, int nAudioChannel)
{
	// �ж�MP4�����Ƿ���Ч...
	if( m_lpLibMP4 == NULL || m_strMP4Path.size() <= 0 )
		return false;
	ASSERT( m_lpLibMP4 != NULL && m_strMP4Path.size() > 0 );

	// ���Ƚ������洢���ݹ����Ĳ���...
	int audio_rate_index = 0;
	if (nAudioSampleRate == 48000)
		audio_rate_index = 0x03;
	else if (nAudioSampleRate == 44100)
		audio_rate_index = 0x04;
	else if (nAudioSampleRate == 32000)
		audio_rate_index = 0x05;
	else if (nAudioSampleRate == 24000)
		audio_rate_index = 0x06;
	else if (nAudioSampleRate == 22050)
		audio_rate_index = 0x07;
	else if (nAudioSampleRate == 16000)
		audio_rate_index = 0x08;
	else if (nAudioSampleRate == 12000)
		audio_rate_index = 0x09;
	else if (nAudioSampleRate == 11025)
		audio_rate_index = 0x0a;
	else if (nAudioSampleRate == 8000)
		audio_rate_index = 0x0b;
	// AudioSpecificConfig
	char   aac_seq_buf[2048] = {0};
    char * pbuf = aac_seq_buf;
	PutBitContext pb;
	init_put_bits(&pb, pbuf, 1024);
	put_bits(&pb, 5, 2);				//object type - AAC-LC
	put_bits(&pb, 4, audio_rate_index);	// sample rate index
	put_bits(&pb, 4, nAudioChannel);	// channel configuration

	//GASpecificConfig
	put_bits(&pb, 1, 0);    // frame length - 1024 samples
	put_bits(&pb, 1, 0);    // does not depend on core coder
	put_bits(&pb, 1, 0);    // is not extension

	flush_put_bits(&pb);
	pbuf += 2;

	// ���� AES ����ͷ��Ϣ...
	string strAES;
	int aac_len = (int)(pbuf - aac_seq_buf);
	strAES.assign(aac_seq_buf, aac_len);
	// ������Ƶ���...
	if( !m_lpLibMP4->IsAudioCreated() ) {
		if( !m_lpLibMP4->CreateAudioTrack(m_strMP4Path.c_str(), nAudioSampleRate, strAES) )
			return false;
	}
	// ������ȷֵ...
	ASSERT( m_lpLibMP4->IsAudioCreated() );
	return true;
}

bool CRecThread::CreateVideoTrack(string & strSPS, string & strPPS)
{
	// �ж�MP4�����Ƿ���Ч...
	if( m_lpLibMP4 == NULL || m_strMP4Path.size() <= 0 )
		return false;
	ASSERT( m_lpLibMP4 != NULL && m_strMP4Path.size() > 0 );

	// ��ȡ width �� height...
	int	nPicWidth = 0;
	int	nPicHeight = 0;
	if( strSPS.size() >  0 ) {
		CSPSReader _spsreader;
		bs_t    s = {0};
		s.p		  = (uint8_t *)strSPS.c_str();
		s.p_start = (uint8_t *)strSPS.c_str();
		s.p_end	  = (uint8_t *)strSPS.c_str() + strSPS.size();
		s.i_left  = 8; // ����ǹ̶���,���볤��...
		_spsreader.Do_Read_SPS(&s, &nPicWidth, &nPicHeight);
	}

	// ������Ƶ���...
	if( !m_lpLibMP4->IsVideoCreated() ) {
		if( !m_lpLibMP4->CreateVideoTrack(m_strMP4Path.c_str(), VIDEO_TIME_SCALE, nPicWidth, nPicHeight, strSPS, strPPS) )
			return false;
	}
	// ������ȷֵ...
	ASSERT( m_lpLibMP4->IsVideoCreated() );
	return true;
}

bool CRecThread::WriteSample(bool bIsVideo, BYTE * lpFrame, int nSize, DWORD inTimeStamp, DWORD inRenderOffset, bool bIsKeyFrame)
{
	// �жϽӿ��Ƿ���Ч...
	if( m_lpLibMP4 == NULL )
		return false;
	ASSERT( m_lpLibMP4 != NULL );
	// ���ýӿڣ�ֱ��д��...
	if( !m_lpLibMP4->WriteSample(bIsVideo, lpFrame, nSize, inTimeStamp, inRenderOffset, bIsKeyFrame) )
		return false;
	// �ۼӴ��̳��ȣ���¼�Ѿ�д���ʱ���...
	m_dwWriteSize = m_lpLibMP4->GetWriteSize();
	m_dwWriteRecMS = m_lpLibMP4->GetWriteRecMS();
	// ��ӡ������Ϣ...
	//TRACE("IsVideo = %d, IsKeyFrame = %d, TimeStamp = %lu \n", bIsVideo, bIsKeyFrame, inTimeStamp);
	// ����Ҫ��Ƭ������ֱ�ӷ���...
	if( m_dwRecSliceMS <= 0 )
		return true;
	ASSERT( m_dwRecSliceMS > 0 );
	// ������Ƭʱ�䣬֪ͨ��Ƭ����λ����ֹ�ٴ�֪ͨ...
	/*if( inTimeStamp >= m_dwRecSliceMS ) {
		m_dwRecSliceMS = 0;
		ASSERT( m_hWndParent != NULL );
		::PostMessage(m_hWndParent, WM_REC_SLICE_MSG, NULL, m_nTaskID);
	}*/
	//uint64_t nDuration = m_lpLibMP4->GetDuration();
	//TRACE("IsVideo = %d, Duration = %I64d, MS = %lu\n", bIsVideo, nDuration, nDuration / (VIDEO_TIME_SCALE / 1000));
	return true;
}

float CRecThread::GetRecSizeM()
{
	OSMutexLocker  theLock(&m_Mutex);
	return (m_dwWriteSize / 1024.0f / 1024.0f);
}

CRtmpRecThread::CRtmpRecThread(HWND hWndParent)
{
	m_lpRtmp = NULL;
	m_hWndParent = hWndParent;
	ASSERT( m_hWndParent != NULL );
}

CRtmpRecThread::~CRtmpRecThread()
{
	// ֹͣ�߳�...
	this->StopAndWaitForThread();

	// �ͷ�MP4����...
	if( m_lpLibMP4 != NULL ) {
		delete m_lpLibMP4;
		m_lpLibMP4 = NULL;
	}

	// �ͷ���������...
	if( m_lpRtmp != NULL ) {
		delete m_lpRtmp;
		m_lpRtmp = NULL;
	}

	// ��ӡ�˳���Ϣ...
	TRACE("[~CRtmpRecThread Thread] - Exit\n");
}

bool CRtmpRecThread::InitThread(int nTaskID, LPCTSTR lpszURL, LPCTSTR lpszPath)
{
	if( lpszURL == NULL || lpszPath == NULL )
		return false;

	// ���洫�ݹ����Ĳ���...
	m_nTaskID = nTaskID;
	m_strURL = lpszURL;

	// MP4·��������UTF-8�ĸ�ʽ...
	if( lpszPath != NULL && strlen(lpszPath) > 0 ) {
		m_strMP4Path = CUtilTool::ANSI_UTF8(lpszPath);
	}

	// �ͷ�MP4����...
	if( m_lpLibMP4 != NULL ) {
		delete m_lpLibMP4;
		m_lpLibMP4 = NULL;
	}

	// ���� MP4 ����...
	m_lpLibMP4 = new LibMP4();
	ASSERT( m_lpLibMP4 != NULL );

	// �ͷ�rtmp����...
	if( m_lpRtmp != NULL ) {
		delete m_lpRtmp;
		m_lpRtmp = NULL;
	}

	// ����rtmp����...
	m_lpRtmp = new LibRtmp(false, false, NULL, this);
	ASSERT( m_lpRtmp != NULL );

	// �����߳�...
	this->Start();

	return true;
}

void CRtmpRecThread::Entry()
{
	// �ж�rtmp�������Ч��...
	if( m_lpRtmp == NULL ) {
		this->doErrNotify();
		return;
	}

	// ����rtmp������...
	ASSERT( m_lpRtmp != NULL );
	if( !m_lpRtmp->Open(m_strURL.c_str()) ) {
		delete m_lpRtmp; m_lpRtmp = NULL;
		this->doErrNotify();
		return;
	}

	// ѭ����ȡ���ݲ�����...
	while( !this->IsStopRequested() ) {
		if( m_lpRtmp->IsClosed() || !m_lpRtmp->Read() ) {
			this->doErrNotify();
			return;
		}
	}
}

void CRtmpRecThread::doErrNotify()
{
	ASSERT( m_hWndParent != NULL );
	::PostMessage(m_hWndParent, WM_ERR_TASK_MSG, NULL, m_nTaskID);
}

/////////////////////////////////////////////////
// �����й� rtsp ֱ��¼��Ĵ���ʵ��...
/////////////////////////////////////////////////
CRtspRecThread::CRtspRecThread(HWND hWndParent, CCamera * lpCamera, DWORD dwRecSliceMS)
{
	m_lpCamera = lpCamera;
	m_hWndParent = hWndParent;
	m_dwRecSliceMS = dwRecSliceMS;
	ASSERT( m_lpCamera != NULL );
	ASSERT( m_hWndParent != NULL );

	m_env_ = NULL;
	m_scheduler_ = NULL;
	m_rtspClient_ = NULL;
	m_rtspEventLoopWatchVariable = 0;
}

CRtspRecThread::~CRtspRecThread()
{
	// ����rtspѭ���˳���־...
	m_rtspEventLoopWatchVariable = 1;

	// ֹͣ�߳�...
	this->StopAndWaitForThread();

	// �ͷ�MP4����...
	if( m_lpLibMP4 != NULL ) {
		delete m_lpLibMP4;
		m_lpLibMP4 = NULL;
	}

	// ֪ͨIPC���Զ�¼����и�������...
	if( m_lpCamera != NULL ) {
		m_lpCamera->doRecEnd(m_dwWriteRecMS/1000);
	}

	// ��ӡ�˳���Ϣ...
	TRACE("[~CRtspRecThread Thread] - Exit\n");
}

bool CRtspRecThread::InitThread(int nTaskID, LPCTSTR lpszURL, LPCTSTR lpszPath)
{
	if( lpszURL == NULL || lpszPath == NULL )
		return false;

	// ���洫�ݹ����Ĳ���...
	m_nTaskID = nTaskID;
	m_strURL = lpszURL;

	// MP4·��������UTF-8�ĸ�ʽ...
	if( lpszPath != NULL && strlen(lpszPath) > 0 ) {
		m_strMP4Path = CUtilTool::ANSI_UTF8(lpszPath);
	}

	// �ͷ�MP4����...
	if( m_lpLibMP4 != NULL ) {
		delete m_lpLibMP4;
		m_lpLibMP4 = NULL;
	}

	// ���� MP4 ����...
	m_lpLibMP4 = new LibMP4();
	ASSERT( m_lpLibMP4 != NULL );

	// ����rtsp���ӻ���...
	m_scheduler_ = BasicTaskScheduler::createNew();
	m_env_ = BasicUsageEnvironment::createNew(*m_scheduler_);
	m_rtspClient_ = ourRTSPClient::createNew(*m_env_, m_strURL.c_str(), 1, "rtspRecord", NULL, this);
	
	// �����һ��rtsp����...
	m_rtspClient_->sendDescribeCommand(continueAfterDESCRIBE); 

	// �����߳�...
	this->Start();

	return true;
}

void CRtspRecThread::Entry()
{
	// ��������ѭ����⣬�޸� g_rtspEventLoopWatchVariable �����������˳�...
	ASSERT( m_env_ != NULL && m_rtspClient_ != NULL );
	m_env_->taskScheduler().doEventLoop(&m_rtspEventLoopWatchVariable);

	// ֪ͨ���̣߳���Ҫ�˳�...
	this->doErrNotify();

	// �����˳�֮�����ͷ�rtsp�����Դ...
	// ֻ����������� shutdownStream() �����ط���Ҫ����...
	if( m_rtspClient_ != NULL ) {
		m_rtspClient_->shutdownStream();
		m_rtspClient_ = NULL;
	}

	// �ͷ��������...
	if( m_scheduler_ != NULL ) {
		delete m_scheduler_;
		m_scheduler_ = NULL;
	}

	// �ͷŻ�������...
	if( m_env_ != NULL ) {
		m_env_->reclaim();
		m_env_ = NULL;
	}
}

void CRtspRecThread::doErrNotify()
{
	ASSERT( m_hWndParent != NULL );
	::PostMessage(m_hWndParent, WM_ERR_TASK_MSG, NULL, m_nTaskID);
}
