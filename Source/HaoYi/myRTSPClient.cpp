
#include "StdAfx.h"
#include "myRTSPClient.h"
#include "PushThread.h"

#include "libmp4v2\RecThread.h"
#include "librtmp\AmfByteStream.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

void continueAfterOPTIONS(RTSPClient* rtspClient, int resultCode, char* resultString)
{
	ASSERT( rtspClient != NULL );
	((ourRTSPClient*)rtspClient)->myAfterOPTIONS(resultCode, resultString);
	delete[] resultString;
}

void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString)
{
	ASSERT( rtspClient != NULL );
	((ourRTSPClient*)rtspClient)->myAfterDESCRIBE(resultCode, resultString);
	delete[] resultString;
}

void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString)
{
	ASSERT( rtspClient != NULL );
	((ourRTSPClient*)rtspClient)->myAfterSETUP(resultCode, resultString);
	delete[] resultString;
}

void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString)
{
	ASSERT( rtspClient != NULL );
	((ourRTSPClient*)rtspClient)->myAfterPLAY(resultCode, resultString);
	delete[] resultString;
}

StreamClientState::StreamClientState()
  : m_iter(NULL),
    m_session(NULL),
	m_subsession(NULL),
	m_streamTimerTask(NULL),
	m_duration(0.0)
{
}

StreamClientState::~StreamClientState()
{
	if( m_iter != NULL ) {
		delete m_iter;
		m_iter = NULL;
	}

	// We also need to delete "session", and unschedule "streamTimerTask" (if set)
	if( m_session != NULL ) {
		UsageEnvironment& env = m_session->envir(); // alias
		env.taskScheduler().unscheduleDelayedTask(m_streamTimerTask);
		Medium::close(m_session);
	}
}

ourRTSPClient* ourRTSPClient::createNew(UsageEnvironment& env, char const* rtspURL,	int verbosityLevel, char const* applicationName, CRtspThread * lpRtspThread, CRtspRecThread * lpRecThread)
{
	return new ourRTSPClient(env, rtspURL, verbosityLevel, applicationName, lpRtspThread, lpRecThread);
}

ourRTSPClient::ourRTSPClient(UsageEnvironment& env, char const* rtspURL, int verbosityLevel, char const* applicationName, CRtspThread * lpRtspThread, CRtspRecThread * lpRecThread)
  : RTSPClient(env, rtspURL, verbosityLevel, applicationName, 0, -1),
    m_lpRtspThread(lpRtspThread),
	m_lpRecThread(lpRecThread),
	m_bHasStart(false),
	m_bHasVideo(false),
	m_bHasAudio(false),
	m_audio_channels(0),
	m_audio_rate(0)
{
	m_strSPS.clear();
	m_strPPS.clear();
}

ourRTSPClient::~ourRTSPClient()
{
}

void ourRTSPClient::myAfterOPTIONS(int resultCode, char* resultString)
{
	do {
		// 发生错误，打印返回...
		if( resultCode != 0 ) {
			TRACE("[OPTIONS] Code = %d, Error = %s\n", resultCode, resultString);
			break;
		}
		// 成功，发起DESCRIBE请求...
		TRACE("[OPTIONS] = %s\n", resultString);
		this->sendDescribeCommand(continueAfterDESCRIBE);
		return;
	}while( 0 );

	// 发生错误，让任务循环退出，然后自动停止会话...
	if( m_lpRtspThread != NULL ) {
		m_lpRtspThread->ResetEventLoop();
	}
	// 发生错误，停止录像线程...
	if( m_lpRecThread != NULL ) {
		m_lpRecThread->ResetEventLoop();
	}
}

