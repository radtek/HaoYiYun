
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

static bool DoProcSaveJpeg(AVFrame * pSrcFrame, AVPixelFormat inSrcFormat, int64_t inPTS, LPCTSTR lpPath)
{
	/*int seconds_value = 0;
	uint64_t cur_time_ns = os_gettime_ns();
	// ������ǵ�һ�Σ��������ŵ����� => ÿ��2���ӽ�ͼ...
	if( g_time_start > 0 ) {
		seconds_value = (int)((cur_time_ns - g_time_start) / 1000000 / 1000);
		if (seconds_value < MAX_SNAP_SECONDS) return false;
	}
	// ���浱ǰ������ʱ��ֵ...
	g_time_start = cur_time_ns;
	// ��ȡ������Ҫ��������Ϣ => ·�����ļ���...
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
	// ��ǰ֡����ʾʱ���Ѿ����� => ֱ�Ӷ��� => ����500�������ʱ...
	/*int64_t   nDeltaDelay = 500000000; //500����...
	if( inSysCurNS - nFramePTS > nDeltaDelay ) {
		TRACE("[Video] RG: %I64d ms, Delay: %I64d ms, Discard, Size: %d\n", nFramePTS/1000000, (inSysCurNS - nFramePTS)/1000000, nSize);
		// ֻ������ǰ֡ => ���������ã�������free��erase...
		av_frame_free(&lpSrcFrame);
		m_MapFrame.erase(itorItem);
		return;
	}*/
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
}
//
// ��Ҫ����Ƶ֡�������ͷ��Ϣ...
void CAudioDecoder::doFillPacket(string & inData, int inPTS, bool bIsKeyFrame, int inOffset)
{
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

void CAudioDecoder::doFillAudio(Uint8 * inStream, int inLen)
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
	// ��Ƶ�Ǳ�����ȡ���ݣ����ü��...
	/*if( m_AudioDecoder != NULL ) {
		int64_t min_audio_ns = m_AudioDecoder->doGetMinNextNS();
		if( min_audio_ns < min_next_ns  )
			min_next_ns = min_audio_ns;
	}*/
	m_play_next_ns = min_next_ns;
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

	// ��¼��ʼʱ���...
	m_dwStartMS = ::GetTickCount();
}

CMP4Thread::~CMP4Thread()
{
	// ֹͣ�߳�...
	this->StopAndWaitForThread();

	// �ͷ�MP4���...
	if( m_hMP4Handle != MP4_INVALID_FILE_HANDLE ) {
		MP4Close(m_hMP4Handle);
		m_hMP4Handle = MP4_INVALID_FILE_HANDLE;
	}

	MsgLogINFO("[~CMP4Thread Thread] - Exit");
}

