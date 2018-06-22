
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
	// 释放解码器对象...
	if( m_lpDecoder != NULL ) {
		avcodec_close(m_lpDecoder);
		av_free(m_lpDecoder);
	}
	// 释放队列中的解码前的数据块...
	GM_MapPacket::iterator itorPack;
	for(itorPack = m_MapPacket.begin(); itorPack != m_MapPacket.end(); ++itorPack) {
		av_free_packet(&itorPack->second);
	}
	m_MapPacket.clear();
	// 释放没有播放完毕的解码后的数据帧...
	GM_MapFrame::iterator itorFrame;
	for(itorFrame = m_MapFrame.begin(); itorFrame != m_MapFrame.end(); ++itorFrame) {
		av_frame_free(&itorFrame->second);
	}
	m_MapFrame.clear();
}

void CDecoder::doPushPacket(AVPacket & inPacket)
{
	// 注意：这里必须以DTS排序，决定了解码的先后顺序...
	// 如果有相同DTS的数据帧已经存在，直接释放AVPacket，返回...
	if( m_MapPacket.find(inPacket.dts) != m_MapPacket.end() ) {
		av_free_packet(&inPacket);
		return;
	}
	// 如果没有相同DTS的数据帧，保存起来...
	m_MapPacket[inPacket.dts] = inPacket;
}

void CDecoder::doSleepTo()
{
	// < 0 不能休息，直接返回...
	if( m_play_next_ns < 0 )
		return;
	// 最多休息50毫秒...
	uint64_t cur_time_ns = CUtilTool::os_gettime_ns();
	const uint64_t timeout_ns = 50000000;
	// 如果比当前时间小(已过期)，直接返回...
	if( m_play_next_ns <= cur_time_ns )
		return;
	// 计算超前时间的差值，最多休息50毫秒...
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
	// 等待线程退出...
	this->StopAndWaitForThread();
	// 销毁SDL窗口...
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
	// 销毁SDL窗口时会隐藏关联窗口...
	if( m_lpRenderWnd != NULL ) {
		m_lpRenderWnd->ShowWindow(SW_SHOW);
		m_lpRenderWnd->SetRenderState(CRenderWnd::ST_RENDER);
	}
}

BOOL CVideoThread::InitVideo(CRenderWnd * lpRenderWnd, string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS)
{
	// 保存传递过来的参数信息...
	m_lpRenderWnd = lpRenderWnd;
	m_nHeight = nHeight;
	m_nWidth = nWidth;
	m_nFPS = nFPS;
	m_strSPS = inSPS;
	m_strPPS = inPPS;

	// 设置播放窗口状态...
	m_lpRenderWnd->SetRenderState(CRenderWnd::ST_WAIT);
	// 创建SDL需要的对象...
	m_sdlScreen = SDL_CreateWindowFrom((void*)m_lpRenderWnd->m_hWnd);
	m_sdlRenderer = SDL_CreateRenderer(m_sdlScreen, -1, 0);
	m_sdlTexture = SDL_CreateTexture(m_sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, m_nWidth, m_nHeight);
	// 初始化ffmpeg解码器...
	av_register_all();
	// 准备一些特定的参数...
	AVCodecID src_codec_id = AV_CODEC_ID_H264;
	// 查找需要的解码器 => 不用创建解析器...
	m_lpCodec = avcodec_find_decoder(src_codec_id);
	m_lpDecoder = avcodec_alloc_context3(m_lpCodec);
	// 打开获取到的解码器...
	if( avcodec_open2(m_lpDecoder, m_lpCodec, NULL) < 0 ) {
		TRACE("[Video] avcodec_open2 failed.\n");
		return false;
	}
	// 启动线程开始运转...
	this->Start();
	return true;
}

void CVideoThread::Entry()
{
	while( !this->IsStopRequested() ) {
		// 进行sleep等待...
		this->doSleepTo();
		// 解码一帧视频...
		this->doDecodeFrame();
		// 显示一帧视频...
		this->doDisplaySDL();
		// 计算休息时间...
		this->doCalcNextNS();
	}
}

void CVideoThread::doCalcNextNS()
{
	OSMutexLocker theLock(&m_Mutex);
	// 如果有需要解码的数据，直接返回-1...
	if( m_MapPacket.size() > 0 ) {
		m_play_next_ns = -1;
		return;
	}
	// 如果没有需要显示的解码后数据，休息20毫秒...
	if( m_MapFrame.size() <= 0 ) {
		m_play_next_ns = CUtilTool::os_gettime_ns() + 20000000;
		return;
	}
	// 直接返回马上要播放的第一帧数据的PTS...
	m_play_next_ns = m_MapFrame.begin()->first;
}

void CVideoThread::doFillPacket(string & inData, int inPTS, bool bIsKeyFrame, int inOffset)
{
	// 线程正在退出中，直接返回...
	if( this->IsStopRequested() )
		return;
	// 进入线程互斥状态中...
	OSMutexLocker theLock(&m_Mutex);
	//TRACE("[Video] PTS: %d, Offset: %d, Size: %d\n", inPTS, inOffset, inData.size());
	// 每个关键帧都放入sps和pps，播放会加快...
	string strCurFrame;
	// 如果是关键帧，必须先写入sps，再写如pps...
	if( bIsKeyFrame ) {
		DWORD dwTag = 0x01000000;
		strCurFrame.append((char*)&dwTag, sizeof(DWORD));
		strCurFrame.append(m_strSPS);
		strCurFrame.append((char*)&dwTag, sizeof(DWORD));
		strCurFrame.append(m_strPPS);
	}
	// 修改视频帧的起始码 => 0x00000001
	char * lpszSrc = (char*)inData.c_str();
	memset((void*)lpszSrc, 0, sizeof(DWORD));
	lpszSrc[3] = 0x01;
	// 最后追加新数据...
	strCurFrame.append(inData);
	// 构造一个临时AVPacket，并存入帧数据内容...
	AVPacket  theNewPacket = {0};
	av_new_packet(&theNewPacket, strCurFrame.size());
	ASSERT(theNewPacket.size == strCurFrame.size());
	memcpy(theNewPacket.data, strCurFrame.c_str(), theNewPacket.size);
	// 计算当前帧的PTS，关键帧标志 => 视频流 => 1
	theNewPacket.pts = inPTS;
	theNewPacket.dts = inPTS - inOffset;
	theNewPacket.flags = bIsKeyFrame;
	theNewPacket.stream_index = 1;
	// 将数据压入解码前队列当中...
	this->doPushPacket(theNewPacket);
}
//
// 取出一帧解码后的视频，比对系统时间，看看能否播放...
void CVideoThread::doDisplaySDL()
{
	OSMutexLocker theLock(&m_Mutex);
	// 没有缓存，直接返回...
	if( m_MapFrame.size() <= 0 )
		return;
	/////////////////////////////////////////////////////////////
	// 注意：延时模拟目前测试出来，后续配合网络实测后再模拟...
	/////////////////////////////////////////////////////////////
	// 获取当前的系统时间 => 纳秒...
	int64_t inSysCurNS = CUtilTool::os_gettime_ns();
	// 获取第一个，时间最小的数据块...
	GM_MapFrame::iterator itorItem = m_MapFrame.begin();
	AVFrame * lpSrcFrame  = itorItem->second;
	int64_t   nFramePTS   = itorItem->first;
	int       nSize = m_MapFrame.size();
	// 当前帧的显示时间还没有到 => 直接返回 => 继续等待...
	if( nFramePTS > inSysCurNS ) {
		//TRACE("[Video] Advance: %I64d ms, Wait, Size: %lu\n", (nFramePTS - inSysCurNS)/1000000, nSize);
		return;
	}
	///////////////////////////////////////////////////////////////////////////////////
	// 注意：视频延时帧（落后帧），不能丢弃，必须继续显示，视频消耗速度相对较快。
	///////////////////////////////////////////////////////////////////////////////////
	// 将数据转换成jpg...
	//DoProcSaveJpeg(lpSrcFrame, m_lpDecoder->pix_fmt, nFramePTS, "F:/MP4/Dst");
	TRACE("[Video] OS: %I64d ms, Delay: %I64d ms, Success, Size: %d, Type: %d\n", inSysCurNS/1000000, (inSysCurNS - nFramePTS)/1000000, nSize, lpSrcFrame->pict_type);
	// 准备需要转换的格式信息...
	enum AVPixelFormat nDestFormat = AV_PIX_FMT_YUV420P;
	enum AVPixelFormat nSrcFormat = m_lpDecoder->pix_fmt;
	int nSrcWidth = m_lpDecoder->width;
	int nSrcHeight = m_lpDecoder->height;
	// 不管什么格式，都需要进行像素格式的转换...
	AVFrame * pDestFrame = av_frame_alloc();
	int nDestBufSize = avpicture_get_size(nDestFormat, nSrcWidth, nSrcHeight);
	uint8_t * pDestOutBuf = (uint8_t *)av_malloc(nDestBufSize);
	avpicture_fill((AVPicture *)pDestFrame, pDestOutBuf, nDestFormat, nSrcWidth, nSrcHeight);
	// 调用ffmpeg的格式转换接口函数...
	struct SwsContext * img_convert_ctx = sws_getContext(nSrcWidth, nSrcHeight, nSrcFormat, nSrcWidth, nSrcHeight, nDestFormat, SWS_BICUBIC, NULL, NULL, NULL);
	int nReturn = sws_scale(img_convert_ctx, (const uint8_t* const*)lpSrcFrame->data, lpSrcFrame->linesize, 0, nSrcHeight, pDestFrame->data, pDestFrame->linesize);
	sws_freeContext(img_convert_ctx);
	// 设置转换后的数据帧内容...
	pDestFrame->width = nSrcWidth;
	pDestFrame->height = nSrcHeight;
	pDestFrame->format = nDestFormat;
	//////////////////////////////////////////////
	// 使用SDL 进行画面绘制工作...
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
	// 释放临时分配的数据空间...
	av_frame_free(&pDestFrame);
	av_free(pDestOutBuf);
	// 释放并删除已经使用完毕原始数据块...
	av_frame_free(&lpSrcFrame);
	m_MapFrame.erase(itorItem);
}

