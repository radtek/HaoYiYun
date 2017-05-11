
#include "StdAfx.h"
#include "UtilTool.h"
#include "FastThread.h"
#include "FastSession.h"

#include <time.h>
#include <MMSystem.h>

#include "SocketUtils.h"
#include "StringFormatter.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CFastThread::CFastThread(HWND hWndMsg, int nMaxWait /* = 20 */)
  : m_nMaxWait(nMaxWait)
  , m_hEmptyEvent(NULL)
  , m_hWndMsg(hWndMsg)
  , m_Mutex(false)
{
	m_nMaxWait = ((nMaxWait <= 0) ? 20 : nMaxWait);
	m_nMaxWait = min(m_nMaxWait, WSA_MAXIMUM_WAIT_EVENTS);
	m_hEmptyEvent = ::CreateEvent(NULL, false, false, NULL);
	ASSERT( m_hEmptyEvent != NULL );
	ASSERT( m_hWndMsg != NULL );

	m_dwCheckTime = ::timeGetTime();
}
//
// 先设置退出标志，再进行资源释放...
CFastThread::~CFastThread()
{
	this->SendStopRequest();
	this->UnInitialize();
	this->StopAndWaitForThread();
}

int CFastThread::GetMapSize()
{
	OS_MUTEX_LOCKER(&m_Mutex);
	return m_MapEvent.size();
}

BOOL CFastThread::IsOverFlow()
{
	OS_MUTEX_LOCKER(&m_Mutex);
	return ((m_MapEvent.size() >= m_nMaxWait) ? true : false);
}

GM_Error CFastThread::Initialize()
{
	this->Start();
	return GM_NoErr;
}

GM_Error CFastThread::UnInitialize()
{
	OS_MUTEX_LOCKER(&m_Mutex);
	// 关闭该线程的空转事件...
	BOOL bReturn = false;
	bReturn = ::SetEvent(m_hEmptyEvent);
	bReturn = ::CloseHandle(m_hEmptyEvent);
	m_hEmptyEvent = NULL;
	// 将所有与该线程相关联会话复位...
	GM_MapEvent::iterator itor;
	CFastSession * lpSession = NULL;
	for(itor = m_MapEvent.begin(); itor != m_MapEvent.end(); ++itor ) {
		lpSession = itor->second; ASSERT( lpSession != NULL );
		lpSession->SetEventThread(NULL);
	}
	m_MapEvent.clear();
	return GM_NoErr;
}

GM_Error CFastThread::AddSession(CFastSession * lpSession)
{
	OS_MUTEX_LOCKER(&m_Mutex);
	GM_Error theErr = GM_OverFlow;
	if( lpSession == NULL || m_MapEvent.size() >= m_nMaxWait ) {
		MsgLogGM(theErr);
		return theErr;
	}
	WSAEVENT theEvent = lpSession->GetSocketEvent();
	GM_MapEvent::iterator itor = m_MapEvent.find(theEvent);
	if( itor != m_MapEvent.end() ) {
		MsgLogGM(theErr);
		return theErr;
	}
	BOOL bReturn = false;
	ASSERT( itor == m_MapEvent.end() );
	m_MapEvent[theEvent] = lpSession;
	lpSession->SetEventThread(this);
	bReturn = ::SetEvent(m_hEmptyEvent);
	return GM_NoErr;
}
//
// 移出并删除会话集合
GM_Error CFastThread::DelSession(CFastSession * lpSession)
{
	OS_MUTEX_LOCKER(&m_Mutex);
	if( lpSession == NULL )
		return GM_NoErr;
	// 在集合中查找会话...
	ASSERT( lpSession != NULL );
	WSAEVENT theEvent = lpSession->GetSocketEvent();
	GM_MapEvent::iterator itor = m_MapEvent.find(theEvent);
	// 没有找到指定会话,返回错误...
	if( itor == m_MapEvent.end() )
		return GM_Session_None;
	ASSERT( itor != m_MapEvent.end() );
	// 找到会话,移除并删除之...
	m_MapEvent.erase(itor);
	delete lpSession;
	lpSession = NULL;
	return GM_NoErr;
}
//
// 从集合中移出会话(不删除)...
GM_Error CFastThread::RemoveSession(CFastSession * lpSession)
{
	OS_MUTEX_LOCKER(&m_Mutex);
	if( lpSession == NULL )
		return GM_NoErr;
	ASSERT( lpSession != NULL );
	WSAEVENT theEvent = lpSession->GetSocketEvent();
	GM_MapEvent::iterator itor = m_MapEvent.find(theEvent);
	// 找到后直接移出集合...
	if( itor == m_MapEvent.end() )
		return GM_NoErr;
	m_MapEvent.erase(itor);
	return GM_NoErr;
}
//
// 检测会话是否超时...
GM_Error CFastThread::ProTimeOut()
{
	OS_MUTEX_LOCKER(&m_Mutex);
	if( (::timeGetTime() - m_dwCheckTime) < TIMEOUT_STEP_CHECK )
		return GM_NoErr;
	// 每隔10秒轮询会话，检测是否超时...
	m_dwCheckTime = ::timeGetTime();
	GM_MapEvent::iterator itor;
	CFastSession * lpSession = NULL;
	for(itor = m_MapEvent.begin(); itor != m_MapEvent.end(); ) {
		lpSession = itor->second;
		// 会话没有超时，继续检测下一个...
		if( !lpSession->IsTimeout() ) {
			++itor;
			continue;
		}
		// 先从集合中移除的目的是为了不再响应网络事件通知,这个很重要...			
		lpSession->SetErrorCode(GM_Err_Timeout);
		ASSERT( lpSession->IsTimeout() );
		m_MapEvent.erase(itor++);
		// 通知界面层删除超时会话...
		::PostMessage(m_hWndMsg, WM_EVENT_SESSION_MSG, OPT_DelSession, (LPARAM)lpSession);			
	}
	return GM_NoErr;
}

