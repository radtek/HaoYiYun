
#include "StdAfx.h"
#include "LibMP4.h"

LibMP4::LibMP4(void)
{
	m_hFileHandle = MP4_INVALID_FILE_HANDLE;
	m_videoID = MP4_INVALID_TRACK_ID;
	m_audioID = MP4_INVALID_TRACK_ID;
	m_nVideoTimeScale = 0;
	m_bFirstFrame = true;
	
	m_VLastFrame.m_nTimeStamp = 0;
	m_VLastFrame.m_strData.clear();
	m_VLastFrame.m_bKeyFrame = false;
}

LibMP4::~LibMP4(void)
{
	this->Close();
}
//
// 创建视频轨道...
bool LibMP4::CreateVideoTrack(const char* lpUTF8Name, int nVideoTimeScale, int width, int height, string & strSPS, string & strPPS)
{
	if( strSPS.size() <= 0 || strPPS.size() <= 0 || lpUTF8Name == NULL )
		return false;
	// 如果句柄为空，则创建MP4对象...
	if( m_hFileHandle == MP4_INVALID_FILE_HANDLE ) {
		m_hFileHandle = MP4Create(lpUTF8Name);
	}
	// 再次判断文件句柄...
	if( m_hFileHandle == MP4_INVALID_FILE_HANDLE )
		return false;
	// 设置TimeScale，时间刻度，通常为任意的固定数值...
	BYTE * lpSPS = (BYTE*)strSPS.c_str();
	uint32_t theTimeScale = nVideoTimeScale;
	MP4SetTimeScale(m_hFileHandle, theTimeScale);
	// 添加视频轨道, 不要用固定时间间隔，用每帧的时间间隔...
	// 这里的是视频时间间隔，一定要用 MP4_INVALID_DURATION，计算两帧之间的时间差的方案...
	// 这种方案可以满足各种信号来源的数据，用 fps 方式不精确...(2015.10.17)
	m_videoID = MP4AddH264VideoTrack(m_hFileHandle, 
									theTimeScale, 
									MP4_INVALID_DURATION, //theTimeScale/fps,
									width, height, 
									lpSPS[1],
									lpSPS[2],
									lpSPS[3],
									3);
	if( m_videoID == MP4_INVALID_TRACK_ID ) {
		MP4Close(m_hFileHandle);
		m_hFileHandle = MP4_INVALID_FILE_HANDLE;
		return false;
	}
	// 设置视频level...
	MP4SetVideoProfileLevel(m_hFileHandle, 0x7F);

	// 设置SPS/PPS...
	MP4AddH264SequenceParameterSet(m_hFileHandle, m_videoID, (BYTE*)strSPS.c_str(), strSPS.size());
	MP4AddH264PictureParameterSet(m_hFileHandle, m_videoID, (BYTE*)strPPS.c_str(), strPPS.size());

	m_nVideoTimeScale = nVideoTimeScale;

	return true;
}
//
// 创建音频轨道...
bool LibMP4::CreateAudioTrack(const char* lpUTF8Name, int nAudioSampleRate, string & strAES)
{
	if( strAES.size() <= 0 || lpUTF8Name == NULL )
		return false;
	// 如果句柄为空，则创建MP4对象...
	if( m_hFileHandle == MP4_INVALID_FILE_HANDLE ) {
		m_hFileHandle = MP4Create(lpUTF8Name);
	}
	// 再次判断文件句柄...
	if( m_hFileHandle == MP4_INVALID_FILE_HANDLE )
		return false;

	// 添加音频轨道，音频时间刻度固定为 1024，否则会出现问题...
	// AAC 都是采用固定的刻度运转的...(2015.10.17)
	m_audioID = MP4AddAudioTrack(m_hFileHandle, nAudioSampleRate, 1024, MP4_MPEG4_AUDIO_TYPE);
	if( m_audioID == MP4_INVALID_TRACK_ID ) {
		MP4Close(m_hFileHandle);
		m_hFileHandle = MP4_INVALID_FILE_HANDLE;
		return false;
	}
	
	// 设置音频 level 和 AES...
	MP4SetAudioProfileLevel(m_hFileHandle, 0x02);//0xFF);
	MP4SetTrackESConfiguration(m_hFileHandle, m_audioID, (BYTE*)strAES.c_str(), strAES.size());

	return true;
}

