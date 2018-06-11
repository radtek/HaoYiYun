
#include "stdafx.h"
#include "VideoWnd.h"
#include "PushThread.h"
#include "XmlConfig.h"
#include "srs_librtmp.h"
#include "BitWritter.h"
#include "UtilTool.h"
#include "LibRtmp.h"
#include "Camera.h"
#include "LibMP4.h"
#include "ReadSPS.h"
#include "md5.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define ADTS_HEADER_SIZE	7

static LARGE_INTEGER		g_clock_freq;
static bool					g_have_clockfreq = false;

static uint64_t get_clockfreq(void)
{
	if (!g_have_clockfreq) {
		QueryPerformanceFrequency(&g_clock_freq);
		g_have_clockfreq = true;
	}
	return g_clock_freq.QuadPart;
}

uint64_t os_gettime_ns(void)
{
	LARGE_INTEGER current_time;
	double time_val;

	QueryPerformanceCounter(&current_time);
	time_val = (double)current_time.QuadPart;
	time_val *= 1000000000.0;
	time_val /= (double)get_clockfreq();

	return (uint64_t)time_val;
}

bool os_sleepto_ns(uint64_t time_target)
{
	uint64_t t = os_gettime_ns();
	uint32_t milliseconds;

	if (t >= time_target)
		return false;

	milliseconds = (uint32_t)((time_target - t)/1000000);
	if (milliseconds > 1) {
		Sleep(milliseconds-1);
	}
	for (;;) {
		t = os_gettime_ns();
		if (t >= time_target)
			return true;
#if 0
		Sleep(1);
#else
		Sleep(0);
#endif
	}
}

CDecoder::CDecoder()
  : m_lpCodec(NULL)
  , m_lpDecoder(NULL)
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

CVideoDecoder::CVideoDecoder()
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

static bool DoProcSaveJpeg(AVFrame * pSrcFrame, AVPixelFormat inSrcFormat, int64_t inPTS, LPCTSTR lpPath)
{
	/*int seconds_value = 0;
	uint64_t cur_time_ns = os_gettime_ns();
	// 如果不是第一次，计算流逝的秒数 => 每隔2分钟截图...
	if( g_time_start > 0 ) {
		seconds_value = (int)((cur_time_ns - g_time_start) / 1000000 / 1000);
		if (seconds_value < MAX_SNAP_SECONDS) return false;
	}
	// 保存当前的纳秒时间值...
	g_time_start = cur_time_ns;
	// 获取存盘需要的配置信息 => 路径和文件名...
	char szSaveFile[100] = { 0 };
	char szSavePath[300] = { 0 };
	struct obs_service * lpService = obs->data.first_service;
	const char * lpKey = lpService->info.get_key(lpService->context.data);
	if (lpKey == NULL || strnicmp(lpKey, "live", strlen("live")) != 0) {
		blog(LOG_ERROR, "DoProcSaveJpeg: service key error!");
		return false;
	}
	sprintf(szSaveFile, "obs-studio/live_%s.jpg", lpKey + 4);
	if (os_get_config_path(szSavePath, sizeof(szSavePath), szSaveFile) <= 0) {
		blog(LOG_ERROR, "DoProcSaveJpeg: save path error!");
		return false;
	}*/
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
	// 当前帧的显示时间已经过了 => 直接丢弃 => 容忍500毫秒的延时...
	/*int64_t   nDeltaDelay = 500000000; //500毫秒...
	if( inSysCurNS - nFramePTS > nDeltaDelay ) {
		TRACE("[Video] RG: %I64d ms, Delay: %I64d ms, Discard, Size: %d\n", nFramePTS/1000000, (inSysCurNS - nFramePTS)/1000000, nSize);
		// 只丢掉当前帧 => 这里是引用，必须先free再erase...
		av_frame_free(&lpSrcFrame);
		m_MapFrame.erase(itorItem);
		return;
	}*/
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
}
//
// 需要对音频帧数据添加头信息...
void CAudioDecoder::doFillPacket(string & inData, int inPTS, bool bIsKeyFrame, int inOffset)
{
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

void CAudioDecoder::doFillAudio(Uint8 * inStream, int inLen)
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
	// 音频是被动获取数据，不用检测...
	/*if( m_AudioDecoder != NULL ) {
		int64_t min_audio_ns = m_AudioDecoder->doGetMinNextNS();
		if( min_audio_ns < min_next_ns  )
			min_next_ns = min_audio_ns;
	}*/
	m_play_next_ns = min_next_ns;
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
}

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

CMP4Thread::CMP4Thread()
{
	m_nVideoHeight = 0;
	m_nVideoWidth = 0;
	m_nVideoFPS = 25;

	m_bFileLoop = false;
	m_strMP4File.clear();
	m_lpPushThread = NULL;
	m_dwMP4Duration = 0;
	m_nLoopCount = 0;

	m_hMP4Handle = MP4_INVALID_FILE_HANDLE;
	m_strAACHeader.clear();
	m_strAVCHeader.clear();
	m_strPPS.clear();
	m_strSPS.clear();
	m_strAES.clear();

	m_audio_type = 0;
	m_audio_rate_index = 0;
	m_audio_channel_num = 0;

	m_iVSampleInx = 1;
	m_iASampleInx = 1;
	m_bFinished = false;
	m_bVideoComplete = true;
	m_bAudioComplete = true;
	m_tidAudio = MP4_INVALID_TRACK_ID;
	m_tidVideo = MP4_INVALID_TRACK_ID;

	// 记录开始时间点...
	m_dwStartMS = ::GetTickCount();
}

CMP4Thread::~CMP4Thread()
{
	// 停止线程...
	this->StopAndWaitForThread();

	// 释放MP4句柄...
	if( m_hMP4Handle != MP4_INVALID_FILE_HANDLE ) {
		MP4Close(m_hMP4Handle);
		m_hMP4Handle = MP4_INVALID_FILE_HANDLE;
	}

	MsgLogINFO("[~CMP4Thread Thread] - Exit");
}

