
#include "stdafx.h"
#include "PullThread.h"
#include "PushThread.h"
#include "XmlConfig.h"
#include "srs_librtmp.h"
#include "BitWritter.h"
#include "UtilTool.h"
#include "LibRtmp.h"
#include "Camera.h"
#include "LibMP4.h"
#include "ReadSPS.h"
#include "md5.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CMP4Thread::CMP4Thread()
{
	m_nVideoHeight = 0;
	m_nVideoWidth = 0;
	m_nVideoFPS = 25;

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

	m_iVSampleInx = 1;
	m_iASampleInx = 1;
	m_bFinished = false;
	m_bVideoComplete = true;
	m_bAudioComplete = true;
	m_tidAudio = MP4_INVALID_TRACK_ID;
	m_tidVideo = MP4_INVALID_TRACK_ID;

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

	MsgLogINFO("[~CMP4Thread Thread] - Exit");
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
				MsgLogINFO("== [File-Complete] ==");
				m_bFinished = true;
				return;
			}
			// 打印循环次数...
			ASSERT( m_bFileLoop );
			CUtilTool::MsgLog(kTxtLogger, "== [File-Loop] Count = %d ==\r\n", m_nLoopCount);
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

	// 计算发送时间 => PTS => 刻度时间转换成毫秒...
	msectime *= UINT64_C( 1000 );
	msectime /= timescale;
	// 计算开始时间 => DTS => 刻度时间转换成毫秒...
	nStartTime *= UINT64_C( 1000 );
	nStartTime /= timescale;
	// 计算偏差时间 => CTTS => 刻度时间转换成毫秒...
	nOffset *= UINT64_C( 1000 );
	nOffset /= timescale;

	// 注意：msectime | nOffset | m_dwMP4Duration 都要统一成毫秒时间...
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

	// 返回发送时间(毫秒) => 已将刻度时间转换成了毫秒...
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
		MsgLogGM(GM_File_Not_Open);
		m_hMP4Handle = MP4_INVALID_FILE_HANDLE;
		return false;
	}
	// 文件打开正确, 抽取需要的数据...
	if( !this->doMP4ParseAV(m_hMP4Handle) ) {
		MP4Close(m_hMP4Handle);
		MsgLogGM(GM_File_Not_Open);
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
	
	// 首先获取文件的每秒刻度数和总刻度数(不是毫秒数)...
	uint32_t dwFileScale = MP4GetTimeScale(inFile);
	MP4Duration theDuration = MP4GetDuration(inFile);
	// 总毫秒数 = 总刻度数*1000/每秒刻度数 => 先乘法可以降低误差...
	m_dwMP4Duration = theDuration*1000/dwFileScale;

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
			// 释放分配的缓存...
			if( pAES != NULL ) {
				free(pAES);
				pAES = NULL;
			}
			// 保存音频数据头...
			this->WriteAACSequenceHeader();
		}
	}
	// 如果音频和视频都没有，返回失败...
	if((m_tidVideo == MP4_INVALID_TRACK_ID) && (m_tidAudio == MP4_INVALID_TRACK_ID)) {
		MsgLogGM(GM_File_Read_Err);
		return false;
	}
	// 一切正常，返回成功...
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
	int nPicFPS = 0;
	int	nPicWidth = 0;
	int	nPicHeight = 0;
	if( m_strSPS.size() >  0 ) {
		/*CSPSReader _spsreader;
		bs_t    s = {0};
		s.p		  = (uint8_t *)m_strSPS.c_str();
		s.p_start = (uint8_t *)m_strSPS.c_str();
		s.p_end	  = (uint8_t *)m_strSPS.c_str() + m_strSPS.size();
		s.i_left  = 8; // 这个是固定的,对齐长度...
		_spsreader.Do_Read_SPS(&s, &nPicWidth, &nPicHeight);*/

		h264_decode_sps((BYTE*)m_strSPS.c_str(), m_strSPS.size(), nPicWidth, nPicHeight, nPicFPS);

		m_nVideoHeight = nPicHeight;
		m_nVideoWidth = nPicWidth;
		m_nVideoFPS = nPicFPS;
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
	m_nVideoFPS = 25;

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

	MsgLogINFO("== [~CRtspThread Thread] - Exit ==");
}