void CVideoThread::doDecodeFrame()
{
	OSMutexLocker theLock(&m_Mutex);
	// 没有缓存，直接返回...
	if( m_MapPacket.size() <= 0 )
		return;
	int64_t inSysZeroNS = m_lpPlaySDL->GetSysZeroNS();
	// 抽取一个AVPacket进行解码操作，一个AVPacket一定能解码出一个Picture...
	int got_picture = 0, nResult = 0;
	AVFrame  * lpFrame = av_frame_alloc();
	GM_MapPacket::iterator itorItem = m_MapPacket.begin();
	AVPacket & thePacket = itorItem->second;
	nResult = avcodec_decode_video2(m_lpDecoder, lpFrame, &got_picture, &thePacket);
	// 解码失败或没有得到完整图像，直接扔掉数据...
	if( nResult < 0 || !got_picture ) {
		TRACE("[Video] PTS: %I64d, decode_frame failed.\n", thePacket.pts);
		av_frame_free(&lpFrame);
		av_free_packet(&thePacket);
		m_MapPacket.erase(itorItem);
		return;
	}
	//TRACE("[Video] best: %I64d, pkt_dts: %I64d, pkt_pts: %I64d\n", lpFrame->best_effort_timestamp, lpFrame->pkt_dts, lpFrame->pkt_pts);
	//TRACE("[Video] DTS: %I64d, PTS: %I64d, Size: %lu\n", thePacket.dts, thePacket.pts, thePacket.size);
	// 计算解码后的数据帧的时间戳 => 将毫秒转换成纳秒...
	AVRational base_pack  = {1, 1000};
	AVRational base_frame = {1, 1000000000};
	int64_t frame_pts = av_rescale_q(lpFrame->best_effort_timestamp, base_pack, base_frame);
	// 累加系统计时零点...
	frame_pts += inSysZeroNS;
	// 将解码后的数据帧放入播放队列当中...
	m_MapFrame[frame_pts] = lpFrame;
	//DoProcSaveJpeg(lpFrame, m_lpDecoder->pix_fmt, frame_pts, "F:/MP4/Src");
	// 这里是引用，必须先free再erase...
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
	// 等待线程退出...
	this->StopAndWaitForThread();
	// 关闭缓冲区...
	if( m_out_buffer != NULL ) {
		av_free(m_out_buffer);
		m_out_buffer = NULL;
	}
	// 关闭音频播放对象...
	if( m_au_convert_ctx != NULL ) {
		swr_free(&m_au_convert_ctx);
		m_au_convert_ctx = NULL;
	}
	// 关闭音频设备...
	SDL_CloseAudio();
	m_nDeviceID = 0;
}

void CAudioThread::doDecodeFrame()
{
	OSMutexLocker theLock(&m_Mutex);
	// 没有缓存，直接返回...
	if( m_MapPacket.size() <= 0 || m_nDeviceID <= 0 )
		return;
	int64_t inSysZeroNS = m_lpPlaySDL->GetSysZeroNS();
	// 抽取一个AVPacket进行解码操作，一个AVPacket一定能解码出一个Picture...
	int got_picture = 0, nResult = 0;
	AVFrame  * lpFrame = av_frame_alloc();
	GM_MapPacket::iterator itorItem = m_MapPacket.begin();
	AVPacket & thePacket = itorItem->second;
	nResult = avcodec_decode_audio4(m_lpDecoder, lpFrame, &got_picture, &thePacket);
	// 解码失败或没有得到完整图像，直接扔掉数据...
	if( nResult < 0 || !got_picture ) {
		av_frame_free(&lpFrame);
		av_free_packet(&thePacket);
		m_MapPacket.erase(itorItem);
		return;
	}
	// 计算解码后的数据帧的时间戳 => 将毫秒转换成纳秒...
	AVRational base_pack  = {1, 1000};
	AVRational base_frame = {1, 1000000000};
	int64_t frame_pts = av_rescale_q(lpFrame->best_effort_timestamp, base_pack, base_frame);
	// 累加系统计时零点...
	frame_pts += inSysZeroNS;
	// 对解码后的音频进行类型转换...
	nResult = swr_convert(m_au_convert_ctx, &m_out_buffer, m_out_buffer_size * 2, (const uint8_t **)lpFrame->data , lpFrame->nb_samples);
	// 转换后的数据存放到转换后的队列当中...
	string strData;
	strData.append((char*)m_out_buffer, m_out_buffer_size);
	m_MapAudio[frame_pts] = strData;
	// 这里是引用，必须先free再erase...
	av_frame_free(&lpFrame);
	av_free_packet(&thePacket);
	m_MapPacket.erase(itorItem);
}

BOOL CAudioThread::InitAudio(int nRateIndex, int nChannelNum)
{
	OSMutexLocker theLock(&m_Mutex);
	// 根据索引获取采样率...
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
	// 保存采样率索引和声道...
	m_audio_rate_index = nRateIndex;
	m_audio_channel_num = nChannelNum;
	// 初始化ffmpeg解码器...
	av_register_all();
	// 准备一些特定的参数...
	AVCodecID src_codec_id = AV_CODEC_ID_AAC;
	// 查找需要的解码器和相关容器、解析器...
	m_lpCodec = avcodec_find_decoder(src_codec_id);
	m_lpDecoder = avcodec_alloc_context3(m_lpCodec);
	// 打开获取到的解码器...
	if( avcodec_open2(m_lpDecoder, m_lpCodec, NULL) != 0 )
		return false;
	// 输入声道和输出声道是一样的...
	int in_audio_channel_num = m_audio_channel_num;
	int out_audio_channel_num = m_audio_channel_num;
	int64_t in_channel_layout = av_get_default_channel_layout(in_audio_channel_num);
	int64_t out_channel_layout = (out_audio_channel_num <= 1) ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
	// 输出音频采样格式...
	AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
	// out_nb_samples: AAC-1024 MP3-1152
	int out_nb_samples = 1024;
	// 设置输入输出采样率 => 不变...
	int in_sample_rate = m_audio_sample_rate;
	int out_sample_rate = m_audio_sample_rate;

	//SDL_AudioSpec => 不能使用系统推荐参数 => 不用回调模式，主动投递...
	SDL_AudioSpec audioSpec = {0};
	audioSpec.freq = out_sample_rate; 
	audioSpec.format = AUDIO_S16SYS; //m_lpDecoder->sample_fmt => AV_SAMPLE_FMT_FLTP
	audioSpec.channels = out_audio_channel_num; 
	audioSpec.samples = out_nb_samples;
	audioSpec.callback = NULL; 
	audioSpec.userdata = NULL;
	audioSpec.silence = 0;

	// 打开SDL音频设备 => 只能打开一个设备...
	if( SDL_OpenAudio(&audioSpec, NULL) != 0 ) {
		SDL_Log("Failed to open audio: %s", SDL_GetError());
		return false;
	}
	// 计算每帧间隔时间 => 毫秒...
	m_nSampleDuration = out_nb_samples * 1000 / out_sample_rate;
	// 设置打开的主设备编号...
	m_nDeviceID = 1;
	// 获取音频解码后输出的缓冲区大小...
	m_out_buffer_size = av_samples_get_buffer_size(NULL, out_audio_channel_num, out_nb_samples, out_sample_fmt, 1);
	m_out_buffer = (uint8_t *)av_malloc(m_out_buffer_size * 2);
	
	// 分配并初始化转换器...
	m_au_convert_ctx = swr_alloc();
	m_au_convert_ctx = swr_alloc_set_opts(m_au_convert_ctx, out_channel_layout, out_sample_fmt, out_sample_rate,
										  in_channel_layout, m_lpDecoder->sample_fmt, in_sample_rate, 0, NULL);
	swr_init(m_au_convert_ctx);

	// 开始播放 => 使用默认主设备...
	SDL_PauseAudioDevice(m_nDeviceID, 0);

	// 启动线程...
	this->Start();

	return true;
}

