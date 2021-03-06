
#include "StdAfx.h"
#include "Camera.h"
#include "UtilTool.h"
#include "VideoWnd.h"
#include "MidView.h"
#include "XmlConfig.h"
#include "RecThread.h"
#include "PushThread.h"
#include "HaoYiView.h"
#include "md5.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CCamera::CCamera(CVideoWnd * lpWndParent)
  : m_lpVideoWnd(lpWndParent)
  , m_nStreamProp(kStreamDevice)
  , m_nCameraType(kCameraHK)
  , m_nRtspPort(554)
  , m_nRecCourseID(-1)
  , m_hWndRight(NULL)
  , m_lpPushThread(NULL)
  , m_lpDeviceMainRec(NULL)
  , m_bIsTwiceMode(false)
  , m_bStreamLogin(false)
  , m_bIsExiting(false)
  , m_HKLoginIng(false)
  , m_dwHKErrCode(0)
  , m_HKLoginID(-1)
  , m_HKPlayID(-1)
  , m_nDBCameraID(0)
{
	ASSERT( m_lpVideoWnd != NULL );
	memset(&m_HKDeviceInfo, 0, sizeof(m_HKDeviceInfo));
}

CCamera::~CCamera(void)
{
	// 等待异步登录退出...
	this->WaitForExit();
	// 释放所有的资源...
	this->ClearResource();
}
//
// 等待异步登录退出 => 使用互斥不起作用...
void CCamera::WaitForExit()
{
	m_bIsExiting = true;
	while( m_HKLoginIng ) {
		::Sleep(5);
	}
	ASSERT( !m_HKLoginIng );
}
//
// DVR是否已经成功登陆...
BOOL CCamera::IsLogin()
{
	// 如果是流转发模式 => 返回流转发登录标志...
	if( m_nStreamProp != kStreamDevice ) {
		return m_bStreamLogin;
	}
	// 如果是摄像头设备模式...
	ASSERT( m_nStreamProp == kStreamDevice );
	if( m_nCameraType == kCameraHK ) {
		if( m_HKLoginID >= 0 ) {
			return true;
		} else {
			// 正在异步登录中，已算登录成功...
			ASSERT( m_HKLoginID < 0 );
			return (m_HKLoginIng ? true : false);
		}
	} else if( m_nCameraType == kCameraDH ) {
		return false;
	}
	return false;
}
//
// DVR是否正在实时预览...
BOOL CCamera::IsPlaying()
{
	if( m_lpPushThread == NULL )
		return false;
	ASSERT( m_lpPushThread != NULL );
	return m_lpPushThread->IsStreamPlaying();
}
//
// 通道是否正处在发布中...
BOOL CCamera::IsPublishing()
{
	if( m_lpPushThread == NULL )
		return false;
	ASSERT( m_lpPushThread != NULL );
	return m_lpPushThread->IsPublishing();
}
//
// 返回是否正在录像状态标志...
BOOL CCamera::IsRecording()
{
	// 如果是摄像头设备，并且是双流模式...
	if( this->IsCameraDevice() && this->m_bIsTwiceMode ) {
		// 如果录像有效，但已经结束了， 删除录像对象...
		if( m_lpDeviceMainRec != NULL && m_lpDeviceMainRec->IsRecFinished() ) {
			// 停止并删除主流录像对象...
			this->DeviceStopMainRec();
			// 将正在运行的录像记录编号复位...
			m_nRecCourseID = -1;
			return false;
		}
		// 其它状态，通过验证录像对象返回结果...
		return ((m_lpDeviceMainRec != NULL) ? true : false);
	}
	// 如果是其它模式，调用推流线程...
	if( m_lpPushThread == NULL )
		return false;
	return m_lpPushThread->IsRecording();
}
//
// 判断是否显示设备特殊状态...
BOOL CCamera::IsDeviceStatus()
{
	// 不是设备通道，直接返回失败...
	if( !this->IsCameraDevice() )
		return false;
	// 如果通道的上传对象为空，返回需要显示特殊状态...
	if( m_lpPushThread == NULL )
		return true;
	ASSERT( m_lpPushThread != NULL );
	// 最后，如果是正在预览画面，返回需要特殊状态...
	return ((m_HKPlayID >= 0) ? true : false);
}
//
// 释放建立资源...
void CCamera::ClearResource()
{
	// 释放正在录像资源，实时预览资源...
	if( m_HKPlayID >= 0 ) {
		NET_DVR_StopRealPlay(m_HKPlayID);
		m_HKPlayID = -1;
	}
	// 释放登录资源...
	if( m_HKLoginID >= 0 ) {
		NET_DVR_Logout_V30(m_HKLoginID);
		m_HKLoginID = -1;
		memset(&m_HKDeviceInfo, 0, sizeof(m_HKDeviceInfo));
	}
	// 释放摄像头双流模式录像对象...
	this->DeviceStopMainRec();
	// 释放自定义直播上传对象...
	this->doDeletePushThread();
	// 设置录像任务为无效...
	m_nRecCourseID = -1;
	// 设置异步登录标志...
	m_bStreamLogin = false;
}
//
// 执行云台操作...
DWORD CCamera::doDevicePTZCmd(DWORD dwPTZCmd, BOOL bStop)
{
	// 必须在预览模式下使用...
	DWORD dwErr = GM_NoErr;
	if( m_HKPlayID < 0 )
		return GM_NoErr;
	ASSERT( m_HKPlayID >= 0 );
	// 这里不用Speed方式，在 PAN_AUTO 下不起作用...
	//bReturn = NET_DVR_PTZControlWithSpeed(m_HKPlayID, dwPTZCmd, bStop, nSpeed);
	//bReturn = NET_DVR_PTZControl_EX(m_HKPlayID, dwPTZCmd, bStop);
	if( !NET_DVR_PTZControl(m_HKPlayID, dwPTZCmd, bStop) ) {
		dwErr = NET_DVR_GetLastError();
		MsgLogGM(dwErr);
		return dwErr;
	}
	return GM_NoErr;
}
//
// 通知网站通道状态...
void CCamera::doWebStatCamera(int nStatus, int nErrCode/* = 0*/, LPCTSTR lpszErrMsg/* = NULL*/)
{
	// 判断 VideoWnd 是否为空...
	if( m_lpVideoWnd == NULL )
		return;
	ASSERT( m_lpVideoWnd != NULL );
	// 获取 MidView 窗口对象...
	CMidView * lpMidView = (CMidView*)m_lpVideoWnd->GetRealParent();
	if( lpMidView == NULL )
		return;
	ASSERT( lpMidView != NULL );
	// 通过本地编号获取数据库编号...
	int nDBCameraID = this->GetDBCameraID();
	lpMidView->doWebStatCamera(nDBCameraID, nStatus, nErrCode, lpszErrMsg);
}
//
// 设置流数据转发状态...
void CCamera::doStreamStatus(LPCTSTR lpszStatus)
{
	CString strStatus = lpszStatus;
	CRenderWnd * lpRenderWnd = m_lpVideoWnd->GetRenderWnd();
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	
	m_strLogStatus.Format("%s - %s", strStatus, theConfig.GetDBCameraTitle(m_nDBCameraID));

	lpRenderWnd->SetStreamStatus(strStatus);
	lpRenderWnd->SetRenderText(m_strLogStatus);
}
//
// 执行DVR注销操作...
void CCamera::doDeviceLogout()
{
	// 释放建立的资源...
	this->ClearResource();
	// 复位登录状态信息...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	m_strLogStatus.Format("未登录 - %s", theConfig.GetDBCameraTitle(m_nDBCameraID));
	// 设置渲染窗口状态...
	ASSERT( m_lpVideoWnd != NULL );
	CString strStatus = "未连接...";
	CRenderWnd * lpRenderWnd = m_lpVideoWnd->GetRenderWnd();
	lpRenderWnd->SetRenderState(CRenderWnd::ST_WAIT);
	lpRenderWnd->SetRenderText(m_strLogStatus);
	lpRenderWnd->SetStreamStatus(strStatus);
	// 通知网站将通道设置为等待状态 => 硬件设备 和 流转发 都会走这条路...
	this->doWebStatCamera(kCameraWait);
}
//
// 登录回调函数接口...
void CALLBACK CCamera::DeviceLoginResult(LONG lUserID, DWORD dwResult, LPNET_DVR_DEVICEINFO_V30 lpDeviceInfo, void * pUser)
{
	CCamera * lpCamera = (CCamera*)pUser;
	lpCamera->onDeviceLoginAsync(lUserID, dwResult, lpDeviceInfo);
}
//
// 实际处理登录回调的函数...
void CCamera::onDeviceLoginAsync(LONG lUserID, DWORD dwResult, LPNET_DVR_DEVICEINFO_V30 lpDeviceInfo)
{
	// 正在登录状态复位...
	m_HKLoginIng = false;
	// 处在退出状态中，直接返回...
	if( m_bIsExiting ) {
		TRACE("CCamera::onDeviceLoginAsync => Exit\n");
		return;
	}
	// 登录失败的处理过程，需要通知上层窗口...
	if( dwResult <= 0 || lUserID < 0 ) {
		m_dwHKErrCode = NET_DVR_GetLastError();
		m_strLogStatus.Format("登录失败，错误：%s", NET_DVR_GetErrorMsg(&m_dwHKErrCode));
		m_lpVideoWnd->GetRenderWnd()->SetRenderText(m_strLogStatus);
		::PostMessage(m_hWndRight, WM_DEVICE_LOGIN_RESULT, m_nDBCameraID, false);
		// 通知网站将通道设置为等待状态 => 只有硬件设备会走这条路...
		this->doWebStatCamera(kCameraWait, m_dwHKErrCode, NET_DVR_GetErrorMsg(&m_dwHKErrCode));
		// 释放该通道上的资源数据...
		this->ClearResource();
		MsgLogGM(m_dwHKErrCode);
		return;
	}
	// 登录成功之后，保存数据...
	m_dwHKErrCode = NET_DVR_NOERROR;
	this->m_HKLoginID = lUserID;
	memcpy(&m_HKDeviceInfo, lpDeviceInfo, sizeof(NET_DVR_DEVICEINFO_V30));
	// 记录登陆状态，返回正确结果...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	m_strLogStatus.Format("已登录 - %s", theConfig.GetDBCameraTitle(m_nDBCameraID));
	m_lpVideoWnd->GetRenderWnd()->SetRenderText(m_strLogStatus);
	// 更新主窗口状态，通知主窗口，处理异步登录结果...
	ASSERT( m_hWndRight != NULL && m_nDBCameraID > 0 );
	::PostMessage(m_hWndRight, WM_DEVICE_LOGIN_RESULT, m_nDBCameraID, true);
	// 这里先不通知网站通道状态，因为，后面还有操作，也可能发生错误...
}
//
// 执行DVR异步登录成功的消息事件...
DWORD CCamera::onDeviceLoginSuccess()
{
	ASSERT( m_nDBCameraID > 0 );
	LONG dwErr = GM_NoErr;
	GM_MapData theMapWeb;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetCamera(m_nDBCameraID, theMapWeb);
	ASSERT( m_HKPlayID < 0 && m_HKLoginID >= 0 );
	do {
		// DVR的第一个通道，一个DVR可能有多个通道...
		LONG  nDvrStartChan = m_HKDeviceInfo.byStartChan;
		DWORD dwReturn = 0;
		// 获取DVR的通道资源...
		//NET_DVR_IPPARACFG_V40 dvrIPCfg = {0};
		//BOOL bIPRet = NET_DVR_GetDVRConfig(m_HKLoginID, NET_DVR_GET_IPPARACFG_V40, nDvrStartChan, &dvrIPCfg, sizeof(dvrIPCfg), &dwReturn);
		// 获取云台解码信息...
		//NET_DVR_DECODERCFG_V30 dvrDecoderCfg = {0};
		//BOOL bDecRet = NET_DVR_GetDVRConfig(m_HKLoginID, NET_DVR_GET_DECODERCFG_V30, nDvrStartChan, &dvrDecoderCfg, sizeof(dvrDecoderCfg), &dwReturn);
		//(目前通过WEB配置DVR) 获取设备所有编码能力 => 需要解析xml，让程序自动选择或用户手动选择，写入设备当中...
		//TCHAR szEncAbility[MAX_PATH*20] = {0};
		//if( !NET_DVR_GetDeviceAbility(m_HKLoginID, DEVICE_ENCODE_ALL_ABILITY, NULL, 0, szEncAbility, MAX_PATH*20) ) {
		//	dwErr = NET_DVR_GetLastError();
		//	MsgLogGM(dwErr);
		//	break;
		//}
		//TCHAR   szEncAbility[MAX_PATH*40] = {0};
		//TCHAR * lpInput = "<AudioVideoCompressInfo><AudioChannelNumber>1</AudioChannelNumber><VideoChannelNumber>1</VideoChannelNumber></AudioVideoCompressInfo>";
		//if( !NET_DVR_GetDeviceAbility(m_HKLoginID, DEVICE_ENCODE_ALL_ABILITY_V20, lpInput, strlen(lpInput), szEncAbility, MAX_PATH*40) ) {
		//	dwErr = NET_DVR_GetLastError();
		//	MsgLogGM(dwErr);
		//	break;
		//}
		// 获取IPC的rtsp端口号...
		NET_DVR_RTSPCFG dvrRtsp = {0};
		if( !NET_DVR_GetRtspConfig(m_HKLoginID, 0, &dvrRtsp, sizeof(NET_DVR_RTSPCFG)) ) {
			dwErr = NET_DVR_GetLastError();
			MsgLogGM(dwErr);
			break;
		}
		// 调用正确，保存rtsp端口...
		m_nRtspPort = dvrRtsp.wPort;
		// 获取更为多码流的配置 => NET_DVR_GetDeviceConfig(NET_DVR_GET_MULTI_STREAM_COMPRESSIONCFG) => NET_DVR_MULTI_STREAM_COMPRESSIONCFG_COND NET_DVR_MULTI_STREAM_COMPRESSIONCFG
		// 获取压缩配置参数信息 => 包含了 主码流 和 子码流 ...
		NET_DVR_COMPRESSIONCFG_V30 dvrCompressCfg = {0};
		if( !NET_DVR_GetDVRConfig(m_HKLoginID, NET_DVR_GET_COMPRESSCFG_V30, nDvrStartChan, &dvrCompressCfg, sizeof(dvrCompressCfg), &dwReturn) ) {
			dwErr = NET_DVR_GetLastError();
			MsgLogGM(dwErr);
			break;
		}
		// 判断主码流或子码流的视频类型是否正确 => 复合流
		if( dvrCompressCfg.struNormHighRecordPara.byStreamType != 1 ||
			dvrCompressCfg.struNetPara.byStreamType != 1 ) {
			dwErr = GM_DVR_VType_Err;
			MsgLogGM(dwErr);
			break;
		}
		// 判断主码流或子码流视频编码类型是否正确 => H264
		if( dvrCompressCfg.struNormHighRecordPara.byVideoEncType != NET_DVR_ENCODER_H264 ||
			dvrCompressCfg.struNetPara.byVideoEncType != NET_DVR_ENCODER_H264 ) {
			dwErr = GM_DVR_VEnc_Err;
			MsgLogGM(dwErr);
			break;
		}
		// 判断主码流或子码流音频编码类型是否正确 => AAC
		if( dvrCompressCfg.struNormHighRecordPara.byAudioEncType != AUDIOTALKTYPE_AAC ||
			dvrCompressCfg.struNetPara.byAudioEncType != AUDIOTALKTYPE_AAC ) {
			dwErr = GM_DVR_AEnc_Err;
			MsgLogGM(dwErr);
			break;
		}
		// 调整一些参数 => 根据设备的编码能力来设置，而不是随意设置的...
		// 从配置文件中读取并设置主码流大小和子码流大小 => 自定义码流...
		dvrCompressCfg.struNormHighRecordPara.dwVideoBitrate = theConfig.GetMainKbps() * 1024;
		dvrCompressCfg.struNetPara.dwVideoBitrate = theConfig.GetSubKbps() * 1024;
		dvrCompressCfg.struNormHighRecordPara.dwVideoBitrate |= 0x80000000;
		dvrCompressCfg.struNetPara.dwVideoBitrate |= 0x80000000;
		// 设置 主码流 和 子码流 的配置参数信息...
		if( !NET_DVR_SetDVRConfig(m_HKLoginID, NET_DVR_SET_COMPRESSCFG_V30, nDvrStartChan, &dvrCompressCfg, sizeof(dvrCompressCfg)) ) {
			dwErr = NET_DVR_GetLastError();
			MsgLogGM(dwErr);
			break;
		}
		// 获取DVR设备的前端参数...
		NET_DVR_CAMERAPARAMCFG dvrCCDParam = {0};
		if( !NET_DVR_GetDVRConfig(m_HKLoginID, NET_DVR_GET_CCDPARAMCFG, nDvrStartChan, &dvrCCDParam, sizeof(dvrCCDParam), &dwReturn) ) {
			dwErr = NET_DVR_GetLastError();
			MsgLogGM(dwErr);
			break;
		}
		// 从通道配置文件中获取是否开启镜像...
		string & strMirror = theMapWeb["device_mirror"];
		BOOL bOpenMirror = ((strMirror.size() > 0) ? atoi(strMirror.c_str()) : false);
		// 对镜像模式进行处理 => 镜像：0 关闭;1 左右;2 上下;3 中间
		dvrCCDParam.byMirror = (bOpenMirror ? 3 : 0);
		if( !NET_DVR_SetDVRConfig(m_HKLoginID, NET_DVR_SET_CCDPARAMCFG, nDvrStartChan, &dvrCCDParam, sizeof(dvrCCDParam)) ) {
			dwErr = NET_DVR_GetLastError(); // 注意这个错误号：NET_DVR_NETWORK_ERRORDATA
			MsgLogGM(dwErr);
			break;
		}
		// 获取图像参数 => OSD | 坐标 | 日期 | 星期 | 字体 | 属性
		NET_DVR_PICCFG_V40 dvrPicV40 = {0};
		if( !NET_DVR_GetDVRConfig(m_HKLoginID, NET_DVR_GET_PICCFG_V40, nDvrStartChan, &dvrPicV40, sizeof(dvrPicV40), &dwReturn) ) {
			dwErr = NET_DVR_GetLastError();
			MsgLogGM(dwErr);
			break;
		}
		// 从通道配置文件中获取是否开启OSD...
		string & strOSD = theMapWeb["device_osd"];
		BOOL bOpenOSD = ((strOSD.size() > 0) ? atoi(strOSD.c_str()) : true);
		// 设置图像格式 => OSD | 坐标 | 日期 | 星期 | 字体 | 属性
		//strcpy((char*)dvrPicV40.sChanName, "Camera"); // 通道名称...
		//dvrPicV40.dwVideoFormat = 2; // 视频制式：0- 不支持，1- NTSC，2- PAL 
		//dvrPicV40.dwShowChanName = 0; // 预览的图象上是否显示通道名称：0-不显示，1-显示（区域大小704*576） 
		//dvrPicV40.wShowNameTopLeftX = 200; // 通道名称显示位置的x坐标
		//dvrPicV40.wShowNameTopLeftY = 100; // 通道名称显示位置的y坐标
		//dvrPicV40.dwEnableHide = 1; // 是否启动隐私遮蔽：0-否，1-是
		dvrPicV40.dwShowOsd = bOpenOSD; // 预览的图象上是否显示OSD：0-不显示，1-显示（区域大小704*576）
		dvrPicV40.wOSDTopLeftX = 300; // OSD的x坐标
		dvrPicV40.wOSDTopLeftY = 20; // OSD的y坐标
		dvrPicV40.byOSDType = 2; // OSD类型(年月日格式) 0－XXXX-XX-XX 年月日; 1－XX-XX-XXXX 月日年; 2－XXXX年XX月XX日; 3－XX月XX日XXXX年; 4－XX-XX-XXXX 日月年; 5－XX日XX月XXXX年; 6－xx/xx/xxxx 月/日/年; 7－xxxx/xx/xx 年/月/日; 8－xx/xx/xxxx 日/月/年
		dvrPicV40.byDispWeek = 0; // 是否显示星期：0-不显示，1-显示
		dvrPicV40.byOSDAttrib = 2; // OSD属性（透明/闪烁）：1－透明，闪烁；2－透明，不闪烁；3－闪烁，不透明；4－不透明，不闪烁
		dvrPicV40.byHourOSDType = 0; // 小时制：0表示24小时制，1表示12小时制或am/pm 
		dvrPicV40.byFontSize = 0xFF; // 字体大小：0- 16*16(中)/8*16(英)，1- 32*32(中)/16*32(英)，2- 64*64(中)/32*64(英)，3- 48*48(中)/24*48(英)，4- 24*24(中)/12*24(英)，5- 96*96(中)/48*96(英)，0xff- 自适应
		dvrPicV40.byOSDColorType = 0; // OSD颜色模式：0- 默认（黑白），1-自定义(颜色见struOsdColor)
		dvrPicV40.struOsdColor.byRed = 255;
		dvrPicV40.struOsdColor.byGreen = 0;
		dvrPicV40.struOsdColor.byBlue = 0;
		dvrPicV40.byAlignment = 0; // 对齐方式：0- 自适应，1- 右对齐，2- 左对齐
		if( !NET_DVR_SetDVRConfig(m_HKLoginID, NET_DVR_SET_PICCFG_V40, nDvrStartChan, &dvrPicV40, sizeof(dvrPicV40)) ) {
			dwErr = NET_DVR_GetLastError();
			MsgLogGM(dwErr);
			break;
		}
		// 对DVR设备进行校时操作 => 设置成跟电脑时间一致...
		NET_DVR_TIME dvrTime = {0};
		CTime curTime = CTime::GetCurrentTime();
		dvrTime.dwYear = curTime.GetYear();
		dvrTime.dwMonth = curTime.GetMonth();
		dvrTime.dwDay = curTime.GetDay();
		dvrTime.dwHour = curTime.GetHour();
		dvrTime.dwMinute = curTime.GetMinute();
		dvrTime.dwSecond = curTime.GetSecond();
		if( !NET_DVR_SetDVRConfig(m_HKLoginID, NET_DVR_SET_TIMECFG, 0, &dvrTime, sizeof(dvrTime)) ) {
			dwErr = NET_DVR_GetLastError();
			MsgLogGM(dwErr);
			break;
		}
		// 2017.12.13 - by jackey => 不用处理，作用不大...
		// 设置设备异常消息回调接口函数...
		/*if( !NET_DVR_SetExceptionCallBack_V30(0, NULL, CCamera::DeviceException, this) ) {
			dwErr = NET_DVR_GetLastError();
			MsgLogGM(dwErr);
			break;
		}*/
		// 从通道配置中获取是否开启本地预览...
		string & strPreview = theMapWeb["device_show"];
		BOOL bPreview = ((strPreview.size() > 0) ? atoi(strPreview.c_str()) : true);
		// 配置了可以预览画面才显示...
		if( bPreview ) {
			// 准备显示预览画面需要的参数...
			ASSERT( m_lpVideoWnd != NULL );
			CRenderWnd * lpRenderWnd = m_lpVideoWnd->GetRenderWnd();
			NET_DVR_CLIENTINFO dvrClientInfo = {0};
			dvrClientInfo.hPlayWnd     = lpRenderWnd->m_hWnd;
			dvrClientInfo.lChannel     = nDvrStartChan;
			dvrClientInfo.lLinkMode    = 0;
			dvrClientInfo.sMultiCastIP = NULL;
			// 调用实时预览接口...
			m_HKPlayID = NET_DVR_RealPlay_V30(m_HKLoginID, &dvrClientInfo, NULL, NULL, TRUE);
			if( m_HKPlayID < 0 ) {
				dwErr = NET_DVR_GetLastError();
				MsgLogGM(dwErr);
				break;
			}
			// 设置渲染窗口状态...
			lpRenderWnd->SetRenderState(CRenderWnd::ST_RENDER);
		}
	}while(false);
	// 如果调用失败，清除所有资源...
	if( dwErr != GM_NoErr ) {
		m_strLogStatus.Format("登录失败，错误：%s", NET_DVR_GetErrorMsg(&dwErr));
		m_lpVideoWnd->GetRenderWnd()->SetRenderText(m_strLogStatus);
		this->ClearResource();
		// 通知网站将通道设置为等待状态 => 只有硬件设备走这条路...
		this->doWebStatCamera(kCameraWait, dwErr, NET_DVR_GetErrorMsg(&dwErr));
		return dwErr;
	}
	// 记录登陆状态，返回正确结果...
	m_strLogStatus.Format("已登录 - %s", theConfig.GetDBCameraTitle(m_nDBCameraID));
	m_lpVideoWnd->GetRenderWnd()->SetRenderText(m_strLogStatus);
	// 通知网站将通道设置为运行状态 => 只有硬件设备走这条路...
	this->doWebStatCamera(kCameraRun);
	/////////////////////////////////////////////////////////////////////////////////////
	// 2017.08.15 - by jackey => 启动一路拉rtsp主码流线程...
	// 启动rtsp拉流线程，只拉流不录像，录像由录像命令触发...
	/////////////////////////////////////////////////////////////////////////////////////
	// 准备rtsp链接地址 => 主码流 => rtsp://admin:12345@192.168.1.65/Streaming/Channels/101
	// 准备rtsp链接地址 => 子码流 => rtsp://admin:12345@192.168.1.65/Streaming/Channels/102
	m_strRtspMainUrl.Format("rtsp://%s:%s@%s:%d/Streaming/Channels/101", m_strLoginUser, m_strLoginPass, theMapWeb["device_ip"].c_str(), m_nRtspPort);
	m_strRtspSubUrl.Format("rtsp://%s:%s@%s:%d/Streaming/Channels/102", m_strLoginUser, m_strLoginPass, theMapWeb["device_ip"].c_str(), m_nRtspPort);
	// 获取当前通道启动时刻点的双流模式标志，并保存起来，不要动态获取，否则，录像的开始和停止可能会造成标志不一样....
	string & strTwice = theMapWeb["device_twice"];
	m_bIsTwiceMode = ((strTwice.size() > 0) ? atoi(strTwice.c_str()) : false);
	// 若开启双流模式，使用子码流直播、主码流录像；不开启使用主码流直播、主码流录像...
	string strStreamUrl, strStreamMP4;
	strStreamUrl.assign(m_bIsTwiceMode ? m_strRtspSubUrl : m_strRtspMainUrl);
	// 创建流转发、推流线程对象，并立即启动...
	m_lpPushThread = new CPushThread(m_lpVideoWnd->m_hWnd, this);
	m_lpPushThread->StreamInitThread(false, strStreamUrl, strStreamMP4);
	return GM_NoErr;
}
//
// 设备异常回调函数接口...
void CALLBACK CCamera::DeviceException(DWORD dwType, LONG lUserID, LONG lHandle, void * pUser)
{
	CCamera * lpCamera = (CCamera*)pUser;
	lpCamera->onDeviceException(dwType, lUserID, lHandle);
}
//
// 处理设备异常的实际函数...
void CCamera::onDeviceException(DWORD dwType, LONG lUserID, LONG lHandle)
{
	TRACE("=== Device Exception 0x%x ===\n", dwType);
}
//
// 处理录像前的截图事件...
/*DWORD CCamera::doDeviceSnapJPG(CString & inJpgName)
{
	// 为了保险起见，一定要用播放状态判断，登录状态由于是异步操作，会不精确...
	if( !this->IsPlaying() )
		return GM_NoErr;
	ASSERT( m_HKLoginID >= 0 );
	ASSERT( inJpgName.GetLength() > 0 );
	// DVR的第一个通道，一个DVR可能有多个通道...
	DWORD dwReturn = 0;
	DWORD dwErr = GM_NoErr;
	LONG  nDvrStartChan = m_HKDeviceInfo.byStartChan;
	// 组建jpg结构 => 当图像压缩分辨率为VGA时，支持0=CIF, 1=QCIF, 2=D1抓图, 当分辨率为3=UXGA(1600x1200), 4=SVGA(800x600), 5=HD720p(1280x720),6=VGA,7=XVGA, 8=HD900p 仅支持当前分辨率的抓图
	// 默认设置成 D1 模式，采用最高质量...
	NET_DVR_JPEGPARA dvrJpg = {0};
	dvrJpg.wPicSize = 2;		// D1
	dvrJpg.wPicQuality = 0;		// 0-最好 1-较好 2-一般
	// 直接用硬件抓图，不依赖是否开启了预览...
	if( !NET_DVR_CaptureJPEGPicture(m_HKLoginID, nDvrStartChan, &dvrJpg, (LPSTR)inJpgName.GetString()) ) {
		dwErr = NET_DVR_GetLastError();
		MsgLogGM(dwErr);
		return dwErr;
	}
	return GM_NoErr;
}*/
//
// 开启设备主码流录像...
BOOL CCamera::DeviceStartMainRec()
{
	// 首先，关闭主码流录像对象...
	this->DeviceStopMainRec();
	ASSERT( m_lpDeviceMainRec == NULL );
	// 新建一个主码流录像对象...
	m_lpDeviceMainRec = new CRtspRecThread(this);
	return m_lpDeviceMainRec->InitThread(m_strRtspMainUrl);
}
//
// 关闭设备主码流录像...
BOOL CCamera::DeviceStopMainRec()
{
	if( m_lpDeviceMainRec != NULL ) {
		delete m_lpDeviceMainRec;
		m_lpDeviceMainRec = NULL;
	}
	return true;
}
//
// 启动任务录像 => 同一时间只能有一个录像...
void CCamera::doRecStartCourse(int nCourseID)
{
	// 如果当前通道还没有播放，直接返回...
	if( !this->IsPlaying() )
		return;
	// 如果正在运行的记录就是当前记录，直接返回...
	if( m_nRecCourseID == nCourseID )
		return;
	ASSERT( m_nRecCourseID != nCourseID );
	// 先保存当前课程编号，推流对象会用到...
	m_nRecCourseID = nCourseID;
	// 如果是摄像头设备，并且是双流模式，使用单独的主码流录像...
	if( this->IsCameraDevice() && this->m_bIsTwiceMode ) {
		this->DeviceStartMainRec();
		return;
	}
	////////////////////////////////////////////////////////////
	// 如果是单流模式或转发模式 => 直接用转发线程录像...
	////////////////////////////////////////////////////////////
	// 推流线程一定有效 => IsPlaying() 可以保证...
	ASSERT( m_lpPushThread != NULL );
	// 调用推流线程的录像接口...
	m_lpPushThread->StreamBeginRecord();
	return;
}
//
// 停止任务录像 => 同一时间只能有一个录像...
void CCamera::doRecStopCourse(int nCourseID)
{
	// 如果当前通道还没有登录，直接返回...
	if( !this->IsPlaying() )
		return;
	// 如果正在运行的记录不是当前记录，直接返回...
	if( m_nRecCourseID != nCourseID )
		return;
	ASSERT( m_nRecCourseID == nCourseID );
	// 直接调用接口停止录像...
	if( m_lpPushThread != NULL ) {
		m_lpPushThread->StreamEndRecord();
	}
	// 直接调用接口，停止主码流录像...
	this->DeviceStopMainRec();
	// 将正在运行的记录编号复位...
	m_nRecCourseID = -1;
}
//
// 执行DVR登录操作...
DWORD CCamera::doDeviceLogin(HWND hWndNotify, LPCTSTR lpIPAddr, int nCmdPort, LPCTSTR lpUser, LPCTSTR lpPass)
{
	// 将摄像机的错误标志复位...
	m_dwHKErrCode = NET_DVR_NOERROR;
	// 登录之前，先释放资源，保存通知窗口...
	DWORD dwErr = GM_NoErr;
	ASSERT( hWndNotify != NULL );
	m_hWndRight = hWndNotify;
	this->ClearResource();
	// 开始根据不同的DVR调用不同的SDK...
	if( m_nCameraType == kCameraHK ) {
		do {
			// 异步方式登录DVR设备...
			NET_DVR_DEVICEINFO_V40  dvrDevV40 = {0};
			NET_DVR_USER_LOGIN_INFO dvrLoginInfo = {0};
			dvrLoginInfo.cbLoginResult = CCamera::DeviceLoginResult;
			strcpy(dvrLoginInfo.sDeviceAddress, lpIPAddr);
			strcpy(dvrLoginInfo.sUserName, lpUser);
			strcpy(dvrLoginInfo.sPassword, lpPass);
			dvrLoginInfo.bUseAsynLogin = 1;
			dvrLoginInfo.wPort = nCmdPort;
			dvrLoginInfo.pUser = this;
			// 调用异步接口函数...
			if( NET_DVR_Login_V40(&dvrLoginInfo, &dvrDevV40) < 0 ) {
				dwErr = NET_DVR_GetLastError();
				MsgLogGM(dwErr);
				break;
			}
			// 设置正在异步登录中标志，记录用户名和密码...
			m_strLoginUser = lpUser;
			m_strLoginPass = lpPass;
			m_HKLoginIng = true;
		}while(false);
	} else if( m_nCameraType == kCameraDH ) {
	}
	// 如果调用失败，清除所有资源...
	if( dwErr != GM_NoErr ) {
		m_strLogStatus.Format("登录失败，错误号：%lu", dwErr);
		m_lpVideoWnd->GetRenderWnd()->SetRenderText(m_strLogStatus);
		this->ClearResource();
		return dwErr;
	}
	// 记录登陆状态，返回正确结果...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	CString strTitle = theConfig.GetDBCameraTitle(m_nDBCameraID);
	m_strLogStatus.Format("正在登录 - %s", strTitle);
	m_lpVideoWnd->GetRenderWnd()->SetRenderText(m_strLogStatus);
	return GM_NoErr;
}
//
// 初始化摄像头...
BOOL CCamera::InitCamera(GM_MapData & inMapWeb)
{
	// 保存摄像头类型...
	string & strType = inMapWeb["camera_type"];
	if( strType.size() > 0 ) {
		m_nCameraType = (CAMERA_TYPE)atoi(strType.c_str());
	}
	string & strStream = inMapWeb["stream_prop"];
	if( strStream.size() > 0 ) {
		m_nStreamProp = (STREAM_PROP)atoi(strStream.c_str());
	}
	// 保存设备序列号，保存登录状态...
	m_strDeviceSN = inMapWeb["device_sn"];
	// 将监控通道配置，直接写入配置当中...
	ASSERT( m_lpVideoWnd != NULL );
	m_nDBCameraID = m_lpVideoWnd->GetDBCameraID();
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	CString strTitle = theConfig.GetDBCameraTitle(m_nDBCameraID);
	// 这里不用存盘，因为，本地不再存放配置，都在内存当中...
	m_strLogStatus.Format("未登录 - %s", strTitle);
	return true;
}
//
// 处理获取到的摄像机配置信息...
/*GM_Error CCamera::ForDeviceUDPData(GM_MapData & inNetData)
{
	// 已有的配置与输入的一致，直接返回...
	if( m_MapNetConfig == inNetData )
		return GM_NoErr;
	// 根据Types来保存不同的数据 => inquiry | update...
	GM_Error theErr = GM_No_Command;
	string & strCmd = inNetData["Types"];
	// 判断是否是有效的命令反馈...
	if( strCmd.compare("inquiry") != 0 && strCmd.compare("update") != 0 ) {
		MsgLogGM(theErr);
		return theErr;
	}
	// 直接保存有效数据并存盘...
	ASSERT( m_MapNetConfig.size() >= 0 );
	m_MapNetConfig = inNetData;
	// 获取当前摄像头存放的配置...
	ASSERT( m_lpVideoWnd != NULL );
	GM_MapData theMapWeb;
	int nDBCameraID = m_lpVideoWnd->GetDBCameraID();
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetCamera(nDBCameraID, theMapWeb);
	ASSERT( theMapWeb.size() > 0 );
	// 如果本摄像头IP地址发生了改变，通知上层处理，通道会自行检测...
	//if( theMapWeb["IPv4Address"] != inNetData["IPv4Address"] ) {
	//}
	// 更新已经存在的配置信息...
	theMapWeb["DeviceType"] = inNetData["DeviceType"];
	theMapWeb["DeviceDescription"] = inNetData["DeviceDescription"];
	theMapWeb["DeviceSN"] = inNetData["DeviceSN"];
	theMapWeb["CommandPort"] = inNetData["CommandPort"];
	theMapWeb["HttpPort"] = inNetData["HttpPort"];
	theMapWeb["MAC"] = inNetData["MAC"];
	theMapWeb["IPv4Address"] = inNetData["IPv4Address"];
	theMapWeb["BootTime"] = inNetData["BootTime"];
	theMapWeb["DigitalChannelNum"] = inNetData["DigitalChannelNum"];
	theMapWeb["DiskNumber"] = inNetData["DiskNumber"];
	// 将获取的更新数据直接存盘...
	theConfig.SetCamera(nDBCameraID, theMapWeb);
	theConfig.GMSaveConfig();
	return GM_NoErr;
}*/
//
// 更新窗口标题名称...
void CCamera::UpdateWndTitle(STREAM_PROP inPropType, CString & strTitle)
{
	if( m_lpVideoWnd == NULL )
		return;
	// 保存修改后的流属性 => 很重要...
	m_nStreamProp = inPropType;
	// 设置状态信息供右侧窗口使用，然后更新窗口标题信息...
	m_strLogStatus.Format("%s - %s", (this->IsLogin() ? "已登录" : "未登录"), strTitle);
	m_lpVideoWnd->GetRenderWnd()->SetRenderText(m_strLogStatus);
	m_lpVideoWnd->SetDispTitleText(strTitle);
}
//
// 停止上传，删除上传线程...
void CCamera::doDeletePushThread()
{
	// 删除上传推送线程...
	if( m_lpPushThread != NULL ) {
		m_lpPushThread->m_bDeleteFlag = true;
		delete m_lpPushThread;
		m_lpPushThread = NULL;
	}
}
//
// 处理来自播放器退出引发的删除上传直播通知...
// 备注：最终实际调用this->doStreamStopLivePush()...
void CCamera::doPostStopLiveMsg()
{
	// 摄像头设备模式 => 只停止rtmp上传对象...
	// 流数据转发模式 => 只停止rtmp上传对象...
	//WPARAM wMsgID = ((m_nStreamProp == kStreamDevice) ? WM_ERR_PUSH_MSG : WM_STOP_STREAM_MSG);
	WPARAM wMsgID = WM_STOP_LIVE_PUSH_MSG;
	if( m_lpVideoWnd != NULL && m_lpVideoWnd->m_hWnd != NULL ) {
		::PostMessage(m_lpVideoWnd->m_hWnd, wMsgID, NULL, NULL);
	}
}

