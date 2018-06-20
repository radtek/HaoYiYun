
#pragma once

#include "OSMutex.h"
#include "OSThread.h"
#include "circlebuf.h"
#include "rtp.h"

class UDPSocket;
class CUDPRecvThread : public OSThread
{
public:
	CUDPRecvThread(int nDBRoomID, int nDBCameraID);
	virtual ~CUDPRecvThread();
	virtual void Entry();
public:
	GM_Error		InitThread();
private:
	void			CloseSocket();
	void			doSendCreateCmd();
	void			doSendDeleteCmd();
	void			doSendSupplyCmd();
	void			doSendDetectCmd();
	void			doSendReadyCmd();
	void			doRecvPacket();
	void			doSleepTo();

	void			doProcServerReady(char * lpBuffer, int inRecvLen);
	void			doProcServerHeader(char * lpBuffer, int inRecvLen);
	void			doProcServerReload(char * lpBuffer, int inRecvLen);

	void			doTagDetectProcess(char * lpBuffer, int inRecvLen);
	void			doTagHeaderProcess(char * lpBuffer, int inRecvLen);
	void			doTagAVPackProcess(char * lpBuffer, int inRecvLen);
	void			doEraseLoseSeq(uint32_t inSeqID);
private:
	enum {
		kCmdSendCreate	= 0,				// ��ʼ���� => ��������״̬
		kCmdSendReady	= 1,				// ��ʼ���� => ׼����������״̬
		kCmdRecvAVPak	= 2,				// ��ʼ���� => ����Ƶ���ݰ�����״̬
	} m_nCmdState;							// ����״̬����...

	string			m_strSPS;				// ��Ƶsps
	string			m_strPPS;				// ��Ƶpps

	OSMutex			m_Mutex;				// �������
	UDPSocket	*	m_lpUDPSocket;			// UDP����

	bool			m_bNeedSleep;			// ��Ϣ��־ => ֻҪ�з������հ��Ͳ�����Ϣ...
	int				m_rtt_ms;				// ���������ӳ�ֵ => ����
	int				m_rtt_var_ms;			// ���綶��ʱ��� => ����

	circlebuf		m_circle;				// ���ζ���
	rtp_detect_t	m_rtp_detect;			// RTP̽������ṹ��
	rtp_create_t	m_rtp_create;			// RTP���������ֱ���ṹ��
	rtp_delete_t	m_rtp_delete;			// RTPɾ�������ֱ���ṹ��
	rtp_supply_t	m_rtp_supply;			// RTP��������ṹ��

	rtp_header_t	m_rtp_header;			// RTP����ͷ�ṹ��   => ���� => ����������...
	rtp_ready_t		m_rtp_ready;			// RTP׼�������ṹ�� => ���� => ����������...
	rtp_reload_t	m_rtp_reload;			// RTP�ؽ�����ṹ�� => ���� => ���Է�����...

	int64_t			m_next_create_ns;		// �´η��ʹ�������ʱ��� => ���� => ÿ��100���뷢��һ��...
	int64_t			m_next_detect_ns;		// �´η���̽�����ʱ��� => ���� => ÿ��1�뷢��һ��...
	int64_t			m_next_ready_ns;		// �´η��;�������ʱ��� => ���� => ÿ��100���뷢��һ��...

	GM_MapLose		m_MapLose;				// ��⵽�Ķ������϶���...
};