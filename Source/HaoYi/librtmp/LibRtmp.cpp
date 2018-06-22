
#include "stdafx.h"
#include "LibRtmp.h"
#include "UtilTool.h"
#include "srs_librtmp.h"
#include "AmfByteStream.h"
#include "..\ReadSPS.h"
#include "..\PullThread.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

uint32_t _bs_read( bs_t *s, int i_count )
{
	static uint32_t i_mask[33] ={
		0x00,
		0x01,      0x03,      0x07,      0x0f,
		0x1f,      0x3f,      0x7f,      0xff,
		0x1ff,     0x3ff,     0x7ff,     0xfff,
		0x1fff,    0x3fff,    0x7fff,    0xffff,
		0x1ffff,   0x3ffff,   0x7ffff,   0xfffff,
		0x1fffff,  0x3fffff,  0x7fffff,  0xffffff,
		0x1ffffff, 0x3ffffff, 0x7ffffff, 0xfffffff,
		0x1fffffff,0x3fffffff,0x7fffffff,0xffffffff
	};

	int      i_shr;
	uint32_t i_result = 0;

	while( i_count > 0 )
	{
		if( s->p >= s->p_end )
		{
			break;
		}

		if( ( i_shr = s->i_left - i_count ) >= 0 )
		{
			/* more in the buffer than requested */
			i_result |= ( *s->p >> i_shr )&i_mask[i_count];
			s->i_left -= i_count;
			if( s->i_left == 0 )
			{
				s->p++;
				s->i_left = 8;
			}
			return( i_result );
		}
		else
		{
			// less in the buffer than requested
			i_result |= (*s->p&i_mask[s->i_left]) << -i_shr;
			i_count  -= s->i_left;
			s->p++;
			s->i_left = 8;
		}
	}

	return( i_result );
}

LibRtmp::LibRtmp(bool isNeedLog, bool isNeedPush, CRtmpThread * lpPullThread)
{
	// 保存rtmp拉流对象和rtmp录像对象...
	m_lpRtmpThread = lpPullThread;
	m_bPushStartOK = false;
	m_lpSRSRtmp = NULL;

	// 初始化常规变量...
	m_stream_name_.clear();
    m_streming_url_.clear();
	m_is_need_push_ = isNeedPush;
	CUtilTool::MsgLog(kTxtLogger, "SRS-Version: %d.%d.%d\r\n", srs_version_major(), srs_version_minor(), srs_version_revision());
}

LibRtmp::~LibRtmp()
{
    this->Close();
}

bool LibRtmp::Open(const char* url)
{
	if( url == NULL || strlen(url) <= 0 )
		return false;
    m_streming_url_.assign(url, strlen(url));
	m_stream_name_ = m_streming_url_.substr(m_streming_url_.rfind("/")+1);

	// 拉流和推流都使用SRS提供的接口...
	int nErrCode = 0;
	BOOL bError = false;
	do {
		// 创建SRS的rtmp拉流对象...
		ASSERT( m_lpSRSRtmp == NULL );
		m_lpSRSRtmp = srs_rtmp_create(m_streming_url_.c_str());
		if( m_lpSRSRtmp == NULL ) {
			MsgLogINFO("== LibRtmp::Open srs_rtmp_create error ==");
			bError = true; break;
		}
		// 简单握手协议...
		if ((nErrCode = srs_rtmp_handshake(m_lpSRSRtmp)) != 0) {
			MsgLogGM(nErrCode);
			bError = true; break;
		}
		// 连接直播服务器...
		if ((nErrCode = srs_rtmp_connect_app(m_lpSRSRtmp)) != 0) {
			MsgLogGM(nErrCode);
			bError = true; break;
		}
		// 推流和拉流调用的接口不同...
		if( m_is_need_push_ ) {
			// 推流调用发布接口...
			if ((nErrCode = srs_rtmp_publish_stream(m_lpSRSRtmp)) != 0) {
				MsgLogGM(nErrCode);
				bError = true; break;
			}
		} else {
			// 拉流调用播放接口...
			if ((nErrCode = srs_rtmp_play_stream(m_lpSRSRtmp)) != 0) {
				MsgLogGM(nErrCode);
				bError = true; break;
			}
		}
	}while( false );
	// 连接服务器失败，销毁对象...
	if( bError ) {
		srs_rtmp_destroy(m_lpSRSRtmp);
		m_lpSRSRtmp = NULL;
		return false;
	}
	// 连接成功，开始读取数据...
	return true;
}