void ourRTSPClient::myAfterDESCRIBE(int resultCode, char* resultString)
{
	do {
		// 获取环境变量...
		UsageEnvironment & env = this->envir();
		StreamClientState & scs = this->m_scs;

		// 打印获取的SDP信息...
		TRACE("[SDP] = %s\n", resultString);

		// 返回错误，退出...
		if( resultCode != 0 ) {
			TRACE("[DESCRIBE] Code = %d, Error = %s\n", resultCode, resultString);
			break;
		}
		
		// 用获取的SDP创建会话...
		scs.m_session = MediaSession::createNew(env, resultString);

		// 判断创建会话的结果...
		if( scs.m_session == NULL ) {
			TRACE("[DESCRIBE] Error = %s\n", env.getResultMsg());
			break;
		} else if ( !scs.m_session->hasSubsessions() ) {
			TRACE("[DESCRIBE] Error = This session has no media subsessions\n");
			break;
		}
		
		// Then, create and set up our data source objects for the session.  We do this by iterating over the session's 'subsessions',
		// calling "MediaSubsession::initiate()", and then sending a RTSP "SETUP" command, on each one.
		// (Each 'subsession' will have its own data source.)
		// 发起下一个SETUP握手操作...
		scs.m_iter = new MediaSubsessionIterator(*scs.m_session);
		this->setupNextSubsession();
		return;
	} while (0);
	
	// 发生错误，让任务循环退出，然后自动停止会话...
	if( m_lpRtspThread != NULL ) {
		m_lpRtspThread->ResetEventLoop();
	}
	// 发生错误，停止录像线程...
	if( m_lpRecThread != NULL ) {
		m_lpRecThread->ResetEventLoop();
	}
}
//
// 这里可以判断，音视频格式是否符合要求...
// 音视频会各自出现一次，然后才进入到 play 状态...
void ourRTSPClient::myAfterSETUP(int resultCode, char* resultString)
{
	do {
		// 获取环境变量...
		UsageEnvironment & env = this->envir();
		StreamClientState & scs = this->m_scs;
		
		// 返回错误，退出...
		if( resultCode != 0 ) {
			TRACE("[%s/%s] Failed to Setup.\n", scs.m_subsession->mediumName(), scs.m_subsession->codecName());
			break;
		}

		// 判断视频格式是否正确，必须是 video/H264 ...
		if( strcmp(scs.m_subsession->mediumName(), "video") == 0 ) {
			if( strcmp(scs.m_subsession->codecName(), "H264") != 0 ) {
				TRACE("[%s/%s] Error => Must be Video/H264.\n", scs.m_subsession->mediumName(), scs.m_subsession->codecName());
				break;
			}
			// 必须有 SPS 和 PPS 的数据包...
			ASSERT( strcmp(scs.m_subsession->codecName(), "H264") == 0 );
			const char * lpszSpro = scs.m_subsession->fmtp_spropparametersets();
			if( lpszSpro == NULL ) {
				TRACE("[%s/%s] Error => SPS or PPS...\n", scs.m_subsession->mediumName(), scs.m_subsession->codecName());
				break;
			}
			// 获得第一个 SPS 和 第一个PPS ...
			ASSERT( lpszSpro != NULL );
			unsigned numSPropRecords = 0;
			SPropRecord * sPropRecords = parseSPropParameterSets(lpszSpro, numSPropRecords);
			for(unsigned i = 0; i < numSPropRecords; ++i) {
				if( i == 0 && m_strSPS.size() <= 0 ) {
					m_strSPS.assign((char*)sPropRecords[i].sPropBytes, sPropRecords[i].sPropLength);
				}
				if( i == 1 && m_strPPS.size() <= 0 ) {
					m_strPPS.assign((char*)sPropRecords[i].sPropBytes, sPropRecords[i].sPropLength);
				}
			}
			delete[] sPropRecords;
			// 必须同时包含 SPS 和 PPS...
			if( m_strSPS.size() <= 0 || m_strPPS.size() <= 0 ) {
				TRACE("[%s/%s] Error => SPS or PPS...\n", scs.m_subsession->mediumName(), scs.m_subsession->codecName());
				break;
			}
			ASSERT( m_strSPS.size() > 0 && m_strPPS.size() > 0 );
			// 通知rtsp线程，格式头已经准备好了...
			/*if( m_lpRtspThread != NULL ) {
				m_lpRtspThread->WriteAVCSequenceHeader(strSPS, strPPS);
			}
			// 通知录像线程，格式头已经准备好了...
			if( m_lpRecThread != NULL ) {
				m_lpRecThread->CreateVideoTrack(strSPS, strPPS);
			}*/
			// 保存视频标志...
			m_bHasVideo = true;
		}

		// 判断音频格式是否正确, 必须是 audio/MPEG4 ...
		if( strcmp(scs.m_subsession->mediumName(), "audio") == 0 ) {
			if( strnicmp(scs.m_subsession->codecName(), "MPEG4", strlen("MPEG4")) != 0 ) {
				TRACE("[%s/%s] Error => Must be Audio/MPEG4.\n", scs.m_subsession->mediumName(), scs.m_subsession->codecName());
				break;
			}
			ASSERT( strnicmp(scs.m_subsession->codecName(), "MPEG4", strlen("MPEG4")) == 0 );
			// 获取声道数和采样率信息...
			m_audio_channels = scs.m_subsession->numChannels();
			m_audio_rate = scs.m_subsession->rtpTimestampFrequency();
			if( m_audio_channels <= 0 || m_audio_rate <= 0 ) {
				TRACE("[%s/%s] Error => channel(%d),rate(%d).\n", scs.m_subsession->mediumName(), scs.m_subsession->codecName(), m_audio_channels, m_audio_rate);
				break;
			}
			// 通知rtsp线程，格式头已经准备好了...
			/*if( m_lpRtspThread != NULL ) {
				m_lpRtspThread->WriteAACSequenceHeader(audio_rate, audio_channels);
			}
			// 通知录像线程，格式头已经准备好了...
			if( m_lpRecThread != NULL ) {
				m_lpRecThread->CreateAudioTrack(audio_rate, audio_channels);
			}*/
			// 保存音频标志...
			m_bHasAudio = true;
		}
		
		// 打印正确信息...
		TRACE("[%s/%s] Setup OK.\n", scs.m_subsession->mediumName(), scs.m_subsession->codecName());
		if( scs.m_subsession->rtcpIsMuxed() ) {
			TRACE("[client port] %d \n", scs.m_subsession->clientPortNum());
		} else {
			TRACE("[client ports] %d - %d \n", scs.m_subsession->clientPortNum(), scs.m_subsession->clientPortNum()+1);
		}
		
		// Having successfully setup the subsession, create a data sink for it, and call "startPlaying()" on it.
		// (This will prepare the data sink to receive data; the actual flow of data from the client won't start happening until later,
		// after we've sent a RTSP "PLAY" command.)

		// 创建新的帧数据处理对象...		
		// perhaps use your own custom "MediaSink" subclass instead
		ASSERT( m_lpRtspThread != NULL || m_lpRecThread != NULL );
		scs.m_subsession->sink = DummySink::createNew(env, *scs.m_subsession, this->url(), this);

		// 创建失败，打印错误信息...
		if( scs.m_subsession->sink == NULL ) {
			TRACE("[%s/%s] Error = %s. \n", scs.m_subsession->mediumName(), scs.m_subsession->codecName(), env.getResultMsg());
			break;
		}
		
		TRACE("[%s/%s] Created a data sink ok. \n", scs.m_subsession->mediumName(), scs.m_subsession->codecName());

		// 发起 PLAY 协议...
		scs.m_subsession->miscPtr = this;
		scs.m_subsession->sink->startPlaying(*(scs.m_subsession->readSource()), subsessionAfterPlaying, scs.m_subsession);

		// Also set a handler to be called if a RTCP "BYE" arrives for this subsession:
		if( scs.m_subsession->rtcpInstance() != NULL ) {
			scs.m_subsession->rtcpInstance()->setByeHandler(subsessionByeHandler, scs.m_subsession);
		}

		// Set up the next subsession, if any:
		this->setupNextSubsession();
		return;
	} while (0);

	// 发生错误，让任务循环退出，然后自动停止会话...
	if( m_lpRtspThread != NULL ) {
		m_lpRtspThread->ResetEventLoop();
	}
	// 发生错误，停止录像线程...
	if( m_lpRecThread != NULL ) {
		m_lpRecThread->ResetEventLoop();
	}
}
//
// 启动推流或录像线程...
BOOL ourRTSPClient::doStartEvent(string & inNewSPS, string & inNewPPS, BOOL bIsFromAudio)
{
	// 如果是通过音频启动的，但是有视频，直接返回false，等待视频启动...
	if( bIsFromAudio && m_bHasVideo ) {
		m_bHasStart = false;
		return m_bHasStart;
	}
	// 如果是通过音频启动的，但没有视频，SPS和PPS一定为空，可以启动...
	if( bIsFromAudio && !m_bHasVideo ) {
		ASSERT( inNewSPS.size() <= 0 );
		ASSERT( inNewPPS.size() <= 0 );
	}
	// 用新的SPS和PPS替换旧的SPS和PPS...
	m_strSPS = inNewSPS; m_strPPS = inNewPPS;
	// 根据音视频格式头的有效性，创建对应的音视频节点或轨道...
	// 通知rtsp线程，格式头已经准备好了...
	if( m_lpRtspThread != NULL ) {
		// 创建视频格式数据头...
		if( m_strSPS.size() > 0 && m_strPPS.size() > 0 ) {
			m_lpRtspThread->WriteAVCSequenceHeader(m_strSPS, m_strPPS);
		}
		// 创建音频格式数据头...
		if( m_audio_channels > 0 && m_audio_rate > 0 ) {
			m_lpRtspThread->WriteAACSequenceHeader(m_audio_rate, m_audio_channels);
		}
		// 启动rtmp推送线程...
		m_lpRtspThread->StartPushThread();
	}
	// 通知录像线程，格式头已经准备好了...
	if( m_lpRecThread != NULL ) {
		// 创建视频轨道对象...
		if( m_strSPS.size() > 0 && m_strPPS.size() > 0 ) {
			m_lpRecThread->CreateVideoTrack(m_strSPS, m_strPPS);
		}
		// 创建音频轨道对象...
		if( m_audio_channels > 0 && m_audio_rate > 0 ) {
			m_lpRecThread->CreateAudioTrack(m_audio_rate, m_audio_channels);
		}
	}
	// 启动成功，返回结果...
	m_bHasStart = true;
	return m_bHasStart;
}
//
// 进行数据帧的处理...
void ourRTSPClient::WriteSample(bool bIsVideo, string & inFrame, DWORD inTimeStamp, DWORD inRenderOffset, bool bIsKeyFrame)
{
	// 通知录像线程，保存一帧数据包...
	if( m_lpRecThread != NULL ) {
		m_lpRecThread->WriteSample(bIsVideo, (BYTE*)inFrame.c_str(), inFrame.size(), inTimeStamp, inRenderOffset, bIsKeyFrame);
	}
	// 构造音视频数据帧，推送给rtsp线程...
	if( m_lpRtspThread != NULL ) {
		FMS_FRAME	theFrame;
		theFrame.typeFlvTag = (bIsVideo ? FLV_TAG_TYPE_VIDEO : FLV_TAG_TYPE_AUDIO);	// 设置音视频标志
		theFrame.dwSendTime = inTimeStamp;
		theFrame.dwRenderOffset = inRenderOffset;
		theFrame.is_keyframe = bIsKeyFrame;
		theFrame.strData = inFrame;
		// 推送数据帧给rtsp线程...
		m_lpRtspThread->PushFrame(theFrame);
	}
}

