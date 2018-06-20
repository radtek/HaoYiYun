
#pragma once

#include "OSMutex.h"
#include "OSThread.h"
#include "circlebuf.h"
#include "rtp.h"

class UDPSocket;
class CUDPSendThread : public OSThread
{
public:
	CUDPSendThread(int nDBRoomID, int nDBCameraID);
	virtual ~CUDPSendThread();
	virtual void Entry();
public:
	BOOL			InitVideo(string & inSPS, string & inPPS, int nWidth, int nHeight, int nFPS);
	BOOL			InitAudio(int nRateIndex, int nChannelNum);
	void			PushFrame(FMS_FRAME & inFrame);
private:
	GM_Error		InitThread();
	void			CloseSocket();
	void			doSendCreateCmd();
	void			doSendHeaderCmd();
	void			doSendDeleteCmd();
	void			doSendDetectCmd();
	void			doSendLosePacket();
	void			doSendPacket();
	void			doRecvPacket();
	void			doSleepTo();

	void			doProcServerCreate(char * lpBuffer, int inRecvLen);
	void			doProcServerHeader(char * lpBuffer, int inRecvLen);
	void			doProcServerReady(char * lpBuffer, int inRecvLen);
	void			doProcServerReload(char * lpBuffer, int inRecvLen);

	void			doTagDetectProcess(char * lpBuffer, int inRecvLen);
	void			doTagSupplyProcess(char * lpBuffer, int inRecvLen);
private:
	enum {
		kCmdSendCreate	= 0,				// ��ʼ���� => ��������״̬
		kCmdSendHeader	= 1,				// ��ʼ���� => ����ͷ����״̬
		kCmdWaitReady	= 2,				// �ȴ����� => ���Թۿ��˵�׼����������״̬
		kCmdSendAVPack	= 3,				// ��ʼ���� => ����Ƶ���ݰ�״̬
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
	rtp_header_t	m_rtp_header;			// RTP����ͷ�ṹ��

	rtp_ready_t		m_rtp_ready;			// RTP׼�������ṹ�� => ���� => ����ۿ�����Ϣ
	rtp_reload_t	m_rtp_reload;			// RTP�ؽ�����ṹ�� => ���� => ���Է�����

	int64_t			m_next_create_ns;		// �´η��ʹ�������ʱ��� => ���� => ÿ��100���뷢��һ��...
	int64_t			m_next_header_ns;		// �´η�������ͷ����ʱ��� => ���� => ÿ��100���뷢��һ��...
	int64_t			m_next_detect_ns;		// �´η���̽�����ʱ��� => ���� => ÿ��1�뷢��һ��...

	uint32_t		m_nCurPackSeq;			// RTP��ǰ������к�
	uint32_t		m_nCurSendSeq;			// RTP��ǰ�������к�

	GM_MapLose		m_MapLose;				// ��⵽�Ķ������϶���...
};