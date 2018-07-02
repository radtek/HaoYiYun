
#pragma once

#include "OSMutex.h"
#include "OSThread.h"
#include "circlebuf.h"
#include "rtp.h"

class CPlaySDL;
class UDPSocket;
class CPushThread;
class CUDPRecvThread : public OSThread
{
public:
	CUDPRecvThread(CPushThread * lpPushThread, int nDBRoomID, int nDBCameraID);
	virtual ~CUDPRecvThread();
	virtual void Entry();
public:
	GM_Error		InitThread();
private:
	void			ClosePlayer();
	void			CloseSocket();
	void			doSendCreateCmd();
	void			doSendDeleteCmd();
	void			doSendSupplyCmd(bool bIsAudio);
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

	void			doFillLosePack(uint8_t inPType, uint32_t nStartLoseID, uint32_t nEndLoseID);
	void			doEraseLoseSeq(uint8_t inPType, uint32_t inSeqID);
	void			doParseFrame(bool bIsAudio);

	uint32_t		doCalcMaxConSeq(bool bIsAudio);
private:
	enum {
		kCmdSendCreate	= 0,				// ��ʼ���� => ��������״̬
		kCmdSendReady	= 1,				// ��ʼ���� => ׼����������״̬ => �Ѿ���ʼ��������Ƶ������
		kCmdConnetOK	= 2,				// ���뽻����ϣ�ֻ����Ϊһ����ֹ��������׼����������ı�־
	} m_nCmdState;							// ����״̬����...

	string			m_strSPS;				// ��Ƶsps
	string			m_strPPS;				// ��Ƶpps

	OSMutex			m_Mutex;				// �������
	UDPSocket	*	m_lpUDPSocket;			// UDP����
  
	uint16_t		m_HostServerPort;		// �������˿� => host
	uint32_t	    m_HostServerAddr;		// ��������ַ => host

	bool			m_bNeedSleep;			// ��Ϣ��־ => ֻҪ�з������հ��Ͳ�����Ϣ...
	int				m_dt_to_dir;			// ����·�߷��� => TO_SERVER | TO_P2P
	int				m_p2p_rtt_ms;			// P2P    => ���������ӳ�ֵ => ����
	int				m_p2p_rtt_var_ms;		// P2P    => ���綶��ʱ��� => ����
	int				m_p2p_cache_time_ms;	// P2P    => ��������ʱ��   => ���� => ���ǲ�����ʱʱ��
	int				m_server_rtt_ms;		// Server => ���������ӳ�ֵ => ����
	int				m_server_rtt_var_ms;	// Server => ���綶��ʱ��� => ����
	int				m_server_cache_time_ms;	// Server => ��������ʱ��   => ���� => ���ǲ�����ʱʱ��
	int				m_nMaxResendCount;		// ��ǰ��������ط�����

	circlebuf		m_audio_circle;			// ��Ƶ���ζ���
	circlebuf		m_video_circle;			// ��Ƶ���ζ���

	rtp_detect_t	m_rtp_detect;			// RTP̽������ṹ��
	rtp_create_t	m_rtp_create;			// RTP���������ֱ���ṹ��
	rtp_delete_t	m_rtp_delete;			// RTPɾ�������ֱ���ṹ��
	rtp_supply_t	m_rtp_supply;			// RTP��������ṹ��

	rtp_header_t	m_rtp_header;			// RTP����ͷ�ṹ��   => ���� => ����������...
	rtp_ready_t		m_rtp_ready;			// RTP׼�������ṹ�� => ���� => ����������...
	rtp_reload_t	m_rtp_reload;			// RTP�ؽ�����ṹ�� => ���� => ���Է�����...

	int64_t			m_sys_zero_ns;			// ϵͳ��ʱ��� => ��һ�����ݰ������ϵͳʱ�̵� => ����...
	int64_t			m_next_create_ns;		// �´η��ʹ�������ʱ��� => ���� => ÿ��100���뷢��һ��...
	int64_t			m_next_detect_ns;		// �´η���̽�����ʱ��� => ���� => ÿ��1�뷢��һ��...
	int64_t			m_next_ready_ns;		// �´η��;�������ʱ��� => ���� => ÿ��100���뷢��һ��...

	uint32_t		m_nAudioMaxPlaySeq;		// ��ƵRTP��ǰ��󲥷����к� => ���������Ч���к�...
	uint32_t		m_nVideoMaxPlaySeq;		// ��ƵRTP��ǰ��󲥷����к� => ���������Ч���к�...

	GM_MapLose		m_AudioMapLose;			// ��Ƶ��⵽�Ķ������϶���...
	GM_MapLose		m_VideoMapLose;			// ��Ƶ��⵽�Ķ������϶���...
	
	CPlaySDL    *  m_lpPlaySDL;             // SDL���Ź�����...
	CPushThread *  m_lpPushThread;          // �ϴ�����������...
};