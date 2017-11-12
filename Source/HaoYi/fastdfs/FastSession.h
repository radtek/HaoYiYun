
#pragma once

#include "OSThread.h"
#include "TCPSocket.h"
#include "transmit.h"
#include "fastdfs.h"

class CHaoYiView;
class CFastThread;
class CFastSession
{
public:
	CFastSession();
	virtual ~CFastSession(void);
public:
	virtual GM_Error ForConnect() = 0;
	virtual GM_Error ForRead() = 0;
public:
	BOOL			IsConnected() { return ((m_eState == kConnectState) ? true : false); }
	GM_Error		InitSession(LPCTSTR lpszAddr, int nPort);
	void			SetEventThread(CFastThread * lpThread);
	void			SetErrorCode(GM_Error inErrCode) { m_nXErrorCode = inErrCode; }
	GM_Error		GetErrorCode() { return m_nXErrorCode; }
	WSAEVENT		GetSocketEvent() { return m_TCPSocket.GetWSAEvent(); }
	SOCKET			GetSocket() { return m_TCPSocket.GetSocket(); }

	GM_Error		ProcessEvent(int eventBits);
	GM_Error		OnConnectEvent();
	GM_Error		OnReadEvent();
	
	BOOL			IsTimeout();
	void			ResetTimeout();					// 重置超时起点
	DWORD			GetFrequeTime();				// 得到当前的时间
protected:
	GM_Error		OpenConnect(const string & strAddr, int nPort);
	GM_Error		OpenConnect(LPCTSTR lpszAddr, int nPort);
	GM_Error		DisConnect();
protected:
	enum {
		kNoneState		= 0,						// 没有状态或处于关闭状态
		kOpenState		= 1,						// Socket刚刚建立，正在连接中
		kConnectState	= 2,						// 连接成功，可以进行交互了
	}m_eState;
protected:
	string				m_strRemoteAddr;			// 远程链接地址(可能是域名或IP)
	string				m_strRemoteIP;				// 远程链接地址(只能是IP地址)
	int					m_iRemotePort;				// 远程链接端口

	CFastThread		*	m_lpEventThread;			// 会话所在线程对象
	GM_Error			m_nXErrorCode;				// 保存错误号码
	TCPSocket			m_TCPSocket;				// Socket对象
	string				m_strRecv;					// 从网络上实际读取的数据
	HWND				m_hWndMsg;					// 消息通知窗口句柄
	OSMutex				m_Mutex;					// 线程互斥对象
	
	ULONG				m_iTimeout;					// 超时实际的时间点
	LARGE_INTEGER		m_llSysFreq;				// 系统时钟频率
};

// 与Tracker服务器交互的会话对象...
class CTrackerSession : public CFastSession
{
public:
	CTrackerSession();
	virtual ~CTrackerSession();
public:
	virtual GM_Error	ForConnect();
	virtual GM_Error	ForRead();
public:
	StorageServer   &   GetStorageServer() { return m_NewStorage; }
private:
	GM_Error			SendCmd(char inCmd);		// 发送命令...
private:
	TrackerHeader		m_TrackerCmd;				// Tracker-Header-Cmd...
	StorageServer		m_NewStorage;				// 当前有效的存储服务器...
	FDFSGroupStat	*	m_lpGroupStat;				// group列表头指针...
	int					m_nGroupCount;				// group数量...
};

// 与Storage服务器交互的会话对象...
class CStorageSession : public CFastSession, public OSThread
{
public:
	CStorageSession(StorageServer * lpStorage);
	virtual ~CStorageSession();
public:
	DWORD		GetCurSendKbps() { return m_dwSendKbps; }
	LPCTSTR  	GetCurSendFile() { return m_strCurFile; }
	void		doCurlPost(char * pData, size_t nSize);
public:
	virtual GM_Error ForConnect();
	virtual GM_Error ForRead();
	virtual	void	 Entry();
private:
	enum {
		kPackSize = 128 * 1024,			// 数据包大小 => 越大，发送码流越高(每秒发送64次) => 8KB(4Mbps)|64KB(32Mbps)|128KB(64Mbps)
	};
private:
	void		CloseUpFile();			// 关闭当前正在上传的文件...
	FILE	*	FindOneFile();			// 查找一个有效文件句柄...
	int			SendOnePacket();		// 发送一个有效数据包...
	GM_Error	SendCmdHeader(ULONGLONG llSize, LPCTSTR lpszExt);
	BOOL		doWebSaveFDFS(CString & strPathFile, CString & strFDFS, ULONGLONG llFileSize);
private:
	StorageServer	m_NewStorage;		// 当前有效的存储服务器...
	string			m_strCurData;		// 当前正在发送的数据包内容...
	CString			m_strCurFile;		// 当前正在发送的文件全路径...
	FILE		*	m_lpCurFile;		// 当前正在发送的文件句柄...
	ULONGLONG		m_llFileSize;		// 当前正在发送的文件大小...
	ULONGLONG		m_llSendSize;		// 当前已经发送文件的长度...
	DWORD			m_dwSendKbps;		// 发送码流
	DWORD			m_dwCurSendByte;	// 当前已发送字节数
	char		*	m_lpPackBuffer;		// 数据包缓存指针
};

// 与命令中转服务器交互的会话对象...
class CRemoteSession : public CFastSession
{
public:
	CRemoteSession(CHaoYiView * lpHaoYiView);
	virtual ~CRemoteSession();
public:
	virtual GM_Error ForConnect();
	virtual GM_Error ForRead();
private:
	GM_Error	doCmdLiveVary(LPCTSTR lpData, int nSize);
	GM_Error    doCmdPlayLogin(LPCTSTR lpData, int nSize);
	GM_Error    doPHPSetGatherSys(LPCTSTR lpData, int nSize);
	GM_Error	doPHPSetCourseOpt(LPCTSTR lpData, int nSize);
	GM_Error	doPHPSetCameraAdd(LPCTSTR lpData, int nSize);
	GM_Error	doPHPSetCameraMod(LPCTSTR lpData, int nSize);
	GM_Error	doPHPSetCameraDel(LPCTSTR lpData, int nSize);
	GM_Error	doPHPCameraOperate(LPCTSTR lpData, int nSize, BOOL bIsStart);
	GM_Error	doPHPGetCourseRecord(string & inData);
	GM_Error	SendData(LPCTSTR lpDataPtr, int nDataSize);
	GM_Error	SendLoginCmd();
private:
	enum {
		kSendBufSize = 2048,				// 发送数据包缓存...
	};
private:
	char		*	m_lpSendBuf;			// 发送缓存...
	CHaoYiView	*	m_lpHaoYiView;			// 视图指针...
};