void CAudioThread::doDisplaySDL()
{
	//////////////////////////////////////////////////////////////////
	// 注意：使用主动投递方式，可以有效降低回调造成的延时...
	//////////////////////////////////////////////////////////////////
	OSMutexLocker theLock(&m_Mutex);
	// 没有缓存，直接返回...
	if( m_MapAudio.size() <= 0 || m_nDeviceID <= 0 )
		return;
	// 获取当前的系统时间 => 纳秒...
	int64_t inSysCurNS = CUtilTool::os_gettime_ns();
	// 获取第一个，时间最小的数据块...
	GM_MapAudio::iterator itorItem = m_MapAudio.begin();
	string  & stringPCM  = itorItem->second;
	int64_t   nFramePTS  = itorItem->first;
	// 不能超前投递数据，会造成硬件层数据堆积，造成缓存积压，引发缓存清理...
	if( nFramePTS > inSysCurNS ) {
		//TRACE("[Audio] Advance: %I64d ms, AudioSize: %d, QueueBytes: %lu\n", (nFramePTS - inSysCurNS)/1000000, m_MapAudio.size(), nQueueBytes);
		return;
	}
	// 投递数据前，先查看正在排队的音频数据 => 缓存超过300毫秒就清理...
	int nAllowDelay = 300;
	int nAllowSample = nAllowDelay / m_nSampleDuration;
	int nQueueBytes = SDL_GetQueuedAudioSize(m_nDeviceID);
	int nQueueSample = nQueueBytes / m_out_buffer_size;
	if( nQueueSample > nAllowSample ) {
		TRACE("[Audio] Clear Audio Buffer, QueueBytes: %lu\n", nQueueBytes);
		SDL_ClearQueuedAudio(m_nDeviceID);
	}
	// 将音频解码后的数据帧投递给音频设备...
	if( SDL_QueueAudio(m_nDeviceID, stringPCM.c_str(), stringPCM.size()) < 0 ) {
		SDL_Log("Failed to queue audio: %s", SDL_GetError());
		return;
	}
	// 打印已经投递的音频数据信息...
	nQueueBytes = SDL_GetQueuedAudioSize(m_nDeviceID);
	TRACE("[Audio] OS: %I64d ms, Delay: %I64d ms, AudioSize: %d, QueueBytes: %lu\n", inSysCurNS/1000000, (inSysCurNS - nFramePTS)/1000000, m_MapAudio.size(), nQueueBytes);
	// 删除已经使用的音频数据...
	m_MapAudio.erase(itorItem);
}

void CAudioThread::doCalcNextNS()
{
	OSMutexLocker theLock(&m_Mutex);
	// 如果有需要解码的数据，直接返回-1...
	if( m_MapPacket.size() > 0 ) {
		m_play_next_ns = -1;
		return;
	}
	// 如果没有需要显示的解码后数据，休息20毫秒...
	if( m_MapAudio.size() <= 0 ) {
		m_play_next_ns = CUtilTool::os_gettime_ns() + 20000000;
		return;
	}
	// 直接返回马上要播放的第一帧数据的PTS...
	m_play_next_ns = m_MapAudio.begin()->first;
}

void CAudioThread::Entry()
{
	// 注意：提高线程优先级，并不能解决音频受系统干扰问题...
	// 设置线程优先级 => 最高级，防止外部干扰...
	//if( !::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_HIGHEST) ) {
	//	TRACE("[Audio] SetThreadPriority to THREAD_PRIORITY_HIGHEST failed.\n");
	//}
	while( !this->IsStopRequested() ) {
		// 进行sleep等待...
		this->doSleepTo();
		// 解码一帧视频...
		this->doDecodeFrame();
		// 显示一帧视频...
		this->doDisplaySDL();
		// 计算休息时间...
		this->doCalcNextNS();
	}
}
//
// 需要对音频帧数据添加头信息...
void CAudioThread::doFillPacket(string & inData, int inPTS, bool bIsKeyFrame, int inOffset)
{
	// 线程正在退出中，直接返回...
	if( this->IsStopRequested() )
		return;
	// 进入线程互斥状态中...
	OSMutexLocker theLock(&m_Mutex);
	// 先加入ADTS头，再加入数据帧内容...
	int nTotalSize = ADTS_HEADER_SIZE + inData.size();
	// 构造ADTS帧头...
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

	// 构造一个临时AVPacket，并存入帧数据内容...
	AVPacket  theNewPacket = {0};
	av_new_packet(&theNewPacket, nTotalSize);
	ASSERT(theNewPacket.size == nTotalSize);
	// 先添加帧头数据，再添加帧内容信息...
	memcpy(theNewPacket.data, pbuf, ADTS_HEADER_SIZE);
	memcpy(theNewPacket.data + ADTS_HEADER_SIZE, inData.c_str(), inData.size());
	// 计算当前帧的PTS，关键帧标志 => 音频流 => 0
	theNewPacket.pts = inPTS;
	theNewPacket.dts = inPTS - inOffset;
	theNewPacket.flags = bIsKeyFrame;
	theNewPacket.stream_index = 0;
	// 将数据压入解码前队列当中...
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
	// 释放音视频解码对象...
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
	// 创建新的视频对象...
	if( m_lpVideoThread != NULL ) {
		delete m_lpVideoThread;
		m_lpVideoThread = NULL;
	}
	m_lpVideoThread = new CVideoThread(this);
	return m_lpVideoThread->InitVideo(lpRenderWnd, inSPS, inPPS, nWidth, nHeight, nFPS);
}

BOOL CPlaySDL::InitAudio(int nRateIndex, int nChannelNum)
{
	// 创建新的音频对象...
	if( m_lpAudioThread != NULL ) {
		delete m_lpAudioThread;
		m_lpAudioThread = NULL;
	}
	m_lpAudioThread = new CAudioThread(this);
	return m_lpAudioThread->InitAudio(nRateIndex, nChannelNum);
}

