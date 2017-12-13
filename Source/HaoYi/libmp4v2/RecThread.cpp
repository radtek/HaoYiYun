
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
	// 保存 SPS 和 PPS 格式头信息..
	ASSERT( inSPS.size() > 0 && inPPS.size() > 0 );
	m_strSPS = inSPS, m_strPPS = inPPS;	
}

void CRecThread::StoreAudioHeader(int inAudioRate, int inAudioChannel)
{
	// 根据音频采样率转换成采样索引...
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
	// 保存音频采样率和音频声道数...
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

	// 保存 AES 数据头信息...
	int aac_len = (int)(pbuf - aac_seq_buf);
	m_strAudioAES.assign(aac_seq_buf, aac_len);
}

BOOL CRecThread::MP4CreateVideoTrack()
{
	// 判断MP4对象是否有效...
	if( m_lpLibMP4 == NULL || m_strUTF8MP4.size() <= 0 )
		return false;
	ASSERT( m_lpLibMP4 != NULL && m_strUTF8MP4.size() > 0 );
	// 判断获取数据的有效性 => 没有视频，直接返回...
	if( m_nVideoWidth <= 0 || m_nVideoHeight <= 0 || m_strPPS.size() <= 0 || m_strSPS.size() <= 0 )
		return false;
	// 创建视频轨道...
	ASSERT( !m_lpLibMP4->IsVideoCreated() );
	return m_lpLibMP4->CreateVideoTrack(m_strUTF8MP4.c_str(), VIDEO_TIME_SCALE, m_nVideoWidth, m_nVideoHeight, m_strSPS, m_strPPS);
}

BOOL CRecThread::MP4CreateAudioTrack()
{
	// 判断MP4对象是否有效...
	if( m_lpLibMP4 == NULL || m_strUTF8MP4.size() <= 0 )
		return false;
	ASSERT( m_lpLibMP4 != NULL && m_strUTF8MP4.size() > 0 );
	// 判断音频参数是否有效 => 没有音频，直接返回...
	if( m_audio_sample_rate <= 0 || m_strAudioAES.size() <= 0 )
		return false;
	// 创建音频轨道...
	ASSERT( !m_lpLibMP4->IsAudioCreated() );
	return m_lpLibMP4->CreateAudioTrack(m_strUTF8MP4.c_str(), m_audio_sample_rate, m_strAudioAES);
}

BOOL CRecThread::StreamBeginRecord()
{
	// 释放MP4对象...
	if( m_lpLibMP4 != NULL ) {
		delete m_lpLibMP4;
		m_lpLibMP4 = NULL;
	}
	// 创建新的MP4对象...
	m_lpLibMP4 = new LibMP4();
	ASSERT( m_lpLibMP4 != NULL );
	// 启动第一个切片录像...
	return this->BeginRecSlice();
}

