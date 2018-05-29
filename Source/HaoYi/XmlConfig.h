
#pragma once

#include "tinyxml.h"
#include "OSMutex.h"

typedef map<string, int>		GM_MapServer;		// addr => port
typedef map<int, GM_MapData>	GM_MapNodeCamera;	// int  => 是指数据库DBCameraID
typedef map<int, GM_MapCourse>  GM_MapNodeCourse;	// int  => 是指数据库DBCameraID

struct AVCodecContext;
struct AVFrame;
class CXmlConfig
{
public:
	CXmlConfig(void);
private:
	~CXmlConfig(void);
public:
	static CXmlConfig & GMInstance();
	
	BOOL	 GMSaveConfig();
	BOOL	 GMLoadConfig();

	void	doDelDVR(int nDBCameraID);

	string & GetCopyRight() { return m_strCopyRight; }
	string & GetVersion() { return m_strVersion; }
	string & GetPhone() { return m_strPhone; }
	string & GetWebSite() { return m_strWebSite; }
//	string & GetAddress() { return m_strAddress; }

	string & GetSavePath() { return m_strSavePath; }
	string & GetWebAddr() { return m_strWebAddr; }
	int		 GetWebPort() { return m_nWebPort; }
	BOOL	 IsWebHttps() { return ((strnicmp("https://", m_strWebAddr.c_str(), strlen("https://")) == 0) ? true : false); }

	/////////////////////////////////////////////////////////////
	// 这几个地址和端口是动态获取的，不会存入xml当中...
	/////////////////////////////////////////////////////////////
	bool     GetAuthLicense() { return m_bAuthLicense; }
	int      GetAuthDays() { return m_nAuthDays; }
	string & GetAuthExpired() { return m_strAuthExpired; }
	string & GetMainName() { return m_strMainName; }
	int		 GetMainKbps() { return m_nMainKbps; }
	int      GetSubKbps()  { return m_nSubKbps; }
	int		 GetSliceVal() { return m_nSliceVal; }
	int		 GetInterVal() { return m_nInterVal; }
	int		 GetSnapVal()  { return m_nSnapVal; }
	BOOL	 GetAutoLinkDVR() { return m_bAutoLinkDVR; }
	BOOL	 GetAutoLinkFDFS() { return m_bAutoLinkFDFS; }
	BOOL	 GetAutoDetectIPC() { return m_bAutoDetectIPC; }
	int		 GetMaxCamera() { return m_nMaxCamera; }
	int		 GetPerPageSize() { return m_nPageSize; }

	string & GetWebVer()  { return m_strWebVer; }
	string & GetWebTag()  { return m_strWebTag; }
	string & GetWebName() { return m_strWebName; }
	int		 GetWebType() { return m_nWebType; }
	string & GetRemoteAddr() { return m_strRemoteAddr; }
	int		 GetRemotePort() { return m_nRemotePort; }
	string & GetTrackerAddr() { return m_strTrackerAddr; }
	int		 GetTrackerPort() { return m_nTrackerPort; }
	int      GetDBGatherID() { return m_nDBGatherID; }
	int      GetDBHaoYiUserID() { return m_nDBHaoYiUserID; }
	int      GetDBHaoYiNodeID() { return m_nDBHaoYiNodeID; }
	int      GetDBHaoYiGatherID() { return m_nDBHaoYiGatherID; }
	void	 SetRemoteAddr(const string & strAddr) { m_strRemoteAddr = strAddr; }
	void     SetRemotePort(int nPort) { m_nRemotePort = nPort; }
	void	 SetTrackerAddr(const string & strAddr) { m_strTrackerAddr = strAddr; }
	void     SetTrackerPort(int nPort) { m_nTrackerPort = nPort; }
	void	 SetWebType(int nWebType) { m_nWebType = nWebType; }
	void	 SetWebName(const string & strWebName) { m_strWebName = strWebName; }
	void	 SetWebTag(const string & strWebTag) { m_strWebTag = strWebTag; }
	void	 SetWebVer(const string & strWebVer) { m_strWebVer = strWebVer; }
	void	 SetDBGatherID(int nDBGatherID) { m_nDBGatherID = nDBGatherID; }
	void     SetDBHaoYiUserID(int nDBUserID) { m_nDBHaoYiUserID = nDBUserID; }
	void	 SetDBHaoYiNodeID(int nDBNodeID) { m_nDBHaoYiNodeID = nDBNodeID; }
	void     SetDBHaoYiGatherID(int nDBGatherID) { m_nDBHaoYiGatherID = nDBGatherID; }

	void	 SetAuthDays(const int nAuthDays) { m_nAuthDays = nAuthDays; }
	void	 SetAuthExpired(const string & strExpired) { m_strAuthExpired = strExpired; }
	void	 SetAuthLicense(bool bLicense) { m_bAuthLicense = bLicense; }
	void	 SetMainName(const string & strName) { m_strMainName = strName; }
	void	 SetMainKbps(int nMainKbps) { m_nMainKbps = nMainKbps; } 
	void     SetSubKbps(int nSubKbps) { m_nSubKbps = nSubKbps; }
	void	 SetSliceVal(int nSliceVal) { m_nSliceVal = nSliceVal; }
	void	 SetInterVal(int nInterVal) { m_nInterVal = nInterVal; }
	void	 SetSnapVal(int nSnapVal)   { m_nSnapVal = nSnapVal; }
	void	 SetAutoLinkDVR(BOOL bAutoLinkDVR) { m_bAutoLinkDVR = bAutoLinkDVR; }
	void	 SetAutoLinkFDFS(BOOL bAutoLinkFDFS) { m_bAutoLinkFDFS = bAutoLinkFDFS; }
	void	 SetAutoDetectIPC(BOOL bAutoDetectIPC) { m_bAutoDetectIPC = bAutoDetectIPC; }
	void	 SetMaxCamera(int nMaxCamera) { m_nMaxCamera = nMaxCamera; }
	void	 SetPerPageSize(int nPageSize) { m_nPageSize = nPageSize; }