void ourRTSPClient::myAfterPLAY(int resultCode, char* resultString)
{
	do {
		// 获取环境变量...
		UsageEnvironment & env = this->envir();
		StreamClientState & scs = this->m_scs;
		
		// 返回错误，打印信息...
		if( resultCode != 0 ) {
			TRACE("Failed to start playing session.\n");
			break;
		}
		
		// Set a timer to be handled at the end of the stream's expected duration (if the stream does not already signal its end
		// using a RTCP "BYE").  This is optional.  If, instead, you want to keep the stream active - e.g., so you can later
		// 'seek' back within it and do another RTSP "PLAY" - then you can omit this code.
		// (Alternatively, if you don't want to receive the entire stream, you could set this timer for some shorter value.)
		if( scs.m_duration > 0 ) {
			unsigned const delaySlop = 2; // number of seconds extra to delay, after the stream's expected duration.  (This is optional.)
			scs.m_duration += delaySlop;
			unsigned uSecsToDelay = (unsigned)(scs.m_duration*1000000);
			scs.m_streamTimerTask = env.taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)streamTimerHandler, this);
		}
		
		// 一切正常，打印信息，直接返回...
		TRACE("Started playing session.(for up to %.2f)\n", scs.m_duration);
		// 启动rtmp推送线程...
		//if( m_lpRtspThread != NULL ) {
		//	m_lpRtspThread->StartPushThread();
		//}
		return;
	} while (0);

	// 发生错误，让任务循环退出，然后自动停止会话...
	if( m_lpRtspThread != NULL ) {
		m_lpRtspThread->ResetEventLoop();
	}
	// 发生错误，停止录像线程...
	if( m_lpRecThread != NULL ) {
		m_lpRecThread->ResetEventLoop();
	}
}
//
// 发起下一个协议...
void ourRTSPClient::setupNextSubsession()
{
	// 获取环境变量信息...
	UsageEnvironment & env = this->envir();
	StreamClientState & scs = this->m_scs;

	// 获取会话对象...
	scs.m_subsession = scs.m_iter->next();
	if( scs.m_subsession != NULL ) {
		if( !scs.m_subsession->initiate() ) {
			// give up on this subsession; go to the next one
			TRACE("[%s/%s] Error = %s\n", scs.m_subsession->mediumName(), scs.m_subsession->codecName(), env.getResultMsg());
			this->setupNextSubsession();
		} else {
			TRACE("[%s/%s] OK\n", scs.m_subsession->mediumName(), scs.m_subsession->codecName());
			if( scs.m_subsession->rtcpIsMuxed() ) {
				TRACE("[client port] %d \n", scs.m_subsession->clientPortNum());
			} else {
				TRACE("[client ports] %d - %d \n", scs.m_subsession->clientPortNum(), scs.m_subsession->clientPortNum()+1);
			}
			// Continue setting up this subsession, by sending a RTSP "SETUP" command: 
			// REQUEST_STREAMING_OVER_TCP == True...
			// 发起SETUP协议，这里必须用TCP模式，否则有些服务器不支持RTP模式...
			this->sendSetupCommand(*scs.m_subsession, continueAfterSETUP, False, True);
		}
		return;
	}
	
	// 这里发起 PLAY 协议过程...
	// We've finished setting up all of the subsessions.  Now, send a RTSP "PLAY" command to start the streaming:
	if( scs.m_session->absStartTime() != NULL ) {
		// Special case: The stream is indexed by 'absolute' time, so send an appropriate "PLAY" command:
		this->sendPlayCommand(*scs.m_session, continueAfterPLAY, scs.m_session->absStartTime(), scs.m_session->absEndTime());
	} else {
		scs.m_duration = scs.m_session->playEndTime() - scs.m_session->playStartTime();
		this->sendPlayCommand(*scs.m_session, continueAfterPLAY);
	}
}
//
// 关闭会话过程...
void ourRTSPClient::shutdownStream()
{
	// 获取环境变量...
	UsageEnvironment & env = this->envir();
	StreamClientState & scs = this->m_scs;
	
	// First, check whether any subsessions have still to be closed:
	if( scs.m_session != NULL ) { 
		Boolean someSubsessionsWereActive = False;
		MediaSubsessionIterator iter(*scs.m_session);
		MediaSubsession* subsession = NULL;
		
		while( (subsession = iter.next()) != NULL ) {
			if( subsession->sink != NULL ) {
				Medium::close(subsession->sink);
				subsession->sink = NULL;
				
				// in case the server sends a RTCP "BYE" while handling "TEARDOWN"
				if( subsession->rtcpInstance() != NULL ) {
					subsession->rtcpInstance()->setByeHandler(NULL, NULL);
				}
				someSubsessionsWereActive = True;
			}
		}
		
		// Send a RTSP "TEARDOWN" command, to tell the server to shutdown the stream.
		// Don't bother handling the response to the "TEARDOWN".
		if( someSubsessionsWereActive ) {
			this->sendTeardownCommand(*scs.m_session, NULL);
		}
	}
	
	// 关闭这个rtsp连接对象...
	TRACE("[%s] Closing the stream.\n", this->url());
	Medium::close(this);

	// Note that this will also cause this stream's "StreamClientState" structure to get reclaimed.
}

