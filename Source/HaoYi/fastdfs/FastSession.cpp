
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
	// 保存输入的信息...
	m_iRemotePort = nPort;
	m_strRemoteAddr = lpszAddr;
	m_strRemoteIP = m_strRemoteAddr;
	if( m_strRemoteIP.size() <= 0 || m_iRemotePort <= 0 ) {
		MsgLogGM(theErr);
		return theErr;
	}
	// 创建新的套接字...
	theErr = m_TCPSocket.Open(false);
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
		return theErr;
	}
	// 设置保持连接状态 => 每隔5秒发送心跳包...
	m_TCPSocket.KeepAlive();
	// 设置接收缓冲并创建事件对象...
	m_TCPSocket.SetSocketRcvBufSize(128*1024);
	m_TCPSocket.SetSocketRcvBufSize(128*1024);
	theErr = m_TCPSocket.CreateEvent();
	if( theErr != GM_NoErr ) {
		MsgLogGM(theErr);
		return theErr;
	}
	// 填充本机相关信息...
	ASSERT(theErr == GM_NoErr);
	//this->BuildClientPragma();
	m_eState = kOpenState;
	// 使用可能是域名的地址进行链接服务器...
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

	// 拷贝输入地址到一个临时变量...
	LPCTSTR lpszRemoteIP = lpszAddr;
	TCHAR szHostAddr[MAX_PATH] = {0};
	// 通过域名只能获取IPV4的地址，IPV6只能输入地址...
	// 假设输入信息就是一个IPV4域名，IPV6不进行域名解析...
	hostent * lpHost = gethostbyname(lpszAddr);
	if( lpHost != NULL && lpHost->h_addr_list != NULL ) {
		LPSTR lpszTemp = inet_ntoa(*(in_addr*)lpHost->h_addr_list[0]);
		(lpszTemp != NULL) ? strcpy(szHostAddr, lpszTemp) : NULL;
		// 如果解析出了地址，则保存有效地IP地址，并使用IP地址链接服务器...
		m_strRemoteIP.assign(szHostAddr, strlen(szHostAddr));
		lpszRemoteIP = szHostAddr;
	}
	// 通过IP地址链接服务器...
	return m_TCPSocket.Connect(lpszRemoteIP, nPort);
}

GM_Error CFastSession::DisConnect()
{
	m_TCPSocket.Close();
	m_eState = kNoneState;
	return GM_NoErr;
}
//
// 响应网络事件...
GM_Error CFastSession::ProcessEvent(int eventBits)
{
	GM_Error theErr = GM_NoErr;
	// 处理链接事件通知...
	if( eventBits & FD_CONNECT ) {
		theErr = this->OnConnectEvent();
		if( theErr != GM_NoErr ) {
			MsgLogGM(theErr);
			return theErr;
		}
		eventBits &= ~ FD_CONNECT;
	}
	// 处理读取事件通知...
	if( eventBits & FD_READ ) {
		theErr = this->OnReadEvent();
		if( theErr != GM_NoErr ) {
			MsgLogGM(theErr);
			return theErr;
		}
		eventBits &= ~ FD_READ;
	}
	// 处理读取事件通知...
	if( eventBits & FD_WRITE ) {
		eventBits &= ~ FD_WRITE;
	}
	// 不肯能再有其它事件通知...
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
	// 读取当前所有的网络数据...
	theErr = m_TCPSocket.Read(strRead, &theCurRead);
	if( theErr == WSAEWOULDBLOCK ) {
		::Sleep(5);
		return GM_NoErr;
	}
	// 发生网络故障或读取长度为空...
	if( theErr != GM_NoErr || strRead.size() <= 0 ) {
		MsgLogGM(theErr);
		return theErr;
	}
	// 追加读取数据并构造解析头指针...
	m_strRecv.append(strRead);
	// 分发到各个Session当中处理...
	return this->ForRead();
}
//
// 判断会话是否超时...
BOOL CFastSession::IsTimeout()
{
	ASSERT( m_iTimeout > 0 );
	return ((m_iTimeout >= this->GetFrequeTime() ) ? false : true);
}
//
// 重置超时起点...
void CFastSession::ResetTimeout()
{
	m_iTimeout = this->GetFrequeTime() + DEF_NETSESSION_TIMEOUT * 60 * 1000;
}
//
// 得到当前的时间(毫秒)
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
// Tracker会话实现内容...
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
	// 记录Tracker退出信息...
	MsgLogINFO("[~CTrackerSession] - Exit");
}
//
// 发送命令...
GM_Error CTrackerSession::SendCmd(char inCmd)
{
	// 判断是否是链接状态，只有链接状态才发送命令包...
	if( m_eState != kConnectState )
		return GM_NotImplement;
	ASSERT( m_eState == kConnectState );
	// 直接组合命令发送出去...
	UInt32	nReturn = 0;
	int     nSendSize = sizeof(m_TrackerCmd);
	memset(&m_TrackerCmd, 0, nSendSize);
	m_TrackerCmd.cmd = inCmd;
	return m_TCPSocket.Send((char*)&m_TrackerCmd, nSendSize, &nReturn);
}

GM_Error CTrackerSession::ForConnect()
{
	ASSERT( m_eState == kConnectState );
	// 链接成功，发送查询storage命令 => Tracker会返回一个最合适的Storage...
	return this->SendCmd(TRACKER_PROTO_CMD_SERVICE_QUERY_STORE_WITHOUT_GROUP_ONE);
}