void CMP4Thread::Entry()
{
	// 解析并准备MP4文件...
	if( !this->doPrepareMP4() ) {
		// 解析失败 => 这里只需要设置结束标志，超时机制会自动删除 => 这里是拉流模式，不能调用 doErrNotify()...
		// 在超时机制中会首先检测m_bFinished标志，速度快...
		m_bFinished = true;
		return;
	}
	// 设置流转发播放状态标志...
	m_lpPushThread->SetStreamPlaying(true);

	// 在发起直播时才需要启动上传线程，准备完毕，启动主线程...
	//m_lpPushThread->Start();

	// 循环抽取数据帧...
	int nAccTime = 1000;
	uint32_t dwCompTime = 0;
	uint32_t dwElapsTime = 0;
	uint32_t dwOutVSendTime = 0;
	uint32_t dwOutASendTime = 0;
	while( !this->IsStopRequested() ) {

		// 这里需要多发送几秒钟的数据，避免出现缓冲(只需要加速一次)...
		dwElapsTime = ::GetTickCount() - m_dwStartMS + nAccTime;

		// 抽取一帧视频...
		if( (dwElapsTime >= dwOutVSendTime) && (!m_bVideoComplete) && (m_tidVideo != MP4_INVALID_TRACK_ID) ) {
			if( !ReadOneFrameFromMP4(m_tidVideo, m_iVSampleInx++, true, dwOutVSendTime) ) {
				m_bVideoComplete = true;
				continue;
			}
			//TRACE("[Video] ElapsTime = %lu, OutSendTime = %lu\n", dwElapsTime, dwOutVSendTime);
		}

		// 抽取一帧音频...
		if( (dwElapsTime >= dwOutASendTime) && (!m_bAudioComplete) && (m_tidAudio != MP4_INVALID_TRACK_ID) ) {
			if( !ReadOneFrameFromMP4(m_tidAudio, m_iASampleInx++, false, dwOutASendTime) ) {
				m_bAudioComplete = true;
				continue;
			}
			//TRACE("[Audio] ElapsTime = %lu, OutSendTime = %lu\n", dwElapsTime, dwOutASendTime);
		}

		// 如果音频和视频全部结束，则重新计数...
		if( m_bVideoComplete && m_bAudioComplete ) {
			// 如果不循环，直接退出线程...
			if( !m_bFileLoop ) {
				MsgLogINFO("== [File-Complete] ==");
				m_bFinished = true;
				return;
			}
			// 打印循环次数...
			ASSERT( m_bFileLoop );
			CUtilTool::MsgLog(kTxtLogger, "== [File-Loop] Count = %d ==\r\n", m_nLoopCount);
			// 如果要循环，重新计数，复位大量的执行参数...
			m_bVideoComplete = ((m_tidVideo != MP4_INVALID_TRACK_ID) ? false : true);	// 有视频才复位标志
			m_bAudioComplete = ((m_tidAudio != MP4_INVALID_TRACK_ID) ? false : true);	// 有音频才复位标志
			dwOutVSendTime = dwOutASendTime = 0;	// 帧计时器复位到0点
			m_iASampleInx = m_iVSampleInx = 1;		// 音视频的索引复位
			m_dwStartMS = ::GetTickCount();			// 时间起点复位
			m_nLoopCount += 1;						// 循环次数累加
			nAccTime = 0;							// 将加速器复位
			continue;
		}
		// 如果视频有效，使用获取的视频时间戳比较...
		if( m_tidVideo != MP4_INVALID_TRACK_ID ) {
			dwCompTime = dwOutVSendTime;
		}
		// 如果音频有效，使用获取的音频时间戳比较...
		if( m_tidAudio != MP4_INVALID_TRACK_ID ) {
			dwCompTime = dwOutASendTime;
		}
		// 如果音视频都有，则取比较小的值比较...
		if((m_tidVideo != MP4_INVALID_TRACK_ID) && (m_tidAudio != MP4_INVALID_TRACK_ID)) {
			dwCompTime = min(dwOutASendTime, dwOutVSendTime);
		}
		// 如果当前时间比较小，等待一下再读取...
		if( dwElapsTime < dwCompTime ) {
			//TRACE("ElapsTime = %lu, AudioSendTime = %lu, VideoSendTime = %lu\n", dwElapsTime, dwOutASendTime, dwOutVSendTime);
			::Sleep(5);
		}
	}
}

