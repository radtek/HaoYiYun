
#include "stdafx.h"
#include "PushThread.h"

#include "srs_librtmp.h"
#include "BitWritter.h"
#include "UtilTool.h"
#include "LibRtmp.h"
#include "Camera.h"
#include "LibMP4.h"
#include "ReadSPS.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CMP4Thread::CMP4Thread()
{
	m_nVideoHeight = 0;
	m_nVideoWidth = 0;

	m_bFileLoop = false;
	m_strMP4File.clear();
	m_lpPushThread = NULL;
	m_dwMP4Duration = 0;
	m_nLoopCount = 0;

	m_hMP4Handle = MP4_INVALID_FILE_HANDLE;
	m_strAACHeader.clear();
	m_strAVCHeader.clear();
	m_strPPS.clear();
	m_strSPS.clear();
	m_strAES.clear();

	m_audio_type = 0;
	m_audio_rate_index = 0;
	m_audio_channel_num = 0;

	m_tidAudio = 0;
	m_tidVideo = 0;
	m_iVSampleInx = 1;
	m_iASampleInx = 1;
	m_bVideoComplete = true;
	m_bAudioComplete = true;
	m_bFinished = false;

	// 记录开始时间点...
	m_dwStartMS = ::GetTickCount();
}

CMP4Thread::~CMP4Thread()
{
	// 停止线程...
	this->StopAndWaitForThread();

	// 释放MP4句柄...
	if( m_hMP4Handle != MP4_INVALID_FILE_HANDLE ) {
		MP4Close(m_hMP4Handle);
		m_hMP4Handle = MP4_INVALID_FILE_HANDLE;
	}

	TRACE("[~CMP4Thread Thread] - Exit\n");
}

void CMP4Thread::Entry()
{
	// 解析并准备MP4文件...
	if( !this->doPrepareMP4() ) {
		// 解析失败 => 这里只需要设置结束标志，超时机制会自动删除 => 这里是拉流模式，不能调用 doErrNotify()...
		// 在超时机制中会首先检测m_bFinished标志，速度快...
		m_bFinished = true;
		return;
	}
	// 设置流转发播放状态标志...
	m_lpPushThread->SetStreamPlaying(true);

	// 在发起直播时才需要启动上传线程，准备完毕，启动主线程...
	//m_lpPushThread->Start();

	// 循环抽取数据帧...
	int nAccTime = 1000;
	uint32_t dwCompTime = 0;
	uint32_t dwElapsTime = 0;
	uint32_t dwOutVSendTime = 0;
	uint32_t dwOutASendTime = 0;
	while( !this->IsStopRequested() ) {

		// 这里需要多发送几秒钟的数据，避免出现缓冲(只需要加速一次)...
		dwElapsTime = ::GetTickCount() - m_dwStartMS + nAccTime;

		// 抽取一帧视频...
		if( (dwElapsTime >= dwOutVSendTime) && (!m_bVideoComplete) && (m_tidVideo != MP4_INVALID_TRACK_ID) ) {
			if( !ReadOneFrameFromMP4(m_tidVideo, m_iVSampleInx++, true, dwOutVSendTime) ) {
				m_bVideoComplete = true;
				continue;
			}
			//TRACE("[Video] ElapsTime = %lu, OutSendTime = %lu\n", dwElapsTime, dwOutVSendTime);
		}

		// 抽取一帧音频...
		if( (dwElapsTime >= dwOutASendTime) && (!m_bAudioComplete) && (m_tidAudio != MP4_INVALID_TRACK_ID) ) {
			if( !ReadOneFrameFromMP4(m_tidAudio, m_iASampleInx++, false, dwOutASendTime) ) {
				m_bAudioComplete = true;
				continue;
			}
			//TRACE("[Audio] ElapsTime = %lu, OutSendTime = %lu\n", dwElapsTime, dwOutASendTime);
		}

		// 如果音频和视频全部结束，则重新计数...
		if( m_bVideoComplete && m_bAudioComplete ) {
			// 如果不循环，直接退出线程...
			if( !m_bFileLoop ) {
				TRACE("[File-Complete]\n");
				m_bFinished = true;
				return;
			}
			// 打印循环次数...
			ASSERT( m_bFileLoop );
			TRACE("[File-Loop] Count = %d\n", m_nLoopCount);
			// 如果要循环，重新计数，复位大量的执行参数...
			m_bVideoComplete = ((m_tidVideo != MP4_INVALID_TRACK_ID) ? false : true);	// 有视频才复位标志
			m_bAudioComplete = ((m_tidAudio != MP4_INVALID_TRACK_ID) ? false : true);	// 有音频才复位标志
			dwOutVSendTime = dwOutASendTime = 0;	// 帧计时器复位到0点
			m_iASampleInx = m_iVSampleInx = 1;		// 音视频的索引复位
			m_dwStartMS = ::GetTickCount();			// 时间起点复位
			m_nLoopCount += 1;						// 循环次数累加
			nAccTime = 0;							// 将加速器复位
			continue;
		}
		// 如果视频有效，使用获取的视频时间戳比较...
		if( m_tidVideo != MP4_INVALID_TRACK_ID ) {
			dwCompTime = dwOutVSendTime;
		}
		// 如果音频有效，使用获取的音频时间戳比较...
		if( m_tidAudio != MP4_INVALID_TRACK_ID ) {
			dwCompTime = dwOutASendTime;
		}
		// 如果音视频都有，则取比较小的值比较...
		if((m_tidVideo != MP4_INVALID_TRACK_ID) && (m_tidAudio != MP4_INVALID_TRACK_ID)) {
			dwCompTime = min(dwOutASendTime, dwOutVSendTime);
		}
		// 如果当前时间比较小，等待一下再读取...
		if( dwElapsTime < dwCompTime ) {
			//TRACE("ElapsTime = %lu, AudioSendTime = %lu, VideoSendTime = %lu\n", dwElapsTime, dwOutASendTime, dwOutVSendTime);
			::Sleep(5);
		}
	}
}