void CPlaySDL::PushFrame(string & inData, int inTypeTag, bool bIsKeyFrame, uint32_t inSendTime)
{
	// 判断处理帧的对象是否存在，不存在，直接丢弃...
	if( inTypeTag == FLV_TAG_TYPE_AUDIO && m_lpAudioThread == NULL )
		return;
	if( inTypeTag == FLV_TAG_TYPE_VIDEO && m_lpVideoThread == NULL )
		return;
	// 获取第一帧的PTS时间戳...
	if( m_start_pts < 0 ) {
		m_start_pts = inSendTime;
	}
	// 注意：有数据到达时，才进行零点计算...
	// 设置系统零点时间 => 播放启动时间戳...
	if( m_play_sys_ts < 0 ) {
		m_play_sys_ts = CUtilTool::os_gettime_ns();
	}
	// 如果当前帧的时间戳比第一帧的时间戳还要小，直接扔掉...
	if( inSendTime < m_start_pts )
		return;
	// 计算当前帧的时间戳....
	int nCalcPTS = inSendTime - (uint32_t)m_start_pts;

	/////////////////////////////////////////////////////////////
	// 注意：延时模拟目前测试出来，后续配合网络实测后再模拟...
	/////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////
	// 随机丢掉数据帧 => 每隔10秒，丢1秒的音视频数据帧...
	//////////////////////////////////////////////////////////////////
	//if( (inFrame.dwSendTime/1000>0) && ((inFrame.dwSendTime/1000)%5==0) ) {
	//	TRACE("[%s] Discard Packet, PTS: %d\n", inFrame.typeFlvTag == FLV_TAG_TYPE_AUDIO ? "Audio" : "Video", nCalcPTS);
	//	return;
	//}

	// 根据音视频类型进行相关操作...
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
	// 注意：input->conversion 是需要变换的格式，
	// 因此，应该从 video->info 当中获取原始数据信息...
	// 同时，sws_getContext 需要AVPixelFormat而不是video_format格式...
	/////////////////////////////////////////////////////////////////////////
	// 设置ffmpeg的日志回调函数...
	//av_log_set_level(AV_LOG_VERBOSE);
	//av_log_set_callback(my_av_logoutput);
	// 统一数据源输入格式，找到压缩器需要的像素格式...
	enum AVPixelFormat nDestFormat = AV_PIX_FMT_YUV420P;
	enum AVPixelFormat nSrcFormat = inSrcFormat;
	int nSrcWidth = pSrcFrame->width;
	int nSrcHeight = pSrcFrame->height;
	// 不管什么格式，都需要进行像素格式的转换...
	AVFrame * pDestFrame = av_frame_alloc();
	int nDestBufSize = avpicture_get_size(nDestFormat, nSrcWidth, nSrcHeight);
	uint8_t * pDestOutBuf = (uint8_t *)av_malloc(nDestBufSize);
	avpicture_fill((AVPicture *)pDestFrame, pDestOutBuf, nDestFormat, nSrcWidth, nSrcHeight);

	// 注意：这里不用libyuv的原因是，使用sws更简单，不用根据不同像素格式调用不同接口...
	// ffmpeg自带的sws_scale转换也是没有问题的，之前有问题是由于sws_getContext的输入源需要格式AVPixelFormat，写成了video_format，造成的格式错位问题...
	// 注意：目的像素格式不能为AV_PIX_FMT_YUVJ420P，会提示警告信息，但并不影响格式转换，因此，还是使用不会警告的AV_PIX_FMT_YUV420P格式...
	struct SwsContext * img_convert_ctx = sws_getContext(nSrcWidth, nSrcHeight, nSrcFormat, nSrcWidth, nSrcHeight, nDestFormat, SWS_BICUBIC, NULL, NULL, NULL);
	int nReturn = sws_scale(img_convert_ctx, (const uint8_t* const*)pSrcFrame->data, pSrcFrame->linesize, 0, nSrcHeight, pDestFrame->data, pDestFrame->linesize);
	sws_freeContext(img_convert_ctx);

	// 设置转换后的数据帧内容...
	pDestFrame->width = nSrcWidth;
	pDestFrame->height = nSrcHeight;
	pDestFrame->format = nDestFormat;

	// 将转换后的 YUV 数据存盘成 jpg 文件...
	AVCodecContext * pOutCodecCtx = NULL;
	bool bRetSave = false;
	do {
		// 预先查找jpeg压缩器需要的输入数据格式...
		AVOutputFormat * avOutputFormat = av_guess_format("mjpeg", NULL, NULL); //av_guess_format(0, lpszJpgName, 0);
		AVCodec * pOutAVCodec = avcodec_find_encoder(avOutputFormat->video_codec);
		if (pOutAVCodec == NULL)
			break;
		// 创建ffmpeg压缩器的场景对象...
		pOutCodecCtx = avcodec_alloc_context3(pOutAVCodec);
		if (pOutCodecCtx == NULL)
			break;
		// 准备数据结构需要的参数...
		pOutCodecCtx->bit_rate = 200000;
		pOutCodecCtx->width = nSrcWidth;
		pOutCodecCtx->height = nSrcHeight;
		// 注意：没有使用适配方式，适配出来格式有可能不是YUVJ420P，造成压缩器崩溃，因为传递的数据已经固定成YUV420P...
		// 注意：输入像素是YUV420P格式，压缩器像素是YUVJ420P格式...
		pOutCodecCtx->pix_fmt = AV_PIX_FMT_YUVJ420P; //avcodec_find_best_pix_fmt_of_list(pOutAVCodec->pix_fmts, (AVPixelFormat)-1, 1, 0);
		pOutCodecCtx->codec_id = avOutputFormat->video_codec; //AV_CODEC_ID_MJPEG;  
		pOutCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
		pOutCodecCtx->time_base.num = 1;
		pOutCodecCtx->time_base.den = 25;
		// 打开 ffmpeg 压缩器...
		if (avcodec_open2(pOutCodecCtx, pOutAVCodec, 0) < 0)
			break;
		// 设置图像质量，默认是0.5，修改为0.8(图片太大,0.5刚刚好)...
		pOutCodecCtx->qcompress = 0.5f;
		// 准备接收缓存，开始压缩jpg数据...
		int got_pic = 0;
		int nResult = 0;
		AVPacket pkt = { 0 };
		// 采用新的压缩函数...
		nResult = avcodec_encode_video2(pOutCodecCtx, &pkt, pDestFrame, &got_pic);
		// 解码失败或没有得到完整图像，继续解析...
		if (nResult < 0 || !got_pic)
			break;
		// 打开jpg文件句柄...
		FILE * pFile = fopen(szSavePath, "wb");
		// 打开jpg失败，注意释放资源...
		if (pFile == NULL) {
			av_packet_unref(&pkt);
			break;
		}
		// 保存到磁盘，并释放资源...
		fwrite(pkt.data, 1, pkt.size, pFile);
		av_packet_unref(&pkt);
		// 释放文件句柄，返回成功...
		fclose(pFile); pFile = NULL;
		bRetSave = true;
	} while (false);
	// 清理中间产生的对象...
	if (pOutCodecCtx != NULL) {
		avcodec_close(pOutCodecCtx);
		av_free(pOutCodecCtx);
	}

	// 释放临时分配的数据空间...
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
	// 销毁SDL窗口...
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
	// 销毁SDL窗口时会隐藏关联窗口...
	if( m_lpRenderWnd != NULL ) {
		m_lpRenderWnd->ShowWindow(SW_SHOW);
		m_lpRenderWnd->SetRenderState(CRenderWnd::ST_RENDER);
	}
}

