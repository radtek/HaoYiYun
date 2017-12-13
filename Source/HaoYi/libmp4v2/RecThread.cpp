
#include "stdafx.h"
#include "LibMP4.h"
#include "UtilTool.h"
#include "RecThread.h"
#include "md5.h"
#include "..\Camera.h"
#include "..\ReadSPS.h"
#include "..\XmlConfig.h"
#include "..\librtmp\BitWritter.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CRecThread::CRecThread()
{
	m_audio_sample_rate = 0;
	m_audio_rate_index = 0;
	m_audio_channel_num = 0;
	m_nVideoHeight = 0;
	m_nVideoWidth = 0;

	m_bFinished = false;
	m_lpCamera = NULL;
	m_lpLibMP4 = NULL;
	m_dwRecCTime = 0;
	m_dwWriteSize = 0;
	m_dwWriteSec = 0;
	m_nKeyMonitor = 0;
}

CRecThread::~CRecThread()
{
}

void CRecThread::StoreVideoHeader(string & inSPS, string & inPPS)
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
	// ���� SPS �� PPS ��ʽͷ��Ϣ..
	ASSERT( inSPS.size() > 0 && inPPS.size() > 0 );
	m_strSPS = inSPS, m_strPPS = inPPS;	
}

void CRecThread::StoreAudioHeader(int inAudioRate, int inAudioChannel)
{
	// ������Ƶ������ת���ɲ�������...
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
	// ������Ƶ�����ʺ���Ƶ������...
	m_audio_sample_rate = inAudioRate;
	m_audio_channel_num = inAudioChannel;
	// AudioSpecificConfig
	char   aac_seq_buf[2048] = {0};
    char * pbuf = aac_seq_buf;
	PutBitContext pb;
	init_put_bits(&pb, pbuf, 1024);
	put_bits(&pb, 5, 2);					//object type - AAC-LC
	put_bits(&pb, 4, m_audio_rate_index);	// sample rate index
	put_bits(&pb, 4, m_audio_channel_num);	// channel configuration

	//GASpecificConfig
	put_bits(&pb, 1, 0);    // frame length - 1024 samples
	put_bits(&pb, 1, 0);    // does not depend on core coder
	put_bits(&pb, 1, 0);    // is not extension

	flush_put_bits(&pb);
	pbuf += 2;

	// ���� AES ����ͷ��Ϣ...
	int aac_len = (int)(pbuf - aac_seq_buf);
	m_strAudioAES.assign(aac_seq_buf, aac_len);
}

BOOL CRecThread::MP4CreateVideoTrack()
{
	// �ж�MP4�����Ƿ���Ч...
	if( m_lpLibMP4 == NULL || m_strUTF8MP4.size() <= 0 )
		return false;
	ASSERT( m_lpLibMP4 != NULL && m_strUTF8MP4.size() > 0 );
	// �жϻ�ȡ���ݵ���Ч�� => û����Ƶ��ֱ�ӷ���...
	if( m_nVideoWidth <= 0 || m_nVideoHeight <= 0 || m_strPPS.size() <= 0 || m_strSPS.size() <= 0 )
		return false;
	// ������Ƶ���...
	ASSERT( !m_lpLibMP4->IsVideoCreated() );
	return m_lpLibMP4->CreateVideoTrack(m_strUTF8MP4.c_str(), VIDEO_TIME_SCALE, m_nVideoWidth, m_nVideoHeight, m_strSPS, m_strPPS);
}

BOOL CRecThread::MP4CreateAudioTrack()
{
	// �ж�MP4�����Ƿ���Ч...
	if( m_lpLibMP4 == NULL || m_strUTF8MP4.size() <= 0 )
		return false;
	ASSERT( m_lpLibMP4 != NULL && m_strUTF8MP4.size() > 0 );
	// �ж���Ƶ�����Ƿ���Ч => û����Ƶ��ֱ�ӷ���...
	if( m_audio_sample_rate <= 0 || m_strAudioAES.size() <= 0 )
		return false;
	// ������Ƶ���...
	ASSERT( !m_lpLibMP4->IsAudioCreated() );
	return m_lpLibMP4->CreateAudioTrack(m_strUTF8MP4.c_str(), m_audio_sample_rate, m_strAudioAES);
}

BOOL CRecThread::StreamBeginRecord()
{
	// �ͷ�MP4����...
	if( m_lpLibMP4 != NULL ) {
		delete m_lpLibMP4;
		m_lpLibMP4 = NULL;
	}
	// �����µ�MP4����...
	m_lpLibMP4 = new LibMP4();
	ASSERT( m_lpLibMP4 != NULL );
	// ������һ����Ƭ¼��...
	return this->BeginRecSlice();
}