// Implementation of the other event handlers:
void subsessionAfterPlaying(void* clientData)
{
	MediaSubsession * subsession = (MediaSubsession*)clientData;
	ourRTSPClient * rtspClient = (ourRTSPClient*)(subsession->miscPtr);
	
	// Begin by closing this subsession's stream:
	Medium::close(subsession->sink);
	subsession->sink = NULL;
	
	// Next, check whether *all* subsessions' streams have now been closed:
	MediaSession& session = subsession->parentSession();
	MediaSubsessionIterator iter(session);
	while( (subsession = iter.next()) != NULL ) {
		if( subsession->sink != NULL )
			return; // this subsession is still active
	}
	
	// 让任务循环退出，然后自动停止会话...
	if( rtspClient->m_lpRtspThread != NULL ) {
		rtspClient->m_lpRtspThread->ResetEventLoop();
	}
	// 让录像线程退出...
	if( rtspClient->m_lpRecThread != NULL ) {
		rtspClient->m_lpRecThread->ResetEventLoop();
	}
}

void subsessionByeHandler(void* clientData)
{
	MediaSubsession * subsession = (MediaSubsession*)clientData;
	RTSPClient * rtspClient = (RTSPClient*)subsession->miscPtr;
	UsageEnvironment & env = rtspClient->envir(); // alias
	
	TRACE("Received RTCP BYTE on %s/%s \n", subsession->mediumName(), subsession->codecName());
	
	// Now act as if the subsession had closed:
	subsessionAfterPlaying(subsession);
}