WSAEVENT CFastThread::WaitEventSession()
{
	int			nCount = 0;
	DWORD		dwIndex = 0;
	WSAEVENT	theEvent = NULL;
	WSAEVENT	arEvents[WSA_MAXIMUM_WAIT_EVENTS] = {0};
	GM_MapEvent::iterator itor;
	// 当会话集合里面没有对象时，等待一段时间...
	if( m_MapEvent.size() <= 0 ) {
		dwIndex = WaitForSingleObject(m_hEmptyEvent, INFINITE);
		return NULL;
	}
	ASSERT( m_MapEvent.size() > 0 );
	// 将所有的事件对象收集到一个数组当中...
	do {
		OS_MUTEX_LOCKER(&m_Mutex);
		ASSERT( m_MapEvent.size() <= WSA_MAXIMUM_WAIT_EVENTS );
		for(itor = m_MapEvent.begin(); itor != m_MapEvent.end(); ++itor, ++nCount) {
			arEvents[nCount] = itor->first;
		}
	}while( false );
	if( this->IsStopRequested() )
		return NULL;
	// 等待其中的某一个事件的触发...
	dwIndex = WSAWaitForMultipleEvents(nCount, arEvents, false, DEF_WAIT_TIME, true);
	if( this->IsStopRequested() )
		return NULL;
	if( dwIndex == WSA_WAIT_FAILED )
		return NULL;
	if( dwIndex == WSA_WAIT_TIMEOUT || dwIndex == WAIT_IO_COMPLETION )
		return NULL;
	ASSERT( dwIndex <= nCount );
	// 根据事件对象查找到相应的会话对象...
	do {
		OS_MUTEX_LOCKER(&m_Mutex);
		theEvent = arEvents[dwIndex - WSA_WAIT_EVENT_0];
		itor = m_MapEvent.find(theEvent);
		if( itor == m_MapEvent.end() )
			return NULL;
		ASSERT(itor->first == theEvent);
		return theEvent;
	}while( false );
	return NULL;
}

void CFastThread::Entry()
{
	GM_Error theErr = GM_NoErr;
	while( !this->IsStopRequested() ) {
		// 获取一个发生网络事件的会话对象...
		WSAEVENT theEvent = this->WaitEventSession();
		if( this->IsStopRequested() )
			break;
		// 检测会话超时(可以不用检测)...
		//this->ProTimeOut();
		if( theEvent == NULL )
			continue;
		ASSERT( theEvent != NULL );
		// 处理这个会话的具体网络事件...
		do {
			OS_MUTEX_LOCKER(&m_Mutex);
			GM_MapEvent::iterator itor;
			itor = m_MapEvent.find(theEvent);
			if( itor == m_MapEvent.end() )
				continue;
			// 再次查找这个会话，看看是否有效...
			CFastSession * lpSession = itor->second;
			try{
				theErr = this->ProcSession(lpSession);
			}
			catch(...){
				theErr = GM_Err_Exception;
				MsgLogGM(theErr);
			}
			// 发生错误通知创建者自己删除会话...
			// 先从集合中移除的目的是为了不再响应网络事件通知,这个很重要...
			if( theErr != GM_NoErr ) {
				this->RemoveSession(lpSession);
				lpSession->SetErrorCode(theErr);
				::PostMessage(m_hWndMsg, WM_EVENT_SESSION_MSG, OPT_DelSession, (LPARAM)lpSession);
				continue;
			}
			// 诶有发生错误，继续运行，等待网络会话...
			ASSERT( lpSession != NULL );
			
		}while( false );		
	}
}

GM_Error CFastThread::ProcSession(CFastSession * lpSession)
{
	GM_Error theErr = GM_NoErr;
	SOCKET	 theSocket = lpSession->GetSocket();
	WSAEVENT theEvent = lpSession->GetSocketEvent();
	WSANETWORKEVENTS theNetwork = {0};
	// 查询网络事件，对错误信息进行必要加工处理...
	if( SOCKET_ERROR == WSAEnumNetworkEvents(theSocket, theEvent, &theNetwork) ) {
		theErr = WSAGetLastError();
		theErr = (theErr == GM_NoErr) ? GM_NetClose : theErr;
		MsgLogGM(theErr);
		return theErr;
	}
	// 没有网络事件，空转一次...
	if( theNetwork.lNetworkEvents == 0 )
		return GM_NoErr;
	// 对网络事件进行移位操作，目的得到错误号...
	int nEventBit = 0;
	int nEventTemp = theNetwork.lNetworkEvents >> 1;
	while( nEventTemp > 0 ) {
		++nEventBit;
		nEventTemp >>= 1;
	}
	ASSERT( nEventBit >= 0 );
	// 网络层发生了FD_CLOSE事件...
	if( theNetwork.lNetworkEvents & FD_CLOSE ) {
		theErr = GM_NetClose;
		MsgLogGM(theErr);
		return theErr;
	}
	// 网络事件有错误信息...
	if( theNetwork.iErrorCode[nEventBit] > 0 ) {
		theErr = theNetwork.iErrorCode[nEventBit];
		MsgLogGM(theErr);
		return theErr;
	}
	// 执行没有错误的网络事件...
	ASSERT( theErr == GM_NoErr );
	return lpSession->ProcessEvent(theNetwork.lNetworkEvents);
}
