
#pragma once

//#define DEBUG_FRAME						// �Ƿ��������֡...
//#define DEBUG_DECODE						// �Ƿ���Խ�����...

#define DEF_RTP_LOG			"rtp.log"		// Ĭ��rtp��־�ļ�
#define DEF_UDP_HOME        "edu.ihaoyi.cn" // Ĭ��UDP��������ַ
#define DEF_UDP_PORT        5252            // Ĭ��UDP�������˿�
#define DEF_MTU_SIZE        800             // Ĭ��MTU��Ƭ��С(�ֽ�)...
#define LOG_MAX_SIZE        2048            // ������־��󳤶�(�ֽ�)...
#define MAX_BUFF_LEN        1024            // ����ĳ���(�ֽ�)...
#define MAX_SLEEP_MS		15				// �����Ϣʱ��(����)
#define RELOAD_TIME_OUT     20				// �ؽ��������(��) => �ȴ���������ʱ��ɾ��֮�󣬲��ܱ��ؽ�...
#define ADTS_HEADER_SIZE	7				// AAC��Ƶ���ݰ�ͷ����(�ֽ�)...
#define DEF_CIRCLE_SIZE		128	* 1024		// Ĭ�ϻ��ζ��г���(�ֽ�)...

//
// ���彻���ն�����...
enum
{
  TM_TAG_STUDENT  = 0x01, // ѧ���˱��...
  TM_TAG_TEACHER  = 0x02, // ��ʦ�˱��...
  TM_TAG_SERVER   = 0x03, // ���������...
};
//
// ���彻���ն����...
enum
{
  ID_TAG_PUSHER  = 0x01,  // ��������� => ������...
  ID_TAG_LOOKER  = 0x02,  // ��������� => �ۿ���...
  ID_TAG_SERVER  = 0x03,  // ���������
};
//
// ����RTP�غ���������...
enum
{
  PT_TAG_DETECT  = 0x01,  // ̽��������...
  PT_TAG_CREATE  = 0x02,  // ����������...
  PT_TAG_DELETE  = 0x03,  // ɾ��������...
  PT_TAG_SUPPLY  = 0x04,  // ����������...
  PT_TAG_HEADER  = 0x05,  // ����Ƶ����ͷ...
  PT_TAG_READY   = 0x06,  // ׼����������...
  PT_TAG_RELOAD  = 0x07,  // �ؽ������� => ר��������������...
  PT_TAG_AUDIO   = 0x08,  // ��Ƶ�� => FLV_TAG_TYPE_AUDIO...
  PT_TAG_VIDEO   = 0x09,  // ��Ƶ�� => FLV_TAG_TYPE_VIDEO...
  PT_TAG_LOSE    = 0x0A,  // �Ѷ�ʧ���ݰ�...
};
//
// ����̽������ṹ�� => PT_TAG_DETECT
typedef struct {
  unsigned char   tm:2;         // terminate type => TM_TAG_STUDENT | TM_TAG_TEACHER
  unsigned char   id:2;         // identify type => ID_TAG_PUSHER | ID_TAG_LOOKER
  unsigned char   pt:4;         // payload type => PT_TAG_DETECT
  unsigned char   noset;        // ���� => �ֽڶ���
  unsigned short  dtNum;        // ̽�����
  unsigned int    tsSrc;        // ̽�ⷢ��ʱ��ʱ��� => ����
  unsigned int    maxConSeq;    // ���ն����յ�����������к� => ���߷��Ͷˣ��������֮ǰ�����ݰ�������ɾ����
}rtp_detect_t;
//
// ���崴������ṹ�� => PT_TAG_CREATE
typedef struct {
  unsigned char   tm:2;         // terminate type => TM_TAG_STUDENT | TM_TAG_TEACHER
  unsigned char   id:2;         // identify type => ID_TAG_PUSHER | ID_TAG_LOOKER
  unsigned char   pt:4;         // payload type => PT_TAG_CREATE
  unsigned char   noset;        // ���� => �ֽڶ���
  unsigned short  liveID;       // ѧ��������ͷ���
  unsigned int    roomID;       // ���ҷ�����
}rtp_create_t;
//
// ����ɾ������ṹ�� => PT_TAG_DELETE
typedef struct {
  unsigned char   tm:2;         // terminate type => TM_TAG_STUDENT | TM_TAG_TEACHER
  unsigned char   id:2;         // identify type => ID_TAG_PUSHER | ID_TAG_LOOKER
  unsigned char   pt:4;         // payload type => PT_TAG_DELETE
  unsigned char   noset;        // ���� => �ֽڶ���
  unsigned short  liveID;       // ѧ��������ͷ���
  unsigned int    roomID;       // ���ҷ�����
}rtp_delete_t;
//
// ���岹������ṹ�� => PT_TAG_SUPPLY
typedef struct {
  unsigned char   tm:2;         // terminate type => TM_TAG_STUDENT | TM_TAG_TEACHER
  unsigned char   id:2;         // identify type => ID_TAG_PUSHER | ID_TAG_LOOKER
  unsigned char   pt:4;         // payload type => PT_TAG_SUPPLY
  unsigned char   noset;        // ���� => �ֽڶ���
  unsigned short  suSize;       // �������� / 4 = ��������
  // unsigned int => �������1
  // unsigned int => �������2
  // unsigned int => �������3
}rtp_supply_t;
//
// ��������Ƶ����ͷ�ṹ�� => PT_TAG_HEADER
typedef struct {
  unsigned char   tm:2;         // terminate type => TM_TAG_STUDENT | TM_TAG_TEACHER
  unsigned char   id:2;         // identify type => ID_TAG_PUSHER | ID_TAG_LOOKER
  unsigned char   pt:4;         // payload type => PT_TAG_HEADER
  unsigned char   hasAudio:4;   // �Ƿ�����Ƶ => 0 or 1
  unsigned char   hasVideo:4;   // �Ƿ�����Ƶ => 0 or 1
  unsigned char   rateIndex:5;  // ��Ƶ�������������
  unsigned char   channelNum:3; // ��Ƶͨ������
  unsigned char   fpsNum;       // ��Ƶfps��С
  unsigned short  picWidth;     // ��Ƶ���
  unsigned short  picHeight;    // ��Ƶ�߶�
  unsigned short  spsSize;      // sps����
  unsigned short  ppsSize;      // pps����
  // .... => sps data           // sps������
  // .... => pps data           // pps������
}rtp_header_t;
//
// ����׼����������ṹ�� => PT_TAG_READY
typedef struct {
  unsigned char   tm:2;         // terminate type => TM_TAG_STUDENT | TM_TAG_TEACHER
  unsigned char   id:2;         // identify type => ID_TAG_PUSHER | ID_TAG_LOOKER
  unsigned char   pt:4;         // payload type => PT_TAG_READY
  unsigned char   noset;        // ���� => �ֽڶ���
  unsigned short  recvPort;     // �����ߴ�͸�˿� => ���� => host
  unsigned int    recvAddr;     // �����ߴ�͸��ַ => ���� => host
}rtp_ready_t;
//
// �����ؽ�����ṹ�� => PT_TAG_RELOAD
typedef struct {
  unsigned char   tm:2;         // terminate type => TM_TAG_SERVER
  unsigned char   id:2;         // identify type => ID_TAG_SERVER
  unsigned char   pt:4;         // payload type => PT_TAG_RELOAD
  unsigned char   noset;        // ���� => �ֽڶ���
  unsigned short  reload_count; // �ؽ����� => �ɽ��ն˴���...
  unsigned int    reload_time;  // �ؽ�ʱ�� => �н��ն˴���...
}rtp_reload_t;
//
// ����RTP���ݰ�ͷ�ṹ�� => PT_TAG_AUDIO | PT_TAG_VIDEO
typedef struct {
  unsigned char   tm:2;         // terminate type => TM_TAG_STUDENT | TM_TAG_TEACHER
  unsigned char   id:2;         // identify type => ID_TAG_PUSHER | ID_TAG_LOOKER
  unsigned char   pt:4;         // payload type => PT_TAG_AUDIO | PT_TAG_VIDEO
  unsigned char   pk:4;         // payload is keyframe => 0 or 1
  unsigned char   pst:2;        // payload start flag => 0 or 1
  unsigned char   ped:2;        // payload end flag => 0 or 1
  unsigned short  psize;        // payload size => ������ͷ��������
  unsigned int    seq;          // rtp���к� => ��1��ʼ
  unsigned int    ts;           // ֡ʱ���  => ����
}rtp_hdr_t;
//
// ���嶪���ṹ��...
typedef struct {
  unsigned int   lose_seq;      // ��⵽�Ķ������к�
  unsigned int   resend_time;   // �ط�ʱ��� => cur_time + rtt_var => ����ʱ�ĵ�ǰʱ�� + ����ʱ�����綶��ʱ���
  unsigned int   resend_count;  // �ط�����ֵ => ��ǰ��ʧ�������ط�����
}rtp_lose_t;

// �����⵽�Ķ������� => ���к� : �����ṹ��...
typedef map<uint32_t, rtp_lose_t>  GM_MapLose;

// ������־�������ͺ� => debug ģʽֻ��ӡ��д��־�ļ�...
bool do_trace(const char * inFile, int inLine, bool bIsDebug, const char *msg, ...);
#define log_trace(msg, ...) do_trace(__FILE__, __LINE__, false, msg, ##__VA_ARGS__)
#define log_debug(msg, ...) do_trace(__FILE__, __LINE__, true, msg, ##__VA_ARGS__)
