
#include "stdafx.h"
#include "PushThread.h"

#include "AmfByteStream.h"
#include "BitWritter.h"
#include "LibRtmp.h"
#include "Camera.h"

CRtspThread::CRtspThread()
{
	m_bFinished = false;
	m_strRtspUrl.clear();
	m_strAVCHeader.clear();
	m_strAACHeader.clear();
	m_lpPushThread = NULL;
	
	m_strPPS.clear();
	m_strSPS.clear();

	m_audio_rate_index = 0;
	m_audio_channel_num = 0;

	m_env_ = NULL;
	m_scheduler_ = NULL;
	m_rtspClient_ = NULL;

	m_rtspEventLoopWatchVariable = 0;
}

CRtspThread::~CRtspThread()
{
	// 设置rtsp循环退出标志...
	m_rtspEventLoopWatchVariable = 1;

	// 停止线程...
	this->StopAndWaitForThread();

	TRACE("[~CRtspThread Thread] - Exit\n");
}

BOOL CRtspThread::InitRtsp(CPushThread * inPushThread, CString & strRtspUrl)
{
	// 保存传递的参数...
	m_lpPushThread = inPushThread;
	m_strRtspUrl = strRtspUrl.GetString();

	// 创建rtsp链接环境...
	m_scheduler_ = BasicTaskScheduler::createNew();
	m_env_ = BasicUsageEnvironment::createNew(*m_scheduler_);
	m_rtspClient_ = ourRTSPClient::createNew(*m_env_, m_strRtspUrl.c_str(), 1, "rtspTransfer", this, NULL);
	
	// 发起第一次rtsp握手...
	m_rtspClient_->sendDescribeCommand(continueAfterDESCRIBE); 

	//启动rtsp检测线程...
	this->Start();
	
	return true;
}