bool CMP4Thread::ReadOneFrameFromMP4(MP4TrackId tid, uint32_t sid, bool bIsVideo, uint32_t & outSendTime)
{
	if( m_hMP4Handle == MP4_INVALID_FILE_HANDLE )
		return false;
	ASSERT( m_hMP4Handle != MP4_INVALID_FILE_HANDLE );

	uint8_t		*	pSampleData = NULL;		// 帧数据指针
	uint32_t		nSampleSize = 0;		// 帧数据长度
	MP4Timestamp	nStartTime = 0;			// 开始时间
	MP4Duration		nDuration = 0;			// 持续时间
	MP4Duration     nOffset = 0;			// 偏移时间
	bool			bIsKeyFrame = false;	// 是否关键帧
	uint32_t		timescale = 0;
	uint64_t		msectime = 0;

	timescale = MP4GetTrackTimeScale( m_hMP4Handle, tid );
	msectime = MP4GetSampleTime( m_hMP4Handle, tid, sid );

	if( false == MP4ReadSample(m_hMP4Handle, tid, sid, &pSampleData, &nSampleSize, &nStartTime, &nDuration, &nOffset, &bIsKeyFrame) )
		return false;

	// 计算发送时间 => PTS
	msectime *= UINT64_C( 1000 );
	msectime /= timescale;
	// 计算开始时间 => DTS
	nStartTime *= UINT64_C( 1000 );
	nStartTime /= timescale;
	// 计算偏差时间 => CTTS
	nOffset *= UINT64_C( 1000 );
	nOffset /= timescale;

	FMS_FRAME	theFrame;
	theFrame.typeFlvTag = (bIsVideo ? FLV_TAG_TYPE_VIDEO : FLV_TAG_TYPE_AUDIO);	// 设置音视频标志
	theFrame.dwSendTime = (uint32_t)msectime + m_dwMP4Duration * m_nLoopCount;	// 这里非常重要，牵涉到循环播放
	theFrame.dwRenderOffset = nOffset;  // 2017.07.06 - by jackey => 保存时间偏移值...
	theFrame.is_keyframe = bIsKeyFrame;
	theFrame.strData.assign((char*)pSampleData, nSampleSize);

	// 这里需要释放读取的缓冲区...
	MP4Free(pSampleData);
	pSampleData = NULL;

	// 将数据帧加入待发送队列当中...
	ASSERT( m_lpPushThread != NULL );
	m_lpPushThread->PushFrame(theFrame);

	// 返回发送时间(毫秒)...
	outSendTime = (uint32_t)msectime;
	
	//TRACE("[%s] duration = %I64d, offset = %I64d, KeyFrame = %d, SendTime = %lu, StartTime = %I64d, Size = %lu\n", bIsVideo ? "Video" : "Audio", nDuration, nOffset, bIsKeyFrame, outSendTime, nStartTime, nSampleSize);

	return true;
}

BOOL CMP4Thread::InitMP4(CPushThread * inPushThread, string & strMP4File)
{
	// 目前的模式，文件一定是循环模式...
	m_lpPushThread = inPushThread;
	m_strMP4File = strMP4File;
	m_bFileLoop = true;

	this->Start();

	return true;
}

void myMP4LogCallback( MP4LogLevel loglevel, const char* fmt, va_list ap)
{
	// 组合传递过来的格式...
	CString	strDebug;
	strDebug.FormatV(fmt, ap);
	if( (strDebug.ReverseFind('\r') < 0) && (strDebug.ReverseFind('\n') < 0) ) {
		strDebug.Append("\r\n");
	}
	// 进行格式转换，并打印出来...
	string strANSI = CUtilTool::UTF8_ANSI(strDebug);
	TRACE(strANSI.c_str());
}

bool CMP4Thread::doPrepareMP4()
{
	// 设置MP4调试级别 => 最高最详细级别...
	//MP4LogLevel theLevel = MP4LogGetLevel();
	//MP4LogSetLevel(MP4_LOG_VERBOSE4);
	//MP4SetLogCallback(myMP4LogCallback);
	// 文件打开失败，直接返回...
	string strUTF8 = CUtilTool::ANSI_UTF8(m_strMP4File.c_str());
	m_hMP4Handle = MP4Read( strUTF8.c_str() );
	if( m_hMP4Handle == MP4_INVALID_FILE_HANDLE ) {
		MP4Close(m_hMP4Handle);
		m_hMP4Handle = MP4_INVALID_FILE_HANDLE;
		return false;
	}
	// 文件打开正确, 抽取需要的数据...
	if( !this->doMP4ParseAV(m_hMP4Handle) ) {
		MP4Close(m_hMP4Handle);
		m_hMP4Handle = MP4_INVALID_FILE_HANDLE;
		return false;
	}
	return true;
}

bool CMP4Thread::doMP4ParseAV(MP4FileHandle inFile)
{
	if( inFile == MP4_INVALID_FILE_HANDLE )
		return false;
	ASSERT( inFile != MP4_INVALID_FILE_HANDLE );
	
	// 首先获取文件的总时间长度(毫秒)...
	m_dwMP4Duration = (uint32_t)MP4GetDuration(inFile);
	ASSERT( m_dwMP4Duration > 0 );

	// 获取需要的相关信息...
    uint32_t trackCount = MP4GetNumberOfTracks( inFile );
    for( uint32_t i = 0; i < trackCount; ++i ) {
		MP4TrackId  id = MP4FindTrackId( inFile, i );
		const char* type = MP4GetTrackType( inFile, id );
		if( MP4_IS_VIDEO_TRACK_TYPE( type ) ) {
			// 视频已经有效，检测下一个...
			if( m_tidVideo > 0 )
				continue;
			// 获取视频信息...
			m_tidVideo = id;
			m_bVideoComplete = false;
			char * lpVideoInfo = MP4Info(inFile, id);
			free(lpVideoInfo);
			// 获取视频的PPS/SPS信息...
			uint8_t  ** spsHeader = NULL;
			uint8_t  ** ppsHeader = NULL;
			uint32_t  * spsSize = NULL;
			uint32_t  * ppsSize = NULL;
			uint32_t    ix = 0;
			bool bResult = MP4GetTrackH264SeqPictHeaders(inFile, id, &spsHeader, &spsSize, &ppsHeader, &ppsSize);
			for(ix = 0; spsSize[ix] != 0; ++ix) {
				// SPS指针和长度...
				uint8_t * lpSPS = spsHeader[ix];
				uint32_t  nSize = spsSize[ix];
				// 只存储第一个SPS信息...
				if( m_strSPS.size() <= 0 && nSize > 0 ) {
					m_strSPS.assign((char*)lpSPS, nSize);
				}
				free(spsHeader[ix]);
			}
			free(spsHeader);
			free(spsSize);
			for(ix = 0; ppsSize[ix] != 0; ++ix) {
				// PPS指针和长度...
				uint8_t * lpPPS = ppsHeader[ix];
				uint32_t  nSize = ppsSize[ix];
				// 只存储第一个PPS信息...
				if( m_strPPS.size() <= 0 && nSize > 0 ) {
					m_strPPS.assign((char*)lpPPS, nSize);
				}
				free(ppsHeader[ix]);
			}
			free(ppsHeader);
			free(ppsSize);

			// 保存视频数据头...
			this->WriteAVCSequenceHeader();
		}
		else if( MP4_IS_AUDIO_TRACK_TYPE( type ) ) {
			// 音频已经有效，检测下一个...
			if( m_tidAudio > 0 )
				continue;
			// 获取音频信息...
			m_tidAudio = id;
			m_bAudioComplete = false;
			char * lpAudioInfo = MP4Info(inFile, id);
			free(lpAudioInfo);

			// 获取音频的类型/名称/采样率/声道信息...
			m_audio_type = MP4GetTrackAudioMpeg4Type(inFile, id);
			m_audio_channel_num = MP4GetTrackAudioChannels(inFile, id);
			const char * audio_name = MP4GetTrackMediaDataName(inFile, id);
			uint32_t timeScale = MP4GetTrackTimeScale(inFile, id);
			if (timeScale == 48000)
				m_audio_rate_index = 0x03;
			else if (timeScale == 44100)
				m_audio_rate_index = 0x04;
			else if (timeScale == 32000)
				m_audio_rate_index = 0x05;
			else if (timeScale == 24000)
				m_audio_rate_index = 0x06;
			else if (timeScale == 22050)
				m_audio_rate_index = 0x07;
			else if (timeScale == 16000)
				m_audio_rate_index = 0x08;
			else if (timeScale == 12000)
				m_audio_rate_index = 0x09;
			else if (timeScale == 11025)
				m_audio_rate_index = 0x0a;
			else if (timeScale == 8000)
				m_audio_rate_index = 0x0b;
			// 获取音频扩展信息...
            uint8_t  * pAES = NULL;
            uint32_t   nSize = 0;
            bool haveEs = MP4GetTrackESConfiguration(inFile, id, &pAES, &nSize);
			// 存储音频扩展信息...
			if( m_strAES.size() <= 0 && nSize > 0 ) {
				m_strAES.assign((char*)pAES, nSize);
			}
			free(pAES);

			// 保存音频数据头...
			this->WriteAACSequenceHeader();
		}
	}
	return true;
}

