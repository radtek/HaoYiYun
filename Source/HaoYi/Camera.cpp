
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
	// �ȴ��첽��¼�˳�...
	this->WaitForExit();
	// �ͷ����е���Դ...
	this->ClearResource();
}
//
// �ȴ��첽��¼�˳� => ʹ�û��ⲻ������...
void CCamera::WaitForExit()
{
	m_bIsExiting = true;
	while( m_HKLoginIng ) {
		::Sleep(5);
	}
	ASSERT( !m_HKLoginIng );
}
//
// DVR�Ƿ��Ѿ��ɹ���½...
BOOL CCamera::IsLogin()
{
	// �������ת��ģʽ => ������ת����¼��־...
	if( m_nStreamProp != kStreamDevice ) {
		return m_bStreamLogin;
	}
	// ���������ͷ�豸ģʽ...
	ASSERT( m_nStreamProp == kStreamDevice );
	if( m_nCameraType == kCameraHK ) {
		if( m_HKLoginID >= 0 ) {
			return true;
		} else {
			// �����첽��¼�У������¼�ɹ�...
			ASSERT( m_HKLoginID < 0 );
			return (m_HKLoginIng ? true : false);
		}
	} else if( m_nCameraType == kCameraDH ) {
		return false;
	}
	return false;
}
//
// DVR�Ƿ�����ʵʱԤ��...
BOOL CCamera::IsPlaying()
{
	if( m_lpPushThread == NULL )
		return false;
	ASSERT( m_lpPushThread != NULL );
	return m_lpPushThread->IsStreamPlaying();
	// �������ת��ģʽ...
	/*if( m_nStreamProp != kStreamDevice ) {
		if( m_lpPushThread == NULL )
			return false;
		return m_lpPushThread->IsStreamPlaying();
	}
	// ���������ͷ�豸ģʽ...
	ASSERT( m_nStreamProp == kStreamDevice );
	switch( m_nCameraType )
	{
	case kCameraHK: return ((m_HKPlayID < 0) ? false : true);
	case kCameraDH: return false;
	}
	return false;*/
}
//
// ͨ���Ƿ������ڷ�����...
BOOL CCamera::IsPublishing()
{
	if( m_lpPushThread == NULL )
		return false;
	ASSERT( m_lpPushThread != NULL );
	return m_lpPushThread->IsPublishing();
}
//
// �����Ƿ�����¼��״̬��־...
BOOL CCamera::IsRecording()
{
	// ���������ͷ�豸��������˫��ģʽ...
	if( this->IsCameraDevice() && this->m_bIsTwiceMode ) {
		// ���¼����Ч�����Ѿ������ˣ� ɾ��¼�����...
		if( m_lpDeviceMainRec != NULL && m_lpDeviceMainRec->IsRecFinished() ) {
			// ֹͣ��ɾ������¼�����...
			this->DeviceStopMainRec();
			// ���������е�¼���¼��Ÿ�λ...
			m_nRecCourseID = -1;
			return false;
		}
		// ����״̬��ͨ����֤¼����󷵻ؽ��...
		return ((m_lpDeviceMainRec != NULL) ? true : false);
	}
	// ���������ģʽ�����������߳�...
	if( m_lpPushThread == NULL )
		return false;
	return m_lpPushThread->IsRecording();
}