bool LibMP4::WriteSample(bool bIsVideo, BYTE * lpFrame, int nSize, uint32_t inTimeStamp, bool bIsKeyFrame)
{
	if( m_hFileHandle == MP4_INVALID_FILE_HANDLE || lpFrame == NULL || nSize <= 0 )
		return false;
	if( bIsVideo && m_videoID == MP4_INVALID_TRACK_ID )
		return false;
	if( !bIsVideo && m_audioID == MP4_INVALID_TRACK_ID )
		return false;
	// 第一帧数据必须是视频关键帧...
	if( m_bFirstFrame ) {
		// 如果是音频，将数据帧缓存起来...
		// 注意：这里不能直接存音频，如果有视频，则会出现音视频不同步，因此，需要先缓存，如果确实没有视频再存盘。
		if( !bIsVideo ) {
			RTMPFrame theFrame;
			theFrame.m_bKeyFrame = bIsKeyFrame;
			theFrame.m_strData.assign((char*)lpFrame, nSize);
			theFrame.m_nTimeStamp = inTimeStamp;
			m_deqAudio.push_back(theFrame);
			// 如果缓存了500帧之后，还没有视频，则认为只有音频，直接存盘...
			if( m_deqAudio.size() >= 500 ) {
				// 设置非第一帧标志...
				m_bFirstFrame = false;
				KH_DeqFrame::iterator itor;
				// 开始存储缓存的音频数据帧，使用固定的帧时间间隔...
				for(itor = m_deqAudio.begin(); itor != m_deqAudio.end(); ++itor) {
					RTMPFrame & myFrame = (*itor);
					MP4WriteSample(m_hFileHandle, m_audioID, (BYTE*)myFrame.m_strData.c_str(), myFrame.m_strData.size(), MP4_INVALID_DURATION, 0, myFrame.m_bKeyFrame);
				}
				// 存盘之后，释放音频缓存...
				TRACE("[No Video] Audio-Deque = %d\n", m_deqAudio.size());
				m_deqAudio.clear();
			}
			return true;
		}
		// 视频非关键帧，直接丢弃...
		if( !bIsKeyFrame )
			return true;
		// 设置非第一帧标志...
		m_bFirstFrame = false;
		// 之前缓存的音频数据，直接丢弃，否则会出现音视频不同步...
		TRACE("[Has Video] Audio-Deque = %d\n", m_deqAudio.size());
		m_deqAudio.clear();
	}
	// 准备一些共同的数据...
	MP4TrackId  theTrackID = bIsVideo ? m_videoID : m_audioID;
	bool		bWriteFlag = false;
	if( bIsVideo ) { 
		// 判断是否需要保存第一帧...
		if( m_VLastFrame.m_strData.size() <= 0 ) {
			m_VLastFrame.m_bKeyFrame = bIsKeyFrame;
			m_VLastFrame.m_strData.assign((char*)lpFrame, nSize);
			m_VLastFrame.m_nTimeStamp = inTimeStamp;
			return true;
		}
		// 准备需要的数据内容...
		int			uFrameMS = inTimeStamp - m_VLastFrame.m_nTimeStamp;
		MP4Duration uDuration = uFrameMS * m_nVideoTimeScale / 1000;
		if( uFrameMS <= 0 ) uDuration = 1;
		//TRACE("Video-Duration = %I64d, size = %lu, keyFrame = %d\n", uDuration, m_VLastFrame.m_strData.size(), m_VLastFrame.m_bKeyFrame);
		
		// 调用写入帧的接口函数，视频需要计算帧间隔...
		bWriteFlag = MP4WriteSample(m_hFileHandle, theTrackID, (BYTE*)m_VLastFrame.m_strData.c_str(), m_VLastFrame.m_strData.size(), uDuration, 0, m_VLastFrame.m_bKeyFrame);
		
		// 保存这一帧的数据，下次使用...
		m_VLastFrame.m_bKeyFrame = bIsKeyFrame;
		m_VLastFrame.m_strData.assign((char*)lpFrame, nSize);
		m_VLastFrame.m_nTimeStamp = inTimeStamp;
	} else {
		// 音频数据，直接调用接口写盘，音频不用计算帧间隔时间，采用固定的帧间隔...
		bWriteFlag = MP4WriteSample(m_hFileHandle, theTrackID, lpFrame, nSize, MP4_INVALID_DURATION, 0, bIsKeyFrame);
	}
	// 返回存盘结果...
	return bWriteFlag;
}
//
// 关闭录像文件...
bool LibMP4::Close()
{
	if( m_hFileHandle != MP4_INVALID_FILE_HANDLE ) {
		MP4Close(m_hFileHandle);
		m_hFileHandle = MP4_INVALID_FILE_HANDLE;
	}
	return true;
}
//
// 返回文件中播放时间(毫秒)...
MP4Duration LibMP4::GetDuration()
{
	MP4Duration nDuration = 0;
	if( m_hFileHandle != MP4_INVALID_FILE_HANDLE ) {
		nDuration = MP4GetDuration(m_hFileHandle);
	}
	return nDuration;
}
