
#include "stdafx.h"
#include "UtilTool.h"
#include "VideoWnd.h"
#include "BitWritter.h"
#include "..\ReadSPS.h"

#include "UDPPlayThread.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define ADTS_HEADER_SIZE	7

CDecoder::CDecoder()
  : m_lpCodec(NULL)
  , m_lpDecoder(NULL)
  , m_play_next_ns(-1)
{
}

CDecoder::~CDecoder()
{
	// �ͷŽ���������...
	if( m_lpDecoder != NULL ) {
		avcodec_close(m_lpDecoder);
		av_free(m_lpDecoder);
	}
	// �ͷŶ����еĽ���ǰ�����ݿ�...
	GM_MapPacket::iterator itorPack;
	for(itorPack = m_MapPacket.begin(); itorPack != m_MapPacket.end(); ++itorPack) {
		av_free_packet(&itorPack->second);
	}
	m_MapPacket.clear();
	// �ͷ�û�в�����ϵĽ���������֡...
	GM_MapFrame::iterator itorFrame;
	for(itorFrame = m_MapFrame.begin(); itorFrame != m_MapFrame.end(); ++itorFrame) {
		av_frame_free(&itorFrame->second);
	}
	m_MapFrame.clear();
}

void CDecoder::doPushPacket(AVPacket & inPacket)
{
	// ע�⣺���������DTS���򣬾����˽�����Ⱥ�˳��...
	// �������ͬDTS������֡�Ѿ����ڣ�ֱ���ͷ�AVPacket������...
	if( m_MapPacket.find(inPacket.dts) != m_MapPacket.end() ) {
		av_free_packet(&inPacket);
		return;
	}
	// ���û����ͬDTS������֡����������...
	m_MapPacket[inPacket.dts] = inPacket;
}

void CDecoder::doSleepTo()
{
	// < 0 ������Ϣ��ֱ�ӷ���...
	if( m_play_next_ns < 0 )
		return;
	// �����Ϣ50����...
	uint64_t cur_time_ns = CUtilTool::os_gettime_ns();
	const uint64_t timeout_ns = 50000000;
	// ����ȵ�ǰʱ��С(�ѹ���)��ֱ�ӷ���...
	if( m_play_next_ns <= cur_time_ns )
		return;
	// ���㳬ǰʱ��Ĳ�ֵ�������Ϣ50����...
	uint64_t delta_ns = m_play_next_ns - cur_time_ns;
	delta_ns = ((delta_ns < timeout_ns) ? delta_ns : timeout_ns);
	//TRACE("[Video] Sleep: %I64d ms\n", delta_ns/1000000);
	CUtilTool::os_sleepto_ns(cur_time_ns + delta_ns);
}

CVideoThread::CVideoThread(CPlaySDL * lpPlaySDL)
  : m_lpPlaySDL(lpPlaySDL)
  , m_lpRenderWnd(NULL)
  , m_sdlRenderer(NULL)
  , m_sdlTexture(NULL)
  , m_sdlScreen(NULL)
  , m_nHeight(0)
  , m_nWidth(0)
  , m_nFPS(0)
{
	ASSERT( m_lpPlaySDL != NULL );
}

CVideoThread::~CVideoThread()
{
	// �ȴ��߳��˳�...
	this->StopAndWaitForThread();
	// ����SDL����...
	if( m_sdlScreen != NULL ) {
		SDL_DestroyWindow(m_sdlScreen);
		m_sdlScreen = NULL;
	}
	if( m_sdlRenderer != NULL ) {
		SDL_DestroyRenderer(m_sdlRenderer);
		m_sdlRenderer = NULL;
	}
	if( m_sdlTexture != NULL ) {
		SDL_DestroyTexture(m_sdlTexture);
		m_sdlTexture = NULL;
	}
	// ����SDL����ʱ�����ع�������...
	if( m_lpRenderWnd != NULL ) {
		m_lpRenderWnd->ShowWindow(SW_SHOW);
		m_lpRenderWnd->SetRenderState(CRenderWnd::ST_RENDER);
	}
}

BOOL CVideoThread::InitVideo(CRenderWnd * lpRenderWnd, string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS)
{
	// ���洫�ݹ����Ĳ�����Ϣ...
	m_lpRenderWnd = lpRenderWnd;
	m_nHeight = nHeight;
	m_nWidth = nWidth;
	m_nFPS = nFPS;
	m_strSPS = inSPS;
	m_strPPS = inPPS;

	// ���ò��Ŵ���״̬...
	m_lpRenderWnd->SetRenderState(CRenderWnd::ST_WAIT);
	// ����SDL��Ҫ�Ķ���...
	m_sdlScreen = SDL_CreateWindowFrom((void*)m_lpRenderWnd->m_hWnd);
	m_sdlRenderer = SDL_CreateRenderer(m_sdlScreen, -1, 0);
	m_sdlTexture = SDL_CreateTexture(m_sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, m_nWidth, m_nHeight);
	// ��ʼ��ffmpeg������...
	av_register_all();
	// ׼��һЩ�ض��Ĳ���...
	AVCodecID src_codec_id = AV_CODEC_ID_H264;
	// ������Ҫ�Ľ����� => ���ô���������...
	m_lpCodec = avcodec_find_decoder(src_codec_id);
	m_lpDecoder = avcodec_alloc_context3(m_lpCodec);
	// �򿪻�ȡ���Ľ�����...
	if( avcodec_open2(m_lpDecoder, m_lpCodec, NULL) < 0 ) {
		TRACE("[Video] avcodec_open2 failed.\n");
		return false;
	}
	// �����߳̿�ʼ��ת...
	this->Start();
	return true;
}

void CVideoThread::Entry()
{
	while( !this->IsStopRequested() ) {
		// ����sleep�ȴ�...
		this->doSleepTo();
		// ����һ֡��Ƶ...
		this->doDecodeFrame();
		// ��ʾһ֡��Ƶ...
		this->doDisplaySDL();
		// ������Ϣʱ��...
		this->doCalcNextNS();
	}
}

void CVideoThread::doCalcNextNS()
{
	OSMutexLocker theLock(&m_Mutex);
	// �������Ҫ��������ݣ�ֱ�ӷ���-1...
	if( m_MapPacket.size() > 0 ) {
		m_play_next_ns = -1;
		return;
	}
	// ���û����Ҫ��ʾ�Ľ�������ݣ���Ϣ20����...
	if( m_MapFrame.size() <= 0 ) {
		m_play_next_ns = CUtilTool::os_gettime_ns() + 20000000;
		return;
	}
	// ֱ�ӷ�������Ҫ���ŵĵ�һ֡���ݵ�PTS...
	m_play_next_ns = m_MapFrame.begin()->first;
}

void CVideoThread::doFillPacket(string & inData, int inPTS, bool bIsKeyFrame, int inOffset)
{
	// �߳������˳��У�ֱ�ӷ���...
	if( this->IsStopRequested() )
		return;
	// �����̻߳���״̬��...
	OSMutexLocker theLock(&m_Mutex);
	//TRACE("[Video] PTS: %d, Offset: %d, Size: %d\n", inPTS, inOffset, inData.size());
	// ÿ���ؼ�֡������sps��pps�����Ż�ӿ�...
	string strCurFrame;
	// ����ǹؼ�֡��������д��sps����д��pps...
	if( bIsKeyFrame ) {
		DWORD dwTag = 0x01000000;
		strCurFrame.append((char*)&dwTag, sizeof(DWORD));
		strCurFrame.append(m_strSPS);
		strCurFrame.append((char*)&dwTag, sizeof(DWORD));
		strCurFrame.append(m_strPPS);
	}
	// �޸���Ƶ֡����ʼ�� => 0x00000001
	char * lpszSrc = (char*)inData.c_str();
	memset((void*)lpszSrc, 0, sizeof(DWORD));
	lpszSrc[3] = 0x01;
	// ���׷��������...
	strCurFrame.append(inData);
	// ����һ����ʱAVPacket��������֡��������...
	AVPacket  theNewPacket = {0};
	av_new_packet(&theNewPacket, strCurFrame.size());
	ASSERT(theNewPacket.size == strCurFrame.size());
	memcpy(theNewPacket.data, strCurFrame.c_str(), theNewPacket.size);
	// ���㵱ǰ֡��PTS���ؼ�֡��־ => ��Ƶ�� => 1
	theNewPacket.pts = inPTS;
	theNewPacket.dts = inPTS - inOffset;
	theNewPacket.flags = bIsKeyFrame;
	theNewPacket.stream_index = 1;
	// ������ѹ�����ǰ���е���...
	this->doPushPacket(theNewPacket);
}
//
// ȡ��һ֡��������Ƶ���ȶ�ϵͳʱ�䣬�����ܷ񲥷�...
void CVideoThread::doDisplaySDL()
{
	OSMutexLocker theLock(&m_Mutex);
	// û�л��棬ֱ�ӷ���...
	if( m_MapFrame.size() <= 0 )
		return;
	/////////////////////////////////////////////////////////////
	// ע�⣺��ʱģ��Ŀǰ���Գ����������������ʵ�����ģ��...
	/////////////////////////////////////////////////////////////
	// ��ȡ��ǰ��ϵͳʱ�� => ����...
	int64_t inSysCurNS = CUtilTool::os_gettime_ns();
	// ��ȡ��һ����ʱ����С�����ݿ�...
	GM_MapFrame::iterator itorItem = m_MapFrame.begin();
	AVFrame * lpSrcFrame  = itorItem->second;
	int64_t   nFramePTS   = itorItem->first;
	int       nSize = m_MapFrame.size();
	// ��ǰ֡����ʾʱ�仹û�е� => ֱ�ӷ��� => �����ȴ�...
	if( nFramePTS > inSysCurNS ) {
		//TRACE("[Video] Advance: %I64d ms, Wait, Size: %lu\n", (nFramePTS - inSysCurNS)/1000000, nSize);
		return;
	}
	///////////////////////////////////////////////////////////////////////////////////
	// ע�⣺��Ƶ��ʱ֡�����֡�������ܶ��������������ʾ����Ƶ�����ٶ���ԽϿ졣
	///////////////////////////////////////////////////////////////////////////////////
	// ������ת����jpg...
	//DoProcSaveJpeg(lpSrcFrame, m_lpDecoder->pix_fmt, nFramePTS, "F:/MP4/Dst");
	TRACE("[Video] OS: %I64d ms, Delay: %I64d ms, Success, Size: %d, Type: %d\n", inSysCurNS/1000000, (inSysCurNS - nFramePTS)/1000000, nSize, lpSrcFrame->pict_type);
	// ׼����Ҫת���ĸ�ʽ��Ϣ...
	enum AVPixelFormat nDestFormat = AV_PIX_FMT_YUV420P;
	enum AVPixelFormat nSrcFormat = m_lpDecoder->pix_fmt;
	int nSrcWidth = m_lpDecoder->width;
	int nSrcHeight = m_lpDecoder->height;
	// ����ʲô��ʽ������Ҫ�������ظ�ʽ��ת��...
	AVFrame * pDestFrame = av_frame_alloc();
	int nDestBufSize = avpicture_get_size(nDestFormat, nSrcWidth, nSrcHeight);
	uint8_t * pDestOutBuf = (uint8_t *)av_malloc(nDestBufSize);
	avpicture_fill((AVPicture *)pDestFrame, pDestOutBuf, nDestFormat, nSrcWidth, nSrcHeight);
	// ����ffmpeg�ĸ�ʽת���ӿں���...
	struct SwsContext * img_convert_ctx = sws_getContext(nSrcWidth, nSrcHeight, nSrcFormat, nSrcWidth, nSrcHeight, nDestFormat, SWS_BICUBIC, NULL, NULL, NULL);
	int nReturn = sws_scale(img_convert_ctx, (const uint8_t* const*)lpSrcFrame->data, lpSrcFrame->linesize, 0, nSrcHeight, pDestFrame->data, pDestFrame->linesize);
	sws_freeContext(img_convert_ctx);
	// ����ת���������֡����...
	pDestFrame->width = nSrcWidth;
	pDestFrame->height = nSrcHeight;
	pDestFrame->format = nDestFormat;
	//////////////////////////////////////////////
	// ʹ��SDL ���л�����ƹ���...
	//////////////////////////////////////////////
	int nResult = 0;
	CRect rcRect;
	SDL_Rect srcSdlRect = { 0 };
	SDL_Rect dstSdlRect = { 0 };
	m_lpRenderWnd->GetClientRect(rcRect);
	srcSdlRect.w = nSrcWidth;
	srcSdlRect.h = nSrcHeight;
	dstSdlRect.w = rcRect.Width();
	dstSdlRect.h = rcRect.Height();
	nResult = SDL_UpdateTexture( m_sdlTexture, &srcSdlRect, pDestFrame->data[0], pDestFrame->linesize[0] );
    nResult = SDL_RenderClear( m_sdlRenderer );
    nResult = SDL_RenderCopy( m_sdlRenderer, m_sdlTexture, &srcSdlRect, &dstSdlRect );
    SDL_RenderPresent( m_sdlRenderer );
	// �ͷ���ʱ��������ݿռ�...
	av_frame_free(&pDestFrame);
	av_free(pDestOutBuf);
	// �ͷŲ�ɾ���Ѿ�ʹ�����ԭʼ���ݿ�...
	av_frame_free(&lpSrcFrame);
	m_MapFrame.erase(itorItem);
}

