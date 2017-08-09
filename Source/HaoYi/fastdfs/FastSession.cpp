
#include "StdAfx.h"
#include "UtilTool.h"
#include "FastThread.h"
#include "FastSession.h"
#include "StringParser.h"
#include "VideoWnd.h"
#include "..\HaoYiView.h"
#include "..\XmlConfig.h"
#include "..\Camera.h"
#include <curl.h>

void long2buff(LONGLONG n, char *buff)
{
	unsigned char *p;
	p = (unsigned char *)buff;
	*p++ = (n >> 56) & 0xFF;
	*p++ = (n >> 48) & 0xFF;
	*p++ = (n >> 40) & 0xFF;
	*p++ = (n >> 32) & 0xFF;
	*p++ = (n >> 24) & 0xFF;
	*p++ = (n >> 16) & 0xFF;
	*p++ = (n >> 8) & 0xFF;
	*p++ = n & 0xFF;
}

LONGLONG buff2long(const char *buff)
{
	unsigned char *p;
	p = (unsigned char *)buff;
	return  (((LONGLONG)(*p)) << 56) | \
		(((LONGLONG)(*(p+1))) << 48) |  \
		(((LONGLONG)(*(p+2))) << 40) |  \
		(((LONGLONG)(*(p+3))) << 32) |  \
		(((LONGLONG)(*(p+4))) << 24) |  \
		(((LONGLONG)(*(p+5))) << 16) |  \
		(((LONGLONG)(*(p+6))) << 8) | \
		((LONGLONG)(*(p+7)));
}

CFastSession::CFastSession()
  : m_nXErrorCode(GM_NoErr)
  , m_lpEventThread(NULL)
  , m_eState(kNoneState)
  , m_iRemotePort(0)
  , m_iTimeout( 0 )
  , m_hWndMsg(NULL)
{
	memset(&m_llSysFreq, 0, sizeof(m_llSysFreq));
	QueryPerformanceFrequency(&m_llSysFreq);
	ASSERT( m_llSysFreq.QuadPart > 0 );
	this->ResetTimeout();
	ASSERT( m_iTimeout > 0 );
}

CFastSession::~CFastSession(void)
{
	this->DisConnect();
}

void CFastSession::SetEventThread(CFastThread * lpThread)
{
	m_lpEventThread = lpThread;
	if( lpThread == NULL )
		return;
	ASSERT( lpThread != NULL );
	m_hWndMsg = lpThread->GetHWndMsg();
}

GM_Error CFastSession::InitSession(LPCTSTR lpszAddr, int nPort)
{
	ASSERT( m_eState == kNoneState );
	GM_Error theErr = GM_URIErr;
	// �����������Ϣ...
	m_iRemotePort = nPort;
	m_strRemoteAddr = lpszAddr;
	m_strRemoteIP = m_strRemoteAddr;
	if( m_strRemoteIP.size() <= 0 || m_iRemotePort <= 0 ) {
		MsgLogGM(theErr);
		return theErr;
	}
	// �����µ��׽���...
	theErr = m_TCPSocket.Open(false);
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
		return theErr;
	}
	// ���ñ�������״̬ => ÿ��5�뷢��������...
	m_TCPSocket.KeepAlive();
	// ���ý��ջ��岢�����¼�����...
	m_TCPSocket.SetSocketRcvBufSize(128*1024);
	m_TCPSocket.SetSocketRcvBufSize(128*1024);
	theErr = m_TCPSocket.CreateEvent();
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
		return theErr;
	}
	// ��䱾�������Ϣ...
	ASSERT(theErr == GM_NoErr);
	//this->BuildClientPragma();
	m_eState = kOpenState;
	// ʹ�ÿ����������ĵ�ַ�������ӷ�����...
	return this->OpenConnect(m_strRemoteIP, m_iRemotePort);
}

GM_Error CFastSession::OpenConnect(const string & strAddr, int nPort)
{
	return this->OpenConnect(strAddr.c_str(), nPort);
}

GM_Error CFastSession::OpenConnect(LPCTSTR lpszAddr, int nPort)
{
	//UInt32 inRemoteAddr = ntohl(::inet_addr(lpszAddr));
	//return m_TCPSocket.ConnectV4(inRemoteAddr, nPort);

	// ���������ַ��һ����ʱ����...
	LPCTSTR lpszRemoteIP = lpszAddr;
	TCHAR szHostAddr[MAX_PATH] = {0};
	// ͨ������ֻ�ܻ�ȡIPV4�ĵ�ַ��IPV6ֻ�������ַ...
	// ����������Ϣ����һ��IPV4������IPV6��������������...
	hostent * lpHost = gethostbyname(lpszAddr);
	if( lpHost != NULL && lpHost->h_addr_list != NULL ) {
		LPSTR lpszTemp = inet_ntoa(*(in_addr*)lpHost->h_addr_list[0]);
		(lpszTemp != NULL) ? strcpy(szHostAddr, lpszTemp) : NULL;
		// ����������˵�ַ���򱣴���Ч��IP��ַ����ʹ��IP��ַ���ӷ�����...
		m_strRemoteIP.assign(szHostAddr, strlen(szHostAddr));
		lpszRemoteIP = szHostAddr;
	}
	// ͨ��IP��ַ���ӷ�����...
	return m_TCPSocket.Connect(lpszRemoteIP, nPort);
}

GM_Error CFastSession::DisConnect()
{
	m_TCPSocket.Close();
	m_eState = kNoneState;
	return GM_NoErr;
}
//
// ��Ӧ�����¼�...
GM_Error CFastSession::ProcessEvent(int eventBits)
{
	GM_Error theErr = GM_NoErr;
	// ���������¼�֪ͨ...
	if( eventBits & FD_CONNECT ) {
		theErr = this->OnConnectEvent();
		if( theErr != GM_NoErr ) {
			MsgLogGM(theErr);
			return theErr;
		}
		eventBits &= ~ FD_CONNECT;
	}
	// �����ȡ�¼�֪ͨ...
	if( eventBits & FD_READ ) {
		theErr = this->OnReadEvent();
		if( theErr != GM_NoErr ) {
			MsgLogGM(theErr);
			return theErr;
		}
		eventBits &= ~ FD_READ;
	}
	// �����ȡ�¼�֪ͨ...
	if( eventBits & FD_WRITE ) {
		eventBits &= ~ FD_WRITE;
	}
	// ���������������¼�֪ͨ...
	ASSERT( eventBits == 0 );
	return GM_NoErr;
}

GM_Error CFastSession::OnConnectEvent()
{
	m_eState = kConnectState;
	return this->ForConnect();
}

GM_Error CFastSession::OnReadEvent()
{
	GM_Error theErr = GM_NoErr;
	UInt32	 theProcessed = 0;
	UInt32	 theCurRead = 0;
	string	 strRead;
	// ��ȡ��ǰ���е���������...
	theErr = m_TCPSocket.Read(strRead, &theCurRead);
	if( theErr == WSAEWOULDBLOCK ) {
		::Sleep(5);
		return GM_NoErr;
	}
	// ����������ϻ��ȡ����Ϊ��...
	if( theErr != GM_NoErr || strRead.size() <= 0 ) {
		MsgLogGM(theErr);
		return theErr;
	}
	// ׷�Ӷ�ȡ���ݲ��������ͷָ��...
	m_strRecv.append(strRead);
	// �ַ�������Session���д���...
	return this->ForRead();
}
//
// �жϻỰ�Ƿ�ʱ...
BOOL CFastSession::IsTimeout()
{
	ASSERT( m_iTimeout > 0 );
	return ((m_iTimeout >= this->GetFrequeTime() ) ? false : true);
}
//
// ���ó�ʱ���...
void CFastSession::ResetTimeout()
{
	m_iTimeout = this->GetFrequeTime() + DEF_NETSESSION_TIMEOUT * 60 * 1000;
}
//
// �õ���ǰ��ʱ��(����)
DWORD CFastSession::GetFrequeTime()
{
	BOOL			bRet = FALSE;
	LARGE_INTEGER	llTimeCount = {0};

	ASSERT( m_llSysFreq.QuadPart > 0 );
	bRet = QueryPerformanceCounter(&llTimeCount);
	ASSERT( bRet && llTimeCount.QuadPart > 0 );
	return (llTimeCount.QuadPart * 1000 / m_llSysFreq.QuadPart);
}

