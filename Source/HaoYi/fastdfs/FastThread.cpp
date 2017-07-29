
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
// �������˳���־���ٽ�����Դ�ͷ�...
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
	// �رո��̵߳Ŀ�ת�¼�...
	BOOL bReturn = false;
	bReturn = ::SetEvent(m_hEmptyEvent);
	bReturn = ::CloseHandle(m_hEmptyEvent);
	m_hEmptyEvent = NULL;
	// ����������߳�������Ự��λ...
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
// �Ƴ���ɾ���Ự����
GM_Error CFastThread::DelSession(CFastSession * lpSession)
{
	OS_MUTEX_LOCKER(&m_Mutex);
	if( lpSession == NULL )
		return GM_NoErr;
	// �ڼ����в��һỰ...
	ASSERT( lpSession != NULL );
	WSAEVENT theEvent = lpSession->GetSocketEvent();
	GM_MapEvent::iterator itor = m_MapEvent.find(theEvent);
	// û���ҵ�ָ���Ự,���ش���...
	if( itor == m_MapEvent.end() )
		return GM_Session_None;
	ASSERT( itor != m_MapEvent.end() );
	// �ҵ��Ự,�Ƴ���ɾ��֮...
	m_MapEvent.erase(itor);
	delete lpSession;
	lpSession = NULL;
	return GM_NoErr;
}
//
// �Ӽ������Ƴ��Ự(��ɾ��)...
GM_Error CFastThread::RemoveSession(CFastSession * lpSession)
{
	OS_MUTEX_LOCKER(&m_Mutex);
	if( lpSession == NULL )
		return GM_NoErr;
	ASSERT( lpSession != NULL );
	WSAEVENT theEvent = lpSession->GetSocketEvent();
	GM_MapEvent::iterator itor = m_MapEvent.find(theEvent);
	// �ҵ���ֱ���Ƴ�����...
	if( itor == m_MapEvent.end() )
		return GM_NoErr;
	m_MapEvent.erase(itor);
	return GM_NoErr;
}
//
// ���Ự�Ƿ�ʱ...
/*GM_Error CFastThread::ProTimeOut()
{
	OS_MUTEX_LOCKER(&m_Mutex);
	if( (::timeGetTime() - m_dwCheckTime) < TIMEOUT_STEP_CHECK )
		return GM_NoErr;
	// ÿ��10����ѯ�Ự������Ƿ�ʱ...
	m_dwCheckTime = ::timeGetTime();
	GM_MapEvent::iterator itor;
	CFastSession * lpSession = NULL;
	for(itor = m_MapEvent.begin(); itor != m_MapEvent.end(); ) {
		lpSession = itor->second;
		// �Ựû�г�ʱ�����������һ��...
		if( !lpSession->IsTimeout() ) {
			++itor;
			continue;
		}
		// �ȴӼ������Ƴ���Ŀ����Ϊ�˲�����Ӧ�����¼�֪ͨ,�������Ҫ...			
		lpSession->SetErrorCode(GM_Err_Timeout);
		ASSERT( lpSession->IsTimeout() );
		m_MapEvent.erase(itor++);
		// ֪ͨ�����ɾ����ʱ�Ự...
		::PostMessage(m_hWndMsg, WM_EVENT_SESSION_MSG, OPT_DelSession, (LPARAM)lpSession);			
	}
	return GM_NoErr;
}*/