void CVideoThread::doDecodeFrame()
{
	OSMutexLocker theLock(&m_Mutex);
	// û�л��棬ֱ�ӷ���...
	if( m_MapPacket.size() <= 0 )
		return;
	int64_t inSysZeroNS = m_lpPlaySDL->GetSysZeroNS();
	// ��ȡһ��AVPacket���н��������һ��AVPacketһ���ܽ����һ��Picture...
	int got_picture = 0, nResult = 0;
	AVFrame  * lpFrame = av_frame_alloc();
	GM_MapPacket::iterator itorItem = m_MapPacket.begin();
	AVPacket & thePacket = itorItem->second;
	nResult = avcodec_decode_video2(m_lpDecoder, lpFrame, &got_picture, &thePacket);
	// ����ʧ�ܻ�û�еõ�����ͼ��ֱ���ӵ�����...
	if( nResult < 0 || !got_picture ) {
		TRACE("[Video] PTS: %I64d, decode_frame failed.\n", thePacket.pts);
		av_frame_free(&lpFrame);
		av_free_packet(&thePacket);
		m_MapPacket.erase(itorItem);
		return;
	}
	//TRACE("[Video] best: %I64d, pkt_dts: %I64d, pkt_pts: %I64d\n", lpFrame->best_effort_timestamp, lpFrame->pkt_dts, lpFrame->pkt_pts);
	//TRACE("[Video] DTS: %I64d, PTS: %I64d, Size: %lu\n", thePacket.dts, thePacket.pts, thePacket.size);
	// �������������֡��ʱ��� => ������ת��������...
	AVRational base_pack  = {1, 1000};
	AVRational base_frame = {1, 1000000000};
	int64_t frame_pts = av_rescale_q(lpFrame->best_effort_timestamp, base_pack, base_frame);
	// �ۼ�ϵͳ��ʱ���...
	frame_pts += inSysZeroNS;
	// ������������֡���벥�Ŷ��е���...
	m_MapFrame[frame_pts] = lpFrame;
	//DoProcSaveJpeg(lpFrame, m_lpDecoder->pix_fmt, frame_pts, "F:/MP4/Src");
	// ���������ã�������free��erase...
	av_free_packet(&thePacket);
	m_MapPacket.erase(itorItem);
}

CAudioThread::CAudioThread(CPlaySDL * lpPlaySDL)
{
	m_nDeviceID = 0;
	m_nSampleDuration = 0;
	m_lpPlaySDL = lpPlaySDL;
	m_audio_sample_rate = 0;
	m_audio_rate_index = 0;
	m_audio_channel_num = 0;
	m_out_buffer = NULL;
	m_au_convert_ctx = NULL;
	m_out_buffer_size = 0;
}

CAudioThread::~CAudioThread()
{
	// �ȴ��߳��˳�...
	this->StopAndWaitForThread();
	// �رջ�����...
	if( m_out_buffer != NULL ) {
		av_free(m_out_buffer);
		m_out_buffer = NULL;
	}
	// �ر���Ƶ���Ŷ���...
	if( m_au_convert_ctx != NULL ) {
		swr_free(&m_au_convert_ctx);
		m_au_convert_ctx = NULL;
	}
	// �ر���Ƶ�豸...
	SDL_CloseAudio();
	m_nDeviceID = 0;
}

void CAudioThread::doDecodeFrame()
{
	OSMutexLocker theLock(&m_Mutex);
	// û�л��棬ֱ�ӷ���...
	if( m_MapPacket.size() <= 0 || m_nDeviceID <= 0 )
		return;
	int64_t inSysZeroNS = m_lpPlaySDL->GetSysZeroNS();
	// ��ȡһ��AVPacket���н��������һ��AVPacketһ���ܽ����һ��Picture...
	int got_picture = 0, nResult = 0;
	AVFrame  * lpFrame = av_frame_alloc();
	GM_MapPacket::iterator itorItem = m_MapPacket.begin();
	AVPacket & thePacket = itorItem->second;
	nResult = avcodec_decode_audio4(m_lpDecoder, lpFrame, &got_picture, &thePacket);
	// ����ʧ�ܻ�û�еõ�����ͼ��ֱ���ӵ�����...
	if( nResult < 0 || !got_picture ) {
		av_frame_free(&lpFrame);
		av_free_packet(&thePacket);
		m_MapPacket.erase(itorItem);
		return;
	}
	// �������������֡��ʱ��� => ������ת��������...
	AVRational base_pack  = {1, 1000};
	AVRational base_frame = {1, 1000000000};
	int64_t frame_pts = av_rescale_q(lpFrame->best_effort_timestamp, base_pack, base_frame);
	// �ۼ�ϵͳ��ʱ���...
	frame_pts += inSysZeroNS;
	// �Խ�������Ƶ��������ת��...
	nResult = swr_convert(m_au_convert_ctx, &m_out_buffer, m_out_buffer_size * 2, (const uint8_t **)lpFrame->data , lpFrame->nb_samples);
	// ת��������ݴ�ŵ�ת����Ķ��е���...
	string strData;
	strData.append((char*)m_out_buffer, m_out_buffer_size);
	m_MapAudio[frame_pts] = strData;
	// ���������ã�������free��erase...
	av_frame_free(&lpFrame);
	av_free_packet(&thePacket);
	m_MapPacket.erase(itorItem);
}

BOOL CAudioThread::InitAudio(int nRateIndex, int nChannelNum)
{
	OSMutexLocker theLock(&m_Mutex);
	// ����������ȡ������...
	switch( nRateIndex )
	{
	case 0x03: m_audio_sample_rate = 48000; break;
	case 0x04: m_audio_sample_rate = 44100; break;
	case 0x05: m_audio_sample_rate = 32000; break;
	case 0x06: m_audio_sample_rate = 24000; break;
	case 0x07: m_audio_sample_rate = 22050; break;
	case 0x08: m_audio_sample_rate = 16000; break;
	case 0x09: m_audio_sample_rate = 12000; break;
	case 0x0a: m_audio_sample_rate = 11025; break;
	case 0x0b: m_audio_sample_rate =  8000; break;
	default:   m_audio_sample_rate = 48000; break;
	}
	// �������������������...
	m_audio_rate_index = nRateIndex;
	m_audio_channel_num = nChannelNum;
	// ��ʼ��ffmpeg������...
	av_register_all();
	// ׼��һЩ�ض��Ĳ���...
	AVCodecID src_codec_id = AV_CODEC_ID_AAC;
	// ������Ҫ�Ľ����������������������...
	m_lpCodec = avcodec_find_decoder(src_codec_id);
	m_lpDecoder = avcodec_alloc_context3(m_lpCodec);
	// �򿪻�ȡ���Ľ�����...
	if( avcodec_open2(m_lpDecoder, m_lpCodec, NULL) != 0 )
		return false;
	// �������������������һ����...
	int in_audio_channel_num = m_audio_channel_num;
	int out_audio_channel_num = m_audio_channel_num;
	int64_t in_channel_layout = av_get_default_channel_layout(in_audio_channel_num);
	int64_t out_channel_layout = (out_audio_channel_num <= 1) ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
	// �����Ƶ������ʽ...
	AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
	// out_nb_samples: AAC-1024 MP3-1152
	int out_nb_samples = 1024;
	// ����������������� => ����...
	int in_sample_rate = m_audio_sample_rate;
	int out_sample_rate = m_audio_sample_rate;

	//SDL_AudioSpec => ����ʹ��ϵͳ�Ƽ����� => ���ûص�ģʽ������Ͷ��...
	SDL_AudioSpec audioSpec = {0};
	audioSpec.freq = out_sample_rate; 
	audioSpec.format = AUDIO_S16SYS; //m_lpDecoder->sample_fmt => AV_SAMPLE_FMT_FLTP
	audioSpec.channels = out_audio_channel_num; 
	audioSpec.samples = out_nb_samples;
	audioSpec.callback = NULL; 
	audioSpec.userdata = NULL;
	audioSpec.silence = 0;

	// ��SDL��Ƶ�豸 => ֻ�ܴ�һ���豸...
	if( SDL_OpenAudio(&audioSpec, NULL) != 0 ) {
		SDL_Log("Failed to open audio: %s", SDL_GetError());
		return false;
	}
	// ����ÿ֡���ʱ�� => ����...
	m_nSampleDuration = out_nb_samples * 1000 / out_sample_rate;
	// ���ô򿪵����豸���...
	m_nDeviceID = 1;
	// ��ȡ��Ƶ���������Ļ�������С...
	m_out_buffer_size = av_samples_get_buffer_size(NULL, out_audio_channel_num, out_nb_samples, out_sample_fmt, 1);
	m_out_buffer = (uint8_t *)av_malloc(m_out_buffer_size * 2);
	
	// ���䲢��ʼ��ת����...
	m_au_convert_ctx = swr_alloc();
	m_au_convert_ctx = swr_alloc_set_opts(m_au_convert_ctx, out_channel_layout, out_sample_fmt, out_sample_rate,
										  in_channel_layout, m_lpDecoder->sample_fmt, in_sample_rate, 0, NULL);
	swr_init(m_au_convert_ctx);

	// ��ʼ���� => ʹ��Ĭ�����豸...
	SDL_PauseAudioDevice(m_nDeviceID, 0);

	// �����߳�...
	this->Start();

	return true;
}