//////////////////////////////////////////////////
// Tracker�Ựʵ������...
//////////////////////////////////////////////////
CTrackerSession::CTrackerSession()
{
	m_nGroupCount = 0;
	m_lpGroupStat = NULL;
	memset(&m_NewStorage, 0, sizeof(m_NewStorage));
	memset(&m_TrackerCmd, 0, sizeof(m_TrackerCmd));
}

CTrackerSession::~CTrackerSession()
{
	if( m_lpGroupStat != NULL ) {
		delete [] m_lpGroupStat;
		m_lpGroupStat = NULL;
		m_nGroupCount = 0;
	}
	// ��¼Tracker�˳���Ϣ...
	MsgLogINFO("[~CTrackerSession] - Exit");
}
//
// ��������...
GM_Error CTrackerSession::SendCmd(char inCmd)
{
	// �ж��Ƿ�������״̬��ֻ������״̬�ŷ��������...
	if( m_eState != kConnectState )
		return GM_NotImplement;
	ASSERT( m_eState == kConnectState );
	// ֱ���������ͳ�ȥ...
	UInt32	nReturn = 0;
	int     nSendSize = sizeof(m_TrackerCmd);
	memset(&m_TrackerCmd, 0, nSendSize);
	m_TrackerCmd.cmd = inCmd;
	return m_TCPSocket.Send((char*)&m_TrackerCmd, nSendSize, &nReturn);
}

GM_Error CTrackerSession::ForConnect()
{
	ASSERT( m_eState == kConnectState );
	// ���ӳɹ������Ͳ�ѯstorage���� => Tracker�᷵��һ������ʵ�Storage...
	return this->SendCmd(TRACKER_PROTO_CMD_SERVICE_QUERY_STORE_WITHOUT_GROUP_ONE);
}

GM_Error CTrackerSession::ForRead()
{
	// �õ������ݳ��Ȳ�����ֱ�ӷ��أ��ȴ�������...
	int nCmdLength = sizeof(TrackerHeader);
	if( m_strRecv.size() < nCmdLength )
		return GM_NoErr;
	ASSERT( m_strRecv.size() >= nCmdLength );
	// ������ͷ���зַ�����...
	if( m_TrackerCmd.cmd == TRACKER_PROTO_CMD_SERVICE_QUERY_STORE_WITHOUT_GROUP_ONE ) {
		// �õ������ݳ��Ȳ�����ֱ�ӱ����ȴ��µ����ӽ��룬Storage���ߺ󲻻������㱨...
		if( m_strRecv.size() < (nCmdLength + TRACKER_QUERY_STORAGE_STORE_BODY_LEN) ) {
			MsgLogGM(GM_NetNoData);
			return GM_NetNoData;
		}
		// �Ի�ȡ�����ݽ���ת�ƴ���...
		char * in_buff = (char*)m_strRecv.c_str() + nCmdLength;
		memset(&m_NewStorage, 0, sizeof(m_NewStorage));
		memcpy(m_NewStorage.group_name, in_buff, FDFS_GROUP_NAME_MAX_LEN);
		memcpy(m_NewStorage.ip_addr, in_buff + FDFS_GROUP_NAME_MAX_LEN, IP_ADDRESS_SIZE-1);
		m_NewStorage.port = (int)buff2long(in_buff + FDFS_GROUP_NAME_MAX_LEN + IP_ADDRESS_SIZE - 1);
		m_NewStorage.store_path_index = (int)(*(in_buff + FDFS_GROUP_NAME_MAX_LEN + IP_ADDRESS_SIZE - 1 + FDFS_PROTO_PKG_LEN_SIZE)); // �ڴ���ֻ��һ���ֽ�...
		TRACE("Group = %s, Storage = %s:%d, PathIndex = %d\n", m_NewStorage.group_name, m_NewStorage.ip_addr, m_NewStorage.port, m_NewStorage.store_path_index);
		// �����������м��ٴ���...
		m_strRecv.erase(0, nCmdLength + TRACKER_QUERY_STORAGE_STORE_BODY_LEN);
		// ֪ͨ��Ϣ���ڣ����Դ����µĴ洢�Ự�����������ϴ��Ĳ�����...
		//::PostMessage(m_hWndMsg, WM_EVENT_SESSION_MSG, OPT_OpenStorage, (LPARAM)&m_NewStorage);
		// �����µ����� => ��ѯ���е����б�...
		return this->SendCmd(TRACKER_PROTO_CMD_SERVER_LIST_ALL_GROUPS);
	} else if( m_TrackerCmd.cmd == TRACKER_PROTO_CMD_SERVER_LIST_ALL_GROUPS ) {
		// �õ������ݳ��Ȳ�����ֱ�ӷ��أ��ȴ�������...
		int in_bytes = m_strRecv.size() - nCmdLength;
		if( in_bytes % sizeof(TrackerGroupStat) != 0 )
			return GM_NoErr;
		m_nGroupCount = in_bytes / sizeof(TrackerGroupStat);
		m_lpGroupStat = new FDFSGroupStat[m_nGroupCount];
		memset(m_lpGroupStat, 0, sizeof(FDFSGroupStat) * m_nGroupCount);
		TrackerGroupStat * pSrc = (TrackerGroupStat*)(m_strRecv.c_str() + nCmdLength);
		TrackerGroupStat * pEnd = pSrc + m_nGroupCount;
		FDFSGroupStat * pDest = m_lpGroupStat;
		for( ; pSrc < pEnd; pSrc++ )
		{
			memcpy(pDest->group_name, pSrc->group_name, FDFS_GROUP_NAME_MAX_LEN);
			pDest->total_mb = buff2long(pSrc->sz_total_mb);
			pDest->free_mb = buff2long(pSrc->sz_free_mb);
			pDest->trunk_free_mb = buff2long(pSrc->sz_trunk_free_mb);
			pDest->count= buff2long(pSrc->sz_count);
			pDest->storage_port= buff2long(pSrc->sz_storage_port);
			pDest->storage_http_port= buff2long(pSrc->sz_storage_http_port);
			pDest->active_count = buff2long(pSrc->sz_active_count);
			pDest->current_write_server = buff2long( pSrc->sz_current_write_server);
			pDest->store_path_count = buff2long( pSrc->sz_store_path_count);
			pDest->subdir_count_per_path = buff2long( pSrc->sz_subdir_count_per_path);
			pDest->current_trunk_file_id = buff2long( pSrc->sz_current_trunk_file_id);

			pDest++;
		}
	}
	return GM_NoErr;
}

//////////////////////////////////////////////////
// Storage�Ựʵ������...
//////////////////////////////////////////////////
CStorageSession::CStorageSession(StorageServer * lpStorage)
{
	m_NewStorage = *lpStorage;
	m_lpCurFile  = NULL;
	m_llFileSize = 0;
	m_llSendSize = 0;
	m_dwSendKbps  = 0;
	m_dwCurSendByte = 0;
	m_lpPackBuffer = new char[kPackSize + 1];
	memset(m_lpPackBuffer, 0, kPackSize + 1);
}

CStorageSession::~CStorageSession()
{
	// ֹͣ�ϴ��߳�...
	this->StopAndWaitForThread();
	// �ر��ļ����...
	this->CloseUpFile();
	// ��ӡ��ǰ�����ϴ����ļ���λ��...
	// û���ϴ���ϵ��ļ���fdfs-storage���Զ�(�ع�)ɾ���������˸������´����ϴ�ʱ�����ϴ�������
	// STORAGE_PROTO_CMD_UPLOAD_FILE �� STORAGE_PROTO_CMD_APPEND_FILE ������...
	if( m_llFileSize != m_llSendSize && m_llSendSize > 0 ) {
		TRACE("File = %s, Send = %I64d, Size = %I64d\n", m_strCurFile, m_llSendSize, m_llFileSize);
	}
	// �ر����ݰ�������...
	if( m_lpPackBuffer != NULL ) {
		delete [] m_lpPackBuffer;
		m_lpPackBuffer = NULL;
	}
	// ��ӡCStorageSession�˳���Ϣ...
	MsgLogINFO("[~CStorageSession] - Exit");
}