void streamTimerHandler(void* clientData)
{
	ourRTSPClient* rtspClient = (ourRTSPClient*)clientData;
	StreamClientState& scs = rtspClient->m_scs;
	
	scs.m_streamTimerTask = NULL;
	
	// 让任务循环退出，然后自动停止会话...
	if( rtspClient->m_lpRtspThread != NULL ) {
		rtspClient->m_lpRtspThread->ResetEventLoop();
	}
	// 让录像线程退出...
	if( rtspClient->m_lpRecThread != NULL ) {
		rtspClient->m_lpRecThread->ResetEventLoop();
	}
}

#define DUMMY_SINK_RECEIVE_BUFFER_SIZE 1024 * 1024

DummySink* DummySink::createNew(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId, ourRTSPClient * lpRtspClient)
{
	return new DummySink(env, subsession, streamId, lpRtspClient);
}

DummySink::DummySink(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId, ourRTSPClient * lpRtspClient)
  : MediaSink(env), fSubsession(subsession), fRtspClient(lpRtspClient)
{
	m_strNewSPS.clear();
	m_strNewPPS.clear();
	ASSERT( fRtspClient != NULL );
	fStreamId = strDup(streamId);
	fReceiveBuffer = new u_int8_t[DUMMY_SINK_RECEIVE_BUFFER_SIZE];

	//fSTimeVal.tv_sec = fSTimeVal.tv_usec = 0;
	memset(&m_llTimCountFirst, 0, sizeof(m_llTimCountFirst));
}

