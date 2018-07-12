
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

	MsgLogINFO("[~CMP4Thread Thread] - Exit");
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
				MsgLogINFO("== [File-Complete] ==");
				m_bFinished = true;
				return;
			}
			// ��ӡѭ������...
			ASSERT( m_bFileLoop );
			CUtilTool::MsgLog(kTxtLogger, "== [File-Loop] Count = %d ==\r\n", m_nLoopCount);
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

	// ���㷢��ʱ�� => PTS => �̶�ʱ��ת���ɺ���...
	msectime *= UINT64_C( 1000 );
	msectime /= timescale;
	// ���㿪ʼʱ�� => DTS => �̶�ʱ��ת���ɺ���...
	nStartTime *= UINT64_C( 1000 );
	nStartTime /= timescale;
	// ����ƫ��ʱ�� => CTTS => �̶�ʱ��ת���ɺ���...
	nOffset *= UINT64_C( 1000 );
	nOffset /= timescale;

	// ע�⣺msectime | nOffset | m_dwMP4Duration ��Ҫͳһ�ɺ���ʱ��...
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

	// ���ط���ʱ��(����) => �ѽ��̶�ʱ��ת�����˺���...
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
		MsgLogGM(GM_File_Not_Open);
		m_hMP4Handle = MP4_INVALID_FILE_HANDLE;
		return false;
	}
	// �ļ�����ȷ, ��ȡ��Ҫ������...
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
	
	// ���Ȼ�ȡ�ļ���ÿ��̶������̶ܿ���(���Ǻ�����)...
	uint32_t dwFileScale = MP4GetTimeScale(inFile);
	MP4Duration theDuration = MP4GetDuration(inFile);
	// �ܺ����� = �̶ܿ���*1000/ÿ��̶��� => �ȳ˷����Խ������...
	m_dwMP4Duration = theDuration*1000/dwFileScale;

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
			// �ͷŷ���Ļ���...
			if( pAES != NULL ) {
				free(pAES);
				pAES = NULL;
			}
			// ������Ƶ����ͷ...
			this->WriteAACSequenceHeader();
		}
	}
	// �����Ƶ����Ƶ��û�У�����ʧ��...
	if((m_tidVideo == MP4_INVALID_TRACK_ID) && (m_tidAudio == MP4_INVALID_TRACK_ID)) {
		MsgLogGM(GM_File_Read_Err);
		return false;
	}
	// һ�����������سɹ�...
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
	int nPicFPS = 0;
	int	nPicWidth = 0;
	int	nPicHeight = 0;
	if( m_strSPS.size() >  0 ) {
		/*CSPSReader _spsreader;
		bs_t    s = {0};
		s.p		  = (uint8_t *)m_strSPS.c_str();
		s.p_start = (uint8_t *)m_strSPS.c_str();
		s.p_end	  = (uint8_t *)m_strSPS.c_str() + m_strSPS.size();
		s.i_left  = 8; // ����ǹ̶���,���볤��...
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

	// ����AVC����ͷ��Ϣ...
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
	// ����rtspѭ���˳���־...
	m_rtspEventLoopWatchVariable = 1;

	// ֹͣ�߳�...
	this->StopAndWaitForThread();

	MsgLogINFO("== [~CRtspThread Thread] - Exit ==");
}

BOOL CRtspThread::InitRtsp(BOOL bUsingTCP, CPushThread * inPushThread, string & strRtspUrl)
{
	// ���洫�ݵĲ���...
	m_lpPushThread = inPushThread;
	m_strRtspUrl = strRtspUrl;

	// ����rtsp���ӻ���...
	m_scheduler_ = BasicTaskScheduler::createNew();
	m_env_ = BasicUsageEnvironment::createNew(*m_scheduler_);
	m_rtspClient_ = ourRTSPClient::createNew(*m_env_, m_strRtspUrl.c_str(), 1, "rtspTransfer", bUsingTCP, this, NULL);
	
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
	// ����UDP�����߳�...
	//m_lpPushThread->StartUDPSendThread();
	// �������Ĳ���״̬...
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
	int nPicFPS = 0;
	int	nPicWidth = 0;
	int	nPicHeight = 0;
	if( inSPS.size() >  0 ) {
		/*CSPSReader _spsreader;
		bs_t    s = {0};
		s.p		  = (uint8_t *)inSPS.c_str();
		s.p_start = (uint8_t *)inSPS.c_str();
		s.p_end	  = (uint8_t *)inSPS.c_str() + inSPS.size();
		s.i_left  = 8; // ����ǹ̶���,���볤��...
		_spsreader.Do_Read_SPS(&s, &nPicWidth, &nPicHeight);*/

		h264_decode_sps((BYTE*)inSPS.c_str(), inSPS.size(), nPicWidth, nPicHeight, nPicFPS);

		m_nVideoHeight = nPicHeight;
		m_nVideoWidth = nPicWidth;
		m_nVideoFPS = nPicFPS;
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
	// ֹͣ�߳�...
	this->StopAndWaitForThread();

	// ɾ��rtmp����...
	if( m_lpRtmp != NULL ) {
		delete m_lpRtmp;
		m_lpRtmp = NULL;
	}

	MsgLogINFO("== [~CRtmpThread Thread] - Exit ==");
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
	int nPicFPS = 0;
	int	nPicWidth = 0;
	int	nPicHeight = 0;
	if( inSPS.size() >  0 ) {
		/*CSPSReader _spsreader;
		bs_t    s = {0};
		s.p		  = (uint8_t *)inSPS.c_str();
		s.p_start = (uint8_t *)inSPS.c_str();
		s.p_end	  = (uint8_t *)inSPS.c_str() + inSPS.size();
		s.i_left  = 8; // ����ǹ̶���,���볤��...
		_spsreader.Do_Read_SPS(&s, &nPicWidth, &nPicHeight);*/

		h264_decode_sps((BYTE*)inSPS.c_str(), inSPS.size(), nPicWidth, nPicHeight, nPicFPS);

		m_nVideoHeight = nPicHeight;
		m_nVideoWidth = nPicWidth;
		m_nVideoFPS = nPicFPS;
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