void CRtspThread::Entry()
{
	// 进行任务循环检测，修改 g_rtspEventLoopWatchVariable 可以让任务退出...
	ASSERT( m_env_ != NULL && m_rtspClient_ != NULL );
	m_env_->taskScheduler().doEventLoop(&m_rtspEventLoopWatchVariable);

	// 设置数据结束标志...
	m_bFinished = true;

	// 通知主线程，需要退出...
	if( m_lpPushThread != NULL ) {
		m_lpPushThread->doErrNotify();
	}
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

void CRtspThread::StartPushThread()
{
	if( m_lpPushThread == NULL )
		return;
	ASSERT( m_lpPushThread != NULL );
	// 至少有一个音频或视频已经准备好了...
	ASSERT( m_strAACHeader.size() > 0 || m_strAVCHeader.size() > 0 );
	// 直接启动rtmp推送线程，开始启动rtmp推送过程...
	m_lpPushThread->Start();
}

void CRtspThread::PushFrame(FMS_FRAME & inFrame)
{
	// 将数据帧加入待发送队列当中...
	ASSERT( m_lpPushThread != NULL );
	m_lpPushThread->PushFrame(inFrame);
}

void CRtspThread::WriteAACSequenceHeader(int inAudioRate, int inAudioChannel)
{
	// 首先解析并存储传递过来的参数...
	if (inAudioRate == 48000)
		m_audio_rate_index = 0x03;
	else if (inAudioRate == 44100)
		m_audio_rate_index = 0x04;
	else if (inAudioRate == 32000)
		m_audio_rate_index = 0x05;
	else if (inAudioRate == 24000)
		m_audio_rate_index = 0x06;
	else if (inAudioRate == 22050)
		m_audio_rate_index = 0x07;
	else if (inAudioRate == 16000)
		m_audio_rate_index = 0x08;
	else if (inAudioRate == 12000)
		m_audio_rate_index = 0x09;
	else if (inAudioRate == 11025)
		m_audio_rate_index = 0x0a;
	else if (inAudioRate == 8000)
		m_audio_rate_index = 0x0b;

	m_audio_channel_num = inAudioChannel;

	char   aac_seq_buf[4096] = {0};
    char * pbuf = aac_seq_buf;

    unsigned char flag = 0;
    flag = (10 << 4) |  // soundformat "10 == AAC"
        (3 << 2) |      // soundrate   "3  == 44-kHZ"
        (1 << 1) |      // soundsize   "1  == 16bit"
        1;              // soundtype   "1  == Stereo"

    pbuf = UI08ToBytes(pbuf, flag);
    pbuf = UI08ToBytes(pbuf, 0);    // aac packet type (0, header)

    // AudioSpecificConfig

	PutBitContext pb;
	init_put_bits(&pb, pbuf, 1024);
	put_bits(&pb, 5, 2);					//object type - AAC-LC
	put_bits(&pb, 4, m_audio_rate_index);	// sample rate index
	put_bits(&pb, 4, m_audio_channel_num);  // channel configuration

	//GASpecificConfig
	put_bits(&pb, 1, 0);    // frame length - 1024 samples
	put_bits(&pb, 1, 0);    // does not depend on core coder
	put_bits(&pb, 1, 0);    // is not extension

	flush_put_bits(&pb);
	pbuf += 2;

	// 保存AAC数据头信息...
	int aac_len = (int)(pbuf - aac_seq_buf);
	m_strAACHeader.assign(aac_seq_buf, aac_len);
}

void CRtspThread::WriteAVCSequenceHeader(string & inSPS, string & inPPS)
{
	// 先保存 SPS 和 PPS 格式头信息..
	ASSERT( inSPS.size() > 0 && inPPS.size() > 0 );
	m_strSPS = inSPS, m_strPPS = inPPS;

	// Write AVC Sequence Header use SPS and PPS...
	char * sps_ = (char*)m_strSPS.c_str();
	char * pps_ = (char*)m_strPPS.c_str();
	int    sps_size_ = m_strSPS.size();
	int	   pps_size_ = m_strPPS.size();

    char   avc_seq_buf[4096] = {0};
    char * pbuf = avc_seq_buf;

    unsigned char flag = 0;
    flag = (1 << 4) |				// frametype "1  == keyframe"
				 7;					// codecid   "7  == AVC"
    pbuf = UI08ToBytes(pbuf, flag);	// flag...
    pbuf = UI08ToBytes(pbuf, 0);    // avc packet type (0, header)
    pbuf = UI24ToBytes(pbuf, 0);    // composition time

    // AVC Decoder Configuration Record...

    pbuf = UI08ToBytes(pbuf, 1);            // configurationVersion
    pbuf = UI08ToBytes(pbuf, sps_[1]);      // AVCProfileIndication
    pbuf = UI08ToBytes(pbuf, sps_[2]);      // profile_compatibility
    pbuf = UI08ToBytes(pbuf, sps_[3]);      // AVCLevelIndication
    pbuf = UI08ToBytes(pbuf, 0xff);         // 6 bits reserved (111111) + 2 bits nal size length - 1
    pbuf = UI08ToBytes(pbuf, 0xe1);         // 3 bits reserved (111) + 5 bits number of sps (00001)
    pbuf = UI16ToBytes(pbuf, sps_size_);    // sps
    memcpy(pbuf, sps_, sps_size_);
    pbuf += sps_size_;
	
    pbuf = UI08ToBytes(pbuf, 1);            // number of pps
    pbuf = UI16ToBytes(pbuf, pps_size_);    // pps
    memcpy(pbuf, pps_, pps_size_);
    pbuf += pps_size_;

	// 保存AVC数据头信息...
	int avc_len = (int)(pbuf - avc_seq_buf);
	m_strAVCHeader.assign(avc_seq_buf, avc_len);
}

CRtmpThread::CRtmpThread()
{
	m_bFinished = false;
	m_strRtmpUrl.clear();
	m_strAVCHeader.clear();
	m_strAACHeader.clear();
	m_lpPushThread = NULL;
	m_lpRtmp = NULL;
	
	m_strPPS.clear();
	m_strSPS.clear();

	m_audio_rate_index = 0;
	m_audio_channel_num = 0;
}

CRtmpThread::~CRtmpThread()
{
	// 停止线程...
	this->StopAndWaitForThread();

	// 删除rtmp对象...
	if( m_lpRtmp != NULL ) {
		delete m_lpRtmp;
		m_lpRtmp = NULL;
	}

	TRACE("[~CRtmpThread Thread] - Exit\n");
}

BOOL CRtmpThread::InitRtmp(CPushThread * inPushThread)
{
	// 保存传递的参数...
	m_lpPushThread = inPushThread;
	//m_nPushID = atoi(dbRec[CPushSession::GetItemName(kPushIDItem)].c_str());;
	//m_strRtmpUrl = dbRec[CPushSession::GetItemName(kPushRtspItem)];

	// 创建rtmp对象，注意是拉数据...
	m_lpRtmp = new LibRtmp(false, false, this, NULL);
	ASSERT( m_lpRtmp != NULL );

	//启动rtmp检测线程...
	this->Start();
	
	return true;
}

void CRtmpThread::Entry()
{
	ASSERT( m_lpRtmp != NULL );
	ASSERT( m_lpPushThread != NULL );
	ASSERT( m_strRtmpUrl.size() > 0 );

	// 链接rtmp服务器...
	if( !m_lpRtmp->Open(m_strRtmpUrl.c_str()) ) {
		delete m_lpRtmp; m_lpRtmp = NULL;
		m_lpPushThread->doErrNotify();
		m_bFinished = true;
		return;
	}
	// 循环读取数据并转发出去...
	while( !this->IsStopRequested() ) {
		if( m_lpRtmp->IsClosed() || !m_lpRtmp->Read() ) {
			m_lpPushThread->doErrNotify();
			m_bFinished = true;
			return;
		}
	}
	/*// 设置数据结束标志...
	m_bFinished = true;
	// 通知主线程，需要退出...
	if( m_lpPushThread != NULL ) {
		m_lpPushThread->doErrNotify();
	}*/
}

void CRtmpThread::StartPushThread()
{
	if( m_lpPushThread == NULL )
		return;
	ASSERT( m_lpPushThread != NULL );
	// 至少有一个音频或视频已经准备好了...
	ASSERT( m_strAACHeader.size() > 0 || m_strAVCHeader.size() > 0 );
	// 直接启动rtmp推送线程，开始启动rtmp推送过程...
	m_lpPushThread->Start();
}

int CRtmpThread::PushFrame(FMS_FRAME & inFrame)
{
	// 将数据帧加入待发送队列当中...
	ASSERT( m_lpPushThread != NULL );
	return m_lpPushThread->PushFrame(inFrame);
}

void CRtmpThread::WriteAACSequenceHeader(int inAudioRate, int inAudioChannel)
{
	// 首先解析并存储传递过来的参数...
	if (inAudioRate == 48000)
		m_audio_rate_index = 0x03;
	else if (inAudioRate == 44100)
		m_audio_rate_index = 0x04;
	else if (inAudioRate == 32000)
		m_audio_rate_index = 0x05;
	else if (inAudioRate == 24000)
		m_audio_rate_index = 0x06;
	else if (inAudioRate == 22050)
		m_audio_rate_index = 0x07;
	else if (inAudioRate == 16000)
		m_audio_rate_index = 0x08;
	else if (inAudioRate == 12000)
		m_audio_rate_index = 0x09;
	else if (inAudioRate == 11025)
		m_audio_rate_index = 0x0a;
	else if (inAudioRate == 8000)
		m_audio_rate_index = 0x0b;

	m_audio_channel_num = inAudioChannel;

	char   aac_seq_buf[4096] = {0};
    char * pbuf = aac_seq_buf;

    unsigned char flag = 0;
    flag = (10 << 4) |  // soundformat "10 == AAC"
        (3 << 2) |      // soundrate   "3  == 44-kHZ"
        (1 << 1) |      // soundsize   "1  == 16bit"
        1;              // soundtype   "1  == Stereo"

    pbuf = UI08ToBytes(pbuf, flag);
    pbuf = UI08ToBytes(pbuf, 0);    // aac packet type (0, header)

    // AudioSpecificConfig

	PutBitContext pb;
	init_put_bits(&pb, pbuf, 1024);
	put_bits(&pb, 5, 2);					//object type - AAC-LC
	put_bits(&pb, 4, m_audio_rate_index);	// sample rate index
	put_bits(&pb, 4, m_audio_channel_num);  // channel configuration

	//GASpecificConfig
	put_bits(&pb, 1, 0);    // frame length - 1024 samples
	put_bits(&pb, 1, 0);    // does not depend on core coder
	put_bits(&pb, 1, 0);    // is not extension

	flush_put_bits(&pb);
	pbuf += 2;

	// 保存AAC数据头信息...
	int aac_len = (int)(pbuf - aac_seq_buf);
	m_strAACHeader.assign(aac_seq_buf, aac_len);
}

void CRtmpThread::WriteAVCSequenceHeader(string & inSPS, string & inPPS)
{
	// 先保存 SPS 和 PPS 格式头信息..
	ASSERT( inSPS.size() > 0 && inPPS.size() > 0 );
	m_strSPS = inSPS, m_strPPS = inPPS;

	// Write AVC Sequence Header use SPS and PPS...
	char * sps_ = (char*)m_strSPS.c_str();
	char * pps_ = (char*)m_strPPS.c_str();
	int    sps_size_ = m_strSPS.size();
	int	   pps_size_ = m_strPPS.size();

    char   avc_seq_buf[4096] = {0};
    char * pbuf = avc_seq_buf;

    unsigned char flag = 0;
    flag = (1 << 4) |				// frametype "1  == keyframe"
				 7;					// codecid   "7  == AVC"
    pbuf = UI08ToBytes(pbuf, flag);	// flag...
    pbuf = UI08ToBytes(pbuf, 0);    // avc packet type (0, header)
    pbuf = UI24ToBytes(pbuf, 0);    // composition time

    // AVC Decoder Configuration Record...

    pbuf = UI08ToBytes(pbuf, 1);            // configurationVersion
    pbuf = UI08ToBytes(pbuf, sps_[1]);      // AVCProfileIndication
    pbuf = UI08ToBytes(pbuf, sps_[2]);      // profile_compatibility
    pbuf = UI08ToBytes(pbuf, sps_[3]);      // AVCLevelIndication
    pbuf = UI08ToBytes(pbuf, 0xff);         // 6 bits reserved (111111) + 2 bits nal size length - 1
    pbuf = UI08ToBytes(pbuf, 0xe1);         // 3 bits reserved (111) + 5 bits number of sps (00001)
    pbuf = UI16ToBytes(pbuf, sps_size_);    // sps
    memcpy(pbuf, sps_, sps_size_);
    pbuf += sps_size_;
	
    pbuf = UI08ToBytes(pbuf, 1);            // number of pps
    pbuf = UI16ToBytes(pbuf, pps_size_);    // pps
    memcpy(pbuf, pps_, pps_size_);
    pbuf += pps_size_;

	// 保存AVC数据头信息...
	int avc_len = (int)(pbuf - avc_seq_buf);
	m_strAVCHeader.assign(avc_seq_buf, avc_len);
}

CPushThread::CPushThread(HWND hWndParent)
  : m_hWndParent(hWndParent)
{
	ASSERT( m_hWndParent != NULL );

	m_lpRtmp = NULL;
	m_lpRtspThread = NULL;
	m_lpRtmpThread = NULL;
	m_dwTimeOutMS = 0;
	m_bDeleteFlag = false;

	m_bFileMode = false;
	m_strRtmpUrl.clear();
	m_nCurSendByte = 0;
	m_nSendKbps = 0;
}

CPushThread::~CPushThread()
{
	// 停止线程...
	this->StopAndWaitForThread();

	// 删除rtmp对象，这里必须加互斥，避免Connect返回时rtmp已经被删除，造成内存错误...
	if( m_lpRtmp != NULL ) {
		OSMutexLocker theLock(&m_Mutex);
		delete m_lpRtmp; m_lpRtmp = NULL;
	}

	// 删除rtmp线程...
	if( m_lpRtmpThread != NULL ) {
		delete m_lpRtmpThread;
		m_lpRtmpThread = NULL;
	}

	// 删除rtsp线程...
	if( m_lpRtspThread != NULL ) {
		delete m_lpRtspThread;
		m_lpRtspThread = NULL;
	}
	
	TRACE("[~CPushThread Thread] - Exit\n");
}

BOOL CPushThread::InitThread(CCamera * lpCamera, CString & strRtspUrl, string & strRtmpUrl)
{
	// 保存传递过来的参数...
	m_lpCamera = lpCamera;
	m_strRtmpUrl = strRtmpUrl;
	// 创建rtsp拉流数据线程...
	m_lpRtspThread = new CRtspThread();
	m_lpRtspThread->InitRtsp(this, strRtspUrl);

	// 创建rtsp或rtmp线程...
	/*if( strnicmp("rtsp://", strRtspUrl.c_str(), strlen("rtsp://")) == 0 ) {
		m_lpRtspThread = new CRtspThread();
		m_lpRtspThread->InitRtsp(this);
	} else {
		m_lpRtmpThread = new CRtmpThread();
		m_lpRtmpThread->InitRtmp(this);
	}*/
	
	// 创建rtmp上传对象...
	m_lpRtmp = new LibRtmp(false, true, NULL, NULL);
	ASSERT( m_lpRtmp != NULL );

	// 这里不能启动线程，等待环境准备妥当之后才能启动...

	return true;
}

bool CPushThread::IsDataFinished()
{
	if( m_lpRtspThread != NULL ) {
		return m_lpRtspThread->IsFinished();
	} else if( m_lpRtmpThread != NULL ) {
		return m_lpRtmpThread->IsFinished();
	}
	return true;
}

void CPushThread::doErrNotify()
{
	// 这里需要通知正在等待的播放器退出...
	if( m_lpCamera != NULL ) {
		m_lpCamera->doDelayTransmit(GM_Push_Fail);
	}
	// 如果线程已经被主动删除了，直接退出...
	if( m_bDeleteFlag )
		return;
	ASSERT( !m_bDeleteFlag );
	ASSERT( m_hWndParent != NULL );
	::PostMessage(m_hWndParent, WM_ERR_PUSH_MSG, NULL, NULL);
}

void CPushThread::Entry()
{
	// 连接rtmp server，失败，通知上层删除之...
	if( !this->OpenRtmpUrl() ) {
		TRACE("[CPushThread::OpenRtmpUrl] - Error\n");
		this->doErrNotify();
		return;
	}
	// 握手成功，发送metadata数据包，失败，通知上层删除之...
	if( !this->SendMetadataPacket() ) {
		TRACE("[CPushThread::SendMetadataPacket] - Error\n");
		this->doErrNotify();
		return;
	}
	// 发送视频序列头数据包，失败，通知上层删除之...
	if( !this->SendAVCSequenceHeaderPacket() ) {
		TRACE("[CPushThread::SendAVCSequenceHeaderPacket] - Error\n");
		this->doErrNotify();
		return;
	}
	// 发送音频序列头数据包，失败，通知上层删除之...
	if( !this->SendAACSequenceHeaderPacket() ) {
		TRACE("[CPushThread::SendAACSequenceHeaderPacket] - Error\n");
		this->doErrNotify();
		return;
	}
	// 上传准备完毕，通知 RemoteSession 可以反馈消息给直播播放器了...
	if( m_lpCamera != NULL ) {
		m_lpCamera->doDelayTransmit(GM_NoErr);
	}
	// 超时时间记录起点...
	m_dwTimeOutMS = ::GetTickCount();
	// 开始线程循环...
	int nRetValue = 0;
	uint32_t dwStartTimeMS = ::GetTickCount();
	while( !this->IsStopRequested() ) {
		// 如果时间间隔大于了1000毫秒，则计算一次发送码流...
		if( (::GetTickCount() - dwStartTimeMS) >= 1000 ) {
			dwStartTimeMS = ::GetTickCount();
			m_nSendKbps = m_nCurSendByte * 8 / 1024;
			m_nCurSendByte = 0;
		}
		nRetValue = this->SendOneDataPacket();
		// < 0 直接向上层反馈删除之...
		if( nRetValue < 0 ) {
			TRACE("[CPushThread::SendOneDataPacket] - Error\n");
			this->doErrNotify();
			return;
		}
		// == 0 马上继续...
		if( nRetValue == 0 ) {
			Sleep(5);
			continue;
		}
		// > 0 说明还有,马上发送...
		ASSERT( nRetValue > 0 );
	}
}

BOOL CPushThread::OpenRtmpUrl()
{
	OSMutexLocker theLock(&m_Mutex);
	if( m_lpRtmp == NULL || m_strRtmpUrl.size() <= 0  )
		return false;
	ASSERT( m_lpRtmp != NULL && m_strRtmpUrl.size() > 0);
	
	// 连接rtmp server，完成握手等协议
	return m_lpRtmp->Open(m_strRtmpUrl.c_str());
}
//
// 通过其它渠道输送数据帧...
int CPushThread::PushFrame(FMS_FRAME & inFrame)
{
	OSMutexLocker theLock(&m_Mutex);

	m_MapFrame.insert(pair<uint32_t, FMS_FRAME>(inFrame.dwSendTime, inFrame));

	return m_MapFrame.size();
}
//
// 检查数据包是否已经超时...
BOOL CPushThread::IsFrameTimeout()
{
	// 首先判断数据线程是否已经结束了...
	if( this->IsDataFinished() )
		return true;
	// 如果缓存不低于100个数据包，则超时失败，重置超时计时器...
	if( m_MapFrame.size() >= 100 ) {
		m_dwTimeOutMS = ::GetTickCount();
		return false;
	}
	ASSERT( m_MapFrame.size() < 100 );
	// 数据包小于100个，继续计时，直到超过预设时间(3分钟)...
	int   nWaitMinute = 3;
	DWORD dwElapseSec = (::GetTickCount() - m_dwTimeOutMS) / 1000;
	return ((dwElapseSec >= nWaitMinute * 60) ? true : false);
}
//
// 发送一个帧数据包...
uint32_t g_LastATime = 0;
int CPushThread::SendOneDataPacket()
{
	OSMutexLocker theLock(&m_Mutex);

	// 如果长时间低于一定量的缓冲，则认为需要断开链接了...
	if( this->IsFrameTimeout() ) {
		TRACE("[CPushThread::FrameTimeout] - Frame(%d)\n", m_MapFrame.size());
		return -1;
	}
	// 如果数据还没有结束，则需要有一定缓存，以便音视频能够自动排序，然后再发送数据包...
	if( !this->IsDataFinished() && m_MapFrame.size() < 100 ) {
		return 0;
	}
	ASSERT( !this->IsDataFinished() || m_MapFrame.size() >= 100 );

	bool is_ok = false;
	ASSERT( m_MapFrame.size() > 0 );
	
	KH_MapFrame::iterator itorFrame;
	KH_MapFrame::iterator itorPair;
	pair<KH_MapFrame::iterator, KH_MapFrame::iterator> myPair;

	// 获取队列头，并找到重复的列表...
	itorFrame = m_MapFrame.begin();
	myPair = m_MapFrame.equal_range(itorFrame->first);

	for(itorPair = myPair.first; itorPair != myPair.second; ++itorPair) {
		// 遍历这个相同时间的列表...
		ASSERT( itorPair->first == itorFrame->first );
		FMS_FRAME & theFrame = itorPair->second;
		// 将获取到的数据帧发送走...
		switch( theFrame.typeFlvTag )
		{
		case FLV_TAG_TYPE_AUDIO: is_ok = this->SendAudioDataPacket(theFrame); break;
		case FLV_TAG_TYPE_VIDEO: is_ok = this->SendVideoDataPacket(theFrame); break;
		}
		// 累加发送字节数...
		m_nCurSendByte += theFrame.strData.size();
		// 打印发送结果...
		if( theFrame.typeFlvTag == FLV_TAG_TYPE_AUDIO ) {
			g_LastATime = ((g_LastATime <= 0 ) ? theFrame.dwSendTime: g_LastATime);
			//TRACE("[%s] SendTime = %lu, Duration = %lu, Size = %d, MapSize = %d\n", ((theFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO) ? "Video" : "Audio"), theFrame.dwSendTime, theFrame.dwSendTime - g_LastATime, theFrame.strData.size(), m_MapFrame.size());
			g_LastATime = theFrame.dwSendTime;
		}
	}

	// 从队列中移除一个相同时间的数据包 => 一定要用关键字删除，否则，删不干净...
	m_MapFrame.erase(itorFrame->first);

	// 发送失败，返回错误...
	return (is_ok ? 1 : -1);
}
//
// 发送视频数据包...
BOOL CPushThread::SendVideoDataPacket(FMS_FRAME & inFrame)
{
    int need_buf_size = inFrame.strData.size() + 5;
	char * video_mem_buf_ = new char[need_buf_size];
    char * pbuf  = video_mem_buf_;

	bool   is_ok = false;
    unsigned char flag = 0;
    flag = (inFrame.is_keyframe ? 0x17 : 0x27);

    pbuf = UI08ToBytes(pbuf, flag);
    pbuf = UI08ToBytes(pbuf, 1);    // avc packet type (0, nalu)
    pbuf = UI24ToBytes(pbuf, 0);    // composition time

    memcpy(pbuf, inFrame.strData.c_str(), inFrame.strData.size());
    pbuf += inFrame.strData.size();

    is_ok = m_lpRtmp->Send(video_mem_buf_, (int)(pbuf - video_mem_buf_), FLV_TAG_TYPE_VIDEO, inFrame.dwSendTime);

	delete [] video_mem_buf_;

	//TRACE("[Video] SendTime = %lu, StartTime = %lu\n", inFrame.dwSendTime, inFrame.dwStartTime);

	return is_ok;
}
//
// 发送音频数据包...
BOOL CPushThread::SendAudioDataPacket(FMS_FRAME & inFrame)
{
    int need_buf_size = inFrame.strData.size() + 5;
	char * audio_mem_buf_ = new char[need_buf_size];
    char * pbuf  = audio_mem_buf_;

	bool   is_ok = false;
    unsigned char flag = 0;
    flag = (10 << 4) |  // soundformat "10 == AAC"
        (3 << 2) |      // soundrate   "3  == 44-kHZ"
        (1 << 1) |      // soundsize   "1  == 16bit"
        1;              // soundtype   "1  == Stereo"

    pbuf = UI08ToBytes(pbuf, flag);
    pbuf = UI08ToBytes(pbuf, 1);    // aac packet type (1, raw)

    memcpy(pbuf, inFrame.strData.c_str(), inFrame.strData.size());
    pbuf += inFrame.strData.size();

    is_ok = m_lpRtmp->Send(audio_mem_buf_, (int)(pbuf - audio_mem_buf_), FLV_TAG_TYPE_AUDIO, inFrame.dwSendTime);

	delete [] audio_mem_buf_;
	
	//TRACE("[Audio] SendTime = %lu, StartTime = %lu\n", inFrame.dwSendTime, inFrame.dwStartTime);

	return is_ok;
}
//
// 发送视频序列头...
BOOL CPushThread::SendAVCSequenceHeaderPacket()
{
	OSMutexLocker theLock(&m_Mutex);

	string strAVC;

	if( m_lpRtspThread != NULL ) {
		strAVC = m_lpRtspThread->GetAVCHeader();
	} else if( m_lpRtmpThread != NULL ) {
		strAVC = m_lpRtmpThread->GetAVCHeader();
	}

	// 没有视频数据...
	if( strAVC.size() <= 0 )
		return true;
	
	ASSERT( strAVC.size() > 0 );
	return m_lpRtmp->Send(strAVC.c_str(), strAVC.size(), FLV_TAG_TYPE_VIDEO, 0);
}

//
// 发送音频序列头...
BOOL CPushThread::SendAACSequenceHeaderPacket()
{
	OSMutexLocker theLock(&m_Mutex);

	string strAAC;

	if( m_lpRtspThread != NULL ) {
		strAAC = m_lpRtspThread->GetAACHeader();
	} else if( m_lpRtmpThread != NULL ) {
		strAAC = m_lpRtmpThread->GetAACHeader();
	}

	// 没有音频数据...
	if( strAAC.size() <= 0 )
		return true;

	ASSERT( strAAC.size() > 0 );
    return m_lpRtmp->Send(strAAC.c_str(), strAAC.size(), FLV_TAG_TYPE_AUDIO, 0);
}

BOOL CPushThread::SendMetadataPacket()
{
	OSMutexLocker theLock(&m_Mutex);

	char   metadata_buf[4096];
    char * pbuf = this->WriteMetadata(metadata_buf);

    return m_lpRtmp->Send(metadata_buf, (int)(pbuf - metadata_buf), FLV_TAG_TYPE_META, 0);
}

char * CPushThread::WriteMetadata(char * buf)
{
    char* pbuf = buf;

    pbuf = UI08ToBytes(pbuf, AMF_DATA_TYPE_STRING);
    pbuf = AmfStringToBytes(pbuf, "@setDataFrame");

    pbuf = UI08ToBytes(pbuf, AMF_DATA_TYPE_STRING);
    pbuf = AmfStringToBytes(pbuf, "onMetaData");

	//pbuf = UI08ToBytes(pbuf, AMF_DATA_TYPE_MIXEDARRAY);
	//pbuf = UI32ToBytes(pbuf, 2);
    pbuf = UI08ToBytes(pbuf, AMF_DATA_TYPE_OBJECT);

    /*pbuf = AmfStringToBytes(pbuf, "width");
    pbuf = AmfDoubleToBytes(pbuf, 704); //width_);

    pbuf = AmfStringToBytes(pbuf, "height");
    pbuf = AmfDoubleToBytes(pbuf, 576); //height_);*/

    pbuf = AmfStringToBytes(pbuf, "framerate");
    pbuf = AmfDoubleToBytes(pbuf, 25);

    pbuf = AmfStringToBytes(pbuf, "videocodecid");
    pbuf = UI08ToBytes(pbuf, AMF_DATA_TYPE_STRING);
    pbuf = AmfStringToBytes(pbuf, "avc1");

    // 0x00 0x00 0x09
    pbuf = AmfStringToBytes(pbuf, "");
    pbuf = UI08ToBytes(pbuf, AMF_DATA_TYPE_OBJECT_END);
    
    return pbuf;
}