DummySink::~DummySink()
{
	if( fReceiveBuffer != NULL ) {
		delete[] fReceiveBuffer;
		fReceiveBuffer = NULL;
	}
	if( fStreamId != NULL ) {
		delete[] fStreamId;
		fStreamId = NULL;
	}
}

void DummySink::afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
				  struct timeval presentationTime, unsigned durationInMicroseconds)
{
	DummySink* sink = (DummySink*)clientData;
	sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

void DoTextLog(LPCTSTR lpszLog)
{
	FILE * theFile = fopen("C:\\rtmpFlash\\logs\\GMPullerLog.txt", "a+");
	fputs(lpszLog, theFile);
	fclose(theFile);
}
//
// We've just received a frame of data.  (Optionally) print out information about it:
void DummySink::afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
				  struct timeval presentationTime, unsigned durationInMicroseconds)
{
	// 把第一帧时间戳作为起点时间...
	string			strFrame;
	uint32_t		dwTimeStamp = 0;
	ULARGE_INTEGER	llTimCountCur = {0};
	// 得到当前时间，时间单位是0.1微妙...
	::GetSystemTimeAsFileTime((FILETIME *)&llTimCountCur);
	if( m_llTimCountFirst.QuadPart <= 0 ) {
		m_llTimCountFirst.QuadPart = llTimCountCur.QuadPart;
	}
	// 计算时间戳（毫秒)，需要除以10000...
	dwTimeStamp = (llTimCountCur.QuadPart - m_llTimCountFirst.QuadPart)/10000;
	