void CCamera::doStreamStopLivePush()
{
	if( m_lpPushThread == NULL )
		return;
	ASSERT( m_lpPushThread != NULL );
	m_lpPushThread->StreamStopLivePush();

	/*if( m_nStreamProp == kStreamDevice || m_lpPushThread == NULL )
		return;
	ASSERT( m_nStreamProp != kStreamDevice && m_lpPushThread != NULL );
	m_lpPushThread->StreamStopLivePush();*/
}
//
// 启动直播上传...
// -1 => 启动失败，设置 err_code ...
//  0 => 已经启动，不用反馈给播放器...
//  1 => 首次启动，不用延时反馈给播放器...
int CCamera::doStreamStartLivePush(string & strRtmpUrl)
{
	// 如果当前通道还没有播放，直接返回...
	if( !this->IsPlaying() )
		return -1;
	// 推流线程必须有效，全部统一起来了...
	if( m_lpPushThread == NULL )
		return -1;
	ASSERT( m_lpPushThread != NULL );
	// 如果通道已经上传直播服务器成功，直接返回...
	if( m_lpPushThread->IsPublishing() )
		return 0;
	ASSERT( !m_lpPushThread->IsPublishing() );
	// 如果通道还没有上传成功，调用上传接口...
	m_lpPushThread->StreamStartLivePush(strRtmpUrl);
	return 1;
	/*// 如果是流数据转发的模式...
	if( m_nStreamProp != kStreamDevice ) {
		// 如果通道已经上传直播服务器成功，直接返回...
		if( m_lpPushThread->IsPublishing() )
			return 0;
		ASSERT( !m_lpPushThread->IsPublishing() );
		// 如果通道还没有上传成功，调用上传接口...
		m_lpPushThread->StreamStartLivePush(strRtmpUrl);
		return 1;
	}
	// 如果上传通道正在上传，不用将rtmp链接反馈给播放端...
	ASSERT( m_nStreamProp == kStreamDevice );
	if( m_lpPushThread != NULL )
		return 0;
	ASSERT( m_lpPushThread == NULL );
	// 准备直播需要的数据...
	CString strRtspUrl;
	GM_MapData theMapLoc;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetCamera(m_nCameraID, theMapLoc);
	// 准备rtsp直播链接地址 => 子码流 => rtsp://admin:12345@192.168.1.65/Streaming/Channels/102
	strRtspUrl.Format("rtsp://%s:%s@%s:%d/Streaming/Channels/102", m_strLoginUser, m_strLoginPass, theMapLoc["IPv4Address"].c_str(), m_nRtspPort);
	m_lpPushThread = new CPushThread(m_lpWndParent->m_hWnd, this);
	m_lpPushThread->DeviceInitThread(strRtspUrl, strRtmpUrl);
	return 1;*/
}
//
// 2017.06.15 - by jackey => 放弃这个接口...
// 延时通知直播播放器，上传结果...
/*void CCamera::doDelayTransmit(GM_Error inErr)
{
	if( m_lpHaoYiView == NULL || m_nPlayerSock <= 0 )
		return;
	ASSERT( m_lpHaoYiView != NULL && m_nPlayerSock > 0 );
	// 延时转发命令，然后将请求置空，复位...
	m_lpHaoYiView->doTransmitPlayer(m_nPlayerSock, m_strRtmpUrl, inErr);
	m_nPlayerSock = -1;
}*/
//
// 进行流转发模式的登录操作...
GM_Error CCamera::doStreamLogin()
{
	// 首先判断正在运行的资源...
	if( m_lpPushThread != NULL )
		return GM_NoErr;
	ASSERT( m_lpPushThread == NULL );
	// 设置异步登录标志...
	m_bStreamLogin = true;
	// 获取当前摄像头存放的配置...
	GM_MapData theMapWeb;
	ASSERT( m_lpVideoWnd != NULL );
	int nDBCameraID = m_lpVideoWnd->GetDBCameraID();
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetCamera(nDBCameraID, theMapWeb);
	ASSERT( theMapWeb.size() > 0 );
	// 获取需要的流转发参数信息...
	string & strStreamMP4 = theMapWeb["stream_mp4"];
	string & strStreamUrl = theMapWeb["stream_url"];
	//BOOL bStreamAuto = atoi(theMapLoc["stream_auto"].c_str());
	//BOOL bStreamLoop = atoi(theMapLoc["stream_loop"].c_str());
	BOOL bFileMode = ((m_nStreamProp == kStreamMP4File) ? true : false);
	// 只启动数据流部分，不进行rtmp推流处理...
	m_lpPushThread = new CPushThread(m_lpVideoWnd->m_hWnd, this);
	m_lpPushThread->StreamInitThread(bFileMode, strStreamUrl, strStreamMP4);
	// 更新登录状态...
	CString strStatus = "正在链接";
	CString strTitle = theConfig.GetDBCameraTitle(nDBCameraID);
	m_strLogStatus.Format("%s - %s", strStatus, strTitle);
	m_lpVideoWnd->GetRenderWnd()->SetRenderText(m_strLogStatus);
	m_lpVideoWnd->GetRenderWnd()->SetStreamStatus(strStatus);
	return GM_NoErr;
}
//
// 进行流转发模式的退出操作...
GM_Error CCamera::doStreamLogout()
{
	// 直接调用退出接口...
	this->doDeviceLogout();
	return GM_NoErr;
}

int CCamera::GetRecvPullKbps()
{
	// 2017.06.12 - by jackey => 这里返回-1很重要，表示通道已经关闭了...
	// 推流线程已经关闭，但是，通道状态还是处于登录当中，需要返回-1，彻底关闭...
	if( m_lpPushThread == NULL && this->IsLogin() )
		return - 1;
	// 通道正在的处理状态...
	return ((m_lpPushThread == NULL) ? 0 : m_lpPushThread->GetRecvKbps());
}

int	CCamera::GetSendPushKbps()
{
	return ((m_lpPushThread == NULL) ? 0 : m_lpPushThread->GetSendKbps());
}

LPCTSTR	CCamera::GetStreamPushUrl()
{
	if( m_lpPushThread == NULL )
		return NULL;
	return m_lpPushThread->m_strRtmpUrl.c_str();
}
