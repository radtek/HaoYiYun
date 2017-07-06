
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
	// 判断MP4对象是否有效...
	if( m_lpLibMP4 == NULL || m_strMP4Path.size() <= 0 )
		return false;
	ASSERT( m_lpLibMP4 != NULL && m_strMP4Path.size() > 0 );

	// 首先解析并存储传递过来的参数...
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

	// 保存 AES 数据头信息...
	string strAES;
	int aac_len = (int)(pbuf - aac_seq_buf);
	strAES.assign(aac_seq_buf, aac_len);
	// 创建音频轨道...
	if( !m_lpLibMP4->IsAudioCreated() ) {
		if( !m_lpLibMP4->CreateAudioTrack(m_strMP4Path.c_str(), nAudioSampleRate, strAES) )
			return false;
	}
	// 返回正确值...
	ASSERT( m_lpLibMP4->IsAudioCreated() );
	return true;
}

bool CRecThread::CreateVideoTrack(string & strSPS, string & strPPS)
{
	// 判断MP4对象是否有效...
	if( m_lpLibMP4 == NULL || m_strMP4Path.size() <= 0 )
		return false;
	ASSERT( m_lpLibMP4 != NULL && m_strMP4Path.size() > 0 );

	// 获取 width 和 height...
	int	nPicWidth = 0;
	int	nPicHeight = 0;
	if( strSPS.size() >  0 ) {
		CSPSReader _spsreader;
		bs_t    s = {0};
		s.p		  = (uint8_t *)strSPS.c_str();
		s.p_start = (uint8_t *)strSPS.c_str();
		s.p_end	  = (uint8_t *)strSPS.c_str() + strSPS.size();
		s.i_left  = 8; // 这个是固定的,对齐长度...
		_spsreader.Do_Read_SPS(&s, &nPicWidth, &nPicHeight);
	}

	// 创建视频轨道...
	if( !m_lpLibMP4->IsVideoCreated() ) {
		if( !m_lpLibMP4->CreateVideoTrack(m_strMP4Path.c_str(), VIDEO_TIME_SCALE, nPicWidth, nPicHeight, strSPS, strPPS) )
			return false;
	}
	// 返回正确值...
	ASSERT( m_lpLibMP4->IsVideoCreated() );
	return true;
}

bool CRecThread::WriteSample(bool bIsVideo, BYTE * lpFrame, int nSize, DWORD inTimeStamp, DWORD inRenderOffset, bool bIsKeyFrame)
{
	// 判断接口是否有效...
	if( m_lpLibMP4 == NULL )
		return false;
	ASSERT( m_lpLibMP4 != NULL );
	// 调用接口，直接写盘...
	if( !m_lpLibMP4->WriteSample(bIsVideo, lpFrame, nSize, inTimeStamp, inRenderOffset, bIsKeyFrame) )
		return false;
	// 累加存盘长度，记录已经写入的时间戳...
	m_dwWriteSize = m_lpLibMP4->GetWriteSize();
	m_dwWriteRecMS = m_lpLibMP4->GetWriteRecMS();
	// 打印调试信息...
	//TRACE("IsVideo = %d, IsKeyFrame = %d, TimeStamp = %lu \n", bIsVideo, bIsKeyFrame, inTimeStamp);
	// 不需要切片操作，直接返回...
	if( m_dwRecSliceMS <= 0 )
		return true;
	ASSERT( m_dwRecSliceMS > 0 );
	// 到达切片时间，通知切片，复位，阻止再次通知...
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
	// 停止线程...
	this->StopAndWaitForThread();

	// 释放MP4对象...
	if( m_lpLibMP4 != NULL ) {
		delete m_lpLibMP4;
		m_lpLibMP4 = NULL;
	}

	// 释放拉流对象...
	if( m_lpRtmp != NULL ) {
		delete m_lpRtmp;
		m_lpRtmp = NULL;
	}

	// 打印退出信息...
	TRACE("[~CRtmpRecThread Thread] - Exit\n");
}