	/*if((fSTimeVal.tv_sec <= 0) && (fSTimeVal.tv_usec <= 0)) {
		fSTimeVal = presentationTime;
	}
	// 计算时间戳(毫秒)，tv_sec是秒，tv_usec是微秒...
	dwTimeStamp = uint32_t((presentationTime.tv_sec - fSTimeVal.tv_sec)*1000.0f + (presentationTime.tv_usec - fSTimeVal.tv_usec)/1000.0f);*/

	//char szBuf[MAX_PATH] = {0};

	// 获取视频标志，关键帧标志...
	// H264的 nalu 标志类型1(片段),5(关键帧),7(SPS),8(PPS)...
	BOOL bIsVideo = false, bIsKeyFrame = false;
	if( strcmp(fSubsession.mediumName(), "video") == 0 ) {
		bIsVideo = true;
		// 计算关键帧标志...
		BYTE nalType = fReceiveBuffer[0] & 0x1f;
		// 2017.04.10 - by jackey => 如果是SPS或PPS，直接丢弃...
		// 否则会造成HTML5播放器在video标签中无法播放，通过MPlayer发现，写了多余的坏帧，刚好是3个...
		if( nalType > 5 ) {
			// 如果没有启动，进行启动处理...
			if( fRtspClient != NULL && !fRtspClient->IsHasStart() ) {
				// 保存数据区里的SPS...
				if( nalType == 7 ) {
					m_strNewSPS.assign((const char*)fReceiveBuffer, frameSize);
				}
				// 保存数据区里的PPS...
				if( nalType == 8 ) {
					m_strNewPPS.assign((const char*)fReceiveBuffer, frameSize);
				}
				// SPS与PPS获取完毕，通知推送线程...
				if( m_strNewSPS.size() > 0 && m_strNewPPS.size() > 0 ) {
					fRtspClient->doStartEvent(m_strNewSPS, m_strNewPPS, false);
				}
			}
			// 直接丢弃这些头部数据...
			this->continuePlaying();
			return;
		}
		// 设置关键帧标志...
		if( nalType == 5 ) {
			bIsKeyFrame = true;
		}
		// 将起始码0x00000001，替换为nalu的大小...
		char pNalBuf[4] = {0};
		UI32ToBytes(pNalBuf, frameSize);
		// 视频帧需要加入在前面加入帧的长度(4字节)...
		strFrame.assign(pNalBuf, sizeof(pNalBuf));
		strFrame.append((char*)fReceiveBuffer, frameSize);
		//TRACE("[%x][Video] TimeStamp = %lu, Size = %lu, KeyFrame = %d\n", fReceiveBuffer[0], dwTimeStamp, frameSize, bIsKeyFrame);
		//TRACE("[Video] TimeStamp = %lu, Size = %lu, KeyFrame = %d\n", dwTimeStamp, frameSize, bIsKeyFrame);
		//sprintf(szBuf, "[Video] TimeStamp = %lu, Size = %lu, KeyFrame = %d\n", dwTimeStamp, frameSize, bIsKeyFrame);
		//DoTextLog(szBuf);
	}
	// 获取音频标志，关键帧标志...
	if( strcmp(fSubsession.mediumName(), "audio") == 0 ) {
		bIsVideo = false; bIsKeyFrame = true;
		strFrame.assign((char*)fReceiveBuffer, frameSize);
		// 从音频数据帧去启动推流或录像线程...
		if( fRtspClient != NULL && !fRtspClient->IsHasStart() ) {
			fRtspClient->doStartEvent(m_strNewSPS, m_strNewPPS, true);
		}
		//TRACE("[Audio] TimeStamp = %lu, Size = %lu, KeyFrame = %d\n", dwTimeStamp, frameSize, bIsKeyFrame);
		//sprintf(szBuf, "[Audio] TimeStamp = %lu, Size = %lu, KeyFrame = %d\n", dwTimeStamp, frameSize, bIsKeyFrame);
		//DoTextLog(szBuf);
	}
	// 启动成功之后，才需要处理帧数据包...
	if( fRtspClient != NULL && fRtspClient->IsHasStart() ) {
		fRtspClient->WriteSample(bIsVideo, strFrame, dwTimeStamp, 0, bIsKeyFrame);
	}
	// 通知录像线程，保存一帧数据包...
	/*if( fRecThread != NULL ) {
		fRecThread->WriteSample(bIsVideo, (BYTE*)strFrame.c_str(), strFrame.size(), dwTimeStamp, 0, bIsKeyFrame);
	}
	// 构造音视频数据帧，推送给rtsp线程...
	if( fRtspThread != NULL ) {
		FMS_FRAME	theFrame;
		theFrame.typeFlvTag = (bIsVideo ? FLV_TAG_TYPE_VIDEO : FLV_TAG_TYPE_AUDIO);	// 设置音视频标志
		theFrame.dwSendTime = dwTimeStamp;
		theFrame.dwRenderOffset = 0;
		theFrame.is_keyframe = bIsKeyFrame;
		theFrame.strData = strFrame;
		// 推送数据帧给rtsp线程...
		fRtspThread->PushFrame(theFrame);
	}*/

	// 处理下一帧的数据...
	this->continuePlaying();
}

Boolean DummySink::continuePlaying()
{
	// sanity check (should not happen)
	if( fSource == NULL )
		return False;
	// Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
	fSource->getNextFrame(fReceiveBuffer, DUMMY_SINK_RECEIVE_BUFFER_SIZE, afterGettingFrame, this, onSourceClosure, this);
	return True;
}