BOOL CRecThread::StreamEndRecord()
{
	// 先停止录像切片操作...
	this->EndRecSlice();
	// 再删除录像对象...
	if( m_lpLibMP4 != NULL ) {
		delete m_lpLibMP4;
		m_lpLibMP4 = NULL;
	}
	// 最后清空交错缓存区...
	m_MapMonitor.clear();
	m_nKeyMonitor = 0;
	return true;
}
//
// 启动切片录像操作...
BOOL CRecThread::BeginRecSlice()
{
	if( m_lpCamera == NULL )
		return false;
	ASSERT( m_lpCamera != NULL );
	// 复位录像信息变量...
	m_dwWriteSec = 0;
	m_dwWriteSize = 0;
	// 获取唯一的文件名...
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
	// 准备录像需要的信息...
	CString  strMP4Path;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	string & strSavePath = theConfig.GetSavePath();
	// 准备JPG截图文件 => PATH + Uniqid + DBCameraID + .jpg
	m_strJpgName.Format("%s\\%s_%d.jpg", strSavePath.c_str(), strUniqid.c_str(), nDBCameraID);
	// 2017.08.10 - by jackey => 新增创建时间戳字段...
	// 准备MP4录像名称 => PATH + Uniqid + DBCameraID + CreateTime + CourseID
	m_dwRecCTime = (DWORD)::time(NULL);
	m_strMP4Name.Format("%s\\%s_%d_%lu_%d", strSavePath.c_str(), strUniqid.c_str(), nDBCameraID, m_dwRecCTime, nCourseID);
	// 录像时使用.tmp，避免没有录像完就被上传...
	strMP4Path.Format("%s.tmp", m_strMP4Name);
	m_strUTF8MP4 = CUtilTool::ANSI_UTF8(strMP4Path);
	// 创建视频轨道...
	this->MP4CreateVideoTrack();
	// 创建音频轨道...
	this->MP4CreateAudioTrack();
	return true;
}
//
// 停止录像切片操作...
BOOL CRecThread::EndRecSlice()
{
	// 关闭录像之前，获取已录制时间和长度...
	if( m_lpLibMP4 != NULL ) {
		m_lpLibMP4->Close();
	}
	// 进行录像后的截图、改文件名操作...
	if( m_dwWriteSize > 0 && m_dwWriteSec > 0 ) {
		this->doStreamSnapJPG(m_dwWriteSec);
	}
	// 这里需要复位录制变量，否则在退出时出错...
	m_dwWriteSec = 0; m_dwWriteSize = 0;
	return true;
}
//
// 处理录像后的截图事件...
void CRecThread::doStreamSnapJPG(int nRecSecond)
{
	// 更换录像文件的扩展名...
	CString strMP4Temp, strMP4File;
	strMP4Temp.Format("%s.tmp", m_strMP4Name);
	strMP4File.Format("%s_%d.mp4", m_strMP4Name, nRecSecond);
	// 如果文件不存在，直接返回...
	if( _access(strMP4Temp, 0) < 0 )
		return;
	// 调用接口进行截图操作，截图失败，错误记录...
	ASSERT( m_strJpgName.GetLength() > 0 );
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	if( !theConfig.StreamSnapJpeg(strMP4Temp, m_strJpgName, nRecSecond) ) {
		MsgLogGM(GM_Snap_Jpg_Err);
	}
	// 直接对文件进行更改操作，并记录失败的日志...
	if( !MoveFile(strMP4Temp, strMP4File) ) {
		MsgLogGM(::GetLastError());
	}
}
//
// 进行录像处理 => 并判断是否开启新的切片...
BOOL CRecThread::StreamWriteRecord(FMS_FRAME & inFrame)
{
	// 录像对象有效，才能写盘...
	if( m_lpLibMP4 == NULL )
		return false;
	ASSERT( m_lpLibMP4 != NULL );
	// 进行写盘操作，内部会判断是否能够写盘...
	BOOL bIsVideo = ((inFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO) ? true : false);
	if( !m_lpLibMP4->WriteSample(bIsVideo, (BYTE*)inFrame.strData.c_str(), inFrame.strData.size(), inFrame.dwSendTime, inFrame.dwRenderOffset, inFrame.is_keyframe) )
		return false;
	// 这里需要记录已录制文件大小和已录制毫秒数...
	m_dwWriteSize = m_lpLibMP4->GetWriteSize();
	m_dwWriteSec = m_lpLibMP4->GetWriteSec();
	// 如果没有视频，则不做交错处理...
	if( !m_lpLibMP4->IsVideoCreated() )
		return true;
	// 如果是不是云监控模式，不进行切片处理...
	// 2017.12.08 => 放开限制，云监控、云录播都能切片...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	/*if( theConfig.GetWebType() != kCloudMonitor )
		return true;
	ASSERT( theConfig.GetWebType() == kCloudMonitor );*/
	// 将录像切片由分钟转换成秒数...
	int nSliceSec  = theConfig.GetSliceVal() * 60;
	int nInterKey = theConfig.GetInterVal();
	// 如果切片时间为0，表示不进行切片...
	if( nSliceSec <= 0 )
		return true;
	ASSERT( nSliceSec > 0 );
	do {
		// 如果切片交错 <= 0，复位切片交错相关变量...
		if( nInterKey <= 0 ) {
			m_MapMonitor.clear();
			m_nKeyMonitor = 0;
			break;
		}
		// 如果切片交错 > 0，进行切片交错...
		ASSERT( nInterKey > 0 );
		// 将数据转存到专门的缓存队列当中 => 第一帧肯定是关键帧，第一个数据不是关键帧，直接丢弃...
		if((m_MapMonitor.size() <= 0) && (inFrame.typeFlvTag != FLV_TAG_TYPE_VIDEO || !inFrame.is_keyframe))
			break;
		// 将写盘成功的数据帧缓存起来，以便交错时使用...
		m_MapMonitor.insert(pair<uint32_t, FMS_FRAME>(inFrame.dwSendTime, inFrame));
		// 如果是视频关键帧，关键帧计数器累加，超过设定值，丢弃前面的关键帧和非关键帧数据 => 音视频一起丢弃...
		if( inFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO && inFrame.is_keyframe ) {
			if( ++m_nKeyMonitor > nInterKey ) {
				this->dropSliceKeyFrame();
			}
		}
		// 处理完毕，跳出循环，检测录像切片时间是否到达...
	}while( false );
	// 没有到达录像切片时间，直接返回 => 单位是秒...
	// 切片时间是录像计时时间，而不是已写入文件的时间...
	DWORD dwElapseSec = (DWORD)::time(NULL) - m_dwRecCTime;
	if( dwElapseSec < nSliceSec )
		return true;
	ASSERT( dwElapseSec >= nSliceSec );
	// 到达切片时间，停止录像切片...
	this->EndRecSlice();
	// 开启新的切片录像...
	this->BeginRecSlice();
	// 需要切片交错 => 立即把缓存的交错关键帧存盘...
	if( nInterKey > 0 ) {
		this->doSaveInterFrame();
	}
	return true;
}
//
// 保存交错缓存当中的数据 => 已经按照时序排列好了...
void CRecThread::doSaveInterFrame()
{
	TRACE("== [doSaveInterFrame] nKeyMonitor = %d, MonitorSize = %d ==\n", m_nKeyMonitor, m_MapMonitor.size());
	KH_MapFrame::iterator itorItem = m_MapMonitor.begin();
	while( itorItem != m_MapMonitor.end() ) {
		int nSize = m_MapMonitor.count(itorItem->first);
		// 对相同时间戳的数据帧进行循环存盘 ...
		for(int i = 0; i < nSize; ++i) {
			FMS_FRAME & theFrame = itorItem->second;
			BOOL bIsVideo = ((theFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO) ? true : false);
			m_lpLibMP4->WriteSample(bIsVideo, (BYTE*)theFrame.strData.c_str(), theFrame.strData.size(), theFrame.dwSendTime, theFrame.dwRenderOffset, theFrame.is_keyframe);
			// 累加算子，插入下一个节点...
			++itorItem;
		}
	}
}
//
// 删除交错缓冲队列中的数据，直到遇到关键帧为止...
void CRecThread::dropSliceKeyFrame()
{
	// 设置已经删除关键帧标志...
	BOOL bHasDelKeyFrame = false;
	KH_MapFrame::iterator itorItem = m_MapMonitor.begin();
	while( itorItem != m_MapMonitor.end() ) {
		int nSize = m_MapMonitor.count(itorItem->first);
		// 对相同时间戳的数据帧进行循环删除...
		for(int i = 0; i < nSize; ++i) {
			FMS_FRAME & theFrame = itorItem->second;
			// 如果发现了视频关键帧...
			if( theFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO && theFrame.is_keyframe ) {
				// 已经删除过一个关键帧，遇到新的关键帧，设置发送时间戳，直接返回...
				if( bHasDelKeyFrame ) {
					//TRACE("== [dropSliceKeyFrame] nKeyMonitor = %d, MonitorSize = %d ==\n", m_nKeyMonitor, m_MapMonitor.size());
					return;
				}
				// 删除这个关键帧，设置标志，打印信息...
				--m_nKeyMonitor; bHasDelKeyFrame = true;
			}
			// 这里使用算子删除而不是关键字...
			m_MapMonitor.erase(itorItem++);
		}
	}
}