bool CMP4Thread::ReadOneFrameFromMP4(MP4TrackId tid, uint32_t sid, bool bIsVideo, uint32_t & outSendTime)
{
	if( m_hMP4Handle == MP4_INVALID_FILE_HANDLE )
		return false;
	ASSERT( m_hMP4Handle != MP4_INVALID_FILE_HANDLE );

	uint8_t		*	pSampleData = NULL;		// 帧数据指针
	uint32_t		nSampleSize = 0;		// 帧数据长度
	MP4Timestamp	nStartTime = 0;			// 开始时间
	MP4Duration		nDuration = 0;			// 持续时间
	MP4Duration     nOffset = 0;			// 偏移时间
	bool			bIsKeyFrame = false;	// 是否关键帧
	uint32_t		timescale = 0;
	uint64_t		msectime = 0;

	timescale = MP4GetTrackTimeScale( m_hMP4Handle, tid );
	msectime = MP4GetSampleTime( m_hMP4Handle, tid, sid );

	if( false == MP4ReadSample(m_hMP4Handle, tid, sid, &pSampleData, &nSampleSize, &nStartTime, &nDuration, &nOffset, &bIsKeyFrame) )
		return false;

	// 计算发送时间 => PTS => 刻度时间转换成毫秒...
	msectime *= UINT64_C( 1000 );
	msectime /= timescale;
	// 计算开始时间 => DTS => 刻度时间转换成毫秒...
	nStartTime *= UINT64_C( 1000 );
	nStartTime /= timescale;
	// 计算偏差时间 => CTTS => 刻度时间转换成毫秒...
	nOffset *= UINT64_C( 1000 );
	nOffset /= timescale;

	// 注意：msectime | nOffset | m_dwMP4Duration 都要统一成毫秒时间...
	FMS_FRAME	theFrame;
	theFrame.typeFlvTag = (bIsVideo ? FLV_TAG_TYPE_VIDEO : FLV_TAG_TYPE_AUDIO);	// 设置音视频标志
	theFrame.dwSendTime = (uint32_t)msectime + m_dwMP4Duration * m_nLoopCount;	// 这里非常重要，牵涉到循环播放
	theFrame.dwRenderOffset = nOffset;  // 2017.07.06 - by jackey => 保存时间偏移值...
	theFrame.is_keyframe = bIsKeyFrame;
	theFrame.strData.assign((char*)pSampleData, nSampleSize);

	// 这里需要释放读取的缓冲区...
	MP4Free(pSampleData);
	pSampleData = NULL;

	// 将数据帧加入待发送队列当中...
	ASSERT( m_lpPushThread != NULL );
	m_lpPushThread->PushFrame(theFrame);

	// 返回发送时间(毫秒) => 已将刻度时间转换成了毫秒...
	outSendTime = (uint32_t)msectime;
	
	//TRACE("[%s] duration = %I64d, offset = %I64d, KeyFrame = %d, SendTime = %lu, StartTime = %I64d, Size = %lu\n", bIsVideo ? "Video" : "Audio", nDuration, nOffset, bIsKeyFrame, outSendTime, nStartTime, nSampleSize);

	return true;
}

BOOL CMP4Thread::InitMP4(CPushThread * inPushThread, string & strMP4File)
{
	// 目前的模式，文件一定是循环模式...
	m_lpPushThread = inPushThread;
	m_strMP4File = strMP4File;
	m_bFileLoop = true;

	this->Start();

	return true;
}

void myMP4LogCallback( MP4LogLevel loglevel, const char* fmt, va_list ap)
{
	// 组合传递过来的格式...
	CString	strDebug;
	strDebug.FormatV(fmt, ap);
	if( (strDebug.ReverseFind('\r') < 0) && (strDebug.ReverseFind('\n') < 0) ) {
		strDebug.Append("\r\n");
	}
	// 进行格式转换，并打印出来...
	string strANSI = CUtilTool::UTF8_ANSI(strDebug);
	TRACE(strANSI.c_str());
}

bool CMP4Thread::doPrepareMP4()
{
	// 设置MP4调试级别 => 最高最详细级别...
	//MP4LogLevel theLevel = MP4LogGetLevel();
	//MP4LogSetLevel(MP4_LOG_VERBOSE4);
	//MP4SetLogCallback(myMP4LogCallback);
	// 文件打开失败，直接返回...
	string strUTF8 = CUtilTool::ANSI_UTF8(m_strMP4File.c_str());
	m_hMP4Handle = MP4Read( strUTF8.c_str() );
	if( m_hMP4Handle == MP4_INVALID_FILE_HANDLE ) {
		MP4Close(m_hMP4Handle);
		MsgLogGM(GM_File_Not_Open);
		m_hMP4Handle = MP4_INVALID_FILE_HANDLE;
		return false;
	}
	// 文件打开正确, 抽取需要的数据...
	if( !this->doMP4ParseAV(m_hMP4Handle) ) {
		MP4Close(m_hMP4Handle);
		MsgLogGM(GM_File_Not_Open);
		m_hMP4Handle = MP4_INVALID_FILE_HANDLE;
		return false;
	}
	return true;
}