void CAudioThread::doDisplaySDL()
{
	//////////////////////////////////////////////////////////////////
	// ע�⣺ʹ������Ͷ�ݷ�ʽ��������Ч���ͻص���ɵ���ʱ...
	//////////////////////////////////////////////////////////////////
	OSMutexLocker theLock(&m_Mutex);
	// û�л��棬ֱ�ӷ���...
	if( m_MapAudio.size() <= 0 || m_nDeviceID <= 0 )
		return;
	// ��ȡ��ǰ��ϵͳʱ�� => ����...
	int64_t inSysCurNS = CUtilTool::os_gettime_ns();
	// ��ȡ��һ����ʱ����С�����ݿ�...
	GM_MapAudio::iterator itorItem = m_MapAudio.begin();
	string  & stringPCM  = itorItem->second;
	int64_t   nFramePTS  = itorItem->first;
	// ���ܳ�ǰͶ�����ݣ������Ӳ�������ݶѻ�����ɻ����ѹ��������������...
	if( nFramePTS > inSysCurNS ) {
		//TRACE("[Audio] Advance: %I64d ms, AudioSize: %d, QueueBytes: %lu\n", (nFramePTS - inSysCurNS)/1000000, m_MapAudio.size(), nQueueBytes);
		return;
	}
	// Ͷ������ǰ���Ȳ鿴�����Ŷӵ���Ƶ���� => ���泬��300���������...
	int nAllowDelay = 300;
	int nAllowSample = nAllowDelay / m_nSampleDuration;
	int nQueueBytes = SDL_GetQueuedAudioSize(m_nDeviceID);
	int nQueueSample = nQueueBytes / m_out_buffer_size;
	if( nQueueSample > nAllowSample ) {
		TRACE("[Audio] Clear Audio Buffer, QueueBytes: %lu\n", nQueueBytes);
		SDL_ClearQueuedAudio(m_nDeviceID);
	}
	// ����Ƶ����������֡Ͷ�ݸ���Ƶ�豸...
	if( SDL_QueueAudio(m_nDeviceID, stringPCM.c_str(), stringPCM.size()) < 0 ) {
		SDL_Log("Failed to queue audio: %s", SDL_GetError());
		return;
	}
	// ��ӡ�Ѿ�Ͷ�ݵ���Ƶ������Ϣ...
	nQueueBytes = SDL_GetQueuedAudioSize(m_nDeviceID);
	TRACE("[Audio] OS: %I64d ms, Delay: %I64d ms, AudioSize: %d, QueueBytes: %lu\n", inSysCurNS/1000000, (inSysCurNS - nFramePTS)/1000000, m_MapAudio.size(), nQueueBytes);
	// ɾ���Ѿ�ʹ�õ���Ƶ����...
	m_MapAudio.erase(itorItem);
}

void CAudioThread::doCalcNextNS()
{
	OSMutexLocker theLock(&m_Mutex);
	// �������Ҫ��������ݣ�ֱ�ӷ���-1...
	if( m_MapPacket.size() > 0 ) {
		m_play_next_ns = -1;
		return;
	}
	// ���û����Ҫ��ʾ�Ľ�������ݣ���Ϣ20����...
	if( m_MapAudio.size() <= 0 ) {
		m_play_next_ns = CUtilTool::os_gettime_ns() + 20000000;
		return;
	}
	// ֱ�ӷ�������Ҫ���ŵĵ�һ֡���ݵ�PTS...
	m_play_next_ns = m_MapAudio.begin()->first;
}

void CAudioThread::Entry()
{
	// ע�⣺����߳����ȼ��������ܽ����Ƶ��ϵͳ��������...
	// �����߳����ȼ� => ��߼�����ֹ�ⲿ����...
	//if( !::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_HIGHEST) ) {
	//	TRACE("[Audio] SetThreadPriority to THREAD_PRIORITY_HIGHEST failed.\n");
	//}
	while( !this->IsStopRequested() ) {
		// ����sleep�ȴ�...
		this->doSleepTo();
		// ����һ֡��Ƶ...
		this->doDecodeFrame();
		// ��ʾһ֡��Ƶ...
		this->doDisplaySDL();
		// ������Ϣʱ��...
		this->doCalcNextNS();
	}
}
//
// ��Ҫ����Ƶ֡�������ͷ��Ϣ...
void CAudioThread::doFillPacket(string & inData, int inPTS, bool bIsKeyFrame, int inOffset)
{
	// �߳������˳��У�ֱ�ӷ���...
	if( this->IsStopRequested() )
		return;
	// �����̻߳���״̬��...
	OSMutexLocker theLock(&m_Mutex);
	// �ȼ���ADTSͷ���ټ�������֡����...
	int nTotalSize = ADTS_HEADER_SIZE + inData.size();
	// ����ADTS֡ͷ...
	PutBitContext pb;
	char pbuf[ADTS_HEADER_SIZE * 2] = {0};
	init_put_bits(&pb, pbuf, ADTS_HEADER_SIZE);
    /* adts_fixed_header */    
    put_bits(&pb, 12, 0xfff);   /* syncword */    
    put_bits(&pb, 1, 0);        /* ID */    
    put_bits(&pb, 2, 0);        /* layer */    
    put_bits(&pb, 1, 1);        /* protection_absent */    
    put_bits(&pb, 2, 2);		/* profile_objecttype */    
    put_bits(&pb, 4, m_audio_rate_index);    
    put_bits(&pb, 1, 0);        /* private_bit */    
    put_bits(&pb, 3, m_audio_channel_num); /* channel_configuration */    
    put_bits(&pb, 1, 0);        /* original_copy */    
    put_bits(&pb, 1, 0);        /* home */    
    /* adts_variable_header */    
    put_bits(&pb, 1, 0);        /* copyright_identification_bit */    
    put_bits(&pb, 1, 0);        /* copyright_identification_start */    
	put_bits(&pb, 13, nTotalSize); /* aac_frame_length */    
    put_bits(&pb, 11, 0x7ff);   /* adts_buffer_fullness */    
    put_bits(&pb, 2, 0);        /* number_of_raw_data_blocks_in_frame */    
    
    flush_put_bits(&pb);    

	// ����һ����ʱAVPacket��������֡��������...
	AVPacket  theNewPacket = {0};
	av_new_packet(&theNewPacket, nTotalSize);
	ASSERT(theNewPacket.size == nTotalSize);
	// �����֡ͷ���ݣ������֡������Ϣ...
	memcpy(theNewPacket.data, pbuf, ADTS_HEADER_SIZE);
	memcpy(theNewPacket.data + ADTS_HEADER_SIZE, inData.c_str(), inData.size());
	// ���㵱ǰ֡��PTS���ؼ�֡��־ => ��Ƶ�� => 0
	theNewPacket.pts = inPTS;
	theNewPacket.dts = inPTS - inOffset;
	theNewPacket.flags = bIsKeyFrame;
	theNewPacket.stream_index = 0;
	// ������ѹ�����ǰ���е���...
	this->doPushPacket(theNewPacket);
}

CPlaySDL::CPlaySDL(CUDPRecvThread * inRecvThread)
  : m_lpRecvThread(inRecvThread)
  , m_lpVideoThread(NULL)
  , m_lpAudioThread(NULL)
  , m_play_sys_ts(-1)
  , m_start_pts(-1)
{
	ASSERT( m_lpRecvThread != NULL );
}

CPlaySDL::~CPlaySDL()
{
	// �ͷ�����Ƶ�������...
	if( m_lpAudioThread != NULL ) {
		delete m_lpAudioThread;
		m_lpAudioThread = NULL;
	}
	if( m_lpVideoThread != NULL ) {
		delete m_lpVideoThread;
		m_lpVideoThread = NULL;
	}
}

BOOL CPlaySDL::InitVideo(CRenderWnd * lpRenderWnd, string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS)
{
	// �����µ���Ƶ����...
	if( m_lpVideoThread != NULL ) {
		delete m_lpVideoThread;
		m_lpVideoThread = NULL;
	}
	m_lpVideoThread = new CVideoThread(this);
	return m_lpVideoThread->InitVideo(lpRenderWnd, inSPS, inPPS, nWidth, nHeight, nFPS);
}

BOOL CPlaySDL::InitAudio(int nRateIndex, int nChannelNum)
{
	// �����µ���Ƶ����...
	if( m_lpAudioThread != NULL ) {
		delete m_lpAudioThread;
		m_lpAudioThread = NULL;
	}
	m_lpAudioThread = new CAudioThread(this);
	return m_lpAudioThread->InitAudio(nRateIndex, nChannelNum);
}

void CPlaySDL::PushFrame(string & inData, int inTypeTag, bool bIsKeyFrame, uint32_t inSendTime)
{
	// �жϴ���֡�Ķ����Ƿ���ڣ������ڣ�ֱ�Ӷ���...
	if( inTypeTag == FLV_TAG_TYPE_AUDIO && m_lpAudioThread == NULL )
		return;
	if( inTypeTag == FLV_TAG_TYPE_VIDEO && m_lpVideoThread == NULL )
		return;
	// ��ȡ��һ֡��PTSʱ���...
	if( m_start_pts < 0 ) {
		m_start_pts = inSendTime;
	}
	// ע�⣺�����ݵ���ʱ���Ž���������...
	// ����ϵͳ���ʱ�� => ��������ʱ���...
	if( m_play_sys_ts < 0 ) {
		m_play_sys_ts = CUtilTool::os_gettime_ns();
	}
	// �����ǰ֡��ʱ����ȵ�һ֡��ʱ�����ҪС��ֱ���ӵ�...
	if( inSendTime < m_start_pts )
		return;
	// ���㵱ǰ֡��ʱ���....
	int nCalcPTS = inSendTime - (uint32_t)m_start_pts;

	/////////////////////////////////////////////////////////////
	// ע�⣺��ʱģ��Ŀǰ���Գ����������������ʵ�����ģ��...
	/////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////
	// �����������֡ => ÿ��10�룬��1�������Ƶ����֡...
	//////////////////////////////////////////////////////////////////
	//if( (inFrame.dwSendTime/1000>0) && ((inFrame.dwSendTime/1000)%5==0) ) {
	//	TRACE("[%s] Discard Packet, PTS: %d\n", inFrame.typeFlvTag == FLV_TAG_TYPE_AUDIO ? "Audio" : "Video", nCalcPTS);
	//	return;
	//}

	// ��������Ƶ���ͽ�����ز���...
	if( inTypeTag == FLV_TAG_TYPE_AUDIO ) {
		m_lpAudioThread->doFillPacket(inData, nCalcPTS, bIsKeyFrame, 0);
	} else if( inTypeTag == FLV_TAG_TYPE_VIDEO ) {
		m_lpVideoThread->doFillPacket(inData, nCalcPTS, bIsKeyFrame, 0);
	}
	//TRACE("[%s] RenderOffset: %lu\n", inFrame.typeFlvTag == FLV_TAG_TYPE_AUDIO ? "Audio" : "Video", inFrame.dwRenderOffset);
}