void LibRtmp::Close()
{
	// 释放拉流对象...
	srs_rtmp_destroy(m_lpSRSRtmp);
	m_lpSRSRtmp = NULL;
}

bool LibRtmp::IsClosed()
{
	return ((m_lpSRSRtmp == NULL) ? true : false);
}

bool LibRtmp::Read()
{
	// 准备读取数据需要的变量...
	int ret = ERROR_SUCCESS;
    int size = 0; char type = 0;
    char* data = NULL; u_int32_t timestamp = 0;
	// 拉取数据失败，直接返回...
	ret = srs_rtmp_read_packet(m_lpSRSRtmp, &type, &timestamp, &data, &size);
	if( ret != 0 ) {
		if( data != NULL ) {
			free(data);
		}
		CUtilTool::MsgLog(kTxtLogger, "== LibRtmp::Read error(%d) ==\r\n", ret);
		return false;
	}
	// 判断读取数据的有效性...
	if( size <= 0 ) {
		if( data != NULL ) {
			free(data);
		}
		CUtilTool::MsgLog(kTxtLogger, "== LibRtmp::Read size(%d) ==\r\n", size);
		return false;
	}
	// 对收到的数据包进行分发处理...
	bool is_ok = false;
	switch( type )
	{
	case SRS_RTMP_TYPE_VIDEO:  is_ok = this->doVideo(timestamp, data, size); break;
	case SRS_RTMP_TYPE_AUDIO:  is_ok = this->doAudio(timestamp, data, size); break;
	case SRS_RTMP_TYPE_SCRIPT: is_ok = this->doScript(timestamp, data, size); break;
	default:				   is_ok = this->doInvoke(timestamp, data, size); break;
	}
	// 释放读取到的数据区...
	if( data != NULL ) {
		free(data);
	}
	// 返回上面的执行结果...
	return is_ok;
}
//
// 处理音频数据...
bool LibRtmp::doAudio(DWORD dwTimeStamp, char * lpData, int nSize)
{
	// 获取数据包的类型 => < 0(错误), = 0(序列头), > 0(帧数据)
	char thePackType = srs_utils_flv_audio_aac_packet_type(lpData, nSize);
	if( thePackType < 0 )
		return false;
	// 处理音频序列头，会发送多次...
	if( thePackType == 0 ) {
		if( m_strAAC.size() <= 0 ) {
			m_strAAC.assign(lpData, nSize);
			this->ParseAACSequence(lpData, nSize);
		}
		ASSERT( m_strAAC.size() > 0 );
		return true;
	}
	// 处理音频帧数据内容...
	ASSERT( thePackType > 0 );

	// 如果外部录像线程有效，则组帧向外通知...
	/*if( m_lpRecThread != NULL ) {
		// 先去掉外层的数据壳...
		BOOL   bWriteFlag = false;
		BYTE * lpFrame = (BYTE*)lpData + 2;
		int    nDataSize = nSize - 2;
		// 存储音频数据帧...
		bWriteFlag = m_lpRecThread->WriteSample(false, lpFrame, nDataSize, dwTimeStamp, 0, true);
		// 打印调试信息...
		//TRACE("Audio, Size = %d, Time = %d\r\n", nSize, dwTimeStamp);
	}*/
	
	// 如果外部拉流线程有效，则组帧向外通知...
	if( m_lpRtmpThread != NULL ) {
		// 组合一个音频帧，通知上层...
		FMS_FRAME	theFrame;
		theFrame.typeFlvTag = FLV_TAG_TYPE_AUDIO;
		theFrame.dwSendTime = dwTimeStamp;
		theFrame.dwRenderOffset = 0;
		theFrame.is_keyframe = true;

		// 这里需要去掉一个2字节外壳...
		theFrame.strData.assign(lpData+2, nSize-2);

		// 推送数据帧给rtmp线程...
		int nFrameCount = m_lpRtmpThread->PushFrame(theFrame);
		// 缓存了一部分数据之后，才能启动线程...
		if( !m_bPushStartOK && nFrameCount >= 100 ) {
			m_lpRtmpThread->StartPushThread();
			m_bPushStartOK = true;
		}
	}
	return true;
}
//
// 处理视频数据...
bool LibRtmp::doVideo(DWORD dwTimeStamp, char * lpData, int nSize)
{
	// 获取数据包的类型 => < 0(错误), = 0(序列头), > 0(帧数据)
	char thePackType = srs_utils_flv_video_avc_packet_type(lpData, nSize);
	if( thePackType < 0 )
		return false;
	// 处理视频序列头，会发送多次...
	if( thePackType == 0 ) {
		if( m_strAVC.size() <= 0 ) {
			m_strAVC.assign(lpData, nSize);
			this->ParseAVCSequence(lpData, nSize);
		}
		ASSERT( m_strAVC.size() > 0 );
		return true;
	}
	// 处理视频帧数据内容...
	ASSERT( thePackType > 0 );

	// 如果外部录像线程有效，则组帧向外通知...
	/*if( m_lpRecThread != NULL ) {
		// 获取外层壳的数据...
		BYTE nFlag = BytesToUI08(lpData);
		BYTE nType = BytesToUI08(lpData+1);
		int  nOffset = BytesToUI24(lpData+2);
		// 先去掉外层的数据壳...
		BOOL   bWriteFlag = false;
		BYTE * lpFrame = (BYTE*)lpData + 5;
		int    nDataSize = nSize - 5;
		BOOL   bKeyFrame = ((lpData[0] == 0x17) ? true : false);
		// 存储视频数据帧...
		bWriteFlag = m_lpRecThread->WriteSample(true, lpFrame, nDataSize, dwTimeStamp, nOffset, bKeyFrame);
		// 打印调试信息...
		//TRACE("Video, Size = %d, Time = %d\r\n", nSize, dwTimeStamp);
	}*/

	// 如果外部拉流线程有效，则组帧向外通知...
	if( m_lpRtmpThread != NULL ) {
		// 获取外层壳的数据...
		BYTE nFlag = BytesToUI08(lpData);
		BYTE nType = BytesToUI08(lpData+1);
		int  nOffset = BytesToUI24(lpData+2);
		// 组合一个视频帧，通知上层... FLV_TAG_TYPE_VIDEO
		FMS_FRAME	theFrame;
		theFrame.typeFlvTag = FLV_TAG_TYPE_VIDEO;
		theFrame.dwSendTime = dwTimeStamp;
		theFrame.dwRenderOffset = nOffset;
		theFrame.is_keyframe = ((lpData[0] == 0x17) ? true : false);

		// 这里需要去掉一个5字节外壳...
		theFrame.strData.assign(lpData+5, nSize-5);

		// 推送数据帧给rtmp线程...
		int nFrameCount = m_lpRtmpThread->PushFrame(theFrame);
		// 缓存了一部分数据之后，才能启动线程...
		if( !m_bPushStartOK && nFrameCount >= 100 ) {
			m_lpRtmpThread->StartPushThread();
			m_bPushStartOK = true;
		}
	}

	return true;
}
//
// 处理脚本数据...
bool LibRtmp::doScript(DWORD dwTimeStamp, char * lpData, int nSize)
{
	return true;
}
//
// 处理其它数据...
bool LibRtmp::doInvoke(DWORD dwTimeStamp, char * lpData, int nSize)
{
	return true;
}
//
// 解析音频序列头信息...
void LibRtmp::ParseAACSequence(char * inBuf, int nSize)
{
	bs_t	s = {0};
	s.p		  = (uint8_t *)inBuf;
	s.p_start = (uint8_t *)inBuf;
	s.p_end	  = (uint8_t *)inBuf + nSize;
	s.i_left  = 8; // 这个是固定的,对齐长度...

	// 保存音频的 AES 信息...
	m_strAES.assign(inBuf+2, nSize-2);

	//int soundformat = _bs_read(&s, 4);
	//int soundrate = _bs_read(&s, 2);
	//int soundsize = _bs_read(&s, 1);
	//int soundtype = _bs_read(&s, 1);
	
	int aac_flag = _bs_read(&s, 8);
	int aac_type = _bs_read(&s, 8);

	// 获取音频的采样率索引和声道数...
	int object_type = _bs_read(&s, 5);
	int sample_rate_index = _bs_read(&s, 4);
	int source_channel_ = _bs_read(&s, 4);
	int	audio_sample_rate = 48000;		// 音频采样率
  
	// 根据索引获取采样率...
	switch( sample_rate_index )
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

	// 通知录像模块音频头到达了...
	/*if( m_lpRecThread != NULL ) {
		m_lpRecThread->CreateAudioTrack(audio_sample_rate, source_channel_);
	}*/

	// 通知上层音频头到达了...
	if( m_lpRtmpThread != NULL ) {
		m_lpRtmpThread->WriteAACSequenceHeader(audio_sample_rate, source_channel_);
	}
}
//
// 解析视频序列头信息...
void LibRtmp::ParseAVCSequence(char * inBuf, int nSize)
{
	bs_t	s = {0};
	s.p		  = (uint8_t *)inBuf;
	s.p_start = (uint8_t *)inBuf;
	s.p_end	  = (uint8_t *)inBuf + nSize;
	s.i_left  = 8; // 这个是固定的,对齐长度...

	int frametype = _bs_read(&s, 4);
	int codecid   = _bs_read(&s, 4);
	int pack_type = _bs_read(&s, 8);
	int pack_time = _bs_read(&s, 24);

	int configurationVersion  = _bs_read(&s, 8);
	int AVCProfileIndication  = _bs_read(&s, 8);
	int profile_compatibility = _bs_read(&s, 8);
	int AVCLevelIndication    = _bs_read(&s, 8);
	int reserved_1			  = _bs_read(&s, 8);
	int reserved_2			  = _bs_read(&s, 8);
	int sps_size_			  = _bs_read(&s, 16);

	// 获取 SPS 信息...
	m_strSPS.assign((char*)s.p, sps_size_);
	s.p += sps_size_;
	s.i_left = 8;

	int nNumPPS   = _bs_read(&s, 8);
	int pps_size_ = _bs_read(&s, 16);
	
	// 获取 PPS 信息...
	m_strPPS.assign((char*)s.p, pps_size_);

	// 通知上层视频头到达了...
	/*if( m_lpRecThread != NULL ) {
		m_lpRecThread->CreateVideoTrack(m_strSPS, m_strPPS);
	}*/

	// 通知上层视频头到达了...
	if( m_lpRtmpThread != NULL ) {
		m_lpRtmpThread->WriteAVCSequenceHeader(m_strSPS, m_strPPS);
	}
}
//
// 直接调用SRS提供的接口...
bool LibRtmp::Send(const char* data, int size, int type, unsigned int timestamp)
{
	// 判断输入数据的有效性...
	int nResult = ERROR_SUCCESS;
	if( m_lpSRSRtmp == NULL || data == NULL || size <= 0 )
		return false;
	// SRS底层会自动释放内存，因此，这里需要重新new或malloc内存出来...
	char * lpPacket = (char*)malloc(size);
	memcpy(lpPacket, data, size);
	// 直接调用 SRS 的发送接口...
	nResult = srs_rtmp_write_packet(m_lpSRSRtmp, type, timestamp, lpPacket, size);
	// 打印错误编号信息...
	if( nResult != 0 ) {
		MsgLogGM(nResult);
		return false;
	}
	// 返回正确...
	return true;
}