	void	 SetSavePath(const string & strPath) { m_strSavePath = strPath; }
	void	 SetFocusServer(const string & inWebAddr, int inWebPort);

	void	 SetCamera(int nDBCameraID, GM_MapData & inMapData) { m_MapNodeCamera[nDBCameraID] = inMapData; }
	void	 GetCamera(int nDBCameraID, GM_MapData & outMapData) { outMapData = m_MapNodeCamera[nDBCameraID]; }
	GM_MapNodeCamera & GetNodeCamera() { return m_MapNodeCamera; }

	GM_MapRoom & GetMapLiveRoom() { return m_MapLiveRoom; }
	int		GetBeginRoomID() { return m_nBeginRoomID; }
	int		GetCurSelRoomID() { return m_nCurSelRoomID; }
	void	SetMapLiveRoom(GM_MapRoom & inMapRoom) { m_MapLiveRoom = inMapRoom; }
	void	SetBeginRoomID(int nBeginRoomID) { m_nBeginRoomID = nBeginRoomID; }
	void	SetCurSelRoomID(int nCurSelRoomID) { m_nCurSelRoomID = nCurSelRoomID; }

	GM_MapServer & GetServerList() { return m_MapServer; }

	void	 SetCourse(int nDBCameraID, GM_MapCourse & inMapCourse);
	void	 GetCourse(int nDBCameraID, GM_MapCourse & outMapCourse);
	GM_MapNodeCourse & GetNodeCourse();

	CString  GetDBCameraTitle(int nDBCameraID);

	BOOL	FFmpegSnapJpeg(const string & inSrcFrame, const CString & inDestJpgName);
	BOOL	StreamSnapJpeg(const CString & inSrcMP4File, const CString & inDestJpgName, int nRecSec);
private:
	BOOL	FFmpegSaveJpeg(AVCodecContext * pOrigCodecCtx, AVFrame * pOrigFrame, LPCTSTR lpszJpgName);
	BOOL	GenerateJpegFromMediaFile(const CString & inMP4File, int nPosSec, int nFrames);
	void	SendMPlayerCmd(const CString &cmdLine);
	void    ReadHolePipe(HANDLE hStdOut, string & strPipe);
private:
	TiXmlElement		BuildXmlElem(const string & strNode, int inData);
	TiXmlElement		BuildXmlElem(const string & strNode, const string & strData);
private:
	string				m_strCopyRight;					// 版权
	string				m_strVersion;					// 版本
	string				m_strPhone;						// 电话
	string				m_strWebSite;					// 网站
//	string				m_strAddress;					// 地址

	string				m_strMPlayer;					// 截图工具的全路径...
	string				m_strXMLFile;					// XML配置文件
	string				m_strMainName;					// 主窗口标题名称...
	string				m_strSavePath;					// 录像或截图存放路径...
	int					m_nMainKbps;					// 主码流大小...
	int					m_nSubKbps;						// 子码流大小...
	int					m_nSliceVal;					// 录像切片时间(0~30分钟)
	int					m_nInterVal;					// 切片交错关键帧(0~3个)
	int				    m_nSnapVal;						// 通道截图间隔(1~10分钟)
	BOOL				m_bAutoLinkDVR;					// 自动重连DVR摄像头...
	BOOL				m_bAutoLinkFDFS;				// 自动重连FDFS服务器...
	BOOL				m_bAutoDetectIPC;				// 自动探测IPC标志...
	int					m_nPageSize;					// 每页窗口数目...

	string				m_strWebAddr;					// 焦点网站的IP或域名...
	int					m_nWebPort;						// 焦点网站的端口地址...
	
	string				m_strWebVer;					// 注册是获取的网站版本
	string				m_strWebTag;					// 注册时获取的网站标志
	string				m_strWebName;					// 注册时获取的网站名称
	int					m_nWebType;						// 注册时获取的网站类型
	string				m_strRemoteAddr;				// 远程中转服务器的IP地址...
	int					m_nRemotePort;					// 远程中转服务器的端口地址...
	string				m_strTrackerAddr;				// FDFS-Tracker的IP地址...
	int					m_nTrackerPort;					// FDFS-Tracker的端口地址...
	int                 m_nDBGatherID;					// 数据库中采集端编号...
	int					m_nDBHaoYiUserID;				// 在中心服务器上的绑定用户编号...
	int					m_nDBHaoYiNodeID;				// 在中心服务器上的节点编号...
	int                 m_nDBHaoYiGatherID;				// 在中心服务器上的采集端编号...
	string				m_strAuthExpired;				// 中心服务器反馈的授权过期时间...
	bool				m_bAuthLicense;					// 中心服务器反馈的永久授权标志...
	int					m_nAuthDays;					// 中心服务器反馈的剩余授权天数...

	GM_MapServer		m_MapServer;					// 可连接的服务器列表...
	GM_MapRoom			m_MapLiveRoom;					// 服务器上有效的直播间...
	int					m_nBeginRoomID;					// 直播间开始编号...
	int					m_nCurSelRoomID;				// 当前已选中直播间号...

	int					m_nMaxCamera;					// 能够支持的最大摄像头数（默认为16个）
	GM_MapNodeCamera	m_MapNodeCamera;				// 监控通道配置信息(数据库CameraID）
	GM_MapNodeCourse	m_MapNodeCourse;				// 监控通道课表记录(数据库CourseID)
	OSMutex				m_MutexCourse;					// 专门用于课程表的互斥对象

	friend class CHaoYiView;
};