/*static bool DoProcSaveJpeg(AVFrame * pSrcFrame, AVPixelFormat inSrcFormat, int64_t inPTS, LPCTSTR lpPath)
{
	char szSavePath[MAX_PATH] = {0};
	sprintf(szSavePath, "%s/%I64d.jpg", lpPath, inPTS/1000000);
	/////////////////////////////////////////////////////////////////////////
	// ע�⣺input->conversion ����Ҫ�任�ĸ�ʽ��
	// ��ˣ�Ӧ�ô� video->info ���л�ȡԭʼ������Ϣ...
	// ͬʱ��sws_getContext ��ҪAVPixelFormat������video_format��ʽ...
	/////////////////////////////////////////////////////////////////////////
	// ����ffmpeg����־�ص�����...
	//av_log_set_level(AV_LOG_VERBOSE);
	//av_log_set_callback(my_av_logoutput);
	// ͳһ����Դ�����ʽ���ҵ�ѹ������Ҫ�����ظ�ʽ...
	enum AVPixelFormat nDestFormat = AV_PIX_FMT_YUV420P;
	enum AVPixelFormat nSrcFormat = inSrcFormat;
	int nSrcWidth = pSrcFrame->width;
	int nSrcHeight = pSrcFrame->height;
	// ����ʲô��ʽ������Ҫ�������ظ�ʽ��ת��...
	AVFrame * pDestFrame = av_frame_alloc();
	int nDestBufSize = avpicture_get_size(nDestFormat, nSrcWidth, nSrcHeight);
	uint8_t * pDestOutBuf = (uint8_t *)av_malloc(nDestBufSize);
	avpicture_fill((AVPicture *)pDestFrame, pDestOutBuf, nDestFormat, nSrcWidth, nSrcHeight);

	// ע�⣺���ﲻ��libyuv��ԭ���ǣ�ʹ��sws���򵥣����ø��ݲ�ͬ���ظ�ʽ���ò�ͬ�ӿ�...
	// ffmpeg�Դ���sws_scaleת��Ҳ��û������ģ�֮ǰ������������sws_getContext������Դ��Ҫ��ʽAVPixelFormat��д����video_format����ɵĸ�ʽ��λ����...
	// ע�⣺Ŀ�����ظ�ʽ����ΪAV_PIX_FMT_YUVJ420P������ʾ������Ϣ��������Ӱ���ʽת������ˣ�����ʹ�ò��ᾯ���AV_PIX_FMT_YUV420P��ʽ...
	struct SwsContext * img_convert_ctx = sws_getContext(nSrcWidth, nSrcHeight, nSrcFormat, nSrcWidth, nSrcHeight, nDestFormat, SWS_BICUBIC, NULL, NULL, NULL);
	int nReturn = sws_scale(img_convert_ctx, (const uint8_t* const*)pSrcFrame->data, pSrcFrame->linesize, 0, nSrcHeight, pDestFrame->data, pDestFrame->linesize);
	sws_freeContext(img_convert_ctx);

	// ����ת���������֡����...
	pDestFrame->width = nSrcWidth;
	pDestFrame->height = nSrcHeight;
	pDestFrame->format = nDestFormat;

	// ��ת����� YUV ���ݴ��̳� jpg �ļ�...
	AVCodecContext * pOutCodecCtx = NULL;
	bool bRetSave = false;
	do {
		// Ԥ�Ȳ���jpegѹ������Ҫ���������ݸ�ʽ...
		AVOutputFormat * avOutputFormat = av_guess_format("mjpeg", NULL, NULL); //av_guess_format(0, lpszJpgName, 0);
		AVCodec * pOutAVCodec = avcodec_find_encoder(avOutputFormat->video_codec);
		if (pOutAVCodec == NULL)
			break;
		// ����ffmpegѹ�����ĳ�������...
		pOutCodecCtx = avcodec_alloc_context3(pOutAVCodec);
		if (pOutCodecCtx == NULL)
			break;
		// ׼�����ݽṹ��Ҫ�Ĳ���...
		pOutCodecCtx->bit_rate = 200000;
		pOutCodecCtx->width = nSrcWidth;
		pOutCodecCtx->height = nSrcHeight;
		// ע�⣺û��ʹ�����䷽ʽ�����������ʽ�п��ܲ���YUVJ420P�����ѹ������������Ϊ���ݵ������Ѿ��̶���YUV420P...
		// ע�⣺����������YUV420P��ʽ��ѹ����������YUVJ420P��ʽ...
		pOutCodecCtx->pix_fmt = AV_PIX_FMT_YUVJ420P; //avcodec_find_best_pix_fmt_of_list(pOutAVCodec->pix_fmts, (AVPixelFormat)-1, 1, 0);
		pOutCodecCtx->codec_id = avOutputFormat->video_codec; //AV_CODEC_ID_MJPEG;  
		pOutCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
		pOutCodecCtx->time_base.num = 1;
		pOutCodecCtx->time_base.den = 25;
		// �� ffmpeg ѹ����...
		if (avcodec_open2(pOutCodecCtx, pOutAVCodec, 0) < 0)
			break;
		// ����ͼ��������Ĭ����0.5���޸�Ϊ0.8(ͼƬ̫��,0.5�ոպ�)...
		pOutCodecCtx->qcompress = 0.5f;
		// ׼�����ջ��棬��ʼѹ��jpg����...
		int got_pic = 0;
		int nResult = 0;
		AVPacket pkt = { 0 };
		// �����µ�ѹ������...
		nResult = avcodec_encode_video2(pOutCodecCtx, &pkt, pDestFrame, &got_pic);
		// ����ʧ�ܻ�û�еõ�����ͼ�񣬼�������...
		if (nResult < 0 || !got_pic)
			break;
		// ��jpg�ļ����...
		FILE * pFile = fopen(szSavePath, "wb");
		// ��jpgʧ�ܣ�ע���ͷ���Դ...
		if (pFile == NULL) {
			av_packet_unref(&pkt);
			break;
		}
		// ���浽���̣����ͷ���Դ...
		fwrite(pkt.data, 1, pkt.size, pFile);
		av_packet_unref(&pkt);
		// �ͷ��ļ���������سɹ�...
		fclose(pFile); pFile = NULL;
		bRetSave = true;
	} while (false);
	// �����м�����Ķ���...
	if (pOutCodecCtx != NULL) {
		avcodec_close(pOutCodecCtx);
		av_free(pOutCodecCtx);
	}

	// �ͷ���ʱ��������ݿռ�...
	av_frame_free(&pDestFrame);
	av_free(pDestOutBuf);

	return bRetSave;
}*/