/////////////////////////////////////////////////
// 这是有关 rtsp 直接录像的代码实现...
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
	// 设置rtsp循环退出标志...
	m_rtspEventLoopWatchVariable = 1;
	// 停止线程...
	this->StopAndWaitForThread();
	// 停止录像...
	this->StreamEndRecord();
	// 打印退出信息...
	TRACE("[~CRtspRecThread Thread] - Exit\n");
}

BOOL CRtspRecThread::InitThread(LPCTSTR lpszRtspUrl)
{
	// 判断并保存rtsp拉流地址...
	if( lpszRtspUrl == NULL || strlen(lpszRtspUrl) <= 0 )
		return false;
	m_strRtspUrl = lpszRtspUrl;

	// 创建rtsp链接环境...
	m_scheduler_ = BasicTaskScheduler::createNew();
	m_env_ = BasicUsageEnvironment::createNew(*m_scheduler_);
	m_rtspClient_ = ourRTSPClient::createNew(*m_env_, m_strRtspUrl.c_str(), 1, "rtspRecord", NULL, this);

	// 2017.07.21 - by jackey => 有些服务器必须先发OPTIONS...
	// 发起第一次rtsp握手 => 先发起 OPTIONS 命令...
	m_rtspClient_->sendOptionsCommand(continueAfterOPTIONS); 

	// 启动线程...
	this->Start();

	return true;
}

void CRtspRecThread::Entry()
{
	// 进行任务循环检测，修改 m_rtspEventLoopWatchVariable 可以让任务退出...
	ASSERT( m_env_ != NULL && m_rtspClient_ != NULL );
	m_env_->taskScheduler().doEventLoop(&m_rtspEventLoopWatchVariable);

	// 设置数据结束标志...
	m_bFinished = true;

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