//
// �ͷŽ�����Դ...
void CCamera::ClearResource()
{
	// �ͷ�����¼����Դ��ʵʱԤ����Դ...
	if( m_HKPlayID >= 0 ) {
		NET_DVR_StopRealPlay(m_HKPlayID);
		m_HKPlayID = -1;
	}
	// �ͷŵ�¼��Դ...
	if( m_HKLoginID >= 0 ) {
		NET_DVR_Logout_V30(m_HKLoginID);
		m_HKLoginID = -1;
		memset(&m_HKDeviceInfo, 0, sizeof(m_HKDeviceInfo));
	}
	// �ͷ�����ͷ˫��ģʽ¼�����...
	this->DeviceStopMainRec();
	// �ͷ��Զ���ֱ���ϴ�����...
	this->doDeletePushThread();
	// ����¼������Ϊ��Ч...
	m_nRecCourseID = -1;
	// �����첽��¼��־...
	m_bStreamLogin = false;
}
//
// ִ����̨����...
DWORD CCamera::doDevicePTZCmd(DWORD dwPTZCmd, BOOL bStop)
{
	// ������Ԥ��ģʽ��ʹ��...
	DWORD dwErr = GM_NoErr;
	if( m_HKPlayID < 0 )
		return GM_NoErr;
	ASSERT( m_HKPlayID >= 0 );
	// ���ﲻ��Speed��ʽ���� PAN_AUTO �²�������...
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
// ֪ͨ��վͨ��״̬...
void CCamera::doWebStatCamera(int nStatus, int nErrCode/* = 0*/, LPCTSTR lpszErrMsg/* = NULL*/)
{
	// �ж� VideoWnd �Ƿ�Ϊ��...
	if( m_lpVideoWnd == NULL )
		return;
	ASSERT( m_lpVideoWnd != NULL );
	// ��ȡ MidView ���ڶ���...
	CMidView * lpMidView = (CMidView*)m_lpVideoWnd->GetRealParent();
	if( lpMidView == NULL )
		return;
	ASSERT( lpMidView != NULL );
	// ͨ�����ر�Ż�ȡ���ݿ���...
	int nDBCameraID = this->GetDBCameraID();
	lpMidView->doWebStatCamera(nDBCameraID, nStatus, nErrCode, lpszErrMsg);
}
//
// ����������ת��״̬...
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
// ִ��DVRע������...
void CCamera::doDeviceLogout()
{
	// �ͷŽ�������Դ...
	this->ClearResource();
	// ��λ��¼״̬��Ϣ...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	m_strLogStatus.Format("δ��¼ - %s", theConfig.GetDBCameraTitle(m_nDBCameraID));
	// ������Ⱦ����״̬...
	ASSERT( m_lpVideoWnd != NULL );
	CString strStatus = "δ����...";
	CRenderWnd * lpRenderWnd = m_lpVideoWnd->GetRenderWnd();
	lpRenderWnd->SetRenderState(CRenderWnd::ST_WAIT);
	lpRenderWnd->SetRenderText(m_strLogStatus);
	lpRenderWnd->SetStreamStatus(strStatus);
	// ֪ͨ��վ��ͨ������Ϊ�ȴ�״̬ => Ӳ���豸 �� ��ת�� ����������·...
	this->doWebStatCamera(kCameraWait);
}
//
// ��¼�ص������ӿ�...
void CALLBACK CCamera::DeviceLoginResult(LONG lUserID, DWORD dwResult, LPNET_DVR_DEVICEINFO_V30 lpDeviceInfo, void * pUser)
{
	CCamera * lpCamera = (CCamera*)pUser;
	lpCamera->onDeviceLoginAsync(lUserID, dwResult, lpDeviceInfo);
}
//
// ʵ�ʴ����¼�ص��ĺ���...
void CCamera::onDeviceLoginAsync(LONG lUserID, DWORD dwResult, LPNET_DVR_DEVICEINFO_V30 lpDeviceInfo)
{
	// ���ڵ�¼״̬��λ...
	m_HKLoginIng = false;
	// �����˳�״̬�У�ֱ�ӷ���...
	if( m_bIsExiting ) {
		TRACE("CCamera::onDeviceLoginAsync => Exit\n");
		return;
	}
	// ��¼ʧ�ܵĴ�����̣���Ҫ֪ͨ�ϲ㴰��...
	if( dwResult <= 0 || lUserID < 0 ) {
		m_dwHKErrCode = NET_DVR_GetLastError();
		m_strLogStatus.Format("��¼ʧ�ܣ�����%s", NET_DVR_GetErrorMsg(&m_dwHKErrCode));
		m_lpVideoWnd->GetRenderWnd()->SetRenderText(m_strLogStatus);
		::PostMessage(m_hWndRight, WM_DEVICE_LOGIN_RESULT, m_nDBCameraID, false);
		// ֪ͨ��վ��ͨ������Ϊ�ȴ�״̬ => ֻ��Ӳ���豸��������·...
		this->doWebStatCamera(kCameraWait, m_dwHKErrCode, NET_DVR_GetErrorMsg(&m_dwHKErrCode));
		// �ͷŸ�ͨ���ϵ���Դ����...
		this->ClearResource();
		MsgLogGM(m_dwHKErrCode);
		return;
	}
	// ��¼�ɹ�֮�󣬱�������...
	m_dwHKErrCode = NET_DVR_NOERROR;
	this->m_HKLoginID = lUserID;
	memcpy(&m_HKDeviceInfo, lpDeviceInfo, sizeof(NET_DVR_DEVICEINFO_V30));
	// ��¼��½״̬��������ȷ���...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	m_strLogStatus.Format("�ѵ�¼ - %s", theConfig.GetDBCameraTitle(m_nDBCameraID));
	m_lpVideoWnd->GetRenderWnd()->SetRenderText(m_strLogStatus);
	// ����������״̬��֪ͨ�����ڣ������첽��¼���...
	ASSERT( m_hWndRight != NULL && m_nDBCameraID > 0 );
	::PostMessage(m_hWndRight, WM_DEVICE_LOGIN_RESULT, m_nDBCameraID, true);
	// �����Ȳ�֪ͨ��վͨ��״̬����Ϊ�����滹�в�����Ҳ���ܷ�������...
}
//
// ִ��DVR�첽��¼�ɹ�����Ϣ�¼�...
DWORD CCamera::onDeviceLoginSuccess()
{
	ASSERT( m_nDBCameraID > 0 );
	LONG dwErr = GM_NoErr;
	GM_MapData theMapWeb;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetCamera(m_nDBCameraID, theMapWeb);
	ASSERT( m_HKPlayID < 0 && m_HKLoginID >= 0 );
	do {
		// DVR�ĵ�һ��ͨ����һ��DVR�����ж��ͨ��...
		LONG  nDvrStartChan = m_HKDeviceInfo.byStartChan;
		DWORD dwReturn = 0;
		// ��ȡDVR��ͨ����Դ...
		//NET_DVR_IPPARACFG_V40 dvrIPCfg = {0};
		//BOOL bIPRet = NET_DVR_GetDVRConfig(m_HKLoginID, NET_DVR_GET_IPPARACFG_V40, nDvrStartChan, &dvrIPCfg, sizeof(dvrIPCfg), &dwReturn);
		// ��ȡ��̨������Ϣ...
		//NET_DVR_DECODERCFG_V30 dvrDecoderCfg = {0};
		//BOOL bDecRet = NET_DVR_GetDVRConfig(m_HKLoginID, NET_DVR_GET_DECODERCFG_V30, nDvrStartChan, &dvrDecoderCfg, sizeof(dvrDecoderCfg), &dwReturn);
		//(Ŀǰͨ��WEB����DVR) ��ȡ�豸���б������� => ��Ҫ����xml���ó����Զ�ѡ����û��ֶ�ѡ��д���豸����...
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
		// ��ȡIPC��rtsp�˿ں�...
		NET_DVR_RTSPCFG dvrRtsp = {0};
		if( !NET_DVR_GetRtspConfig(m_HKLoginID, 0, &dvrRtsp, sizeof(NET_DVR_RTSPCFG)) ) {
			dwErr = NET_DVR_GetLastError();
			MsgLogGM(dwErr);
			break;
		}
		// ������ȷ������rtsp�˿�...
		m_nRtspPort = dvrRtsp.wPort;
		// ��ȡ��Ϊ������������ => NET_DVR_GetDeviceConfig(NET_DVR_GET_MULTI_STREAM_COMPRESSIONCFG) => NET_DVR_MULTI_STREAM_COMPRESSIONCFG_COND NET_DVR_MULTI_STREAM_COMPRESSIONCFG
		// ��ȡѹ�����ò�����Ϣ => ������ ������ �� ������ ...
		NET_DVR_COMPRESSIONCFG_V30 dvrCompressCfg = {0};
		if( !NET_DVR_GetDVRConfig(m_HKLoginID, NET_DVR_GET_COMPRESSCFG_V30, nDvrStartChan, &dvrCompressCfg, sizeof(dvrCompressCfg), &dwReturn) ) {
			dwErr = NET_DVR_GetLastError();
			MsgLogGM(dwErr);
			break;
		}
		// �ж�������������������Ƶ�����Ƿ���ȷ => ������
		if( dvrCompressCfg.struNormHighRecordPara.byStreamType != 1 ||
			dvrCompressCfg.struNetPara.byStreamType != 1 ) {
			dwErr = GM_DVR_VType_Err;
			MsgLogGM(dwErr);
			break;
		}
		// �ж�����������������Ƶ���������Ƿ���ȷ => H264
		if( dvrCompressCfg.struNormHighRecordPara.byVideoEncType != NET_DVR_ENCODER_H264 ||
			dvrCompressCfg.struNetPara.byVideoEncType != NET_DVR_ENCODER_H264 ) {
			dwErr = GM_DVR_VEnc_Err;
			MsgLogGM(dwErr);
			break;
		}
		// �ж�����������������Ƶ���������Ƿ���ȷ => AAC
		if( dvrCompressCfg.struNormHighRecordPara.byAudioEncType != AUDIOTALKTYPE_AAC ||
			dvrCompressCfg.struNetPara.byAudioEncType != AUDIOTALKTYPE_AAC ) {
			dwErr = GM_DVR_AEnc_Err;
			MsgLogGM(dwErr);
			break;
		}
		// ����һЩ���� => �����豸�ı������������ã��������������õ�...
		// �������ļ��ж�ȡ��������������С����������С => �Զ�������...
		dvrCompressCfg.struNormHighRecordPara.dwVideoBitrate = theConfig.GetMainKbps() * 1024;
		dvrCompressCfg.struNetPara.dwVideoBitrate = theConfig.GetSubKbps() * 1024;
		dvrCompressCfg.struNormHighRecordPara.dwVideoBitrate |= 0x80000000;
		dvrCompressCfg.struNetPara.dwVideoBitrate |= 0x80000000;
		// ���� ������ �� ������ �����ò�����Ϣ...
		if( !NET_DVR_SetDVRConfig(m_HKLoginID, NET_DVR_SET_COMPRESSCFG_V30, nDvrStartChan, &dvrCompressCfg, sizeof(dvrCompressCfg)) ) {
			dwErr = NET_DVR_GetLastError();
			MsgLogGM(dwErr);
			break;
		}
		// ��ȡDVR�豸��ǰ�˲���...
		NET_DVR_CAMERAPARAMCFG dvrCCDParam = {0};
		if( !NET_DVR_GetDVRConfig(m_HKLoginID, NET_DVR_GET_CCDPARAMCFG, nDvrStartChan, &dvrCCDParam, sizeof(dvrCCDParam), &dwReturn) ) {
			dwErr = NET_DVR_GetLastError();
			MsgLogGM(dwErr);
			break;
		}
		// ��ͨ�������ļ��л�ȡ�Ƿ�������...
		string & strMirror = theMapWeb["device_mirror"];
		BOOL bOpenMirror = ((strMirror.size() > 0) ? atoi(strMirror.c_str()) : false);
		// �Ծ���ģʽ���д��� => ����0 �ر�;1 ����;2 ����;3 �м�
		dvrCCDParam.byMirror = (bOpenMirror ? 3 : 0);
		if( !NET_DVR_SetDVRConfig(m_HKLoginID, NET_DVR_SET_CCDPARAMCFG, nDvrStartChan, &dvrCCDParam, sizeof(dvrCCDParam)) ) {
			dwErr = NET_DVR_GetLastError(); // ע���������ţ�NET_DVR_NETWORK_ERRORDATA
			MsgLogGM(dwErr);
			break;
		}
		// ��ȡͼ����� => OSD | ���� | ���� | ���� | ���� | ����
		NET_DVR_PICCFG_V40 dvrPicV40 = {0};
		if( !NET_DVR_GetDVRConfig(m_HKLoginID, NET_DVR_GET_PICCFG_V40, nDvrStartChan, &dvrPicV40, sizeof(dvrPicV40), &dwReturn) ) {
			dwErr = NET_DVR_GetLastError();
			MsgLogGM(dwErr);
			break;
		}
		// ��ͨ�������ļ��л�ȡ�Ƿ���OSD...
		string & strOSD = theMapWeb["device_osd"];
		BOOL bOpenOSD = ((strOSD.size() > 0) ? atoi(strOSD.c_str()) : true);
		// ����ͼ���ʽ => OSD | ���� | ���� | ���� | ���� | ����
		//strcpy((char*)dvrPicV40.sChanName, "Camera"); // ͨ������...
		//dvrPicV40.dwVideoFormat = 2; // ��Ƶ��ʽ��0- ��֧�֣�1- NTSC��2- PAL 
		//dvrPicV40.dwShowChanName = 0; // Ԥ����ͼ�����Ƿ���ʾͨ�����ƣ�0-����ʾ��1-��ʾ�������С704*576�� 
		//dvrPicV40.wShowNameTopLeftX = 200; // ͨ��������ʾλ�õ�x����
		//dvrPicV40.wShowNameTopLeftY = 100; // ͨ��������ʾλ�õ�y����
		//dvrPicV40.dwEnableHide = 1; // �Ƿ�������˽�ڱΣ�0-��1-��
		dvrPicV40.dwShowOsd = bOpenOSD; // Ԥ����ͼ�����Ƿ���ʾOSD��0-����ʾ��1-��ʾ�������С704*576��
		dvrPicV40.wOSDTopLeftX = 300; // OSD��x����
		dvrPicV40.wOSDTopLeftY = 20; // OSD��y����
		dvrPicV40.byOSDType = 2; // OSD����(�����ո�ʽ) 0��XXXX-XX-XX ������; 1��XX-XX-XXXX ������; 2��XXXX��XX��XX��; 3��XX��XX��XXXX��; 4��XX-XX-XXXX ������; 5��XX��XX��XXXX��; 6��xx/xx/xxxx ��/��/��; 7��xxxx/xx/xx ��/��/��; 8��xx/xx/xxxx ��/��/��
		dvrPicV40.byDispWeek = 0; // �Ƿ���ʾ���ڣ�0-����ʾ��1-��ʾ
		dvrPicV40.byOSDAttrib = 2; // OSD���ԣ�͸��/��˸����1��͸������˸��2��͸��������˸��3����˸����͸����4����͸��������˸
		dvrPicV40.byHourOSDType = 0; // Сʱ�ƣ�0��ʾ24Сʱ�ƣ�1��ʾ12Сʱ�ƻ�am/pm 
		dvrPicV40.byFontSize = 0xFF; // �����С��0- 16*16(��)/8*16(Ӣ)��1- 32*32(��)/16*32(Ӣ)��2- 64*64(��)/32*64(Ӣ)��3- 48*48(��)/24*48(Ӣ)��4- 24*24(��)/12*24(Ӣ)��5- 96*96(��)/48*96(Ӣ)��0xff- ����Ӧ
		dvrPicV40.byOSDColorType = 0; // OSD��ɫģʽ��0- Ĭ�ϣ��ڰף���1-�Զ���(��ɫ��struOsdColor)
		dvrPicV40.struOsdColor.byRed = 255;
		dvrPicV40.struOsdColor.byGreen = 0;
		dvrPicV40.struOsdColor.byBlue = 0;
		dvrPicV40.byAlignment = 0; // ���뷽ʽ��0- ����Ӧ��1- �Ҷ��룬2- �����
		if( !NET_DVR_SetDVRConfig(m_HKLoginID, NET_DVR_SET_PICCFG_V40, nDvrStartChan, &dvrPicV40, sizeof(dvrPicV40)) ) {
			dwErr = NET_DVR_GetLastError();
			MsgLogGM(dwErr);
			break;
		}
		// ��DVR�豸����Уʱ���� => ���óɸ�����ʱ��һ��...
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
		// ׼����ʾԤ��������Ҫ�Ĳ���...
		ASSERT( m_lpVideoWnd != NULL );
		CRenderWnd * lpRenderWnd = m_lpVideoWnd->GetRenderWnd();
		NET_DVR_CLIENTINFO dvrClientInfo = {0};
		dvrClientInfo.hPlayWnd     = lpRenderWnd->m_hWnd;
		dvrClientInfo.lChannel     = nDvrStartChan;
		dvrClientInfo.lLinkMode    = 0;
		dvrClientInfo.sMultiCastIP = NULL;
		// ����ʵʱԤ���ӿ�...
		m_HKPlayID = NET_DVR_RealPlay_V30(m_HKLoginID, &dvrClientInfo, NULL, NULL, TRUE);
		if( m_HKPlayID < 0 ) {
			dwErr = NET_DVR_GetLastError();
			MsgLogGM(dwErr);
			break;
		}
		// ������Ⱦ����״̬...
		lpRenderWnd->SetRenderState(CRenderWnd::ST_RENDER);
	}while(false);
	// �������ʧ�ܣ����������Դ...
	if( dwErr != GM_NoErr ) {
		m_strLogStatus.Format("��¼ʧ�ܣ�����ţ�%lu", dwErr);
		m_lpVideoWnd->GetRenderWnd()->SetRenderText(m_strLogStatus);
		this->ClearResource();
		// ֪ͨ��վ��ͨ������Ϊ�ȴ�״̬ => ֻ��Ӳ���豸������·...
		this->doWebStatCamera(kCameraWait, dwErr, NET_DVR_GetErrorMsg(&dwErr));
		return dwErr;
	}
	// ��¼��½״̬��������ȷ���...
	m_strLogStatus.Format("�ѵ�¼ - %s", theConfig.GetDBCameraTitle(m_nDBCameraID));
	m_lpVideoWnd->GetRenderWnd()->SetRenderText(m_strLogStatus);
	// ֪ͨ��վ��ͨ������Ϊ����״̬ => ֻ��Ӳ���豸������·...
	this->doWebStatCamera(kCameraRun);
	/////////////////////////////////////////////////////////////////////////////////////
	// 2017.08.15 - by jackey => ����һ·��rtsp�������߳�...
	// ����rtsp�����̣߳�ֻ������¼��¼����¼�������...
	/////////////////////////////////////////////////////////////////////////////////////
	// ׼��rtsp���ӵ�ַ => ������ => rtsp://admin:12345@192.168.1.65/Streaming/Channels/101
	// ׼��rtsp���ӵ�ַ => ������ => rtsp://admin:12345@192.168.1.65/Streaming/Channels/102
	m_strRtspMainUrl.Format("rtsp://%s:%s@%s:%d/Streaming/Channels/101", m_strLoginUser, m_strLoginPass, theMapWeb["device_ip"].c_str(), m_nRtspPort);
	m_strRtspSubUrl.Format("rtsp://%s:%s@%s:%d/Streaming/Channels/102", m_strLoginUser, m_strLoginPass, theMapWeb["device_ip"].c_str(), m_nRtspPort);
	// ��ȡ��ǰͨ������ʱ�̵��˫��ģʽ��־����������������Ҫ��̬��ȡ������¼��Ŀ�ʼ��ֹͣ���ܻ���ɱ�־��һ��....
	string & strTwice = theMapWeb["device_twice"];
	m_bIsTwiceMode = ((strTwice.size() > 0) ? atoi(strTwice.c_str()) : false);
	// ������˫��ģʽ��ʹ��������ֱ����������¼�񣻲�����ʹ��������ֱ����������¼��...
	string strStreamUrl, strStreamMP4;
	strStreamUrl.assign(m_bIsTwiceMode ? m_strRtspSubUrl : m_strRtspMainUrl);
	// ������ת���������̶߳��󣬲���������...
	m_lpPushThread = new CPushThread(m_lpVideoWnd->m_hWnd, this);
	m_lpPushThread->StreamInitThread(false, strStreamUrl, strStreamMP4);
	return GM_NoErr;
}
//
// ����һ��¼����Ƭ...
/*void CCamera::StartRecSlice()
{
	// �����Զ����rtsp¼����� => �Զ�¼������������Ƶ...
	if( m_lpRecThread != NULL ) {
		delete m_lpRecThread;
		m_lpRecThread = NULL;
	}
	// ��ȡΨһ���ļ���...
	MD5	    md5;
	string  strUniqid;
	CString strTimeMicro;
	ULARGE_INTEGER	llTimCountCur = {0};
	::GetSystemTimeAsFileTime((FILETIME *)&llTimCountCur);
	strTimeMicro.Format("%I64d", llTimCountCur.QuadPart);
	md5.update(strTimeMicro, strTimeMicro.GetLength());
	strUniqid = md5.toString();

	// ����һ����ͼ�ļ�...
	this->doSnapJPG(strUniqid);

	// ��ȡ¼����Ƭʱ��(����)...
	GM_MapData theMapLoc;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	DWORD dwRecSliceMS = theConfig.GetRecSlice() * 1000;
	theConfig.GetCamera(m_nCameraID, theMapLoc);
	// ׼��rtsp���ӵ�ַ��MP4¼������...
	CString  strRtspUrl, strMP4Path;
	string & strSavePath = theConfig.GetSavePath();
	string & strDBCameraID = theMapLoc["DBCameraID"];
	//strCurTime = CTime::GetCurrentTime().Format("%Y%m%d%H%M%S");
	//m_strMP4Name.Format("%s\\%s_%d", strSavePath.c_str(), strCurTime, m_nCameraID);
	m_strMP4Name.Format("%s\\%s_%s", strSavePath.c_str(), strUniqid.c_str(), strDBCameraID.c_str());
	strMP4Path.Format("%s.tmp", m_strMP4Name);
	// ׼��rtsp���ӵ�ַ => ������ => rtsp://admin:12345@192.168.1.65/Streaming/Channels/101
	strRtspUrl.Format("rtsp://%s:%s@%s:%d/Streaming/Channels/101", m_strLoginUser, m_strLoginPass, theMapLoc["IPv4Address"].c_str(), m_nRtspPort);
	// ����¼���̶߳���...
	ASSERT( m_lpWndParent != NULL && m_lpWndParent->m_hWnd != NULL );
	m_lpRecThread = new CRtspRecThread(m_lpWndParent->m_hWnd, this, dwRecSliceMS);
	// �����ɹ���ֱ�ӳ�ʼ��¼���߳�...
	if( !m_lpRecThread->InitThread(m_nCameraID, strRtspUrl, strMP4Path) ) {
		delete m_lpRecThread;
		m_lpRecThread = NULL;
	}
}*/
//
// ִ��¼����Ƭ�������������µ�¼��...
/*void CCamera::doRecSlice()
{
	// ���û�п�ʼ¼�ƣ�ֱ�ӷ���...
	if( m_lpRecThread == NULL )
		return;
	ASSERT( m_lpRecThread != NULL );
	// ֱ��ɾ��¼���߳�(����doRecEnd)�������µ�¼���߳�...
	delete m_lpRecThread; m_lpRecThread = NULL;
	// ������¼�����д��һ��ͳһ�ļ򵥺���...
	//this->StartRecSlice();
}*/
//
// ִ��¼������¼�...
/*void CCamera::doRecEnd(int nRecSecond)
{
	// �Ƚ��������еļ�¼��Ÿ�λ...
	m_nRecCourseID = -1;
	// ����¼���ļ�����չ��...
	CString strMP4Temp, strMP4File;
	strMP4Temp.Format("%s.tmp", m_strMP4Name);
	strMP4File.Format("%s_%d.mp4", m_strMP4Name, nRecSecond);
	// ����ļ������ڣ�ֱ�ӷ���...
	if( _access(strMP4Temp, 0) < 0 )
		return;
	// ֱ�Ӷ��ļ����и��Ĳ���������¼ʧ�ܵ���־...
	if( !MoveFile(strMP4Temp, strMP4File) ) {
		MsgLogGM(::GetLastError());
	}
}*/
//
// ����¼��ǰ�Ľ�ͼ�¼�...
/*DWORD CCamera::doDeviceSnapJPG(CString & inJpgName)
{
	// Ϊ�˱��������һ��Ҫ�ò���״̬�жϣ���¼״̬�������첽�������᲻��ȷ...
	if( !this->IsPlaying() )
		return GM_NoErr;
	ASSERT( m_HKLoginID >= 0 );
	ASSERT( inJpgName.GetLength() > 0 );
	// DVR�ĵ�һ��ͨ����һ��DVR�����ж��ͨ��...
	DWORD dwReturn = 0;
	DWORD dwErr = GM_NoErr;
	LONG  nDvrStartChan = m_HKDeviceInfo.byStartChan;
	// �齨jpg�ṹ => ��ͼ��ѹ���ֱ���ΪVGAʱ��֧��0=CIF, 1=QCIF, 2=D1ץͼ, ���ֱ���Ϊ3=UXGA(1600x1200), 4=SVGA(800x600), 5=HD720p(1280x720),6=VGA,7=XVGA, 8=HD900p ��֧�ֵ�ǰ�ֱ��ʵ�ץͼ
	// Ĭ�����ó� D1 ģʽ�������������...
	NET_DVR_JPEGPARA dvrJpg = {0};
	dvrJpg.wPicSize = 2;		// D1
	dvrJpg.wPicQuality = 0;		// 0-��� 1-�Ϻ� 2-һ��
	// ֱ����Ӳ��ץͼ���������Ƿ�����Ԥ��...
	if( !NET_DVR_CaptureJPEGPicture(m_HKLoginID, nDvrStartChan, &dvrJpg, (LPSTR)inJpgName.GetString()) ) {
		dwErr = NET_DVR_GetLastError();
		MsgLogGM(dwErr);
		return dwErr;
	}
	return GM_NoErr;
}*/
//
// �����豸������¼��...
BOOL CCamera::DeviceStartMainRec()
{
	// ���ȣ��ر�������¼�����...
	this->DeviceStopMainRec();
	ASSERT( m_lpDeviceMainRec == NULL );
	// �½�һ��������¼�����...
	m_lpDeviceMainRec = new CRtspRecThread(this);
	return m_lpDeviceMainRec->InitThread(m_strRtspMainUrl);
}
//
// �ر��豸������¼��...
BOOL CCamera::DeviceStopMainRec()
{
	if( m_lpDeviceMainRec != NULL ) {
		delete m_lpDeviceMainRec;
		m_lpDeviceMainRec = NULL;
	}
	return true;
}
//
// ��������¼�� => ͬһʱ��ֻ����һ��¼��...
void CCamera::doRecStartCourse(int nCourseID)
{
	// �����ǰͨ����û�в��ţ�ֱ�ӷ���...
	if( !this->IsPlaying() )
		return;
	// ����������еļ�¼���ǵ�ǰ��¼��ֱ�ӷ���...
	if( m_nRecCourseID == nCourseID )
		return;
	ASSERT( m_nRecCourseID != nCourseID );
	// �ȱ��浱ǰ�γ̱�ţ�����������õ�...
	m_nRecCourseID = nCourseID;
	// ���������ͷ�豸��������˫��ģʽ��ʹ�õ�����������¼��...
	if( this->IsCameraDevice() && this->m_bIsTwiceMode ) {
		this->DeviceStartMainRec();
		return;
	}
	////////////////////////////////////////////////////////////
	// ����ǵ���ģʽ��ת��ģʽ => ֱ����ת���߳�¼��...
	////////////////////////////////////////////////////////////
	// �����߳�һ����Ч => IsPlaying() ���Ա�֤...
	ASSERT( m_lpPushThread != NULL );
	// ���������̵߳�¼��ӿ�...
	m_lpPushThread->StreamBeginRecord();
	return;
}
//
// ֹͣ����¼�� => ͬһʱ��ֻ����һ��¼��...
void CCamera::doRecStopCourse(int nCourseID)
{
	// �����ǰͨ����û�е�¼��ֱ�ӷ���...
	if( !this->IsPlaying() )
		return;
	// ����������еļ�¼���ǵ�ǰ��¼��ֱ�ӷ���...
	if( m_nRecCourseID != nCourseID )
		return;
	ASSERT( m_nRecCourseID == nCourseID );
	// ֱ�ӵ��ýӿ�ֹͣ¼��...
	if( m_lpPushThread != NULL ) {
		m_lpPushThread->StreamEndRecord();
	}
	// ֱ�ӵ��ýӿڣ�ֹͣ������¼��...
	this->DeviceStopMainRec();
	// ���������еļ�¼��Ÿ�λ...
	m_nRecCourseID = -1;
}
//
// ִ��DVR��¼����...
DWORD CCamera::doDeviceLogin(HWND hWndNotify, LPCTSTR lpIPAddr, int nCmdPort, LPCTSTR lpUser, LPCTSTR lpPass)
{
	// ��������Ĵ����־��λ...
	m_dwHKErrCode = NET_DVR_NOERROR;
	// ��¼֮ǰ�����ͷ���Դ������֪ͨ����...
	DWORD dwErr = GM_NoErr;
	ASSERT( hWndNotify != NULL );
	m_hWndRight = hWndNotify;
	this->ClearResource();
	// ��ʼ���ݲ�ͬ��DVR���ò�ͬ��SDK...
	if( m_nCameraType == kCameraHK ) {
		do {
			// �첽��ʽ��¼DVR�豸...
			NET_DVR_DEVICEINFO_V40  dvrDevV40 = {0};
			NET_DVR_USER_LOGIN_INFO dvrLoginInfo = {0};
			dvrLoginInfo.cbLoginResult = CCamera::DeviceLoginResult;
			strcpy(dvrLoginInfo.sDeviceAddress, lpIPAddr);
			strcpy(dvrLoginInfo.sUserName, lpUser);
			strcpy(dvrLoginInfo.sPassword, lpPass);
			dvrLoginInfo.bUseAsynLogin = 1;
			dvrLoginInfo.wPort = nCmdPort;
			dvrLoginInfo.pUser = this;
			// �����첽�ӿں���...
			if( NET_DVR_Login_V40(&dvrLoginInfo, &dvrDevV40) < 0 ) {
				dwErr = NET_DVR_GetLastError();
				MsgLogGM(dwErr);
				break;
			}
			// ���������첽��¼�б�־����¼�û���������...
			m_strLoginUser = lpUser;
			m_strLoginPass = lpPass;
			m_HKLoginIng = true;
		}while(false);
	} else if( m_nCameraType == kCameraDH ) {
	}
	// �������ʧ�ܣ����������Դ...
	if( dwErr != GM_NoErr ) {
		m_strLogStatus.Format("��¼ʧ�ܣ�����ţ�%lu", dwErr);
		m_lpVideoWnd->GetRenderWnd()->SetRenderText(m_strLogStatus);
		this->ClearResource();
		return dwErr;
	}
	// ��¼��½״̬��������ȷ���...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	CString strTitle = theConfig.GetDBCameraTitle(m_nDBCameraID);
	m_strLogStatus.Format("���ڵ�¼ - %s", strTitle);
	m_lpVideoWnd->GetRenderWnd()->SetRenderText(m_strLogStatus);
	return GM_NoErr;
}
//
// ��ʼ������ͷ...
BOOL CCamera::InitCamera(GM_MapData & inMapWeb)
{
	// ��������ͷ����...
	string & strType = inMapWeb["camera_type"];
	if( strType.size() > 0 ) {
		m_nCameraType = (CAMERA_TYPE)atoi(strType.c_str());
	}
	string & strStream = inMapWeb["stream_prop"];
	if( strStream.size() > 0 ) {
		m_nStreamProp = (STREAM_PROP)atoi(strStream.c_str());
	}
	// �����豸���кţ������¼״̬...
	m_strDeviceSN = inMapWeb["device_sn"];
	// �����ͨ�����ã�ֱ��д�����õ���...
	ASSERT( m_lpVideoWnd != NULL );
	m_nDBCameraID = m_lpVideoWnd->GetDBCameraID();
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	CString strTitle = theConfig.GetDBCameraTitle(m_nDBCameraID);
	// ���ﲻ�ô��̣���Ϊ�����ز��ٴ�����ã������ڴ浱��...
	m_strLogStatus.Format("δ��¼ - %s", strTitle);
	return true;
}
//
// �����ȡ���������������Ϣ...
/*GM_Error CCamera::ForDeviceUDPData(GM_MapData & inNetData)
{
	// ���е������������һ�£�ֱ�ӷ���...
	if( m_MapNetConfig == inNetData )
		return GM_NoErr;
	// ����Types�����治ͬ������ => inquiry | update...
	GM_Error theErr = GM_No_Command;
	string & strCmd = inNetData["Types"];
	// �ж��Ƿ�����Ч�������...
	if( strCmd.compare("inquiry") != 0 && strCmd.compare("update") != 0 ) {
		MsgLogGM(theErr);
		return theErr;
	}
	// ֱ�ӱ�����Ч���ݲ�����...
	ASSERT( m_MapNetConfig.size() >= 0 );
	m_MapNetConfig = inNetData;
	// ��ȡ��ǰ����ͷ��ŵ�����...
	ASSERT( m_lpVideoWnd != NULL );
	GM_MapData theMapWeb;
	int nDBCameraID = m_lpVideoWnd->GetDBCameraID();
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetCamera(nDBCameraID, theMapWeb);
	ASSERT( theMapWeb.size() > 0 );
	// ���������ͷIP��ַ�����˸ı䣬֪ͨ�ϲ㴦��ͨ�������м��...
	//if( theMapWeb["IPv4Address"] != inNetData["IPv4Address"] ) {
	//}
	// �����Ѿ����ڵ�������Ϣ...
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
	// ����ȡ�ĸ�������ֱ�Ӵ���...
	theConfig.SetCamera(nDBCameraID, theMapWeb);
	theConfig.GMSaveConfig();
	return GM_NoErr;
}*/
//
// ���´��ڱ�������...
void CCamera::UpdateWndTitle(STREAM_PROP inPropType, CString & strTitle)
{
	if( m_lpVideoWnd == NULL )
		return;
	// �����޸ĺ�������� => ����Ҫ...
	m_nStreamProp = inPropType;
	// ����״̬��Ϣ���Ҳര��ʹ�ã�Ȼ����´��ڱ�����Ϣ...
	m_strLogStatus.Format("%s - %s", (this->IsLogin() ? "�ѵ�¼" : "δ��¼"), strTitle);
	m_lpVideoWnd->GetRenderWnd()->SetRenderText(m_strLogStatus);
	m_lpVideoWnd->SetDispTitleText(strTitle);
}
//
// ֹͣ�ϴ���ɾ���ϴ��߳�...
void CCamera::doDeletePushThread()
{
	// ɾ���ϴ������߳�...
	if( m_lpPushThread != NULL ) {
		m_lpPushThread->m_bDeleteFlag = true;
		delete m_lpPushThread;
		m_lpPushThread = NULL;
	}
}
//
// �������Բ������˳�������ɾ���ϴ�ֱ��֪ͨ...
// ��ע������ʵ�ʵ���this->doStreamStopLivePush()...
void CCamera::doPostStopLiveMsg()
{
	// ����ͷ�豸ģʽ => ֹֻͣrtmp�ϴ�����...
	// ������ת��ģʽ => ֹֻͣrtmp�ϴ�����...
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
// ����ֱ���ϴ�...
// -1 => ����ʧ�ܣ����� err_code ...
//  0 => �Ѿ����������÷�����������...
//  1 => �״�������������ʱ������������...
int CCamera::doStreamStartLivePush(string & strRtmpUrl)
{
	// �����ǰͨ����û�в��ţ�ֱ�ӷ���...
	if( !this->IsPlaying() )
		return -1;
	// �����̱߳�����Ч��ȫ��ͳһ������...
	if( m_lpPushThread == NULL )
		return -1;
	ASSERT( m_lpPushThread != NULL );
	// ���ͨ���Ѿ��ϴ�ֱ���������ɹ���ֱ�ӷ���...
	if( m_lpPushThread->IsPublishing() )
		return 0;
	ASSERT( !m_lpPushThread->IsPublishing() );
	// ���ͨ����û���ϴ��ɹ��������ϴ��ӿ�...
	m_lpPushThread->StreamStartLivePush(strRtmpUrl);
	return 1;
	/*// �����������ת����ģʽ...
	if( m_nStreamProp != kStreamDevice ) {
		// ���ͨ���Ѿ��ϴ�ֱ���������ɹ���ֱ�ӷ���...
		if( m_lpPushThread->IsPublishing() )
			return 0;
		ASSERT( !m_lpPushThread->IsPublishing() );
		// ���ͨ����û���ϴ��ɹ��������ϴ��ӿ�...
		m_lpPushThread->StreamStartLivePush(strRtmpUrl);
		return 1;
	}
	// ����ϴ�ͨ�������ϴ������ý�rtmp���ӷ��������Ŷ�...
	ASSERT( m_nStreamProp == kStreamDevice );
	if( m_lpPushThread != NULL )
		return 0;
	ASSERT( m_lpPushThread == NULL );
	// ׼��ֱ����Ҫ������...
	CString strRtspUrl;
	GM_MapData theMapLoc;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetCamera(m_nCameraID, theMapLoc);
	// ׼��rtspֱ�����ӵ�ַ => ������ => rtsp://admin:12345@192.168.1.65/Streaming/Channels/102
	strRtspUrl.Format("rtsp://%s:%s@%s:%d/Streaming/Channels/102", m_strLoginUser, m_strLoginPass, theMapLoc["IPv4Address"].c_str(), m_nRtspPort);
	m_lpPushThread = new CPushThread(m_lpWndParent->m_hWnd, this);
	m_lpPushThread->DeviceInitThread(strRtspUrl, strRtmpUrl);
	return 1;*/
}
//
// 2017.06.15 - by jackey => ��������ӿ�...
// ��ʱֱ֪ͨ�����������ϴ����...
/*void CCamera::doDelayTransmit(GM_Error inErr)
{
	if( m_lpHaoYiView == NULL || m_nPlayerSock <= 0 )
		return;
	ASSERT( m_lpHaoYiView != NULL && m_nPlayerSock > 0 );
	// ��ʱת�����Ȼ�������ÿգ���λ...
	m_lpHaoYiView->doTransmitPlayer(m_nPlayerSock, m_strRtmpUrl, inErr);
	m_nPlayerSock = -1;
}*/
//
// ������ת��ģʽ�ĵ�¼����...
GM_Error CCamera::doStreamLogin()
{
	// �����ж��������е���Դ...
	if( m_lpPushThread != NULL )
		return GM_NoErr;
	ASSERT( m_lpPushThread == NULL );
	// �����첽��¼��־...
	m_bStreamLogin = true;
	// ��ȡ��ǰ����ͷ��ŵ�����...
	GM_MapData theMapWeb;
	ASSERT( m_lpVideoWnd != NULL );
	int nDBCameraID = m_lpVideoWnd->GetDBCameraID();
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetCamera(nDBCameraID, theMapWeb);
	ASSERT( theMapWeb.size() > 0 );
	// ��ȡ��Ҫ����ת��������Ϣ...
	string & strStreamMP4 = theMapWeb["stream_mp4"];
	string & strStreamUrl = theMapWeb["stream_url"];
	//BOOL bStreamAuto = atoi(theMapLoc["stream_auto"].c_str());
	//BOOL bStreamLoop = atoi(theMapLoc["stream_loop"].c_str());
	BOOL bFileMode = ((m_nStreamProp == kStreamMP4File) ? true : false);
	// ֻ�������������֣�������rtmp��������...
	m_lpPushThread = new CPushThread(m_lpVideoWnd->m_hWnd, this);
	m_lpPushThread->StreamInitThread(bFileMode, strStreamUrl, strStreamMP4);
	// ���µ�¼״̬...
	CString strStatus = "��������";
	CString strTitle = theConfig.GetDBCameraTitle(nDBCameraID);
	m_strLogStatus.Format("%s - %s", strStatus, strTitle);
	m_lpVideoWnd->GetRenderWnd()->SetRenderText(m_strLogStatus);
	m_lpVideoWnd->GetRenderWnd()->SetStreamStatus(strStatus);
	return GM_NoErr;
}
//
// ������ת��ģʽ���˳�����...
GM_Error CCamera::doStreamLogout()
{
	// ֱ�ӵ����˳��ӿ�...
	this->doDeviceLogout();
	return GM_NoErr;
}

int CCamera::GetRecvPullKbps()
{
	// 2017.06.12 - by jackey => ���ﷵ��-1����Ҫ����ʾͨ���Ѿ��ر���...
	// �����߳��Ѿ��رգ����ǣ�ͨ��״̬���Ǵ��ڵ�¼���У���Ҫ����-1�����׹ر�...
	if( m_lpPushThread == NULL && this->IsLogin() )
		return - 1;
	// ͨ�����ڵĴ���״̬...
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
