
#pragma once

#include <windows.h>
#include <windowsx.h>
#include <winsock2.h>
#include <mswsock.h>
#include <process.h>
#include <ws2tcpip.h>
#include <io.h>
#include <direct.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

//
// Basic data type redefine for Apple Code.
//
typedef unsigned char		UInt8;
typedef signed char			SInt8;
typedef unsigned short		UInt16;
typedef signed short		SInt16;
typedef unsigned long		UInt32;
typedef signed long			SInt32;
typedef LONGLONG			SInt64;
typedef ULONGLONG			UInt64;
typedef float				Float32;
typedef double				Float64;
typedef UInt16				Bool16;
typedef UInt8				Bool8;

#pragma warning(disable: 4786)

#include <map>
#include <list>
#include <deque>
#include <string>
#include <vector>
#include <algorithm>
#include <hash_map>

using namespace std;

#include "HYVersion.h"
#include "GMError.h"

#define LOCAL_ADDRESS_V4			"127.0.0.1"				// ���ص�ַ����-IPV4
#define LOCAL_ADDRESS_V6			"[::1]"					// ���ص�ַ����-IPV6
#define LINGER_TIME					500						// SOCKETֹͣʱ������·��BUFF��յ�����ӳ�ʱ��
#define LOCAL_LOOP_ADDR_V6			"fe80::1"				// ���ص�ַ����-IPV6
#define LOCAL_LOOP1_ADDR_V6			"::1"					// ���ص�ַ����-IPV6
#define LOCAL_ERR_ADDR_V6			"fe80::ffff:ffff:fffd"	// ���ص�ַ����-IPV6

#define SNAP_TOOL_NAME				"mplayer.exe"			// ��ͼ��������
#define XML_CONFIG					"Config.xml"			// ���ķ����������ļ�(GB2312)
#define SERVER_NAME					"HYServer"				// Server name
#define TXT_EXCEPT					"Except.txt"			// �쳣�ļ�
#define TXT_LOGGER					"Logger.txt"			// ��־�ļ�
#define XML_DECLARE_GB2312			"<?xml version=\"1.0\" encoding=\"GB2312\"?>"
#define XML_DECLARE_UTF8			"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
#define DEF_REC_PATH				"C:\\GMSave"			// Ĭ��¼��Ŀ¼
#define DEF_MAIN_NAME				"�ɼ���"				// Ĭ�������ڱ���
#define DEF_LOGIN_USER_HK			"admin"					// Ĭ�ϵĵ�¼�û� => �������������
#define DEF_LOGIN_PASS_HK			"12345"					// Ĭ�ϵĵ�¼���� => �������������
#define DEF_SAVE_PATH				"GMSave"				// Ĭ�ϵĴ���·��
#define DEF_MAIN_KBPS				1024					// Ĭ�ϵ�������Kbps => ¼��ͽ�ͼ
#define DEF_SUB_KBPS				512						// Ĭ�ϵ�������Kbps => ʵʱֱ���ϴ�
#define DEF_SNAP_STEP				5						// Ĭ�ϵĽ�ͼ������� => .jpg
#define DEF_REC_SLICE				10 * 60					// Ĭ�ϵ�¼����Ƭ���� => .mp4
#define DEF_NETSESSION_TIMEOUT		2						// Ĭ������Ự��ʱʱ��(����)
#define DEF_CAMERA_START_ID			1						// Ĭ������ͷ��ʼID
#define DEF_MAX_CAMERA              16						// Ĭ���������ͷ��Ŀ
#define DEF_WEB_ADDR				"192.168.1.180"			// WebĬ�ϵ�ַ
#define DEF_WEB_PORT				80						// WebĬ�϶˿� 

#define VIDEO_TIME_SCALE			90000					// ��Ƶʱ��̶ȣ��̶�Խ�����Խ��ȷ???

#define QR_CODE_CX					270						// ��ά�봰�ڿ��
#define QR_CODE_CY					270						// ��ά�봰�ڸ߶�
#define	WND_BK_COLOR				RGB(64,64,64)			// ���ڱ���ɫ

#define _COLOR_KEY					RGB(10, 0, 10)
#define _COLOR_BK					RGB(96, 96, 96)

#define ID_RIGHT_VIEW_BEGIN         13000
#define ID_VIDEO_WND_BEGIN          15000
#define ID_RENDER_WND_BEGIN			16000

#define KHSS_MIN_VIDEO_WIDTH		180
#define KHSS_MIN_VIDEO_HEIGHT		100

#define KHSS_LEFT_VIEW_WIDTH		300	
#define KHSS_RIGHT_VIEW_WIDTH		240
#define KHSS_VIDEO_STEP				240

#define WM_POPUP_CHANNEL_MENU		(WM_USER + 101)
#define WM_SELECT_CHANNEL			(WM_USER + 102)
#define WM_FIND_HK_CAMERA			(WM_USER + 103)
#define WM_FIND_DH_CAMERA			(WM_USER + 104)
#define WM_FOCUS_VIDEO				(WM_USER + 105)
#define WM_BUTTON_LDOWNUP			(WM_USER + 106)
#define WM_DVR_LOGIN_RESULT			(WM_USER + 107)
#define WM_WEB_LOAD_RESOURCE		(WM_USER + 108)
#define	WM_WEB_UPDATE_NAME			(WM_USER + 109)
#define	WM_WEB_AUTH_RESULT			(WM_USER + 110)

#define WM_ERR_TASK_MSG				(WM_USER + 501)			// ¼������Ự֪ͨ��Ϣ
#define WM_ERR_PUSH_MSG				(WM_USER + 502)			// ��ת�ƻỰ֪ͨ��Ϣ
#define WM_REC_SLICE_MSG			(WM_USER + 503)			// ¼����Ƭ֪ͨ��Ϣ
#define WM_STOP_STREAM_MSG			(WM_USER + 504)			// ֹͣ���ϴ�֪ͨ��Ϣ
#define WM_EVENT_SESSION_MSG		(WM_USER + 505)			// �¼��Ự֪ͨ��Ϣ

typedef	enum AUTH_STATE {
	kAuthRegiter	= 1,		// ��վע����Ȩ
	kAuthExpired	= 2,		// ��Ȩ������֤
};

typedef enum OPT_PARAM {		// WPARAM��������
	OPT_DelSession	= 1,		// ɾ���Ự֪ͨ(����)
};

typedef enum LOG_TXT {
	kTxtLogger		= 0,		// ��־�ļ�����
	kTxtExcept		= 1,		// �쳣�ļ�����
};

typedef enum LOG_LEVEL			// ��־�����Ϣ
{
	kLogINFO		= 0,		// ������Ϣ��־
	kLogSYS			= 1,		// API���ô�����־
	kLogGM			= 2,		// �ڲ��߼�������־
};

typedef enum STREAM_PROP
{
	kStreamDevice	= 0,		// ����ͷ�豸������ => camera
	kStreamMP4File	= 1,		// MP4�ļ�������    => .mp4
	kStreamUrlLink	= 2,		// URL����������    => rtsp/rtmp
};

typedef enum CAMERA_TYPE		// �������������
{
	kCameraNO	= 0,			// û�������
	kCameraHK	= 1,			// ���������
	kCameraDH	= 2,			// �������
};

typedef enum CAMERA_STATE
{
  kCameraWait   = 0,			// δ����
  kCameraRun    = 1,			// ������
  kCameraRec    = 2,			// ¼����
};

typedef enum WEB_TYPE
{
	kCloudRecorder	= 0,		// ��¼��
	kCloudMonitor   = 1,		// �Ƽ��
};