/*CVideoDecoder::CVideoDecoder()
  : m_lpRenderWnd(NULL)
  , m_sdlRenderer(NULL)
  , m_sdlTexture(NULL)
  , m_sdlScreen(NULL)
  , m_nHeight(0)
  , m_nWidth(0)
  , m_nFPS(0)
{
}

CVideoDecoder::~CVideoDecoder()
{
	// ����SDL����...
	if( m_sdlScreen != NULL ) {
		SDL_DestroyWindow(m_sdlScreen);
		m_sdlScreen = NULL;
	}
	if( m_sdlRenderer != NULL ) {
		SDL_DestroyRenderer(m_sdlRenderer);
		m_sdlRenderer = NULL;
	}
	if( m_sdlTexture != NULL ) {
		SDL_DestroyTexture(m_sdlTexture);
		m_sdlTexture = NULL;
	}
	// ����SDL����ʱ�����ع�������...
	if( m_lpRenderWnd != NULL ) {
		m_lpRenderWnd->ShowWindow(SW_SHOW);
		m_lpRenderWnd->SetRenderState(CRenderWnd::ST_RENDER);
	}
}

BOOL CVideoDecoder::InitVideo(CRenderWnd * lpRenderWnd, string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS)
{
	// ���洫�ݹ����Ĳ�����Ϣ...
	m_lpRenderWnd = lpRenderWnd;
	m_nHeight = nHeight;
	m_nWidth = nWidth;
	m_nFPS = nFPS;
	m_strSPS = inSPS;
	m_strPPS = inPPS;

	// ���ò��Ŵ���״̬...
	m_lpRenderWnd->SetRenderState(CRenderWnd::ST_WAIT);
	// ����SDL��Ҫ�Ķ���...
	m_sdlScreen = SDL_CreateWindowFrom((void*)m_lpRenderWnd->m_hWnd);
	m_sdlRenderer = SDL_CreateRenderer(m_sdlScreen, -1, 0);
	m_sdlTexture = SDL_CreateTexture(m_sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, m_nWidth, m_nHeight);
	// ��ʼ��ffmpeg������...
	av_register_all();
	// ׼��һЩ�ض��Ĳ���...
	AVCodecID src_codec_id = AV_CODEC_ID_H264;
	// ������Ҫ�Ľ����� => ���ô���������...
	m_lpCodec = avcodec_find_decoder(src_codec_id);
	m_lpDecoder = avcodec_alloc_context3(m_lpCodec);
	// �򿪻�ȡ���Ľ�����...
	int nResult = avcodec_open2(m_lpDecoder, m_lpCodec, NULL);
	return ((nResult == 0) ? true : false);
}
//
// ��Ҫ����Ƶ֡���ݽ����޸Ĵ���...
void CVideoDecoder::doFillPacket(string & inData, int inPTS, bool bIsKeyFrame, int inOffset)
{
	//TRACE("[Video] PTS: %d, Offset: %d, Size: %d\n", inPTS, inOffset, inData.size());
	// ÿ���ؼ�֡������sps��pps�����Ż�ӿ�...
	string strCurFrame;
	// ����ǹؼ�֡��������д��sps����д��pps...
	if( bIsKeyFrame ) {
		DWORD dwTag = 0x01000000;
		strCurFrame.append((char*)&dwTag, sizeof(DWORD));
		strCurFrame.append(m_strSPS);
		strCurFrame.append((char*)&dwTag, sizeof(DWORD));
		strCurFrame.append(m_strPPS);
	}
	// �޸���Ƶ֡����ʼ�� => 0x00000001
	char * lpszSrc = (char*)inData.c_str();
	memset((void*)lpszSrc, 0, sizeof(DWORD));
	lpszSrc[3] = 0x01;
	// ���׷��������...
	strCurFrame.append(inData);
	// ����һ����ʱAVPacket��������֡��������...
	AVPacket  theNewPacket = {0};
	av_new_packet(&theNewPacket, strCurFrame.size());
	ASSERT(theNewPacket.size == strCurFrame.size());
	memcpy(theNewPacket.data, strCurFrame.c_str(), theNewPacket.size);
	// ���㵱ǰ֡��PTS���ؼ�֡��־ => ��Ƶ�� => 1
	theNewPacket.pts = inPTS;
	theNewPacket.dts = inPTS - inOffset;
	theNewPacket.flags = bIsKeyFrame;
	theNewPacket.stream_index = 1;
	// ������ѹ�����ǰ���е���...
	this->doPushPacket(theNewPacket);
}
//
// ȡ��һ֡��������Ƶ���ȶ�ϵͳʱ�䣬�����ܷ񲥷�...
void CVideoDecoder::doDisplaySDL()
{
	// û�л��棬ֱ�ӷ���...
	if( m_MapFrame.size() <= 0 )
		return;
	/////////////////////////////////////////////////////////////
	// ע�⣺��ʱģ��Ŀǰ���Գ����������������ʵ�����ģ��...
	/////////////////////////////////////////////////////////////
	// ��ȡ��ǰ��ϵͳʱ�� => ����...
	int64_t inSysCurNS = os_gettime_ns();
	// ��ȡ��һ����ʱ����С�����ݿ�...
	GM_MapFrame::iterator itorItem = m_MapFrame.begin();
	AVFrame * lpSrcFrame  = itorItem->second;
	int64_t   nFramePTS   = itorItem->first;
	int       nSize = m_MapFrame.size();
	// ��ǰ֡����ʾʱ�仹û�е� => ֱ�ӷ��� => �����ȴ�...
	if( nFramePTS > inSysCurNS ) {
		//TRACE("[Video] Advance: %I64d ms, Wait, Size: %lu\n", (nFramePTS - inSysCurNS)/1000000, nSize);
		return;
	}
	///////////////////////////////////////////////////////////////////////////////////
	// ע�⣺��Ƶ��ʱ֡�����֡�������ܶ��������������ʾ����Ƶ�����ٶ���ԽϿ졣
	///////////////////////////////////////////////////////////////////////////////////
	// ������ת����jpg...
	//DoProcSaveJpeg(lpSrcFrame, m_lpDecoder->pix_fmt, nFramePTS, "F:/MP4/Dst");
	//TRACE("[Video] OS: %I64d ms, Delay: %I64d ms, Success, Size: %d, Type: %d\n", inSysCurNS/1000000, (inSysCurNS - nFramePTS)/1000000, nSize, lpSrcFrame->pict_type);
	// ׼����Ҫת���ĸ�ʽ��Ϣ...
	enum AVPixelFormat nDestFormat = AV_PIX_FMT_YUV420P;
	enum AVPixelFormat nSrcFormat = m_lpDecoder->pix_fmt;
	int nSrcWidth = m_lpDecoder->width;
	int nSrcHeight = m_lpDecoder->height;
	// ����ʲô��ʽ������Ҫ�������ظ�ʽ��ת��...
	AVFrame * pDestFrame = av_frame_alloc();
	int nDestBufSize = avpicture_get_size(nDestFormat, nSrcWidth, nSrcHeight);
	uint8_t * pDestOutBuf = (uint8_t *)av_malloc(nDestBufSize);
	avpicture_fill((AVPicture *)pDestFrame, pDestOutBuf, nDestFormat, nSrcWidth, nSrcHeight);
	// ����ffmpeg�ĸ�ʽת���ӿں���...
	struct SwsContext * img_convert_ctx = sws_getContext(nSrcWidth, nSrcHeight, nSrcFormat, nSrcWidth, nSrcHeight, nDestFormat, SWS_BICUBIC, NULL, NULL, NULL);
	int nReturn = sws_scale(img_convert_ctx, (const uint8_t* const*)lpSrcFrame->data, lpSrcFrame->linesize, 0, nSrcHeight, pDestFrame->data, pDestFrame->linesize);
	sws_freeContext(img_convert_ctx);
	// ����ת���������֡����...
	pDestFrame->width = nSrcWidth;
	pDestFrame->height = nSrcHeight;
	pDestFrame->format = nDestFormat;
	//////////////////////////////////////////////
	// ʹ��SDL ���л�����ƹ���...
	//////////////////////////////////////////////
	int nResult = 0;
	CRect rcRect;
	SDL_Rect srcSdlRect = { 0 };
	SDL_Rect dstSdlRect = { 0 };
	m_lpRenderWnd->GetClientRect(rcRect);
	srcSdlRect.w = nSrcWidth;
	srcSdlRect.h = nSrcHeight;
	dstSdlRect.w = rcRect.Width();
	dstSdlRect.h = rcRect.Height();
	nResult = SDL_UpdateTexture( m_sdlTexture, &srcSdlRect, pDestFrame->data[0], pDestFrame->linesize[0] );
    nResult = SDL_RenderClear( m_sdlRenderer );
    nResult = SDL_RenderCopy( m_sdlRenderer, m_sdlTexture, &srcSdlRect, &dstSdlRect );
    SDL_RenderPresent( m_sdlRenderer );
	// �ͷ���ʱ��������ݿռ�...
	av_frame_free(&pDestFrame);
	av_free(pDestOutBuf);
	// �ͷŲ�ɾ���Ѿ�ʹ�����ԭʼ���ݿ�...
	av_frame_free(&lpSrcFrame);
	m_MapFrame.erase(itorItem);
}

void CVideoDecoder::doDecodeFrame(int64_t inSysZeroNS)
{
	// û�л��棬ֱ�ӷ���...
	if( m_MapPacket.size() <= 0 )
		return;
	// ��ȡһ��AVPacket���н��������һ��AVPacketһ���ܽ����һ��Picture...
	int got_picture = 0, nResult = 0;
	AVFrame  * lpFrame = av_frame_alloc();
	GM_MapPacket::iterator itorItem = m_MapPacket.begin();
	AVPacket & thePacket = itorItem->second;
	nResult = avcodec_decode_video2(m_lpDecoder, lpFrame, &got_picture, &thePacket);
	// ����ʧ�ܻ�û�еõ�����ͼ��ֱ���ӵ�����...
	if( nResult < 0 || !got_picture ) {
		av_frame_free(&lpFrame);
		av_free_packet(&thePacket);
		m_MapPacket.erase(itorItem);
		return;
	}
	//TRACE("[Video] best: %I64d, pkt_dts: %I64d, pkt_pts: %I64d\n", lpFrame->best_effort_timestamp, lpFrame->pkt_dts, lpFrame->pkt_pts);
	//TRACE("[Video] DTS: %I64d, PTS: %I64d, Size: %lu\n", thePacket.dts, thePacket.pts, thePacket.size);
	// �������������֡��ʱ��� => ������ת��������...
	AVRational base_pack  = {1, 1000};
	AVRational base_frame = {1, 1000000000};
	int64_t frame_pts = av_rescale_q(lpFrame->best_effort_timestamp, base_pack, base_frame);
	// �ۼ�ϵͳ��ʱ���...
	frame_pts += inSysZeroNS;
	// ������������֡���벥�Ŷ��е���...
	m_MapFrame[frame_pts] = lpFrame;
	//DoProcSaveJpeg(lpFrame, m_lpDecoder->pix_fmt, frame_pts, "F:/MP4/Src");
	// ���������ã�������free��erase...
	av_free_packet(&thePacket);
	m_MapPacket.erase(itorItem);
}

int64_t CVideoDecoder::doGetMinNextNS()
{
	// �������Ҫ��������ݣ�ֱ�ӷ���-1...
	if( m_MapPacket.size() > 0 )
		return -1;
	// ���û����Ҫ��ʾ�Ľ�������ݣ���Ϣ20����...
	if( m_MapFrame.size() <= 0 ) {
		return os_gettime_ns() + 20000000;
	}
	// ֱ�ӷ�������Ҫ���ŵĵ�һ֡���ݵ�PTS...
	return m_MapFrame.begin()->first;
}

int64_t CAudioDecoder::doGetMinNextNS()
{
	// �������Ҫ��������ݣ�ֱ�ӷ���-1...
	if( m_MapPacket.size() > 0 )
		return -1;
	// ���û����Ҫ��ʾ�Ľ�������ݣ���Ϣ20����...
	if( m_MapAudio.size() <= 0 ) {
		return os_gettime_ns() + 20000000;
	}
	// ֱ�ӷ�������Ҫ���ŵĵ�һ֡���ݵ�PTS...
	return m_MapAudio.begin()->first;
}

void CAudioDecoder::doDecodeFrame(int64_t inSysZeroNS)
{
	// û�л��棬ֱ�ӷ���...
	if( m_MapPacket.size() <= 0 )
		return;
	// ��ȡһ��AVPacket���н��������һ��AVPacketһ���ܽ����һ��Picture...
	int got_picture = 0, nResult = 0;
	AVFrame  * lpFrame = av_frame_alloc();
	GM_MapPacket::iterator itorItem = m_MapPacket.begin();
	AVPacket & thePacket = itorItem->second;
	nResult = avcodec_decode_audio4(m_lpDecoder, lpFrame, &got_picture, &thePacket);
	// ����ʧ�ܻ�û�еõ�����ͼ��ֱ���ӵ�����...
	if( nResult < 0 || !got_picture ) {
		av_frame_free(&lpFrame);
		av_free_packet(&thePacket);
		m_MapPacket.erase(itorItem);
		return;
	}
	// �������������֡��ʱ��� => ������ת��������...
	AVRational base_pack  = {1, 1000};
	AVRational base_frame = {1, 1000000000};
	int64_t frame_pts = av_rescale_q(lpFrame->best_effort_timestamp, base_pack, base_frame);
	// �ۼ�ϵͳ��ʱ���...
	frame_pts += inSysZeroNS;
	// �Խ�������Ƶ��������ת��...
	nResult = swr_convert(m_au_convert_ctx, &m_out_buffer, m_out_buffer_size * 2, (const uint8_t **)lpFrame->data , lpFrame->nb_samples);
	// ת��������ݴ�ŵ�ת����Ķ��е���...
	string strData;
	strData.append((char*)m_out_buffer, m_out_buffer_size);
	m_MapAudio[frame_pts] = strData;
	// ���������ã�������free��erase...
	av_frame_free(&lpFrame);
	av_free_packet(&thePacket);
	m_MapPacket.erase(itorItem);
}

CAudioDecoder::CAudioDecoder(CPlayThread * lpPlayThread)
  : m_lpPlayThread(lpPlayThread)
{
	m_audio_sample_rate = 0;
	m_audio_rate_index = 0;
	m_audio_channel_num = 0;
	m_out_buffer = NULL;
	m_au_convert_ctx = NULL;
	m_out_buffer_size = 0;
}

CAudioDecoder::~CAudioDecoder()
{
	// �رջ�����...
	if( m_out_buffer != NULL ) {
		av_free(m_out_buffer);
		m_out_buffer = NULL;
	}
	// �ر���Ƶ���Ŷ���...
	if( m_au_convert_ctx != NULL ) {
		swr_free(&m_au_convert_ctx);
		m_au_convert_ctx = NULL;
	}
	// �ر���Ƶ...
	SDL_CloseAudio();
}

BOOL CAudioDecoder::InitAudio(int nRateIndex, int nChannelNum)
{
	// ����������ȡ������...
	switch( nRateIndex )
	{
	case 0x03: m_audio_sample_rate = 48000; break;
	case 0x04: m_audio_sample_rate = 44100; break;
	case 0x05: m_audio_sample_rate = 32000; break;
	case 0x06: m_audio_sample_rate = 24000; break;
	case 0x07: m_audio_sample_rate = 22050; break;
	case 0x08: m_audio_sample_rate = 16000; break;
	case 0x09: m_audio_sample_rate = 12000; break;
	case 0x0a: m_audio_sample_rate = 11025; break;
	case 0x0b: m_audio_sample_rate =  8000; break;
	default:   m_audio_sample_rate = 48000; break;
	}
	// �������������������...
	m_audio_rate_index = nRateIndex;
	m_audio_channel_num = nChannelNum;
	// ��ʼ��ffmpeg������...
	av_register_all();
	// ׼��һЩ�ض��Ĳ���...
	AVCodecID src_codec_id = AV_CODEC_ID_AAC;
	// ������Ҫ�Ľ����������������������...
	m_lpCodec = avcodec_find_decoder(src_codec_id);
	m_lpDecoder = avcodec_alloc_context3(m_lpCodec);
	// �򿪻�ȡ���Ľ�����...
	if( avcodec_open2(m_lpDecoder, m_lpCodec, NULL) != 0 )
		return false;
	// �������������������һ����...
	int in_audio_channel_num = m_audio_channel_num;
	int out_audio_channel_num = m_audio_channel_num;
	int64_t in_channel_layout = av_get_default_channel_layout(in_audio_channel_num);
	int64_t out_channel_layout = (out_audio_channel_num <= 1) ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
	// �����Ƶ������ʽ...
	AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
	// out_nb_samples: AAC-1024 MP3-1152
	int out_nb_samples = 1024;
	// ����������������� => ����...
	int in_sample_rate = m_audio_sample_rate;
	int out_sample_rate = m_audio_sample_rate;

	//SDL_AudioSpec => ����ʹ��ϵͳ�Ƽ�����...
	SDL_AudioSpec audioSpec = {0};
	audioSpec.freq = out_sample_rate; 
	audioSpec.format = AUDIO_S16SYS; //m_lpDecoder->sample_fmt => AV_SAMPLE_FMT_FLTP
	audioSpec.channels = out_audio_channel_num; 
	audioSpec.samples = out_nb_samples;
	audioSpec.callback = CPlayThread::do_fill_audio; 
	audioSpec.userdata = m_lpPlayThread;
	audioSpec.silence = 0;

	// ��SDL��Ƶ�豸 => ֻ�ܴ�һ���豸...
	if( SDL_OpenAudio(&audioSpec, NULL) != 0 ) {
		SDL_Log("Failed to open audio: %s", SDL_GetError());
		return false;
	}

	// ��ȡ��Ƶ���������Ļ�������С...
	m_out_buffer_size = av_samples_get_buffer_size(NULL, out_audio_channel_num, out_nb_samples, out_sample_fmt, 1);
	m_out_buffer = (uint8_t *)av_malloc(m_out_buffer_size * 2);
	
	// ���䲢��ʼ��ת����...
	m_au_convert_ctx = swr_alloc();
	m_au_convert_ctx = swr_alloc_set_opts(m_au_convert_ctx, out_channel_layout, out_sample_fmt, out_sample_rate,
										  in_channel_layout, m_lpDecoder->sample_fmt, in_sample_rate, 0, NULL);
	swr_init(m_au_convert_ctx);

	// ��ʼ����...
	SDL_PauseAudio(0);

	return true;
}*/
//
// ��Ҫ����Ƶ֡�������ͷ��Ϣ...
//void CAudioDecoder::doFillPacket(string & inData, int inPTS, bool bIsKeyFrame, int inOffset)
//{
//	// �ȼ���ADTSͷ���ټ�������֡����...
//	int nTotalSize = ADTS_HEADER_SIZE + inData.size();
//	// ����ADTS֡ͷ...
//	PutBitContext pb;
//	char pbuf[ADTS_HEADER_SIZE * 2] = {0};
//	init_put_bits(&pb, pbuf, ADTS_HEADER_SIZE);
//    /* adts_fixed_header */    
//    put_bits(&pb, 12, 0xfff);   /* syncword */    
//    put_bits(&pb, 1, 0);        /* ID */    
//    put_bits(&pb, 2, 0);        /* layer */    
//    put_bits(&pb, 1, 1);        /* protection_absent */    
//    put_bits(&pb, 2, 2);		/* profile_objecttype */    
//    put_bits(&pb, 4, m_audio_rate_index);    
//    put_bits(&pb, 1, 0);        /* private_bit */    
//    put_bits(&pb, 3, m_audio_channel_num); /* channel_configuration */    
//    put_bits(&pb, 1, 0);        /* original_copy */    
//    put_bits(&pb, 1, 0);        /* home */    
//    /* adts_variable_header */    
//    put_bits(&pb, 1, 0);        /* copyright_identification_bit */    
//    put_bits(&pb, 1, 0);        /* copyright_identification_start */    
//	put_bits(&pb, 13, nTotalSize); /* aac_frame_length */    
//    put_bits(&pb, 11, 0x7ff);   /* adts_buffer_fullness */    
//    put_bits(&pb, 2, 0);        /* number_of_raw_data_blocks_in_frame */    
//    
//    flush_put_bits(&pb);    
//
//	// ����һ����ʱAVPacket��������֡��������...
//	AVPacket  theNewPacket = {0};
//	av_new_packet(&theNewPacket, nTotalSize);
//	ASSERT(theNewPacket.size == nTotalSize);
//	// �����֡ͷ���ݣ������֡������Ϣ...
//	memcpy(theNewPacket.data, pbuf, ADTS_HEADER_SIZE);
//	memcpy(theNewPacket.data + ADTS_HEADER_SIZE, inData.c_str(), inData.size());
//	// ���㵱ǰ֡��PTS���ؼ�֡��־ => ��Ƶ�� => 0
//	theNewPacket.pts = inPTS;
//	theNewPacket.dts = inPTS - inOffset;
//	theNewPacket.flags = bIsKeyFrame;
//	theNewPacket.stream_index = 0;
//	// ������ѹ�����ǰ���е���...
//	this->doPushPacket(theNewPacket);
//}

