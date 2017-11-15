
#include "stdafx.h"
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

	// ��¼��ʼʱ���...
	m_dwStartMS = ::GetTickCount();
}

CMP4Thread::~CMP4Thread()
{
	// ֹͣ�߳�...
	this->StopAndWaitForThread();

	// �ͷ�MP4���...
	if( m_hMP4Handle != MP4_INVALID_FILE_HANDLE ) {
		MP4Close(m_hMP4Handle);
		m_hMP4Handle = MP4_INVALID_FILE_HANDLE;
	}

	TRACE("[~CMP4Thread Thread] - Exit\n");
}

void CMP4Thread::Entry()
{
	// ������׼��MP4�ļ�...
	if( !this->doPrepareMP4() ) {
		// ����ʧ�� => ����ֻ��Ҫ���ý�����־����ʱ���ƻ��Զ�ɾ�� => ����������ģʽ�����ܵ��� doErrNotify()...
		// �ڳ�ʱ�����л����ȼ��m_bFinished��־���ٶȿ�...
		m_bFinished = true;
		return;
	}
	// ������ת������״̬��־...
	m_lpPushThread->SetStreamPlaying(true);

	// �ڷ���ֱ��ʱ����Ҫ�����ϴ��̣߳�׼����ϣ��������߳�...
	//m_lpPushThread->Start();

	// ѭ����ȡ����֡...
	int nAccTime = 1000;
	uint32_t dwCompTime = 0;
	uint32_t dwElapsTime = 0;
	uint32_t dwOutVSendTime = 0;
	uint32_t dwOutASendTime = 0;
	while( !this->IsStopRequested() ) {

		// ������Ҫ�෢�ͼ����ӵ����ݣ�������ֻ���(ֻ��Ҫ����һ��)...
		dwElapsTime = ::GetTickCount() - m_dwStartMS + nAccTime;

		// ��ȡһ֡��Ƶ...
		if( (dwElapsTime >= dwOutVSendTime) && (!m_bVideoComplete) && (m_tidVideo != MP4_INVALID_TRACK_ID) ) {
			if( !ReadOneFrameFromMP4(m_tidVideo, m_iVSampleInx++, true, dwOutVSendTime) ) {
				m_bVideoComplete = true;
				continue;
			}
			//TRACE("[Video] ElapsTime = %lu, OutSendTime = %lu\n", dwElapsTime, dwOutVSendTime);
		}

		// ��ȡһ֡��Ƶ...
		if( (dwElapsTime >= dwOutASendTime) && (!m_bAudioComplete) && (m_tidAudio != MP4_INVALID_TRACK_ID) ) {
			if( !ReadOneFrameFromMP4(m_tidAudio, m_iASampleInx++, false, dwOutASendTime) ) {
				m_bAudioComplete = true;
				continue;
			}
			//TRACE("[Audio] ElapsTime = %lu, OutSendTime = %lu\n", dwElapsTime, dwOutASendTime);
		}

		// �����Ƶ����Ƶȫ�������������¼���...
		if( m_bVideoComplete && m_bAudioComplete ) {
			// �����ѭ����ֱ���˳��߳�...
			if( !m_bFileLoop ) {
				TRACE("[File-Complete]\n");
				m_bFinished = true;
				return;
			}
			// ��ӡѭ������...
			ASSERT( m_bFileLoop );
			TRACE("[File-Loop] Count = %d\n", m_nLoopCount);
			// ���Ҫѭ�������¼�������λ������ִ�в���...
			m_bVideoComplete = ((m_tidVideo != MP4_INVALID_TRACK_ID) ? false : true);	// ����Ƶ�Ÿ�λ��־
			m_bAudioComplete = ((m_tidAudio != MP4_INVALID_TRACK_ID) ? false : true);	// ����Ƶ�Ÿ�λ��־
			dwOutVSendTime = dwOutASendTime = 0;	// ֡��ʱ����λ��0��
			m_iASampleInx = m_iVSampleInx = 1;		// ����Ƶ��������λ
			m_dwStartMS = ::GetTickCount();			// ʱ����㸴λ
			m_nLoopCount += 1;						// ѭ�������ۼ�
			nAccTime = 0;							// ����������λ
			continue;
		}
		// �����Ƶ��Ч��ʹ�û�ȡ����Ƶʱ����Ƚ�...
		if( m_tidVideo != MP4_INVALID_TRACK_ID ) {
			dwCompTime = dwOutVSendTime;
		}
		// �����Ƶ��Ч��ʹ�û�ȡ����Ƶʱ����Ƚ�...
		if( m_tidAudio != MP4_INVALID_TRACK_ID ) {
			dwCompTime = dwOutASendTime;
		}
		// �������Ƶ���У���ȡ�Ƚ�С��ֵ�Ƚ�...
		if((m_tidVideo != MP4_INVALID_TRACK_ID) && (m_tidAudio != MP4_INVALID_TRACK_ID)) {
			dwCompTime = min(dwOutASendTime, dwOutVSendTime);
		}
		// �����ǰʱ��Ƚ�С���ȴ�һ���ٶ�ȡ...
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

	uint8_t		*	pSampleData = NULL;		// ֡����ָ��
	uint32_t		nSampleSize = 0;		// ֡���ݳ���
	MP4Timestamp	nStartTime = 0;			// ��ʼʱ��
	MP4Duration		nDuration = 0;			// ����ʱ��
	MP4Duration     nOffset = 0;			// ƫ��ʱ��
	bool			bIsKeyFrame = false;	// �Ƿ�ؼ�֡
	uint32_t		timescale = 0;
	uint64_t		msectime = 0;

	timescale = MP4GetTrackTimeScale( m_hMP4Handle, tid );
	msectime = MP4GetSampleTime( m_hMP4Handle, tid, sid );

	if( false == MP4ReadSample(m_hMP4Handle, tid, sid, &pSampleData, &nSampleSize, &nStartTime, &nDuration, &nOffset, &bIsKeyFrame) )
		return false;

	// ���㷢��ʱ�� => PTS
	msectime *= UINT64_C( 1000 );
	msectime /= timescale;
	// ���㿪ʼʱ�� => DTS
	nStartTime *= UINT64_C( 1000 );
	nStartTime /= timescale;
	// ����ƫ��ʱ�� => CTTS
	nOffset *= UINT64_C( 1000 );
	nOffset /= timescale;

	FMS_FRAME	theFrame;
	theFrame.typeFlvTag = (bIsVideo ? FLV_TAG_TYPE_VIDEO : FLV_TAG_TYPE_AUDIO);	// ��������Ƶ��־
	theFrame.dwSendTime = (uint32_t)msectime + m_dwMP4Duration * m_nLoopCount;	// ����ǳ���Ҫ��ǣ�浽ѭ������
	theFrame.dwRenderOffset = nOffset;  // 2017.07.06 - by jackey => ����ʱ��ƫ��ֵ...
	theFrame.is_keyframe = bIsKeyFrame;
	theFrame.strData.assign((char*)pSampleData, nSampleSize);

	// ������Ҫ�ͷŶ�ȡ�Ļ�����...
	MP4Free(pSampleData);
	pSampleData = NULL;

	// ������֡��������Ͷ��е���...
	ASSERT( m_lpPushThread != NULL );
	m_lpPushThread->PushFrame(theFrame);

	// ���ط���ʱ��(����)...
	outSendTime = (uint32_t)msectime;
	
	//TRACE("[%s] duration = %I64d, offset = %I64d, KeyFrame = %d, SendTime = %lu, StartTime = %I64d, Size = %lu\n", bIsVideo ? "Video" : "Audio", nDuration, nOffset, bIsKeyFrame, outSendTime, nStartTime, nSampleSize);

	return true;
}

BOOL CMP4Thread::InitMP4(CPushThread * inPushThread, string & strMP4File)
{
	// Ŀǰ��ģʽ���ļ�һ����ѭ��ģʽ...
	m_lpPushThread = inPushThread;
	m_strMP4File = strMP4File;
	m_bFileLoop = true;

	this->Start();

	return true;
}

void myMP4LogCallback( MP4LogLevel loglevel, const char* fmt, va_list ap)
{
	// ��ϴ��ݹ����ĸ�ʽ...
	CString	strDebug;
	strDebug.FormatV(fmt, ap);
	if( (strDebug.ReverseFind('\r') < 0) && (strDebug.ReverseFind('\n') < 0) ) {
		strDebug.Append("\r\n");
	}
	// ���и�ʽת��������ӡ����...
	string strANSI = CUtilTool::UTF8_ANSI(strDebug);
	TRACE(strANSI.c_str());
}

bool CMP4Thread::doPrepareMP4()
{
	// ����MP4���Լ��� => �������ϸ����...
	//MP4LogLevel theLevel = MP4LogGetLevel();
	//MP4LogSetLevel(MP4_LOG_VERBOSE4);
	//MP4SetLogCallback(myMP4LogCallback);
	// �ļ���ʧ�ܣ�ֱ�ӷ���...
	string strUTF8 = CUtilTool::ANSI_UTF8(m_strMP4File.c_str());
	m_hMP4Handle = MP4Read( strUTF8.c_str() );
	if( m_hMP4Handle == MP4_INVALID_FILE_HANDLE ) {
		MP4Close(m_hMP4Handle);
		m_hMP4Handle = MP4_INVALID_FILE_HANDLE;
		return false;
	}
	// �ļ�����ȷ, ��ȡ��Ҫ������...
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
	
	// ���Ȼ�ȡ�ļ�����ʱ�䳤��(����)...
	m_dwMP4Duration = (uint32_t)MP4GetDuration(inFile);
	ASSERT( m_dwMP4Duration > 0 );

	// ��ȡ��Ҫ�������Ϣ...
    uint32_t trackCount = MP4GetNumberOfTracks( inFile );
    for( uint32_t i = 0; i < trackCount; ++i ) {
		MP4TrackId  id = MP4FindTrackId( inFile, i );
		const char* type = MP4GetTrackType( inFile, id );
		if( MP4_IS_VIDEO_TRACK_TYPE( type ) ) {
			// ��Ƶ�Ѿ���Ч�������һ��...
			if( m_tidVideo > 0 )
				continue;
			// ��ȡ��Ƶ��Ϣ...
			m_tidVideo = id;
			m_bVideoComplete = false;
			char * lpVideoInfo = MP4Info(inFile, id);
			free(lpVideoInfo);
			// ��ȡ��Ƶ��PPS/SPS��Ϣ...
			uint8_t  ** spsHeader = NULL;
			uint8_t  ** ppsHeader = NULL;
			uint32_t  * spsSize = NULL;
			uint32_t  * ppsSize = NULL;
			uint32_t    ix = 0;
			bool bResult = MP4GetTrackH264SeqPictHeaders(inFile, id, &spsHeader, &spsSize, &ppsHeader, &ppsSize);
			for(ix = 0; spsSize[ix] != 0; ++ix) {
				// SPSָ��ͳ���...
				uint8_t * lpSPS = spsHeader[ix];
				uint32_t  nSize = spsSize[ix];
				// ֻ�洢��һ��SPS��Ϣ...
				if( m_strSPS.size() <= 0 && nSize > 0 ) {
					m_strSPS.assign((char*)lpSPS, nSize);
				}
				free(spsHeader[ix]);
			}
			free(spsHeader);
			free(spsSize);
			for(ix = 0; ppsSize[ix] != 0; ++ix) {
				// PPSָ��ͳ���...
				uint8_t * lpPPS = ppsHeader[ix];
				uint32_t  nSize = ppsSize[ix];
				// ֻ�洢��һ��PPS��Ϣ...
				if( m_strPPS.size() <= 0 && nSize > 0 ) {
					m_strPPS.assign((char*)lpPPS, nSize);
				}
				free(ppsHeader[ix]);
			}
			free(ppsHeader);
			free(ppsSize);

			// ������Ƶ����ͷ...
			this->WriteAVCSequenceHeader();
		}
		else if( MP4_IS_AUDIO_TRACK_TYPE( type ) ) {
			// ��Ƶ�Ѿ���Ч�������һ��...
			if( m_tidAudio > 0 )
				continue;
			// ��ȡ��Ƶ��Ϣ...
			m_tidAudio = id;
			m_bAudioComplete = false;
			char * lpAudioInfo = MP4Info(inFile, id);
			free(lpAudioInfo);

			// ��ȡ��Ƶ������/����/������/������Ϣ...
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
			// ��ȡ��Ƶ��չ��Ϣ...
            uint8_t  * pAES = NULL;
            uint32_t   nSize = 0;
            bool haveEs = MP4GetTrackESConfiguration(inFile, id, &pAES, &nSize);
			// �洢��Ƶ��չ��Ϣ...
			if( m_strAES.size() <= 0 && nSize > 0 ) {
				m_strAES.assign((char*)pAES, nSize);
			}
			free(pAES);

			// ������Ƶ����ͷ...
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

	// ����AAC����ͷ��Ϣ...
	int aac_len = (int)(pbuf - aac_seq_buf);
	m_strAACHeader.assign(aac_seq_buf, aac_len);
}

void CMP4Thread::WriteAVCSequenceHeader()
{
	// ��ȡ width �� height...
	int	nPicWidth = 0;
	int	nPicHeight = 0;
	if( m_strSPS.size() >  0 ) {
		CSPSReader _spsreader;
		bs_t    s = {0};
		s.p		  = (uint8_t *)m_strSPS.c_str();
		s.p_start = (uint8_t *)m_strSPS.c_str();
		s.p_end	  = (uint8_t *)m_strSPS.c_str() + m_strSPS.size();
		s.i_left  = 8; // ����ǹ̶���,���볤��...
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

	// ����AVC����ͷ��Ϣ...
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
	// ����rtspѭ���˳���־...
	m_rtspEventLoopWatchVariable = 1;

	// ֹͣ�߳�...
	this->StopAndWaitForThread();

	TRACE("[~CRtspThread Thread] - Exit\n");
}

BOOL CRtspThread::InitRtsp(CPushThread * inPushThread, string & strRtspUrl)
{
	// ���洫�ݵĲ���...
	m_lpPushThread = inPushThread;
	m_strRtspUrl = strRtspUrl;

	// ����rtsp���ӻ���...
	m_scheduler_ = BasicTaskScheduler::createNew();
	m_env_ = BasicUsageEnvironment::createNew(*m_scheduler_);
	m_rtspClient_ = ourRTSPClient::createNew(*m_env_, m_strRtspUrl.c_str(), 1, "rtspTransfer", this, NULL);
	
	// 2017.07.21 - by jackey => ��Щ�����������ȷ�OPTIONS...
	// �����һ��rtsp���� => �ȷ��� OPTIONS ����...
	m_rtspClient_->sendOptionsCommand(continueAfterOPTIONS); 

	//����rtsp����߳�...
	this->Start();
	
	return true;
}

void CRtspThread::Entry()
{
	// ��������ѭ����⣬�޸� g_rtspEventLoopWatchVariable �����������˳�...
	ASSERT( m_env_ != NULL && m_rtspClient_ != NULL );
	m_env_->taskScheduler().doEventLoop(&m_rtspEventLoopWatchVariable);

	// �������ݽ�����־...
	m_bFinished = true;

	// 2017.06.12 - by jackey => ����������ģʽ�����ܵ��� doErrNotify����Ҫ�����Ͽ��ķ�ʽ...
	// �ڳ�ʱ�����л����ȼ��m_bFinished��־���ٶȿ�...
	// ����ֻ��Ҫ���ñ�־����ʱ���ƻ��Զ�ɾ��...
	//if( m_lpPushThread != NULL ) {
	//	m_lpPushThread->doErrNotify();
	//}

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

void CRtspThread::StartPushThread()
{
	if( m_lpPushThread == NULL )
		return;
	ASSERT( m_lpPushThread != NULL );
	m_lpPushThread->SetStreamPlaying(true);

	/*// �������ת��ģʽ��ֻ����������״̬...
	if( !m_lpPushThread->IsCameraDevice() ) {
		m_lpPushThread->SetStreamPlaying(true);
		return;
	}
	// ������һ����Ƶ����Ƶ�Ѿ�׼������...
	ASSERT( m_lpPushThread->IsCameraDevice() );
	ASSERT( m_strAACHeader.size() > 0 || m_strAVCHeader.size() > 0 );
	// ֱ������rtmp�����̣߳���ʼ����rtmp���͹���...
	m_lpPushThread->Start();*/
}

void CRtspThread::PushFrame(FMS_FRAME & inFrame)
{
	// ������֡��������Ͷ��е���...
	ASSERT( m_lpPushThread != NULL );
	m_lpPushThread->PushFrame(inFrame);
}

void CRtspThread::WriteAACSequenceHeader(int inAudioRate, int inAudioChannel)
{
	// ���Ƚ������洢���ݹ����Ĳ���...
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

	// ����AAC����ͷ��Ϣ...
	int aac_len = (int)(pbuf - aac_seq_buf);
	m_strAACHeader.assign(aac_seq_buf, aac_len);
}

void CRtspThread::WriteAVCSequenceHeader(string & inSPS, string & inPPS)
{
	// ��ȡ width �� height...
	int	nPicWidth = 0;
	int	nPicHeight = 0;
	if( inSPS.size() >  0 ) {
		CSPSReader _spsreader;
		bs_t    s = {0};
		s.p		  = (uint8_t *)inSPS.c_str();
		s.p_start = (uint8_t *)inSPS.c_str();
		s.p_end	  = (uint8_t *)inSPS.c_str() + inSPS.size();
		s.i_left  = 8; // ����ǹ̶���,���볤��...
		_spsreader.Do_Read_SPS(&s, &nPicWidth, &nPicHeight);
		m_nVideoHeight = nPicHeight;
		m_nVideoWidth = nPicWidth;
	}

	// �ȱ��� SPS �� PPS ��ʽͷ��Ϣ..
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

	// ����AVC����ͷ��Ϣ...
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
	// ֹͣ�߳�...
	this->StopAndWaitForThread();

	// ɾ��rtmp����...
	if( m_lpRtmp != NULL ) {
		delete m_lpRtmp;
		m_lpRtmp = NULL;
	}

	TRACE("[~CRtmpThread Thread] - Exit\n");
}

BOOL CRtmpThread::InitRtmp(CPushThread * inPushThread, string & strRtmpUrl)
{
	// ���洫�ݵĲ���...
	m_lpPushThread = inPushThread;
	m_strRtmpUrl = strRtmpUrl;

	// ����rtmp����ע����������...
	m_lpRtmp = new LibRtmp(false, false, this);
	ASSERT( m_lpRtmp != NULL );

	//����rtmp����߳�...
	this->Start();
	
	return true;
}

void CRtmpThread::Entry()
{
	ASSERT( m_lpRtmp != NULL );
	ASSERT( m_lpPushThread != NULL );
	ASSERT( m_strRtmpUrl.size() > 0 );

	// ����rtmp������...
	if( !m_lpRtmp->Open(m_strRtmpUrl.c_str()) ) {
		delete m_lpRtmp; m_lpRtmp = NULL;
		// 2017.06.12 - by jackey => ����������ģʽ�����ܵ��� doErrNotify����Ҫ�����Ͽ��ķ�ʽ...
		// �ڳ�ʱ�����л����ȼ��m_bFinished��־���ٶȿ�...
		// ����ֻ��Ҫ���ñ�־����ʱ���ƻ��Զ�ɾ��...
		m_bFinished = true;
		return;
	}
	// ѭ����ȡ���ݲ�ת����ȥ...
	while( !this->IsStopRequested() ) {
		if( m_lpRtmp->IsClosed() || !m_lpRtmp->Read() ) {
			// 2017.06.12 - by jackey => ����������ģʽ�����ܵ��� doErrNotify����Ҫ�����Ͽ��ķ�ʽ...
			// �ڳ�ʱ�����л����ȼ��m_bFinished��־���ٶȿ�...
			// ����ֻ��Ҫ���ñ�־����ʱ���ƻ��Զ�ɾ��...
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

	/*// �������ת��ģʽ��ֻ����������״̬...
	if( !m_lpPushThread->IsCameraDevice() ) {
		m_lpPushThread->SetStreamPlaying(true);
		return;
	}
	// ������һ����Ƶ����Ƶ�Ѿ�׼������...
	ASSERT( m_lpPushThread->IsCameraDevice() );
	ASSERT( m_strAACHeader.size() > 0 || m_strAVCHeader.size() > 0 );
	// ֱ������rtmp�����̣߳���ʼ����rtmp���͹���...
	m_lpPushThread->Start();*/
}

int CRtmpThread::PushFrame(FMS_FRAME & inFrame)
{
	// ������֡��������Ͷ��е���...
	ASSERT( m_lpPushThread != NULL );
	return m_lpPushThread->PushFrame(inFrame);
}

void CRtmpThread::WriteAACSequenceHeader(int inAudioRate, int inAudioChannel)
{
	// ���Ƚ������洢���ݹ����Ĳ���...
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

	// ����AAC����ͷ��Ϣ...
	int aac_len = (int)(pbuf - aac_seq_buf);
	m_strAACHeader.assign(aac_seq_buf, aac_len);
}

void CRtmpThread::WriteAVCSequenceHeader(string & inSPS, string & inPPS)
{
	// ��ȡ width �� height...
	int	nPicWidth = 0;
	int	nPicHeight = 0;
	if( inSPS.size() >  0 ) {
		CSPSReader _spsreader;
		bs_t    s = {0};
		s.p		  = (uint8_t *)inSPS.c_str();
		s.p_start = (uint8_t *)inSPS.c_str();
		s.p_end	  = (uint8_t *)inSPS.c_str() + inSPS.size();
		s.i_left  = 8; // ����ǹ̶���,���볤��...
		_spsreader.Do_Read_SPS(&s, &nPicWidth, &nPicHeight);
		m_nVideoHeight = nPicHeight;
		m_nVideoWidth = nPicWidth;
	}

	// �ȱ��� SPS �� PPS ��ʽͷ��Ϣ..
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

	// ����AVC����ͷ��Ϣ...
	int avc_len = (int)(pbuf - avc_seq_buf);
	m_strAVCHeader.assign(avc_seq_buf, avc_len);
}

CPushThread::CPushThread(HWND hWndVideo, CCamera * lpCamera)
  : m_hWndVideo(hWndVideo)
  , m_lpCamera(lpCamera)
{
	ASSERT( m_lpCamera != NULL );
	ASSERT( m_hWndVideo != NULL );
	m_bIsPublishing = false;
	m_bStreamPlaying = false;
	
	m_nKeyFrame = 0;
	m_nKeyMonitor = 0;
	m_dwFirstSendTime = 0;

	m_lpRtmpPush = NULL;
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
	m_dwRecCTime = 0;
	m_dwWriteSize = 0;
	m_dwWriteRecMS = 0;
}

CPushThread::~CPushThread()
{
	// ֹͣ�߳�...
	this->StopAndWaitForThread();

	// ɾ��rtmp�����������ӻ��⣬����Connect����ʱrtmp�Ѿ���ɾ��������ڴ����...
	{
		OSMutexLocker theLock(&m_Mutex);
		if( m_lpRtmpPush != NULL ) {
			delete m_lpRtmpPush;
			m_lpRtmpPush = NULL;
		}
	}
	// ɾ��rtmp�߳�...
	if( m_lpRtmpThread != NULL ) {
		delete m_lpRtmpThread;
		m_lpRtmpThread = NULL;
	}
	// ɾ��rtsp�߳�...
	if( m_lpRtspThread != NULL ) {
		delete m_lpRtspThread;
		m_lpRtspThread = NULL;
	}
	// ɾ��MP4�߳�...
	if( m_lpMP4Thread != NULL ) {
		delete m_lpMP4Thread;
		m_lpMP4Thread = NULL;
	}
	// �ж�¼��...
	this->StreamEndRecord();

	// ��λ��ر�־...
	m_nKeyFrame = 0;
	m_bIsPublishing = false;
	m_bStreamPlaying = false;
	TRACE("[~CPushThread Thread] - Exit\n");
}
//
// ��������ͷ�豸���̳߳�ʼ��...
/*BOOL CPushThread::DeviceInitThread(CString & strRtspUrl, string & strRtmpUrl)
{
	if( m_lpCamera == NULL )
		return false;
	ASSERT( m_lpCamera != NULL );
	// ���洫�ݹ����Ĳ���...
	m_strRtmpUrl = strRtmpUrl;
	m_nStreamProp = m_lpCamera->GetStreamProp();
	if( m_nStreamProp != kStreamDevice )
		return false;
	ASSERT( m_nStreamProp == kStreamDevice );
	// ����rtsp���������߳�...
	string strNewRtsp = strRtspUrl.GetString();
	m_lpRtspThread = new CRtspThread();
	m_lpRtspThread->InitRtsp(this, strNewRtsp);

	// ����rtmp�ϴ�����...
	m_lpRtmpPush = new LibRtmp(false, true, NULL, NULL);
	ASSERT( m_lpRtmpPush != NULL );

	// ��ʼ���������ݳ�ʱʱ���¼��� => ���� 3 ���������ݽ��գ����ж�Ϊ��ʱ...
	m_dwTimeOutMS = ::GetTickCount();

	// ���ﲻ�������̣߳��ȴ�����׼���׵�֮���������...
	// ���� rtsp �����߳��������豸�������߳�...

	return true;
}*/
//
// ������ת���̵߳ĳ�ʼ��...
BOOL CPushThread::StreamInitThread(BOOL bFileMode, string & strStreamUrl, string & strStreamMP4)
{
	// ���洫�ݹ����Ĳ���...
	if( m_lpCamera == NULL )
		return false;
	ASSERT( m_lpCamera != NULL );
	// ����MP4�̻߳�rtsp��rtmp�߳�...
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
	// ��ʼ����������ʱ����� => ÿ��1���Ӹ�λ...
	m_dwRecvTimeMS = ::GetTickCount();
	// ��ʼ���������ݳ�ʱʱ���¼��� => ���� 3 ���������ݽ��գ����ж�Ϊ��ʱ...
	m_dwTimeOutMS = ::GetTickCount();
	return true;
}
//
// ������ת��ֱ���ϴ�...
BOOL CPushThread::StreamStartLivePush(string & strRtmpUrl)
{
	if( m_lpCamera == NULL )
		return false;
	ASSERT( m_lpCamera != NULL );
	// IE8�������������鲥�������Ҫȷ��ֻ����һ��...
	if( m_lpRtmpPush != NULL )
		return true;
	// ������Ҫ��������Ϣ...
	m_strRtmpUrl = strRtmpUrl;
	ASSERT( strRtmpUrl.size() > 0 );
	// ����rtmp�ϴ�����...
	m_lpRtmpPush = new LibRtmp(false, true, NULL);
	ASSERT( m_lpRtmpPush != NULL );
	// �������������߳�...
	this->Start();
	return true;
}
//
// ֹͣ��ת��ֱ���ϴ�...
BOOL CPushThread::StreamStopLivePush()
{
	if( m_lpCamera == NULL )
		return false;
	ASSERT( m_lpCamera != NULL );
	// ֹͣ�ϴ��̣߳���λ����״̬��־...
	this->StopAndWaitForThread();
	this->SetStreamPublish(false);
	this->EndSendPacket();
	// ��λ������Ϣ...
	m_dwFirstSendTime = 0;
	m_nCurSendByte = 0;
	m_nSendKbps = 0;
	return true;
}

BOOL CPushThread::MP4CreateVideoTrack()
{
	// �ж�MP4�����Ƿ���Ч...
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
	// �жϻ�ȡ���ݵ���Ч�� => û����Ƶ��ֱ�ӷ���...
	if( nWidth <= 0 || nHeight <= 0 || strPPS.size() <= 0 || strSPS.size() <= 0 )
		return false;
	// ������Ƶ���...
	ASSERT( !m_lpRecMP4->IsVideoCreated() );
	return m_lpRecMP4->CreateVideoTrack(m_strUTF8MP4.c_str(), VIDEO_TIME_SCALE, nWidth, nHeight, strSPS, strPPS);
}

BOOL CPushThread::MP4CreateAudioTrack()
{
	// �ж�MP4�����Ƿ���Ч...
	if( m_lpRecMP4 == NULL || m_strUTF8MP4.size() <= 0 )
		return false;
	ASSERT( m_lpRecMP4 != NULL && m_strUTF8MP4.size() > 0 );

	string strAAC;
	int audio_type = 2;
	int	audio_rate_index = 0;			// �����ʱ��
	int	audio_channel_num = 0;			// ������Ŀ
	int	audio_sample_rate = 48000;		// ��Ƶ������

	if( m_lpRtspThread != NULL ) {
		strAAC = m_lpRtspThread->GetAACHeader();
		audio_rate_index = m_lpRtspThread->GetAudioRateIndex();
		audio_channel_num = m_lpRtspThread->GetAudioChannelNum();
	} else if( m_lpRtmpThread != NULL ) {
		strAAC = m_lpRtmpThread->GetAACHeader();
		audio_rate_index = m_lpRtmpThread->GetAudioRateIndex();
		audio_channel_num = m_lpRtmpThread->GetAudioChannelNum();
	} else if( m_lpMP4Thread != NULL ) {
		strAAC = m_lpMP4Thread->GetAACHeader();
		audio_type = m_lpMP4Thread->GetAudioType();
		audio_rate_index = m_lpMP4Thread->GetAudioRateIndex();
		audio_channel_num = m_lpMP4Thread->GetAudioChannelNum();
	}
	// û����Ƶ��ֱ�ӷ��� => 2017.07.25 - by jackey...
	if( strAAC.size() <= 0 )
		return false;
  
	// ����������ȡ������...
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
	
	// ���� AES ����ͷ��Ϣ...
	string strAES;
	int aac_len = (int)(pbuf - aac_seq_buf);
	strAES.assign(aac_seq_buf, aac_len);

	// ������Ƶ���...
	ASSERT( !m_lpRecMP4->IsAudioCreated() );
	return m_lpRecMP4->CreateAudioTrack(m_strUTF8MP4.c_str(), audio_sample_rate, strAES);
}
//
// ������Ƭ¼�����...
BOOL CPushThread::BeginRecSlice()
{
	if( m_lpCamera == NULL )
		return false;
	ASSERT( m_lpCamera != NULL );
	// ��λ¼����Ϣ����...
	m_dwWriteRecMS = 0;
	m_dwWriteSize = 0;
	// ��ȡΨһ���ļ���...
	MD5	    md5;
	string  strUniqid;
	CString strTimeMicro;
	ULARGE_INTEGER	llTimCountCur = {0};
	int nCourseID = m_lpCamera->GetRecCourseID();
	int nDBCameraID = m_lpCamera->GetDBCameraID();
	::GetSystemTimeAsFileTime((FILETIME *)&llTimCountCur);
	strTimeMicro.Format("%I64d", llTimCountCur.QuadPart);
	md5.update(strTimeMicro, strTimeMicro.GetLength());
	strUniqid = md5.toString();
	// ׼��¼����Ҫ����Ϣ...
	GM_MapData theMapWeb;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetCamera(nDBCameraID, theMapWeb);
	CString  strMP4Path;
	string & strSavePath = theConfig.GetSavePath();
	string & strDBCameraID = theMapWeb["camera_id"];
	// ׼��JPG��ͼ�ļ� => PATH + Uniqid + DBCameraID + .jpg
	m_strJpgName.Format("%s\\%s_%s.jpg", strSavePath.c_str(), strUniqid.c_str(), strDBCameraID.c_str());
	// 2017.08.10 - by jackey => ��������ʱ����ֶ�...
	// ׼��MP4¼������ => PATH + Uniqid + DBCameraID + CreateTime + CourseID
	m_dwRecCTime = (DWORD)::time(NULL);
	m_strMP4Name.Format("%s\\%s_%s_%lu_%d", strSavePath.c_str(), strUniqid.c_str(), strDBCameraID.c_str(), m_dwRecCTime, nCourseID);
	// ¼��ʱʹ��.tmp������û��¼����ͱ��ϴ�...
	strMP4Path.Format("%s.tmp", m_strMP4Name);
	m_strUTF8MP4 = CUtilTool::ANSI_UTF8(strMP4Path);
	// ������Ƶ���...
	this->MP4CreateVideoTrack();
	// ������Ƶ���...
	this->MP4CreateAudioTrack();
	return true;
}
//
// ֹͣ¼����Ƭ����...
BOOL CPushThread::EndRecSlice()
{
	// �ر�¼��֮ǰ����ȡ��¼��ʱ��ͳ���...
	if( m_lpRecMP4 != NULL ) {
		m_lpRecMP4->Close();
	}
	// ����¼���Ľ�ͼ�����ļ�������...
	if( m_dwWriteSize > 0 && m_dwWriteRecMS > 0 ) {
		this->doStreamSnapJPG(m_dwWriteRecMS/1000);
	}
	// ������Ҫ��λ¼�Ʊ������������˳�ʱ����...
	m_dwWriteRecMS = 0; m_dwWriteSize = 0;
	return true;
}
//
// ����¼���Ľ�ͼ�¼�...
void CPushThread::doStreamSnapJPG(int nRecSecond)
{
	// ����¼���ļ�����չ��...
	CString strMP4Temp, strMP4File;
	strMP4Temp.Format("%s.tmp", m_strMP4Name);
	strMP4File.Format("%s_%d.mp4", m_strMP4Name, nRecSecond);
	// ����ļ������ڣ�ֱ�ӷ���...
	if( _access(strMP4Temp, 0) < 0 )
		return;
	// ���ýӿڽ��н�ͼ��������ͼʧ�ܣ������¼...
	ASSERT( m_strJpgName.GetLength() > 0 );
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	if( !theConfig.StreamSnapJpeg(strMP4Temp, m_strJpgName, nRecSecond) ) {
		MsgLogGM(GM_Snap_Jpg_Err);
	}
	// ֱ�Ӷ��ļ����и��Ĳ���������¼ʧ�ܵ���־...
	if( !MoveFile(strMP4Temp, strMP4File) ) {
		MsgLogGM(::GetLastError());
	}
}
//
// ��ʼ��ת��ģʽ��¼��...
BOOL CPushThread::StreamBeginRecord()
{
	// ������Ҫ���л��Ᵽ��...
	OSMutexLocker theLock(&m_Mutex);
	// �ж�״̬�Ƿ���Ч...
	if( !this->IsStreamPlaying() )
		return false;
	ASSERT( this->IsStreamPlaying() );
	// �ͷ�¼�����...
	if( m_lpRecMP4 != NULL ) {
		delete m_lpRecMP4;
		m_lpRecMP4 = NULL;
	}
	// �����µ�¼�����...
	m_lpRecMP4 = new LibMP4();
	ASSERT( m_lpRecMP4 != NULL );
	// ������һ����Ƭ¼��...
	return this->BeginRecSlice();
}
//
// ����¼���� => ���ж��Ƿ����µ���Ƭ...
BOOL CPushThread::StreamWriteRecord(FMS_FRAME & inFrame)
{
	// ¼�������Ч������д��...
	if( m_lpRecMP4 == NULL )
		return false;
	ASSERT( m_lpRecMP4 != NULL );
	// ����д�̲������ڲ����ж��Ƿ��ܹ�д��...
	BOOL bIsVideo = ((inFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO) ? true : false);
	if( !m_lpRecMP4->WriteSample(bIsVideo, (BYTE*)inFrame.strData.c_str(), inFrame.strData.size(), inFrame.dwSendTime, inFrame.dwRenderOffset, inFrame.is_keyframe) )
		return false;
	// ������Ҫ��¼��¼���ļ���С����¼�ƺ�����...
	m_dwWriteSize = m_lpRecMP4->GetWriteSize();
	m_dwWriteRecMS = m_lpRecMP4->GetWriteRecMS();
	// ���û����Ƶ������������...
	if( !m_lpRecMP4->IsVideoCreated() )
		return true;
	// ����ǲ����Ƽ��ģʽ����������Ƭ����...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	if( theConfig.GetWebType() != kCloudMonitor )
		return true;
	ASSERT( theConfig.GetWebType() == kCloudMonitor );
	// ��¼����Ƭ�ɷ���ת��������...
	int nSliceSec  = theConfig.GetSliceVal() * 60;
	int nInterKey = theConfig.GetInterVal();
	// �����Ƭʱ��Ϊ0����ʾ��������Ƭ...
	if( nSliceSec <= 0 )
		return true;
	ASSERT( nSliceSec > 0 );
	do {
		// �����Ƭ���� <= 0����λ��Ƭ������ر���...
		if( nInterKey <= 0 ) {
			m_MapMonitor.clear();
			m_nKeyMonitor = 0;
			break;
		}
		// �����Ƭ���� > 0��������Ƭ����...
		ASSERT( nInterKey > 0 );
		// ������ת�浽ר�ŵĻ�����е��� => ��һ֡�϶��ǹؼ�֡����һ�����ݲ��ǹؼ�֡��ֱ�Ӷ���...
		if((m_MapMonitor.size() <= 0) && (inFrame.typeFlvTag != FLV_TAG_TYPE_VIDEO || !inFrame.is_keyframe))
			break;
		// ��д�̳ɹ�������֡�����������Ա㽻��ʱʹ��...
		m_MapMonitor.insert(pair<uint32_t, FMS_FRAME>(inFrame.dwSendTime, inFrame));
		// �������Ƶ�ؼ�֡���ؼ�֡�������ۼӣ������趨ֵ������ǰ��Ĺؼ�֡�ͷǹؼ�֡���� => ����Ƶһ����...
		if( inFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO && inFrame.is_keyframe ) {
			if( ++m_nKeyMonitor > nInterKey ) {
				this->dropSliceKeyFrame();
			}
		}
		// ������ϣ�����ѭ�������¼����Ƭʱ���Ƿ񵽴�...
	}while( false );
	// û�е���¼����Ƭʱ�䣬ֱ�ӷ��� => ��λ����...
	// ��Ƭʱ����¼���ʱʱ�䣬��������д���ļ���ʱ��...
	DWORD dwElapseSec = (DWORD)::time(NULL) - m_dwRecCTime;
	if( dwElapseSec < nSliceSec )
		return true;
	ASSERT( dwElapseSec >= nSliceSec );
	// ������Ƭʱ�䣬ֹͣ¼����Ƭ...
	this->EndRecSlice();
	// �����µ���Ƭ¼��...
	this->BeginRecSlice();
	// ��Ҫ��Ƭ���� => �����ѻ���Ľ���ؼ�֡����...
	if( nInterKey > 0 ) {
		this->doSaveInterFrame();
	}
	return true;
}
//
// ���潻���浱�е����� => �Ѿ�����ʱ�����к���...
void CPushThread::doSaveInterFrame()
{
	TRACE("== [doSaveInterFrame] nKeyMonitor = %d, MonitorSize = %d ==\n", m_nKeyMonitor, m_MapMonitor.size());
	KH_MapFrame::iterator itorItem = m_MapMonitor.begin();
	while( itorItem != m_MapMonitor.end() ) {
		int nSize = m_MapMonitor.count(itorItem->first);
		// ����ͬʱ���������֡����ѭ������ ...
		for(int i = 0; i < nSize; ++i) {
			FMS_FRAME & theFrame = itorItem->second;
			BOOL bIsVideo = ((theFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO) ? true : false);
			m_lpRecMP4->WriteSample(bIsVideo, (BYTE*)theFrame.strData.c_str(), theFrame.strData.size(), theFrame.dwSendTime, theFrame.dwRenderOffset, theFrame.is_keyframe);
			// �ۼ����ӣ�������һ���ڵ�...
			++itorItem;
		}
	}
}
//
// ֹͣ��ת��ģʽ��¼�� => ɾ��¼�����ֹͣ��Ƭ...
BOOL CPushThread::StreamEndRecord()
{
	// ������Ҫ���л��Ᵽ��...
	OSMutexLocker theLock(&m_Mutex);
	// ��ֹͣ¼����Ƭ����...
	this->EndRecSlice();
	// ��ɾ��¼�����...
	if( m_lpRecMP4 != NULL ) {
		delete m_lpRecMP4;
		m_lpRecMP4 = NULL;
	}
	// �����ս�������...
	m_MapMonitor.clear();
	m_nKeyMonitor = 0;
	return true;
}
//
// �����Ƿ�����¼���־...
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
	// ע�⣺������Ҫ֪ͨ��վ��ͨ������������...
	// ע�⣺������ר�Ŵ�����ת��ģʽ������֪ͨ...
	m_bStreamPlaying = bFlag;
	if( m_lpCamera != NULL ) {
		m_lpCamera->doStreamStatus(bFlag ? "������" : "δ����");
		m_lpCamera->doWebStatCamera(kCameraRun);
	}
}

void CPushThread::SetStreamPublish(BOOL bFlag)
{
	m_bIsPublishing = bFlag;
	if( m_lpCamera == NULL )
		return;
	if( m_bIsPublishing ) {
		m_lpCamera->doStreamStatus("ֱ����");
	} else {
		m_lpCamera->doStreamStatus(m_bStreamPlaying ? "������" : "δ����");
	}
}
//
// ����ֻ��Ҫ��������ʧ�ܵ����������ʧ�ܻ�ȫ��ͨ����ʱ�����Զ�����...
void CPushThread::doErrPushNotify()
{
	// ����߳��Ѿ�������ɾ���ˣ�ֱ���˳�...
	if( m_bDeleteFlag )
		return;
	ASSERT( !m_bDeleteFlag );
	// ���ڶ��������Ч...
	if( m_hWndVideo == NULL )
		return;
	ASSERT( m_hWndVideo != NULL );
	// ����ͷ�豸ģʽ => ֹֻͣrtmp�ϴ�����...
	// ������ת��ģʽ => ֹֻͣrtmp�ϴ�����...
	//WPARAM wMsgID = ((m_nStreamProp == kStreamDevice) ? WM_ERR_PUSH_MSG : WM_STOP_STREAM_MSG);
	WPARAM wMsgID = WM_STOP_LIVE_PUSH_MSG;
	::PostMessage(m_hWndVideo, wMsgID, NULL, NULL);
}

void CPushThread::Entry()
{
	// ����rtmp server��ʧ�ܣ�֪ͨ�ϲ�ɾ��֮...
	if( !this->OpenRtmpUrl() ) {
		TRACE("[CPushThread::OpenRtmpUrl] - Error\n");
		this->doErrPushNotify();
		return;
	}
	// ���ֳɹ�������metadata���ݰ���ʧ�ܣ�֪ͨ�ϲ�ɾ��֮...
	if( !this->SendMetadataPacket() ) {
		TRACE("[CPushThread::SendMetadataPacket] - Error\n");
		this->doErrPushNotify();
		return;
	}
	// ������Ƶ����ͷ���ݰ���ʧ�ܣ�֪ͨ�ϲ�ɾ��֮...
	if( !this->SendAVCSequenceHeaderPacket() ) {
		TRACE("[CPushThread::SendAVCSequenceHeaderPacket] - Error\n");
		this->doErrPushNotify();
		return;
	}
	// ������Ƶ����ͷ���ݰ���ʧ�ܣ�֪ͨ�ϲ�ɾ��֮...
	if( !this->SendAACSequenceHeaderPacket() ) {
		TRACE("[CPushThread::SendAACSequenceHeaderPacket] - Error\n");
		this->doErrPushNotify();
		return;
	}
	// ���淢���ɹ���־...
	this->SetStreamPublish(true);
	// ������Ҫ�ĵ�һ֡ʱ���...
	this->BeginSendPacket();
	// 2017.06.15 - by jackey => ȡ����������ʱ��������...
	// �ϴ�׼����ϣ�֪ͨ RemoteSession ���Է�����Ϣ��ֱ����������...
	//if( m_lpCamera != NULL ) {
	//	m_lpCamera->doDelayTransmit(GM_NoErr);
	//}
	// ��ʼ�߳�ѭ��...
	int nRetValue = 0;
	uint32_t dwStartTimeMS = ::GetTickCount();
	while( !this->IsStopRequested() ) {
		// ���ʱ����������1000���룬�����һ�η�������...
		if( (::GetTickCount() - dwStartTimeMS) >= 1000 ) {
			dwStartTimeMS = ::GetTickCount();
			m_nSendKbps = m_nCurSendByte * 8 / 1024;
			m_nCurSendByte = 0;
		}
		nRetValue = this->SendOneDataPacket();
		// < 0 ֱ�����ϲ㷴��ɾ��֮...
		if( nRetValue < 0 ) {
			TRACE("[CPushThread::SendOneDataPacket] - Error\n");
			this->doErrPushNotify();
			return;
		}
		// == 0 ���ϼ���...
		if( nRetValue == 0 ) {
			Sleep(5);
			continue;
		}
		// > 0 ˵������,���Ϸ���...
		ASSERT( nRetValue > 0 );
	}
}

BOOL CPushThread::OpenRtmpUrl()
{
	OSMutexLocker theLock(&m_Mutex);
	if( m_lpRtmpPush == NULL || m_strRtmpUrl.size() <= 0  )
		return false;
	ASSERT( m_lpRtmpPush != NULL && m_strRtmpUrl.size() > 0);
	
	// ����rtmp server��������ֵ�Э��
	return m_lpRtmpPush->Open(m_strRtmpUrl.c_str());
}
//
// ͨ������������������֡...
int CPushThread::PushFrame(FMS_FRAME & inFrame)
{
	OSMutexLocker theLock(&m_Mutex);
	// ����ʱ��ʱ�㸴λ�����¼�ʱ...
	m_dwTimeOutMS = ::GetTickCount();
	// ����¼����...
	this->StreamWriteRecord(inFrame);
	// ���ʱ����������1000���룬�����һ�ν�������...
	DWORD dwCurTimeMS = ::GetTickCount();
	if((dwCurTimeMS - m_dwRecvTimeMS) >= 1000 ) {
		float fSecond = (dwCurTimeMS - m_dwRecvTimeMS)/1000.0f;
		m_nRecvKbps = m_nCurRecvByte * 8 / 1024 / fSecond;
		m_dwRecvTimeMS = dwCurTimeMS;
		m_nCurRecvByte = 0;
	}
	// �ۼӽ������ݰ����ֽ��������뻺�����...
	m_nCurRecvByte += inFrame.strData.size();
	m_MapFrame.insert(pair<uint32_t, FMS_FRAME>(inFrame.dwSendTime, inFrame));
	
	/*// ���������ͷ�������йؼ�֡�����Ͷ�֡����...
	if( this->IsCameraDevice() ) {
		return m_MapFrame.size();
	}
	// �������ת��ģʽ������û�д��ڷ���״̬...
	ASSERT( !this->IsCameraDevice() );*/

	// �ж��������Ƿ�����Ƶ�ؼ�֡��������Ƶ�ؼ�֡��ֱ�ӷ���...
	if( inFrame.typeFlvTag != FLV_TAG_TYPE_VIDEO || !inFrame.is_keyframe ) {
		return m_MapFrame.size();
	}
	// �������Ƶ�ؼ�֡���ۼӼ�����...
	ASSERT( inFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO && inFrame.is_keyframe );
	// �ۼӹؼ�֡������...
	++m_nKeyFrame;
	//TRACE("== [PushFrame] nKeyFrame = %d, FrameSize = %d, StreamSize = %d, FirstSendTime = %lu ==\n", m_nKeyFrame, m_MapFrame.size(), m_MapStream.size(), m_dwFirstSendTime);
	// ����Ѿ����ڷ�����ֱ�ӷ���...
	if( this->IsPublishing() ) {
		return m_MapFrame.size();
	}
	ASSERT( !this->IsPublishing() );
	// �������3���ؼ�֡��ɾ����һ���ؼ�֡����������...
	if( m_nKeyFrame >= 3 ) {
		this->dropToKeyFrame();
	}
	// ֱ�ӷ����ܵ�֡��...
	return m_MapFrame.size();
}
//
// ɾ����������е����ݣ�ֱ�������ؼ�֡Ϊֹ...
void CPushThread::dropToKeyFrame()
{
	// �����Ѿ�ɾ���ؼ�֡��־...
	BOOL bHasDelKeyFrame = false;
	KH_MapFrame::iterator itorItem = m_MapFrame.begin();
	while( itorItem != m_MapFrame.end() ) {
		int nSize = m_MapFrame.count(itorItem->first);
		// ����ͬʱ���������֡����ѭ��ɾ��...
		for(int i = 0; i < nSize; ++i) {
			FMS_FRAME & theFrame = itorItem->second;
			// �����������Ƶ�ؼ�֡...
			if( theFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO && theFrame.is_keyframe ) {
				// �Ѿ�ɾ����һ���ؼ�֡�������µĹؼ�֡�����÷���ʱ�����ֱ�ӷ���...
				if( bHasDelKeyFrame ) {
					m_dwFirstSendTime = theFrame.dwSendTime;
					//TRACE("== [dropToKeyFrame] nKeyFrame = %d, FrameSize = %d, StreamSize = %d, FirstSendTime = %lu ==\n", m_nKeyFrame, m_MapFrame.size(), m_MapStream.size(), m_dwFirstSendTime);
					return;
				}
				// ɾ������ؼ�֡�����ñ�־����ӡ��Ϣ...
				--m_nKeyFrame; bHasDelKeyFrame = true;
			}
			// ����ʹ������ɾ�������ǹؼ���...
			m_MapFrame.erase(itorItem++);
		}
	}
}
//
// ɾ������������е����ݣ�ֱ�������ؼ�֡Ϊֹ...
void CPushThread::dropSliceKeyFrame()
{
	// �����Ѿ�ɾ���ؼ�֡��־...
	BOOL bHasDelKeyFrame = false;
	KH_MapFrame::iterator itorItem = m_MapMonitor.begin();
	while( itorItem != m_MapMonitor.end() ) {
		int nSize = m_MapMonitor.count(itorItem->first);
		// ����ͬʱ���������֡����ѭ��ɾ��...
		for(int i = 0; i < nSize; ++i) {
			FMS_FRAME & theFrame = itorItem->second;
			// �����������Ƶ�ؼ�֡...
			if( theFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO && theFrame.is_keyframe ) {
				// �Ѿ�ɾ����һ���ؼ�֡�������µĹؼ�֡�����÷���ʱ�����ֱ�ӷ���...
				if( bHasDelKeyFrame ) {
					//TRACE("== [dropSliceKeyFrame] nKeyMonitor = %d, MonitorSize = %d ==\n", m_nKeyMonitor, m_MapMonitor.size());
					return;
				}
				// ɾ������ؼ�֡�����ñ�־����ӡ��Ϣ...
				--m_nKeyMonitor; bHasDelKeyFrame = true;
			}
			// ����ʹ������ɾ�������ǹؼ���...
			m_MapMonitor.erase(itorItem++);
		}
	}
}
//
// ��ʼ׼���������ݰ�...
void CPushThread::BeginSendPacket()
{
	OSMutexLocker theLock(&m_Mutex);
	ASSERT( this->IsPublishing() );
	
	/*// ֻ������ת��ģʽ�²Ž��У���ʼʱ��ĵ���...
	// ��Ϊ����ͷģʽ�£����ǵ�����ͷ���� rtsp ������֡���ѵ�����...
	if( this->IsCameraDevice() )
		return;
	ASSERT( !this->IsCameraDevice() );*/

	// ���õ�һ֡�ķ���ʱ���...
	KH_MapFrame::iterator itorItem = m_MapFrame.begin();
	if( itorItem != m_MapFrame.end() ) {
		m_dwFirstSendTime = itorItem->first;
		TRACE("== [BeginSendPacket] nKeyFrame = %d, FrameSize = %d, StreamSize = %d, FirstSendTime = %lu ==\n", m_nKeyFrame, m_MapFrame.size(), m_MapStream.size(), m_dwFirstSendTime);
	}
}
//
// ֹͣ�������ݰ�...
void CPushThread::EndSendPacket()
{
	// ɾ���ϴ�������Ҫ���Ᵽ��...
	OSMutexLocker theLock(&m_Mutex);
	if( m_lpRtmpPush != NULL ) {
		delete m_lpRtmpPush;
		m_lpRtmpPush = NULL;
	}
	
	/*// �������ת��ģʽ����Ҫ����������ݲ��뵽���淢�Ͷ���ǰ��...
	if( this->IsCameraDevice() )
		return;
	ASSERT( !this->IsCameraDevice() );*/

	// �����Ѿ��������ת������(�ⲿ�������Ѿ�����������������Ҫ�������һ���)...
	KH_MapFrame::iterator itorItem = m_MapStream.begin();
	while( itorItem != m_MapStream.end() ) {
		int nSize = m_MapStream.count(itorItem->first);
		// ����ͬʱ���������֡����ѭ��ɾ��...
		for(int i = 0; i < nSize; ++i) {
			FMS_FRAME & theFrame = itorItem->second;
			// ������Խ��������ȡ����Ϊ�ǰ���ʱ������У��������Զ�����...
			m_MapFrame.insert(pair<uint32_t, FMS_FRAME>(theFrame.dwSendTime, theFrame));
			// �ۼ����ӣ�������һ���ڵ�...
			++itorItem;
		}
	}
	// 2017.08.11 - by jackey => MapStream����ֻ������һ����Ƶ�ؼ�֡�����ݱ���ԭ���˻�����У���ˣ���Ҫ�Թؼ�֡�����ۼ�...
	// ���������������ݣ��ؼ�֡����������...
	if( m_MapStream.size() > 0 ) {
		++m_nKeyFrame;
		TRACE("== [EndSendPacket] nKeyFrame = %d, FrameSize = %d, StreamSize = %d, FirstSendTime = %lu ==\n", m_nKeyFrame, m_MapFrame.size(), m_MapStream.size(), m_dwFirstSendTime);
	}
	// ��ջ������...
	m_MapStream.clear();
}
//
// ��ȡ�������� => -1 ��ʾ��ʱ...
int CPushThread::GetRecvKbps()
{
	// ���������ʱ������ -1���ȴ�ɾ��...
	if( this->IsFrameTimeout() ) {
		MsgLogGM(GM_Err_Timeout);
		return -1;
	}
	// ���ؽ�������...
	return m_nRecvKbps;
}
//
// ������ݰ��Ƿ��Ѿ���ʱ...
// ��ת��ģʽ�� => ���õĳ�ʱ����� StreamInitThread
// ����ͷ�豸�� => ���õĳ�ʱ����� DeviceInitThread
// ���ǻ�������ȡ�������ݵ�λ�ã���ʱ����������ģʽ��Ч...
BOOL CPushThread::IsFrameTimeout()
{
	OSMutexLocker theLock(&m_Mutex);
	// �����ж������߳��Ƿ��Ѿ�������...
	if( this->IsDataFinished() )
		return true;
	// һֱû�������ݵ������ 3 ���ӣ����ж�Ϊ��ʱ...
	int nWaitMinute = 3;
	DWORD dwElapseSec = (::GetTickCount() - m_dwTimeOutMS) / 1000;
	return ((dwElapseSec >= nWaitMinute * 60) ? true : false);
}
//
// ����һ��֡���ݰ�...
int CPushThread::SendOneDataPacket()
{
	OSMutexLocker theLock(&m_Mutex);
	// �����Ѿ������ˣ�ֱ�ӷ���-1...
	if( this->IsDataFinished() )
		return -1;
	// ������ݻ�û�н���������Ҫ��һ�����棬�Ա�����Ƶ�ܹ��Զ�����Ȼ���ٷ������ݰ�...
	// ������ǰ�趨��100�����ݰ���Ϊ�˽�����ʱ������Ϊ20�����ݰ�...
	if( m_MapFrame.size() < 20 )
		return 0;
	ASSERT( !this->IsDataFinished() && m_MapFrame.size() >= 20 );

	bool is_ok = false;
	ASSERT( m_MapFrame.size() > 0 );

	// ��ȡ��һ�����ݰ�(���ܰ����������)...
	KH_MapFrame::iterator itorItem = m_MapFrame.begin();
	int nSize = m_MapFrame.count(itorItem->first);
	// ����ͬʱ���������֡����ѭ������...
	for(int i = 0; i < nSize; ++i) {
		FMS_FRAME & theFrame = itorItem->second;
		
		/*// �������ת��ģʽ������Ҫ��ʱ������е�����Ӳ��ģʽ�µ�ʱ������Ǵ�0��ʼ�ģ��������...
		if( !this->IsCameraDevice() ) {
			theFrame.dwSendTime = theFrame.dwSendTime - m_dwFirstSendTime;
		}*/

		// �ǳ������ӣ���Ҫ��ʱ����������� => Ӳ��ģʽҲͳһ������ת��ģʽ...
		theFrame.dwSendTime = theFrame.dwSendTime - m_dwFirstSendTime;

		//TRACE("[%s] SendTime = %lu, Key = %d, Size = %d, MapSize = %d\n", ((theFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO) ? "Video" : "Audio"), theFrame.dwSendTime, theFrame.is_keyframe, theFrame.strData.size(), m_MapFrame.size());
		// ����ȡ��������֡������...
		switch( theFrame.typeFlvTag )
		{
		case FLV_TAG_TYPE_AUDIO: is_ok = this->SendAudioDataPacket(theFrame); break;
		case FLV_TAG_TYPE_VIDEO: is_ok = this->SendVideoDataPacket(theFrame); break;
		}
		// �ۼӷ����ֽ���...
		m_nCurSendByte += theFrame.strData.size();
		// ����ؼ�֡ => �������ǰ��������� => �ؼ�֡����������...
		// ��ʱ�����ԭ��ȥ...
		theFrame.dwSendTime = theFrame.dwSendTime + m_dwFirstSendTime;
		// �����µĹؼ�֡�������ǰ�Ļ��棬���ٹؼ�֡������...
		if( theFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO && theFrame.is_keyframe ) {
			--m_nKeyFrame; m_MapStream.clear();
			//TRACE("== [SendOneDataPacket] nKeyFrame = %d, FrameSize = %d, FirstSendTime = %lu, CurSendTime = %lu ==\n", m_nKeyFrame, m_MapFrame.size(), m_dwFirstSendTime, theFrame.dwSendTime);
		}
		// ������ת�浽ר�ŵĻ�����е��� => ��һ֡�϶��ǹؼ�֡...
		m_MapStream.insert(pair<uint32_t, FMS_FRAME>(theFrame.dwSendTime, theFrame));
		// �Ӷ������Ƴ�һ����ͬʱ������ݰ� => ����ʹ�õ�������...
		m_MapFrame.erase(itorItem++);
	}
	// ����ʧ�ܣ����ش���...
	return (is_ok ? 1 : -1);
}
//
// ������Ƶ���ݰ�...
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
// ������Ƶ���ݰ�...
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
// ������Ƶ����ͷ...
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

	// û����Ƶ����...
	if( strAVC.size() <= 0 )
		return true;
	
	ASSERT( strAVC.size() > 0 );
	return m_lpRtmpPush->Send(strAVC.c_str(), strAVC.size(), FLV_TAG_TYPE_VIDEO, 0);
}

//
// ������Ƶ����ͷ...
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

	// û����Ƶ����...
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

	// ������Ƶ�������...
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
	// ������Ƶ�߶�����...
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
	// ������Ƶ֡������...
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
	// ͳһ����Ĭ�ϵ���Ƶ֡������...
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