void CMP4Thread::WriteAACSequenceHeader()
{
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

	if( m_strAES.size() > 0 ) {
		memcpy(pbuf, m_strAES.c_str(), m_strAES.size());
		pbuf += m_strAES.size();
	} else {
		PutBitContext pb;
		init_put_bits(&pb, pbuf, 1024);
		put_bits(&pb, 5, m_audio_type);			//object type - AAC-LC
		put_bits(&pb, 4, m_audio_rate_index);	// sample rate index
		put_bits(&pb, 4, m_audio_channel_num);  // channel configuration

		//GASpecificConfig
		put_bits(&pb, 1, 0);    // frame length - 1024 samples
		put_bits(&pb, 1, 0);    // does not depend on core coder
		put_bits(&pb, 1, 0);    // is not extension

		flush_put_bits(&pb);
		pbuf += 2;
	}

	// 保存AAC数据头信息...
	int aac_len = (int)(pbuf - aac_seq_buf);
	m_strAACHeader.assign(aac_seq_buf, aac_len);
}

void CMP4Thread::WriteAVCSequenceHeader()
{
	// 获取 width 和 height...
	int	nPicWidth = 0;
	int	nPicHeight = 0;
	if( m_strSPS.size() >  0 ) {
		CSPSReader _spsreader;
		bs_t    s = {0};
		s.p		  = (uint8_t *)m_strSPS.c_str();
		s.p_start = (uint8_t *)m_strSPS.c_str();
		s.p_end	  = (uint8_t *)m_strSPS.c_str() + m_strSPS.size();
		s.i_left  = 8; // 这个是固定的,对齐长度...
		_spsreader.Do_Read_SPS(&s, &nPicWidth, &nPicHeight);
		m_nVideoHeight = nPicHeight;
		m_nVideoWidth = nPicWidth;
	}

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

CRtspThread::CRtspThread()
{
	m_nVideoHeight = 0;
	m_nVideoWidth = 0;

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

BOOL CRtspThread::InitRtsp(CPushThread * inPushThread, string & strRtspUrl)
{
	// 保存传递的参数...
	m_lpPushThread = inPushThread;
	m_strRtspUrl = strRtspUrl;

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

	// 2017.06.12 - by jackey => 这里是拉流模式，不能调用 doErrNotify，需要整个断开的方式...
	// 在超时机制中会首先检测m_bFinished标志，速度快...
	// 这里只需要设置标志，超时机制会自动删除...
	//if( m_lpPushThread != NULL ) {
	//	m_lpPushThread->doErrNotify();
	//}

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
	// 如果是流转发模式，只设置流播放状态...
	if( !m_lpPushThread->IsCameraDevice() ) {
		m_lpPushThread->SetStreamPlaying(true);
		return;
	}
	// 至少有一个音频或视频已经准备好了...
	ASSERT( m_lpPushThread->IsCameraDevice() );
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
	// 获取 width 和 height...
	int	nPicWidth = 0;
	int	nPicHeight = 0;
	if( inSPS.size() >  0 ) {
		CSPSReader _spsreader;
		bs_t    s = {0};
		s.p		  = (uint8_t *)inSPS.c_str();
		s.p_start = (uint8_t *)inSPS.c_str();
		s.p_end	  = (uint8_t *)inSPS.c_str() + inSPS.size();
		s.i_left  = 8; // 这个是固定的,对齐长度...
		_spsreader.Do_Read_SPS(&s, &nPicWidth, &nPicHeight);
		m_nVideoHeight = nPicHeight;
		m_nVideoWidth = nPicWidth;
	}

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
	m_nVideoHeight = 0;
	m_nVideoWidth = 0;

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

BOOL CRtmpThread::InitRtmp(CPushThread * inPushThread, string & strRtmpUrl)
{
	// 保存传递的参数...
	m_lpPushThread = inPushThread;
	m_strRtmpUrl = strRtmpUrl;

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
		// 2017.06.12 - by jackey => 这里是拉流模式，不能调用 doErrNotify，需要整个断开的方式...
		// 在超时机制中会首先检测m_bFinished标志，速度快...
		// 这里只需要设置标志，超时机制会自动删除...
		m_bFinished = true;
		return;
	}
	// 循环读取数据并转发出去...
	while( !this->IsStopRequested() ) {
		if( m_lpRtmp->IsClosed() || !m_lpRtmp->Read() ) {
			// 2017.06.12 - by jackey => 这里是拉流模式，不能调用 doErrNotify，需要整个断开的方式...
			// 在超时机制中会首先检测m_bFinished标志，速度快...
			// 这里只需要设置标志，超时机制会自动删除...
			m_bFinished = true;
			return;
		}
	}
}

void CRtmpThread::StartPushThread()
{
	if( m_lpPushThread == NULL )
		return;
	ASSERT( m_lpPushThread != NULL );
	// 如果是流转发模式，只设置流播放状态...
	if( !m_lpPushThread->IsCameraDevice() ) {
		m_lpPushThread->SetStreamPlaying(true);
		return;
	}
	// 至少有一个音频或视频已经准备好了...
	ASSERT( m_lpPushThread->IsCameraDevice() );
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
	// 获取 width 和 height...
	int	nPicWidth = 0;
	int	nPicHeight = 0;
	if( inSPS.size() >  0 ) {
		CSPSReader _spsreader;
		bs_t    s = {0};
		s.p		  = (uint8_t *)inSPS.c_str();
		s.p_start = (uint8_t *)inSPS.c_str();
		s.p_end	  = (uint8_t *)inSPS.c_str() + inSPS.size();
		s.i_left  = 8; // 这个是固定的,对齐长度...
		_spsreader.Do_Read_SPS(&s, &nPicWidth, &nPicHeight);
		m_nVideoHeight = nPicHeight;
		m_nVideoWidth = nPicWidth;
	}

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
	m_nStreamProp = kStreamDevice;
	m_bIsPublishing = false;
	m_bStreamPlaying = false;
	
	m_nKeyFrame = 0;
	m_dwFirstSendTime = 0;

	m_lpRtmpPush = NULL;
	m_lpCamera = NULL;
	m_lpMP4Thread = NULL;
	m_lpRtspThread = NULL;
	m_lpRtmpThread = NULL;
	m_bDeleteFlag = false;
	m_dwTimeOutMS = 0;

	m_strRtmpUrl.clear();
	m_nCurSendByte = 0;
	m_nSendKbps = 0;
	m_nRecvKbps = 0;
	m_nCurRecvByte = 0;
	m_dwRecvTimeMS = 0;

	m_lpRecMP4 = NULL;
	m_dwWriteSize = 0;
	m_dwWriteRecMS = 0;
}

CPushThread::~CPushThread()
{
	// 停止线程...
	this->StopAndWaitForThread();

	// 删除rtmp对象，这里必须加互斥，避免Connect返回时rtmp已经被删除，造成内存错误...
	{
		OSMutexLocker theLock(&m_Mutex);
		if( m_lpRtmpPush != NULL ) {
			delete m_lpRtmpPush;
			m_lpRtmpPush = NULL;
		}
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
	// 删除MP4线程...
	if( m_lpMP4Thread != NULL ) {
		delete m_lpMP4Thread;
		m_lpMP4Thread = NULL;
	}
	// 中断录像...
	this->StreamEndRecord();

	// 复位相关标志...
	m_nKeyFrame = 0;
	m_bIsPublishing = false;
	m_bStreamPlaying = false;
	TRACE("[~CPushThread Thread] - Exit\n");
}
//
// 处理摄像头设备的线程初始化...
BOOL CPushThread::DeviceInitThread(CCamera * lpCamera, CString & strRtspUrl, string & strRtmpUrl)
{
	// 保存传递过来的参数...
	m_lpCamera = lpCamera;
	m_strRtmpUrl = strRtmpUrl;
	m_nStreamProp = m_lpCamera->GetStreamProp();
	if( m_nStreamProp != kStreamDevice )
		return false;
	ASSERT( m_nStreamProp == kStreamDevice );
	// 创建rtsp拉流数据线程...
	string strNewRtsp = strRtspUrl.GetString();
	m_lpRtspThread = new CRtspThread();
	m_lpRtspThread->InitRtsp(this, strNewRtsp);

	// 创建rtmp上传对象...
	m_lpRtmpPush = new LibRtmp(false, true, NULL, NULL);
	ASSERT( m_lpRtmpPush != NULL );

	// 初始化接收数据超时时间记录起点 => 超过 3 分钟无数据接收，则判定为超时...
	m_dwTimeOutMS = ::GetTickCount();

	// 这里不能启动线程，等待环境准备妥当之后才能启动...

	return true;
}
//
// 处理流转发线程的初始化...
BOOL CPushThread::StreamInitThread(CCamera * lpCamera, BOOL bFileMode, string & strStreamUrl, string & strStreamMP4)
{
	// 保存传递过来的参数...
	m_lpCamera = lpCamera;
	m_nStreamProp = m_lpCamera->GetStreamProp();
	if( m_nStreamProp == kStreamDevice )
		return false;
	ASSERT( m_nStreamProp != kStreamDevice );
	// 创建MP4线程或rtsp或rtmp线程...
	if( bFileMode ) {
		m_lpMP4Thread = new CMP4Thread();
		m_lpMP4Thread->InitMP4(this, strStreamMP4);
	} else {
		if( strnicmp("rtsp://", strStreamUrl.c_str(), strlen("rtsp://")) == 0 ) {
			m_lpRtspThread = new CRtspThread();
			m_lpRtspThread->InitRtsp(this, strStreamUrl);
		} else {
			m_lpRtmpThread = new CRtmpThread();
			m_lpRtmpThread->InitRtmp(this, strStreamUrl);
		}
	}
	// 初始化接收码流时间起点 => 每隔1秒钟复位...
	m_dwRecvTimeMS = ::GetTickCount();
	// 初始化接收数据超时时间记录起点 => 超过 3 分钟无数据接收，则判定为超时...
	m_dwTimeOutMS = ::GetTickCount();
	return true;
}
//
// 启动流转发直播上传...
BOOL CPushThread::StreamStartLivePush(CCamera * lpCamera, string & strRtmpUrl)
{
	if( m_lpCamera != lpCamera )
		return false;
	// IE8会连续发送两遍播放命令，需要确保只启动一次...
	if( m_lpRtmpPush != NULL )
		return true;
	// 保存需要的数据信息...
	m_strRtmpUrl = strRtmpUrl;
	ASSERT( strRtmpUrl.size() > 0 );
	ASSERT( m_lpCamera == lpCamera );
	ASSERT( m_nStreamProp != kStreamDevice );
	// 创建rtmp上传对象...
	m_lpRtmpPush = new LibRtmp(false, true, NULL, NULL);
	ASSERT( m_lpRtmpPush != NULL );
	// 理解启动上传线程...
	this->Start();
	return true;
}
//
// 停止流转发直播上传...
BOOL CPushThread::StreamStopLivePush(CCamera * lpCamera)
{
	if( m_lpCamera != lpCamera )
		return false;
	ASSERT( m_lpCamera == lpCamera );
	ASSERT( m_nStreamProp != kStreamDevice );
	// 停止上传线程，复位发布状态标志...
	this->StopAndWaitForThread();
	this->SetStreamPublish(false);
	this->EndSendPacket();
	// 复位变量信息...
	m_dwFirstSendTime = 0;
	m_nCurSendByte = 0;
	m_nSendKbps = 0;
	return true;
}

BOOL CPushThread::MP4CreateVideoTrack()
{
	// 判断MP4对象是否有效...
	if( m_lpRecMP4 == NULL || m_strUTF8MP4.size() <= 0 )
		return false;
	ASSERT( m_lpRecMP4 != NULL && m_strUTF8MP4.size() > 0 );

	string strSPS, strPPS;
	int nHeight = 0;
	int nWidth = 0;
	
	if( m_lpRtspThread != NULL ) {
		strSPS = m_lpRtspThread->GetVideoSPS();
		strPPS = m_lpRtspThread->GetVideoPPS();
		nWidth = m_lpRtspThread->GetVideoWidth();
		nHeight = m_lpRtspThread->GetVideoHeight();
	} else if( m_lpRtmpThread != NULL ) {
		strSPS = m_lpRtmpThread->GetVideoSPS();
		strPPS = m_lpRtmpThread->GetVideoPPS();
		nWidth = m_lpRtmpThread->GetVideoWidth();
		nHeight = m_lpRtmpThread->GetVideoHeight();
	} else if( m_lpMP4Thread != NULL ) {
		strSPS = m_lpMP4Thread->GetVideoSPS();
		strPPS = m_lpMP4Thread->GetVideoPPS();
		nWidth = m_lpMP4Thread->GetVideoWidth();
		nHeight = m_lpMP4Thread->GetVideoHeight();
	}
	// 判断获取数据的有效性...
	if( nWidth <= 0 || nHeight <= 0 || strPPS.size() <= 0 || strSPS.size() <= 0 )
		return false;
	// 创建视频轨道...
	ASSERT( !m_lpRecMP4->IsVideoCreated() );
	return m_lpRecMP4->CreateVideoTrack(m_strUTF8MP4.c_str(), VIDEO_TIME_SCALE, nWidth, nHeight, strSPS, strPPS);
}

BOOL CPushThread::MP4CreateAudioTrack()
{
	// 判断MP4对象是否有效...
	if( m_lpRecMP4 == NULL || m_strUTF8MP4.size() <= 0 )
		return false;
	ASSERT( m_lpRecMP4 != NULL && m_strUTF8MP4.size() > 0 );

	int audio_type = 2;
	int	audio_rate_index = 0;			// 采样率编号
	int	audio_channel_num = 0;			// 声道数目
	int	audio_sample_rate = 48000;		// 音频采样率

	if( m_lpRtspThread != NULL ) {
		audio_rate_index = m_lpRtspThread->GetAudioRateIndex();
		audio_channel_num = m_lpRtspThread->GetAudioChannelNum();
	} else if( m_lpRtmpThread != NULL ) {
		audio_rate_index = m_lpRtmpThread->GetAudioRateIndex();
		audio_channel_num = m_lpRtmpThread->GetAudioChannelNum();
	} else if( m_lpMP4Thread != NULL ) {
		audio_type = m_lpMP4Thread->GetAudioType();
		audio_rate_index = m_lpMP4Thread->GetAudioRateIndex();
		audio_channel_num = m_lpMP4Thread->GetAudioChannelNum();
	}
  
	// 根据索引获取采样率...
	switch( audio_rate_index )
	{
	case 0x03: audio_sample_rate = 48000; break;
	case 0x04: audio_sample_rate = 44100; break;
	case 0x05: audio_sample_rate = 32000; break;
	case 0x06: audio_sample_rate = 24000; break;
	case 0x07: audio_sample_rate = 22050; break;
	case 0x08: audio_sample_rate = 16000; break;
	case 0x09: audio_sample_rate = 12000; break;
	case 0x0a: audio_sample_rate = 11025; break;
	case 0x0b: audio_sample_rate =  8000; break;
	default:   audio_sample_rate = 48000; break;
	}

	// AudioSpecificConfig
	char   aac_seq_buf[2048] = {0};
    char * pbuf = aac_seq_buf;
	PutBitContext pb;
	init_put_bits(&pb, pbuf, 1024);
	put_bits(&pb, 5, audio_type);		// object type - AAC-LC
	put_bits(&pb, 4, audio_rate_index);	// sample rate index
	put_bits(&pb, 4, audio_channel_num);// channel configuration

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
	ASSERT( !m_lpRecMP4->IsAudioCreated() );
	return m_lpRecMP4->CreateAudioTrack(m_strUTF8MP4.c_str(), audio_sample_rate, strAES);
}
//
// 开始流转发模式的录像...
BOOL CPushThread::StreamBeginRecord(LPCTSTR lpszPathMP4)
{
	// 这里需要进行互斥保护...
	OSMutexLocker theLock(&m_Mutex);
	// 判断状态是否有效...
	if( this->IsCameraDevice() || !this->IsStreamPlaying() )
		return false;
	ASSERT( !this->IsCameraDevice() );
	ASSERT( this->IsStreamPlaying() );
	ASSERT( lpszPathMP4 != NULL );
	// MP4路径必须是UTF-8的格式...
	if( lpszPathMP4 != NULL && strlen(lpszPathMP4) > 0 ) {
		m_strUTF8MP4 = CUtilTool::ANSI_UTF8(lpszPathMP4);
	}
	// 释放录像对象...
	if( m_lpRecMP4 != NULL ) {
		delete m_lpRecMP4;
		m_lpRecMP4 = NULL;
	}
	// 复位记录变量...
	m_dwWriteSize = 0;
	m_dwWriteRecMS = 0;
	// 创建新的录像对象...
	m_lpRecMP4 = new LibMP4();
	ASSERT( m_lpRecMP4 != NULL );
	// 创建视频轨道...
	this->MP4CreateVideoTrack();
	// 创建音频轨道...
	this->MP4CreateAudioTrack();
	return true;
}
//
// 进行录像处理...
BOOL CPushThread::StreamWriteRecord(FMS_FRAME & inFrame)
{
	// 一定是流转发模式下才会有效...
	if( m_lpRecMP4 == NULL )
		return false;
	ASSERT( !this->IsCameraDevice() );
	// 进行写盘操作...
	BOOL bIsVideo = ((inFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO) ? true : false);
	if( !m_lpRecMP4->WriteSample(bIsVideo, (BYTE*)inFrame.strData.c_str(), inFrame.strData.size(), inFrame.dwSendTime, inFrame.dwRenderOffset, inFrame.is_keyframe) )
		return false;
	// 累加存盘长度，记录已经写入的时间戳...
	m_dwWriteSize = m_lpRecMP4->GetWriteSize();
	m_dwWriteRecMS = m_lpRecMP4->GetWriteRecMS();
	return true;
}
//
// 停止流转发模式的录像...
BOOL CPushThread::StreamEndRecord()
{
	// 这里需要进行互斥保护...
	OSMutexLocker theLock(&m_Mutex);
	// 删除录像对象...
	if( m_lpRecMP4 != NULL ) {
		delete m_lpRecMP4;
		m_lpRecMP4 = NULL;
	}
	// 进行录像后的截图操作 => 截图完毕，复位录像记录变量...
	if( m_lpCamera != NULL && m_dwWriteSize > 0 && m_dwWriteRecMS > 0 ) {
		m_lpCamera->doStreamSnapJPG(m_dwWriteRecMS/1000);
		m_dwWriteSize = 0; m_dwWriteRecMS = 0;
	}
	return true;
}
//
// 返回是否正在录像标志...
BOOL CPushThread::IsRecording()
{
	OSMutexLocker theLock(&m_Mutex);
	return ((m_lpRecMP4 != NULL) ? true : false);
}

bool CPushThread::IsDataFinished()
{
	if( m_lpMP4Thread != NULL ) {
		return m_lpMP4Thread->IsFinished();
	} else if( m_lpRtspThread != NULL ) {
		return m_lpRtspThread->IsFinished();
	} else if( m_lpRtmpThread != NULL ) {
		return m_lpRtmpThread->IsFinished();
	}
	return true;
}

void CPushThread::SetStreamPlaying(BOOL bFlag)
{
	// 注意：这里需要通知网站，通道正在运行了...
	// 注意：这里是专门处理流转发模式的运行通知...
	m_bStreamPlaying = bFlag;
	if( m_lpCamera != NULL ) {
		m_lpCamera->doStreamStatus(bFlag ? "已连接" : "未连接");
		m_lpCamera->doWebStatCamera(kCameraRun);
	}
}

void CPushThread::SetStreamPublish(BOOL bFlag)
{
	m_bIsPublishing = bFlag;
	if( m_lpCamera == NULL )
		return;
	if( m_bIsPublishing ) {
		m_lpCamera->doStreamStatus("直播中");
	} else {
		m_lpCamera->doStreamStatus(m_bStreamPlaying ? "已连接" : "未连接");
	}
}
//
// 这里只需要处理推流失败的情况，拉流失败会全部通过超时机制自动处理...
void CPushThread::doErrNotify()
{
	// 如果线程已经被主动删除了，直接退出...
	if( m_bDeleteFlag )
		return;
	ASSERT( !m_bDeleteFlag );
	// 窗口对象必须有效...
	if( m_hWndParent == NULL )
		return;
	ASSERT( m_hWndParent != NULL );
	// 摄像头设备模式 => 删除整个数据处理线程..
	// 流数据转发模式 => 只停止rtmp上传对象...
	WPARAM wMsgID = ((m_nStreamProp == kStreamDevice) ? WM_ERR_PUSH_MSG : WM_STOP_STREAM_MSG);
	::PostMessage(m_hWndParent, wMsgID, NULL, NULL);
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
	// 保存发布成功标志...
	this->SetStreamPublish(true);
	// 保存需要的第一帧时间戳...
	this->BeginSendPacket();
	// 2017.06.15 - by jackey => 取消了这种延时反馈机制...
	// 上传准备完毕，通知 RemoteSession 可以反馈消息给直播播放器了...
	//if( m_lpCamera != NULL ) {
	//	m_lpCamera->doDelayTransmit(GM_NoErr);
	//}
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
	if( m_lpRtmpPush == NULL || m_strRtmpUrl.size() <= 0  )
		return false;
	ASSERT( m_lpRtmpPush != NULL && m_strRtmpUrl.size() > 0);
	
	// 连接rtmp server，完成握手等协议
	return m_lpRtmpPush->Open(m_strRtmpUrl.c_str());
}
//
// 通过其它渠道输送数据帧...
int CPushThread::PushFrame(FMS_FRAME & inFrame)
{
	OSMutexLocker theLock(&m_Mutex);
	// 将超时计时点复位，重新计时...
	m_dwTimeOutMS = ::GetTickCount();
	// 进行录像处理...
	this->StreamWriteRecord(inFrame);
	// 如果时间间隔大于了1000毫秒，则计算一次接收码流...
	DWORD dwCurTimeMS = ::GetTickCount();
	if((dwCurTimeMS - m_dwRecvTimeMS) >= 1000 ) {
		float fSecond = (dwCurTimeMS - m_dwRecvTimeMS)/1000.0f;
		m_nRecvKbps = m_nCurRecvByte * 8 / 1024 / fSecond;
		m_dwRecvTimeMS = dwCurTimeMS;
		m_nCurRecvByte = 0;
	}
	// 累加接收数据包的字节数，加入缓存队列...
	m_nCurRecvByte += inFrame.strData.size();
	m_MapFrame.insert(pair<uint32_t, FMS_FRAME>(inFrame.dwSendTime, inFrame));
	// 如果是摄像头，不进行关键帧计数和丢帧处理...
	if( this->IsCameraDevice() ) {
		return m_MapFrame.size();
	}
	// 如果是流转发模式，并且没有处于发布状态...
	ASSERT( !this->IsCameraDevice() );
	// 判断新数据是否是视频关键帧，不是视频关键帧，直接返回...
	if( inFrame.typeFlvTag != FLV_TAG_TYPE_VIDEO || !inFrame.is_keyframe ) {
		return m_MapFrame.size();
	}
	// 如果是视频关键帧，累加计数器...
	ASSERT( inFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO && inFrame.is_keyframe );
	// 累加关键帧计数器...
	++m_nKeyFrame;
	TRACE("== [PushFrame] nKeyFrame = %d, Size = %d, SendTime = %lu ==\n", m_nKeyFrame, m_MapFrame.size(), m_dwFirstSendTime);
	// 如果已经处于发布，直接返回...
	if( this->IsPublishing() ) {
		return m_MapFrame.size();
	}
	ASSERT( !this->IsPublishing() );
	// 如果超过3个关键帧，删除第一个关键帧的所有数据...
	if( m_nKeyFrame >= 3 ) {
		this->dropToKeyFrame();
	}
	// 直接返回总的帧数...
	return m_MapFrame.size();
}
//
// 删除缓冲队列中的数据，直到遇到关键帧为止...
void CPushThread::dropToKeyFrame()
{
	// 设置已经删除关键帧标志...
	BOOL bHasDelKeyFrame = false;
	KH_MapFrame::iterator itorItem = m_MapFrame.begin();
	while( itorItem != m_MapFrame.end() ) {
		int nSize = m_MapFrame.count(itorItem->first);
		// 对相同时间戳的数据帧进行循环删除...
		for(int i = 0; i < nSize; ++i) {
			FMS_FRAME & theFrame = itorItem->second;
			// 如果发现了视频关键帧...
			if( theFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO && theFrame.is_keyframe ) {
				// 已经删除过一个关键帧，遇到新的关键帧，设置发送时间戳，直接返回...
				if( bHasDelKeyFrame ) {
					m_dwFirstSendTime = theFrame.dwSendTime;
					TRACE("== [dropToKeyFrame] nKeyFrame = %d, Size = %d, SendTime = %lu ==\n", m_nKeyFrame, m_MapFrame.size(), m_dwFirstSendTime);
					return;
				}
				// 删除这个关键帧，设置标志，打印信息...
				--m_nKeyFrame; bHasDelKeyFrame = true;
			}
			// 这里使用算子删除而不是关键字...
			m_MapFrame.erase(itorItem++);
		}
	}
}
//
// 开始准备发送数据包...
void CPushThread::BeginSendPacket()
{
	OSMutexLocker theLock(&m_Mutex);
	ASSERT( this->IsPublishing() );
	// 只有在流转发模式下才进行，开始时间的调整...
	// 因为摄像头模式下，都是单独重头链接 rtsp 不会有帧断裂的问题...
	if( this->IsCameraDevice() )
		return;
	ASSERT( !this->IsCameraDevice() );
	// 设置第一帧的发送时间戳...
	KH_MapFrame::iterator itorItem = m_MapFrame.begin();
	if( itorItem != m_MapFrame.end() ) {
		m_dwFirstSendTime = itorItem->first;
		TRACE("== [BeginSendPacket] nKeyFrame = %d, Size = %d, SendTime = %lu ==\n", m_nKeyFrame, m_MapFrame.size(), m_dwFirstSendTime);
	}
}
//
// 停止发送数据包...
void CPushThread::EndSendPacket()
{
	// 删除上传对象，需要互斥保护...
	OSMutexLocker theLock(&m_Mutex);
	if( m_lpRtmpPush != NULL ) {
		delete m_lpRtmpPush;
		m_lpRtmpPush = NULL;
	}
	// 如果是流转发模式，需要将缓存的数据插入到缓存发送队列前面...
	if( this->IsCameraDevice() )
		return;
	ASSERT( !this->IsCameraDevice() );
	// 遍历已经缓存的流转发数据(这部分数据已经发给服务器，现在要把它们找回来)...
	KH_MapFrame::iterator itorItem = m_MapStream.begin();
	while( itorItem != m_MapStream.end() ) {
		int nSize = m_MapStream.count(itorItem->first);
		// 对相同时间戳的数据帧进行循环删除...
		for(int i = 0; i < nSize; ++i) {
			FMS_FRAME & theFrame = itorItem->second;
			// 这里可以进行正向读取，因为是按照时间戳排列，插入后会自动排序...
			m_MapFrame.insert(pair<uint32_t, FMS_FRAME>(theFrame.dwSendTime, theFrame));
			// 累加算子，插入下一个节点...
			++itorItem;
		}
	}
	// 如果缓存队列有数据，关键帧计数器增加...
	if( m_MapStream.size() > 0 ) {
		++m_nKeyFrame;
		TRACE("== [EndSendPacket] nKeyFrame = %d, Size = %d, SendTime = %lu ==\n", m_nKeyFrame, m_MapFrame.size(), m_dwFirstSendTime);
	}
	// 清空缓存队列...
	m_MapStream.clear();
}
//
// 获取接收码流 => -1 表示超时...
int CPushThread::GetRecvKbps()
{
	// 如果发生超时，返回 -1，等待删除...
	if( this->IsFrameTimeout() ) {
		MsgLogGM(GM_Err_Timeout);
		return -1;
	}
	// 返回接收码流...
	return m_nRecvKbps;
}
//
// 检查数据包是否已经超时...
// 流转发模式下 => 设置的超时起点是 StreamInitThread
// 摄像头设备下 => 设置的超时起点是 DeviceInitThread
// 都是会立即获取拉流数据的位置，超时函数对所有模式有效...
BOOL CPushThread::IsFrameTimeout()
{
	OSMutexLocker theLock(&m_Mutex);
	// 首先判断数据线程是否已经结束了...
	if( this->IsDataFinished() )
		return true;
	// 一直没有新数据到达，超过 3 分钟，则判定为超时...
	int nWaitMinute = 3;
	DWORD dwElapseSec = (::GetTickCount() - m_dwTimeOutMS) / 1000;
	return ((dwElapseSec >= nWaitMinute * 60) ? true : false);
}
//
// 发送一个帧数据包...
int CPushThread::SendOneDataPacket()
{
	OSMutexLocker theLock(&m_Mutex);
	// 数据已经结束了，直接返回-1...
	if( this->IsDataFinished() )
		return -1;
	// 如果数据还没有结束，则需要有一定缓存，以便音视频能够自动排序，然后再发送数据包...
	if( m_MapFrame.size() < 100 )
		return 0;
	ASSERT( !this->IsDataFinished() && m_MapFrame.size() >= 100 );

	bool is_ok = false;
	ASSERT( m_MapFrame.size() > 0 );

	// 读取第一个数据包(可能包含多个数据)...
	KH_MapFrame::iterator itorItem = m_MapFrame.begin();
	int nSize = m_MapFrame.count(itorItem->first);
	// 对相同时间戳的数据帧进行循环处理...
	for(int i = 0; i < nSize; ++i) {
		FMS_FRAME & theFrame = itorItem->second;
		// 如果是流转发模式，才需要对时间戳进行调整，硬件模式下的时间戳都是从0开始的，无需调整...
		if( !this->IsCameraDevice() ) {
			theFrame.dwSendTime = theFrame.dwSendTime - m_dwFirstSendTime;
		}
		//TRACE("[%s] SendTime = %lu, Key = %d, Size = %d, MapSize = %d\n", ((theFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO) ? "Video" : "Audio"), theFrame.dwSendTime, theFrame.is_keyframe, theFrame.strData.size(), m_MapFrame.size());
		// 将获取到的数据帧发送走...
		switch( theFrame.typeFlvTag )
		{
		case FLV_TAG_TYPE_AUDIO: is_ok = this->SendAudioDataPacket(theFrame); break;
		case FLV_TAG_TYPE_VIDEO: is_ok = this->SendVideoDataPacket(theFrame); break;
		}
		// 累加发送字节数...
		m_nCurSendByte += theFrame.strData.size();
		// 如果是流转发模式 => 缓存关键帧 => 先清空以前缓存的数据 => 关键帧计数器减少...
		if( !this->IsCameraDevice() ) {
			// 把时间戳还原回去...
			theFrame.dwSendTime = theFrame.dwSendTime + m_dwFirstSendTime;
			// 发现新的关键帧，清空以前的缓存，减少关键帧计数器...
			if( theFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO && theFrame.is_keyframe ) {
				--m_nKeyFrame; m_MapStream.clear();
				TRACE("== [SendOneDataPacket] nKeyFrame = %d, Size = %d, FirstSendTime = %lu, CurSendTime = %lu ==\n", m_nKeyFrame, m_MapFrame.size(), m_dwFirstSendTime, theFrame.dwSendTime);
			}
			// 将数据转存到专门的缓存队列当中 => 第一帧肯定是关键帧...
			m_MapStream.insert(pair<uint32_t, FMS_FRAME>(theFrame.dwSendTime, theFrame));
		}
		// 从队列中移除一个相同时间的数据包 => 这里使用的是算子...
		m_MapFrame.erase(itorItem++);
	}

	/*KH_MapFrame::iterator itorFrame;
	KH_MapFrame::iterator itorPair;
	pair<KH_MapFrame::iterator, KH_MapFrame::iterator> myPair;

	// 获取队列头，找到时间戳重复的列表...
	itorFrame = m_MapFrame.begin();
	myPair = m_MapFrame.equal_range(itorFrame->first);
	for(itorPair = myPair.first; itorPair != myPair.second; ++itorPair) {
		// 遍历这个相同时间的列表，发送数据包...
		ASSERT( itorPair->first == itorFrame->first );
		FMS_FRAME & theFrame = itorPair->second;
		// 如果是流转发模式，才需要对时间戳进行调整，硬件模式下的时间戳都是从0开始的，无需调整...
		if( !this->IsCameraDevice() ) {
			theFrame.dwSendTime = theFrame.dwSendTime - m_dwFirstSendTime;
		}
		//TRACE("[%s] SendTime = %lu, Key = %d, Size = %d, MapSize = %d\n", ((theFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO) ? "Video" : "Audio"), theFrame.dwSendTime, theFrame.is_keyframe, theFrame.strData.size(), m_MapFrame.size());
		// 将获取到的数据帧发送走...
		switch( theFrame.typeFlvTag )
		{
		case FLV_TAG_TYPE_AUDIO: is_ok = this->SendAudioDataPacket(theFrame); break;
		case FLV_TAG_TYPE_VIDEO: is_ok = this->SendVideoDataPacket(theFrame); break;
		}
		// 累加发送字节数...
		m_nCurSendByte += theFrame.strData.size();
	}
	// 从队列中移除一个相同时间的数据包 => 一定要用关键字删除，否则，删不干净...
	m_MapFrame.erase(itorFrame->first);*/

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
    pbuf = UI08ToBytes(pbuf, 1);						// avc packet type (0, nalu)
	pbuf = UI24ToBytes(pbuf, inFrame.dwRenderOffset);   // composition time

    memcpy(pbuf, inFrame.strData.c_str(), inFrame.strData.size());
    pbuf += inFrame.strData.size();

    is_ok = m_lpRtmpPush->Send(video_mem_buf_, (int)(pbuf - video_mem_buf_), FLV_TAG_TYPE_VIDEO, inFrame.dwSendTime);

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

    is_ok = m_lpRtmpPush->Send(audio_mem_buf_, (int)(pbuf - audio_mem_buf_), FLV_TAG_TYPE_AUDIO, inFrame.dwSendTime);

	delete [] audio_mem_buf_;
	
	//TRACE("[Audio] SendTime = %lu, StartTime = %lu\n", inFrame.dwSendTime, inFrame.dwStartTime);

	return is_ok;
}
//
// 发送视频序列头...
BOOL CPushThread::SendAVCSequenceHeaderPacket()
{
	OSMutexLocker theLock(&m_Mutex);
	if( m_lpRtmpPush == NULL )
		return false;

	string strAVC;

	if( m_lpRtspThread != NULL ) {
		strAVC = m_lpRtspThread->GetAVCHeader();
	} else if( m_lpRtmpThread != NULL ) {
		strAVC = m_lpRtmpThread->GetAVCHeader();
	} else if( m_lpMP4Thread != NULL ) {
		strAVC = m_lpMP4Thread->GetAVCHeader();
	}

	// 没有视频数据...
	if( strAVC.size() <= 0 )
		return true;
	
	ASSERT( strAVC.size() > 0 );
	return m_lpRtmpPush->Send(strAVC.c_str(), strAVC.size(), FLV_TAG_TYPE_VIDEO, 0);
}

//
// 发送音频序列头...
BOOL CPushThread::SendAACSequenceHeaderPacket()
{
	OSMutexLocker theLock(&m_Mutex);
	if( m_lpRtmpPush == NULL )
		return false;

	string strAAC;

	if( m_lpRtspThread != NULL ) {
		strAAC = m_lpRtspThread->GetAACHeader();
	} else if( m_lpRtmpThread != NULL ) {
		strAAC = m_lpRtmpThread->GetAACHeader();
	} else if( m_lpMP4Thread != NULL ) {
		strAAC = m_lpMP4Thread->GetAACHeader();
	}

	// 没有音频数据...
	if( strAAC.size() <= 0 )
		return true;

	ASSERT( strAAC.size() > 0 );
    return m_lpRtmpPush->Send(strAAC.c_str(), strAAC.size(), FLV_TAG_TYPE_AUDIO, 0);
}

BOOL CPushThread::SendMetadataPacket()
{
	OSMutexLocker theLock(&m_Mutex);
	if( m_lpRtmpPush == NULL )
		return false;

	char   metadata_buf[4096];
    char * pbuf = this->WriteMetadata(metadata_buf);

    return m_lpRtmpPush->Send(metadata_buf, (int)(pbuf - metadata_buf), FLV_TAG_TYPE_META, 0);
}

char * CPushThread::WriteMetadata(char * buf)
{
	int nVideoWidth = 0;
	int nVideoHeight = 0;
    char* pbuf = buf;

    pbuf = UI08ToBytes(pbuf, AMF_DATA_TYPE_STRING);
    pbuf = AmfStringToBytes(pbuf, "@setDataFrame");

    pbuf = UI08ToBytes(pbuf, AMF_DATA_TYPE_STRING);
    pbuf = AmfStringToBytes(pbuf, "onMetaData");

	//pbuf = UI08ToBytes(pbuf, AMF_DATA_TYPE_MIXEDARRAY);
	//pbuf = UI32ToBytes(pbuf, 2);
    pbuf = UI08ToBytes(pbuf, AMF_DATA_TYPE_OBJECT);

	// 设置视频宽度属性...
	if( m_lpRtspThread != NULL ) {
		nVideoWidth = m_lpRtspThread->GetVideoWidth();
	} else if( m_lpRtmpThread != NULL ) {
		nVideoWidth = m_lpRtmpThread->GetVideoWidth();
	} else if( m_lpMP4Thread != NULL ) {
		nVideoWidth = m_lpMP4Thread->GetVideoWidth();
	}
	if( nVideoWidth > 0 ) {
	    pbuf = AmfStringToBytes(pbuf, "width");
		pbuf = AmfDoubleToBytes(pbuf, nVideoWidth);
	}
	// 设置视频高度属性...
	if( m_lpRtspThread != NULL ) {
		nVideoHeight = m_lpRtspThread->GetVideoHeight();
	} else if( m_lpRtmpThread != NULL ) {
		nVideoHeight = m_lpRtmpThread->GetVideoHeight();
	} else if( m_lpMP4Thread != NULL ) {
		nVideoHeight = m_lpMP4Thread->GetVideoHeight();
	}
	if( nVideoHeight > 0 ) {
	    pbuf = AmfStringToBytes(pbuf, "height");
		pbuf = AmfDoubleToBytes(pbuf, nVideoHeight);
	}
	// 设置视频帧率属性...
	/*int nVideoFPS = 0;
	if( m_lpRtspThread != NULL ) {
		nVideoFPS = m_lpRtspThread->GetVideoFPS();
	} else if( m_lpRtmpThread != NULL ) {
		nVideoFPS = m_lpRtmpThread->GetVideoFPS();
	} else if( m_lpMP4Thread != NULL ) {
		nVideoFPS = m_lpMP4Thread->GetVideoFPS();
	}
	if( nVideoFPS > 0 ) {
	    pbuf = AmfStringToBytes(pbuf, "framerate");
		pbuf = AmfDoubleToBytes(pbuf, 25);//nVideoFPS);
	}*/
	// 统一设置默认的视频帧率属性...
    pbuf = AmfStringToBytes(pbuf, "framerate");
	pbuf = AmfDoubleToBytes(pbuf, 25);//nVideoFPS);

	pbuf = AmfStringToBytes(pbuf, "videocodecid");
    pbuf = UI08ToBytes(pbuf, AMF_DATA_TYPE_STRING);
    pbuf = AmfStringToBytes(pbuf, "avc1");

    // 0x00 0x00 0x09
    pbuf = AmfStringToBytes(pbuf, "");
    pbuf = UI08ToBytes(pbuf, AMF_DATA_TYPE_OBJECT_END);
    
    return pbuf;
}

void CPushThread::Initialize()
{
	srs_initialize();
}

void CPushThread::UnInitialize()
{
	srs_uninitialize();
}
