
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
	// 停止线程...
	this->StopAndWaitForThread();

	// 删除UDP数据接收线程...
	if( m_lpUDPRecvThread != NULL ) {
		delete m_lpUDPRecvThread;
		m_lpUDPRecvThread = NULL;
	}
	// 删除UDP数据发送线程...
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
	MsgLogINFO("== [~CPushThread Thread] - Exit ==");

	// 释放存盘文件...
#ifdef _SAVE_H264_
	fclose(m_lpH264File);
	m_lpH264File = NULL;
#endif
}
//
// 处理摄像头设备的线程初始化...
/*BOOL CPushThread::DeviceInitThread(CString & strRtspUrl, string & strRtmpUrl)
{
	if( m_lpCamera == NULL )
		return false;
	ASSERT( m_lpCamera != NULL );
	// 保存传递过来的参数...
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
	// 是由 rtsp 拉流线程来启动设备的推流线程...

	return true;
}*/
//
// 处理流转发线程的初始化...
BOOL CPushThread::StreamInitThread(BOOL bFileMode, BOOL bUsingTCP, string & strStreamUrl, string & strStreamMP4)
{
	// 保存传递过来的参数...
	if( m_lpCamera == NULL )
		return false;
	ASSERT( m_lpCamera != NULL );
	// 创建MP4线程或rtsp或rtmp线程...
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
	// 记录当前时间，单位（毫秒）...
	DWORD dwInitTimeMS = ::GetTickCount();
	// 初始化接收码流时间起点 => 每隔1秒钟复位...
	m_dwRecvTimeMS = dwInitTimeMS;
	// 初始化接收数据超时时间记录起点 => 超过 3 分钟无数据接收，则判定为超时...
	m_dwTimeOutMS = dwInitTimeMS;
	// 记录通道截图间隔时间，单位（毫秒）...
	m_dwSnapTimeMS = dwInitTimeMS;
	return true;
}
//
// 启动流转发直播上传...
BOOL CPushThread::StreamStartLivePush(string & strRtmpUrl)
{
	if( m_lpCamera == NULL )
		return false;
	ASSERT( m_lpCamera != NULL );
	// IE8会连续发送两遍播放命令，需要确保只启动一次...
	if( m_lpRtmpPush != NULL )
		return true;
	// 保存需要的数据信息...
	m_strRtmpUrl = strRtmpUrl;
	ASSERT( strRtmpUrl.size() > 0 );
	// 创建rtmp上传对象...
	m_lpRtmpPush = new LibRtmp(false, true, NULL);
	ASSERT( m_lpRtmpPush != NULL );
	// 立即启动推流线程...
	this->Start();
	return true;
}
//
// 停止流转发直播上传...
BOOL CPushThread::StreamStopLivePush()
{
	if( m_lpCamera == NULL )
		return false;
	ASSERT( m_lpCamera != NULL );
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
	// 判断获取数据的有效性 => 没有视频，直接返回...
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

	string strAAC;
	int audio_type = 2;
	int	audio_rate_index = 0;			// 采样率编号
	int	audio_channel_num = 0;			// 声道数目
	int	audio_sample_rate = 48000;		// 音频采样率

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
	// 没有音频，直接返回 => 2017.07.25 - by jackey...
	if( strAAC.size() <= 0 )
		return false;
  
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
// 启动切片录像操作...
BOOL CPushThread::BeginRecSlice()
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
	// 2018.02.06 - by jackey => 新增切片标志字段...
	// 准备MP4录像名称 => PATH + Uniqid + DBCameraID + CreateTime + CourseID + SliceID + SliceIndex
	m_dwRecCTime = (DWORD)::time(NULL);
	m_strMP4Name.Format("%s\\%s_%d_%lu_%d_%s_%d", strSavePath.c_str(), strUniqid.c_str(), 
						nDBCameraID, m_dwRecCTime, nCourseID, m_strSliceID, ++m_nSliceInx);
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
BOOL CPushThread::EndRecSlice()
{
	// 关闭录像之前，获取已录制时间和长度...
	if( m_lpRecMP4 != NULL ) {
		m_lpRecMP4->Close();
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
void CPushThread::doStreamSnapJPG(int nRecSecond)
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
// 使用ffmpeg进行数据帧动态截图...
void CPushThread::doFFmpegSnapJPG()
{
	// 判断通道对象是否有效...
	if( m_lpCamera == NULL || m_strSnapFrame.size() <= 0 )
		return;
	ASSERT( m_lpCamera != NULL );
	ASSERT( m_strSnapFrame.size() > 0 );
	// 获取路径相关信息...
	CString strJPGFile;
	int nDBCameraID = m_lpCamera->GetDBCameraID();
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	string & strSavePath = theConfig.GetSavePath();
	// 准备JPG截图文件 => PATH + live + DBCameraID + .jpg
	strJPGFile.Format("%s\\live_%d.jpg", strSavePath.c_str(), nDBCameraID);
	// 准备截图需要的sps和pps...
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
	// 必须先写入sps，再写如pps...
	strSnapData.append((char*)&dwTag, sizeof(DWORD));
	strSnapData.append(strSPS);
	strSnapData.append((char*)&dwTag, sizeof(DWORD));
	strSnapData.append(strPPS);
	// 最后写入关键帧，起始码已经更新...
	strSnapData.append(m_strSnapFrame);
	// 调用截图接口进行动态截图...
	if( !theConfig.FFmpegSnapJpeg(strSnapData, strJPGFile) ) {
		MsgLogGM(GM_Snap_Jpg_Err);
	} else {
		CUtilTool::MsgLog(kTxtLogger, "== ffmpeg snap jpg(%d) ==\r\n", nDBCameraID);
	}
}
//
// 开始流转发模式的录像...
BOOL CPushThread::StreamBeginRecord()
{
	// 这里需要进行互斥保护...
	OSMutexLocker theLock(&m_Mutex);
	// 判断状态是否有效...
	if( !this->IsStreamPlaying() )
		return false;
	ASSERT( this->IsStreamPlaying() );
	// 释放录像对象...
	if( m_lpRecMP4 != NULL ) {
		delete m_lpRecMP4;
		m_lpRecMP4 = NULL;
	}
	// 创建新的录像对象...
	m_lpRecMP4 = new LibMP4();
	ASSERT( m_lpRecMP4 != NULL );
	// 计算切片标志信息...
	ULARGE_INTEGER	llTimCountCur = {0};
	::GetSystemTimeAsFileTime((FILETIME *)&llTimCountCur);
	m_strSliceID.Format("%I64d", llTimCountCur.QuadPart);
	m_nSliceInx = 0;
	// 启动第一个切片录像...
	return this->BeginRecSlice();
}
//
// 进行录像处理 => 并判断是否开启新的切片...
BOOL CPushThread::StreamWriteRecord(FMS_FRAME & inFrame)
{
	// 录像对象有效，才能写盘...
	if( m_lpRecMP4 == NULL )
		return false;
	ASSERT( m_lpRecMP4 != NULL );
	// 进行写盘操作，内部会判断是否能够写盘...
	BOOL bIsVideo = ((inFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO) ? true : false);
	if( !m_lpRecMP4->WriteSample(bIsVideo, (BYTE*)inFrame.strData.c_str(), inFrame.strData.size(), inFrame.dwSendTime, inFrame.dwRenderOffset, inFrame.is_keyframe) )
		return false;
	// 这里需要记录已录制文件大小和已录制秒数...
	m_dwWriteSize = m_lpRecMP4->GetWriteSize();
	m_dwWriteSec = m_lpRecMP4->GetWriteSec();
	//TRACE("Write Second: %lu\n", m_dwWriteSec);
	// 如果没有视频，则不做交错处理...
	if( !m_lpRecMP4->IsVideoCreated() )
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
void CPushThread::doSaveInterFrame()
{
	CUtilTool::MsgLog(kTxtLogger,"== [doSaveInterFrame] nKeyMonitor = %d, MonitorSize = %d ==\r\n", m_nKeyMonitor, m_MapMonitor.size());
	KH_MapFrame::iterator itorItem = m_MapMonitor.begin();
	while( itorItem != m_MapMonitor.end() ) {
		int nSize = m_MapMonitor.count(itorItem->first);
		// 对相同时间戳的数据帧进行循环存盘 ...
		for(int i = 0; i < nSize; ++i) {
			FMS_FRAME & theFrame = itorItem->second;
			BOOL bIsVideo = ((theFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO) ? true : false);
			m_lpRecMP4->WriteSample(bIsVideo, (BYTE*)theFrame.strData.c_str(), theFrame.strData.size(), theFrame.dwSendTime, theFrame.dwRenderOffset, theFrame.is_keyframe);
			// 累加算子，插入下一个节点...
			++itorItem;
		}
	}
}
//
// 停止流转发模式的录像 => 删除录像对象，停止切片...
BOOL CPushThread::StreamEndRecord()
{
	// 这里需要进行互斥保护...
	OSMutexLocker theLock(&m_Mutex);
	// 先停止录像切片操作...
	this->EndRecSlice();
	// 再删除录像对象...
	if( m_lpRecMP4 != NULL ) {
		delete m_lpRecMP4;
		m_lpRecMP4 = NULL;
	}
	// 最后清空交错缓存区...
	m_MapMonitor.clear();
	m_nKeyMonitor = 0;
	return true;
}
//
// 返回是否正在录像标志...
BOOL CPushThread::IsRecording()
{
	//2018.01.01 - by jackey => 这里不能加互斥，会卡住界面...
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
void CPushThread::doErrPushNotify()
{
	// 如果线程已经被主动删除了，直接退出...
	if( m_bDeleteFlag )
		return;
	ASSERT( !m_bDeleteFlag );
	// 窗口对象必须有效...
	if( m_hWndVideo == NULL )
		return;
	ASSERT( m_hWndVideo != NULL );
	// 摄像头设备模式 => 只停止rtmp上传对象...
	// 流数据转发模式 => 只停止rtmp上传对象...
	//WPARAM wMsgID = ((m_nStreamProp == kStreamDevice) ? WM_ERR_PUSH_MSG : WM_STOP_STREAM_MSG);
	WPARAM wMsgID = WM_STOP_LIVE_PUSH_MSG;
	::PostMessage(m_hWndVideo, wMsgID, NULL, NULL);
	// 打印退出信息...
	CUtilTool::MsgLog(kTxtLogger, _T("== Camera(%d) stop push by SRS ==\r\n"), ((m_lpCamera != NULL) ? m_lpCamera->GetDBCameraID() : 0));
}

void CPushThread::Entry()
{
	// 连接rtmp server，失败，通知上层删除之...
	if( !this->OpenRtmpUrl() ) {
		MsgLogINFO("[CPushThread::OpenRtmpUrl] - Error");
		this->doErrPushNotify();
		return;
	}
	// 握手成功，发送metadata数据包，失败，通知上层删除之...
	if( !this->SendMetadataPacket() ) {
		MsgLogINFO("[CPushThread::SendMetadataPacket] - Error");
		this->doErrPushNotify();
		return;
	}
	// 发送视频序列头数据包，失败，通知上层删除之...
	if( !this->SendAVCSequenceHeaderPacket() ) {
		MsgLogINFO("[CPushThread::SendAVCSequenceHeaderPacket] - Error");
		this->doErrPushNotify();
		return;
	}
	// 发送音频序列头数据包，失败，通知上层删除之...
	if( !this->SendAACSequenceHeaderPacket() ) {
		MsgLogINFO("[CPushThread::SendAACSequenceHeaderPacket] - Error");
		this->doErrPushNotify();
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
			MsgLogINFO("[CPushThread::SendOneDataPacket] - Error");
			this->doErrPushNotify();
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
	CUtilTool::MsgLog(kTxtLogger,"== OpenRtmpUrl => %s ==\r\n", m_strRtmpUrl.c_str());
	// 连接rtmp server，完成握手等协议...
	return m_lpRtmpPush->Open(m_strRtmpUrl.c_str());
}
//
// 通过其它渠道输送数据帧...
int CPushThread::PushFrame(FMS_FRAME & inFrame)
{
	OSMutexLocker theLock(&m_Mutex);
	// 判断线程是否已经退出...
	if( this->IsStopRequested() ) {
		log_trace("[Student-Pusher] Error => Push Thread has been stoped");
		return 0;
	}
	// 将音视频数据推入发送线程...
	if( m_lpUDPSendThread != NULL ) {
		m_lpUDPSendThread->PushFrame(inFrame);
	}
	// 将音视频数据推入播放线程...
	if( m_lpPlaySDL != NULL ) {
		m_lpPlaySDL->PushFrame(inFrame.strData, inFrame.typeFlvTag, inFrame.is_keyframe, inFrame.dwSendTime);
	}
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

	// 如果是新的视频数据帧是关键帧，丢弃已缓存的，存放新的关键帧，以便截图使用...
	if( inFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO ) {
		// 修改视频帧的起始码 => 0x00000001
		string strCurFrame = inFrame.strData;
		char * lpszSrc = (char*)strCurFrame.c_str();
		memset((void*)lpszSrc, 0, sizeof(DWORD));
		lpszSrc[3] = 0x01;
		// 如果是新关键帧，清空之前的缓存...
		if( inFrame.is_keyframe ) {
			m_strSnapFrame.clear();
		}
		// 将新的数据帧追加到后面，直到下一个关键帧出现...
		m_strSnapFrame.append(strCurFrame);
	}
	// 如果时间间隔超过了截图间隔，发起截图操作 => 使用ffmpeg动态截图...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	int nSnapSec = theConfig.GetSnapVal() * 60;
	// 计算能否截图标志，时间间隔由网站后台设定...
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

	/*// 如果是摄像头，不进行关键帧计数和丢帧处理...
	if( this->IsCameraDevice() ) {
		return m_MapFrame.size();
	}
	// 如果是流转发模式，并且没有处于发布状态...
	ASSERT( !this->IsCameraDevice() );*/

	// 判断新数据是否是视频关键帧，不是视频关键帧，直接返回...
	if( inFrame.typeFlvTag != FLV_TAG_TYPE_VIDEO || !inFrame.is_keyframe ) {
		return m_MapFrame.size();
	}
	// 如果是视频关键帧，累加计数器...
	ASSERT( inFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO && inFrame.is_keyframe );
	// 累加关键帧计数器...
	++m_nKeyFrame;
	//TRACE("== [PushFrame] nKeyFrame = %d, FrameSize = %d, StreamSize = %d, FirstSendTime = %lu ==\n", m_nKeyFrame, m_MapFrame.size(), m_MapStream.size(), m_dwFirstSendTime);
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
					//TRACE("== [dropToKeyFrame] nKeyFrame = %d, FrameSize = %d, StreamSize = %d, FirstSendTime = %lu ==\n", m_nKeyFrame, m_MapFrame.size(), m_MapStream.size(), m_dwFirstSendTime);
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
// 删除交错缓冲队列中的数据，直到遇到关键帧为止...
void CPushThread::dropSliceKeyFrame()
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
//
// 开始准备发送数据包...
void CPushThread::BeginSendPacket()
{
	OSMutexLocker theLock(&m_Mutex);
	ASSERT( this->IsPublishing() );
	
	/*// 只有在流转发模式下才进行，开始时间的调整...
	// 因为摄像头模式下，都是单独重头链接 rtsp 不会有帧断裂的问题...
	if( this->IsCameraDevice() )
		return;
	ASSERT( !this->IsCameraDevice() );*/

	// 设置第一帧的发送时间戳...
	KH_MapFrame::iterator itorItem = m_MapFrame.begin();
	if( itorItem != m_MapFrame.end() ) {
		m_dwFirstSendTime = itorItem->first;
		CUtilTool::MsgLog(kTxtLogger, "== [BeginSendPacket] nKeyFrame = %d, FrameSize = %d, StreamSize = %d, FirstSendTime = %lu ==\r\n", m_nKeyFrame, m_MapFrame.size(), m_MapStream.size(), m_dwFirstSendTime);
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
	
	/*// 如果是流转发模式，需要将缓存的数据插入到缓存发送队列前面...
	if( this->IsCameraDevice() )
		return;
	ASSERT( !this->IsCameraDevice() );*/

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
	// 2017.08.11 - by jackey => MapStream当中只保存了一个视频关键帧，数据被还原给了缓存队列，因此，需要对关键帧计数累加...
	// 如果缓存队列有数据，关键帧计数器增加...
	if( m_MapStream.size() > 0 ) {
		++m_nKeyFrame;
		CUtilTool::MsgLog(kTxtLogger, "== [EndSendPacket] nKeyFrame = %d, FrameSize = %d, StreamSize = %d, FirstSendTime = %lu ==\r\n", m_nKeyFrame, m_MapFrame.size(), m_MapStream.size(), m_dwFirstSendTime);
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
	//2018.01.01 - by jackey => 这里不能加互斥，会卡住界面...
	//OSMutexLocker theLock(&m_Mutex);

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
	if( this->IsDataFinished() ) {
		MsgLogINFO("== Push Data Finished ==");
		return -1;
	}
	// 如果数据还没有结束，则需要有一定缓存，以便音视频能够自动排序，然后再发送数据包...
	// 这里以前设定是100个数据包，为了降低延时，调整为20个数据包...
	if( m_MapFrame.size() < 20 )
		return 0;
	ASSERT( !this->IsDataFinished() && m_MapFrame.size() >= 20 );

	bool is_ok = false;
	ASSERT( m_MapFrame.size() > 0 );

	// 读取第一个数据包(可能包含多个数据)...
	KH_MapFrame::iterator itorItem = m_MapFrame.begin();
	int nSize = m_MapFrame.count(itorItem->first);
	// 对相同时间戳的数据帧进行循环处理...
	//DWORD dwCurTickCount = ::GetTickCount();
	//TRACE("== Begin Send-Time: %lu ==\n", dwCurTickCount);
	for(int i = 0; i < nSize; ++i) {
		FMS_FRAME & theFrame = itorItem->second;
		
		/*// 如果是流转发模式，才需要对时间戳进行调整，硬件模式下的时间戳都是从0开始的，无需调整...
		if( !this->IsCameraDevice() ) {
			theFrame.dwSendTime = theFrame.dwSendTime - m_dwFirstSendTime;
		}*/

		// 是持续连接，需要对时间戳进行修正 => 硬件模式也统一到了流转发模式...
		theFrame.dwSendTime = theFrame.dwSendTime - m_dwFirstSendTime;

		//TRACE("[%s] SendTime = %lu, Key = %d, Size = %d, MapSize = %d\n", ((theFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO) ? "Video" : "Audio"), theFrame.dwSendTime, theFrame.is_keyframe, theFrame.strData.size(), m_MapFrame.size());
		// 将获取到的数据帧发送走...
		switch( theFrame.typeFlvTag )
		{
		case FLV_TAG_TYPE_AUDIO: is_ok = this->SendAudioDataPacket(theFrame); break;
		case FLV_TAG_TYPE_VIDEO: is_ok = this->SendVideoDataPacket(theFrame); break;
		}
		// 累加发送字节数...
		m_nCurSendByte += theFrame.strData.size();
		// 缓存关键帧 => 先清空以前缓存的数据 => 关键帧计数器减少...
		// 把时间戳还原回去...
		theFrame.dwSendTime = theFrame.dwSendTime + m_dwFirstSendTime;
		// 发现新的关键帧，清空以前的缓存，减少关键帧计数器...
		if( theFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO && theFrame.is_keyframe ) {
			--m_nKeyFrame; m_MapStream.clear();
			//TRACE("== [SendOneDataPacket] nKeyFrame = %d, FrameSize = %d, FirstSendTime = %lu, CurSendTime = %lu ==\n", m_nKeyFrame, m_MapFrame.size(), m_dwFirstSendTime, theFrame.dwSendTime);
		}
		// 将数据转存到专门的缓存队列当中 => 第一帧肯定是关键帧...
		m_MapStream.insert(pair<uint32_t, FMS_FRAME>(theFrame.dwSendTime, theFrame));
		// 从队列中移除一个相同时间的数据包 => 这里使用的是算子...
		m_MapFrame.erase(itorItem++);
	}
	//TRACE("== End Spend-Time: %lu ==\n", ::GetTickCount() - dwCurTickCount);
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

	TRACE("[Video] SendTime = %lu, Offset = %lu, Size = %lu\n", inFrame.dwSendTime, inFrame.dwRenderOffset, need_buf_size-5);

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
	
	//TRACE("[Audio] SendTime = %lu, Size = %lu\n", inFrame.dwSendTime, need_buf_size);

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
	MsgLogINFO("== RtmpPush => Send Video SequenceHeaderPacket ==");
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

	// 设置视频宽度和高度属性...
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
	// 获取是否有音视频的标志状态信息...
	bHasVideo = ((strAVC.size() > 0) ? true : false);
	bHasAudio = ((strAAC.size() > 0) ? true : false);
	// 设置宽度属性...
	if( nVideoWidth > 0 ) {
	    pbuf = AmfStringToBytes(pbuf, "width");
		pbuf = AmfDoubleToBytes(pbuf, nVideoWidth);
	}
	// 设置视频高度属性...
	if( nVideoHeight > 0 ) {
	    pbuf = AmfStringToBytes(pbuf, "height");
		pbuf = AmfDoubleToBytes(pbuf, nVideoHeight);
	}
	// 设置视频标志...
    pbuf = AmfStringToBytes(pbuf, "hasVideo");
	pbuf = AmfBoolToBytes(pbuf, bHasVideo);
	// 设置音视标志...
    pbuf = AmfStringToBytes(pbuf, "hasAudio");
	pbuf = AmfBoolToBytes(pbuf, bHasAudio);
	// 如果有视频，才设置视频相关信息...
	if( bHasVideo ) {
		// 统一设置默认的视频帧率属性...
		pbuf = AmfStringToBytes(pbuf, "framerate");
		pbuf = AmfDoubleToBytes(pbuf, 25);
		// 设置视频编码器...
		pbuf = AmfStringToBytes(pbuf, "videocodecid");
		pbuf = UI08ToBytes(pbuf, AMF_DATA_TYPE_STRING);
		pbuf = AmfStringToBytes(pbuf, "avc1");
	}
	// 如果有音频，才设置音频相关信息...
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
	// 如果有视频的情况...
	if( strAVCHeader.size() > 0 ) {
		//this->StartPlayByVideo(strSPS, strPPS, nVideoWidth, nVideoHeight, nVideoFPS);
		this->StartSendByVideo(strSPS, strPPS, nVideoWidth, nVideoHeight, nVideoFPS);
	}
	// 如果有音频的情况...
	if( strAACHeader.size() > 0 ) {
		//this->StartPlayByAudio(nRateIndex, nChannelNum);
		this->StartSendByAudio(nRateIndex, nChannelNum);
	}
	// 音视频都准备好之后才启动线程...
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

	// 创建接收线程...
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

	// 创建接收线程...
	if( m_lpUDPRecvThread != NULL )
		return;
	m_lpUDPRecvThread = new CUDPRecvThread(this, nDBRoomID, nDBCameraID);
	m_lpUDPRecvThread->InitThread();
}