bool CMP4Thread::doMP4ParseAV(MP4FileHandle inFile)
{
	if( inFile == MP4_INVALID_FILE_HANDLE )
		return false;
	ASSERT( inFile != MP4_INVALID_FILE_HANDLE );
	
	// 首先获取文件的每秒刻度数和总刻度数(不是毫秒数)...
	uint32_t dwFileScale = MP4GetTimeScale(inFile);
	MP4Duration theDuration = MP4GetDuration(inFile);
	// 总毫秒数 = 总刻度数*1000/每秒刻度数 => 先乘法可以降低误差...
	m_dwMP4Duration = theDuration*1000/dwFileScale;

	// 获取需要的相关信息...
    uint32_t trackCount = MP4GetNumberOfTracks( inFile );
    for( uint32_t i = 0; i < trackCount; ++i ) {
		MP4TrackId  id = MP4FindTrackId( inFile, i );
		const char* type = MP4GetTrackType( inFile, id );
		if( MP4_IS_VIDEO_TRACK_TYPE( type ) ) {
			// 视频已经有效，检测下一个...
			if( m_tidVideo > 0 )
				continue;
			// 获取视频信息...
			m_tidVideo = id;
			m_bVideoComplete = false;
			char * lpVideoInfo = MP4Info(inFile, id);
			free(lpVideoInfo);
			// 获取视频的PPS/SPS信息...
			uint8_t  ** spsHeader = NULL;
			uint8_t  ** ppsHeader = NULL;
			uint32_t  * spsSize = NULL;
			uint32_t  * ppsSize = NULL;
			uint32_t    ix = 0;
			bool bResult = MP4GetTrackH264SeqPictHeaders(inFile, id, &spsHeader, &spsSize, &ppsHeader, &ppsSize);
			for(ix = 0; spsSize[ix] != 0; ++ix) {
				// SPS指针和长度...
				uint8_t * lpSPS = spsHeader[ix];
				uint32_t  nSize = spsSize[ix];
				// 只存储第一个SPS信息...
				if( m_strSPS.size() <= 0 && nSize > 0 ) {
					m_strSPS.assign((char*)lpSPS, nSize);
				}
				free(spsHeader[ix]);
			}
			free(spsHeader);
			free(spsSize);
			for(ix = 0; ppsSize[ix] != 0; ++ix) {
				// PPS指针和长度...
				uint8_t * lpPPS = ppsHeader[ix];
				uint32_t  nSize = ppsSize[ix];
				// 只存储第一个PPS信息...
				if( m_strPPS.size() <= 0 && nSize > 0 ) {
					m_strPPS.assign((char*)lpPPS, nSize);
				}
				free(ppsHeader[ix]);
			}
			free(ppsHeader);
			free(ppsSize);

			// 保存视频数据头...
			this->WriteAVCSequenceHeader();
		}
		else if( MP4_IS_AUDIO_TRACK_TYPE( type ) ) {
			// 音频已经有效，检测下一个...
			if( m_tidAudio > 0 )
				continue;
			// 获取音频信息...
			m_tidAudio = id;
			m_bAudioComplete = false;
			char * lpAudioInfo = MP4Info(inFile, id);
			free(lpAudioInfo);

			// 获取音频的类型/名称/采样率/声道信息...
			m_audio_type = MP4GetTrackAudioMpeg4Type(inFile, id);
			m_audio_channel_num = MP4GetTrackAudioChannels(inFile, id);
			const char * audio_name = MP4GetTrackMediaDataName(inFile, id);
			uint32_t timeScale = MP4GetTrackTimeScale(inFile, id);
			if (timeScale == 48000)
				m_audio_rate_index = 0x03;
			else if (timeScale == 44100)
				m_audio_rate_index = 0x04;
			else if (timeScale == 32000)
				m_audio_rate_index = 0x05;
			else if (timeScale == 24000)
				m_audio_rate_index = 0x06;
			else if (timeScale == 22050)
				m_audio_rate_index = 0x07;
			else if (timeScale == 16000)
				m_audio_rate_index = 0x08;
			else if (timeScale == 12000)
				m_audio_rate_index = 0x09;
			else if (timeScale == 11025)
				m_audio_rate_index = 0x0a;
			else if (timeScale == 8000)
				m_audio_rate_index = 0x0b;
			// 获取音频扩展信息...
            uint8_t  * pAES = NULL;
            uint32_t   nSize = 0;
            bool haveEs = MP4GetTrackESConfiguration(inFile, id, &pAES, &nSize);
			// 存储音频扩展信息...
			if( m_strAES.size() <= 0 && nSize > 0 ) {
				m_strAES.assign((char*)pAES, nSize);
			}
			// 释放分配的缓存...
			if( pAES != NULL ) {
				free(pAES);
				pAES = NULL;
			}
			// 保存音频数据头...
			this->WriteAACSequenceHeader();
		}
	}
	// 如果音频和视频都没有，返回失败...
	if((m_tidVideo == MP4_INVALID_TRACK_ID) && (m_tidAudio == MP4_INVALID_TRACK_ID)) {
		MsgLogGM(GM_File_Read_Err);
		return false;
	}
	// 一切正常，返回成功...
	return true;
}

void CMP4Thread::WriteAACSequenceHeader()
{
	char   aac_seq_buf[4096] = {0};
    char * pbuf = aac_seq_buf;

    unsigned char flag = 0;
    flag = (10 << 4) |  // soundformat "10 == AAC"
        (3 << 2) |      // soundrate   "3  == 44-kHZ"
        (1 << 1) |      // soundsize   "1  == 16bit"
        1;              // soundtype   "1  == Stereo"

    pbuf = UI08ToBytes(pbuf, flag);
    pbuf = UI08ToBytes(pbuf, 0);    // aac packet type (0, header)

    // AudioSpecificConfig

	if( m_strAES.size() > 0 ) {
		memcpy(pbuf, m_strAES.c_str(), m_strAES.size());
		pbuf += m_strAES.size();
	} else {
		PutBitContext pb;
		init_put_bits(&pb, pbuf, 1024);
		put_bits(&pb, 5, m_audio_type);			//object type - AAC-LC
		put_bits(&pb, 4, m_audio_rate_index);	// sample rate index
		put_bits(&pb, 4, m_audio_channel_num);  // channel configuration

		//GASpecificConfig
		put_bits(&pb, 1, 0);    // frame length - 1024 samples
		put_bits(&pb, 1, 0);    // does not depend on core coder
		put_bits(&pb, 1, 0);    // is not extension

		flush_put_bits(&pb);
		pbuf += 2;
	}

	// 保存AAC数据头信息...
	int aac_len = (int)(pbuf - aac_seq_buf);
	m_strAACHeader.assign(aac_seq_buf, aac_len);
	
	// 开启音频播放线程...
	//m_lpPushThread->StartPlayByAudio(m_audio_rate_index, m_audio_channel_num);
}