void CMP4Thread::Entry()
{
	// ������׼��MP4�ļ�...
	if( !this->doPrepareMP4() ) {
		// ����ʧ�� => ����ֻ��Ҫ���ý�����־����ʱ���ƻ��Զ�ɾ�� => ����������ģʽ�����ܵ��� doErrNotify()...
		// �ڳ�ʱ�����л����ȼ��m_bFinished��־���ٶȿ�...
		m_bFinished = true;
		return;
	}
	// ������ת������״̬��־...
	m_lpPushThread->SetStreamPlaying(true);

	// �ڷ���ֱ��ʱ����Ҫ�����ϴ��̣߳�׼����ϣ��������߳�...
	//m_lpPushThread->Start();

	// ѭ����ȡ����֡...
	int nAccTime = 1000;
	uint32_t dwCompTime = 0;
	uint32_t dwElapsTime = 0;
	uint32_t dwOutVSendTime = 0;
	uint32_t dwOutASendTime = 0;
	while( !this->IsStopRequested() ) {

		// ������Ҫ�෢�ͼ����ӵ����ݣ�������ֻ���(ֻ��Ҫ����һ��)...
		dwElapsTime = ::GetTickCount() - m_dwStartMS + nAccTime;

		// ��ȡһ֡��Ƶ...
		if( (dwElapsTime >= dwOutVSendTime) && (!m_bVideoComplete) && (m_tidVideo != MP4_INVALID_TRACK_ID) ) {
			if( !ReadOneFrameFromMP4(m_tidVideo, m_iVSampleInx++, true, dwOutVSendTime) ) {
				m_bVideoComplete = true;
				continue;
			}
			//TRACE("[Video] ElapsTime = %lu, OutSendTime = %lu\n", dwElapsTime, dwOutVSendTime);
		}

		// ��ȡһ֡��Ƶ...
		if( (dwElapsTime >= dwOutASendTime) && (!m_bAudioComplete) && (m_tidAudio != MP4_INVALID_TRACK_ID) ) {
			if( !ReadOneFrameFromMP4(m_tidAudio, m_iASampleInx++, false, dwOutASendTime) ) {
				m_bAudioComplete = true;
				continue;
			}
			//TRACE("[Audio] ElapsTime = %lu, OutSendTime = %lu\n", dwElapsTime, dwOutASendTime);
		}

		// �����Ƶ����Ƶȫ�������������¼���...
		if( m_bVideoComplete && m_bAudioComplete ) {
			// �����ѭ����ֱ���˳��߳�...
			if( !m_bFileLoop ) {
				MsgLogINFO("== [File-Complete] ==");
				m_bFinished = true;
				return;
			}
			// ��ӡѭ������...
			ASSERT( m_bFileLoop );
			CUtilTool::MsgLog(kTxtLogger, "== [File-Loop] Count = %d ==\r\n", m_nLoopCount);
			// ���Ҫѭ�������¼�������λ������ִ�в���...
			m_bVideoComplete = ((m_tidVideo != MP4_INVALID_TRACK_ID) ? false : true);	// ����Ƶ�Ÿ�λ��־
			m_bAudioComplete = ((m_tidAudio != MP4_INVALID_TRACK_ID) ? false : true);	// ����Ƶ�Ÿ�λ��־
			dwOutVSendTime = dwOutASendTime = 0;	// ֡��ʱ����λ��0��
			m_iASampleInx = m_iVSampleInx = 1;		// ����Ƶ��������λ
			m_dwStartMS = ::GetTickCount();			// ʱ����㸴λ
			m_nLoopCount += 1;						// ѭ�������ۼ�
			nAccTime = 0;							// ����������λ
			continue;
		}
		// �����Ƶ��Ч��ʹ�û�ȡ����Ƶʱ����Ƚ�...
		if( m_tidVideo != MP4_INVALID_TRACK_ID ) {
			dwCompTime = dwOutVSendTime;
		}
		// �����Ƶ��Ч��ʹ�û�ȡ����Ƶʱ����Ƚ�...
		if( m_tidAudio != MP4_INVALID_TRACK_ID ) {
			dwCompTime = dwOutASendTime;
		}
		// �������Ƶ���У���ȡ�Ƚ�С��ֵ�Ƚ�...
		if((m_tidVideo != MP4_INVALID_TRACK_ID) && (m_tidAudio != MP4_INVALID_TRACK_ID)) {
			dwCompTime = min(dwOutASendTime, dwOutVSendTime);
		}
		// �����ǰʱ��Ƚ�С���ȴ�һ���ٶ�ȡ...
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

	uint8_t		*	pSampleData = NULL;		// ֡����ָ��
	uint32_t		nSampleSize = 0;		// ֡���ݳ���
	MP4Timestamp	nStartTime = 0;			// ��ʼʱ��
	MP4Duration		nDuration = 0;			// ����ʱ��
	MP4Duration     nOffset = 0;			// ƫ��ʱ��
	bool			bIsKeyFrame = false;	// �Ƿ�ؼ�֡
	uint32_t		timescale = 0;
	uint64_t		msectime = 0;

	timescale = MP4GetTrackTimeScale( m_hMP4Handle, tid );
	msectime = MP4GetSampleTime( m_hMP4Handle, tid, sid );

	if( false == MP4ReadSample(m_hMP4Handle, tid, sid, &pSampleData, &nSampleSize, &nStartTime, &nDuration, &nOffset, &bIsKeyFrame) )
		return false;

	// ���㷢��ʱ�� => PTS => �̶�ʱ��ת���ɺ���...
	msectime *= UINT64_C( 1000 );
	msectime /= timescale;
	// ���㿪ʼʱ�� => DTS => �̶�ʱ��ת���ɺ���...
	nStartTime *= UINT64_C( 1000 );
	nStartTime /= timescale;
	// ����ƫ��ʱ�� => CTTS => �̶�ʱ��ת���ɺ���...
	nOffset *= UINT64_C( 1000 );
	nOffset /= timescale;

	// ע�⣺msectime | nOffset | m_dwMP4Duration ��Ҫͳһ�ɺ���ʱ��...
	FMS_FRAME	theFrame;
	theFrame.typeFlvTag = (bIsVideo ? FLV_TAG_TYPE_VIDEO : FLV_TAG_TYPE_AUDIO);	// ��������Ƶ��־
	theFrame.dwSendTime = (uint32_t)msectime + m_dwMP4Duration * m_nLoopCount;	// ����ǳ���Ҫ��ǣ�浽ѭ������
	theFrame.dwRenderOffset = nOffset;  // 2017.07.06 - by jackey => ����ʱ��ƫ��ֵ...
	theFrame.is_keyframe = bIsKeyFrame;
	theFrame.strData.assign((char*)pSampleData, nSampleSize);

	// ������Ҫ�ͷŶ�ȡ�Ļ�����...
	MP4Free(pSampleData);
	pSampleData = NULL;

	// ������֡��������Ͷ��е���...
	ASSERT( m_lpPushThread != NULL );
	m_lpPushThread->PushFrame(theFrame);

	// ���ط���ʱ��(����) => �ѽ��̶�ʱ��ת�����˺���...
	outSendTime = (uint32_t)msectime;
	
	//TRACE("[%s] duration = %I64d, offset = %I64d, KeyFrame = %d, SendTime = %lu, StartTime = %I64d, Size = %lu\n", bIsVideo ? "Video" : "Audio", nDuration, nOffset, bIsKeyFrame, outSendTime, nStartTime, nSampleSize);

	return true;
}

BOOL CMP4Thread::InitMP4(CPushThread * inPushThread, string & strMP4File)
{
	// Ŀǰ��ģʽ���ļ�һ����ѭ��ģʽ...
	m_lpPushThread = inPushThread;
	m_strMP4File = strMP4File;
	m_bFileLoop = true;

	this->Start();

	return true;
}

void myMP4LogCallback( MP4LogLevel loglevel, const char* fmt, va_list ap)
{
	// ��ϴ��ݹ����ĸ�ʽ...
	CString	strDebug;
	strDebug.FormatV(fmt, ap);
	if( (strDebug.ReverseFind('\r') < 0) && (strDebug.ReverseFind('\n') < 0) ) {
		strDebug.Append("\r\n");
	}
	// ���и�ʽת��������ӡ����...
	string strANSI = CUtilTool::UTF8_ANSI(strDebug);
	TRACE(strANSI.c_str());
}

bool CMP4Thread::doPrepareMP4()
{
	// ����MP4���Լ��� => �������ϸ����...
	//MP4LogLevel theLevel = MP4LogGetLevel();
	//MP4LogSetLevel(MP4_LOG_VERBOSE4);
	//MP4SetLogCallback(myMP4LogCallback);
	// �ļ���ʧ�ܣ�ֱ�ӷ���...
	string strUTF8 = CUtilTool::ANSI_UTF8(m_strMP4File.c_str());
	m_hMP4Handle = MP4Read( strUTF8.c_str() );
	if( m_hMP4Handle == MP4_INVALID_FILE_HANDLE ) {
		MP4Close(m_hMP4Handle);
		MsgLogGM(GM_File_Not_Open);
		m_hMP4Handle = MP4_INVALID_FILE_HANDLE;
		return false;
	}
	// �ļ�����ȷ, ��ȡ��Ҫ������...
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
	
	// ���Ȼ�ȡ�ļ���ÿ��̶������̶ܿ���(���Ǻ�����)...
	uint32_t dwFileScale = MP4GetTimeScale(inFile);
	MP4Duration theDuration = MP4GetDuration(inFile);
	// �ܺ����� = �̶ܿ���*1000/ÿ��̶��� => �ȳ˷����Խ������...
	m_dwMP4Duration = theDuration*1000/dwFileScale;

	// ��ȡ��Ҫ�������Ϣ...
    uint32_t trackCount = MP4GetNumberOfTracks( inFile );
    for( uint32_t i = 0; i < trackCount; ++i ) {
		MP4TrackId  id = MP4FindTrackId( inFile, i );
		const char* type = MP4GetTrackType( inFile, id );
		if( MP4_IS_VIDEO_TRACK_TYPE( type ) ) {
			// ��Ƶ�Ѿ���Ч�������һ��...
			if( m_tidVideo > 0 )
				continue;
			// ��ȡ��Ƶ��Ϣ...
			m_tidVideo = id;
			m_bVideoComplete = false;
			char * lpVideoInfo = MP4Info(inFile, id);
			free(lpVideoInfo);
			// ��ȡ��Ƶ��PPS/SPS��Ϣ...
			uint8_t  ** spsHeader = NULL;
			uint8_t  ** ppsHeader = NULL;
			uint32_t  * spsSize = NULL;
			uint32_t  * ppsSize = NULL;
			uint32_t    ix = 0;
			bool bResult = MP4GetTrackH264SeqPictHeaders(inFile, id, &spsHeader, &spsSize, &ppsHeader, &ppsSize);
			for(ix = 0; spsSize[ix] != 0; ++ix) {
				// SPSָ��ͳ���...
				uint8_t * lpSPS = spsHeader[ix];
				uint32_t  nSize = spsSize[ix];
				// ֻ�洢��һ��SPS��Ϣ...
				if( m_strSPS.size() <= 0 && nSize > 0 ) {
					m_strSPS.assign((char*)lpSPS, nSize);
				}
				free(spsHeader[ix]);
			}
			free(spsHeader);
			free(spsSize);
			for(ix = 0; ppsSize[ix] != 0; ++ix) {
				// PPSָ��ͳ���...
				uint8_t * lpPPS = ppsHeader[ix];
				uint32_t  nSize = ppsSize[ix];
				// ֻ�洢��һ��PPS��Ϣ...
				if( m_strPPS.size() <= 0 && nSize > 0 ) {
					m_strPPS.assign((char*)lpPPS, nSize);
				}
				free(ppsHeader[ix]);
			}
			free(ppsHeader);
			free(ppsSize);

			// ������Ƶ����ͷ...
			this->WriteAVCSequenceHeader();
		}
		else if( MP4_IS_AUDIO_TRACK_TYPE( type ) ) {
			// ��Ƶ�Ѿ���Ч�������һ��...
			if( m_tidAudio > 0 )
				continue;
			// ��ȡ��Ƶ��Ϣ...
			m_tidAudio = id;
			m_bAudioComplete = false;
			char * lpAudioInfo = MP4Info(inFile, id);
			free(lpAudioInfo);

			// ��ȡ��Ƶ������/����/������/������Ϣ...
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
			// ��ȡ��Ƶ��չ��Ϣ...
            uint8_t  * pAES = NULL;
            uint32_t   nSize = 0;
            bool haveEs = MP4GetTrackESConfiguration(inFile, id, &pAES, &nSize);
			// �洢��Ƶ��չ��Ϣ...
			if( m_strAES.size() <= 0 && nSize > 0 ) {
				m_strAES.assign((char*)pAES, nSize);
			}
			// �ͷŷ���Ļ���...
			if( pAES != NULL ) {
				free(pAES);
				pAES = NULL;
			}
			// ������Ƶ����ͷ...
			this->WriteAACSequenceHeader();
		}
	}
	// �����Ƶ����Ƶ��û�У�����ʧ��...
	if((m_tidVideo == MP4_INVALID_TRACK_ID) && (m_tidAudio == MP4_INVALID_TRACK_ID)) {
		MsgLogGM(GM_File_Read_Err);
		return false;
	}
	// һ�����������سɹ�...
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

	// ����AAC����ͷ��Ϣ...
	int aac_len = (int)(pbuf - aac_seq_buf);
	m_strAACHeader.assign(aac_seq_buf, aac_len);
	
	// ������Ƶ�����߳�...
	//m_lpPushThread->StartPlayByAudio(m_audio_rate_index, m_audio_channel_num);
}

void CMP4Thread::WriteAVCSequenceHeader()
{
	// ��ȡ width �� height...
	int nPicFPS = 0;
	int	nPicWidth = 0;
	int	nPicHeight = 0;
	if( m_strSPS.size() >  0 ) {
		/*CSPSReader _spsreader;
		bs_t    s = {0};
		s.p		  = (uint8_t *)m_strSPS.c_str();
		s.p_start = (uint8_t *)m_strSPS.c_str();
		s.p_end	  = (uint8_t *)m_strSPS.c_str() + m_strSPS.size();
		s.i_left  = 8; // ����ǹ̶���,���볤��...
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

	// ����AVC����ͷ��Ϣ...
	int avc_len = (int)(pbuf - avc_seq_buf);
	m_strAVCHeader.assign(avc_seq_buf, avc_len);

	// ������Ƶ�����߳�...
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
	// ����rtspѭ���˳���־...
	m_rtspEventLoopWatchVariable = 1;

	// ֹͣ�߳�...
	this->StopAndWaitForThread();

	MsgLogINFO("== [~CRtspThread Thread] - Exit ==");
}

BOOL CRtspThread::InitRtsp(BOOL bUsingTCP, CPushThread * inPushThread, string & strRtspUrl)
{
	// ���洫�ݵĲ���...
	m_lpPushThread = inPushThread;
	m_strRtspUrl = strRtspUrl;

	// ����rtsp���ӻ���...
	m_scheduler_ = BasicTaskScheduler::createNew();
	m_env_ = BasicUsageEnvironment::createNew(*m_scheduler_);
	m_rtspClient_ = ourRTSPClient::createNew(*m_env_, m_strRtspUrl.c_str(), 1, "rtspTransfer", bUsingTCP, this, NULL);
	
	// 2017.07.21 - by jackey => ��Щ�����������ȷ�OPTIONS...
	// �����һ��rtsp���� => �ȷ��� OPTIONS ����...
	m_rtspClient_->sendOptionsCommand(continueAfterOPTIONS); 

	//����rtsp����߳�...
	this->Start();
	
	return true;
}

void CRtspThread::Entry()
{
	// ��������ѭ����⣬�޸� g_rtspEventLoopWatchVariable �����������˳�...
	ASSERT( m_env_ != NULL && m_rtspClient_ != NULL );
	m_env_->taskScheduler().doEventLoop(&m_rtspEventLoopWatchVariable);

	// �������ݽ�����־...
	m_bFinished = true;

	// 2017.06.12 - by jackey => ����������ģʽ�����ܵ��� doErrNotify����Ҫ�����Ͽ��ķ�ʽ...
	// �ڳ�ʱ�����л����ȼ��m_bFinished��־���ٶȿ�...
	// ����ֻ��Ҫ���ñ�־����ʱ���ƻ��Զ�ɾ��...
	//if( m_lpPushThread != NULL ) {
	//	m_lpPushThread->doErrNotify();
	//}

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

void CRtspThread::StartPushThread()
{
	if( m_lpPushThread == NULL )
		return;
	ASSERT( m_lpPushThread != NULL );
	m_lpPushThread->SetStreamPlaying(true);

	/*// �������ת��ģʽ��ֻ����������״̬...
	if( !m_lpPushThread->IsCameraDevice() ) {
		m_lpPushThread->SetStreamPlaying(true);
		return;
	}
	// ������һ����Ƶ����Ƶ�Ѿ�׼������...
	ASSERT( m_lpPushThread->IsCameraDevice() );
	ASSERT( m_strAACHeader.size() > 0 || m_strAVCHeader.size() > 0 );
	// ֱ������rtmp�����̣߳���ʼ����rtmp���͹���...
	m_lpPushThread->Start();*/
}

void CRtspThread::PushFrame(FMS_FRAME & inFrame)
{
	// ������֡��������Ͷ��е���...
	ASSERT( m_lpPushThread != NULL );
	m_lpPushThread->PushFrame(inFrame);
}

void CRtspThread::WriteAACSequenceHeader(int inAudioRate, int inAudioChannel)
{
	// ���Ƚ������洢���ݹ����Ĳ���...
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

	// ����AAC����ͷ��Ϣ...
	int aac_len = (int)(pbuf - aac_seq_buf);
	m_strAACHeader.assign(aac_seq_buf, aac_len);
	
	// ������Ƶ�����߳�...
	m_lpPushThread->StartPlayByAudio(m_audio_rate_index, m_audio_channel_num);
}

void CRtspThread::WriteAVCSequenceHeader(string & inSPS, string & inPPS)
{
	// ��ȡ width �� height...
	int nPicFPS = 0;
	int	nPicWidth = 0;
	int	nPicHeight = 0;
	if( inSPS.size() >  0 ) {
		/*CSPSReader _spsreader;
		bs_t    s = {0};
		s.p		  = (uint8_t *)inSPS.c_str();
		s.p_start = (uint8_t *)inSPS.c_str();
		s.p_end	  = (uint8_t *)inSPS.c_str() + inSPS.size();
		s.i_left  = 8; // ����ǹ̶���,���볤��...
		_spsreader.Do_Read_SPS(&s, &nPicWidth, &nPicHeight);*/

		h264_decode_sps((BYTE*)inSPS.c_str(), inSPS.size(), nPicWidth, nPicHeight, nPicFPS);

		m_nVideoHeight = nPicHeight;
		m_nVideoWidth = nPicWidth;
		m_nVideoFPS = nPicFPS;
	}

	// �ȱ��� SPS �� PPS ��ʽͷ��Ϣ..
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

	// ����AVC����ͷ��Ϣ...
	int avc_len = (int)(pbuf - avc_seq_buf);
	m_strAVCHeader.assign(avc_seq_buf, avc_len);
	
	// ������Ƶ�����߳�...
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
	// ֹͣ�߳�...
	this->StopAndWaitForThread();

	// ɾ��rtmp����...
	if( m_lpRtmp != NULL ) {
		delete m_lpRtmp;
		m_lpRtmp = NULL;
	}

	MsgLogINFO("== [~CRtmpThread Thread] - Exit ==");
}

BOOL CRtmpThread::InitRtmp(CPushThread * inPushThread, string & strRtmpUrl)
{
	// ���洫�ݵĲ���...
	m_lpPushThread = inPushThread;
	m_strRtmpUrl = strRtmpUrl;

	// ����rtmp����ע����������...
	m_lpRtmp = new LibRtmp(false, false, this);
	ASSERT( m_lpRtmp != NULL );

	//����rtmp����߳�...
	this->Start();
	
	return true;
}

void CRtmpThread::Entry()
{
	ASSERT( m_lpRtmp != NULL );
	ASSERT( m_lpPushThread != NULL );
	ASSERT( m_strRtmpUrl.size() > 0 );

	// ����rtmp������...
	if( !m_lpRtmp->Open(m_strRtmpUrl.c_str()) ) {
		delete m_lpRtmp; m_lpRtmp = NULL;
		// 2017.06.12 - by jackey => ����������ģʽ�����ܵ��� doErrNotify����Ҫ�����Ͽ��ķ�ʽ...
		// �ڳ�ʱ�����л����ȼ��m_bFinished��־���ٶȿ�...
		// ����ֻ��Ҫ���ñ�־����ʱ���ƻ��Զ�ɾ��...
		m_bFinished = true;
		return;
	}
	// ѭ����ȡ���ݲ�ת����ȥ...
	while( !this->IsStopRequested() ) {
		if( m_lpRtmp->IsClosed() || !m_lpRtmp->Read() ) {
			// 2017.06.12 - by jackey => ����������ģʽ�����ܵ��� doErrNotify����Ҫ�����Ͽ��ķ�ʽ...
			// �ڳ�ʱ�����л����ȼ��m_bFinished��־���ٶȿ�...
			// ����ֻ��Ҫ���ñ�־����ʱ���ƻ��Զ�ɾ��...
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

	/*// �������ת��ģʽ��ֻ����������״̬...
	if( !m_lpPushThread->IsCameraDevice() ) {
		m_lpPushThread->SetStreamPlaying(true);
		return;
	}
	// ������һ����Ƶ����Ƶ�Ѿ�׼������...
	ASSERT( m_lpPushThread->IsCameraDevice() );
	ASSERT( m_strAACHeader.size() > 0 || m_strAVCHeader.size() > 0 );
	// ֱ������rtmp�����̣߳���ʼ����rtmp���͹���...
	m_lpPushThread->Start();*/
}

int CRtmpThread::PushFrame(FMS_FRAME & inFrame)
{
	// ������֡��������Ͷ��е���...
	ASSERT( m_lpPushThread != NULL );
	return m_lpPushThread->PushFrame(inFrame);
}

void CRtmpThread::WriteAACSequenceHeader(int inAudioRate, int inAudioChannel)
{
	// ���Ƚ������洢���ݹ����Ĳ���...
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

	// ����AAC����ͷ��Ϣ...
	int aac_len = (int)(pbuf - aac_seq_buf);
	m_strAACHeader.assign(aac_seq_buf, aac_len);

	// ������Ƶ�����߳�...
}

void CRtmpThread::WriteAVCSequenceHeader(string & inSPS, string & inPPS)
{
	// ��ȡ width �� height...
	int nPicFPS = 0;
	int	nPicWidth = 0;
	int	nPicHeight = 0;
	if( inSPS.size() >  0 ) {
		/*CSPSReader _spsreader;
		bs_t    s = {0};
		s.p		  = (uint8_t *)inSPS.c_str();
		s.p_start = (uint8_t *)inSPS.c_str();
		s.p_end	  = (uint8_t *)inSPS.c_str() + inSPS.size();
		s.i_left  = 8; // ����ǹ̶���,���볤��...
		_spsreader.Do_Read_SPS(&s, &nPicWidth, &nPicHeight);*/

		h264_decode_sps((BYTE*)inSPS.c_str(), inSPS.size(), nPicWidth, nPicHeight, nPicFPS);

		m_nVideoHeight = nPicHeight;
		m_nVideoWidth = nPicWidth;
		m_nVideoFPS = nPicFPS;
	}

	// �ȱ��� SPS �� PPS ��ʽͷ��Ϣ..
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

	// ����AVC����ͷ��Ϣ...
	int avc_len = (int)(pbuf - avc_seq_buf);
	m_strAVCHeader.assign(avc_seq_buf, avc_len);
	
	// ������Ƶ�����߳�...
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
	// ֹͣ�߳�...
	this->StopAndWaitForThread();

	// ɾ����Ƶ�����߳�...
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

	// ɾ��rtmp�����������ӻ��⣬����Connect����ʱrtmp�Ѿ���ɾ��������ڴ����...
	{
		OSMutexLocker theLock(&m_Mutex);
		if( m_lpRtmpPush != NULL ) {
			delete m_lpRtmpPush;
			m_lpRtmpPush = NULL;
		}
	}
	// ɾ��rtmp�߳�...
	if( m_lpRtmpThread != NULL ) {
		delete m_lpRtmpThread;
		m_lpRtmpThread = NULL;
	}
	// ɾ��rtsp�߳�...
	if( m_lpRtspThread != NULL ) {
		delete m_lpRtspThread;
		m_lpRtspThread = NULL;
	}
	// ɾ��MP4�߳�...
	if( m_lpMP4Thread != NULL ) {
		delete m_lpMP4Thread;
		m_lpMP4Thread = NULL;
	}
	// �ж�¼��...
	this->StreamEndRecord();

	// ��λ��ر�־...
	m_nKeyFrame = 0;
	m_bIsPublishing = false;
	m_bStreamPlaying = false;
	MsgLogINFO("== [~CPushThread Thread] - Exit ==");

	// �ͷŴ����ļ�...
#ifdef _SAVE_H264_
	fclose(m_lpH264File);
	m_lpH264File = NULL;
#endif
}
//
// ��������ͷ�豸���̳߳�ʼ��...
/*BOOL CPushThread::DeviceInitThread(CString & strRtspUrl, string & strRtmpUrl)
{
	if( m_lpCamera == NULL )
		return false;
	ASSERT( m_lpCamera != NULL );
	// ���洫�ݹ����Ĳ���...
	m_strRtmpUrl = strRtmpUrl;
	m_nStreamProp = m_lpCamera->GetStreamProp();
	if( m_nStreamProp != kStreamDevice )
		return false;
	ASSERT( m_nStreamProp == kStreamDevice );
	// ����rtsp���������߳�...
	string strNewRtsp = strRtspUrl.GetString();
	m_lpRtspThread = new CRtspThread();
	m_lpRtspThread->InitRtsp(this, strNewRtsp);

	// ����rtmp�ϴ�����...
	m_lpRtmpPush = new LibRtmp(false, true, NULL, NULL);
	ASSERT( m_lpRtmpPush != NULL );

	// ��ʼ���������ݳ�ʱʱ���¼��� => ���� 3 ���������ݽ��գ����ж�Ϊ��ʱ...
	m_dwTimeOutMS = ::GetTickCount();

	// ���ﲻ�������̣߳��ȴ�����׼���׵�֮���������...
	// ���� rtsp �����߳��������豸�������߳�...

	return true;
}*/
//
// ������ת���̵߳ĳ�ʼ��...
BOOL CPushThread::StreamInitThread(BOOL bFileMode, BOOL bUsingTCP, string & strStreamUrl, string & strStreamMP4)
{
	// ���洫�ݹ����Ĳ���...
	if( m_lpCamera == NULL )
		return false;
	ASSERT( m_lpCamera != NULL );
	// ����MP4�̻߳�rtsp��rtmp�߳�...
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
	// ��¼��ǰʱ�䣬��λ�����룩...
	DWORD dwInitTimeMS = ::GetTickCount();
	// ��ʼ����������ʱ����� => ÿ��1���Ӹ�λ...
	m_dwRecvTimeMS = dwInitTimeMS;
	// ��ʼ���������ݳ�ʱʱ���¼��� => ���� 3 ���������ݽ��գ����ж�Ϊ��ʱ...
	m_dwTimeOutMS = dwInitTimeMS;
	// ��¼ͨ����ͼ���ʱ�䣬��λ�����룩...
	m_dwSnapTimeMS = dwInitTimeMS;
	return true;
}
//
// ������ת��ֱ���ϴ�...
BOOL CPushThread::StreamStartLivePush(string & strRtmpUrl)
{
	if( m_lpCamera == NULL )
		return false;
	ASSERT( m_lpCamera != NULL );
	// IE8�������������鲥�������Ҫȷ��ֻ����һ��...
	if( m_lpRtmpPush != NULL )
		return true;
	// ������Ҫ��������Ϣ...
	m_strRtmpUrl = strRtmpUrl;
	ASSERT( strRtmpUrl.size() > 0 );
	// ����rtmp�ϴ�����...
	m_lpRtmpPush = new LibRtmp(false, true, NULL);
	ASSERT( m_lpRtmpPush != NULL );
	// �������������߳�...
	this->Start();
	return true;
}
//
// ֹͣ��ת��ֱ���ϴ�...
BOOL CPushThread::StreamStopLivePush()
{
	if( m_lpCamera == NULL )
		return false;
	ASSERT( m_lpCamera != NULL );
	// ֹͣ�ϴ��̣߳���λ����״̬��־...
	this->StopAndWaitForThread();
	this->SetStreamPublish(false);
	this->EndSendPacket();
	// ��λ������Ϣ...
	m_dwFirstSendTime = 0;
	m_nCurSendByte = 0;
	m_nSendKbps = 0;
	return true;
}

BOOL CPushThread::MP4CreateVideoTrack()
{
	// �ж�MP4�����Ƿ���Ч...
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
	// �жϻ�ȡ���ݵ���Ч�� => û����Ƶ��ֱ�ӷ���...
	if( nWidth <= 0 || nHeight <= 0 || strPPS.size() <= 0 || strSPS.size() <= 0 )
		return false;
	// ������Ƶ���...
	ASSERT( !m_lpRecMP4->IsVideoCreated() );
	return m_lpRecMP4->CreateVideoTrack(m_strUTF8MP4.c_str(), VIDEO_TIME_SCALE, nWidth, nHeight, strSPS, strPPS);
}

BOOL CPushThread::MP4CreateAudioTrack()
{
	// �ж�MP4�����Ƿ���Ч...
	if( m_lpRecMP4 == NULL || m_strUTF8MP4.size() <= 0 )
		return false;
	ASSERT( m_lpRecMP4 != NULL && m_strUTF8MP4.size() > 0 );

	string strAAC;
	int audio_type = 2;
	int	audio_rate_index = 0;			// �����ʱ��
	int	audio_channel_num = 0;			// ������Ŀ
	int	audio_sample_rate = 48000;		// ��Ƶ������

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
	// û����Ƶ��ֱ�ӷ��� => 2017.07.25 - by jackey...
	if( strAAC.size() <= 0 )
		return false;
  
	// ����������ȡ������...
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
	
	// ���� AES ����ͷ��Ϣ...
	string strAES;
	int aac_len = (int)(pbuf - aac_seq_buf);
	strAES.assign(aac_seq_buf, aac_len);

	// ������Ƶ���...
	ASSERT( !m_lpRecMP4->IsAudioCreated() );
	return m_lpRecMP4->CreateAudioTrack(m_strUTF8MP4.c_str(), audio_sample_rate, strAES);
}
//
// ������Ƭ¼�����...
BOOL CPushThread::BeginRecSlice()
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
	// 2018.02.06 - by jackey => ������Ƭ��־�ֶ�...
	// ׼��MP4¼������ => PATH + Uniqid + DBCameraID + CreateTime + CourseID + SliceID + SliceIndex
	m_dwRecCTime = (DWORD)::time(NULL);
	m_strMP4Name.Format("%s\\%s_%d_%lu_%d_%s_%d", strSavePath.c_str(), strUniqid.c_str(), 
						nDBCameraID, m_dwRecCTime, nCourseID, m_strSliceID, ++m_nSliceInx);
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
BOOL CPushThread::EndRecSlice()
{
	// �ر�¼��֮ǰ����ȡ��¼��ʱ��ͳ���...
	if( m_lpRecMP4 != NULL ) {
		m_lpRecMP4->Close();
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
void CPushThread::doStreamSnapJPG(int nRecSecond)
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
// ʹ��ffmpeg��������֡��̬��ͼ...
void CPushThread::doFFmpegSnapJPG()
{
	// �ж�ͨ�������Ƿ���Ч...
	if( m_lpCamera == NULL || m_strSnapFrame.size() <= 0 )
		return;
	ASSERT( m_lpCamera != NULL );
	ASSERT( m_strSnapFrame.size() > 0 );
	// ��ȡ·�������Ϣ...
	CString strJPGFile;
	int nDBCameraID = m_lpCamera->GetDBCameraID();
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	string & strSavePath = theConfig.GetSavePath();
	// ׼��JPG��ͼ�ļ� => PATH + live + DBCameraID + .jpg
	strJPGFile.Format("%s\\live_%d.jpg", strSavePath.c_str(), nDBCameraID);
	// ׼����ͼ��Ҫ��sps��pps...
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
	// ������д��sps����д��pps...
	strSnapData.append((char*)&dwTag, sizeof(DWORD));
	strSnapData.append(strSPS);
	strSnapData.append((char*)&dwTag, sizeof(DWORD));
	strSnapData.append(strPPS);
	// ���д��ؼ�֡����ʼ���Ѿ�����...
	strSnapData.append(m_strSnapFrame);
	// ���ý�ͼ�ӿڽ��ж�̬��ͼ...
	if( !theConfig.FFmpegSnapJpeg(strSnapData, strJPGFile) ) {
		MsgLogGM(GM_Snap_Jpg_Err);
	} else {
		CUtilTool::MsgLog(kTxtLogger, "== ffmpeg snap jpg(%d) ==\r\n", nDBCameraID);
	}
}
//
// ��ʼ��ת��ģʽ��¼��...
BOOL CPushThread::StreamBeginRecord()
{
	// ������Ҫ���л��Ᵽ��...
	OSMutexLocker theLock(&m_Mutex);
	// �ж�״̬�Ƿ���Ч...
	if( !this->IsStreamPlaying() )
		return false;
	ASSERT( this->IsStreamPlaying() );
	// �ͷ�¼�����...
	if( m_lpRecMP4 != NULL ) {
		delete m_lpRecMP4;
		m_lpRecMP4 = NULL;
	}
	// �����µ�¼�����...
	m_lpRecMP4 = new LibMP4();
	ASSERT( m_lpRecMP4 != NULL );
	// ������Ƭ��־��Ϣ...
	ULARGE_INTEGER	llTimCountCur = {0};
	::GetSystemTimeAsFileTime((FILETIME *)&llTimCountCur);
	m_strSliceID.Format("%I64d", llTimCountCur.QuadPart);
	m_nSliceInx = 0;
	// ������һ����Ƭ¼��...
	return this->BeginRecSlice();
}
//
// ����¼���� => ���ж��Ƿ����µ���Ƭ...
BOOL CPushThread::StreamWriteRecord(FMS_FRAME & inFrame)
{
	// ¼�������Ч������д��...
	if( m_lpRecMP4 == NULL )
		return false;
	ASSERT( m_lpRecMP4 != NULL );
	// ����д�̲������ڲ����ж��Ƿ��ܹ�д��...
	BOOL bIsVideo = ((inFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO) ? true : false);
	if( !m_lpRecMP4->WriteSample(bIsVideo, (BYTE*)inFrame.strData.c_str(), inFrame.strData.size(), inFrame.dwSendTime, inFrame.dwRenderOffset, inFrame.is_keyframe) )
		return false;
	// ������Ҫ��¼��¼���ļ���С����¼������...
	m_dwWriteSize = m_lpRecMP4->GetWriteSize();
	m_dwWriteSec = m_lpRecMP4->GetWriteSec();
	//TRACE("Write Second: %lu\n", m_dwWriteSec);
	// ���û����Ƶ������������...
	if( !m_lpRecMP4->IsVideoCreated() )
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
void CPushThread::doSaveInterFrame()
{
	CUtilTool::MsgLog(kTxtLogger,"== [doSaveInterFrame] nKeyMonitor = %d, MonitorSize = %d ==\r\n", m_nKeyMonitor, m_MapMonitor.size());
	KH_MapFrame::iterator itorItem = m_MapMonitor.begin();
	while( itorItem != m_MapMonitor.end() ) {
		int nSize = m_MapMonitor.count(itorItem->first);
		// ����ͬʱ���������֡����ѭ������ ...
		for(int i = 0; i < nSize; ++i) {
			FMS_FRAME & theFrame = itorItem->second;
			BOOL bIsVideo = ((theFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO) ? true : false);
			m_lpRecMP4->WriteSample(bIsVideo, (BYTE*)theFrame.strData.c_str(), theFrame.strData.size(), theFrame.dwSendTime, theFrame.dwRenderOffset, theFrame.is_keyframe);
			// �ۼ����ӣ�������һ���ڵ�...
			++itorItem;
		}
	}
}
//
// ֹͣ��ת��ģʽ��¼�� => ɾ��¼�����ֹͣ��Ƭ...
BOOL CPushThread::StreamEndRecord()
{
	// ������Ҫ���л��Ᵽ��...
	OSMutexLocker theLock(&m_Mutex);
	// ��ֹͣ¼����Ƭ����...
	this->EndRecSlice();
	// ��ɾ��¼�����...
	if( m_lpRecMP4 != NULL ) {
		delete m_lpRecMP4;
		m_lpRecMP4 = NULL;
	}
	// �����ս�������...
	m_MapMonitor.clear();
	m_nKeyMonitor = 0;
	return true;
}
//
// �����Ƿ�����¼���־...
BOOL CPushThread::IsRecording()
{
	//2018.01.01 - by jackey => ���ﲻ�ܼӻ��⣬�Ῠס����...
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
	// ע�⣺������Ҫ֪ͨ��վ��ͨ������������...
	// ע�⣺������ר�Ŵ�����ת��ģʽ������֪ͨ...
	m_bStreamPlaying = bFlag;
	if( m_lpCamera != NULL ) {
		m_lpCamera->doStreamStatus(bFlag ? "������" : "δ����");
		m_lpCamera->doWebStatCamera(kCameraRun);
	}
}

void CPushThread::SetStreamPublish(BOOL bFlag)
{
	m_bIsPublishing = bFlag;
	if( m_lpCamera == NULL )
		return;
	if( m_bIsPublishing ) {
		m_lpCamera->doStreamStatus("ֱ����");
	} else {
		m_lpCamera->doStreamStatus(m_bStreamPlaying ? "������" : "δ����");
	}
}
//
// ����ֻ��Ҫ��������ʧ�ܵ����������ʧ�ܻ�ȫ��ͨ����ʱ�����Զ�����...
void CPushThread::doErrPushNotify()
{
	// ����߳��Ѿ�������ɾ���ˣ�ֱ���˳�...
	if( m_bDeleteFlag )
		return;
	ASSERT( !m_bDeleteFlag );
	// ���ڶ��������Ч...
	if( m_hWndVideo == NULL )
		return;
	ASSERT( m_hWndVideo != NULL );
	// ����ͷ�豸ģʽ => ֹֻͣrtmp�ϴ�����...
	// ������ת��ģʽ => ֹֻͣrtmp�ϴ�����...
	//WPARAM wMsgID = ((m_nStreamProp == kStreamDevice) ? WM_ERR_PUSH_MSG : WM_STOP_STREAM_MSG);
	WPARAM wMsgID = WM_STOP_LIVE_PUSH_MSG;
	::PostMessage(m_hWndVideo, wMsgID, NULL, NULL);
	// ��ӡ�˳���Ϣ...
	CUtilTool::MsgLog(kTxtLogger, _T("== Camera(%d) stop push by SRS ==\r\n"), ((m_lpCamera != NULL) ? m_lpCamera->GetDBCameraID() : 0));
}

void CPushThread::Entry()
{
	// ����rtmp server��ʧ�ܣ�֪ͨ�ϲ�ɾ��֮...
	if( !this->OpenRtmpUrl() ) {
		MsgLogINFO("[CPushThread::OpenRtmpUrl] - Error");
		this->doErrPushNotify();
		return;
	}
	// ���ֳɹ�������metadata���ݰ���ʧ�ܣ�֪ͨ�ϲ�ɾ��֮...
	if( !this->SendMetadataPacket() ) {
		MsgLogINFO("[CPushThread::SendMetadataPacket] - Error");
		this->doErrPushNotify();
		return;
	}
	// ������Ƶ����ͷ���ݰ���ʧ�ܣ�֪ͨ�ϲ�ɾ��֮...
	if( !this->SendAVCSequenceHeaderPacket() ) {
		MsgLogINFO("[CPushThread::SendAVCSequenceHeaderPacket] - Error");
		this->doErrPushNotify();
		return;
	}
	// ������Ƶ����ͷ���ݰ���ʧ�ܣ�֪ͨ�ϲ�ɾ��֮...
	if( !this->SendAACSequenceHeaderPacket() ) {
		MsgLogINFO("[CPushThread::SendAACSequenceHeaderPacket] - Error");
		this->doErrPushNotify();
		return;
	}
	// ���淢���ɹ���־...
	this->SetStreamPublish(true);
	// ������Ҫ�ĵ�һ֡ʱ���...
	this->BeginSendPacket();
	// 2017.06.15 - by jackey => ȡ����������ʱ��������...
	// �ϴ�׼����ϣ�֪ͨ RemoteSession ���Է�����Ϣ��ֱ����������...
	//if( m_lpCamera != NULL ) {
	//	m_lpCamera->doDelayTransmit(GM_NoErr);
	//}
	// ��ʼ�߳�ѭ��...
	int nRetValue = 0;
	uint32_t dwStartTimeMS = ::GetTickCount();
	while( !this->IsStopRequested() ) {
		// ���ʱ����������1000���룬�����һ�η�������...
		if( (::GetTickCount() - dwStartTimeMS) >= 1000 ) {
			dwStartTimeMS = ::GetTickCount();
			m_nSendKbps = m_nCurSendByte * 8 / 1024;
			m_nCurSendByte = 0;
		}
		nRetValue = this->SendOneDataPacket();
		// < 0 ֱ�����ϲ㷴��ɾ��֮...
		if( nRetValue < 0 ) {
			MsgLogINFO("[CPushThread::SendOneDataPacket] - Error");
			this->doErrPushNotify();
			return;
		}
		// == 0 ���ϼ���...
		if( nRetValue == 0 ) {
			Sleep(5);
			continue;
		}
		// > 0 ˵������,���Ϸ���...
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
	// ����rtmp server��������ֵ�Э��...
	return m_lpRtmpPush->Open(m_strRtmpUrl.c_str());
}
//
// ͨ������������������֡...
int CPushThread::PushFrame(FMS_FRAME & inFrame)
{
	OSMutexLocker theLock(&m_Mutex);
	// ����Ƶ���ݷ�����Ƶ�߳�...
	/*if( m_lpVideoThread != NULL && inFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO ) {
		m_lpVideoThread->PushFrame(inFrame.strData, inFrame.is_keyframe);
	}
	if( m_lpAudioThread != NULL && inFrame.typeFlvTag == FLV_TAG_TYPE_AUDIO ) {
		m_lpAudioThread->PushFrame(inFrame.strData);
	}*/
	if( m_lpPlayThread != NULL ) {
		m_lpPlayThread->PushFrame(inFrame);
	}
	// ����ʱ��ʱ�㸴λ�����¼�ʱ...
	m_dwTimeOutMS = ::GetTickCount();
	// ����¼����...
	this->StreamWriteRecord(inFrame);
	// ���ʱ����������1000���룬�����һ�ν�������...
	DWORD dwCurTimeMS = ::GetTickCount();
	if((dwCurTimeMS - m_dwRecvTimeMS) >= 1000 ) {
		float fSecond = (dwCurTimeMS - m_dwRecvTimeMS)/1000.0f;
		m_nRecvKbps = m_nCurRecvByte * 8 / 1024 / fSecond;
		m_dwRecvTimeMS = dwCurTimeMS;
		m_nCurRecvByte = 0;
	}
	// �ۼӽ������ݰ����ֽ��������뻺�����...
	m_nCurRecvByte += inFrame.strData.size();
	m_MapFrame.insert(pair<uint32_t, FMS_FRAME>(inFrame.dwSendTime, inFrame));

	/*// ������Ƶ����֡Ͷ�ݸ������߳� => ��ʱ20������֡...
	static bool g_b_delay = false;
	if( g_b_delay && m_lpPlayThread != NULL ) {
		m_lpPlayThread->PushFrame(inFrame);
	}
	if( !g_b_delay && m_lpPlayThread != NULL && m_MapFrame.size() > 100 ) {
		// ��ʼ����֮ǰ��������Ƶ������...
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

	// ������µ���Ƶ����֡�ǹؼ�֡�������ѻ���ģ�����µĹؼ�֡���Ա��ͼʹ��...
	if( inFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO ) {
		// �޸���Ƶ֡����ʼ�� => 0x00000001
		string strCurFrame = inFrame.strData;
		char * lpszSrc = (char*)strCurFrame.c_str();
		memset((void*)lpszSrc, 0, sizeof(DWORD));
		lpszSrc[3] = 0x01;
		// ������¹ؼ�֡�����֮ǰ�Ļ���...
		if( inFrame.is_keyframe ) {
			m_strSnapFrame.clear();
		}
		// ���µ�����֡׷�ӵ����棬ֱ����һ���ؼ�֡����...
		m_strSnapFrame.append(strCurFrame);
	}
	// ���ʱ���������˽�ͼ����������ͼ���� => ʹ��ffmpeg��̬��ͼ...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	int nSnapSec = theConfig.GetSnapVal() * 60;
	// �����ܷ��ͼ��־��ʱ��������վ��̨�趨...
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

	/*// ���������ͷ�������йؼ�֡�����Ͷ�֡����...
	if( this->IsCameraDevice() ) {
		return m_MapFrame.size();
	}
	// �������ת��ģʽ������û�д��ڷ���״̬...
	ASSERT( !this->IsCameraDevice() );*/

	// �ж��������Ƿ�����Ƶ�ؼ�֡��������Ƶ�ؼ�֡��ֱ�ӷ���...
	if( inFrame.typeFlvTag != FLV_TAG_TYPE_VIDEO || !inFrame.is_keyframe ) {
		return m_MapFrame.size();
	}
	// �������Ƶ�ؼ�֡���ۼӼ�����...
	ASSERT( inFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO && inFrame.is_keyframe );
	// �ۼӹؼ�֡������...
	++m_nKeyFrame;
	//TRACE("== [PushFrame] nKeyFrame = %d, FrameSize = %d, StreamSize = %d, FirstSendTime = %lu ==\n", m_nKeyFrame, m_MapFrame.size(), m_MapStream.size(), m_dwFirstSendTime);
	// ����Ѿ����ڷ�����ֱ�ӷ���...
	if( this->IsPublishing() ) {
		return m_MapFrame.size();
	}
	ASSERT( !this->IsPublishing() );
	// �������3���ؼ�֡��ɾ����һ���ؼ�֡����������...
	if( m_nKeyFrame >= 3 ) {
		this->dropToKeyFrame();
	}
	// ֱ�ӷ����ܵ�֡��...
	return m_MapFrame.size();
}
//
// ɾ����������е����ݣ�ֱ�������ؼ�֡Ϊֹ...
void CPushThread::dropToKeyFrame()
{
	// �����Ѿ�ɾ���ؼ�֡��־...
	BOOL bHasDelKeyFrame = false;
	KH_MapFrame::iterator itorItem = m_MapFrame.begin();
	while( itorItem != m_MapFrame.end() ) {
		int nSize = m_MapFrame.count(itorItem->first);
		// ����ͬʱ���������֡����ѭ��ɾ��...
		for(int i = 0; i < nSize; ++i) {
			FMS_FRAME & theFrame = itorItem->second;
			// �����������Ƶ�ؼ�֡...
			if( theFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO && theFrame.is_keyframe ) {
				// �Ѿ�ɾ����һ���ؼ�֡�������µĹؼ�֡�����÷���ʱ�����ֱ�ӷ���...
				if( bHasDelKeyFrame ) {
					m_dwFirstSendTime = theFrame.dwSendTime;
					//TRACE("== [dropToKeyFrame] nKeyFrame = %d, FrameSize = %d, StreamSize = %d, FirstSendTime = %lu ==\n", m_nKeyFrame, m_MapFrame.size(), m_MapStream.size(), m_dwFirstSendTime);
					return;
				}
				// ɾ������ؼ�֡�����ñ�־����ӡ��Ϣ...
				--m_nKeyFrame; bHasDelKeyFrame = true;
			}
			// ����ʹ������ɾ�������ǹؼ���...
			m_MapFrame.erase(itorItem++);
		}
	}
}
//
// ɾ������������е����ݣ�ֱ�������ؼ�֡Ϊֹ...
void CPushThread::dropSliceKeyFrame()
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
//
// ��ʼ׼���������ݰ�...
void CPushThread::BeginSendPacket()
{
	OSMutexLocker theLock(&m_Mutex);
	ASSERT( this->IsPublishing() );
	
	/*// ֻ������ת��ģʽ�²Ž��У���ʼʱ��ĵ���...
	// ��Ϊ����ͷģʽ�£����ǵ�����ͷ���� rtsp ������֡���ѵ�����...
	if( this->IsCameraDevice() )
		return;
	ASSERT( !this->IsCameraDevice() );*/

	// ���õ�һ֡�ķ���ʱ���...
	KH_MapFrame::iterator itorItem = m_MapFrame.begin();
	if( itorItem != m_MapFrame.end() ) {
		m_dwFirstSendTime = itorItem->first;
		CUtilTool::MsgLog(kTxtLogger, "== [BeginSendPacket] nKeyFrame = %d, FrameSize = %d, StreamSize = %d, FirstSendTime = %lu ==\r\n", m_nKeyFrame, m_MapFrame.size(), m_MapStream.size(), m_dwFirstSendTime);
	}
}
//
// ֹͣ�������ݰ�...
void CPushThread::EndSendPacket()
{
	// ɾ���ϴ�������Ҫ���Ᵽ��...
	OSMutexLocker theLock(&m_Mutex);
	if( m_lpRtmpPush != NULL ) {
		delete m_lpRtmpPush;
		m_lpRtmpPush = NULL;
	}
	
	/*// �������ת��ģʽ����Ҫ����������ݲ��뵽���淢�Ͷ���ǰ��...
	if( this->IsCameraDevice() )
		return;
	ASSERT( !this->IsCameraDevice() );*/

	// �����Ѿ��������ת������(�ⲿ�������Ѿ�����������������Ҫ�������һ���)...
	KH_MapFrame::iterator itorItem = m_MapStream.begin();
	while( itorItem != m_MapStream.end() ) {
		int nSize = m_MapStream.count(itorItem->first);
		// ����ͬʱ���������֡����ѭ��ɾ��...
		for(int i = 0; i < nSize; ++i) {
			FMS_FRAME & theFrame = itorItem->second;
			// ������Խ��������ȡ����Ϊ�ǰ���ʱ������У��������Զ�����...
			m_MapFrame.insert(pair<uint32_t, FMS_FRAME>(theFrame.dwSendTime, theFrame));
			// �ۼ����ӣ�������һ���ڵ�...
			++itorItem;
		}
	}
	// 2017.08.11 - by jackey => MapStream����ֻ������һ����Ƶ�ؼ�֡�����ݱ���ԭ���˻�����У���ˣ���Ҫ�Թؼ�֡�����ۼ�...
	// ���������������ݣ��ؼ�֡����������...
	if( m_MapStream.size() > 0 ) {
		++m_nKeyFrame;
		CUtilTool::MsgLog(kTxtLogger, "== [EndSendPacket] nKeyFrame = %d, FrameSize = %d, StreamSize = %d, FirstSendTime = %lu ==\r\n", m_nKeyFrame, m_MapFrame.size(), m_MapStream.size(), m_dwFirstSendTime);
	}
	// ��ջ������...
	m_MapStream.clear();
}
//
// ��ȡ�������� => -1 ��ʾ��ʱ...
int CPushThread::GetRecvKbps()
{
	// ���������ʱ������ -1���ȴ�ɾ��...
	if( this->IsFrameTimeout() ) {
		MsgLogGM(GM_Err_Timeout);
		return -1;
	}
	// ���ؽ�������...
	return m_nRecvKbps;
}
//
// ������ݰ��Ƿ��Ѿ���ʱ...
// ��ת��ģʽ�� => ���õĳ�ʱ����� StreamInitThread
// ����ͷ�豸�� => ���õĳ�ʱ����� DeviceInitThread
// ���ǻ�������ȡ�������ݵ�λ�ã���ʱ����������ģʽ��Ч...
BOOL CPushThread::IsFrameTimeout()
{
	//2018.01.01 - by jackey => ���ﲻ�ܼӻ��⣬�Ῠס����...
	//OSMutexLocker theLock(&m_Mutex);

	// �����ж������߳��Ƿ��Ѿ�������...
	if( this->IsDataFinished() )
		return true;
	// һֱû�������ݵ������ 3 ���ӣ����ж�Ϊ��ʱ...
	int nWaitMinute = 3;
	DWORD dwElapseSec = (::GetTickCount() - m_dwTimeOutMS) / 1000;
	return ((dwElapseSec >= nWaitMinute * 60) ? true : false);
}
//
// ����һ��֡���ݰ�...
int CPushThread::SendOneDataPacket()
{
	OSMutexLocker theLock(&m_Mutex);
	// �����Ѿ������ˣ�ֱ�ӷ���-1...
	if( this->IsDataFinished() ) {
		MsgLogINFO("== Push Data Finished ==");
		return -1;
	}
	// ������ݻ�û�н���������Ҫ��һ�����棬�Ա�����Ƶ�ܹ��Զ�����Ȼ���ٷ������ݰ�...
	// ������ǰ�趨��100�����ݰ���Ϊ�˽�����ʱ������Ϊ20�����ݰ�...
	if( m_MapFrame.size() < 20 )
		return 0;
	ASSERT( !this->IsDataFinished() && m_MapFrame.size() >= 20 );

	bool is_ok = false;
	ASSERT( m_MapFrame.size() > 0 );

	// ��ȡ��һ�����ݰ�(���ܰ����������)...
	KH_MapFrame::iterator itorItem = m_MapFrame.begin();
	int nSize = m_MapFrame.count(itorItem->first);
	// ����ͬʱ���������֡����ѭ������...
	//DWORD dwCurTickCount = ::GetTickCount();
	//TRACE("== Begin Send-Time: %lu ==\n", dwCurTickCount);
	for(int i = 0; i < nSize; ++i) {
		FMS_FRAME & theFrame = itorItem->second;
		
		/*// �������ת��ģʽ������Ҫ��ʱ������е�����Ӳ��ģʽ�µ�ʱ������Ǵ�0��ʼ�ģ��������...
		if( !this->IsCameraDevice() ) {
			theFrame.dwSendTime = theFrame.dwSendTime - m_dwFirstSendTime;
		}*/

		// �ǳ������ӣ���Ҫ��ʱ����������� => Ӳ��ģʽҲͳһ������ת��ģʽ...
		theFrame.dwSendTime = theFrame.dwSendTime - m_dwFirstSendTime;

		//TRACE("[%s] SendTime = %lu, Key = %d, Size = %d, MapSize = %d\n", ((theFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO) ? "Video" : "Audio"), theFrame.dwSendTime, theFrame.is_keyframe, theFrame.strData.size(), m_MapFrame.size());
		// ����ȡ��������֡������...
		switch( theFrame.typeFlvTag )
		{
		case FLV_TAG_TYPE_AUDIO: is_ok = this->SendAudioDataPacket(theFrame); break;
		case FLV_TAG_TYPE_VIDEO: is_ok = this->SendVideoDataPacket(theFrame); break;
		}
		// �ۼӷ����ֽ���...
		m_nCurSendByte += theFrame.strData.size();
		// ����ؼ�֡ => �������ǰ��������� => �ؼ�֡����������...
		// ��ʱ�����ԭ��ȥ...
		theFrame.dwSendTime = theFrame.dwSendTime + m_dwFirstSendTime;
		// �����µĹؼ�֡�������ǰ�Ļ��棬���ٹؼ�֡������...
		if( theFrame.typeFlvTag == FLV_TAG_TYPE_VIDEO && theFrame.is_keyframe ) {
			--m_nKeyFrame; m_MapStream.clear();
			//TRACE("== [SendOneDataPacket] nKeyFrame = %d, FrameSize = %d, FirstSendTime = %lu, CurSendTime = %lu ==\n", m_nKeyFrame, m_MapFrame.size(), m_dwFirstSendTime, theFrame.dwSendTime);
		}
		// ������ת�浽ר�ŵĻ�����е��� => ��һ֡�϶��ǹؼ�֡...
		m_MapStream.insert(pair<uint32_t, FMS_FRAME>(theFrame.dwSendTime, theFrame));
		// �Ӷ������Ƴ�һ����ͬʱ������ݰ� => ����ʹ�õ�������...
		m_MapFrame.erase(itorItem++);
	}
	//TRACE("== End Spend-Time: %lu ==\n", ::GetTickCount() - dwCurTickCount);
	// ����ʧ�ܣ����ش���...
	return (is_ok ? 1 : -1);
}
//
// ������Ƶ���ݰ�...
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
// ������Ƶ���ݰ�...
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
// ������Ƶ����ͷ...
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
	// û����Ƶ����...
	if( strAVC.size() <= 0 )
		return true;
	ASSERT( strAVC.size() > 0 );
	MsgLogINFO("== RtmpPush => Send Video SequenceHeaderPacket ==");
	return m_lpRtmpPush->Send(strAVC.c_str(), strAVC.size(), FLV_TAG_TYPE_VIDEO, 0);
}

//
// ������Ƶ����ͷ...
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
	// û����Ƶ����...
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

	// ������Ƶ��Ⱥ͸߶�����...
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
	// ��ȡ�Ƿ�������Ƶ�ı�־״̬��Ϣ...
	bHasVideo = ((strAVC.size() > 0) ? true : false);
	bHasAudio = ((strAAC.size() > 0) ? true : false);
	// ���ÿ������...
	if( nVideoWidth > 0 ) {
	    pbuf = AmfStringToBytes(pbuf, "width");
		pbuf = AmfDoubleToBytes(pbuf, nVideoWidth);
	}
	// ������Ƶ�߶�����...
	if( nVideoHeight > 0 ) {
	    pbuf = AmfStringToBytes(pbuf, "height");
		pbuf = AmfDoubleToBytes(pbuf, nVideoHeight);
	}
	// ������Ƶ��־...
    pbuf = AmfStringToBytes(pbuf, "hasVideo");
	pbuf = AmfBoolToBytes(pbuf, bHasVideo);
	// �������ӱ�־...
    pbuf = AmfStringToBytes(pbuf, "hasAudio");
	pbuf = AmfBoolToBytes(pbuf, bHasAudio);
	// �������Ƶ����������Ƶ�����Ϣ...
	if( bHasVideo ) {
		// ͳһ����Ĭ�ϵ���Ƶ֡������...
		pbuf = AmfStringToBytes(pbuf, "framerate");
		pbuf = AmfDoubleToBytes(pbuf, 25);
		// ������Ƶ������...
		pbuf = AmfStringToBytes(pbuf, "videocodecid");
		pbuf = UI08ToBytes(pbuf, AMF_DATA_TYPE_STRING);
		pbuf = AmfStringToBytes(pbuf, "avc1");
	}
	// �������Ƶ����������Ƶ�����Ϣ...
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