WSAEVENT CFastThread::WaitEventSession()
{
	int			nCount = 0;
	DWORD		dwIndex = 0;
	WSAEVENT	theEvent = NULL;
	WSAEVENT	arEvents[WSA_MAXIMUM_WAIT_EVENTS] = {0};
	GM_MapEvent::iterator itor;
	// ���Ự��������û�ж���ʱ���ȴ�һ��ʱ��...
	if( m_MapEvent.size() <= 0 ) {
		dwIndex = WaitForSingleObject(m_hEmptyEvent, INFINITE);
		return NULL;
	}
	ASSERT( m_MapEvent.size() > 0 );
	// �����е��¼������ռ���һ�����鵱��...
	do {
		OS_MUTEX_LOCKER(&m_Mutex);
		ASSERT( m_MapEvent.size() <= WSA_MAXIMUM_WAIT_EVENTS );
		for(itor = m_MapEvent.begin(); itor != m_MapEvent.end(); ++itor, ++nCount) {
			arEvents[nCount] = itor->first;
		}
	}while( false );
	if( this->IsStopRequested() )
		return NULL;
	// �ȴ����е�ĳһ���¼��Ĵ���...
	dwIndex = WSAWaitForMultipleEvents(nCount, arEvents, false, DEF_WAIT_TIME, true);
	if( this->IsStopRequested() )
		return NULL;
	if( dwIndex == WSA_WAIT_FAILED )
		return NULL;
	if( dwIndex == WSA_WAIT_TIMEOUT || dwIndex == WAIT_IO_COMPLETION )
		return NULL;
	ASSERT( dwIndex <= nCount );
	// �����¼�������ҵ���Ӧ�ĻỰ����...
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
		// ��ȡһ�����������¼��ĻỰ����...
		WSAEVENT theEvent = this->WaitEventSession();
		if( this->IsStopRequested() )
			break;
		// ���Ự��ʱ(���Բ��ü��)...
		//this->ProTimeOut();
		if( theEvent == NULL )
			continue;
		ASSERT( theEvent != NULL );
		// ��������Ự�ľ��������¼�...
		do {
			OS_MUTEX_LOCKER(&m_Mutex);
			GM_MapEvent::iterator itor;
			itor = m_MapEvent.find(theEvent);
			if( itor == m_MapEvent.end() )
				continue;
			// �ٴβ�������Ự�������Ƿ���Ч...
			CFastSession * lpSession = itor->second;
			try{
				theErr = this->ProcSession(lpSession);
			}
			catch(...){
				theErr = GM_Err_Exception;
				MsgLogGM(theErr);
			}
			// ��������֪ͨ�������Լ�ɾ���Ự...
			// �ȴӼ������Ƴ���Ŀ����Ϊ�˲�����Ӧ�����¼�֪ͨ,�������Ҫ...
			if( theErr != GM_NoErr ) {
				this->RemoveSession(lpSession);
				lpSession->SetErrorCode(theErr);
				::PostMessage(m_hWndMsg, WM_EVENT_SESSION_MSG, OPT_DelSession, (LPARAM)lpSession);
				continue;
			}
			// ���з������󣬼������У��ȴ�����Ự...
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
	// ��ѯ�����¼����Դ�����Ϣ���б�Ҫ�ӹ�����...
	if( SOCKET_ERROR == WSAEnumNetworkEvents(theSocket, theEvent, &theNetwork) ) {
		theErr = WSAGetLastError();
		theErr = (theErr == GM_NoErr) ? GM_NetClose : theErr;
		MsgLogGM(theErr);
		return theErr;
	}
	// û�������¼�����תһ��...
	if( theNetwork.lNetworkEvents == 0 )
		return GM_NoErr;
	// �������¼�������λ������Ŀ�ĵõ������...
	int nEventBit = 0;
	int nEventTemp = theNetwork.lNetworkEvents >> 1;
	while( nEventTemp > 0 ) {
		++nEventBit;
		nEventTemp >>= 1;
	}
	ASSERT( nEventBit >= 0 );
	// ����㷢����FD_CLOSE�¼�...
	if( theNetwork.lNetworkEvents & FD_CLOSE ) {
		theErr = GM_NetClose;
		MsgLogGM(theErr);
		return theErr;
	}
	// �����¼��д�����Ϣ...
	if( theNetwork.iErrorCode[nEventBit] > 0 ) {
		theErr = theNetwork.iErrorCode[nEventBit];
		MsgLogGM(theErr);
		return theErr;
	}
	// ִ��û�д���������¼�...
	ASSERT( theErr == GM_NoErr );
	return lpSession->ProcessEvent(theNetwork.lNetworkEvents);
}