void CStorageSession::Entry()
{
	int   nRetValue = 0;
	DWORD dwStartTimeMS = ::GetTickCount();
	while( !this->IsStopRequested() ) {
		// ���ʱ����������1000���룬�����һ�η�������...
		if( (::GetTickCount() - dwStartTimeMS) >= 1000 ) {
			dwStartTimeMS = ::GetTickCount();
			m_dwSendKbps = m_dwCurSendByte * 8 / 1024;
			m_dwCurSendByte = 0;
			if( m_dwSendKbps > 0 ) {
				TRACE("Send = %lu Kbps\n", m_dwSendKbps);
			}
		}
		// ���û���ҵ�һ����Ч�ľ��������һ�£�����Ѱ��...
		if( this->FindOneFile() == NULL ) {
			::Sleep(50);
			continue;
		}
		// ��ǰ���ļ��������Ч�ģ���ȡ���������ݰ�...
		nRetValue = this->SendOnePacket();
		// < 0 ��ӡ������Ϣ���ȴ��Ự��ɾ��...
		// ע��ɾ������ֻ��һ���ط�����Ҫ�Ӷ���ط�ɾ�������׻���...
		if( nRetValue < 0 ) {
			ASSERT( this->GetErrorCode() > 0 );
			TRACE("[CStorageSession::SendOnePacket] - Error = %lu\n", this->GetErrorCode());
			continue;
		}
		// == 0 ���ϼ���...
		if( nRetValue == 0 ) {
			::Sleep(5);
			continue;
		}
		// > 0 ˵������,���Ϸ���...
		ASSERT( nRetValue > 0 );
	}
}
//
// �رյ�ǰ�����ϴ����ļ�...
void CStorageSession::CloseUpFile()
{
	// ����Դ�����̱߳���...
	OS_MUTEX_LOCKER(&m_Mutex);
	// �ر������ϴ����ļ����...
	if( m_lpCurFile != NULL ) {
		fclose(m_lpCurFile);
		m_lpCurFile = NULL;
	}
}
//
// �ҵ�һ�������ϴ����ļ�...
FILE * CStorageSession::FindOneFile()
{
	// ����Դ�����̱߳���...
	OS_MUTEX_LOCKER(&m_Mutex);

	// ��ǰ�Ự���ļ��Ѿ�������ϣ��ȴ�����������Զ���ļ���..
	if((m_llFileSize > 0) && (m_llFileSize == m_llSendSize))
		return NULL;
	// �����ǰ�Ự�Ѿ��������󣬵ȴ���ɾ��...
	if( this->GetErrorCode() > 0 )
		return NULL;
	ASSERT( this->GetErrorCode() <= 0 );
	// �жϵ�ǰ�ļ�����Ƿ���Ч...
	if( m_lpCurFile != NULL )
		return m_lpCurFile;
	ASSERT( m_lpCurFile == NULL );
	// ��ȡ���jpg��mp4�ļ�������·��...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	string & strSavePath = theConfig.GetSavePath();
	string   strRootPath = strSavePath;
	if( strRootPath.size() <= 0 ) {
		MsgLogGM(GM_File_Path_Err);
		return NULL;
	}
	// ·���Ƿ���Ҫ���Ϸ�б��...
	BOOL bHasSlash = false;
	bHasSlash = ((strRootPath.at(strRootPath.size()-1) == '\\') ? true : false);
	strRootPath += (bHasSlash ? "*.*" : "\\*.*");
	// ׼��������Ҫ�ı���...
	GM_Error theErr = GM_NoErr;
	WIN32_FIND_DATA theFileData = {0};
	HANDLE	hFind = INVALID_HANDLE_VALUE;
	LPCTSTR	lpszExt = NULL;
	BOOL	bIsOK = true;
	CString strFileName;
	// ���ҵ�һ���ļ���Ŀ¼...
	hFind = ::FindFirstFile(strRootPath.c_str(), &theFileData);
	if( hFind != INVALID_HANDLE_VALUE ) {
		while( bIsOK ) {
			if(	(theFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||	// Ŀ¼
				(theFileData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) ||		// ϵͳ
				(strcmp(theFileData.cFileName, ".") == 0) ||					// ��ǰ
				(strcmp(theFileData.cFileName, "..") == 0) )					// �ϼ�
			{
				bIsOK = ::FindNextFile(hFind, &theFileData);
				continue;
			}
			// �ж���չ���Ƿ���Ч => .jpg .jpeg .mp4 ��Ч...
			lpszExt = ::PathFindExtension(theFileData.cFileName);
			if( (stricmp(lpszExt, ".jpg") != 0) && (stricmp(lpszExt, ".jpeg") != 0) && (stricmp(lpszExt, ".mp4") != 0) ) {
				bIsOK = ::FindNextFile(hFind, &theFileData);
				continue;
			}
			// ���ȣ������Ч���ļ�ȫ·������ȡ�ļ�����...
			strFileName.Format(bHasSlash ? "%s%s" : "%s\\%s", strSavePath.c_str(), theFileData.cFileName);
			ULONGLONG llSize = CUtilTool::GetFileFullSize(strFileName);
			// �ļ�����Ϊ0��������һ���ļ�...
			if( llSize <= 0 ) {
				bIsOK = ::FindNextFile(hFind, &theFileData);
				continue;
			}
			// ���ļ����ʧ�ܣ�������һ���ļ�...
			FILE * lpFile = fopen(strFileName, "rb");
			if( lpFile == NULL ) {
				bIsOK = ::FindNextFile(hFind, &theFileData);
				continue;
			}
			// �����������ָ��ͷ��Ϣ => �ļ����� + ��չ��(ȥ�����)...
			theErr = this->SendCmdHeader(llSize, lpszExt + 1);
			// ����ָ��ͷʧ�ܣ��ر��ļ���������һ��...
			if( theErr != GM_NoErr ) {
				MsgLogGM(theErr);
				fclose(lpFile); lpFile = NULL;
				bIsOK = ::FindNextFile(hFind, &theFileData);
				continue;
			}
			// һ���������رղ��Ҿ��...
			::FindClose(hFind);
			ASSERT( lpFile != NULL );
			// �����ļ�ȫ·�������ȣ����...
			m_strCurFile = strFileName;
			m_lpCurFile = lpFile;
			m_llFileSize = llSize;
			m_llSendSize = 0;
			return m_lpCurFile;
		}
	}
	// ������Ҿ����Ϊ�գ�ֱ�ӹر�֮...
	if( hFind != INVALID_HANDLE_VALUE ) {
		::FindClose(hFind);
	}
	return NULL;
}
//
// ����ָ����Ϣͷ���ݰ�...
GM_Error CStorageSession::SendCmdHeader(ULONGLONG llSize, LPCTSTR lpszExt)
{
	// ׼����Ҫ�ı���...
	GM_Error theErr = GM_NoErr;
	UInt32	 nReturn = 0;
	UInt32	 nLength = 0;
	char  *  lpBuf = NULL;
	char     szBuf[MAX_PATH] = {0};
	int      n_pkg_len = 1 + sizeof(ULONGLONG) + FDFS_FILE_EXT_NAME_MAX_LEN;
	// ָ������� => Ŀ¼���(1) + �ļ�����(8) + �ļ���չ��(6)

	// ���ָ��ͷ => ָ���� + ״̬ + ָ�������(����ָ��ͷ)...
	// STORAGE_PROTO_CMD_UPLOAD_FILE �� STORAGE_PROTO_CMD_APPEND_FILE ������...
	// û���ϴ���ϵ��ļ���fdfs-storage���Զ�(�ع�)ɾ���������˸������´����ϴ�ʱ�����ϴ�������
	TrackerHeader * lpHeader = (TrackerHeader*)szBuf;
	lpHeader->cmd = STORAGE_PROTO_CMD_UPLOAD_FILE;
	lpHeader->status = 0;
	long2buff(llSize + n_pkg_len, lpHeader->pkg_len);

	// ���ָ�������...
	lpBuf = szBuf + sizeof(TrackerHeader);								// ��ָ�붨λ��������...
	lpBuf[0] = m_NewStorage.store_path_index;							// Ŀ¼���   => 0 - 1  => 1���ֽ�
	long2buff(llSize, lpBuf + 1);										// �ļ�����   => 1 - 9  => 8���ֽ� ULONGLONG
	memcpy(lpBuf + 1 + sizeof(ULONGLONG), lpszExt, strlen(lpszExt));	// �ļ���չ�� => 9 - 15 => 6���ֽ� FDFS_FILE_EXT_NAME_MAX_LEN

	// �����ϴ�ָ��ͷ��ָ������� => ����ͷ + ���ݳ���...
	nLength = sizeof(TrackerHeader) + n_pkg_len;
	theErr = m_TCPSocket.Send(szBuf, nLength, &nReturn);
	// �����������������������´η���...
	if( theErr == WSAEWOULDBLOCK ) {
		ASSERT( m_strCurData.size() <= 0 );
		m_strCurData.append(szBuf, nLength);
		return GM_NoErr;
	}
	// �������ʧ�ܣ�ֱ�ӷ���...
	if( theErr != GM_NoErr )
		return theErr;
	// ���ͳɹ����ۼӷ����ֽ���...
	ASSERT( theErr == GM_NoErr );
	m_dwCurSendByte += nReturn;
	return GM_NoErr;
}
//
// ����һ����Ч���ݰ�...
int CStorageSession::SendOnePacket()
{
	// ����Դ�����̱߳���...
	OS_MUTEX_LOCKER(&m_Mutex);

	// ���ȣ����ͻ�û�з��ͳ�ȥ�����ݰ�...
	GM_Error theErr = GM_NoErr;
	UInt32	 nReturn = 0;
	if( m_strCurData.size() > 0 ) {
		// �������ݰ��������������ȴ��´η���...
		theErr = m_TCPSocket.Send(m_strCurData.c_str(), m_strCurData.size(), &nReturn);
		if( theErr == WSAEWOULDBLOCK )
			return 0;
		// �����������ô���ţ��ر����ӣ��رվ��...
		if( theErr != GM_NoErr ) {
			this->SetErrorCode(theErr);
			m_TCPSocket.Close();
			MsgLogGM(theErr);
			return -1;
		}
		// ������ɣ��ۼӷ������������ÿ�...
		ASSERT( m_strCurData.size() == nReturn );
		m_dwCurSendByte += nReturn;
		m_llSendSize += nReturn;
		m_strCurData.clear();
	}
	ASSERT( m_strCurData.size() <= 0 );
	// �ж��ļ��Ƿ��Ѿ����������...
	// �رվ�����ȴ�����������Զ���ļ���...
	// �ļ����Ⱥ��ļ�����Ҫ��λ��֪ͨ��վʱʹ��...
	if( m_llSendSize >= m_llFileSize ) {
		this->CloseUpFile();
		return 0;
	}
	ASSERT( m_strCurData.empty() );
	ASSERT( m_lpCurFile != NULL );
	// ���ļ��ж�ȡһ�����ݰ���������...
	memset(m_lpPackBuffer, 0, kPackSize + 1);
	int nSize = fread(m_lpPackBuffer, 1, kPackSize, m_lpCurFile);
	// �����ȡ�ļ�ʧ�ܣ����ô���ţ��ر����ӣ��رվ��...
	if( nSize <= 0 ) {
		theErr = GM_File_Read_Err;
		this->SetErrorCode(theErr);
		this->CloseUpFile();
		m_TCPSocket.Close();
		MsgLogGM(theErr);
		return -1;
	}
	// ���͵��洢������...
	theErr = m_TCPSocket.Send(m_lpPackBuffer, nSize, &nReturn);
	// �������������������ݻ����������ȴ��´η���...
	if( theErr == WSAEWOULDBLOCK ) {
		m_strCurData.append(m_lpPackBuffer, nSize);
		return 0;
	}
	// �����������ô���ţ��ر����ӣ��رվ��...
	if( theErr != GM_NoErr ) {
		this->SetErrorCode(theErr);
		this->CloseUpFile();
		m_TCPSocket.Close();
		MsgLogGM(theErr);
		return -1;
	}
	// ���ͳɹ����ۼӷ������������µķ���...
	ASSERT( nSize == nReturn );
	m_dwCurSendByte += nReturn;
	m_llSendSize += nReturn;
	return 1;
}

GM_Error CStorageSession::ForConnect()
{
	ASSERT( m_eState == kConnectState );

	// ֱ�������ϴ��߳�...
	this->Start();

	return GM_NoErr;
}

GM_Error CStorageSession::ForRead()
{
	// ����Դ�����̱߳���...
	OS_MUTEX_LOCKER(&m_Mutex);

	// �õ������ݳ��Ȳ������ȴ��µĽ�������...
	int nCmdLength = sizeof(TrackerHeader);
	if( m_strRecv.size() < nCmdLength )
		return GM_NoErr;
	GM_Error theErr = GM_NoErr;
	int nDataSize = m_strRecv.size() - nCmdLength;
	LPCTSTR lpszDataBuf = m_strRecv.c_str() + nCmdLength;
	TrackerHeader * lpHeader = (TrackerHeader*)m_strRecv.c_str();
	// �жϷ��������ص�״̬�Ƿ���ȷ...
	if( lpHeader->status != 0 ) {
		theErr = GM_FDFS_Status_Err;
		MsgLogGM(theErr);
		return theErr;
	}
	// �жϷ��ص������������Ƿ���ȷ => ������� FDFS_GROUP_NAME_MAX_LEN
	if( nDataSize <= FDFS_GROUP_NAME_MAX_LEN ) {
		theErr = GM_FDFS_Data_Err;
		MsgLogGM(theErr);
		return theErr;
	}
	// �õ������� ������ + �ļ���...
	CString strFileFDFS;
	TCHAR group_name[FDFS_GROUP_NAME_MAX_LEN + 1] = {0};
	TCHAR remote_filename[FDFS_REMOTE_NAME_MAX_SIZE] = {0};
	memcpy(group_name, lpszDataBuf, FDFS_GROUP_NAME_MAX_LEN);
	memcpy(remote_filename, lpszDataBuf + FDFS_GROUP_NAME_MAX_LEN, nDataSize - FDFS_GROUP_NAME_MAX_LEN + 1);
	strFileFDFS.Format("%s/%s", group_name, remote_filename);
	// �ر��ϴ��ļ����...
	this->CloseUpFile();
	// ����վ�㱨������FDFS�ļ���¼...
	this->doWebSaveFDFS(m_strCurFile, strFileFDFS, m_llFileSize);
	// ɾ���ļ�����ӡԶ���ļ�id...
	ASSERT( m_lpCurFile == NULL && m_llFileSize > 0 );
	TRACE("Local = %s, Remote = %s, Size = %I64d\n", m_strCurFile, strFileFDFS, m_llFileSize);
	if( !::DeleteFile(m_strCurFile) ) {
		MsgLogGM(GM_File_Del_Err);
	}
	// ��λ�ļ�������λ�ļ����ȣ��Զ��������ļ��ϴ�����...
	m_strCurFile.Empty();
	m_llFileSize = 0;
	m_llSendSize = 0;
	// ���ý��ջ��������ȴ��µ�����...
	m_strRecv.clear();
	return GM_NoErr;
}

size_t procCurlPost(void *ptr, size_t size, size_t nmemb, void *stream)
{
	CStorageSession * lpSession = (CStorageSession*)stream;
	lpSession->doCurlPost((char*)ptr, size * nmemb);
	return size * nmemb;
}
//
// ����һ��json���ݰ�...
void CStorageSession::doCurlPost(char * pData, size_t nSize)
{
	string strUTF8Data;
	string strANSIData;
	Json::Reader reader;
	Json::Value  value;
	// ��UTF8��վ����ת����ANSI��ʽ...
	strUTF8Data.assign(pData, nSize);
	strANSIData = CUtilTool::UTF8_ANSI(strUTF8Data.c_str());
	// ����ת�����json���ݰ�...
	if( !reader.parse(strANSIData, value) ) {
		MsgLogGM(GM_Err_Json);
		return;
	}
	// ��ȡ���صĲɼ��˱�źʹ�����Ϣ...
	if( value.isMember("err_code") && value["err_code"].asBool() ) {
		string & strData = value["err_msg"].asString();
		string & strMsg = CUtilTool::UTF8_ANSI(strData.c_str());
		MsgLogINFO(strMsg.c_str());
		return;
	}
	// �������ݿ����¼�ı�ţ����ã�ֱ�Ӷ���...
	/*int nRetID = 0;
	Json::Value theID;
	if( value.isMember("image_id") ) {
		theID = value["image_id"];
	} else if( value.isMember("record_id") ) {
		theID = value["record_id"];
	}
	// ��Ҫ�������ͷֱ�ת��...
	if( theID.isInt() ) {
		nRetID = theID.asInt();
	} else if( theID.isString() ) {
		nRetID = atoi(theID.asString().c_str());
	}*/
}
//
// ����վ�ϱ���FDFS�ļ���¼...
BOOL CStorageSession::doWebSaveFDFS(CString & strPathFile, CString & strFileFDFS, ULONGLONG llFileSize)
{
	// �ж������Ƿ���Ч...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	string  & strWebAddr = theConfig.GetWebAddr();
	int nWebPort = theConfig.GetWebPort();
	if( nWebPort <= 0 || strWebAddr.size() <= 0 || strPathFile.GetLength() <= 0 || strFileFDFS.GetLength() <= 0 )
		return false;
	// ���ļ��������н�������Ҫ���ļ�������չ��...
	TCHAR szExt[32] = {0};
	TCHAR szDriver[32] = {0};
	TCHAR szDir[MAX_PATH] = {0};
	TCHAR szSrcName[MAX_PATH*2] = {0};
	// szSrcName => jpg => uniqid_DBCameraID
	// szSrcName => mp4 => uniqid_DBCameraID_CourseID_Duration
	_splitpath(strPathFile, szDriver, szDir, szSrcName, szExt);
	// SrcNameת����UTF8��ʽ, �ٽ���URIEncode...
	string strUTF8Name = CUtilTool::ANSI_UTF8(szSrcName);
	StringParser::EncodeURI(strUTF8Name.c_str(), strUTF8Name.size(), szSrcName, MAX_PATH*2);
	// ׼����Ҫ�Ļ㱨���� => POST���ݰ�...
	CString strPost, strUrl;
	strPost.Format("ext=%s&file_src=%s&file_fdfs=%s&file_size=%I64d", szExt, szSrcName, strFileFDFS, llFileSize);
	strUrl.Format("http://%s:%d/wxapi.php/Gather/saveFDFS", strWebAddr.c_str(), nWebPort);
	// ����Curl�ӿڣ��㱨�ɼ�����Ϣ...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if( curl == NULL )
			break;
		// �趨curl����������postģʽ...
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strPost);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strPost.GetLength());
		res = curl_easy_setopt(curl, CURLOPT_HEADER, false);
		res = curl_easy_setopt(curl, CURLOPT_POST, true);
		res = curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
		res = curl_easy_setopt(curl, CURLOPT_URL, strUrl);
		res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, procCurlPost);
		res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this);
		res = curl_easy_perform(curl);
	}while( false );
	// �ͷ���Դ...
	if( curl != NULL ) {
		curl_easy_cleanup(curl);
	}
	return true;
}
//////////////////////////////////////////////////
// Remote�Ựʵ������...
//////////////////////////////////////////////////
CRemoteSession::CRemoteSession(CHaoYiView * lpHaoYiView)
  : m_lpHaoYiView(lpHaoYiView)
  , m_lpSendBuf(NULL)
{
	ASSERT( m_lpHaoYiView != NULL );
	m_lpSendBuf = new char[kSendBufSize];
	memset(m_lpSendBuf, 0, kSendBufSize);
}