BOOL CRecThread::StreamEndRecord()
{
	// ��ֹͣ¼����Ƭ����...
	this->EndRecSlice();
	// ��ɾ��¼�����...
	if( m_lpLibMP4 != NULL ) {
		delete m_lpLibMP4;
		m_lpLibMP4 = NULL;
	}
	// �����ս�������...
	m_MapMonitor.clear();
	m_nKeyMonitor = 0;
	return true;
}
//
// ������Ƭ¼�����...
BOOL CRecThread::BeginRecSlice()
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
	// ׼��MP4¼������ => PATH + Uniqid + DBCameraID + CreateTime + CourseID
	m_dwRecCTime = (DWORD)::time(NULL);
	m_strMP4Name.Format("%s\\%s_%d_%lu_%d", strSavePath.c_str(), strUniqid.c_str(), nDBCameraID, m_dwRecCTime, nCourseID);
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
BOOL CRecThread::EndRecSlice()
{
	// �ر�¼��֮ǰ����ȡ��¼��ʱ��ͳ���...
	if( m_lpLibMP4 != NULL ) {
		m_lpLibMP4->Close();
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
void CRecThread::doStreamSnapJPG(int nRecSecond)
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
// ����¼���� => ���ж��Ƿ����µ���Ƭ...
BOOL CRecThread::StreamWriteRecord(FMS_FRAME & inFrame)
{
	// ¼�������Ч������д��...
	if( m_lpLibMP4 == NULL )
		return false;
	ASSERT( m_lpLibMP4 != NULL );
	// ����д�̲������ڲ����ж��Ƿ��ܹ�д��...
	BOOL bIsVideo = ((inFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO) ? true : false);
	if( !m_lpLibMP4->WriteSample(bIsVideo, (BYTE*)inFrame.strData.c_str(), inFrame.strData.size(), inFrame.dwSendTime, inFrame.dwRenderOffset, inFrame.is_keyframe) )
		return false;
	// ������Ҫ��¼��¼���ļ���С����¼�ƺ�����...
	m_dwWriteSize = m_lpLibMP4->GetWriteSize();
	m_dwWriteSec = m_lpLibMP4->GetWriteSec();
	// ���û����Ƶ������������...
	if( !m_lpLibMP4->IsVideoCreated() )
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
void CRecThread::doSaveInterFrame()
{
	TRACE("== [doSaveInterFrame] nKeyMonitor = %d, MonitorSize = %d ==\n", m_nKeyMonitor, m_MapMonitor.size());
	KH_MapFrame::iterator itorItem = m_MapMonitor.begin();
	while( itorItem != m_MapMonitor.end() ) {
		int nSize = m_MapMonitor.count(itorItem->first);
		// ����ͬʱ���������֡����ѭ������ ...
		for(int i = 0; i < nSize; ++i) {
			FMS_FRAME & theFrame = itorItem->second;
			BOOL bIsVideo = ((theFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO) ? true : false);
			m_lpLibMP4->WriteSample(bIsVideo, (BYTE*)theFrame.strData.c_str(), theFrame.strData.size(), theFrame.dwSendTime, theFrame.dwRenderOffset, theFrame.is_keyframe);
			// �ۼ����ӣ�������һ���ڵ�...
			++itorItem;
		}
	}
}
//
// ɾ������������е����ݣ�ֱ�������ؼ�֡Ϊֹ...
void CRecThread::dropSliceKeyFrame()
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

/////////////////////////////////////////////////
// �����й� rtsp ֱ��¼��Ĵ���ʵ��...
/////////////////////////////////////////////////
CRtspRecThread::CRtspRecThread(CCamera * lpCamera)
{
	m_lpCamera = lpCamera;
	ASSERT( m_lpCamera != NULL );

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
	// ֹͣ¼��...
	this->StreamEndRecord();
	// ��ӡ�˳���Ϣ...
	TRACE("[~CRtspRecThread Thread] - Exit\n");
}

BOOL CRtspRecThread::InitThread(LPCTSTR lpszRtspUrl)
{
	// �жϲ�����rtsp������ַ...
	if( lpszRtspUrl == NULL || strlen(lpszRtspUrl) <= 0 )
		return false;
	m_strRtspUrl = lpszRtspUrl;

	// ����rtsp���ӻ���...
	m_scheduler_ = BasicTaskScheduler::createNew();
	m_env_ = BasicUsageEnvironment::createNew(*m_scheduler_);
	m_rtspClient_ = ourRTSPClient::createNew(*m_env_, m_strRtspUrl.c_str(), 1, "rtspRecord", NULL, this);

	// 2017.07.21 - by jackey => ��Щ�����������ȷ�OPTIONS...
	// �����һ��rtsp���� => �ȷ��� OPTIONS ����...
	m_rtspClient_->sendOptionsCommand(continueAfterOPTIONS); 

	// �����߳�...
	this->Start();

	return true;
}

void CRtspRecThread::Entry()
{
	// ��������ѭ����⣬�޸� m_rtspEventLoopWatchVariable �����������˳�...
	ASSERT( m_env_ != NULL && m_rtspClient_ != NULL );
	m_env_->taskScheduler().doEventLoop(&m_rtspEventLoopWatchVariable);

	// �������ݽ�����־...
	m_bFinished = true;

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