void CMP4Thread::WriteAVCSequenceHeader()
{
	// 获取 width 和 height...
	int nPicFPS = 0;
	int	nPicWidth = 0;
	int	nPicHeight = 0;
	if( m_strSPS.size() >  0 ) {
		/*CSPSReader _spsreader;
		bs_t    s = {0};
		s.p		  = (uint8_t *)m_strSPS.c_str();
		s.p_start = (uint8_t *)m_strSPS.c_str();
		s.p_end	  = (uint8_t *)m_strSPS.c_str() + m_strSPS.size();
		s.i_left  = 8; // 这个是固定的,对齐长度...
		_spsreader.Do_Read_SPS(&s, &nPicWidth, &nPicHeight);*/

		h264_decode_sps((BYTE*)m_strSPS.c_str(), m_strSPS.size(), nPicWidth, nPicHeight, nPicFPS);

		m_nVideoHeight = nPicHeight;
		m_nVideoWidth = nPicWidth;
		m_nVideoFPS = nPicFPS;
	}

	// Write AVC Sequence Header use SPS and PPS...
	char * sps_ = (char*)m_strSPS.c_str();
	char * pps_ = (char*)m_strPPS.c_str();
	int    sps_size_ = m_strSPS.size();
	int	   pps_size_ = m_strPPS.size();

    char   avc_seq_buf[4096] = {0};
    char * pbuf = avc_seq_buf;

    unsigned char flag = 0;
    flag = (1 << 4) |				// frametype "1  == keyframe"
				 7;					// codecid   "7  == AVC"
    pbuf = UI08ToBytes(pbuf, flag);	// flag...
    pbuf = UI08ToBytes(pbuf, 0);    // avc packet type (0, header)
    pbuf = UI24ToBytes(pbuf, 0);    // composition time

    // AVC Decoder Configuration Record...

    pbuf = UI08ToBytes(pbuf, 1);            // configurationVersion
    pbuf = UI08ToBytes(pbuf, sps_[1]);      // AVCProfileIndication
    pbuf = UI08ToBytes(pbuf, sps_[2]);      // profile_compatibility
    pbuf = UI08ToBytes(pbuf, sps_[3]);      // AVCLevelIndication
    pbuf = UI08ToBytes(pbuf, 0xff);         // 6 bits reserved (111111) + 2 bits nal size length - 1
    pbuf = UI08ToBytes(pbuf, 0xe1);         // 3 bits reserved (111) + 5 bits number of sps (00001)
    pbuf = UI16ToBytes(pbuf, sps_size_);    // sps
    memcpy(pbuf, sps_, sps_size_);
    pbuf += sps_size_;
	
    pbuf = UI08ToBytes(pbuf, 1);            // number of pps
    pbuf = UI16ToBytes(pbuf, pps_size_);    // pps
    memcpy(pbuf, pps_, pps_size_);
    pbuf += pps_size_;

	// 保存AVC数据头信息...
	int avc_len = (int)(pbuf - avc_seq_buf);
	m_strAVCHeader.assign(avc_seq_buf, avc_len);

	// 开启视频播放线程...
	//m_lpPushThread->StartPlayByVideo(m_strSPS, m_strPPS, m_nVideoWidth, m_nVideoHeight, m_nVideoFPS);
}

CRtspThread::CRtspThread()
{
	m_nVideoHeight = 0;
	m_nVideoWidth = 0;
	m_nVideoFPS = 25;

	m_bFinished = false;
	m_strRtspUrl.clear();
	m_strAVCHeader.clear();
	m_strAACHeader.clear();
	m_lpPushThread = NULL;
	
	m_strPPS.clear();
	m_strSPS.clear();

	m_audio_rate_index = 0;
	m_audio_channel_num = 0;

	m_env_ = NULL;
	m_scheduler_ = NULL;
	m_rtspClient_ = NULL;

	m_rtspEventLoopWatchVariable = 0;
}

CRtspThread::~CRtspThread()
{
	// 设置rtsp循环退出标志...
	m_rtspEventLoopWatchVariable = 1;

	// 停止线程...
	this->StopAndWaitForThread();

	MsgLogINFO("== [~CRtspThread Thread] - Exit ==");
}

BOOL CRtspThread::InitRtsp(BOOL bUsingTCP, CPushThread * inPushThread, string & strRtspUrl)
{
	// 保存传递的参数...
	m_lpPushThread = inPushThread;
	m_strRtspUrl = strRtspUrl;

	// 创建rtsp链接环境...
	m_scheduler_ = BasicTaskScheduler::createNew();
	m_env_ = BasicUsageEnvironment::createNew(*m_scheduler_);
	m_rtspClient_ = ourRTSPClient::createNew(*m_env_, m_strRtspUrl.c_str(), 1, "rtspTransfer", bUsingTCP, this, NULL);
	
	// 2017.07.21 - by jackey => 有些服务器必须先发OPTIONS...
	// 发起第一次rtsp握手 => 先发起 OPTIONS 命令...
	m_rtspClient_->sendOptionsCommand(continueAfterOPTIONS); 

	//启动rtsp检测线程...
	this->Start();
	
	return true;
}