BOOL CVideoDecoder::InitVideo(CRenderWnd * lpRenderWnd, string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS)
{
	// 保存传递过来的参数信息...
	m_lpRenderWnd = lpRenderWnd;
	m_nHeight = nHeight;
	m_nWidth = nWidth;
	m_nFPS = nFPS;
	m_strSPS = inSPS;
	m_strPPS = inPPS;

	// 设置播放窗口状态...
	m_lpRenderWnd->SetRenderState(CRenderWnd::ST_WAIT);
	// 创建SDL需要的对象...
	m_sdlScreen = SDL_CreateWindowFrom((void*)m_lpRenderWnd->m_hWnd);
	m_sdlRenderer = SDL_CreateRenderer(m_sdlScreen, -1, 0);
	m_sdlTexture = SDL_CreateTexture(m_sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, m_nWidth, m_nHeight);
	// 初始化ffmpeg解码器...
	av_register_all();
	// 准备一些特定的参数...
	AVCodecID src_codec_id = AV_CODEC_ID_H264;
	// 查找需要的解码器 => 不用创建解析器...
	m_lpCodec = avcodec_find_decoder(src_codec_id);
	m_lpDecoder = avcodec_alloc_context3(m_lpCodec);
	// 打开获取到的解码器...
	int nResult = avcodec_open2(m_lpDecoder, m_lpCodec, NULL);
	return ((nResult == 0) ? true : false);
}
//
// 需要对视频帧数据进行修改处理...
void CVideoDecoder::doFillPacket(string & inData, int inPTS, bool bIsKeyFrame, int inOffset)
{
	//TRACE("[Video] PTS: %d, Offset: %d, Size: %d\n", inPTS, inOffset, inData.size());
	// 每个关键帧都放入sps和pps，播放会加快...
	string strCurFrame;
	// 如果是关键帧，必须先写入sps，再写如pps...
	if( bIsKeyFrame ) {
		DWORD dwTag = 0x01000000;
		strCurFrame.append((char*)&dwTag, sizeof(DWORD));
		strCurFrame.append(m_strSPS);
		strCurFrame.append((char*)&dwTag, sizeof(DWORD));
		strCurFrame.append(m_strPPS);
	}
	// 修改视频帧的起始码 => 0x00000001
	char * lpszSrc = (char*)inData.c_str();
	memset((void*)lpszSrc, 0, sizeof(DWORD));
	lpszSrc[3] = 0x01;
	// 最后追加新数据...
	strCurFrame.append(inData);
	// 构造一个临时AVPacket，并存入帧数据内容...
	AVPacket  theNewPacket = {0};
	av_new_packet(&theNewPacket, strCurFrame.size());
	ASSERT(theNewPacket.size == strCurFrame.size());
	memcpy(theNewPacket.data, strCurFrame.c_str(), theNewPacket.size);
	// 计算当前帧的PTS，关键帧标志 => 视频流 => 1
	theNewPacket.pts = inPTS;
	theNewPacket.dts = inPTS - inOffset;
	theNewPacket.flags = bIsKeyFrame;
	theNewPacket.stream_index = 1;
	// 将数据压入解码前队列当中...
	this->doPushPacket(theNewPacket);
}
//
// 取出一帧解码后的视频，比对系统时间，看看能否播放...
void CVideoDecoder::doDisplaySDL()
{
	// 没有缓存，直接返回...
	if( m_MapFrame.size() <= 0 )
		return;
	/////////////////////////////////////////////////////////////
	// 注意：延时模拟目前测试出来，后续配合网络实测后再模拟...
	/////////////////////////////////////////////////////////////
	// 获取当前的系统时间 => 纳秒...
	int64_t inSysCurNS = os_gettime_ns();
	// 获取第一个，时间最小的数据块...
	GM_MapFrame::iterator itorItem = m_MapFrame.begin();
	AVFrame * lpSrcFrame  = itorItem->second;
	int64_t   nFramePTS   = itorItem->first;
	int       nSize = m_MapFrame.size();
	// 当前帧的显示时间还没有到 => 直接返回 => 继续等待...
	if( nFramePTS > inSysCurNS ) {
		//TRACE("[Video] Advance: %I64d ms, Wait, Size: %lu\n", (nFramePTS - inSysCurNS)/1000000, nSize);
		return;
	}
	///////////////////////////////////////////////////////////////////////////////////
	// 注意：视频延时帧（落后帧），不能丢弃，必须继续显示，视频消耗速度相对较快。
	///////////////////////////////////////////////////////////////////////////////////
	// 将数据转换成jpg...
	//DoProcSaveJpeg(lpSrcFrame, m_lpDecoder->pix_fmt, nFramePTS, "F:/MP4/Dst");
	//TRACE("[Video] OS: %I64d ms, Delay: %I64d ms, Success, Size: %d, Type: %d\n", inSysCurNS/1000000, (inSysCurNS - nFramePTS)/1000000, nSize, lpSrcFrame->pict_type);
	// 准备需要转换的格式信息...
	enum AVPixelFormat nDestFormat = AV_PIX_FMT_YUV420P;
	enum AVPixelFormat nSrcFormat = m_lpDecoder->pix_fmt;
	int nSrcWidth = m_lpDecoder->width;
	int nSrcHeight = m_lpDecoder->height;
	// 不管什么格式，都需要进行像素格式的转换...
	AVFrame * pDestFrame = av_frame_alloc();
	int nDestBufSize = avpicture_get_size(nDestFormat, nSrcWidth, nSrcHeight);
	uint8_t * pDestOutBuf = (uint8_t *)av_malloc(nDestBufSize);
	avpicture_fill((AVPicture *)pDestFrame, pDestOutBuf, nDestFormat, nSrcWidth, nSrcHeight);
	// 调用ffmpeg的格式转换接口函数...
	struct SwsContext * img_convert_ctx = sws_getContext(nSrcWidth, nSrcHeight, nSrcFormat, nSrcWidth, nSrcHeight, nDestFormat, SWS_BICUBIC, NULL, NULL, NULL);
	int nReturn = sws_scale(img_convert_ctx, (const uint8_t* const*)lpSrcFrame->data, lpSrcFrame->linesize, 0, nSrcHeight, pDestFrame->data, pDestFrame->linesize);
	sws_freeContext(img_convert_ctx);
	// 设置转换后的数据帧内容...
	pDestFrame->width = nSrcWidth;
	pDestFrame->height = nSrcHeight;
	pDestFrame->format = nDestFormat;
	//////////////////////////////////////////////
	// 使用SDL 进行画面绘制工作...
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
	// 释放临时分配的数据空间...
	av_frame_free(&pDestFrame);
	av_free(pDestOutBuf);
	// 释放并删除已经使用完毕原始数据块...
	av_frame_free(&lpSrcFrame);
	m_MapFrame.erase(itorItem);
}

void CVideoDecoder::doDecodeFrame(int64_t inSysZeroNS)
{
	// 没有缓存，直接返回...
	if( m_MapPacket.size() <= 0 )
		return;
	// 抽取一个AVPacket进行解码操作，一个AVPacket一定能解码出一个Picture...
	int got_picture = 0, nResult = 0;
	AVFrame  * lpFrame = av_frame_alloc();
	GM_MapPacket::iterator itorItem = m_MapPacket.begin();
	AVPacket & thePacket = itorItem->second;
	nResult = avcodec_decode_video2(m_lpDecoder, lpFrame, &got_picture, &thePacket);
	// 解码失败或没有得到完整图像，直接扔掉数据...
	if( nResult < 0 || !got_picture ) {
		av_frame_free(&lpFrame);
		av_free_packet(&thePacket);
		m_MapPacket.erase(itorItem);
		return;
	}
	//TRACE("[Video] best: %I64d, pkt_dts: %I64d, pkt_pts: %I64d\n", lpFrame->best_effort_timestamp, lpFrame->pkt_dts, lpFrame->pkt_pts);
	//TRACE("[Video] DTS: %I64d, PTS: %I64d, Size: %lu\n", thePacket.dts, thePacket.pts, thePacket.size);
	// 计算解码后的数据帧的时间戳 => 将毫秒转换成纳秒...
	AVRational base_pack  = {1, 1000};
	AVRational base_frame = {1, 1000000000};
	int64_t frame_pts = av_rescale_q(lpFrame->best_effort_timestamp, base_pack, base_frame);
	// 累加系统计时零点...
	frame_pts += inSysZeroNS;
	// 将解码后的数据帧放入播放队列当中...
	m_MapFrame[frame_pts] = lpFrame;
	//DoProcSaveJpeg(lpFrame, m_lpDecoder->pix_fmt, frame_pts, "F:/MP4/Src");
	// 这里是引用，必须先free再erase...
	av_free_packet(&thePacket);
	m_MapPacket.erase(itorItem);
}

int64_t CVideoDecoder::doGetMinNextNS()
{
	// 如果有需要解码的数据，直接返回-1...
	if( m_MapPacket.size() > 0 )
		return -1;
	// 如果没有需要显示的解码后数据，休息20毫秒...
	if( m_MapFrame.size() <= 0 ) {
		return os_gettime_ns() + 20000000;
	}
	// 直接返回马上要播放的第一帧数据的PTS...
	return m_MapFrame.begin()->first;
}

int64_t CAudioDecoder::doGetMinNextNS()
{
	// 如果有需要解码的数据，直接返回-1...
	if( m_MapPacket.size() > 0 )
		return -1;
	// 如果没有需要显示的解码后数据，休息20毫秒...
	if( m_MapAudio.size() <= 0 ) {
		return os_gettime_ns() + 20000000;
	}
	// 直接返回马上要播放的第一帧数据的PTS...
	return m_MapAudio.begin()->first;
}

void CAudioDecoder::doDecodeFrame(int64_t inSysZeroNS)
{
	// 没有缓存，直接返回...
	if( m_MapPacket.size() <= 0 )
		return;
	// 抽取一个AVPacket进行解码操作，一个AVPacket一定能解码出一个Picture...
	int got_picture = 0, nResult = 0;
	AVFrame  * lpFrame = av_frame_alloc();
	GM_MapPacket::iterator itorItem = m_MapPacket.begin();
	AVPacket & thePacket = itorItem->second;
	nResult = avcodec_decode_audio4(m_lpDecoder, lpFrame, &got_picture, &thePacket);
	// 解码失败或没有得到完整图像，直接扔掉数据...
	if( nResult < 0 || !got_picture ) {
		av_frame_free(&lpFrame);
		av_free_packet(&thePacket);
		m_MapPacket.erase(itorItem);
		return;
	}
	// 计算解码后的数据帧的时间戳 => 将毫秒转换成纳秒...
	AVRational base_pack  = {1, 1000};
	AVRational base_frame = {1, 1000000000};
	int64_t frame_pts = av_rescale_q(lpFrame->best_effort_timestamp, base_pack, base_frame);
	// 累加系统计时零点...
	frame_pts += inSysZeroNS;
	// 对解码后的音频进行类型转换...
	nResult = swr_convert(m_au_convert_ctx, &m_out_buffer, m_out_buffer_size * 2, (const uint8_t **)lpFrame->data , lpFrame->nb_samples);
	// 转换后的数据存放到转换后的队列当中...
	string strData;
	strData.append((char*)m_out_buffer, m_out_buffer_size);
	m_MapAudio[frame_pts] = strData;
	// 这里是引用，必须先free再erase...
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
	// 关闭缓冲区...
	if( m_out_buffer != NULL ) {
		av_free(m_out_buffer);
		m_out_buffer = NULL;
	}
	// 关闭音频播放对象...
	if( m_au_convert_ctx != NULL ) {
		swr_free(&m_au_convert_ctx);
		m_au_convert_ctx = NULL;
	}
	// 关闭音频...
	SDL_CloseAudio();
}

