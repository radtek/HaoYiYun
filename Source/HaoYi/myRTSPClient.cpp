
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
		// �������󣬴�ӡ����...
		if( resultCode != 0 ) {
			TRACE("[OPTIONS] Code = %d, Error = %s\n", resultCode, resultString);
			break;
		}
		// �ɹ�������DESCRIBE����...
		TRACE("[OPTIONS] = %s\n", resultString);
		this->sendDescribeCommand(continueAfterDESCRIBE);
		return;
	}while( 0 );

	// ��������������ѭ���˳���Ȼ���Զ�ֹͣ�Ự...
	if( m_lpRtspThread != NULL ) {
		m_lpRtspThread->ResetEventLoop();
	}
	// ��������ֹͣ¼���߳�...
	if( m_lpRecThread != NULL ) {
		m_lpRecThread->ResetEventLoop();
	}
}

void ourRTSPClient::myAfterDESCRIBE(int resultCode, char* resultString)
{
	do {
		// ��ȡ��������...
		UsageEnvironment & env = this->envir();
		StreamClientState & scs = this->m_scs;

		// ��ӡ��ȡ��SDP��Ϣ...
		TRACE("[SDP] = %s\n", resultString);

		// ���ش����˳�...
		if( resultCode != 0 ) {
			TRACE("[DESCRIBE] Code = %d, Error = %s\n", resultCode, resultString);
			break;
		}
		
		// �û�ȡ��SDP�����Ự...
		scs.m_session = MediaSession::createNew(env, resultString);

		// �жϴ����Ự�Ľ��...
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
		// ������һ��SETUP���ֲ���...
		scs.m_iter = new MediaSubsessionIterator(*scs.m_session);
		this->setupNextSubsession();
		return;
	} while (0);
	
	// ��������������ѭ���˳���Ȼ���Զ�ֹͣ�Ự...
	if( m_lpRtspThread != NULL ) {
		m_lpRtspThread->ResetEventLoop();
	}
	// ��������ֹͣ¼���߳�...
	if( m_lpRecThread != NULL ) {
		m_lpRecThread->ResetEventLoop();
	}
}
//
// ��������жϣ�����Ƶ��ʽ�Ƿ����Ҫ��...
// ����Ƶ����Գ���һ�Σ�Ȼ��Ž��뵽 play ״̬...
void ourRTSPClient::myAfterSETUP(int resultCode, char* resultString)
{
	do {
		// ��ȡ��������...
		UsageEnvironment & env = this->envir();
		StreamClientState & scs = this->m_scs;
		
		// ���ش����˳�...
		if( resultCode != 0 ) {
			TRACE("[%s/%s] Failed to Setup.\n", scs.m_subsession->mediumName(), scs.m_subsession->codecName());
			break;
		}

		// �ж���Ƶ��ʽ�Ƿ���ȷ�������� video/H264 ...
		if( strcmp(scs.m_subsession->mediumName(), "video") == 0 ) {
			if( strcmp(scs.m_subsession->codecName(), "H264") != 0 ) {
				TRACE("[%s/%s] Error => Must be Video/H264.\n", scs.m_subsession->mediumName(), scs.m_subsession->codecName());
				break;
			}
			// ������ SPS �� PPS �����ݰ�...
			ASSERT( strcmp(scs.m_subsession->codecName(), "H264") == 0 );
			const char * lpszSpro = scs.m_subsession->fmtp_spropparametersets();
			if( lpszSpro == NULL ) {
				TRACE("[%s/%s] Error => SPS or PPS...\n", scs.m_subsession->mediumName(), scs.m_subsession->codecName());
				break;
			}
			// ��õ�һ�� SPS �� ��һ��PPS ...
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
			// ����ͬʱ���� SPS �� PPS...
			if( m_strSPS.size() <= 0 || m_strPPS.size() <= 0 ) {
				TRACE("[%s/%s] Error => SPS or PPS...\n", scs.m_subsession->mediumName(), scs.m_subsession->codecName());
				break;
			}
			ASSERT( m_strSPS.size() > 0 && m_strPPS.size() > 0 );
			// ֪ͨrtsp�̣߳���ʽͷ�Ѿ�׼������...
			/*if( m_lpRtspThread != NULL ) {
				m_lpRtspThread->WriteAVCSequenceHeader(strSPS, strPPS);
			}
			// ֪ͨ¼���̣߳���ʽͷ�Ѿ�׼������...
			if( m_lpRecThread != NULL ) {
				m_lpRecThread->CreateVideoTrack(strSPS, strPPS);
			}*/
			// ������Ƶ��־...
			m_bHasVideo = true;
		}

		// �ж���Ƶ��ʽ�Ƿ���ȷ, ������ audio/MPEG4 ...
		if( strcmp(scs.m_subsession->mediumName(), "audio") == 0 ) {
			if( strnicmp(scs.m_subsession->codecName(), "MPEG4", strlen("MPEG4")) != 0 ) {
				TRACE("[%s/%s] Error => Must be Audio/MPEG4.\n", scs.m_subsession->mediumName(), scs.m_subsession->codecName());
				break;
			}
			ASSERT( strnicmp(scs.m_subsession->codecName(), "MPEG4", strlen("MPEG4")) == 0 );
			// ��ȡ�������Ͳ�������Ϣ...
			m_audio_channels = scs.m_subsession->numChannels();
			m_audio_rate = scs.m_subsession->rtpTimestampFrequency();
			if( m_audio_channels <= 0 || m_audio_rate <= 0 ) {
				TRACE("[%s/%s] Error => channel(%d),rate(%d).\n", scs.m_subsession->mediumName(), scs.m_subsession->codecName(), m_audio_channels, m_audio_rate);
				break;
			}
			// ֪ͨrtsp�̣߳���ʽͷ�Ѿ�׼������...
			/*if( m_lpRtspThread != NULL ) {
				m_lpRtspThread->WriteAACSequenceHeader(audio_rate, audio_channels);
			}
			// ֪ͨ¼���̣߳���ʽͷ�Ѿ�׼������...
			if( m_lpRecThread != NULL ) {
				m_lpRecThread->CreateAudioTrack(audio_rate, audio_channels);
			}*/
			// ������Ƶ��־...
			m_bHasAudio = true;
		}
		
		// ��ӡ��ȷ��Ϣ...
		TRACE("[%s/%s] Setup OK.\n", scs.m_subsession->mediumName(), scs.m_subsession->codecName());
		if( scs.m_subsession->rtcpIsMuxed() ) {
			TRACE("[client port] %d \n", scs.m_subsession->clientPortNum());
		} else {
			TRACE("[client ports] %d - %d \n", scs.m_subsession->clientPortNum(), scs.m_subsession->clientPortNum()+1);
		}
		
		// Having successfully setup the subsession, create a data sink for it, and call "startPlaying()" on it.
		// (This will prepare the data sink to receive data; the actual flow of data from the client won't start happening until later,
		// after we've sent a RTSP "PLAY" command.)

		// �����µ�֡���ݴ������...		
		// perhaps use your own custom "MediaSink" subclass instead
		ASSERT( m_lpRtspThread != NULL || m_lpRecThread != NULL );
		scs.m_subsession->sink = DummySink::createNew(env, *scs.m_subsession, this->url(), this);

		// ����ʧ�ܣ���ӡ������Ϣ...
		if( scs.m_subsession->sink == NULL ) {
			TRACE("[%s/%s] Error = %s. \n", scs.m_subsession->mediumName(), scs.m_subsession->codecName(), env.getResultMsg());
			break;
		}
		
		TRACE("[%s/%s] Created a data sink ok. \n", scs.m_subsession->mediumName(), scs.m_subsession->codecName());

		// ���� PLAY Э��...
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

	// ��������������ѭ���˳���Ȼ���Զ�ֹͣ�Ự...
	if( m_lpRtspThread != NULL ) {
		m_lpRtspThread->ResetEventLoop();
	}
	// ��������ֹͣ¼���߳�...
	if( m_lpRecThread != NULL ) {
		m_lpRecThread->ResetEventLoop();
	}
}
//
// ����������¼���߳�...
BOOL ourRTSPClient::doStartEvent(string & inNewSPS, string & inNewPPS, BOOL bIsFromAudio)
{
	// �����ͨ����Ƶ�����ģ���������Ƶ��ֱ�ӷ���false���ȴ���Ƶ����...
	if( bIsFromAudio && m_bHasVideo ) {
		m_bHasStart = false;
		return m_bHasStart;
	}
	// �����ͨ����Ƶ�����ģ���û����Ƶ��SPS��PPSһ��Ϊ�գ���������...
	if( bIsFromAudio && !m_bHasVideo ) {
		ASSERT( inNewSPS.size() <= 0 );
		ASSERT( inNewPPS.size() <= 0 );
	}
	// ���µ�SPS��PPS�滻�ɵ�SPS��PPS...
	m_strSPS = inNewSPS; m_strPPS = inNewPPS;
	// ��������Ƶ��ʽͷ����Ч�ԣ�������Ӧ������Ƶ�ڵ����...
	// ֪ͨrtsp�̣߳���ʽͷ�Ѿ�׼������...
	if( m_lpRtspThread != NULL ) {
		// ������Ƶ��ʽ����ͷ...
		if( m_strSPS.size() > 0 && m_strPPS.size() > 0 ) {
			m_lpRtspThread->WriteAVCSequenceHeader(m_strSPS, m_strPPS);
		}
		// ������Ƶ��ʽ����ͷ...
		if( m_audio_channels > 0 && m_audio_rate > 0 ) {
			m_lpRtspThread->WriteAACSequenceHeader(m_audio_rate, m_audio_channels);
		}
		// ����rtmp�����߳�...
		m_lpRtspThread->StartPushThread();
	}
	// ֪ͨ¼���̣߳���ʽͷ�Ѿ�׼������...
	if( m_lpRecThread != NULL ) {
		// ������Ƶ�������...
		if( m_strSPS.size() > 0 && m_strPPS.size() > 0 ) {
			m_lpRecThread->CreateVideoTrack(m_strSPS, m_strPPS);
		}
		// ������Ƶ�������...
		if( m_audio_channels > 0 && m_audio_rate > 0 ) {
			m_lpRecThread->CreateAudioTrack(m_audio_rate, m_audio_channels);
		}
	}
	// �����ɹ������ؽ��...
	m_bHasStart = true;
	return m_bHasStart;
}
//
// ��������֡�Ĵ���...
void ourRTSPClient::WriteSample(bool bIsVideo, string & inFrame, DWORD inTimeStamp, DWORD inRenderOffset, bool bIsKeyFrame)
{
	// ֪ͨ¼���̣߳�����һ֡���ݰ�...
	if( m_lpRecThread != NULL ) {
		m_lpRecThread->WriteSample(bIsVideo, (BYTE*)inFrame.c_str(), inFrame.size(), inTimeStamp, inRenderOffset, bIsKeyFrame);
	}
	// ��������Ƶ����֡�����͸�rtsp�߳�...
	if( m_lpRtspThread != NULL ) {
		FMS_FRAME	theFrame;
		theFrame.typeFlvTag = (bIsVideo ? FLV_TAG_TYPE_VIDEO : FLV_TAG_TYPE_AUDIO);	// ��������Ƶ��־
		theFrame.dwSendTime = inTimeStamp;
		theFrame.dwRenderOffset = inRenderOffset;
		theFrame.is_keyframe = bIsKeyFrame;
		theFrame.strData = inFrame;
		// ��������֡��rtsp�߳�...
		m_lpRtspThread->PushFrame(theFrame);
	}
}

void ourRTSPClient::myAfterPLAY(int resultCode, char* resultString)
{
	do {
		// ��ȡ��������...
		UsageEnvironment & env = this->envir();
		StreamClientState & scs = this->m_scs;
		
		// ���ش��󣬴�ӡ��Ϣ...
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
		
		// һ����������ӡ��Ϣ��ֱ�ӷ���...
		TRACE("Started playing session.(for up to %.2f)\n", scs.m_duration);
		// ����rtmp�����߳�...
		//if( m_lpRtspThread != NULL ) {
		//	m_lpRtspThread->StartPushThread();
		//}
		return;
	} while (0);

	// ��������������ѭ���˳���Ȼ���Զ�ֹͣ�Ự...
	if( m_lpRtspThread != NULL ) {
		m_lpRtspThread->ResetEventLoop();
	}
	// ��������ֹͣ¼���߳�...
	if( m_lpRecThread != NULL ) {
		m_lpRecThread->ResetEventLoop();
	}
}
//
// ������һ��Э��...
void ourRTSPClient::setupNextSubsession()
{
	// ��ȡ����������Ϣ...
	UsageEnvironment & env = this->envir();
	StreamClientState & scs = this->m_scs;

	// ��ȡ�Ự����...
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
			// ����SETUPЭ�飬���������TCPģʽ��������Щ��������֧��RTPģʽ...
			this->sendSetupCommand(*scs.m_subsession, continueAfterSETUP, False, True);
		}
		return;
	}
	
	// ���﷢�� PLAY Э�����...
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
// �رջỰ����...
void ourRTSPClient::shutdownStream()
{
	// ��ȡ��������...
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
	
	// �ر����rtsp���Ӷ���...
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
	
	// ������ѭ���˳���Ȼ���Զ�ֹͣ�Ự...
	if( rtspClient->m_lpRtspThread != NULL ) {
		rtspClient->m_lpRtspThread->ResetEventLoop();
	}
	// ��¼���߳��˳�...
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
	
	// ������ѭ���˳���Ȼ���Զ�ֹͣ�Ự...
	if( rtspClient->m_lpRtspThread != NULL ) {
		rtspClient->m_lpRtspThread->ResetEventLoop();
	}
	// ��¼���߳��˳�...
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
	// �ѵ�һ֡ʱ�����Ϊ���ʱ��...
	string			strFrame;
	uint32_t		dwTimeStamp = 0;
	ULARGE_INTEGER	llTimCountCur = {0};
	// �õ���ǰʱ�䣬ʱ�䵥λ��0.1΢��...
	::GetSystemTimeAsFileTime((FILETIME *)&llTimCountCur);
	if( m_llTimCountFirst.QuadPart <= 0 ) {
		m_llTimCountFirst.QuadPart = llTimCountCur.QuadPart;
	}
	// ����ʱ���������)����Ҫ����10000...
	dwTimeStamp = (llTimCountCur.QuadPart - m_llTimCountFirst.QuadPart)/10000;
	
	/*if((fSTimeVal.tv_sec <= 0) && (fSTimeVal.tv_usec <= 0)) {
		fSTimeVal = presentationTime;
	}
	// ����ʱ���(����)��tv_sec���룬tv_usec��΢��...
	dwTimeStamp = uint32_t((presentationTime.tv_sec - fSTimeVal.tv_sec)*1000.0f + (presentationTime.tv_usec - fSTimeVal.tv_usec)/1000.0f);*/

	//char szBuf[MAX_PATH] = {0};

	// ��ȡ��Ƶ��־���ؼ�֡��־...
	// H264�� nalu ��־����1(Ƭ��),5(�ؼ�֡),7(SPS),8(PPS)...
	BOOL bIsVideo = false, bIsKeyFrame = false;
	if( strcmp(fSubsession.mediumName(), "video") == 0 ) {
		bIsVideo = true;
		// ����ؼ�֡��־...
		BYTE nalType = fReceiveBuffer[0] & 0x1f;
		// 2017.04.10 - by jackey => �����SPS��PPS��ֱ�Ӷ���...
		// ��������HTML5��������video��ǩ���޷����ţ�ͨ��MPlayer���֣�д�˶���Ļ�֡���պ���3��...
		if( nalType > 5 ) {
			// ���û��������������������...
			if( fRtspClient != NULL && !fRtspClient->IsHasStart() ) {
				// �������������SPS...
				if( nalType == 7 ) {
					m_strNewSPS.assign((const char*)fReceiveBuffer, frameSize);
				}
				// �������������PPS...
				if( nalType == 8 ) {
					m_strNewPPS.assign((const char*)fReceiveBuffer, frameSize);
				}
				// SPS��PPS��ȡ��ϣ�֪ͨ�����߳�...
				if( m_strNewSPS.size() > 0 && m_strNewPPS.size() > 0 ) {
					fRtspClient->doStartEvent(m_strNewSPS, m_strNewPPS, false);
				}
			}
			// ֱ�Ӷ�����Щͷ������...
			this->continuePlaying();
			return;
		}
		// ���ùؼ�֡��־...
		if( nalType == 5 ) {
			bIsKeyFrame = true;
		}
		// ����ʼ��0x00000001���滻Ϊnalu�Ĵ�С...
		char pNalBuf[4] = {0};
		UI32ToBytes(pNalBuf, frameSize);
		// ��Ƶ֡��Ҫ������ǰ�����֡�ĳ���(4�ֽ�)...
		strFrame.assign(pNalBuf, sizeof(pNalBuf));
		strFrame.append((char*)fReceiveBuffer, frameSize);
		//TRACE("[%x][Video] TimeStamp = %lu, Size = %lu, KeyFrame = %d\n", fReceiveBuffer[0], dwTimeStamp, frameSize, bIsKeyFrame);
		//TRACE("[Video] TimeStamp = %lu, Size = %lu, KeyFrame = %d\n", dwTimeStamp, frameSize, bIsKeyFrame);
		//sprintf(szBuf, "[Video] TimeStamp = %lu, Size = %lu, KeyFrame = %d\n", dwTimeStamp, frameSize, bIsKeyFrame);
		//DoTextLog(szBuf);
	}
	// ��ȡ��Ƶ��־���ؼ�֡��־...
	if( strcmp(fSubsession.mediumName(), "audio") == 0 ) {
		bIsVideo = false; bIsKeyFrame = true;
		strFrame.assign((char*)fReceiveBuffer, frameSize);
		// ����Ƶ����֡ȥ����������¼���߳�...
		if( fRtspClient != NULL && !fRtspClient->IsHasStart() ) {
			fRtspClient->doStartEvent(m_strNewSPS, m_strNewPPS, true);
		}
		//TRACE("[Audio] TimeStamp = %lu, Size = %lu, KeyFrame = %d\n", dwTimeStamp, frameSize, bIsKeyFrame);
		//sprintf(szBuf, "[Audio] TimeStamp = %lu, Size = %lu, KeyFrame = %d\n", dwTimeStamp, frameSize, bIsKeyFrame);
		//DoTextLog(szBuf);
	}
	// �����ɹ�֮�󣬲���Ҫ����֡���ݰ�...
	if( fRtspClient != NULL && fRtspClient->IsHasStart() ) {
		fRtspClient->WriteSample(bIsVideo, strFrame, dwTimeStamp, 0, bIsKeyFrame);
	}
	// ֪ͨ¼���̣߳�����һ֡���ݰ�...
	/*if( fRecThread != NULL ) {
		fRecThread->WriteSample(bIsVideo, (BYTE*)strFrame.c_str(), strFrame.size(), dwTimeStamp, 0, bIsKeyFrame);
	}
	// ��������Ƶ����֡�����͸�rtsp�߳�...
	if( fRtspThread != NULL ) {
		FMS_FRAME	theFrame;
		theFrame.typeFlvTag = (bIsVideo ? FLV_TAG_TYPE_VIDEO : FLV_TAG_TYPE_AUDIO);	// ��������Ƶ��־
		theFrame.dwSendTime = dwTimeStamp;
		theFrame.dwRenderOffset = 0;
		theFrame.is_keyframe = bIsKeyFrame;
		theFrame.strData = strFrame;
		// ��������֡��rtsp�߳�...
		fRtspThread->PushFrame(theFrame);
	}*/

	// ������һ֡������...
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