CRemoteSession::~CRemoteSession()
{
	if( m_lpSendBuf != NULL ) {
		delete [] m_lpSendBuf;
		m_lpSendBuf = NULL;
	}
	MsgLogINFO("[~CRemoteSession] - Exit");
}

GM_Error CRemoteSession::ForConnect()
{
	ASSERT( m_eState == kConnectState );
	// ���ӳɹ����������͵�¼���� => Cmd_Header + JSON...
	return this->SendLoginCmd();
}

GM_Error CRemoteSession::ForRead()
{
	// �����������ݻᷢ��ճ��������ˣ���Ҫѭ��ִ��...
	while( m_strRecv.size() > 0 ) {
		// �õ������ݳ��Ȳ�����ֱ�ӷ��أ��ȴ�������...
		int nCmdLength = sizeof(Cmd_Header);
		if( m_strRecv.size() < nCmdLength )
			return GM_NoErr;
		ASSERT( m_strRecv.size() >= nCmdLength );
		Cmd_Header * lpCmdHeader = (Cmd_Header*)m_strRecv.c_str();
		LPCTSTR lpDataPtr = m_strRecv.c_str() + sizeof(Cmd_Header);
		int nDataSize = m_strRecv.size() - sizeof(Cmd_Header);
		// �ѻ�ȡ�����ݳ��Ȳ�����ֱ�ӷ��أ��ȴ�������...
		if( nDataSize < lpCmdHeader->m_pkg_len )
			return GM_NoErr;
		ASSERT( nDataSize >= lpCmdHeader->m_pkg_len );
		ASSERT( lpCmdHeader->m_type == kClientPHP || lpCmdHeader->m_type == kClientPlay || lpCmdHeader->m_type == kClientLive ); 
		// ��ʼ�ַ�ת��������PHP����...
		GM_Error theErr = GM_NoErr;
		switch( lpCmdHeader->m_cmd )
		{
		case kCmd_Play_Login:			  theErr = this->doCmdPlayLogin(m_strRecv); break;
		case kCmd_Live_Vary:			  theErr = this->doCmdLiveVary(lpDataPtr, lpCmdHeader->m_pkg_len); break;
		case kCmd_PHP_Start_Camera:		  theErr = this->doPHPCameraOperate(lpDataPtr, lpCmdHeader->m_pkg_len, true); break;
		case kCmd_PHP_Stop_Camera:		  theErr = this->doPHPCameraOperate(lpDataPtr, lpCmdHeader->m_pkg_len, false); break;
		case kCmd_PHP_Set_Camera_Name:    theErr = this->doPHPSetCameraName(lpDataPtr, lpCmdHeader->m_pkg_len); break;
		case kCmd_PHP_Set_Course_Mod:     theErr = this->doPHPSetCourseOpt(lpDataPtr, lpCmdHeader->m_pkg_len); break;
		case kCmd_PHP_Get_Course_Record:  theErr = this->doPHPGetCourseRecord(m_strRecv); break;
		//case kCmd_PHP_Set_Course_Add:     theErr = this->doPHPSetCourseOpt(CHaoYiView::kAddCourse, lpDataPtr, lpCmdHeader->m_pkg_len); break;
		//case kCmd_PHP_Set_Course_Del:     theErr = this->doPHPSetCourseOpt(CHaoYiView::kDelCourse, lpDataPtr, lpCmdHeader->m_pkg_len); break;
		//case kCmd_PHP_Get_Camera_Status:  theErr = this->doPHPGetCameraStatus(m_strRecv); break;
		}
		// ɾ���Ѿ�������ϵ����� => Header + pkg_len...
		m_strRecv.erase(0, lpCmdHeader->m_pkg_len + sizeof(Cmd_Header));
		// �жϷ��ؽ������Ч��...
		if( theErr != GM_NoErr ) {
			MsgLogGM(theErr);
			return theErr;
		}
		// ����������ݣ��������������...
		ASSERT(theErr == GM_NoErr);
	}
	return GM_NoErr;
}
//
// 2017.08.08 - by jackey => ������ת����...
// ����PHPת���Ŀ�ʼ��ֹͣͨ������...
GM_Error CRemoteSession::doPHPCameraOperate(LPCTSTR lpData, int nSize, BOOL bIsStart)
{
	// �ж��������ݵ���Ч��...
	if( nSize <= 0 || lpData == NULL )
		return GM_NoErr;
	ASSERT( nSize > 0 && lpData != NULL );
	string strUTF8Data;
	Json::Reader reader;
	Json::Value  value;
	// ��UTF8��վ����ת����ANSI��ʽ => ������php������ģ�ת������Ч����ȡ��������֮�󣬻�Ҫת��һ��...
	GM_Error theErr = GM_Err_Json;
	strUTF8Data.assign(lpData, nSize);
	// ����ת�����JSON���ݰ� => PHP���������ݣ�ת����Ч����Ȼ��UTF8��ʽ...
	if( !reader.parse(strUTF8Data, value) ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// �жϻ�ȡ���ݵ���Ч��...
	if( !value.isMember("camera_id") ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// ��ȡͨ�������ݿ���...
	int nDBCameraID = atoi(CUtilTool::getJsonString(value["camera_id"]).c_str());
	// ��ʼ���Ҷ�Ӧ������ͷ���ر��...
	int nLocalID = -1;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetDBCameraID(nDBCameraID, nLocalID);
	if(  nLocalID <= 0  ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// ���ݱ��ر�Ż�ȡ����ͷ����...
	CCamera * lpCamera = m_lpHaoYiView->FindCameraByID(nLocalID);
	if( lpCamera == NULL || lpCamera->GetVideoWnd() == NULL ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// ����ǰͨ�����óɽ���ͨ��...
	lpCamera->GetVideoWnd()->doFocusAction();
	// ������ͼ���Ϳ�ʼ��ֹͣģ��������...
	int nCmdID = (bIsStart ? ID_LOGIN_DVR : ID_LOGOUT_DVR);
	m_lpHaoYiView->PostMessage(WM_COMMAND, MAKEWPARAM(nCmdID, BN_CLICKED), NULL);
	return GM_NoErr;
}
//
// ����ֱ������������ת���� => �����Ӧ...
GM_Error CRemoteSession::doCmdLiveVary(LPCTSTR lpData, int nSize)
{
	// �ж��������ݵ���Ч��...
	if( nSize <= 0 || lpData == NULL )
		return GM_NoErr;
	ASSERT( nSize > 0 && lpData != NULL );
	string strUTF8Data;
	Json::Reader reader;
	Json::Value  value;
	// ��UTF8��վ����ת����ANSI��ʽ => ������php������ģ�ת������Ч����ȡ��������֮�󣬻�Ҫת��һ��...
	GM_Error theErr = GM_Err_Json;
	strUTF8Data.assign(lpData, nSize);
	// ����ת�����JSON���ݰ� => PHP���������ݣ�ת����Ч����Ȼ��UTF8��ʽ...
	if( !reader.parse(strUTF8Data, value) ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// �жϻ�ȡ���ݵ���Ч��...
	if( !value.isMember("rtmp_live") || !value.isMember("rtmp_user") ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// ��ȡͨ�������ݿ��ź͸�ͨ���ϵ��û���...
	int nDBCameraID = 0, nUserCount = 0;
	Json::Value & theDBCameraID = value["rtmp_live"];
	if( theDBCameraID.isString() ) {
		nDBCameraID = atoi(theDBCameraID.asString().c_str());
	} else if( theDBCameraID.isInt() ) {
		nDBCameraID = theDBCameraID.asInt();
	}
	Json::Value & theUserCount = value["rtmp_user"];
	if( theUserCount.isString() ) {
		nUserCount = atoi(theUserCount.asString().c_str());
	} else if( theUserCount.isInt() ) {
		nUserCount = theUserCount.asInt();
	}
	// ֻ���û��� <= 0 �Ž���ֹͣ����...
	if( nUserCount > 0 )
		return GM_NoErr;
	ASSERT( nUserCount <= 0 );
	// ��ʼ���Ҷ�Ӧ������ͷ���ر��...
	int nLocalID = -1;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetDBCameraID(nDBCameraID, nLocalID);
	if(  nLocalID <= 0  ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// ���ݱ��ر�Ż�ȡ����ͷ����...
	CCamera * lpCamera = m_lpHaoYiView->FindCameraByID(nLocalID);
	if( lpCamera == NULL ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	ASSERT( lpCamera != NULL );
	// �����ͨ���µ��û����Ѿ�Ϊ0�������ɾ���ӿ�...
	// ���ﲻ��ֱ��ɾ�����Ῠ�������ͨ�����ʹ�����Ϣ...
	lpCamera->doStopLiveMessage();
	return GM_NoErr;
}
//
// ����ֱ������������ת���� => �����Ӧ => 2017.06.15 - by jackey...
GM_Error CRemoteSession::doCmdPlayLogin(string & inData)
{
	// �Ӵ��ݹ����������н�����JSON���ݣ���Ч��ǰ���Ѿ��ж���...
	Cmd_Header * lpCmdHeader = (Cmd_Header*)inData.c_str();
	LPCTSTR lpDataPtr = inData.c_str() + sizeof(Cmd_Header);
	int nDataSize = lpCmdHeader->m_pkg_len;
	// ����JSON����...
	Json::Reader reader;
	Json::Value  value, root;
	string strUTF8Data, strRtmpUrl;
	GM_Error theErr = GM_Err_Json;
	strUTF8Data.assign(lpDataPtr, nDataSize);
	// ����ת�����JSON���ݰ� => PHP���������ݣ�ת����Ч����Ȼ��UTF8��ʽ => �����������б�����ת��...
	if( !reader.parse(strUTF8Data, value) ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// �жϴ��ݹ�����������Ч��...
	do {
		if( !value.isMember("rtmp_url") || !value.isMember("rtmp_live") || !value.isMember("rtmp_user") ) {
			MsgLogGM(theErr);
			break;
		}
		// ������rtmp_url��ַ����������...
		strRtmpUrl = value["rtmp_url"].asString();
		// ������ͨ�������ݿ��ţ��û�����...
		int nDBCameraID, nUserCount = 0;
		Json::Value & theDBCameraID = value["rtmp_live"];
		if( theDBCameraID.isString() ) {
			nDBCameraID = atoi(theDBCameraID.asString().c_str());
		} else if( theDBCameraID.isInt() ) {
			nDBCameraID = theDBCameraID.asInt();
		}
		// ��������ͨ���ϵ��û���...
		Json::Value & theUserCount = value["rtmp_user"];
		if( theUserCount.isString() ) {
			nUserCount = atoi(theUserCount.asString().c_str());
		} else if( theUserCount.isInt() ) {
			nUserCount = theUserCount.asInt();
		}
		// ��ʼ���Ҷ�Ӧ������ͷ���ر��...
		int nLocalID = -1;
		CXmlConfig & theConfig = CXmlConfig::GMInstance();
		theConfig.GetDBCameraID(nDBCameraID, nLocalID);
		if(  nLocalID <= 0  ) {
			MsgLogGM(theErr);
			break;
		}
		ASSERT( nLocalID > 0 );
		// ���ݱ��ر�Ż�ȡ����ͷ����...
		CCamera * lpCamera = m_lpHaoYiView->FindCameraByID(nLocalID);
		if( lpCamera == NULL ) {
			MsgLogGM(theErr);
			break;
		}
		ASSERT( lpCamera != NULL );
		// 2017.06.15 - by jackey => ����ֱ���ϴ�ͨ�� => ������ʱ��ֱ�ӷ���...
		int nResult = lpCamera->doStartLivePush(strRtmpUrl);
		(nResult < 0) ? MsgLogGM(theErr) : NULL;
	}while( false );
	// 2017.06.15 - by jackey => �����Ӧ��ֱ�ӷ���...
	return GM_NoErr;
}
//
// ת�����ָ����ֱ��������...
/*GM_Error CRemoteSession::doTransmitPlayer(int nPlayerSock, string & strRtmpUrl, GM_Error inErr)
{
	// ׼������Ҫ���͵Ļ�����...
	string strJson;
	Json::Value root;
	root["err_code"] = (inErr != GM_NoErr) ? ERR_PUSH_FAIL : ERR_OK;
	root["rtmp_url"] = strRtmpUrl;
	// �� JSON ����ת�����ַ���...
	strJson = root.toStyledString();
	// ��Ϸ�������ͷ��Ϣ => ֱ�Ӵ�ת�������п���һЩ����...
	UInt32	nSendSize = 0;
	TCHAR	szSendBuf[1024] = {0};
	Cmd_Header theHeader = {0};
	theHeader.m_pkg_len = strJson.size();
	theHeader.m_type = kClientGather;
	theHeader.m_cmd  = kCmd_Play_Login;
	theHeader.m_sock = nPlayerSock;
	// ׷�������ͷ���������ݰ�����...
	memcpy(szSendBuf, &theHeader, sizeof(theHeader));
	nSendSize += sizeof(theHeader);
	memcpy(szSendBuf+nSendSize, strJson.c_str(), strJson.size());
	nSendSize += strJson.size();
	// ����ͳһ�ķ��ͽӿ�...
	return this->SendData(szSendBuf, nSendSize);
}*/
//
// ����PHP�ͷ��˷��͵���������ͷ��������...
// ���ݸ�ʽ => Cmd_Header + JSON...
// JSON��ʽ => camera_id: xxx, camera_name: xxx...
GM_Error CRemoteSession::doPHPSetCameraName(LPCTSTR lpData, int nSize)
{
	// �ж��������ݵ���Ч��...
	if( nSize <= 0 || lpData == NULL )
		return GM_NoErr;
	ASSERT( nSize > 0 && lpData != NULL );
	string strANSIData;
	string strUTF8Data;
	Json::Reader reader;
	Json::Value  value;
	// ��UTF8��վ����ת����ANSI��ʽ => ������php������ģ�ת������Ч����ȡ��������֮�󣬻�Ҫת��һ��...
	GM_Error theErr = GM_Err_Json;
	strUTF8Data.assign(lpData, nSize);
	strANSIData = CUtilTool::UTF8_ANSI(strUTF8Data.c_str());
	// ����ת�����JSON���ݰ� => PHP���������ݣ�ת����Ч����Ȼ��UTF8��ʽ...
	if( !reader.parse(strANSIData, value) ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// �жϻ�ȡ���ݵ���Ч��...
	if( !value.isMember("camera_id") || !value.isMember("camera_name") ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// ��ȡͨ�����...
	int nDBCameraID = 0;
	Json::Value & theDBCameraID = value["camera_id"];
	if( theDBCameraID.isString() ) {
		nDBCameraID = atoi(theDBCameraID.asString().c_str());
	} else if( theDBCameraID.isInt() ) {
		nDBCameraID = theDBCameraID.asInt();
	}
	// ��ȡͨ������...
	string strDBCameraName;
	Json::Value & theCameraName = value["camera_name"];
	if( theCameraName.isString() ) {
		string & strName = theCameraName.asString();
		strDBCameraName = CUtilTool::UTF8_ANSI(strName.c_str());
	}
	// �жϻ�ȡ���ݵ���Ч��...
	if( nDBCameraID <= 0 || strDBCameraName.size() <= 0 ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// ��ʼ���Ҷ�Ӧ������ͷ���ر��...
	int nLocalID = -1;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetDBCameraID(nDBCameraID, nLocalID);
	if(  nLocalID <= 0  )
		return GM_NoErr;
	ASSERT( nLocalID > 0 );
	// ���ݱ��ر�Ż�ȡ����ͷ����...
	CCamera * lpCamera = m_lpHaoYiView->FindCameraByID(nLocalID);
	if( lpCamera == NULL )
		return GM_NoErr;
	ASSERT( lpCamera != NULL );
	// ����ȡ������ͷ���Ƹ��µ������ļ�����...
	GM_MapData theMapLoc;
	theConfig.GetCamera(nLocalID, theMapLoc);
	theMapLoc["Name"] = strDBCameraName;
	theConfig.SetCamera(nLocalID, theMapLoc);
	// ����ȡ������ͷ���Ƹ��µ�����ͷ���ڶ�����...
	CString strTitleName = strDBCameraName.c_str();
	STREAM_PROP theProp = lpCamera->GetStreamProp();
	lpCamera->UpdateWndTitle(theProp, strTitleName);
	// ���ø����ڽӿڣ������Ҳ����ཹ�㴰�ڵ�����ͷ����...
	m_lpHaoYiView->UpdateFocusTitle(nLocalID, strTitleName);
	// ����ȡ������ͷ���Ʊ��浽ϵͳ�����ļ�����...
	theConfig.GMSaveConfig();
	return GM_NoErr;
}
//
// ����PHP�ͷ��˷��͵�����¼��α������...
GM_Error CRemoteSession::doPHPSetCourseOpt(LPCTSTR lpData, int nSize)
{
	// �ж��������ݵ���Ч��...
	if( nSize <= 0 || lpData == NULL )
		return GM_NoErr;
	ASSERT( nSize > 0 && lpData != NULL );
	string strANSIData;
	string strUTF8Data;
	Json::Reader reader;
	Json::Value  value;
	// ��UTF8��վ����ת����ANSI��ʽ => ������php������ģ�ת������Ч����ȡ��������֮�󣬻�Ҫת��һ��...
	GM_Error theErr = GM_Err_Json;
	strUTF8Data.assign(lpData, nSize);
	strANSIData = CUtilTool::UTF8_ANSI(strUTF8Data.c_str());
	// ����ת�����JSON���ݰ� => PHP���������ݣ�ת����Ч����Ȼ��UTF8��ʽ...
	if( !reader.parse(strANSIData, value) ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	/////////////////////////////////////////////////////////////////////////
	// ע�⣺���봫�� data �� camera_id...
	/////////////////////////////////////////////////////////////////////////
	// �жϻ�ȡ���ݵ���Ч��...
	if( !value.isMember("data") || !value.isMember("camera_id") ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// ��ȡͨ�����...
	string & strDBCameraID = CUtilTool::getJsonString(value["camera_id"]);
	int nDBCameraID = atoi(strDBCameraID.c_str());
	// ��ʼ���Ҷ�Ӧ������ͷ���ر��...
	int nLocalID = -1;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetDBCameraID(nDBCameraID, nLocalID);
	if(  nLocalID <= 0  )
		return GM_NoErr;
	// ���� Course ��¼...
	Json::Value arrayObj = value["data"];
	for (unsigned int i = 0; i < arrayObj.size(); i++)
	{
		int nCourseID = -1;
		int nOperate  = -1;
		int nWeekID   = -1;
		GM_MapData	theMapData;
		Json::Value item = arrayObj[i];
		// is_delete => 1(Add),2(Modify),3(Delete)
		if( item.isMember("is_delete") ) {
			nOperate = atoi(CUtilTool::getJsonString(item["is_delete"]).c_str());
		}
		if( item.isMember("course_id") ) {
			theMapData["course_id"] = CUtilTool::getJsonString(item["course_id"]);
			nCourseID = atoi(theMapData["course_id"].c_str());
		}
		if( item.isMember("week_id") ) {
			theMapData["week_id"] = CUtilTool::getJsonString(item["week_id"]);
			nWeekID = atoi(theMapData["week_id"].c_str());
		}
		if( item.isMember("camera_id") ) {
			theMapData["camera_id"] = CUtilTool::getJsonString(value["camera_id"]);
		}
		if( item.isMember("subject_id") ) {
			theMapData["subject_id"] = CUtilTool::getJsonString(item["subject_id"]);
		}
		if( item.isMember("teacher_id") ) {
			theMapData["teacher_id"] = CUtilTool::getJsonString(item["teacher_id"]);
		}
		if( item.isMember("repeat_id") ) {
			theMapData["repeat_id"] = CUtilTool::getJsonString(item["repeat_id"]);
		}
		if( item.isMember("elapse_sec") ) {
			theMapData["elapse_sec"] = CUtilTool::getJsonString(item["elapse_sec"]);
		}
		if( item.isMember("start_time") ) {
			theMapData["start_time"] = CUtilTool::getJsonString(item["start_time"]);
		}
		if( item.isMember("end_time") ) {
			theMapData["end_time"] = CUtilTool::getJsonString(item["end_time"]);
		}
		// �����¼��Ż���������Ч����ִ��...
		if( nCourseID <= 0 || nOperate <= 0 || nWeekID < 0 )
			continue;
		// ֪ͨ�����ڿα��¼�����˱仯...
		ASSERT( nCourseID > 0 && nOperate > 0 && nLocalID > 0 );
		m_lpHaoYiView->doCourseChanged(nOperate, nLocalID, theMapData);
	}
	return GM_NoErr;
}
//
// 2017.06.14 - by jackey => �������û���ˣ��ɼ��������㱨ͨ��״̬...
// ����PHP�ͷ�����Ҫ������ͷ״̬������ => ��Ҫ���� JSON ����...
// ���ݸ�ʽ => Cmd_Header + JSON...
// JSON��ʽ => 8:7:6:5
/*GM_Error CRemoteSession::doPHPGetCameraStatus(string & inData)
{
	// �Ӵ��ݹ����������н�����JSON���ݣ���Ч��ǰ���Ѿ��ж���...
	Cmd_Header * lpCmdHeader = (Cmd_Header*)inData.c_str();
	LPCTSTR lpDataPtr = inData.c_str() + sizeof(Cmd_Header);
	int nDataSize = lpCmdHeader->m_pkg_len;
	// ����JSON����...
	Json::Reader reader;
	Json::Value  value, root;
	string strUTF8Data, strJson;
	GM_Error theErr = GM_Err_Json;
	strUTF8Data.assign(lpDataPtr, nDataSize);
	// ����ת�����JSON���ݰ� => PHP���������ݣ�ת����Ч����Ȼ��UTF8��ʽ => �����������б�����ת��...
	if( !reader.parse(strUTF8Data, value) ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// �����������ͷ����б�...
	Json::Value::Members & member = value.getMemberNames();
	for(auto iter = member.begin(); iter != member.end(); ++iter) {
		// ��MAC��ַ�ڵ��ų���..
		string & strKey = (*iter);
		if( stricmp(strKey.c_str(), "mac_addr") == 0 )
			continue;
		// ��ͨ���ڵ�״̬�������...
		string & strItem = CUtilTool::getJsonString(value[*iter]);
		int nDBCameraID = atoi(strItem.c_str());
		root[strItem] = m_lpHaoYiView->GetCameraStatusByDBID(nDBCameraID);
	}
	// �� JSON ����ת�����ַ���...
	strJson = root.toStyledString();
	// ��Ϸ�������ͷ��Ϣ => ֱ�Ӵ�ת�������п���һЩ����...
	Cmd_Header theHeader = {0};
	UInt32	nSendSize = 0;
	theHeader.m_pkg_len = strJson.size();
	theHeader.m_type = kClientGather;
	theHeader.m_cmd  = lpCmdHeader->m_cmd;
	theHeader.m_sock = lpCmdHeader->m_sock;
	// ׷�������ͷ���������ݰ�����...
	memcpy(m_lpSendBuf, &theHeader, sizeof(theHeader));
	nSendSize += sizeof(theHeader);
	memcpy(m_lpSendBuf+nSendSize, strJson.c_str(), strJson.size());
	nSendSize += strJson.size();
	// ����ͳһ�ķ��ͽӿ�...
	return this->SendData(m_lpSendBuf, nSendSize);
}*/
//
// 2017.07.01 - by jackey => ���������Ȼ��Ч����Ҫת���� php �ͻ���...
// ����PHP�ͷ�����Ҫ������ͷ״̬������ => ��Ҫ���� JSON ����...
// ���ݸ�ʽ => Cmd_Header + JSON...
// JSON��ʽ => camera_id: xxx...
GM_Error CRemoteSession::doPHPGetCourseRecord(string & inData)
{
	// �Ӵ��ݹ����������н�����JSON���ݣ���Ч��ǰ���Ѿ��ж���...
	Cmd_Header * lpCmdHeader = (Cmd_Header*)inData.c_str();
	LPCTSTR lpDataPtr = inData.c_str() + sizeof(Cmd_Header);
	int nDataSize = lpCmdHeader->m_pkg_len;
	// ����JSON����...
	Json::Reader reader;
	Json::Value  value, root;
	string strUTF8Data, strJson;
	GM_Error theErr = GM_Err_Json;
	strUTF8Data.assign(lpDataPtr, nDataSize);
	// ����ת�����JSON���ݰ� => PHP���������ݣ�ת����Ч����Ȼ��UTF8��ʽ => �����������б�����ת��...
	if( !reader.parse(strUTF8Data, value) ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// �жϻ�ȡ���ݵ���Ч��...
	if( !value.isMember("camera_id")  ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// ��ȡͨ�����...
	int nDBCameraID = 0;
	Json::Value & theDBCameraID = value["camera_id"];
	if( theDBCameraID.isString() ) {
		nDBCameraID = atoi(theDBCameraID.asString().c_str());
	} else if( theDBCameraID.isInt() ) {
		nDBCameraID = theDBCameraID.asInt();
	}
	// �жϻ�ȡ���ݵ���Ч��...
	if( nDBCameraID <= 0  ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// ��ʼ���Ҷ�Ӧ������ͷ���ر��...
	int nLocalID = -1;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetDBCameraID(nDBCameraID, nLocalID);
	if(  nLocalID <= 0  )
		return GM_NoErr;
	ASSERT( nLocalID > 0 );
	// ���ݱ��ر�Ż�ȡ����ͷ����...
	CCamera * lpCamera = m_lpHaoYiView->FindCameraByID(nLocalID);
	if( lpCamera == NULL )
		return GM_NoErr;
	ASSERT( lpCamera != NULL );
	// �õ�������ͷ������¼��ļ�¼���...
	root["course_id"] = lpCamera->GetRecCourseID();
	// �� JSON ����ת�����ַ���...
	strJson = root.toStyledString();
	// ��Ϸ�������ͷ��Ϣ => ֱ�Ӵ�ת�������п���һЩ����...
	Cmd_Header theHeader = {0};
	UInt32	nSendSize = 0;
	theHeader.m_pkg_len = strJson.size();
	theHeader.m_type = kClientGather;
	theHeader.m_cmd  = lpCmdHeader->m_cmd;
	theHeader.m_sock = lpCmdHeader->m_sock;
	// ׷�������ͷ���������ݰ�����...
	memcpy(m_lpSendBuf, &theHeader, sizeof(theHeader));
	nSendSize += sizeof(theHeader);
	memcpy(m_lpSendBuf+nSendSize, strJson.c_str(), strJson.size());
	nSendSize += strJson.size();
	// ����ͳһ�ķ��ͽӿ�...
	return this->SendData(m_lpSendBuf, nSendSize);
}
//
// ���ӳɹ�֮�󣬷��͵�¼����... 
GM_Error CRemoteSession::SendLoginCmd()
{
	ASSERT( m_lpHaoYiView != NULL );
	// ���Login������Ҫ��JSON���ݰ� => �òɼ��˵�MAC��ַ��ΪΨһ��ʶ...
	string strJson;	Json::Value root;
	root["mac_addr"] = m_lpHaoYiView->m_strMacAddr.GetString();
	root["ip_addr"] = m_lpHaoYiView->m_strIPAddr.GetString();
	strJson = root.toStyledString(); ASSERT( strJson.size() > 0 );
	// ��������ͷ => ���ݳ��� | �ն����� | ������ | �ɼ���MAC��ַ
	UInt32	nSendSize = 0;
	Cmd_Header theHeader = {0};
	theHeader.m_pkg_len = strJson.size();
	theHeader.m_type = kClientGather;
	theHeader.m_cmd  = kCmd_Gather_Login;
	// ׷�������ͷ���������ݰ�����...
	memcpy(m_lpSendBuf, &theHeader, sizeof(theHeader));
	nSendSize += sizeof(theHeader);
	memcpy(m_lpSendBuf+nSendSize, strJson.c_str(), strJson.size());
	nSendSize += strJson.size();
	// ����ͳһ�ķ��ͽӿ�...
	return this->SendData(m_lpSendBuf, nSendSize);
}
//
// ͳһ�ķ��ͽӿ�...
GM_Error CRemoteSession::SendData(LPCTSTR lpDataPtr, int nDataSize)
{
	UInt32	nReturn = 0;
	GM_Error theErr = GM_NoErr;
	theErr = m_TCPSocket.Send(lpDataPtr, nDataSize, &nReturn);
	if( theErr != WSAEWOULDBLOCK )
		return theErr;
	ASSERT( theErr == WSAEWOULDBLOCK );
	// �����й� WSAEWOULDBLOCK �����...
	int iCount = 0;
	while( iCount <= 4 ) {
		::Sleep(50);
		// ����50����֮�󣬼�������...
		theErr = m_TCPSocket.Send(lpDataPtr, nDataSize, &nReturn);
		if( theErr != WSAEWOULDBLOCK )
			return theErr;
		ASSERT( theErr == WSAEWOULDBLOCK );
		++iCount;
	}
	// �������Ͷ�Σ���ȻWSAEWOULDBLOCK��ֱ�ӷ��ش���...
	return GM_NetClose;
}