BOOL CAudioDecoder::InitAudio(int nRateIndex, int nChannelNum)
{
	// 根据索引获取采样率...
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
	// 保存采样率索引和声道...
	m_audio_rate_index = nRateIndex;
	m_audio_channel_num = nChannelNum;
	// 初始化ffmpeg解码器...
	av_register_all();
	// 准备一些特定的参数...
	AVCodecID src_codec_id = AV_CODEC_ID_AAC;
	// 查找需要的解码器和相关容器、解析器...
	m_lpCodec = avcodec_find_decoder(src_codec_id);
	m_lpDecoder = avcodec_alloc_context3(m_lpCodec);
	// 打开获取到的解码器...
	if( avcodec_open2(m_lpDecoder, m_lpCodec, NULL) != 0 )
		return false;
	// 输入声道和输出声道是一样的...
	int in_audio_channel_num = m_audio_channel_num;
	int out_audio_channel_num = m_audio_channel_num;
	int64_t in_channel_layout = av_get_default_channel_layout(in_audio_channel_num);
	int64_t out_channel_layout = (out_audio_channel_num <= 1) ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
	// 输出音频采样格式...
	AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
	// out_nb_samples: AAC-1024 MP3-1152
	int out_nb_samples = 1024;
	// 设置输入输出采样率 => 不变...
	int in_sample_rate = m_audio_sample_rate;
	int out_sample_rate = m_audio_sample_rate;

	//SDL_AudioSpec => 不能使用系统推荐参数...
	SDL_AudioSpec audioSpec = {0};
	audioSpec.freq = out_sample_rate; 
	audioSpec.format = AUDIO_S16SYS; //m_lpDecoder->sample_fmt => AV_SAMPLE_FMT_FLTP
	audioSpec.channels = out_audio_channel_num; 
	audioSpec.samples = out_nb_samples;
	audioSpec.callback = CPlayThread::do_fill_audio; 
	audioSpec.userdata = m_lpPlayThread;
	audioSpec.silence = 0;

	// 打开SDL音频设备 => 只能打开一个设备...
	if( SDL_OpenAudio(&audioSpec, NULL) != 0 ) {
		SDL_Log("Failed to open audio: %s", SDL_GetError());
		return false;
	}

	// 获取音频解码后输出的缓冲区大小...
	m_out_buffer_size = av_samples_get_buffer_size(NULL, out_audio_channel_num, out_nb_samples, out_sample_fmt, 1);
	m_out_buffer = (uint8_t *)av_malloc(m_out_buffer_size * 2);
	
	// 分配并初始化转换器...
	m_au_convert_ctx = swr_alloc();
	m_au_convert_ctx = swr_alloc_set_opts(m_au_convert_ctx, out_channel_layout, out_sample_fmt, out_sample_rate,
										  in_channel_layout, m_lpDecoder->sample_fmt, in_sample_rate, 0, NULL);
	swr_init(m_au_convert_ctx);

	// 开始播放...
	SDL_PauseAudio(0);

	return true;
}*/
//
// 需要对音频帧数据添加头信息...
//void CAudioDecoder::doFillPacket(string & inData, int inPTS, bool bIsKeyFrame, int inOffset)
//{
//	// 先加入ADTS头，再加入数据帧内容...
//	int nTotalSize = ADTS_HEADER_SIZE + inData.size();
//	// 构造ADTS帧头...
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
//	// 构造一个临时AVPacket，并存入帧数据内容...
//	AVPacket  theNewPacket = {0};
//	av_new_packet(&theNewPacket, nTotalSize);
//	ASSERT(theNewPacket.size == nTotalSize);
//	// 先添加帧头数据，再添加帧内容信息...
//	memcpy(theNewPacket.data, pbuf, ADTS_HEADER_SIZE);
//	memcpy(theNewPacket.data + ADTS_HEADER_SIZE, inData.c_str(), inData.size());
//	// 计算当前帧的PTS，关键帧标志 => 音频流 => 0
//	theNewPacket.pts = inPTS;
//	theNewPacket.dts = inPTS - inOffset;
//	theNewPacket.flags = bIsKeyFrame;
//	theNewPacket.stream_index = 0;
//	// 将数据压入解码前队列当中...
//	this->doPushPacket(theNewPacket);
//}

