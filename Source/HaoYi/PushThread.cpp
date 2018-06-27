
#include "stdafx.h"
#include "PullThread.h"
#include "PushThread.h"
#include "XmlConfig.h"
#include "srs_librtmp.h"
#include "BitWritter.h"
#include "UtilTool.h"
#include "VideoWnd.h"
#include "LibRtmp.h"
#include "Camera.h"
#include "LibMP4.h"
#include "ReadSPS.h"
#include "md5.h"

#include "UDPSendThread.h"
#include "UDPRecvThread.h"
#include "UDPPlayThread.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CPushThread::CPushThread(HWND hWndVideo, CCamera * lpCamera)
  : m_hWndVideo(hWndVideo)
  , m_lpCamera(lpCamera)
  , m_nSliceInx(0)
{
	m_lpPlaySDL = NULL;
	m_lpUDPSendThread = NULL;
	m_lpUDPRecvThread = NULL;

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
	m_dwSnapTimeMS = 0;

	m_lpRecMP4 = NULL;
	m_dwRecCTime = 0;
	m_dwWriteSize = 0;
	m_dwWriteSec = 0;

#ifdef _SAVE_H264_
	m_bSave_sps = true;
	m_lpH264File = fopen("c:\\test.h264", "wb+");
#endif
}

CPushThread::~CPushThread()
{
	// ֹͣ�߳�...
	this->StopAndWaitForThread();

	// ɾ��UDP���ݽ����߳�...
	if( m_lpUDPRecvThread != NULL ) {
		delete m_lpUDPRecvThread;
		m_lpUDPRecvThread = NULL;
	}
	// ɾ��UDP���ݷ����߳�...
	{
		OSMutexLocker theLock(&m_Mutex);
		if( m_lpUDPSendThread != NULL ) {
			delete m_lpUDPSendThread;
			m_lpUDPSendThread = NULL;
		}
	}

	if( m_lpPlaySDL != NULL ) {
		delete m_lpPlaySDL;
		m_lpPlaySDL = NULL;
	}

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
	MsgLogINFO("== [~CPushThread Thread] - Exit ==");

	// �ͷŴ����ļ�...
#ifdef _SAVE_H264_
	fclose(m_lpH264File);
	m_lpH264File = NULL;
#endif
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
BOOL CPushThread::StreamInitThread(BOOL bFileMode, BOOL bUsingTCP, string & strStreamUrl, string & strStreamMP4)
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
			m_lpRtspThread->InitRtsp(bUsingTCP, this, strStreamUrl);
		} else {
			m_lpRtmpThread = new CRtmpThread();
			m_lpRtmpThread->InitRtmp(this, strStreamUrl);
		}
	}
	// ��¼��ǰʱ�䣬��λ�����룩...
	DWORD dwInitTimeMS = ::GetTickCount();
	// ��ʼ����������ʱ����� => ÿ��1���Ӹ�λ...
	m_dwRecvTimeMS = dwInitTimeMS;
	// ��ʼ���������ݳ�ʱʱ���¼��� => ���� 3 ���������ݽ��գ����ж�Ϊ��ʱ...
	m_dwTimeOutMS = dwInitTimeMS;
	// ��¼ͨ����ͼ���ʱ�䣬��λ�����룩...
	m_dwSnapTimeMS = dwInitTimeMS;
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
	m_dwWriteSec = 0;
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
	CString  strMP4Path;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	string & strSavePath = theConfig.GetSavePath();
	// ׼��JPG��ͼ�ļ� => PATH + Uniqid + DBCameraID + .jpg
	m_strJpgName.Format("%s\\%s_%d.jpg", strSavePath.c_str(), strUniqid.c_str(), nDBCameraID);
	// 2017.08.10 - by jackey => ��������ʱ����ֶ�...
	// 2018.02.06 - by jackey => ������Ƭ��־�ֶ�...
	// ׼��MP4¼������ => PATH + Uniqid + DBCameraID + CreateTime + CourseID + SliceID + SliceIndex
	m_dwRecCTime = (DWORD)::time(NULL);
	m_strMP4Name.Format("%s\\%s_%d_%lu_%d_%s_%d", strSavePath.c_str(), strUniqid.c_str(), 
						nDBCameraID, m_dwRecCTime, nCourseID, m_strSliceID, ++m_nSliceInx);
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
	if( m_dwWriteSize > 0 && m_dwWriteSec > 0 ) {
		this->doStreamSnapJPG(m_dwWriteSec);
	}
	// ������Ҫ��λ¼�Ʊ������������˳�ʱ����...
	m_dwWriteSec = 0; m_dwWriteSize = 0;
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
// ʹ��ffmpeg��������֡��̬��ͼ...
void CPushThread::doFFmpegSnapJPG()
{
	// �ж�ͨ�������Ƿ���Ч...
	if( m_lpCamera == NULL || m_strSnapFrame.size() <= 0 )
		return;
	ASSERT( m_lpCamera != NULL );
	ASSERT( m_strSnapFrame.size() > 0 );
	// ��ȡ·�������Ϣ...
	CString strJPGFile;
	int nDBCameraID = m_lpCamera->GetDBCameraID();
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	string & strSavePath = theConfig.GetSavePath();
	// ׼��JPG��ͼ�ļ� => PATH + live + DBCameraID + .jpg
	strJPGFile.Format("%s\\live_%d.jpg", strSavePath.c_str(), nDBCameraID);
	// ׼����ͼ��Ҫ��sps��pps...
	string strSPS, strPPS, strSnapData;
	DWORD dwTag = 0x01000000;
	if( m_lpRtspThread != NULL ) {
		strSPS = m_lpRtspThread->GetVideoSPS();
		strPPS = m_lpRtspThread->GetVideoPPS();
	} else if( m_lpRtmpThread != NULL ) {
		strSPS = m_lpRtmpThread->GetVideoSPS();
		strPPS = m_lpRtmpThread->GetVideoPPS();
	} else if( m_lpMP4Thread != NULL ) {
		strSPS = m_lpMP4Thread->GetVideoSPS();
		strPPS = m_lpMP4Thread->GetVideoPPS();
	}
	// ������д��sps����д��pps...
	strSnapData.append((char*)&dwTag, sizeof(DWORD));
	strSnapData.append(strSPS);
	strSnapData.append((char*)&dwTag, sizeof(DWORD));
	strSnapData.append(strPPS);
	// ���д��ؼ�֡����ʼ���Ѿ�����...
	strSnapData.append(m_strSnapFrame);
	// ���ý�ͼ�ӿڽ��ж�̬��ͼ...
	if( !theConfig.FFmpegSnapJpeg(strSnapData, strJPGFile) ) {
		MsgLogGM(GM_Snap_Jpg_Err);
	} else {
		CUtilTool::MsgLog(kTxtLogger, "== ffmpeg snap jpg(%d) ==\r\n", nDBCameraID);
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
	// ������Ƭ��־��Ϣ...
	ULARGE_INTEGER	llTimCountCur = {0};
	::GetSystemTimeAsFileTime((FILETIME *)&llTimCountCur);
	m_strSliceID.Format("%I64d", llTimCountCur.QuadPart);
	m_nSliceInx = 0;
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
	// ������Ҫ��¼��¼���ļ���С����¼������...
	m_dwWriteSize = m_lpRecMP4->GetWriteSize();
	m_dwWriteSec = m_lpRecMP4->GetWriteSec();
	//TRACE("Write Second: %lu\n", m_dwWriteSec);
	// ���û����Ƶ������������...
	if( !m_lpRecMP4->IsVideoCreated() )
		return true;
	// ����ǲ����Ƽ��ģʽ����������Ƭ����...
	// 2017.12.08 => �ſ����ƣ��Ƽ�ء���¼��������Ƭ...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	/*if( theConfig.GetWebType() != kCloudMonitor )
		return true;
	ASSERT( theConfig.GetWebType() == kCloudMonitor );*/
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
	CUtilTool::MsgLog(kTxtLogger,"== [doSaveInterFrame] nKeyMonitor = %d, MonitorSize = %d ==\r\n", m_nKeyMonitor, m_MapMonitor.size());
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
	//2018.01.01 - by jackey => ���ﲻ�ܼӻ��⣬�Ῠס����...
	//OSMutexLocker theLock(&m_Mutex);

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
	// ��ӡ�˳���Ϣ...
	CUtilTool::MsgLog(kTxtLogger, _T("== Camera(%d) stop push by SRS ==\r\n"), ((m_lpCamera != NULL) ? m_lpCamera->GetDBCameraID() : 0));
}

void CPushThread::Entry()
{
	// ����rtmp server��ʧ�ܣ�֪ͨ�ϲ�ɾ��֮...
	if( !this->OpenRtmpUrl() ) {
		MsgLogINFO("[CPushThread::OpenRtmpUrl] - Error");
		this->doErrPushNotify();
		return;
	}
	// ���ֳɹ�������metadata���ݰ���ʧ�ܣ�֪ͨ�ϲ�ɾ��֮...
	if( !this->SendMetadataPacket() ) {
		MsgLogINFO("[CPushThread::SendMetadataPacket] - Error");
		this->doErrPushNotify();
		return;
	}
	// ������Ƶ����ͷ���ݰ���ʧ�ܣ�֪ͨ�ϲ�ɾ��֮...
	if( !this->SendAVCSequenceHeaderPacket() ) {
		MsgLogINFO("[CPushThread::SendAVCSequenceHeaderPacket] - Error");
		this->doErrPushNotify();
		return;
	}
	// ������Ƶ����ͷ���ݰ���ʧ�ܣ�֪ͨ�ϲ�ɾ��֮...
	if( !this->SendAACSequenceHeaderPacket() ) {
		MsgLogINFO("[CPushThread::SendAACSequenceHeaderPacket] - Error");
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
			MsgLogINFO("[CPushThread::SendOneDataPacket] - Error");
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
	CUtilTool::MsgLog(kTxtLogger,"== OpenRtmpUrl => %s ==\r\n", m_strRtmpUrl.c_str());
	// ����rtmp server��������ֵ�Э��...
	return m_lpRtmpPush->Open(m_strRtmpUrl.c_str());
}
//
// ͨ������������������֡...
int CPushThread::PushFrame(FMS_FRAME & inFrame)
{
	OSMutexLocker theLock(&m_Mutex);
	// �ж��߳��Ƿ��Ѿ��˳�...
	if( this->IsStopRequested() ) {
		log_trace("[Student-Pusher] Error => Push Thread has been stoped");
		return 0;
	}
	// ������Ƶ�������뷢���߳�...
	if( m_lpUDPSendThread != NULL ) {
		m_lpUDPSendThread->PushFrame(inFrame);
	}
	// ������Ƶ�������벥���߳�...
	if( m_lpPlaySDL != NULL ) {
		m_lpPlaySDL->PushFrame(inFrame.strData, inFrame.typeFlvTag, inFrame.is_keyframe, inFrame.dwSendTime);
	}
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

	// ������µ���Ƶ����֡�ǹؼ�֡�������ѻ���ģ�����µĹؼ�֡���Ա��ͼʹ��...
	if( inFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO ) {
		// �޸���Ƶ֡����ʼ�� => 0x00000001
		string strCurFrame = inFrame.strData;
		char * lpszSrc = (char*)strCurFrame.c_str();
		memset((void*)lpszSrc, 0, sizeof(DWORD));
		lpszSrc[3] = 0x01;
		// ������¹ؼ�֡�����֮ǰ�Ļ���...
		if( inFrame.is_keyframe ) {
			m_strSnapFrame.clear();
		}
		// ���µ�����֡׷�ӵ����棬ֱ����һ���ؼ�֡����...
		m_strSnapFrame.append(strCurFrame);
	}
	// ���ʱ���������˽�ͼ����������ͼ���� => ʹ��ffmpeg��̬��ͼ...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	int nSnapSec = theConfig.GetSnapVal() * 60;
	// �����ܷ��ͼ��־��ʱ��������վ��̨�趨...
	bool bIsCanSnap = (((dwCurTimeMS - m_dwSnapTimeMS) >= nSnapSec * 1000) ? true : false);
	if( m_strSnapFrame.size() > 0 && bIsCanSnap ) {
		m_dwSnapTimeMS = dwCurTimeMS;
		this->doFFmpegSnapJPG();
	}

#ifdef _SAVE_H264_
	DWORD dwTag = 0x01000000;
	if( inFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO && m_lpH264File != NULL ) {
		if( m_bSave_sps ) {
			string strSPS, strPPS;
			if( m_lpRtspThread != NULL ) {
				strSPS = m_lpRtspThread->GetVideoSPS();
				strPPS = m_lpRtspThread->GetVideoPPS();
			} else if( m_lpRtmpThread != NULL ) {
				strSPS = m_lpRtmpThread->GetVideoSPS();
				strPPS = m_lpRtmpThread->GetVideoPPS();
			} else if( m_lpMP4Thread != NULL ) {
				strSPS = m_lpMP4Thread->GetVideoSPS();
				strPPS = m_lpMP4Thread->GetVideoPPS();
			}
			m_bSave_sps = false;
			fwrite(&dwTag, sizeof(DWORD), 1, m_lpH264File);
			fwrite(strSPS.c_str(), strSPS.size(), 1, m_lpH264File);
			fwrite(&dwTag, sizeof(DWORD), 1, m_lpH264File);
			fwrite(strPPS.c_str(), strPPS.size(), 1, m_lpH264File);
		}
		fwrite(&dwTag, sizeof(DWORD), 1, m_lpH264File);
		fwrite(inFrame.strData.c_str()+sizeof(DWORD), inFrame.strData.size()-sizeof(DWORD), 1, m_lpH264File);
	}
	/*if( inFrame.typeFlvTag == FLV_TAG_TYPE_AUDIO && m_lpH264File != NULL ) {
		if( m_bSave_sps ) {
			string strAES;
			if( m_lpMP4Thread != NULL ) {
				strAES = m_lpMP4Thread->GetAACHeader();
			}
			m_bSave_sps = false;
			fwrite(strAES.c_str(), strAES.size(), 1, m_lpH264File);
		}
		fwrite(inFrame.strData.c_str(), inFrame.strData.size(), 1, m_lpH264File);
	}*/
#endif

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
		CUtilTool::MsgLog(kTxtLogger, "== [BeginSendPacket] nKeyFrame = %d, FrameSize = %d, StreamSize = %d, FirstSendTime = %lu ==\r\n", m_nKeyFrame, m_MapFrame.size(), m_MapStream.size(), m_dwFirstSendTime);
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
		CUtilTool::MsgLog(kTxtLogger, "== [EndSendPacket] nKeyFrame = %d, FrameSize = %d, StreamSize = %d, FirstSendTime = %lu ==\r\n", m_nKeyFrame, m_MapFrame.size(), m_MapStream.size(), m_dwFirstSendTime);
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
	//2018.01.01 - by jackey => ���ﲻ�ܼӻ��⣬�Ῠס����...
	//OSMutexLocker theLock(&m_Mutex);

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
	if( this->IsDataFinished() ) {
		MsgLogINFO("== Push Data Finished ==");
		return -1;
	}
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
	//DWORD dwCurTickCount = ::GetTickCount();
	//TRACE("== Begin Send-Time: %lu ==\n", dwCurTickCount);
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
	//TRACE("== End Spend-Time: %lu ==\n", ::GetTickCount() - dwCurTickCount);
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

	TRACE("[Video] SendTime = %lu, Offset = %lu, Size = %lu\n", inFrame.dwSendTime, inFrame.dwRenderOffset, need_buf_size-5);

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
	
	//TRACE("[Audio] SendTime = %lu, Size = %lu\n", inFrame.dwSendTime, need_buf_size);

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
	MsgLogINFO("== RtmpPush => Send Video SequenceHeaderPacket ==");
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
	MsgLogINFO("== RtmpPush => Send Audio SequenceHeaderPacket ==");
    return m_lpRtmpPush->Send(strAAC.c_str(), strAAC.size(), FLV_TAG_TYPE_AUDIO, 0);
}

BOOL CPushThread::SendMetadataPacket()
{
	OSMutexLocker theLock(&m_Mutex);
	if( m_lpRtmpPush == NULL )
		return false;
	MsgLogINFO("== RtmpPush => SendMetadataPacket ==");

	char   metadata_buf[4096];
    char * pbuf = this->WriteMetadata(metadata_buf);

    return m_lpRtmpPush->Send(metadata_buf, (int)(pbuf - metadata_buf), FLV_TAG_TYPE_META, 0);
}

char * CPushThread::WriteMetadata(char * buf)
{
	BOOL bHasVideo = false;
	BOOL bHasAudio = false;
	string strAVC, strAAC;
	int nVideoWidth = 0;
	int nVideoHeight = 0;
    char * pbuf = buf;

    pbuf = UI08ToBytes(pbuf, AMF_DATA_TYPE_STRING);
    pbuf = AmfStringToBytes(pbuf, "@setDataFrame");

    pbuf = UI08ToBytes(pbuf, AMF_DATA_TYPE_STRING);
    pbuf = AmfStringToBytes(pbuf, "onMetaData");

	//pbuf = UI08ToBytes(pbuf, AMF_DATA_TYPE_MIXEDARRAY);
	//pbuf = UI32ToBytes(pbuf, 2);
    pbuf = UI08ToBytes(pbuf, AMF_DATA_TYPE_OBJECT);

	// ������Ƶ��Ⱥ͸߶�����...
	if( m_lpRtspThread != NULL ) {
		strAVC = m_lpRtspThread->GetAVCHeader();
		strAAC = m_lpRtspThread->GetAACHeader();
		nVideoWidth = m_lpRtspThread->GetVideoWidth();
		nVideoHeight = m_lpRtspThread->GetVideoHeight();
	} else if( m_lpRtmpThread != NULL ) {
		strAVC = m_lpRtmpThread->GetAVCHeader();
		strAAC = m_lpRtmpThread->GetAACHeader();
		nVideoWidth = m_lpRtmpThread->GetVideoWidth();
		nVideoHeight = m_lpRtmpThread->GetVideoHeight();
	} else if( m_lpMP4Thread != NULL ) {
		strAVC = m_lpMP4Thread->GetAVCHeader();
		strAAC = m_lpMP4Thread->GetAACHeader();
		nVideoWidth = m_lpMP4Thread->GetVideoWidth();
		nVideoHeight = m_lpMP4Thread->GetVideoHeight();
	}
	// ��ȡ�Ƿ�������Ƶ�ı�־״̬��Ϣ...
	bHasVideo = ((strAVC.size() > 0) ? true : false);
	bHasAudio = ((strAAC.size() > 0) ? true : false);
	// ���ÿ������...
	if( nVideoWidth > 0 ) {
	    pbuf = AmfStringToBytes(pbuf, "width");
		pbuf = AmfDoubleToBytes(pbuf, nVideoWidth);
	}
	// ������Ƶ�߶�����...
	if( nVideoHeight > 0 ) {
	    pbuf = AmfStringToBytes(pbuf, "height");
		pbuf = AmfDoubleToBytes(pbuf, nVideoHeight);
	}
	// ������Ƶ��־...
    pbuf = AmfStringToBytes(pbuf, "hasVideo");
	pbuf = AmfBoolToBytes(pbuf, bHasVideo);
	// �������ӱ�־...
    pbuf = AmfStringToBytes(pbuf, "hasAudio");
	pbuf = AmfBoolToBytes(pbuf, bHasAudio);
	// �������Ƶ����������Ƶ�����Ϣ...
	if( bHasVideo ) {
		// ͳһ����Ĭ�ϵ���Ƶ֡������...
		pbuf = AmfStringToBytes(pbuf, "framerate");
		pbuf = AmfDoubleToBytes(pbuf, 25);
		// ������Ƶ������...
		pbuf = AmfStringToBytes(pbuf, "videocodecid");
		pbuf = UI08ToBytes(pbuf, AMF_DATA_TYPE_STRING);
		pbuf = AmfStringToBytes(pbuf, "avc1");
	}
	// �������Ƶ����������Ƶ�����Ϣ...
	if( bHasAudio ) {
		pbuf = AmfStringToBytes(pbuf, "audiocodecid");
		pbuf = UI08ToBytes(pbuf, AMF_DATA_TYPE_STRING);
		pbuf = AmfStringToBytes(pbuf, "mp4a");
	}
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

CRenderWnd * CPushThread::GetRenderWnd()
{
	if( m_lpCamera == NULL || m_lpCamera->GetVideoWnd() == NULL )
		return NULL;
	ASSERT( m_lpCamera != NULL && m_lpCamera->GetVideoWnd() != NULL );
	return m_lpCamera->GetVideoWnd()->GetRenderWnd();
}

void CPushThread::StartUDPThread()
{
	if( m_lpRtspThread == NULL )
		return;
	int nRateIndex = m_lpRtspThread->GetAudioRateIndex();
	int nChannelNum = m_lpRtspThread->GetAudioChannelNum();
	int nVideoFPS = m_lpRtspThread->GetVideoFPS();
	int nVideoWidth = m_lpRtspThread->GetVideoWidth();
	int nVideoHeight = m_lpRtspThread->GetVideoHeight();
	string & strSPS = m_lpRtspThread->GetVideoSPS();
	string & strPPS = m_lpRtspThread->GetVideoPPS();
	string & strAVCHeader = m_lpRtspThread->GetAVCHeader();
	string & strAACHeader = m_lpRtspThread->GetAACHeader();
	// �������Ƶ�����...
	if( strAVCHeader.size() > 0 ) {
		//this->StartPlayByVideo(strSPS, strPPS, nVideoWidth, nVideoHeight, nVideoFPS);
		this->StartSendByVideo(strSPS, strPPS, nVideoWidth, nVideoHeight, nVideoFPS);
	}
	// �������Ƶ�����...
	if( strAACHeader.size() > 0 ) {
		//this->StartPlayByAudio(nRateIndex, nChannelNum);
		this->StartSendByAudio(nRateIndex, nChannelNum);
	}
	// ����Ƶ��׼����֮��������߳�...
	if( m_lpUDPSendThread != NULL ) {
		m_lpUDPSendThread->InitThread();
	}
}

void CPushThread::StartPlayByVideo(string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS)
{
	if( m_lpPlaySDL == NULL ) {
		m_lpPlaySDL = new CPlaySDL();
	}
	ASSERT( m_lpPlaySDL != NULL );
	CRenderWnd * lpRenderWnd = m_lpCamera->GetVideoWnd()->GetRenderWnd();
	m_lpPlaySDL->InitVideo(lpRenderWnd, inSPS, inPPS, nWidth, nHeight, nFPS);
}

void CPushThread::StartPlayByAudio(int nRateIndex, int nChannelNum)
{
	if( m_lpPlaySDL == NULL ) {
		m_lpPlaySDL = new CPlaySDL();
	}
	ASSERT( m_lpPlaySDL != NULL );
	m_lpPlaySDL->InitAudio(nRateIndex, nChannelNum);
}

void CPushThread::StartSendByAudio(int nRateIndex, int nChannelNum)
{
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	int nDBRoomID = theConfig.GetCurSelRoomID();
	int nDBCameraID = m_lpCamera->GetDBCameraID();
	if( m_lpUDPSendThread == NULL ) {
		m_lpUDPSendThread = new CUDPSendThread(nDBRoomID, nDBCameraID);
	}
	ASSERT( m_lpUDPSendThread != NULL );
	m_lpUDPSendThread->InitAudio(nRateIndex, nChannelNum);

	// ���������߳�...
	if( m_lpUDPRecvThread != NULL )
		return;
	m_lpUDPRecvThread = new CUDPRecvThread(this, nDBRoomID, nDBCameraID);
	m_lpUDPRecvThread->InitThread();
}

void CPushThread::StartSendByVideo(string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS)
{
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	int nDBRoomID = theConfig.GetCurSelRoomID();
	int nDBCameraID = m_lpCamera->GetDBCameraID();
	if( m_lpUDPSendThread == NULL ) {
		m_lpUDPSendThread = new CUDPSendThread(nDBRoomID, nDBCameraID);
	}
	ASSERT( m_lpUDPSendThread != NULL );
	m_lpUDPSendThread->InitVideo(inSPS, inPPS, nWidth, nHeight, nFPS);

	// ���������߳�...
	if( m_lpUDPRecvThread != NULL )
		return;
	m_lpUDPRecvThread = new CUDPRecvThread(this, nDBRoomID, nDBCameraID);
	m_lpUDPRecvThread->InitThread();
}