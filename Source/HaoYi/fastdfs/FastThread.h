
#pragma once

#include "OSMutex.h"
#include "OSThread.h"

class CFastSession;

typedef map<WSAEVENT, CFastSession *>	GM_MapEvent;		// �¼����󼯺�
#define TIMEOUT_STEP_CHECK				10000				// ��ʱ�����ʱ��(����)
#define DEF_WAIT_TIME					500					// Ĭ���¼��̵߳ȴ�ʱ��(����)

class CFastThread : public OSThread
{
public:
	CFastThread(HWND hWndMsg, int nMaxWait = 20);			// Ĭ�ϵ��߳�֧��20������
	virtual ~CFastThread();
public:
	HWND			GetHWndMsg() { return m_hWndMsg; }		// ���ش�����Ϣ���
	int				GetMapSize();							// �õ���ǰ��������
	BOOL			IsOverFlow();							// �Ƿ����
	GM_Error		Initialize();							// ��ʼ��
	GM_Error		UnInitialize();							// ����ʼ��
	GM_Error		AddSession(CFastSession * lpSession);	// ����Ự����
	GM_Error		DelSession(CFastSession * lpSession);	// ɾ���Ự����
private:
	virtual	void	Entry();								// �¼��̺߳���
	WSAEVENT		WaitEventSession();						// �ȴ�һ���Ự
	GM_Error		ProcSession(CFastSession * lpSession);	// ����һ���Ự
	GM_Error		RemoveSession(CFastSession * lpSession);// �Ӽ������Ƴ��Ự(��ɾ��)

	//GM_Error		ProTimeOut();
private:
	int				m_nMaxWait;								// �ܹ�֧�ֵ����������
	OSMutex			m_Mutex;								// �̻߳������
	GM_MapEvent		m_MapEvent;								// �¼��Ự����
	HANDLE			m_hEmptyEvent;							// �߳��ֿ�ʱ���¼�����
	DWORD			m_dwCheckTime;							// ��ʱ���ʱ��
	HWND			m_hWndMsg;								// ��Ϣ֪ͨ���ھ��
};