/*void CAudioDecoder::doFillAudio(Uint8 * inStream, int inLen)
{
	// û�л��棬ֱ�ӷ���...
	SDL_memset(inStream, 0, inLen);
	if( m_MapAudio.size() <= 0 )
		return;
	/////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺��������������£�DeltaDelay��Ϊ�˷�ֹ����������ɵ���Ƶ�����ٶ���������
	// ������������ԵĶ���Ƶ������մ�������ʱ�ﵽһ���̶�ʱ����Ҫ����...
	/////////////////////////////////////////////////////////////////////////////////////////////
	// ��ȡ��ǰ��ϵͳʱ�� => ����...
	int64_t inSysCurNS = os_gettime_ns();
	// ��ȡ��һ����ʱ����С�����ݿ�...
	GM_MapAudio::iterator itorItem = m_MapAudio.begin();
	string  & strPCM = itorItem->second;
	int64_t   nFramePTS = itorItem->first;
	int64_t   nDeltaDelay = 200000000; //200����...
	int       nSize = m_MapAudio.size();
	// ��ǰ���ݰ�����ǰ100�������£��������ţ���ǰ100�������ϣ��������ݣ�ֱ�ӷ���...
	if( (nFramePTS > inSysCurNS) && ((nFramePTS - inSysCurNS) >= nDeltaDelay/2) ) {
		TRACE("[Audio] Advance: %I64d ms, Continue, Size: %d\n", (nFramePTS - inSysCurNS)/1000000, nSize);
		return;
	}
	// ��ǰ֡����ʾʱ���г���200������ʱ => �����Ƶ����ǰ�ͽ���󻺳���...
	if( (inSysCurNS - nFramePTS) > nDeltaDelay ) {
		TRACE("[Audio] Clear Audio Buffer: %I64d ms, Size: %d\n", (inSysCurNS - nFramePTS)/1000000, nSize);
		m_MapPacket.clear();
		m_MapAudio.clear();
		return;
	}
	//TRACE("[Audio] OS: %I64d ms, Delay: %I64d ms, Success, Size: %d\n", inSysCurNS/1000000, (inSysCurNS - nFramePTS)/1000000, nSize);
	// ������ݸ�SDL���Ż���...
	int nAudioSize = strPCM.size();
	Uint8 * lpAudioPos = (Uint8*)strPCM.c_str();
	inLen = (inLen > nAudioSize) ? nAudioSize : inLen;
	SDL_MixAudio(inStream, lpAudioPos, inLen, SDL_MIX_MAXVOLUME);
	// �ͷ��Ѿ�ʹ����ϵ���Դ�ռ�...
	m_MapAudio.erase(itorItem);
}

void CPlayThread::do_fill_audio(void * udata, Uint8 *stream, int inLen)
{
	CPlayThread * lpThread = (CPlayThread*)udata;
	lpThread->doFillAudio(stream, inLen);
}

void CPlayThread::doFillAudio(Uint8 * inStream, int inLen)
{
	OSMutexLocker theLock(&m_Mutex);
	if( m_AudioDecoder == NULL )
		return;
	m_AudioDecoder->doFillAudio(inStream, inLen);
}

CPlayThread::CPlayThread(CPushThread * inPushThread)
  : m_lpPushThread(inPushThread)
  , m_VideoDecoder(NULL)
  , m_AudioDecoder(NULL)
  , m_play_next_ns(-1)
  , m_play_sys_ts(-1)
  , m_start_pts(-1)
{
	ASSERT( m_lpPushThread != NULL );

	//int64_t nResult = 0;
	//AVRational r_frame_rate = {1, 24000};
	//AVRational time_base = {1, 1000};
	//nResult = av_rescale_q(-2002, r_frame_rate, time_base);
}

CPlayThread::~CPlayThread()
{
	// �ȴ��߳��˳�...
	this->StopAndWaitForThread();
	// �ͷ�����Ƶ�������...
	if( m_AudioDecoder != NULL ) {
		delete m_AudioDecoder;
		m_AudioDecoder = NULL;
	}
	if( m_VideoDecoder != NULL ) {
		delete m_VideoDecoder;
		m_VideoDecoder = NULL;
	}
}
//
// ���롢��ʾ����...
void CPlayThread::Entry()
{
	while( !this->IsStopRequested() ) {
		// ����sleep�ȴ�...
		this->doSleepTo();
		// ����һ֡��Ƶ...
		this->doDecodeVideo();
		// ����һ֡��Ƶ...
		this->doDecodeAudio();
		// ��ʾһ֡��Ƶ...
		this->doDisplaySDL();
		// ������Ϣʱ��...
		this->doCalcNextNS();
	}
}

void CPlayThread::doSleepTo()
{
	// < 0 ������Ϣ��ֱ�ӷ���...
	if( m_play_next_ns < 0 )
		return;
	// �����Ϣ200����...
	uint64_t cur_time_ns = os_gettime_ns();
	const uint64_t timeout_ns = 200000000;
	// ����ȵ�ǰʱ��С(�ѹ���)��ֱ�ӷ���...
	if( m_play_next_ns <= cur_time_ns )
		return;
	// ���㳬ǰʱ��Ĳ�ֵ�������Ϣ200����...
	uint64_t delta_ns = m_play_next_ns - cur_time_ns;
	delta_ns = ((delta_ns < timeout_ns) ? delta_ns : timeout_ns);
	//TRACE("[Video] Sleep: %I64d ms\n", delta_ns/1000000);
	os_sleepto_ns(cur_time_ns + delta_ns);
}

void CPlayThread::doDecodeVideo()
{
	OSMutexLocker theLock(&m_Mutex);
	if( m_VideoDecoder == NULL )
		return;
	m_VideoDecoder->doDecodeFrame(m_play_sys_ts);
}

void CPlayThread::doDecodeAudio()
{
	OSMutexLocker theLock(&m_Mutex);
	if( m_AudioDecoder == NULL )
		return;
	m_AudioDecoder->doDecodeFrame(m_play_sys_ts);
}

void CPlayThread::doDisplaySDL()
{
	OSMutexLocker theLock(&m_Mutex);
	if( m_VideoDecoder == NULL )
		return;
	m_VideoDecoder->doDisplaySDL();
}

void CPlayThread::doCalcNextNS()
{
	OSMutexLocker theLock(&m_Mutex);
	int64_t min_next_ns = 0x7FFFFFFFFFFFFFFFLL;
	// �����Ƶ��һ֡��ʱ���...
	if( m_VideoDecoder != NULL ) {
		int64_t min_video_ns = m_VideoDecoder->doGetMinNextNS();
		if( min_video_ns < min_next_ns )
			min_next_ns = min_video_ns;
	}
	//// ��Ƶ�Ǳ�����ȡ���ݣ����ü��...
	//if( m_AudioDecoder != NULL ) {
	//	int64_t min_audio_ns = m_AudioDecoder->doGetMinNextNS();
	//	if( min_audio_ns < min_next_ns  )
	//		min_next_ns = min_audio_ns;
	//}
	//m_play_next_ns = min_next_ns;
}

//
// ��ʼ����Ƶ...
BOOL CPlayThread::InitVideo(CRenderWnd * lpRenderWnd, string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS)
{
	OSMutexLocker theLock(&m_Mutex);
	// �����µ���Ƶ����...
	if( m_VideoDecoder != NULL ) {
		delete m_VideoDecoder;
		m_VideoDecoder = NULL;
	}
	m_VideoDecoder = new CVideoDecoder();
	if( !m_VideoDecoder->InitVideo(lpRenderWnd, inSPS, inPPS, nWidth, nHeight, nFPS) )
		return false;
	// �߳�û��������ֱ������...
	if( this->GetThreadHandle() == NULL ) {
		this->Start();
	}
	return true;
}
//
// ��ʼ����Ƶ...
BOOL CPlayThread::InitAudio(int nRateIndex, int nChannelNum)
{
	OSMutexLocker theLock(&m_Mutex);
	// �����µ���Ƶ����...
	if( m_AudioDecoder != NULL ) {
		delete m_AudioDecoder;
		m_AudioDecoder = NULL;
	}
	m_AudioDecoder = new CAudioDecoder(this);
	if( !m_AudioDecoder->InitAudio(nRateIndex, nChannelNum) )
		return false;
	// �߳�û��������ֱ������...
	if( this->GetThreadHandle() == NULL ) {
		this->Start();
	}
	return true;
}
//
// ������Ƶ����Ƶ����֡...
void CPlayThread::PushFrame(FMS_FRAME & inFrame)
{
	OSMutexLocker theLock(&m_Mutex);
	// ����߳��Ѿ��˳��ˣ�ֱ�ӷ���...
	if( this->IsStopRequested() )
		return;
	// �жϴ���֡�Ķ����Ƿ���ڣ������ڣ�ֱ�Ӷ���...
	if( inFrame.typeFlvTag == FLV_TAG_TYPE_AUDIO && m_AudioDecoder == NULL )
		return;
	if( inFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO && m_VideoDecoder == NULL )
		return;
	// ��ȡ��һ֡��PTSʱ���...
	if( m_start_pts < 0 ) {
		m_start_pts = inFrame.dwSendTime;
	}
	// ע�⣺�����ݵ���ʱ���Ž���������...
	// ����ϵͳ���ʱ�� => ��������ʱ���...
	if( m_play_sys_ts < 0 ) {
		m_play_sys_ts = os_gettime_ns();
	}
	// �����ǰ֡��ʱ����ȵ�һ֡��ʱ�����ҪС��ֱ���ӵ�...
	if( inFrame.dwSendTime < m_start_pts )
		return;
	// ��������Ƶ���ͽ�����ز���...
	int nCalcPTS = inFrame.dwSendTime - (uint32_t)m_start_pts;

	/////////////////////////////////////////////////////////////
	// ע�⣺��ʱģ��Ŀǰ���Գ����������������ʵ�����ģ��...
	/////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////
	// �����������֡ => ÿ��10�룬��1�������Ƶ����֡...
	//////////////////////////////////////////////////////////////////
	//if( (inFrame.dwSendTime/1000>0) && ((inFrame.dwSendTime/1000)%5==0) ) {
	//	TRACE("[%s] Discard Packet, PTS: %d\n", inFrame.typeFlvTag == FLV_TAG_TYPE_AUDIO ? "Audio" : "Video", nCalcPTS);
	//	return;
	//}

	if( inFrame.typeFlvTag == FLV_TAG_TYPE_AUDIO ) {
		m_AudioDecoder->doFillPacket(inFrame.strData, nCalcPTS, inFrame.is_keyframe, inFrame.dwRenderOffset);
	} else if( inFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO ) {
		m_VideoDecoder->doFillPacket(inFrame.strData, nCalcPTS, inFrame.is_keyframe, inFrame.dwRenderOffset);
	}
	//TRACE("[%s] RenderOffset: %lu\n", inFrame.typeFlvTag == FLV_TAG_TYPE_AUDIO ? "Audio" : "Video", inFrame.dwRenderOffset);
}*/