GM_Error CTrackerSession::ForRead()
{
	// 得到的数据长度不够，直接返回，等待新数据...
	int nCmdLength = sizeof(TrackerHeader);
	if( m_strRecv.size() < nCmdLength )
		return GM_NoErr;
	ASSERT( m_strRecv.size() >= nCmdLength );
	// 对命令头进行分发处理...
	if( m_TrackerCmd.cmd == TRACKER_PROTO_CMD_SERVICE_QUERY_STORE_WITHOUT_GROUP_ONE ) {
		// 得到的数据长度不够，直接报错，等待新的链接接入，Storage上线后不会主动汇报...
		if( m_strRecv.size() < (nCmdLength + TRACKER_QUERY_STORAGE_STORE_BODY_LEN) ) {
			MsgLogGM(GM_NetNoData);
			return GM_NetNoData;
		}
		// 对获取的数据进行转移处理...
		char * in_buff = (char*)m_strRecv.c_str() + nCmdLength;
		memset(&m_NewStorage, 0, sizeof(m_NewStorage));
		memcpy(m_NewStorage.group_name, in_buff, FDFS_GROUP_NAME_MAX_LEN);
		memcpy(m_NewStorage.ip_addr, in_buff + FDFS_GROUP_NAME_MAX_LEN, IP_ADDRESS_SIZE-1);
		m_NewStorage.port = (int)buff2long(in_buff + FDFS_GROUP_NAME_MAX_LEN + IP_ADDRESS_SIZE - 1);
		m_NewStorage.store_path_index = (int)(*(in_buff + FDFS_GROUP_NAME_MAX_LEN + IP_ADDRESS_SIZE - 1 + FDFS_PROTO_PKG_LEN_SIZE)); // 内存中只有一个字节...
		TRACE("Group = %s, Storage = %s:%d, PathIndex = %d\n", m_NewStorage.group_name, m_NewStorage.ip_addr, m_NewStorage.port, m_NewStorage.store_path_index);
		// 将缓冲区进行减少处理...
		m_strRecv.erase(0, nCmdLength + TRACKER_QUERY_STORAGE_STORE_BODY_LEN);
		// 通知消息窗口，可以创建新的存储会话，进行数据上传的操作了...
		//::PostMessage(m_hWndMsg, WM_EVENT_SESSION_MSG, OPT_OpenStorage, (LPARAM)&m_NewStorage);
		// 发起新的命令 => 查询所有的组列表...
		return this->SendCmd(TRACKER_PROTO_CMD_SERVER_LIST_ALL_GROUPS);
	} else if( m_TrackerCmd.cmd == TRACKER_PROTO_CMD_SERVER_LIST_ALL_GROUPS ) {
		// 得到的数据长度不够，直接返回，等待新数据...
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
// Storage会话实现内容...
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
	// 停止上传线程...
	this->StopAndWaitForThread();
	// 关闭文件句柄...
	this->CloseUpFile();
	// 打印当前正在上传的文件和位置...
	// 没有上传完毕的文件，fdfs-storage会自动(回滚)删除服务器端副本，下次再上传时重新上传。。。
	// STORAGE_PROTO_CMD_UPLOAD_FILE 与 STORAGE_PROTO_CMD_APPEND_FILE 无区别...
	if( m_llFileSize != m_llSendSize && m_llSendSize > 0 ) {
		TRACE("File = %s, Send = %I64d, Size = %I64d\n", m_strCurFile, m_llSendSize, m_llFileSize);
	}
	// 关闭数据包缓冲区...
	if( m_lpPackBuffer != NULL ) {
		delete [] m_lpPackBuffer;
		m_lpPackBuffer = NULL;
	}
	// 打印CStorageSession退出信息...
	MsgLogINFO("[~CStorageSession] - Exit");
}

void CStorageSession::Entry()
{
	int   nRetValue = 0;
	DWORD dwStartTimeMS = ::GetTickCount();
	while( !this->IsStopRequested() ) {
		// 如果时间间隔大于了1000毫秒，则计算一次发送码流...
		if( (::GetTickCount() - dwStartTimeMS) >= 1000 ) {
			dwStartTimeMS = ::GetTickCount();
			m_dwSendKbps = m_dwCurSendByte * 8 / 1024;
			m_dwCurSendByte = 0;
			if( m_dwSendKbps > 0 ) {
				TRACE("Send = %lu Kbps\n", m_dwSendKbps);
			}
		}
		// 如果没有找到一个有效的句柄，休整一下，继续寻找...
		if( this->FindOneFile() == NULL ) {
			::Sleep(50);
			continue;
		}
		// 当前的文件句柄是有效的，读取并发送数据包...
		nRetValue = this->SendOnePacket();
		// < 0 打印错误信息，等待会话被删除...
		// 注：删除操作只有一个地方，不要从多个地方删除，容易混乱...
		if( nRetValue < 0 ) {
			ASSERT( this->GetErrorCode() > 0 );
			TRACE("[CStorageSession::SendOnePacket] - Error = %lu\n", this->GetErrorCode());
			continue;
		}
		// == 0 马上继续...
		if( nRetValue == 0 ) {
			::Sleep(5);
			continue;
		}
		// > 0 说明还有,马上发送...
		ASSERT( nRetValue > 0 );
	}
}
//
// 关闭当前正在上传的文件...
void CStorageSession::CloseUpFile()
{
	// 对资源进行线程保护...
	OS_MUTEX_LOCKER(&m_Mutex);
	// 关闭正在上传的文件句柄...
	if( m_lpCurFile != NULL ) {
		fclose(m_lpCurFile);
		m_lpCurFile = NULL;
	}
}
//
// 找到一个可以上传的文件...
FILE * CStorageSession::FindOneFile()
{
	// 对资源进行线程保护...
	OS_MUTEX_LOCKER(&m_Mutex);

	// 当前会话的文件已经发送完毕，等待服务器返回远程文件名..
	if((m_llFileSize > 0) && (m_llFileSize == m_llSendSize))
		return NULL;
	// 如果当前会话已经发生错误，等待被删除...
	if( this->GetErrorCode() > 0 )
		return NULL;
	ASSERT( this->GetErrorCode() <= 0 );
	// 判断当前文件句柄是否有效...
	if( m_lpCurFile != NULL )
		return m_lpCurFile;
	ASSERT( m_lpCurFile == NULL );
	// 获取存放jpg和mp4文件的配置路径...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	string & strSavePath = theConfig.GetSavePath();
	string   strRootPath = strSavePath;
	if( strRootPath.size() <= 0 ) {
		MsgLogGM(GM_File_Path_Err);
		return NULL;
	}
	// 路径是否需要加上反斜杠...
	BOOL bHasSlash = false;
	bHasSlash = ((strRootPath.at(strRootPath.size()-1) == '\\') ? true : false);
	strRootPath += (bHasSlash ? "*.*" : "\\*.*");
	// 准备查找需要的变量...
	GM_Error theErr = GM_NoErr;
	WIN32_FIND_DATA theFileData = {0};
	HANDLE	hFind = INVALID_HANDLE_VALUE;
	LPCTSTR	lpszExt = NULL;
	BOOL	bIsOK = true;
	CString strFileName;
	// 查找第一个文件或目录...
	hFind = ::FindFirstFile(strRootPath.c_str(), &theFileData);
	if( hFind != INVALID_HANDLE_VALUE ) {
		while( bIsOK ) {
			if(	(theFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||	// 目录
				(theFileData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) ||		// 系统
				(strcmp(theFileData.cFileName, ".") == 0) ||					// 当前
				(strcmp(theFileData.cFileName, "..") == 0) )					// 上级
			{
				bIsOK = ::FindNextFile(hFind, &theFileData);
				continue;
			}
			// 判断扩展名是否有效 => .jpg .jpeg .mp4 有效...
			lpszExt = ::PathFindExtension(theFileData.cFileName);
			if( (stricmp(lpszExt, ".jpg") != 0) && (stricmp(lpszExt, ".jpeg") != 0) && (stricmp(lpszExt, ".mp4") != 0) ) {
				bIsOK = ::FindNextFile(hFind, &theFileData);
				continue;
			}
			// 首先，组合有效的文件全路径，获取文件长度...
			strFileName.Format(bHasSlash ? "%s%s" : "%s\\%s", strSavePath.c_str(), theFileData.cFileName);
			ULONGLONG llSize = CUtilTool::GetFileFullSize(strFileName);
			// 文件长度为0，查找下一个文件...
			if( llSize <= 0 ) {
				bIsOK = ::FindNextFile(hFind, &theFileData);
				continue;
			}
			// 打开文件句柄失败，查找下一个文件...
			FILE * lpFile = fopen(strFileName, "rb");
			if( lpFile == NULL ) {
				bIsOK = ::FindNextFile(hFind, &theFileData);
				continue;
			}
			// 向服务器发送指令头信息 => 文件长度 + 扩展名(去掉点号)...
			theErr = this->SendCmdHeader(llSize, lpszExt + 1);
			// 发送指令头失败，关闭文件，继续下一个...
			if( theErr != GM_NoErr ) {
				MsgLogGM(theErr);
				fclose(lpFile); lpFile = NULL;
				bIsOK = ::FindNextFile(hFind, &theFileData);
				continue;
			}
			// 一切正常，关闭查找句柄...
			::FindClose(hFind);
			ASSERT( lpFile != NULL );
			// 保存文件全路径，长度，句柄...
			m_strCurFile = strFileName;
			m_lpCurFile = lpFile;
			m_llFileSize = llSize;
			m_llSendSize = 0;
			return m_lpCurFile;
		}
	}
	// 如果查找句柄不为空，直接关闭之...
	if( hFind != INVALID_HANDLE_VALUE ) {
		::FindClose(hFind);
	}
	return NULL;
}
//
// 发送指令信息头数据包...
GM_Error CStorageSession::SendCmdHeader(ULONGLONG llSize, LPCTSTR lpszExt)
{
	// 准备需要的变量...
	GM_Error theErr = GM_NoErr;
	UInt32	 nReturn = 0;
	UInt32	 nLength = 0;
	char  *  lpBuf = NULL;
	char     szBuf[MAX_PATH] = {0};
	int      n_pkg_len = 1 + sizeof(ULONGLONG) + FDFS_FILE_EXT_NAME_MAX_LEN;
	// 指令包长度 => 目录编号(1) + 文件长度(8) + 文件扩展名(6)

	// 填充指令头 => 指令编号 + 状态 + 指令包长度(不含指令头)...
	// STORAGE_PROTO_CMD_UPLOAD_FILE 与 STORAGE_PROTO_CMD_APPEND_FILE 无区别...
	// 没有上传完毕的文件，fdfs-storage会自动(回滚)删除服务器端副本，下次再上传时重新上传。。。
	TrackerHeader * lpHeader = (TrackerHeader*)szBuf;
	lpHeader->cmd = STORAGE_PROTO_CMD_UPLOAD_FILE;
	lpHeader->status = 0;
	long2buff(llSize + n_pkg_len, lpHeader->pkg_len);

	// 填充指令包数据...
	lpBuf = szBuf + sizeof(TrackerHeader);								// 将指针定位到数据区...
	lpBuf[0] = m_NewStorage.store_path_index;							// 目录编号   => 0 - 1  => 1个字节
	long2buff(llSize, lpBuf + 1);										// 文件长度   => 1 - 9  => 8个字节 ULONGLONG
	memcpy(lpBuf + 1 + sizeof(ULONGLONG), lpszExt, strlen(lpszExt));	// 文件扩展名 => 9 - 15 => 6个字节 FDFS_FILE_EXT_NAME_MAX_LEN

	// 发送上传指令头和指令包数据 => 命令头 + 数据长度...
	nLength = sizeof(TrackerHeader) + n_pkg_len;
	theErr = m_TCPSocket.Send(szBuf, nLength, &nReturn);
	// 如果发生阻塞，存放起来，下次发送...
	if( theErr == WSAEWOULDBLOCK ) {
		ASSERT( m_strCurData.size() <= 0 );
		m_strCurData.append(szBuf, nLength);
		return GM_NoErr;
	}
	// 如果发送失败，直接返回...
	if( theErr != GM_NoErr )
		return theErr;
	// 发送成功，累加发送字节数...
	ASSERT( theErr == GM_NoErr );
	m_dwCurSendByte += nReturn;
	return GM_NoErr;
}
//
// 发送一个有效数据包...
int CStorageSession::SendOnePacket()
{
	// 对资源进行线程保护...
	OS_MUTEX_LOCKER(&m_Mutex);

	// 首先，发送还没有发送出去的数据包...
	GM_Error theErr = GM_NoErr;
	UInt32	 nReturn = 0;
	if( m_strCurData.size() > 0 ) {
		// 发送数据包，发生阻塞，等待下次发送...
		theErr = m_TCPSocket.Send(m_strCurData.c_str(), m_strCurData.size(), &nReturn);
		if( theErr == WSAEWOULDBLOCK )
			return 0;
		// 发生错误，设置错误号，关闭连接，关闭句柄...
		if( theErr != GM_NoErr ) {
			this->SetErrorCode(theErr);
			m_TCPSocket.Close();
			MsgLogGM(theErr);
			return -1;
		}
		// 发送完成，累加发送量，缓冲置空...
		ASSERT( m_strCurData.size() == nReturn );
		m_dwCurSendByte += nReturn;
		m_llSendSize += nReturn;
		m_strCurData.clear();
	}
	ASSERT( m_strCurData.size() <= 0 );
	// 判断文件是否已经发送完毕了...
	// 关闭句柄，等待服务器返回远程文件名...
	// 文件长度和文件名不要复位，通知网站时使用...
	if( m_llSendSize >= m_llFileSize ) {
		this->CloseUpFile();
		return 0;
	}
	ASSERT( m_strCurData.empty() );
	ASSERT( m_lpCurFile != NULL );
	// 从文件中读取一个数据包的数据量...
	memset(m_lpPackBuffer, 0, kPackSize + 1);
	int nSize = fread(m_lpPackBuffer, 1, kPackSize, m_lpCurFile);
	// 如果读取文件失败，设置错误号，关闭连接，关闭句柄...
	if( nSize <= 0 ) {
		theErr = GM_File_Read_Err;
		this->SetErrorCode(theErr);
		this->CloseUpFile();
		m_TCPSocket.Close();
		MsgLogGM(theErr);
		return -1;
	}
	// 发送到存储服务器...
	theErr = m_TCPSocket.Send(m_lpPackBuffer, nSize, &nReturn);
	// 发生阻塞，将阻塞数据缓存起来，等待下次发送...
	if( theErr == WSAEWOULDBLOCK ) {
		m_strCurData.append(m_lpPackBuffer, nSize);
		return 0;
	}
	// 发生错误，设置错误号，关闭连接，关闭句柄...
	if( theErr != GM_NoErr ) {
		this->SetErrorCode(theErr);
		this->CloseUpFile();
		m_TCPSocket.Close();
		MsgLogGM(theErr);
		return -1;
	}
	// 发送成功，累加发送量，继续新的发送...
	ASSERT( nSize == nReturn );
	m_dwCurSendByte += nReturn;
	m_llSendSize += nReturn;
	return 1;
}

GM_Error CStorageSession::ForConnect()
{
	ASSERT( m_eState == kConnectState );

	// 直接启动上传线程...
	this->Start();

	return GM_NoErr;
}

GM_Error CStorageSession::ForRead()
{
	// 对资源进行线程保护...
	OS_MUTEX_LOCKER(&m_Mutex);

	// 得到的数据长度不够，等待新的接收命令...
	int nCmdLength = sizeof(TrackerHeader);
	if( m_strRecv.size() < nCmdLength )
		return GM_NoErr;
	GM_Error theErr = GM_NoErr;
	int nDataSize = m_strRecv.size() - nCmdLength;
	LPCTSTR lpszDataBuf = m_strRecv.c_str() + nCmdLength;
	TrackerHeader * lpHeader = (TrackerHeader*)m_strRecv.c_str();
	// 判断服务器返回的状态是否正确...
	if( lpHeader->status != 0 ) {
		theErr = GM_FDFS_Status_Err;
		MsgLogGM(theErr);
		return theErr;
	}
	// 判断返回的数据区长度是否正确 => 必须大于 FDFS_GROUP_NAME_MAX_LEN
	if( nDataSize <= FDFS_GROUP_NAME_MAX_LEN ) {
		theErr = GM_FDFS_Data_Err;
		MsgLogGM(theErr);
		return theErr;
	}
	// 得到反馈的 组名称 + 文件名...
	CString strFileFDFS;
	TCHAR group_name[FDFS_GROUP_NAME_MAX_LEN + 1] = {0};
	TCHAR remote_filename[FDFS_REMOTE_NAME_MAX_SIZE] = {0};
	memcpy(group_name, lpszDataBuf, FDFS_GROUP_NAME_MAX_LEN);
	memcpy(remote_filename, lpszDataBuf + FDFS_GROUP_NAME_MAX_LEN, nDataSize - FDFS_GROUP_NAME_MAX_LEN + 1);
	strFileFDFS.Format("%s/%s", group_name, remote_filename);
	// 关闭上传文件句柄...
	this->CloseUpFile();
	// 向网站汇报并保存FDFS文件记录...
	this->doWebSaveFDFS(m_strCurFile, strFileFDFS, m_llFileSize);
	// 删除文件，打印远程文件id...
	ASSERT( m_lpCurFile == NULL && m_llFileSize > 0 );
	TRACE("Local = %s, Remote = %s, Size = %I64d\n", m_strCurFile, strFileFDFS, m_llFileSize);
	if( !::DeleteFile(m_strCurFile) ) {
		MsgLogGM(GM_File_Del_Err);
	}
	// 复位文件名，复位文件长度，自动发起新文件上传操作...
	m_strCurFile.Empty();
	m_llFileSize = 0;
	m_llSendSize = 0;
	// 重置接收缓冲区，等待新的数据...
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
// 返回一个json数据包...
void CStorageSession::doCurlPost(char * pData, size_t nSize)
{
	string strUTF8Data;
	string strANSIData;
	Json::Reader reader;
	Json::Value  value;
	// 将UTF8网站数据转换成ANSI格式...
	strUTF8Data.assign(pData, nSize);
	strANSIData = CUtilTool::UTF8_ANSI(strUTF8Data.c_str());
	// 解析转换后的json数据包...
	if( !reader.parse(strANSIData, value) ) {
		MsgLogGM(GM_Err_Json);
		return;
	}
	// 获取返回的采集端编号和错误信息...
	if( value.isMember("err_code") && value["err_code"].asBool() ) {
		string & strData = value["err_msg"].asString();
		string & strMsg = CUtilTool::UTF8_ANSI(strData.c_str());
		MsgLogINFO(strMsg.c_str());
		return;
	}
	// 返回数据库里记录的编号，不用，直接丢弃...
	/*int nRetID = 0;
	Json::Value theID;
	if( value.isMember("image_id") ) {
		theID = value["image_id"];
	} else if( value.isMember("record_id") ) {
		theID = value["record_id"];
	}
	// 需要根据类型分别转换...
	if( theID.isInt() ) {
		nRetID = theID.asInt();
	} else if( theID.isString() ) {
		nRetID = atoi(theID.asString().c_str());
	}*/
}
//
// 在网站上保存FDFS文件记录...
BOOL CStorageSession::doWebSaveFDFS(CString & strPathFile, CString & strFileFDFS, ULONGLONG llFileSize)
{
	// 判断数据是否有效...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	string  & strWebAddr = theConfig.GetWebAddr();
	int nWebPort = theConfig.GetWebPort();
	if( nWebPort <= 0 || strWebAddr.size() <= 0 || strPathFile.GetLength() <= 0 || strFileFDFS.GetLength() <= 0 )
		return false;
	// 从文件完整名中解析出需要的文件名和扩展名...
	TCHAR szExt[32] = {0};
	TCHAR szDriver[32] = {0};
	TCHAR szDir[MAX_PATH] = {0};
	TCHAR szSrcName[MAX_PATH*2] = {0};
	// szSrcName => jpg => uniqid_DBCameraID
	// szSrcName => mp4 => uniqid_DBCameraID_CourseID_Duration
	_splitpath(strPathFile, szDriver, szDir, szSrcName, szExt);
	// SrcName转换成UTF8格式, 再进行URIEncode...
	string strUTF8Name = CUtilTool::ANSI_UTF8(szSrcName);
	StringParser::EncodeURI(strUTF8Name.c_str(), strUTF8Name.size(), szSrcName, MAX_PATH*2);
	// 准备需要的汇报数据 => POST数据包...
	CString strPost, strUrl;
	strPost.Format("ext=%s&file_src=%s&file_fdfs=%s&file_size=%I64d", szExt, szSrcName, strFileFDFS, llFileSize);
	strUrl.Format("http://%s:%d/wxapi.php/Gather/saveFDFS", strWebAddr.c_str(), nWebPort);
	// 调用Curl接口，汇报采集端信息...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if( curl == NULL )
			break;
		// 设定curl参数，采用post模式...
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
	// 释放资源...
	if( curl != NULL ) {
		curl_easy_cleanup(curl);
	}
	return true;
}
//////////////////////////////////////////////////
// Remote会话实现内容...
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
	// 链接成功，立即发送登录命令 => Cmd_Header + JSON...
	return this->SendLoginCmd();
}

GM_Error CRemoteSession::ForRead()
{
	// 这里网络数据会发生粘滞现象，因此，需要循环执行...
	while( m_strRecv.size() > 0 ) {
		// 得到的数据长度不够，直接返回，等待新数据...
		int nCmdLength = sizeof(Cmd_Header);
		if( m_strRecv.size() < nCmdLength )
			return GM_NoErr;
		ASSERT( m_strRecv.size() >= nCmdLength );
		Cmd_Header * lpCmdHeader = (Cmd_Header*)m_strRecv.c_str();
		LPCTSTR lpDataPtr = m_strRecv.c_str() + sizeof(Cmd_Header);
		int nDataSize = m_strRecv.size() - sizeof(Cmd_Header);
		// 已获取的数据长度不够，直接返回，等待新数据...
		if( nDataSize < lpCmdHeader->m_pkg_len )
			return GM_NoErr;
		ASSERT( nDataSize >= lpCmdHeader->m_pkg_len );
		ASSERT( lpCmdHeader->m_type == kClientPHP || lpCmdHeader->m_type == kClientPlay || lpCmdHeader->m_type == kClientLive ); 
		// 开始分发转发过来的PHP命令...
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
		// 删除已经处理完毕的数据 => Header + pkg_len...
		m_strRecv.erase(0, lpCmdHeader->m_pkg_len + sizeof(Cmd_Header));
		// 判断返回结果的有效性...
		if( theErr != GM_NoErr ) {
			MsgLogGM(theErr);
			return theErr;
		}
		// 如果还有数据，则继续解析命令...
		ASSERT(theErr == GM_NoErr);
	}
	return GM_NoErr;
}
//
// 2017.08.08 - by jackey => 新增中转命令...
// 处理PHP转发的开始或停止通道命令...
GM_Error CRemoteSession::doPHPCameraOperate(LPCTSTR lpData, int nSize, BOOL bIsStart)
{
	// 判断输入数据的有效性...
	if( nSize <= 0 || lpData == NULL )
		return GM_NoErr;
	ASSERT( nSize > 0 && lpData != NULL );
	string strUTF8Data;
	Json::Reader reader;
	Json::Value  value;
	// 将UTF8网站数据转换成ANSI格式 => 由于是php编码过的，转换后无效，获取具体数据之后，还要转换一遍...
	GM_Error theErr = GM_Err_Json;
	strUTF8Data.assign(lpData, nSize);
	// 解析转换后的JSON数据包 => PHP编码后的数据，转换无效，仍然是UTF8格式...
	if( !reader.parse(strUTF8Data, value) ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// 判断获取数据的有效性...
	if( !value.isMember("camera_id") ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// 获取通道的数据库编号...
	int nDBCameraID = atoi(CUtilTool::getJsonString(value["camera_id"]).c_str());
	// 开始查找对应的摄像头本地编号...
	int nLocalID = -1;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetDBCameraID(nDBCameraID, nLocalID);
	if(  nLocalID <= 0  ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// 根据本地编号获取摄像头对象...
	CCamera * lpCamera = m_lpHaoYiView->FindCameraByID(nLocalID);
	if( lpCamera == NULL || lpCamera->GetVideoWnd() == NULL ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// 将当前通道设置成焦点通道...
	lpCamera->GetVideoWnd()->doFocusAction();
	// 向主视图发送开始或停止模拟点击命令...
	int nCmdID = (bIsStart ? ID_LOGIN_DVR : ID_LOGOUT_DVR);
	m_lpHaoYiView->PostMessage(WM_COMMAND, MAKEWPARAM(nCmdID, BN_CLICKED), NULL);
	return GM_NoErr;
}
//
// 处理直播服务器的中转命令 => 无需回应...
GM_Error CRemoteSession::doCmdLiveVary(LPCTSTR lpData, int nSize)
{
	// 判断输入数据的有效性...
	if( nSize <= 0 || lpData == NULL )
		return GM_NoErr;
	ASSERT( nSize > 0 && lpData != NULL );
	string strUTF8Data;
	Json::Reader reader;
	Json::Value  value;
	// 将UTF8网站数据转换成ANSI格式 => 由于是php编码过的，转换后无效，获取具体数据之后，还要转换一遍...
	GM_Error theErr = GM_Err_Json;
	strUTF8Data.assign(lpData, nSize);
	// 解析转换后的JSON数据包 => PHP编码后的数据，转换无效，仍然是UTF8格式...
	if( !reader.parse(strUTF8Data, value) ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// 判断获取数据的有效性...
	if( !value.isMember("rtmp_live") || !value.isMember("rtmp_user") ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// 获取通道的数据库编号和该通道上的用户数...
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
	// 只有用户数 <= 0 才进行停止操作...
	if( nUserCount > 0 )
		return GM_NoErr;
	ASSERT( nUserCount <= 0 );
	// 开始查找对应的摄像头本地编号...
	int nLocalID = -1;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetDBCameraID(nDBCameraID, nLocalID);
	if(  nLocalID <= 0  ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// 根据本地编号获取摄像头对象...
	CCamera * lpCamera = m_lpHaoYiView->FindCameraByID(nLocalID);
	if( lpCamera == NULL ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	ASSERT( lpCamera != NULL );
	// 如果该通道下的用户数已经为0，则调用删除接口...
	// 这里不能直接删除，会卡死，最好通过发送窗口消息...
	lpCamera->doStopLiveMessage();
	return GM_NoErr;
}
//
// 处理直播播放器的中转命令 => 无需回应 => 2017.06.15 - by jackey...
GM_Error CRemoteSession::doCmdPlayLogin(string & inData)
{
	// 从传递过来的数据中解析出JSON内容，有效性前面已经判断了...
	Cmd_Header * lpCmdHeader = (Cmd_Header*)inData.c_str();
	LPCTSTR lpDataPtr = inData.c_str() + sizeof(Cmd_Header);
	int nDataSize = lpCmdHeader->m_pkg_len;
	// 解析JSON对象...
	Json::Reader reader;
	Json::Value  value, root;
	string strUTF8Data, strRtmpUrl;
	GM_Error theErr = GM_Err_Json;
	strUTF8Data.assign(lpDataPtr, nDataSize);
	// 解析转换后的JSON数据包 => PHP编码后的数据，转换无效，仍然是UTF8格式 => 这里是数字列表，无需转化...
	if( !reader.parse(strUTF8Data, value) ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// 判断传递过来的数据有效性...
	do {
		if( !value.isMember("rtmp_url") || !value.isMember("rtmp_live") || !value.isMember("rtmp_user") ) {
			MsgLogGM(theErr);
			break;
		}
		// 解析出rtmp_url地址，保存起来...
		strRtmpUrl = value["rtmp_url"].asString();
		// 解析出通道的数据库编号，用户数量...
		int nDBCameraID, nUserCount = 0;
		Json::Value & theDBCameraID = value["rtmp_live"];
		if( theDBCameraID.isString() ) {
			nDBCameraID = atoi(theDBCameraID.asString().c_str());
		} else if( theDBCameraID.isInt() ) {
			nDBCameraID = theDBCameraID.asInt();
		}
		// 解析出该通道上的用户数...
		Json::Value & theUserCount = value["rtmp_user"];
		if( theUserCount.isString() ) {
			nUserCount = atoi(theUserCount.asString().c_str());
		} else if( theUserCount.isInt() ) {
			nUserCount = theUserCount.asInt();
		}
		// 开始查找对应的摄像头本地编号...
		int nLocalID = -1;
		CXmlConfig & theConfig = CXmlConfig::GMInstance();
		theConfig.GetDBCameraID(nDBCameraID, nLocalID);
		if(  nLocalID <= 0  ) {
			MsgLogGM(theErr);
			break;
		}
		ASSERT( nLocalID > 0 );
		// 根据本地编号获取摄像头对象...
		CCamera * lpCamera = m_lpHaoYiView->FindCameraByID(nLocalID);
		if( lpCamera == NULL ) {
			MsgLogGM(theErr);
			break;
		}
		ASSERT( lpCamera != NULL );
		// 2017.06.15 - by jackey => 启动直播上传通道 => 无需延时，直接返回...
		int nResult = lpCamera->doStartLivePush(strRtmpUrl);
		(nResult < 0) ? MsgLogGM(theErr) : NULL;
	}while( false );
	// 2017.06.15 - by jackey => 无需回应，直接返回...
	return GM_NoErr;
}
//
// 转发命令到指定的直播播放器...
/*GM_Error CRemoteSession::doTransmitPlayer(int nPlayerSock, string & strRtmpUrl, GM_Error inErr)
{
	// 准备好需要发送的缓存区...
	string strJson;
	Json::Value root;
	root["err_code"] = (inErr != GM_NoErr) ? ERR_PUSH_FAIL : ERR_OK;
	root["rtmp_url"] = strRtmpUrl;
	// 将 JSON 数据转换成字符串...
	strJson = root.toStyledString();
	// 组合反馈命令头信息 => 直接从转发数据中拷贝一些过来...
	UInt32	nSendSize = 0;
	TCHAR	szSendBuf[1024] = {0};
	Cmd_Header theHeader = {0};
	theHeader.m_pkg_len = strJson.size();
	theHeader.m_type = kClientGather;
	theHeader.m_cmd  = kCmd_Play_Login;
	theHeader.m_sock = nPlayerSock;
	// 追加命令包头和命令数据包内容...
	memcpy(szSendBuf, &theHeader, sizeof(theHeader));
	nSendSize += sizeof(theHeader);
	memcpy(szSendBuf+nSendSize, strJson.c_str(), strJson.size());
	nSendSize += strJson.size();
	// 调用统一的发送接口...
	return this->SendData(szSendBuf, nSendSize);
}*/
//
// 处理PHP客服端发送的设置摄像头名称命令...
// 数据格式 => Cmd_Header + JSON...
// JSON格式 => camera_id: xxx, camera_name: xxx...
GM_Error CRemoteSession::doPHPSetCameraName(LPCTSTR lpData, int nSize)
{
	// 判断输入数据的有效性...
	if( nSize <= 0 || lpData == NULL )
		return GM_NoErr;
	ASSERT( nSize > 0 && lpData != NULL );
	string strANSIData;
	string strUTF8Data;
	Json::Reader reader;
	Json::Value  value;
	// 将UTF8网站数据转换成ANSI格式 => 由于是php编码过的，转换后无效，获取具体数据之后，还要转换一遍...
	GM_Error theErr = GM_Err_Json;
	strUTF8Data.assign(lpData, nSize);
	strANSIData = CUtilTool::UTF8_ANSI(strUTF8Data.c_str());
	// 解析转换后的JSON数据包 => PHP编码后的数据，转换无效，仍然是UTF8格式...
	if( !reader.parse(strANSIData, value) ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// 判断获取数据的有效性...
	if( !value.isMember("camera_id") || !value.isMember("camera_name") ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// 获取通道编号...
	int nDBCameraID = 0;
	Json::Value & theDBCameraID = value["camera_id"];
	if( theDBCameraID.isString() ) {
		nDBCameraID = atoi(theDBCameraID.asString().c_str());
	} else if( theDBCameraID.isInt() ) {
		nDBCameraID = theDBCameraID.asInt();
	}
	// 获取通道名称...
	string strDBCameraName;
	Json::Value & theCameraName = value["camera_name"];
	if( theCameraName.isString() ) {
		string & strName = theCameraName.asString();
		strDBCameraName = CUtilTool::UTF8_ANSI(strName.c_str());
	}
	// 判断获取数据的有效性...
	if( nDBCameraID <= 0 || strDBCameraName.size() <= 0 ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// 开始查找对应的摄像头本地编号...
	int nLocalID = -1;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetDBCameraID(nDBCameraID, nLocalID);
	if(  nLocalID <= 0  )
		return GM_NoErr;
	ASSERT( nLocalID > 0 );
	// 根据本地编号获取摄像头对象...
	CCamera * lpCamera = m_lpHaoYiView->FindCameraByID(nLocalID);
	if( lpCamera == NULL )
		return GM_NoErr;
	ASSERT( lpCamera != NULL );
	// 将获取的摄像头名称更新到配置文件当中...
	GM_MapData theMapLoc;
	theConfig.GetCamera(nLocalID, theMapLoc);
	theMapLoc["Name"] = strDBCameraName;
	theConfig.SetCamera(nLocalID, theMapLoc);
	// 将获取的摄像头名称更新到摄像头窗口对象当中...
	CString strTitleName = strDBCameraName.c_str();
	STREAM_PROP theProp = lpCamera->GetStreamProp();
	lpCamera->UpdateWndTitle(theProp, strTitleName);
	// 调用父窗口接口，更新右侧和左侧焦点窗口的摄像头名称...
	m_lpHaoYiView->UpdateFocusTitle(nLocalID, strTitleName);
	// 将获取的摄像头名称保存到系统配置文件当中...
	theConfig.GMSaveConfig();
	return GM_NoErr;
}
//
// 处理PHP客服端发送的设置录像课表的命令...
GM_Error CRemoteSession::doPHPSetCourseOpt(LPCTSTR lpData, int nSize)
{
	// 判断输入数据的有效性...
	if( nSize <= 0 || lpData == NULL )
		return GM_NoErr;
	ASSERT( nSize > 0 && lpData != NULL );
	string strANSIData;
	string strUTF8Data;
	Json::Reader reader;
	Json::Value  value;
	// 将UTF8网站数据转换成ANSI格式 => 由于是php编码过的，转换后无效，获取具体数据之后，还要转换一遍...
	GM_Error theErr = GM_Err_Json;
	strUTF8Data.assign(lpData, nSize);
	strANSIData = CUtilTool::UTF8_ANSI(strUTF8Data.c_str());
	// 解析转换后的JSON数据包 => PHP编码后的数据，转换无效，仍然是UTF8格式...
	if( !reader.parse(strANSIData, value) ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	/////////////////////////////////////////////////////////////////////////
	// 注意：必须传递 data 和 camera_id...
	/////////////////////////////////////////////////////////////////////////
	// 判断获取数据的有效性...
	if( !value.isMember("data") || !value.isMember("camera_id") ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// 获取通道编号...
	string & strDBCameraID = CUtilTool::getJsonString(value["camera_id"]);
	int nDBCameraID = atoi(strDBCameraID.c_str());
	// 开始查找对应的摄像头本地编号...
	int nLocalID = -1;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetDBCameraID(nDBCameraID, nLocalID);
	if(  nLocalID <= 0  )
		return GM_NoErr;
	// 解析 Course 记录...
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
		// 如果记录编号或操作编号无效，不执行...
		if( nCourseID <= 0 || nOperate <= 0 || nWeekID < 0 )
			continue;
		// 通知父窗口课表记录发生了变化...
		ASSERT( nCourseID > 0 && nOperate > 0 && nLocalID > 0 );
		m_lpHaoYiView->doCourseChanged(nOperate, nLocalID, theMapData);
	}
	return GM_NoErr;
}
//
// 2017.06.14 - by jackey => 这个命令没用了，采集端主动汇报通道状态...
// 处理PHP客服端需要的摄像头状态的命令 => 需要返回 JSON 数据...
// 数据格式 => Cmd_Header + JSON...
// JSON格式 => 8:7:6:5
/*GM_Error CRemoteSession::doPHPGetCameraStatus(string & inData)
{
	// 从传递过来的数据中解析出JSON内容，有效性前面已经判断了...
	Cmd_Header * lpCmdHeader = (Cmd_Header*)inData.c_str();
	LPCTSTR lpDataPtr = inData.c_str() + sizeof(Cmd_Header);
	int nDataSize = lpCmdHeader->m_pkg_len;
	// 解析JSON对象...
	Json::Reader reader;
	Json::Value  value, root;
	string strUTF8Data, strJson;
	GM_Error theErr = GM_Err_Json;
	strUTF8Data.assign(lpDataPtr, nDataSize);
	// 解析转换后的JSON数据包 => PHP编码后的数据，转换无效，仍然是UTF8格式 => 这里是数字列表，无需转化...
	if( !reader.parse(strUTF8Data, value) ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// 遍历这个摄像头编号列表...
	Json::Value::Members & member = value.getMemberNames();
	for(auto iter = member.begin(); iter != member.end(); ++iter) {
		// 把MAC地址节点排除掉..
		string & strKey = (*iter);
		if( stricmp(strKey.c_str(), "mac_addr") == 0 )
			continue;
		// 将通道节点状态存放起来...
		string & strItem = CUtilTool::getJsonString(value[*iter]);
		int nDBCameraID = atoi(strItem.c_str());
		root[strItem] = m_lpHaoYiView->GetCameraStatusByDBID(nDBCameraID);
	}
	// 将 JSON 数据转换成字符串...
	strJson = root.toStyledString();
	// 组合反馈命令头信息 => 直接从转发数据中拷贝一些过来...
	Cmd_Header theHeader = {0};
	UInt32	nSendSize = 0;
	theHeader.m_pkg_len = strJson.size();
	theHeader.m_type = kClientGather;
	theHeader.m_cmd  = lpCmdHeader->m_cmd;
	theHeader.m_sock = lpCmdHeader->m_sock;
	// 追加命令包头和命令数据包内容...
	memcpy(m_lpSendBuf, &theHeader, sizeof(theHeader));
	nSendSize += sizeof(theHeader);
	memcpy(m_lpSendBuf+nSendSize, strJson.c_str(), strJson.size());
	nSendSize += strJson.size();
	// 调用统一的发送接口...
	return this->SendData(m_lpSendBuf, nSendSize);
}*/
//
// 2017.07.01 - by jackey => 这个命令依然有效，需要转发给 php 客户端...
// 处理PHP客服端需要的摄像头状态的命令 => 需要返回 JSON 数据...
// 数据格式 => Cmd_Header + JSON...
// JSON格式 => camera_id: xxx...
GM_Error CRemoteSession::doPHPGetCourseRecord(string & inData)
{
	// 从传递过来的数据中解析出JSON内容，有效性前面已经判断了...
	Cmd_Header * lpCmdHeader = (Cmd_Header*)inData.c_str();
	LPCTSTR lpDataPtr = inData.c_str() + sizeof(Cmd_Header);
	int nDataSize = lpCmdHeader->m_pkg_len;
	// 解析JSON对象...
	Json::Reader reader;
	Json::Value  value, root;
	string strUTF8Data, strJson;
	GM_Error theErr = GM_Err_Json;
	strUTF8Data.assign(lpDataPtr, nDataSize);
	// 解析转换后的JSON数据包 => PHP编码后的数据，转换无效，仍然是UTF8格式 => 这里是数字列表，无需转化...
	if( !reader.parse(strUTF8Data, value) ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// 判断获取数据的有效性...
	if( !value.isMember("camera_id")  ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// 获取通道编号...
	int nDBCameraID = 0;
	Json::Value & theDBCameraID = value["camera_id"];
	if( theDBCameraID.isString() ) {
		nDBCameraID = atoi(theDBCameraID.asString().c_str());
	} else if( theDBCameraID.isInt() ) {
		nDBCameraID = theDBCameraID.asInt();
	}
	// 判断获取数据的有效性...
	if( nDBCameraID <= 0  ) {
		MsgLogGM(theErr);
		return GM_NoErr;
	}
	// 开始查找对应的摄像头本地编号...
	int nLocalID = -1;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetDBCameraID(nDBCameraID, nLocalID);
	if(  nLocalID <= 0  )
		return GM_NoErr;
	ASSERT( nLocalID > 0 );
	// 根据本地编号获取摄像头对象...
	CCamera * lpCamera = m_lpHaoYiView->FindCameraByID(nLocalID);
	if( lpCamera == NULL )
		return GM_NoErr;
	ASSERT( lpCamera != NULL );
	// 得到该摄像头下正在录像的记录编号...
	root["course_id"] = lpCamera->GetRecCourseID();
	// 将 JSON 数据转换成字符串...
	strJson = root.toStyledString();
	// 组合反馈命令头信息 => 直接从转发数据中拷贝一些过来...
	Cmd_Header theHeader = {0};
	UInt32	nSendSize = 0;
	theHeader.m_pkg_len = strJson.size();
	theHeader.m_type = kClientGather;
	theHeader.m_cmd  = lpCmdHeader->m_cmd;
	theHeader.m_sock = lpCmdHeader->m_sock;
	// 追加命令包头和命令数据包内容...
	memcpy(m_lpSendBuf, &theHeader, sizeof(theHeader));
	nSendSize += sizeof(theHeader);
	memcpy(m_lpSendBuf+nSendSize, strJson.c_str(), strJson.size());
	nSendSize += strJson.size();
	// 调用统一的发送接口...
	return this->SendData(m_lpSendBuf, nSendSize);
}
//
// 链接成功之后，发送登录命令... 
GM_Error CRemoteSession::SendLoginCmd()
{
	ASSERT( m_lpHaoYiView != NULL );
	// 组合Login命令需要的JSON数据包 => 用采集端的MAC地址作为唯一标识...
	string strJson;	Json::Value root;
	root["mac_addr"] = m_lpHaoYiView->m_strMacAddr.GetString();
	root["ip_addr"] = m_lpHaoYiView->m_strIPAddr.GetString();
	strJson = root.toStyledString(); ASSERT( strJson.size() > 0 );
	// 组合命令包头 => 数据长度 | 终端类型 | 命令编号 | 采集端MAC地址
	UInt32	nSendSize = 0;
	Cmd_Header theHeader = {0};
	theHeader.m_pkg_len = strJson.size();
	theHeader.m_type = kClientGather;
	theHeader.m_cmd  = kCmd_Gather_Login;
	// 追加命令包头和命令数据包内容...
	memcpy(m_lpSendBuf, &theHeader, sizeof(theHeader));
	nSendSize += sizeof(theHeader);
	memcpy(m_lpSendBuf+nSendSize, strJson.c_str(), strJson.size());
	nSendSize += strJson.size();
	// 调用统一的发送接口...
	return this->SendData(m_lpSendBuf, nSendSize);
}
//
// 统一的发送接口...
GM_Error CRemoteSession::SendData(LPCTSTR lpDataPtr, int nDataSize)
{
	UInt32	nReturn = 0;
	GM_Error theErr = GM_NoErr;
	theErr = m_TCPSocket.Send(lpDataPtr, nDataSize, &nReturn);
	if( theErr != WSAEWOULDBLOCK )
		return theErr;
	ASSERT( theErr == WSAEWOULDBLOCK );
	// 处理有关 WSAEWOULDBLOCK 的情况...
	int iCount = 0;
	while( iCount <= 4 ) {
		::Sleep(50);
		// 休整50毫秒之后，继续发送...
		theErr = m_TCPSocket.Send(lpDataPtr, nDataSize, &nReturn);
		if( theErr != WSAEWOULDBLOCK )
			return theErr;
		ASSERT( theErr == WSAEWOULDBLOCK );
		++iCount;
	}
	// 连续发送多次，依然WSAEWOULDBLOCK，直接返回错误...
	return GM_NetClose;
}