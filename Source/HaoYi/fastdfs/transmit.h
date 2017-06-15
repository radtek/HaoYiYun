
#ifndef _TRANSMIT_H_DEF_
#define _TRANSMIT_H_DEF_

#define ERR_OK          0
#define ERR_NO_SOCK     10001
#define ERR_NO_GATHER   10002
#define ERR_SOCK_SEND   10003
#define ERR_NO_JSON     10004
#define ERR_NO_COMMAND  10005
#define ERR_NO_RTMP     10006
#define ERR_NO_MAC_ADDR 10007
#define ERR_PUSH_FAIL	10008

//
// define client type...
enum {
  kClientPHP     = 1,
  kClientGather  = 2,
  kClientLive    = 3,  // RTMPAction
  kClientPlay    = 4,  // RTMPAction
};
//
// define command id...
enum {
  kCmd_Gather_Login				= 1,
  kCmd_PHP_Get_Camera_Status	= 2,
  kCmd_PHP_Set_Camera_Name		= 3,
  kCmd_PHP_Set_Course_Add		= 4,
  kCmd_PHP_Set_Course_Mod		= 5,
  kCmd_PHP_Set_Course_Del		= 6,
  kCmd_PHP_Get_Gather_Status	= 7,
  kCmd_PHP_Get_Course_Record	= 8,
  kCmd_Live_Login				= 9,
  kCmd_Live_Vary				= 10,
  kCmd_Play_Login				= 11,
};
//
// define the command header...
typedef struct {
  int   m_pkg_len;    // body size...
  int   m_type;       // client type...
  int   m_cmd;        // command id...
  int   m_sock;       // php sock in transmit...
} Cmd_Header;

#endif