bool CRtmpRecThread::InitThread(int nTaskID, LPCTSTR lpszURL, LPCTSTR lpszPath)
{
	if( lpszURL == NULL || lpszPath == NULL )
		return false;

	// 保存传递过来的参数...
	m_nTaskID = nTaskID;
	m_strURL = lpszURL;

	// MP4路径必须是UTF-8的格式...
	if( lpszPath != NULL && strlen(lpszPath) > 0 ) {
		m_strMP4Path = CUtilTool::ANSI_UTF8(lpszPath);
	}

	// 释放MP4对象...
	if( m_lpLibMP4 != NULL ) {
		delete m_lpLibMP4;
		m_lpLibMP4 = NULL;
	}

	// 创建 MP4 对象...
	m_lpLibMP4 = new LibMP4();
	ASSERT( m_lpLibMP4 != NULL );

	// 释放rtmp对象...
	if( m_lpRtmp != NULL ) {
		delete m_lpRtmp;
		m_lpRtmp = NULL;
	}

	// 创建rtmp对象...
	m_lpRtmp = new LibRtmp(false, false, NULL, this);
	ASSERT( m_lpRtmp != NULL );

	// 启动线程...
	this->Start();

	return true;
}

void CRtmpRecThread::Entry()
{
	// 判断rtmp对象的有效性...
	if( m_lpRtmp == NULL ) {
		this->doErrNotify();
		return;
	}

	// 链接rtmp服务器...
	ASSERT( m_lpRtmp != NULL );
	if( !m_lpRtmp->Open(m_strURL.c_str()) ) {
		delete m_lpRtmp; m_lpRtmp = NULL;
		this->doErrNotify();
		return;
	}

	// 循环读取数据并存盘...
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
// 这是有关 rtsp 直接录像的代码实现...
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
	// 设置rtsp循环退出标志...
	m_rtspEventLoopWatchVariable = 1;

	// 停止线程...
	this->StopAndWaitForThread();

	// 释放MP4对象...
	if( m_lpLibMP4 != NULL ) {
		delete m_lpLibMP4;
		m_lpLibMP4 = NULL;
	}

	// 通知IPC可以对录像进行改名操作...
	if( m_lpCamera != NULL ) {
		m_lpCamera->doRecEnd(m_dwWriteRecMS/1000);
	}

	// 打印退出信息...
	TRACE("[~CRtspRecThread Thread] - Exit\n");
}

bool CRtspRecThread::InitThread(int nTaskID, LPCTSTR lpszURL, LPCTSTR lpszPath)
{
	if( lpszURL == NULL || lpszPath == NULL )
		return false;

	// 保存传递过来的参数...
	m_nTaskID = nTaskID;
	m_strURL = lpszURL;

	// MP4路径必须是UTF-8的格式...
	if( lpszPath != NULL && strlen(lpszPath) > 0 ) {
		m_strMP4Path = CUtilTool::ANSI_UTF8(lpszPath);
	}

	// 释放MP4对象...
	if( m_lpLibMP4 != NULL ) {
		delete m_lpLibMP4;
		m_lpLibMP4 = NULL;
	}

	// 创建 MP4 对象...
	m_lpLibMP4 = new LibMP4();
	ASSERT( m_lpLibMP4 != NULL );

	// 创建rtsp链接环境...
	m_scheduler_ = BasicTaskScheduler::createNew();
	m_env_ = BasicUsageEnvironment::createNew(*m_scheduler_);
	m_rtspClient_ = ourRTSPClient::createNew(*m_env_, m_strURL.c_str(), 1, "rtspRecord", NULL, this);
	
	// 发起第一次rtsp握手...
	m_rtspClient_->sendDescribeCommand(continueAfterDESCRIBE); 

	// 启动线程...
	this->Start();

	return true;
}

void CRtspRecThread::Entry()
{
	// 进行任务循环检测，修改 g_rtspEventLoopWatchVariable 可以让任务退出...
	ASSERT( m_env_ != NULL && m_rtspClient_ != NULL );
	m_env_->taskScheduler().doEventLoop(&m_rtspEventLoopWatchVariable);

	// 通知主线程，需要退出...
	this->doErrNotify();

	// 任务退出之后，再释放rtsp相关资源...
	// 只能在这里调用 shutdownStream() 其它地方不要调用...
	if( m_rtspClient_ != NULL ) {
		m_rtspClient_->shutdownStream();
		m_rtspClient_ = NULL;
	}

	// 释放任务对象...
	if( m_scheduler_ != NULL ) {
		delete m_scheduler_;
		m_scheduler_ = NULL;
	}

	// 释放环境变量...
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
