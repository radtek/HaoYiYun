
#pragma once

#include "OSMutex.h"
#include "OSThread.h"

class CFastSession;

typedef map<WSAEVENT, CFastSession *>	GM_MapEvent;		// 事件对象集合
#define TIMEOUT_STEP_CHECK				10000				// 超时检测间隔时间(毫秒)
#define DEF_WAIT_TIME					500					// 默认事件线程等待时间(毫秒)

class CFastThread : public OSThread
{
public:
	CFastThread(HWND hWndMsg, int nMaxWait = 20);			// 默认单线程支持20个链接
	virtual ~CFastThread();
public:
	HWND			GetHWndMsg() { return m_hWndMsg; }		// 返回窗口消息句柄
	int				GetMapSize();							// 得到当前处理数量
	BOOL			IsOverFlow();							// 是否过载
	GM_Error		Initialize();							// 初始化
	GM_Error		UnInitialize();							// 反初始化
	GM_Error		AddSession(CFastSession * lpSession);	// 加入会话集合
	GM_Error		DelSession(CFastSession * lpSession);	// 删除会话集合
private:
	virtual	void	Entry();								// 事件线程函数
	WSAEVENT		WaitEventSession();						// 等待一个会话
	GM_Error		ProcSession(CFastSession * lpSession);	// 处理一个会话
	GM_Error		RemoveSession(CFastSession * lpSession);// 从集合中移除会话(不删除)

	GM_Error		ProTimeOut();
private:
	int				m_nMaxWait;								// 能够支持的最大链接数
	OSMutex			m_Mutex;								// 线程互斥对象
	GM_MapEvent		m_MapEvent;								// 事件会话集合
	HANDLE			m_hEmptyEvent;							// 线程轮空时的事件对象
	DWORD			m_dwCheckTime;							// 超时检测时间
	HWND			m_hWndMsg;								// 消息通知窗口句柄
};