/*CVideoThread::CVideoThread(string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS)
{
	//AVRational r_frame_rate = {25, 1};
	//AVRational time_base = {1, 1000};
	//int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(r_frame_rate);
	//int64_t theDuration = (double)calc_duration/(double)(av_q2d(time_base)*AV_TIME_BASE);

	m_sdlScreen = NULL;
	m_sdlTexture = NULL;
	m_sdlRenderer = NULL;
	m_lpPushThread = NULL;
	m_lpRenderWnd = NULL;
	m_strSPS = inSPS;
	m_strPPS = inPPS;
	m_nWidth = nWidth;
	m_nHeight = nHeight;
	m_nFPS = ((nFPS <= 0) ? 25 : nFPS);

	m_lpSrcCodecParserCtx = NULL;
	m_lpSrcCodecCtx = NULL;
	m_lpSrcCodec = NULL;
	m_lpSrcFrame = NULL;
}

CVideoThread::~CVideoThread()
{
	// �ȴ��߳��˳�...
	this->StopAndWaitForThread();
	// �ͷ�Ԥ���Ѿ�����Ŀռ�...
	if( m_lpSrcCodecParserCtx != NULL ) {
		av_parser_close(m_lpSrcCodecParserCtx);
		m_lpSrcCodecParserCtx = NULL;
	}
	if( m_lpSrcFrame != NULL ) {
		av_frame_free(&m_lpSrcFrame);
		m_lpSrcFrame = NULL;
	}
	if( m_lpSrcCodecCtx != NULL ) {
		avcodec_close(m_lpSrcCodecCtx);
		av_free(m_lpSrcCodecCtx);
	}
	if( m_sdlScreen != NULL ) {
		SDL_DestroyWindow(m_sdlScreen);
		m_sdlScreen = NULL;
	}
	if( m_sdlRenderer != NULL ) {
		SDL_DestroyRenderer(m_sdlRenderer);
		m_sdlRenderer = NULL;
	}
	if( m_sdlTexture != NULL ) {
		SDL_DestroyTexture(m_sdlTexture);
		m_sdlTexture = NULL;
	}
	// ����SDL����ʱ�����ع�������...
	m_lpRenderWnd->ShowWindow(SW_SHOW);
	m_lpRenderWnd->SetRenderState(CRenderWnd::ST_RENDER);
}

BOOL CVideoThread::InitThread(CPushThread * inPushThread, CRenderWnd * lpRenderWnd)
{
	OSMutexLocker theLock(&m_Mutex);

	// ���洰�ں͸�����...
	m_lpRenderWnd = lpRenderWnd;
	m_lpPushThread = inPushThread;
	m_lpRenderWnd->SetRenderState(CRenderWnd::ST_WAIT);

	m_sdlScreen = SDL_CreateWindowFrom((void*)m_lpRenderWnd->m_hWnd);
	m_sdlRenderer = SDL_CreateRenderer(m_sdlScreen, -1, 0);
	m_sdlTexture = SDL_CreateTexture(m_sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, m_nWidth, m_nHeight);

	// ��ʼ��ffmpeg������...
	av_register_all();
	// ׼��һЩ�ض��Ĳ���...
	AVCodecID src_codec_id = AV_CODEC_ID_H264;
	// ������Ҫ�Ľ����������������������...
	m_lpSrcCodec = avcodec_find_decoder(src_codec_id);
	m_lpSrcCodecCtx = avcodec_alloc_context3(m_lpSrcCodec);
	m_lpSrcCodecParserCtx = av_parser_init(src_codec_id);
	// �⼸����������˵�ܽ�����ʱ�����񲢲�̫���� => �Ա�������...
	//m_lpSrcCodecCtx->time_base.den = 1;
	//m_lpSrcCodecCtx->time_base.num = 75;
	//av_opt_set(m_lpSrcCodecCtx->priv_data, "preset", "superfast", 0);
	//av_opt_set(m_lpSrcCodecCtx->priv_data, "tune", "zerolatency", 0);
	// �򿪻�ȡ���Ľ�����...
	int nResult = avcodec_open2(m_lpSrcCodecCtx, m_lpSrcCodec, NULL);
	// ������������߳�...
	this->Start();
	return true;
}

void CVideoThread::PushFrame(string & inFrame, bool bIsKeyFrame)
{
	OSMutexLocker theLock(&m_Mutex);
	// ����ǹؼ�֡��������д��sps����д��pps...
	// ÿ���ؼ�֡������sps��pps�����Ż�ӿ�...
	if( bIsKeyFrame ) {
		DWORD dwTag = 0x01000000;
		m_strFrame.append((char*)&dwTag, sizeof(DWORD));
		m_strFrame.append(m_strSPS);
		m_strFrame.append((char*)&dwTag, sizeof(DWORD));
		m_strFrame.append(m_strPPS);
	}
	// �޸���Ƶ֡����ʼ�� => 0x00000001
	char * lpszSrc = (char*)inFrame.c_str();
	memset((void*)lpszSrc, 0, sizeof(DWORD));
	lpszSrc[3] = 0x01;
	// ���׷��������...
	m_strFrame.append(inFrame);
}

int CVideoThread::DecodeFrame()
{
	OSMutexLocker theLock(&m_Mutex);
	// ���û�����ݣ�ֱ�ӷ���...
	if( m_strFrame.size() <= 0 )
		return 0;
	// ���ͷ��ϴη���Ļ�����...
	if( m_lpSrcFrame != NULL ) {
		av_frame_free(&m_lpSrcFrame);
		m_lpSrcFrame = NULL;
	}
	// ��ʼ��ffmpeg����֡...
	BOOL      bReturn = false;
	AVPacket  theSrcPacket = {0};
	m_lpSrcFrame = av_frame_alloc();
	av_init_packet(&theSrcPacket);
	// �������ݹ�����h264����֡...
	uint8_t * lpCurPtr = (uint8_t*)m_strFrame.c_str();
	int nCurSize = m_strFrame.size();
	int got_picture = 0;
	int nParseSize = 0;
	int	nResult = 0;
	while( nCurSize > 0 ) {
		// ������Ҫ��ν�����ֱ�����������еĻ���Ϊֹ...
		nResult = av_parser_parse2( m_lpSrcCodecParserCtx, m_lpSrcCodecCtx,
						  &theSrcPacket.data, &theSrcPacket.size,
						  lpCurPtr, nCurSize, AV_NOPTS_VALUE,
						  AV_NOPTS_VALUE, AV_NOPTS_VALUE);
		// û�н�����packet���ƶ�����ָ��...
		if( nResult > 0 ) {
			lpCurPtr += nResult;
			nCurSize -= nResult;
		}
		//TRACE("PackSize: %lu, Result: %lu\n", theSrcPacket.size, nResult);
		// û�н�����packet������...
		if( theSrcPacket.size == 0 )
			continue;
		// ��������ý������ĳ���...
		nParseSize += theSrcPacket.size;
		//TRACE("Packet: %lu, Parsed: %lu\n", theSrcPacket.size, nParseSize);
		// �Խ�����ȷ��packet���н������...
		nResult = avcodec_decode_video2(m_lpSrcCodecCtx, m_lpSrcFrame, &got_picture, &theSrcPacket);
		// ����ʧ�ܻ�û�еõ�����ͼ�񣬼�������...
		if( nResult < 0 || !got_picture )
			continue;
		// ����ɹ������һ�ȡ��һ��ͼ�񣬽��д��̲���...
		this->DisplaySDL();
		// ���óɹ���־���ж�ѭ��...
		bReturn = true;
		// ɾ���Ѿ�����Ļ�����...
		m_strFrame.erase(0, nParseSize);
		TRACE("Video => Parsed: %lu, RemainFrame: %lu, Type: %lu\n", nParseSize, m_strFrame.size(), m_lpSrcFrame->pict_type);
		break;
	}
	// ���õ������ݽ���������...
	av_free_packet(&theSrcPacket);
	return (bReturn ? 1 : 0);
}
//
// YUV����ͨ��SDL2.0��ʾ���� => OpenGL
void CVideoThread::DisplaySDL()
{
	if( m_lpSrcCodecCtx == NULL || m_lpSrcFrame == NULL )
		return;
	enum AVPixelFormat nDestFormat = AV_PIX_FMT_YUV420P;
	enum AVPixelFormat nSrcFormat = m_lpSrcCodecCtx->pix_fmt;
	int nSrcWidth = m_lpSrcCodecCtx->width;
	int nSrcHeight = m_lpSrcCodecCtx->height;
	// ����ʲô��ʽ������Ҫ�������ظ�ʽ��ת��...
	AVFrame * pDestFrame = av_frame_alloc();
	int nDestBufSize = avpicture_get_size(nDestFormat, nSrcWidth, nSrcHeight);
	uint8_t * pDestOutBuf = (uint8_t *)av_malloc(nDestBufSize);
	avpicture_fill((AVPicture *)pDestFrame, pDestOutBuf, nDestFormat, nSrcWidth, nSrcHeight);
	// ����ffmpeg�ĸ�ʽת���ӿں���...
	struct SwsContext * img_convert_ctx = sws_getContext(nSrcWidth, nSrcHeight, nSrcFormat, nSrcWidth, nSrcHeight, nDestFormat, SWS_BICUBIC, NULL, NULL, NULL);
	int nReturn = sws_scale(img_convert_ctx, (const uint8_t* const*)m_lpSrcFrame->data, m_lpSrcFrame->linesize, 0, nSrcHeight, pDestFrame->data, pDestFrame->linesize);
	sws_freeContext(img_convert_ctx);
	// ����ת���������֡����...
	pDestFrame->width = nSrcWidth;
	pDestFrame->height = nSrcHeight;
	pDestFrame->format = nDestFormat;
	//////////////////////////////////////////////
	// ʹ��SDL ���л�����ƹ���...
	//////////////////////////////////////////////
	int nResult = 0;
	CRect rcRect;
	SDL_Rect srcSdlRect = { 0 };
	SDL_Rect dstSdlRect = { 0 };
	m_lpRenderWnd->GetClientRect(rcRect);
	srcSdlRect.w = nSrcWidth;
	srcSdlRect.h = nSrcHeight;
	dstSdlRect.w = rcRect.Width();
	dstSdlRect.h = rcRect.Height();
	nResult = SDL_UpdateTexture( m_sdlTexture, &srcSdlRect, pDestFrame->data[0], pDestFrame->linesize[0] );
    nResult = SDL_RenderClear( m_sdlRenderer );
    nResult = SDL_RenderCopy( m_sdlRenderer, m_sdlTexture, &srcSdlRect, &dstSdlRect );
    SDL_RenderPresent( m_sdlRenderer );
	//SDL_Delay(40);
	// �ͷ���ʱ��������ݿռ�...
	av_frame_free(&pDestFrame);
	av_free(pDestOutBuf);
}

void CVideoThread::Entry()
{
	while( !this->IsStopRequested() ) {
		if( this->DecodeFrame() <= 0 ) {
			::Sleep(50); continue;
		}
		this->DisplaySDL();
	}
}

CAudioThread::CAudioThread(int nRateIndex, int nChannelNum)
{
	// ����������ȡ������...
	switch( nRateIndex )
	{
	case 0x03: m_audio_sample_rate = 48000; break;
	case 0x04: m_audio_sample_rate = 44100; break;
	case 0x05: m_audio_sample_rate = 32000; break;
	case 0x06: m_audio_sample_rate = 24000; break;
	case 0x07: m_audio_sample_rate = 22050; break;
	case 0x08: m_audio_sample_rate = 16000; break;
	case 0x09: m_audio_sample_rate = 12000; break;
	case 0x0a: m_audio_sample_rate = 11025; break;
	case 0x0b: m_audio_sample_rate =  8000; break;
	default:   m_audio_sample_rate = 48000; break;
	}
	m_audio_rate_index = nRateIndex;
	m_audio_channel_num = nChannelNum;

	m_lpPushThread = NULL;

	m_lpSrcCodecParserCtx = NULL;
	m_lpSrcCodecCtx = NULL;
	m_lpSrcCodec = NULL;
	m_lpSrcFrame = NULL;

	m_out_buffer = NULL;
	m_au_convert_ctx = NULL;
	m_out_buffer_size = 0;
}

CAudioThread::~CAudioThread()
{
	// �ȴ��߳��˳�...
	this->StopAndWaitForThread();
	// �ͷ�Ԥ���Ѿ�����Ŀռ�...
	if( m_lpSrcCodecParserCtx != NULL ) {
		av_parser_close(m_lpSrcCodecParserCtx);
		m_lpSrcCodecParserCtx = NULL;
	}
	if( m_lpSrcFrame != NULL ) {
		av_frame_free(&m_lpSrcFrame);
		m_lpSrcFrame = NULL;
	}
	if( m_lpSrcCodecCtx != NULL ) {
		avcodec_close(m_lpSrcCodecCtx);
		av_free(m_lpSrcCodecCtx);
	}
	if( m_out_buffer != NULL ) {
		av_free(m_out_buffer);
		m_out_buffer = NULL;
	}
	if( m_au_convert_ctx != NULL ) {
		swr_free(&m_au_convert_ctx);
		m_au_convert_ctx = NULL;
	}
	// �ر���Ƶ...
	SDL_CloseAudio();
}

void do_fill_audio(void * udata, Uint8 *stream, int inLen)
{
	CAudioThread * lpThread = (CAudioThread*)udata;
	lpThread->DisplaySDL(stream, inLen);
}

BOOL CAudioThread::InitThread(CPushThread * inPushThread)
{
	OSMutexLocker theLock(&m_Mutex);
	m_lpPushThread = inPushThread;

	// ��ʼ��ffmpeg������...
	av_register_all();
	// ׼��һЩ�ض��Ĳ���...
	AVCodecID src_codec_id = AV_CODEC_ID_AAC;
	// ������Ҫ�Ľ����������������������...
	m_lpSrcCodec = avcodec_find_decoder(src_codec_id);
	m_lpSrcCodecCtx = avcodec_alloc_context3(m_lpSrcCodec);
	m_lpSrcCodecParserCtx = av_parser_init(src_codec_id);
	// �򿪻�ȡ���Ľ�����...
	int nResult = avcodec_open2(m_lpSrcCodecCtx, m_lpSrcCodec, NULL);

	// �������������������һ����...
	int in_audio_channel_num = m_audio_channel_num;
	int out_audio_channel_num = m_audio_channel_num;
	int64_t in_channel_layout = av_get_default_channel_layout(in_audio_channel_num);
	int64_t out_channel_layout = (out_audio_channel_num <= 1) ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
	// �����Ƶ������ʽ...
	AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
	// out_nb_samples: AAC-1024 MP3-1152
	int out_nb_samples = 1024;
	// ����������������� => ����...
	int in_sample_rate = m_audio_sample_rate;
	int out_sample_rate = m_audio_sample_rate;

	//SDL_AudioSpec
	SDL_AudioSpec audioSpec = {0};
	audioSpec.freq = out_sample_rate; 
	audioSpec.format = AUDIO_S16SYS; 
	audioSpec.channels = out_audio_channel_num; 
	audioSpec.samples = out_nb_samples;
	audioSpec.callback = do_fill_audio; 
	audioSpec.userdata = this;
	audioSpec.silence = 0;

	// ��SDL��Ƶ�豸 => ֻ�ܴ�һ���豸...
	nResult = SDL_OpenAudio(&audioSpec, NULL);
	
	// ��ȡ��Ƶ���������Ļ�������С...
	m_out_buffer_size = av_samples_get_buffer_size(NULL, out_audio_channel_num, out_nb_samples, out_sample_fmt, 1);
	m_out_buffer = (uint8_t *)av_malloc(m_out_buffer_size * 2);
	
	// ���䲢��ʼ��ת����...
	m_au_convert_ctx = swr_alloc();
	m_au_convert_ctx = swr_alloc_set_opts(m_au_convert_ctx, out_channel_layout, out_sample_fmt, out_sample_rate,
										  in_channel_layout, m_lpSrcCodecCtx->sample_fmt, in_sample_rate, 0, NULL);
	swr_init(m_au_convert_ctx);

	// ��ʼ����...
	SDL_PauseAudio(0);

	// ������������߳�...
	this->Start();

	return true;
}

void CAudioThread::PushFrame(string & inFrame)
{
	OSMutexLocker theLock(&m_Mutex);
	// ����ADTS֡ͷ...
	PutBitContext pb;
	char pbuf[ADTS_HEADER_SIZE * 2] = {0};
	init_put_bits(&pb, pbuf, ADTS_HEADER_SIZE);
    // adts_fixed_header     
    put_bits(&pb, 12, 0xfff);   // syncword    
    put_bits(&pb, 1, 0);        // ID    
    put_bits(&pb, 2, 0);        // layer    
    put_bits(&pb, 1, 1);        // protection_absent    
    put_bits(&pb, 2, 2);		// profile_objecttype    
    put_bits(&pb, 4, m_audio_rate_index);    
    put_bits(&pb, 1, 0);        // private_bit    
    put_bits(&pb, 3, m_audio_channel_num); // channel_configuration
    put_bits(&pb, 1, 0);        // original_copy
    put_bits(&pb, 1, 0);        // home
    // adts_variable_header    
    put_bits(&pb, 1, 0);        // copyright_identification_bit    
    put_bits(&pb, 1, 0);        // copyright_identification_start    
	put_bits(&pb, 13, ADTS_HEADER_SIZE + inFrame.size()); // aac_frame_length
    put_bits(&pb, 11, 0x7ff);   // adts_buffer_fullness
    put_bits(&pb, 2, 0);        // number_of_raw_data_blocks_in_frame    
    
    flush_put_bits(&pb);    

	// �ȼ���ADTSͷ���ټ�������֡����...
	m_strFrame.append(pbuf, ADTS_HEADER_SIZE);
	m_strFrame.append(inFrame);
}

int CAudioThread::DecodeFrame()
{
	OSMutexLocker theLock(&m_Mutex);
	// ���û�����ݣ�ֱ�ӷ���...
	if( m_strFrame.size() <= 0 )
		return 0;
	// ���ͷ��ϴη���Ļ�����...
	if( m_lpSrcFrame != NULL ) {
		av_frame_free(&m_lpSrcFrame);
		m_lpSrcFrame = NULL;
	}
	// ��ʼ��ffmpeg����֡...
	BOOL      bReturn = false;
	AVPacket  theSrcPacket = {0};
	m_lpSrcFrame = av_frame_alloc();
	av_init_packet(&theSrcPacket);

	// �������ݹ�����aac����֡...
	uint8_t * lpCurPtr = (uint8_t*)m_strFrame.c_str();
	int nCurSize = m_strFrame.size();
	int got_picture = 0;
	int nParseSize = 0;
	int	nResult = 0;
	while( nCurSize > 0 ) {
		// ������Ҫ��ν�����ֱ�����������еĻ���Ϊֹ...
		nResult = av_parser_parse2( m_lpSrcCodecParserCtx, m_lpSrcCodecCtx,
						  &theSrcPacket.data, &theSrcPacket.size,
						  lpCurPtr, nCurSize, AV_NOPTS_VALUE,
						  AV_NOPTS_VALUE, AV_NOPTS_VALUE);
		// û�н�����packet���ƶ�����ָ��...
		if( nResult > 0 ) {
			lpCurPtr += nResult;
			nCurSize -= nResult;
		}
		//TRACE("PackSize: %lu, Result: %lu\n", theSrcPacket.size, nResult);
		// û�н�����packet������...
		if( theSrcPacket.size == 0 )
			continue;
		// ��������ý������ĳ���...
		nParseSize += theSrcPacket.size;
		//TRACE("Packet: %lu, Parsed: %lu\n", theSrcPacket.size, nParseSize);
		// �Խ�����ȷ��packet���н������...
		nResult = avcodec_decode_audio4(m_lpSrcCodecCtx, m_lpSrcFrame, &got_picture, &theSrcPacket);
		// ����ʧ�ܻ�û�еõ�����ͼ�񣬼�������...
		if( nResult < 0 || !got_picture )
			continue;
		nResult = swr_convert(m_au_convert_ctx, &m_out_buffer, m_out_buffer_size * 2, (const uint8_t **)m_lpSrcFrame->data , m_lpSrcFrame->nb_samples);
		int nOutSize = nResult * m_audio_channel_num * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
		//av_samples_get_buffer_size(NULL, out_audio_channel_num, out_nb_samples, out_sample_fmt, 1);
		m_strPCM.append((char*)m_out_buffer, m_out_buffer_size);
		//TRACE("pts:%lld\t packet size:%d\n", theSrcPacket.pts, theSrcPacket.size);
		// ���óɹ���־���ж�ѭ��...
		bReturn = true;
		// ɾ���Ѿ�����Ļ�����...
		m_strFrame.erase(0, nParseSize);
		//TRACE("Audio => Parsed: %lu, RemainFrame: %lu, RemainPCM: %lu\n", nParseSize, m_strFrame.size(), m_strPCM.size()-m_out_buffer_size);
		break;
	}
	// ���õ������ݽ���������...
	av_free_packet(&theSrcPacket);
	return (bReturn ? 1 : 0);
}

void CAudioThread::DisplaySDL(Uint8 * inStream, int inLen)
{
	OSMutexLocker theLock(&m_Mutex);
	SDL_memset(inStream, 0, inLen);
	if( m_strPCM.size() <= 0 )
		return;
	int nAudioSize = m_strPCM.size();
	Uint8 * lpAudioPos = (Uint8*)m_strPCM.c_str();
	inLen = (inLen > nAudioSize) ? nAudioSize : inLen;
	SDL_MixAudio(inStream, lpAudioPos, inLen, SDL_MIX_MAXVOLUME);
	m_strPCM.erase(0, inLen);
}

void CAudioThread::Entry()
{
	while( !this->IsStopRequested() ) {
		if( this->DecodeFrame() <= 0 ) {
			::Sleep(50); continue;
		}
		// Wait until finish...
		//while( m_audio_len > 0 ) {
		//	SDL_Delay(1);
		}
		//Set audio buffer (PCM data)
		//m_audio_chunk = (Uint8 *)m_out_buffer; 
		//Audio buffer length
		//m_audio_len = m_out_buffer_size;
		//m_audio_pos = m_audio_chunk;
	}
}*/