/*void CAudioDecoder::doFillAudio(Uint8 * inStream, int inLen)
{
	// 没有缓存，直接返回...
	SDL_memset(inStream, 0, inLen);
	if( m_MapAudio.size() <= 0 )
		return;
	/////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：网络流畅的情况下，DeltaDelay是为了防止机器抖动造成的音频播放速度慢的问题
	// 这里必须周期性的对音频缓存清空处理，当延时达到一定程度时就需要清理...
	/////////////////////////////////////////////////////////////////////////////////////////////
	// 获取当前的系统时间 => 纳秒...
	int64_t inSysCurNS = os_gettime_ns();
	// 获取第一个，时间最小的数据块...
	GM_MapAudio::iterator itorItem = m_MapAudio.begin();
	string  & strPCM = itorItem->second;
	int64_t   nFramePTS = itorItem->first;
	int64_t   nDeltaDelay = 200000000; //200毫秒...
	int       nSize = m_MapAudio.size();
	// 超前数据包，超前100毫秒以下，继续播放，超前100毫秒以上，不灌数据，直接返回...
	if( (nFramePTS > inSysCurNS) && ((nFramePTS - inSysCurNS) >= nDeltaDelay/2) ) {
		TRACE("[Audio] Advance: %I64d ms, Continue, Size: %d\n", (nFramePTS - inSysCurNS)/1000000, nSize);
		return;
	}
	// 当前帧的显示时间有超过200毫秒延时 => 清空音频解码前和解码后缓冲区...
	if( (inSysCurNS - nFramePTS) > nDeltaDelay ) {
		TRACE("[Audio] Clear Audio Buffer: %I64d ms, Size: %d\n", (inSysCurNS - nFramePTS)/1000000, nSize);
		m_MapPacket.clear();
		m_MapAudio.clear();
		return;
	}
	//TRACE("[Audio] OS: %I64d ms, Delay: %I64d ms, Success, Size: %d\n", inSysCurNS/1000000, (inSysCurNS - nFramePTS)/1000000, nSize);
	// 填充数据给SDL播放缓冲...
	int nAudioSize = strPCM.size();
	Uint8 * lpAudioPos = (Uint8*)strPCM.c_str();
	inLen = (inLen > nAudioSize) ? nAudioSize : inLen;
	SDL_MixAudio(inStream, lpAudioPos, inLen, SDL_MIX_MAXVOLUME);
	// 释放已经使用完毕的资源空间...
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
	// 等待线程退出...
	this->StopAndWaitForThread();
	// 释放音视频解码对象...
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
// 解码、显示过程...
void CPlayThread::Entry()
{
	while( !this->IsStopRequested() ) {
		// 进行sleep等待...
		this->doSleepTo();
		// 解码一帧视频...
		this->doDecodeVideo();
		// 解码一帧音频...
		this->doDecodeAudio();
		// 显示一帧视频...
		this->doDisplaySDL();
		// 计算休息时间...
		this->doCalcNextNS();
	}
}

void CPlayThread::doSleepTo()
{
	// < 0 不能休息，直接返回...
	if( m_play_next_ns < 0 )
		return;
	// 最多休息200毫秒...
	uint64_t cur_time_ns = os_gettime_ns();
	const uint64_t timeout_ns = 200000000;
	// 如果比当前时间小(已过期)，直接返回...
	if( m_play_next_ns <= cur_time_ns )
		return;
	// 计算超前时间的差值，最多休息200毫秒...
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
	// 检测视频第一帧的时间戳...
	if( m_VideoDecoder != NULL ) {
		int64_t min_video_ns = m_VideoDecoder->doGetMinNextNS();
		if( min_video_ns < min_next_ns )
			min_next_ns = min_video_ns;
	}
	//// 音频是被动获取数据，不用检测...
	//if( m_AudioDecoder != NULL ) {
	//	int64_t min_audio_ns = m_AudioDecoder->doGetMinNextNS();
	//	if( min_audio_ns < min_next_ns  )
	//		min_next_ns = min_audio_ns;
	//}
	//m_play_next_ns = min_next_ns;
}

//
// 初始化视频...
BOOL CPlayThread::InitVideo(CRenderWnd * lpRenderWnd, string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS)
{
	OSMutexLocker theLock(&m_Mutex);
	// 创建新的视频对象...
	if( m_VideoDecoder != NULL ) {
		delete m_VideoDecoder;
		m_VideoDecoder = NULL;
	}
	m_VideoDecoder = new CVideoDecoder();
	if( !m_VideoDecoder->InitVideo(lpRenderWnd, inSPS, inPPS, nWidth, nHeight, nFPS) )
		return false;
	// 线程没有启动，直接启动...
	if( this->GetThreadHandle() == NULL ) {
		this->Start();
	}
	return true;
}
//
// 初始化音频...
BOOL CPlayThread::InitAudio(int nRateIndex, int nChannelNum)
{
	OSMutexLocker theLock(&m_Mutex);
	// 创建新的音频对象...
	if( m_AudioDecoder != NULL ) {
		delete m_AudioDecoder;
		m_AudioDecoder = NULL;
	}
	m_AudioDecoder = new CAudioDecoder(this);
	if( !m_AudioDecoder->InitAudio(nRateIndex, nChannelNum) )
		return false;
	// 线程没有启动，直接启动...
	if( this->GetThreadHandle() == NULL ) {
		this->Start();
	}
	return true;
}
//
// 推入音频或视频数据帧...
void CPlayThread::PushFrame(FMS_FRAME & inFrame)
{
	OSMutexLocker theLock(&m_Mutex);
	// 如果线程已经退出了，直接返回...
	if( this->IsStopRequested() )
		return;
	// 判断处理帧的对象是否存在，不存在，直接丢弃...
	if( inFrame.typeFlvTag == FLV_TAG_TYPE_AUDIO && m_AudioDecoder == NULL )
		return;
	if( inFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO && m_VideoDecoder == NULL )
		return;
	// 获取第一帧的PTS时间戳...
	if( m_start_pts < 0 ) {
		m_start_pts = inFrame.dwSendTime;
	}
	// 注意：有数据到达时，才进行零点计算...
	// 设置系统零点时间 => 播放启动时间戳...
	if( m_play_sys_ts < 0 ) {
		m_play_sys_ts = os_gettime_ns();
	}
	// 如果当前帧的时间戳比第一帧的时间戳还要小，直接扔掉...
	if( inFrame.dwSendTime < m_start_pts )
		return;
	// 根据音视频类型进行相关操作...
	int nCalcPTS = inFrame.dwSendTime - (uint32_t)m_start_pts;

	/////////////////////////////////////////////////////////////
	// 注意：延时模拟目前测试出来，后续配合网络实测后再模拟...
	/////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////
	// 随机丢掉数据帧 => 每隔10秒，丢1秒的音视频数据帧...
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
	// 等待线程退出...
	this->StopAndWaitForThread();
	// 释放预先已经分配的空间...
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
	// 销毁SDL窗口时会隐藏关联窗口...
	m_lpRenderWnd->ShowWindow(SW_SHOW);
	m_lpRenderWnd->SetRenderState(CRenderWnd::ST_RENDER);
}

BOOL CVideoThread::InitThread(CPushThread * inPushThread, CRenderWnd * lpRenderWnd)
{
	OSMutexLocker theLock(&m_Mutex);

	// 保存窗口和父对象...
	m_lpRenderWnd = lpRenderWnd;
	m_lpPushThread = inPushThread;
	m_lpRenderWnd->SetRenderState(CRenderWnd::ST_WAIT);

	m_sdlScreen = SDL_CreateWindowFrom((void*)m_lpRenderWnd->m_hWnd);
	m_sdlRenderer = SDL_CreateRenderer(m_sdlScreen, -1, 0);
	m_sdlTexture = SDL_CreateTexture(m_sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, m_nWidth, m_nHeight);

	// 初始化ffmpeg解码器...
	av_register_all();
	// 准备一些特定的参数...
	AVCodecID src_codec_id = AV_CODEC_ID_H264;
	// 查找需要的解码器和相关容器、解析器...
	m_lpSrcCodec = avcodec_find_decoder(src_codec_id);
	m_lpSrcCodecCtx = avcodec_alloc_context3(m_lpSrcCodec);
	m_lpSrcCodecParserCtx = av_parser_init(src_codec_id);
	// 这几个参数网上说能降低延时，好像并不太有用 => 对编码有用...
	//m_lpSrcCodecCtx->time_base.den = 1;
	//m_lpSrcCodecCtx->time_base.num = 75;
	//av_opt_set(m_lpSrcCodecCtx->priv_data, "preset", "superfast", 0);
	//av_opt_set(m_lpSrcCodecCtx->priv_data, "tune", "zerolatency", 0);
	// 打开获取到的解码器...
	int nResult = avcodec_open2(m_lpSrcCodecCtx, m_lpSrcCodec, NULL);
	// 开启界面绘制线程...
	this->Start();
	return true;
}

void CVideoThread::PushFrame(string & inFrame, bool bIsKeyFrame)
{
	OSMutexLocker theLock(&m_Mutex);
	// 如果是关键帧，必须先写入sps，再写如pps...
	// 每个关键帧都放入sps和pps，播放会加快...
	if( bIsKeyFrame ) {
		DWORD dwTag = 0x01000000;
		m_strFrame.append((char*)&dwTag, sizeof(DWORD));
		m_strFrame.append(m_strSPS);
		m_strFrame.append((char*)&dwTag, sizeof(DWORD));
		m_strFrame.append(m_strPPS);
	}
	// 修改视频帧的起始码 => 0x00000001
	char * lpszSrc = (char*)inFrame.c_str();
	memset((void*)lpszSrc, 0, sizeof(DWORD));
	lpszSrc[3] = 0x01;
	// 最后追加新数据...
	m_strFrame.append(inFrame);
}

int CVideoThread::DecodeFrame()
{
	OSMutexLocker theLock(&m_Mutex);
	// 如果没有数据，直接返回...
	if( m_strFrame.size() <= 0 )
		return 0;
	// 先释放上次分配的缓冲区...
	if( m_lpSrcFrame != NULL ) {
		av_frame_free(&m_lpSrcFrame);
		m_lpSrcFrame = NULL;
	}
	// 初始化ffmpeg数据帧...
	BOOL      bReturn = false;
	AVPacket  theSrcPacket = {0};
	m_lpSrcFrame = av_frame_alloc();
	av_init_packet(&theSrcPacket);
	// 解析传递过来的h264数据帧...
	uint8_t * lpCurPtr = (uint8_t*)m_strFrame.c_str();
	int nCurSize = m_strFrame.size();
	int got_picture = 0;
	int nParseSize = 0;
	int	nResult = 0;
	while( nCurSize > 0 ) {
		// 这里需要多次解析，直到解析完所有的缓存为止...
		nResult = av_parser_parse2( m_lpSrcCodecParserCtx, m_lpSrcCodecCtx,
						  &theSrcPacket.data, &theSrcPacket.size,
						  lpCurPtr, nCurSize, AV_NOPTS_VALUE,
						  AV_NOPTS_VALUE, AV_NOPTS_VALUE);
		// 没有解析出packet，移动缓冲指针...
		if( nResult > 0 ) {
			lpCurPtr += nResult;
			nCurSize -= nResult;
		}
		//TRACE("PackSize: %lu, Result: %lu\n", theSrcPacket.size, nResult);
		// 没有解析出packet，继续...
		if( theSrcPacket.size == 0 )
			continue;
		// 这里必须用解析出的长度...
		nParseSize += theSrcPacket.size;
		//TRACE("Packet: %lu, Parsed: %lu\n", theSrcPacket.size, nParseSize);
		// 对解析正确的packet进行解码操作...
		nResult = avcodec_decode_video2(m_lpSrcCodecCtx, m_lpSrcFrame, &got_picture, &theSrcPacket);
		// 解码失败或没有得到完整图像，继续解析...
		if( nResult < 0 || !got_picture )
			continue;
		// 解码成功，并且获取了一副图像，进行存盘操作...
		this->DisplaySDL();
		// 设置成功标志，中断循环...
		bReturn = true;
		// 删除已经解码的缓冲区...
		m_strFrame.erase(0, nParseSize);
		TRACE("Video => Parsed: %lu, RemainFrame: %lu, Type: %lu\n", nParseSize, m_strFrame.size(), m_lpSrcFrame->pict_type);
		break;
	}
	// 对用到的数据进行清理工作...
	av_free_packet(&theSrcPacket);
	return (bReturn ? 1 : 0);
}
//
// YUV数据通过SDL2.0显示出来 => OpenGL
void CVideoThread::DisplaySDL()
{
	if( m_lpSrcCodecCtx == NULL || m_lpSrcFrame == NULL )
		return;
	enum AVPixelFormat nDestFormat = AV_PIX_FMT_YUV420P;
	enum AVPixelFormat nSrcFormat = m_lpSrcCodecCtx->pix_fmt;
	int nSrcWidth = m_lpSrcCodecCtx->width;
	int nSrcHeight = m_lpSrcCodecCtx->height;
	// 不管什么格式，都需要进行像素格式的转换...
	AVFrame * pDestFrame = av_frame_alloc();
	int nDestBufSize = avpicture_get_size(nDestFormat, nSrcWidth, nSrcHeight);
	uint8_t * pDestOutBuf = (uint8_t *)av_malloc(nDestBufSize);
	avpicture_fill((AVPicture *)pDestFrame, pDestOutBuf, nDestFormat, nSrcWidth, nSrcHeight);
	// 调用ffmpeg的格式转换接口函数...
	struct SwsContext * img_convert_ctx = sws_getContext(nSrcWidth, nSrcHeight, nSrcFormat, nSrcWidth, nSrcHeight, nDestFormat, SWS_BICUBIC, NULL, NULL, NULL);
	int nReturn = sws_scale(img_convert_ctx, (const uint8_t* const*)m_lpSrcFrame->data, m_lpSrcFrame->linesize, 0, nSrcHeight, pDestFrame->data, pDestFrame->linesize);
	sws_freeContext(img_convert_ctx);
	// 设置转换后的数据帧内容...
	pDestFrame->width = nSrcWidth;
	pDestFrame->height = nSrcHeight;
	pDestFrame->format = nDestFormat;
	//////////////////////////////////////////////
	// 使用SDL 进行画面绘制工作...
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
	// 释放临时分配的数据空间...
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
	// 根据索引获取采样率...
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
	// 等待线程退出...
	this->StopAndWaitForThread();
	// 释放预先已经分配的空间...
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
	// 关闭音频...
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

	// 初始化ffmpeg解码器...
	av_register_all();
	// 准备一些特定的参数...
	AVCodecID src_codec_id = AV_CODEC_ID_AAC;
	// 查找需要的解码器和相关容器、解析器...
	m_lpSrcCodec = avcodec_find_decoder(src_codec_id);
	m_lpSrcCodecCtx = avcodec_alloc_context3(m_lpSrcCodec);
	m_lpSrcCodecParserCtx = av_parser_init(src_codec_id);
	// 打开获取到的解码器...
	int nResult = avcodec_open2(m_lpSrcCodecCtx, m_lpSrcCodec, NULL);

	// 输入声道和输出声道是一样的...
	int in_audio_channel_num = m_audio_channel_num;
	int out_audio_channel_num = m_audio_channel_num;
	int64_t in_channel_layout = av_get_default_channel_layout(in_audio_channel_num);
	int64_t out_channel_layout = (out_audio_channel_num <= 1) ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
	// 输出音频采样格式...
	AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
	// out_nb_samples: AAC-1024 MP3-1152
	int out_nb_samples = 1024;
	// 设置输入输出采样率 => 不变...
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

	// 打开SDL音频设备 => 只能打开一个设备...
	nResult = SDL_OpenAudio(&audioSpec, NULL);
	
	// 获取音频解码后输出的缓冲区大小...
	m_out_buffer_size = av_samples_get_buffer_size(NULL, out_audio_channel_num, out_nb_samples, out_sample_fmt, 1);
	m_out_buffer = (uint8_t *)av_malloc(m_out_buffer_size * 2);
	
	// 分配并初始化转换器...
	m_au_convert_ctx = swr_alloc();
	m_au_convert_ctx = swr_alloc_set_opts(m_au_convert_ctx, out_channel_layout, out_sample_fmt, out_sample_rate,
										  in_channel_layout, m_lpSrcCodecCtx->sample_fmt, in_sample_rate, 0, NULL);
	swr_init(m_au_convert_ctx);

	// 开始播放...
	SDL_PauseAudio(0);

	// 开启界面绘制线程...
	this->Start();

	return true;
}

void CAudioThread::PushFrame(string & inFrame)
{
	OSMutexLocker theLock(&m_Mutex);
	// 构造ADTS帧头...
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

	// 先加入ADTS头，再加入数据帧内容...
	m_strFrame.append(pbuf, ADTS_HEADER_SIZE);
	m_strFrame.append(inFrame);
}

int CAudioThread::DecodeFrame()
{
	OSMutexLocker theLock(&m_Mutex);
	// 如果没有数据，直接返回...
	if( m_strFrame.size() <= 0 )
		return 0;
	// 先释放上次分配的缓冲区...
	if( m_lpSrcFrame != NULL ) {
		av_frame_free(&m_lpSrcFrame);
		m_lpSrcFrame = NULL;
	}
	// 初始化ffmpeg数据帧...
	BOOL      bReturn = false;
	AVPacket  theSrcPacket = {0};
	m_lpSrcFrame = av_frame_alloc();
	av_init_packet(&theSrcPacket);

	// 解析传递过来的aac数据帧...
	uint8_t * lpCurPtr = (uint8_t*)m_strFrame.c_str();
	int nCurSize = m_strFrame.size();
	int got_picture = 0;
	int nParseSize = 0;
	int	nResult = 0;
	while( nCurSize > 0 ) {
		// 这里需要多次解析，直到解析完所有的缓存为止...
		nResult = av_parser_parse2( m_lpSrcCodecParserCtx, m_lpSrcCodecCtx,
						  &theSrcPacket.data, &theSrcPacket.size,
						  lpCurPtr, nCurSize, AV_NOPTS_VALUE,
						  AV_NOPTS_VALUE, AV_NOPTS_VALUE);
		// 没有解析出packet，移动缓冲指针...
		if( nResult > 0 ) {
			lpCurPtr += nResult;
			nCurSize -= nResult;
		}
		//TRACE("PackSize: %lu, Result: %lu\n", theSrcPacket.size, nResult);
		// 没有解析出packet，继续...
		if( theSrcPacket.size == 0 )
			continue;
		// 这里必须用解析出的长度...
		nParseSize += theSrcPacket.size;
		//TRACE("Packet: %lu, Parsed: %lu\n", theSrcPacket.size, nParseSize);
		// 对解析正确的packet进行解码操作...
		nResult = avcodec_decode_audio4(m_lpSrcCodecCtx, m_lpSrcFrame, &got_picture, &theSrcPacket);
		// 解码失败或没有得到完整图像，继续解析...
		if( nResult < 0 || !got_picture )
			continue;
		nResult = swr_convert(m_au_convert_ctx, &m_out_buffer, m_out_buffer_size * 2, (const uint8_t **)m_lpSrcFrame->data , m_lpSrcFrame->nb_samples);
		int nOutSize = nResult * m_audio_channel_num * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
		//av_samples_get_buffer_size(NULL, out_audio_channel_num, out_nb_samples, out_sample_fmt, 1);
		m_strPCM.append((char*)m_out_buffer, m_out_buffer_size);
		//TRACE("pts:%lld\t packet size:%d\n", theSrcPacket.pts, theSrcPacket.size);
		// 设置成功标志，中断循环...
		bReturn = true;
		// 删除已经解码的缓冲区...
		m_strFrame.erase(0, nParseSize);
		//TRACE("Audio => Parsed: %lu, RemainFrame: %lu, RemainPCM: %lu\n", nParseSize, m_strFrame.size(), m_strPCM.size()-m_out_buffer_size);
		break;
	}
	// 对用到的数据进行清理工作...
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
