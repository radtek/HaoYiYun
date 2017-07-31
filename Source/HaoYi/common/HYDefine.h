
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

#define LOCAL_ADDRESS_V4			"127.0.0.1"				// 本地地址定义-IPV4
#define LOCAL_ADDRESS_V6			"[::1]"					// 本地地址定义-IPV6
#define LINGER_TIME					500						// SOCKET停止时数据链路层BUFF清空的最大延迟时间
#define LOCAL_LOOP_ADDR_V6			"fe80::1"				// 本地地址定义-IPV6
#define LOCAL_LOOP1_ADDR_V6			"::1"					// 本地地址定义-IPV6
#define LOCAL_ERR_ADDR_V6			"fe80::ffff:ffff:fffd"	// 本地地址定义-IPV6

#define SNAP_TOOL_NAME				"mplayer.exe"			// 截图工具名称
#define XML_CONFIG					"Config.xml"			// 中心服务器配置文件(GB2312)
#define SERVER_NAME					"HYServer"				// Server name
#define TXT_EXCEPT					"Except.txt"			// 异常文件
#define TXT_LOGGER					"Logger.txt"			// 日志文件
#define XML_DECLARE_GB2312			"<?xml version=\"1.0\" encoding=\"GB2312\"?>"
#define XML_DECLARE_UTF8			"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
#define DEF_REC_PATH				"C:\\GMSave"			// 默认录像目录
#define DEF_MAIN_NAME				"采集器"				// 默认主窗口标题
#define DEF_LOGIN_USER_HK			"admin"					// 默认的登录用户 => 海康网络摄像机
#define DEF_LOGIN_PASS_HK			"12345"					// 默认的登录密码 => 海康网络摄像机
#define DEF_SAVE_PATH				"GMSave"				// 默认的存盘路径
#define DEF_MAIN_KBPS				1024					// 默认的主码流Kbps => 录像和截图
#define DEF_SUB_KBPS				512						// 默认的子码流Kbps => 实时直播上传
#define DEF_SNAP_STEP				5						// 默认的截图间隔秒数 => .jpg
#define DEF_REC_SLICE				10 * 60					// 默认的录像切片秒数 => .mp4
#define DEF_NETSESSION_TIMEOUT		2						// 默认网络会话超时时间(分钟)
#define DEF_CAMERA_START_ID			1						// 默认摄像头开始ID
#define DEF_MAX_CAMERA              16						// 默认最大摄像头数目
#define DEF_WEB_ADDR				"192.168.1.180"			// Web默认地址
#define DEF_WEB_PORT				80						// Web默认端口 

#define VIDEO_TIME_SCALE			90000					// 视频时间刻度，刻度越大好像越精确???

#define QR_CODE_CX					270						// 二维码窗口宽度
#define QR_CODE_CY					270						// 二维码窗口高度
#define	WND_BK_COLOR				RGB(64,64,64)			// 窗口背景色

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

#define WM_ERR_TASK_MSG				(WM_USER + 501)			// 录像任务会话通知消息
#define WM_ERR_PUSH_MSG				(WM_USER + 502)			// 拉转推会话通知消息
#define WM_REC_SLICE_MSG			(WM_USER + 503)			// 录像切片通知消息
#define WM_STOP_STREAM_MSG			(WM_USER + 504)			// 停止流上传通知消息
#define WM_EVENT_SESSION_MSG		(WM_USER + 505)			// 事件会话通知消息

typedef	enum AUTH_STATE {
	kAuthRegiter	= 1,		// 网站注册授权
	kAuthExpired	= 2,		// 授权过期验证
};

typedef enum OPT_PARAM {		// WPARAM参数定义
	OPT_DelSession	= 1,		// 删除会话通知(所有)
};

typedef enum LOG_TXT {
	kTxtLogger		= 0,		// 日志文件类型
	kTxtExcept		= 1,		// 异常文件类型
};

typedef enum LOG_LEVEL			// 日志层次信息
{
	kLogINFO		= 0,		// 常规信息日志
	kLogSYS			= 1,		// API调用错误日志
	kLogGM			= 2,		// 内部逻辑错误日志
};

typedef enum STREAM_PROP
{
	kStreamDevice	= 0,		// 摄像头设备流类型 => camera
	kStreamMP4File	= 1,		// MP4文件流类型    => .mp4
	kStreamUrlLink	= 2,		// URL链接流类型    => rtsp/rtmp
};

typedef enum CAMERA_TYPE		// 网络摄像机类型
{
	kCameraNO	= 0,			// 没有摄像机
	kCameraHK	= 1,			// 海康摄像机
	kCameraDH	= 2,			// 大华摄像机
};

typedef enum CAMERA_STATE
{
  kCameraWait   = 0,			// 未连接
  kCameraRun    = 1,			// 运行中
  kCameraRec    = 2,			// 录像中
};

typedef enum WEB_TYPE
{
	kCloudRecorder	= 0,		// 云录播
	kCloudMonitor   = 1,		// 云监控
};