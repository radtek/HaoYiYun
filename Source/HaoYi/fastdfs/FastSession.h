
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
	void			ResetTimeout();					// ���ó�ʱ���
	DWORD			GetFrequeTime();				// �õ���ǰ��ʱ��
protected:
	GM_Error		OpenConnect(const string & strAddr, int nPort);
	GM_Error		OpenConnect(LPCTSTR lpszAddr, int nPort);
	GM_Error		DisConnect();
protected:
	enum {
		kNoneState		= 0,						// û��״̬���ڹر�״̬
		kOpenState		= 1,						// Socket�ոս���������������
		kConnectState	= 2,						// ���ӳɹ������Խ��н�����
	}m_eState;
protected:
	string				m_strRemoteAddr;			// Զ�����ӵ�ַ(������������IP)
	string				m_strRemoteIP;				// Զ�����ӵ�ַ(ֻ����IP��ַ)
	int					m_iRemotePort;				// Զ�����Ӷ˿�

	CFastThread		*	m_lpEventThread;			// �Ự�����̶߳���
	GM_Error			m_nXErrorCode;				// ����������
	TCPSocket			m_TCPSocket;				// Socket����
	string				m_strRecv;					// ��������ʵ�ʶ�ȡ������
	HWND				m_hWndMsg;					// ��Ϣ֪ͨ���ھ��
	OSMutex				m_Mutex;					// �̻߳������
	
	ULONG				m_iTimeout;					// ��ʱʵ�ʵ�ʱ���
	LARGE_INTEGER		m_llSysFreq;				// ϵͳʱ��Ƶ��
};

// ��Tracker�����������ĻỰ����...
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
	GM_Error			SendCmd(char inCmd);		// ��������...
private:
	TrackerHeader		m_TrackerCmd;				// Tracker-Header-Cmd...
	StorageServer		m_NewStorage;				// ��ǰ��Ч�Ĵ洢������...
	FDFSGroupStat	*	m_lpGroupStat;				// group�б�ͷָ��...
	int					m_nGroupCount;				// group����...
};

// ��Storage�����������ĻỰ����...
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
		kPackSize = 128 * 1024,			// ���ݰ���С => Խ�󣬷�������Խ��(ÿ�뷢��64��) => 8KB(4Mbps)|64KB(32Mbps)|128KB(64Mbps)
	};
private:
	void		CloseUpFile();			// �رյ�ǰ�����ϴ����ļ�...
	FILE	*	FindOneFile();			// ����һ����Ч�ļ����...
	int			SendOnePacket();		// ����һ����Ч���ݰ�...
	GM_Error	SendCmdHeader(ULONGLONG llSize, LPCTSTR lpszExt);
	BOOL		doWebSaveFDFS(CString & strPathFile, CString & strFDFS, ULONGLONG llFileSize);
private:
	StorageServer	m_NewStorage;		// ��ǰ��Ч�Ĵ洢������...
	string			m_strCurData;		// ��ǰ���ڷ��͵����ݰ�����...
	CString			m_strCurFile;		// ��ǰ���ڷ��͵��ļ�ȫ·��...
	FILE		*	m_lpCurFile;		// ��ǰ���ڷ��͵��ļ����...
	ULONGLONG		m_llFileSize;		// ��ǰ���ڷ��͵��ļ���С...
	ULONGLONG		m_llSendSize;		// ��ǰ�Ѿ������ļ��ĳ���...
	DWORD			m_dwSendKbps;		// ��������
	DWORD			m_dwCurSendByte;	// ��ǰ�ѷ����ֽ���
	char		*	m_lpPackBuffer;		// ���ݰ�����ָ��
};

// ��������ת�����������ĻỰ����...
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
		kSendBufSize = 2048,				// �������ݰ�����...
	};
private:
	char		*	m_lpSendBuf;			// ���ͻ���...
	CHaoYiView	*	m_lpHaoYiView;			// ��ͼָ��...
};