BOOL CRtspThread::InitRtsp(BOOL bUsingTCP, CPushThread * inPushThread, string & strRtspUrl)
{
	// 保存传递的参数...
	m_lpPushThread = inPushThread;
	m_strRtspUrl = strRtspUrl;

	// 创建rtsp链接环境...
	m_scheduler_ = BasicTaskScheduler::createNew();
	m_env_ = BasicUsageEnvironment::createNew(*m_scheduler_);
	m_rtspClient_ = ourRTSPClient::createNew(*m_env_, m_strRtspUrl.c_str(), 1, "rtspTransfer", bUsingTCP, this, NULL);
	
	// 2017.07.21 - by jackey => 有些服务器必须先发OPTIONS...
	// 发起第一次rtsp握手 => 先发起 OPTIONS 命令...
	m_rtspClient_->sendOptionsCommand(continueAfterOPTIONS); 

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
	// 启动UDP推流线程...
	//m_lpPushThread->StartUDPSendThread();
	// 设置流的播放状态...
	m_lpPushThread->SetStreamPlaying(true);

	/*// 如果是流转发模式，只设置流播放状态...
	if( !m_lpPushThread->IsCameraDevice() ) {
		m_lpPushThread->SetStreamPlaying(true);
		return;
	}
	// 至少有一个音频或视频已经准备好了...
	ASSERT( m_lpPushThread->IsCameraDevice() );
	ASSERT( m_strAACHeader.size() > 0 || m_strAVCHeader.size() > 0 );
	// 直接启动rtmp推送线程，开始启动rtmp推送过程...
	m_lpPushThread->Start();*/
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
	int nPicFPS = 0;
	int	nPicWidth = 0;
	int	nPicHeight = 0;
	if( inSPS.size() >  0 ) {
		/*CSPSReader _spsreader;
		bs_t    s = {0};
		s.p		  = (uint8_t *)inSPS.c_str();
		s.p_start = (uint8_t *)inSPS.c_str();
		s.p_end	  = (uint8_t *)inSPS.c_str() + inSPS.size();
		s.i_left  = 8; // 这个是固定的,对齐长度...
		_spsreader.Do_Read_SPS(&s, &nPicWidth, &nPicHeight);*/

		h264_decode_sps((BYTE*)inSPS.c_str(), inSPS.size(), nPicWidth, nPicHeight, nPicFPS);

		m_nVideoHeight = nPicHeight;
		m_nVideoWidth = nPicWidth;
		m_nVideoFPS = nPicFPS;
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
	m_nVideoFPS = 25;

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

	MsgLogINFO("== [~CRtmpThread Thread] - Exit ==");
}

BOOL CRtmpThread::InitRtmp(CPushThread * inPushThread, string & strRtmpUrl)
{
	// 保存传递的参数...
	m_lpPushThread = inPushThread;
	m_strRtmpUrl = strRtmpUrl;

	// 创建rtmp对象，注意是拉数据...
	m_lpRtmp = new LibRtmp(false, false, this);
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
	m_lpPushThread->SetStreamPlaying(true);

	/*// 如果是流转发模式，只设置流播放状态...
	if( !m_lpPushThread->IsCameraDevice() ) {
		m_lpPushThread->SetStreamPlaying(true);
		return;
	}
	// 至少有一个音频或视频已经准备好了...
	ASSERT( m_lpPushThread->IsCameraDevice() );
	ASSERT( m_strAACHeader.size() > 0 || m_strAVCHeader.size() > 0 );
	// 直接启动rtmp推送线程，开始启动rtmp推送过程...
	m_lpPushThread->Start();*/
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
	int nPicFPS = 0;
	int	nPicWidth = 0;
	int	nPicHeight = 0;
	if( inSPS.size() >  0 ) {
		/*CSPSReader _spsreader;
		bs_t    s = {0};
		s.p		  = (uint8_t *)inSPS.c_str();
		s.p_start = (uint8_t *)inSPS.c_str();
		s.p_end	  = (uint8_t *)inSPS.c_str() + inSPS.size();
		s.i_left  = 8; // 这个是固定的,对齐长度...
		_spsreader.Do_Read_SPS(&s, &nPicWidth, &nPicHeight);*/

		h264_decode_sps((BYTE*)inSPS.c_str(), inSPS.size(), nPicWidth, nPicHeight, nPicFPS);

		m_nVideoHeight = nPicHeight;
		m_nVideoWidth = nPicWidth;
		m_nVideoFPS = nPicFPS;
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