void CRtspThread::Entry()
{
	// 进行任务循环检测，修改 g_rtspEventLoopWatchVariable 可以让任务退出...
	ASSERT( m_env_ != NULL && m_rtspClient_ != NULL );
	m_env_->taskScheduler().doEventLoop(&m_rtspEventLoopWatchVariable);

	// 设置数据结束标志...
	m_bFinished = true;

	// 2017.06.12 - by jackey => 这里是拉流模式，不能调用 doErrNotify，需要整个断开的方式...
	// 在超时机制中会首先检测m_bFinished标志，速度快...
	// 这里只需要设置标志，超时机制会自动删除...
	//if( m_lpPushThread != NULL ) {
	//	m_lpPushThread->doErrNotify();
	//}

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

void CRtspThread::StartPushThread()
{
	if( m_lpPushThread == NULL )
		return;
	ASSERT( m_lpPushThread != NULL );
	m_lpPushThread->SetStreamPlaying(true);

	/*// 如果是流转发模式，只设置流播放状态...
	if( !m_lpPushThread->IsCameraDevice() ) {
		m_lpPushThread->SetStreamPlaying(true);
		return;
	}
	// 至少有一个音频或视频已经准备好了...
	ASSERT( m_lpPushThread->IsCameraDevice() );
	ASSERT( m_strAACHeader.size() > 0 || m_strAVCHeader.size() > 0 );
	// 直接启动rtmp推送线程，开始启动rtmp推送过程...
	m_lpPushThread->Start();*/
}

void CRtspThread::PushFrame(FMS_FRAME & inFrame)
{
	// 将数据帧加入待发送队列当中...
	ASSERT( m_lpPushThread != NULL );
	m_lpPushThread->PushFrame(inFrame);
}

void CRtspThread::WriteAACSequenceHeader(int inAudioRate, int inAudioChannel)
{
	// 首先解析并存储传递过来的参数...
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

	m_audio_channel_num = inAudioChannel;

	char   aac_seq_buf[4096] = {0};
    char * pbuf = aac_seq_buf;

    unsigned char flag = 0;
    flag = (10 << 4) |  // soundformat "10 == AAC"
        (3 << 2) |      // soundrate   "3  == 44-kHZ"
        (1 << 1) |      // soundsize   "1  == 16bit"
        1;              // soundtype   "1  == Stereo"

    pbuf = UI08ToBytes(pbuf, flag);
    pbuf = UI08ToBytes(pbuf, 0);    // aac packet type (0, header)

    // AudioSpecificConfig

	PutBitContext pb;
	init_put_bits(&pb, pbuf, 1024);
	put_bits(&pb, 5, 2);					//object type - AAC-LC
	put_bits(&pb, 4, m_audio_rate_index);	// sample rate index
	put_bits(&pb, 4, m_audio_channel_num);  // channel configuration

	//GASpecificConfig
	put_bits(&pb, 1, 0);    // frame length - 1024 samples
	put_bits(&pb, 1, 0);    // does not depend on core coder
	put_bits(&pb, 1, 0);    // is not extension

	flush_put_bits(&pb);
	pbuf += 2;

	// 保存AAC数据头信息...
	int aac_len = (int)(pbuf - aac_seq_buf);
	m_strAACHeader.assign(aac_seq_buf, aac_len);
	
	// 开启音频播放线程...
	m_lpPushThread->StartPlayByAudio(m_audio_rate_index, m_audio_channel_num);
}

void CRtspThread::WriteAVCSequenceHeader(string & inSPS, string & inPPS)
{
	// 获取 width 和 height...
	int nPicFPS = 0;
	int	nPicWidth = 0;
	int	nPicHeight = 0;
	if( inSPS.size() >  0 ) {
		/*CSPSReader _spsreader;
		bs_t    s = {0};
		s.p		  = (uint8_t *)inSPS.c_str();
		s.p_start = (uint8_t *)inSPS.c_str();
		s.p_end	  = (uint8_t *)inSPS.c_str() + inSPS.size();
		s.i_left  = 8; // 这个是固定的,对齐长度...
		_spsreader.Do_Read_SPS(&s, &nPicWidth, &nPicHeight);*/

		h264_decode_sps((BYTE*)inSPS.c_str(), inSPS.size(), nPicWidth, nPicHeight, nPicFPS);

		m_nVideoHeight = nPicHeight;
		m_nVideoWidth = nPicWidth;
		m_nVideoFPS = nPicFPS;
	}

	// 先保存 SPS 和 PPS 格式头信息..
	ASSERT( inSPS.size() > 0 && inPPS.size() > 0 );
	m_strSPS = inSPS, m_strPPS = inPPS;

	// Write AVC Sequence Header use SPS and PPS...
	char * sps_ = (char*)m_strSPS.c_str();
	char * pps_ = (char*)m_strPPS.c_str();
	int    sps_size_ = m_strSPS.size();
	int	   pps_size_ = m_strPPS.size();

    char   avc_seq_buf[4096] = {0};
    char * pbuf = avc_seq_buf;

    unsigned char flag = 0;
    flag = (1 << 4) |				// frametype "1  == keyframe"
				 7;					// codecid   "7  == AVC"
    pbuf = UI08ToBytes(pbuf, flag);	// flag...
    pbuf = UI08ToBytes(pbuf, 0);    // avc packet type (0, header)
    pbuf = UI24ToBytes(pbuf, 0);    // composition time

    // AVC Decoder Configuration Record...

    pbuf = UI08ToBytes(pbuf, 1);            // configurationVersion
    pbuf = UI08ToBytes(pbuf, sps_[1]);      // AVCProfileIndication
    pbuf = UI08ToBytes(pbuf, sps_[2]);      // profile_compatibility
    pbuf = UI08ToBytes(pbuf, sps_[3]);      // AVCLevelIndication
    pbuf = UI08ToBytes(pbuf, 0xff);         // 6 bits reserved (111111) + 2 bits nal size length - 1
    pbuf = UI08ToBytes(pbuf, 0xe1);         // 3 bits reserved (111) + 5 bits number of sps (00001)
    pbuf = UI16ToBytes(pbuf, sps_size_);    // sps
    memcpy(pbuf, sps_, sps_size_);
    pbuf += sps_size_;
	
    pbuf = UI08ToBytes(pbuf, 1);            // number of pps
    pbuf = UI16ToBytes(pbuf, pps_size_);    // pps
    memcpy(pbuf, pps_, pps_size_);
    pbuf += pps_size_;

	// 保存AVC数据头信息...
	int avc_len = (int)(pbuf - avc_seq_buf);
	m_strAVCHeader.assign(avc_seq_buf, avc_len);
	
	// 开启视频播放线程...
	m_lpPushThread->StartPlayByVideo(m_strSPS, m_strPPS, m_nVideoWidth, m_nVideoHeight, m_nVideoFPS);
}

CRtmpThread::CRtmpThread()
{
	m_nVideoHeight = 0;
	m_nVideoWidth = 0;
	m_nVideoFPS = 25;

	m_bFinished = false;
	m_strRtmpUrl.clear();
	m_strAVCHeader.clear();
	m_strAACHeader.clear();
	m_lpPushThread = NULL;
	m_lpRtmp = NULL;
	
	m_strPPS.clear();
	m_strSPS.clear();

	m_audio_rate_index = 0;
	m_audio_channel_num = 0;
}

CRtmpThread::~CRtmpThread()
{
	// 停止线程...
	this->StopAndWaitForThread();

	// 删除rtmp对象...
	if( m_lpRtmp != NULL ) {
		delete m_lpRtmp;
		m_lpRtmp = NULL;
	}

	MsgLogINFO("== [~CRtmpThread Thread] - Exit ==");
}

BOOL CRtmpThread::InitRtmp(CPushThread * inPushThread, string & strRtmpUrl)
{
	// 保存传递的参数...
	m_lpPushThread = inPushThread;
	m_strRtmpUrl = strRtmpUrl;

	// 创建rtmp对象，注意是拉数据...
	m_lpRtmp = new LibRtmp(false, false, this);
	ASSERT( m_lpRtmp != NULL );

	//启动rtmp检测线程...
	this->Start();
	
	return true;
}

void CRtmpThread::Entry()
{
	ASSERT( m_lpRtmp != NULL );
	ASSERT( m_lpPushThread != NULL );
	ASSERT( m_strRtmpUrl.size() > 0 );

	// 链接rtmp服务器...
	if( !m_lpRtmp->Open(m_strRtmpUrl.c_str()) ) {
		delete m_lpRtmp; m_lpRtmp = NULL;
		// 2017.06.12 - by jackey => 这里是拉流模式，不能调用 doErrNotify，需要整个断开的方式...
		// 在超时机制中会首先检测m_bFinished标志，速度快...
		// 这里只需要设置标志，超时机制会自动删除...
		m_bFinished = true;
		return;
	}
	// 循环读取数据并转发出去...
	while( !this->IsStopRequested() ) {
		if( m_lpRtmp->IsClosed() || !m_lpRtmp->Read() ) {
			// 2017.06.12 - by jackey => 这里是拉流模式，不能调用 doErrNotify，需要整个断开的方式...
			// 在超时机制中会首先检测m_bFinished标志，速度快...
			// 这里只需要设置标志，超时机制会自动删除...
			m_bFinished = true;
			return;
		}
	}
}

void CRtmpThread::StartPushThread()
{
	if( m_lpPushThread == NULL )
		return;
	ASSERT( m_lpPushThread != NULL );
	m_lpPushThread->SetStreamPlaying(true);

	/*// 如果是流转发模式，只设置流播放状态...
	if( !m_lpPushThread->IsCameraDevice() ) {
		m_lpPushThread->SetStreamPlaying(true);
		return;
	}
	// 至少有一个音频或视频已经准备好了...
	ASSERT( m_lpPushThread->IsCameraDevice() );
	ASSERT( m_strAACHeader.size() > 0 || m_strAVCHeader.size() > 0 );
	// 直接启动rtmp推送线程，开始启动rtmp推送过程...
	m_lpPushThread->Start();*/
}

int CRtmpThread::PushFrame(FMS_FRAME & inFrame)
{
	// 将数据帧加入待发送队列当中...
	ASSERT( m_lpPushThread != NULL );
	return m_lpPushThread->PushFrame(inFrame);
}

void CRtmpThread::WriteAACSequenceHeader(int inAudioRate, int inAudioChannel)
{
	// 首先解析并存储传递过来的参数...
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

	m_audio_channel_num = inAudioChannel;

	char   aac_seq_buf[4096] = {0};
    char * pbuf = aac_seq_buf;

    unsigned char flag = 0;
    flag = (10 << 4) |  // soundformat "10 == AAC"
        (3 << 2) |      // soundrate   "3  == 44-kHZ"
        (1 << 1) |      // soundsize   "1  == 16bit"
        1;              // soundtype   "1  == Stereo"

    pbuf = UI08ToBytes(pbuf, flag);
    pbuf = UI08ToBytes(pbuf, 0);    // aac packet type (0, header)

    // AudioSpecificConfig

	PutBitContext pb;
	init_put_bits(&pb, pbuf, 1024);
	put_bits(&pb, 5, 2);					//object type - AAC-LC
	put_bits(&pb, 4, m_audio_rate_index);	// sample rate index
	put_bits(&pb, 4, m_audio_channel_num);  // channel configuration

	//GASpecificConfig
	put_bits(&pb, 1, 0);    // frame length - 1024 samples
	put_bits(&pb, 1, 0);    // does not depend on core coder
	put_bits(&pb, 1, 0);    // is not extension

	flush_put_bits(&pb);
	pbuf += 2;

	// 保存AAC数据头信息...
	int aac_len = (int)(pbuf - aac_seq_buf);
	m_strAACHeader.assign(aac_seq_buf, aac_len);

	// 开启音频播放线程...
}

void CRtmpThread::WriteAVCSequenceHeader(string & inSPS, string & inPPS)
{
	// 获取 width 和 height...
	int nPicFPS = 0;
	int	nPicWidth = 0;
	int	nPicHeight = 0;
	if( inSPS.size() >  0 ) {
		/*CSPSReader _spsreader;
		bs_t    s = {0};
		s.p		  = (uint8_t *)inSPS.c_str();
		s.p_start = (uint8_t *)inSPS.c_str();
		s.p_end	  = (uint8_t *)inSPS.c_str() + inSPS.size();
		s.i_left  = 8; // 这个是固定的,对齐长度...
		_spsreader.Do_Read_SPS(&s, &nPicWidth, &nPicHeight);*/

		h264_decode_sps((BYTE*)inSPS.c_str(), inSPS.size(), nPicWidth, nPicHeight, nPicFPS);

		m_nVideoHeight = nPicHeight;
		m_nVideoWidth = nPicWidth;
		m_nVideoFPS = nPicFPS;
	}

	// 先保存 SPS 和 PPS 格式头信息..
	ASSERT( inSPS.size() > 0 && inPPS.size() > 0 );
	m_strSPS = inSPS, m_strPPS = inPPS;

	// Write AVC Sequence Header use SPS and PPS...
	char * sps_ = (char*)m_strSPS.c_str();
	char * pps_ = (char*)m_strPPS.c_str();
	int    sps_size_ = m_strSPS.size();
	int	   pps_size_ = m_strPPS.size();

    char   avc_seq_buf[4096] = {0};
    char * pbuf = avc_seq_buf;

    unsigned char flag = 0;
    flag = (1 << 4) |				// frametype "1  == keyframe"
				 7;					// codecid   "7  == AVC"
    pbuf = UI08ToBytes(pbuf, flag);	// flag...
    pbuf = UI08ToBytes(pbuf, 0);    // avc packet type (0, header)
    pbuf = UI24ToBytes(pbuf, 0);    // composition time

    // AVC Decoder Configuration Record...

    pbuf = UI08ToBytes(pbuf, 1);            // configurationVersion
    pbuf = UI08ToBytes(pbuf, sps_[1]);      // AVCProfileIndication
    pbuf = UI08ToBytes(pbuf, sps_[2]);      // profile_compatibility
    pbuf = UI08ToBytes(pbuf, sps_[3]);      // AVCLevelIndication
    pbuf = UI08ToBytes(pbuf, 0xff);         // 6 bits reserved (111111) + 2 bits nal size length - 1
    pbuf = UI08ToBytes(pbuf, 0xe1);         // 3 bits reserved (111) + 5 bits number of sps (00001)
    pbuf = UI16ToBytes(pbuf, sps_size_);    // sps
    memcpy(pbuf, sps_, sps_size_);
    pbuf += sps_size_;
	
    pbuf = UI08ToBytes(pbuf, 1);            // number of pps
    pbuf = UI16ToBytes(pbuf, pps_size_);    // pps
    memcpy(pbuf, pps_, pps_size_);
    pbuf += pps_size_;

	// 保存AVC数据头信息...
	int avc_len = (int)(pbuf - avc_seq_buf);
	m_strAVCHeader.assign(avc_seq_buf, avc_len);
	
	// 开启视频播放线程...
}

CPushThread::CPushThread(HWND hWndVideo, CCamera * lpCamera)
  : m_hWndVideo(hWndVideo)
  , m_lpCamera(lpCamera)
  , m_nSliceInx(0)
{
	//m_lpVideoThread = NULL;
	//m_lpAudioThread = NULL;
	m_lpPlayThread = NULL;

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

	// 删除视频播放线程...
	/*if( m_lpVideoThread != NULL ) {
		delete m_lpVideoThread;
		m_lpVideoThread = NULL;
	}
	if( m_lpAudioThread != NULL ) {
		delete m_lpAudioThread;
		m_lpAudioThread = NULL;
	}*/
	if( m_lpPlayThread != NULL ) {
		delete m_lpPlayThread;
		m_lpPlayThread = NULL;
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
	// 将视频数据放入视频线程...
	/*if( m_lpVideoThread != NULL && inFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO ) {
		m_lpVideoThread->PushFrame(inFrame.strData, inFrame.is_keyframe);
	}
	if( m_lpAudioThread != NULL && inFrame.typeFlvTag == FLV_TAG_TYPE_AUDIO ) {
		m_lpAudioThread->PushFrame(inFrame.strData);
	}*/
	if( m_lpPlayThread != NULL ) {
		m_lpPlayThread->PushFrame(inFrame);
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

	/*// 将音视频数据帧投递给播放线程 => 延时20个数据帧...
	static bool g_b_delay = false;
	if( g_b_delay && m_lpPlayThread != NULL ) {
		m_lpPlayThread->PushFrame(inFrame);
	}
	if( !g_b_delay && m_lpPlayThread != NULL && m_MapFrame.size() > 100 ) {
		// 开始发包之前启动音视频解码器...
		//this->StartPlayByVideo(m_lpRtspThread->GetVideoSPS(), m_lpRtspThread->GetVideoPPS(), m_lpRtspThread->GetVideoWidth(), m_lpRtspThread->GetVideoHeight(), 15);
		//this->StartPlayByAudio(m_lpRtspThread->GetAudioRateIndex(), m_lpRtspThread->GetAudioChannelNum());
		KH_MapFrame::iterator itorItem = m_MapFrame.begin();
		while( itorItem != m_MapFrame.end() ) {
			int nSize = m_MapFrame.count(itorItem->first);
			for(int i = 0; i < nSize; ++i) {
				FMS_FRAME & theFrame = itorItem->second;
				m_lpPlayThread->PushFrame(theFrame);
				++itorItem;
			}
		}
		g_b_delay = true;
	}*/

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

void CPushThread::StartPlayByVideo(string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS)
{
	/*if( m_lpVideoThread != NULL ) {
		delete m_lpVideoThread;
		m_lpVideoThread = NULL;
	}
	m_lpVideoThread = new CVideoThread(inSPS, inPPS, nWidth, nHeight, nFPS);
	m_lpVideoThread->InitThread(this, m_lpCamera->GetVideoWnd()->GetRenderWnd());*/

	if( m_lpPlayThread == NULL ) {
		m_lpPlayThread = new CPlayThread(this);
	}
	ASSERT( m_lpPlayThread != NULL );
	CRenderWnd * lpRenderWnd = m_lpCamera->GetVideoWnd()->GetRenderWnd();
	m_lpPlayThread->InitVideo(lpRenderWnd, inSPS, inPPS, nWidth, nHeight, nFPS);
}

void CPushThread::StartPlayByAudio(int nRateIndex, int nChannelNum)
{
	/*if( m_lpAudioThread != NULL ) {
		delete m_lpAudioThread;
		m_lpAudioThread = NULL;
	}
	m_lpAudioThread = new CAudioThread(nRateIndex, nChannelNum);
	m_lpAudioThread->InitThread(this);*/

	if( m_lpPlayThread == NULL ) {
		m_lpPlayThread = new CPlayThread(this);
	}
	ASSERT( m_lpPlayThread != NULL );
	m_lpPlayThread->InitAudio(nRateIndex, nChannelNum);
}