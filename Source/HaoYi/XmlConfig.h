
#pragma once

#include "tinyxml.h"
#include "OSMutex.h"

typedef map<int, GM_MapData>	GM_MapNodeCamera;	// int => 是指数据库DBCameraID
typedef map<int, GM_MapCourse>  GM_MapNodeCourse;	// int => 是指数据库DBCameraID

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
	string & GetAddress() { return m_strAddress; }

	string & GetSavePath() { return m_strSavePath; }
	string & GetWebAddr() { return m_strWebAddr; }
	int		 GetWebPort() { return m_nWebPort; }
	BOOL	 IsWebHttps() { return ((strnicmp("https://", m_strWebAddr.c_str(), strlen("https://")) == 0) ? true : false); }

	/////////////////////////////////////////////////////////////
	// 这几个地址和端口是动态获取的，不会存入xml当中...
	/////////////////////////////////////////////////////////////
	string & GetAuthExpired() { return m_strAuthExpired; }
	string & GetMainName() { return m_strMainName; }
	int		 GetMainKbps() { return m_nMainKbps; }
	int      GetSubKbps()  { return m_nSubKbps; }
	int		 GetSliceVal() { return m_nSliceVal; }
	int		 GetInterVal() { return m_nInterVal; }
	BOOL	 GetAutoLinkDVR() { return m_bAutoLinkDVR; }
	BOOL	 GetAutoLinkFDFS() { return m_bAutoLinkFDFS; }
	int		 GetMaxCamera() { return m_nMaxCamera; }

	string & GetWebTag()  { return m_strWebTag; }
	string & GetWebName() { return m_strWebName; }
	int		 GetWebType() { return m_nWebType; }
	string & GetRemoteAddr() { return m_strRemoteAddr; }
	int		 GetRemotePort() { return m_nRemotePort; }
	string & GetTrackerAddr() { return m_strTrackerAddr; }
	int		 GetTrackerPort() { return m_nTrackerPort; }
	int      GetDBGatherID() { return m_nDBGatherID; }
	int      GetDBHaoYiGatherID() { return m_nDBHaoYiGatherID; }
	void	 SetRemoteAddr(const string & strAddr) { m_strRemoteAddr = strAddr; }
	void     SetRemotePort(int nPort) { m_nRemotePort = nPort; }
	void	 SetTrackerAddr(const string & strAddr) { m_strTrackerAddr = strAddr; }
	void     SetTrackerPort(int nPort) { m_nTrackerPort = nPort; }
	void	 SetWebType(int nWebType) { m_nWebType = nWebType; }
	void	 SetWebName(const string & strWebName) { m_strWebName = strWebName; }
	void	 SetWebTag(const string & strWebTag) { m_strWebTag = strWebTag; }
	void	 SetDBGatherID(int nDBGatherID) { m_nDBGatherID = nDBGatherID; }
	void     SetDBHaoYiGatherID(int nDBHaoYiID) { m_nDBHaoYiGatherID = nDBHaoYiID; }

	void	 SetAuthExpired(const string & strExpired) { m_strAuthExpired = strExpired; }
	void	 SetMainName(const string & strName) { m_strMainName = strName; }
	void	 SetMainKbps(int nMainKbps) { m_nMainKbps = nMainKbps; } 
	void     SetSubKbps(int nSubKbps) { m_nSubKbps = nSubKbps; }
	void	 SetSliceVal(int nSliceVal) { m_nSliceVal = nSliceVal; }
	void	 SetInterVal(int nInterVal) { m_nInterVal = nInterVal; }
	void	 SetAutoLinkDVR(BOOL bAutoLinkDVR) { m_bAutoLinkDVR = bAutoLinkDVR; }
	void	 SetAutoLinkFDFS(BOOL bAutoLinkFDFS) { m_bAutoLinkFDFS = bAutoLinkFDFS; }
	void	 SetMaxCamera(int nMaxCamera) { m_nMaxCamera = nMaxCamera; }

	void	 SetWebAddr(const string & strAddr) { m_strWebAddr = strAddr; }
	void     SetWebPort(int nPort) { m_nWebPort = nPort; }
	void	 SetSavePath(const string & strPath) { m_strSavePath = strPath; }

	void	 SetCamera(int nDBCameraID, GM_MapData & inMapData) { m_MapNodeCamera[nDBCameraID] = inMapData; }
	void	 GetCamera(int nDBCameraID, GM_MapData & outMapData) { outMapData = m_MapNodeCamera[nDBCameraID]; }
	GM_MapNodeCamera & GetNodeCamera() { return m_MapNodeCamera; }

	void	 SetCourse(int nDBCameraID, GM_MapCourse & inMapCourse);
	void	 GetCourse(int nDBCameraID, GM_MapCourse & outMapCourse);
	GM_MapNodeCourse & GetNodeCourse();

	CString  GetDBCameraTitle(int nDBCameraID);

	BOOL	StreamSnapJpeg(const CString & inSrcMP4File, const CString & inDestJpgName, int nRecSec);
private:
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
	string				m_strAddress;					// 地址

	string				m_strMPlayer;					// 截图工具的全路径...
	string				m_strXMLFile;					// XML配置文件
	string				m_strMainName;					// 主窗口标题名称...
	string				m_strSavePath;					// 录像或截图存放路径...
	int					m_nMainKbps;					// 主码流大小...
	int					m_nSubKbps;						// 子码流大小...
	int					m_nSliceVal;					// 录像切片时间(1~30分钟)
	int					m_nInterVal;					// 切片交错关键帧(1~3个)
	BOOL				m_bAutoLinkDVR;					// 自动重连DVR摄像头...
	BOOL				m_bAutoLinkFDFS;				// 自动重连FDFS服务器...
	string				m_strWebAddr;					// Web的IP地址...
	int					m_nWebPort;						// Web的端口地址...
	
	string				m_strWebTag;					// 注册时获取的网站标志
	string				m_strWebName;					// 注册时获取的网站名称
	int					m_nWebType;						// 注册时获取的网站类型
	string				m_strRemoteAddr;				// 远程中转服务器的IP地址...
	int					m_nRemotePort;					// 远程中转服务器的端口地址...
	string				m_strTrackerAddr;				// FDFS-Tracker的IP地址...
	int					m_nTrackerPort;					// FDFS-Tracker的端口地址...
	int                 m_nDBGatherID;					// 数据库中采集端编号...
	int                 m_nDBHaoYiGatherID;				// 在中心服务器上的采集端编号...
	string				m_strAuthExpired;				// 中心服务器反馈的授权过期时间...

	int					m_nMaxCamera;					// 能够支持的最大摄像头数（默认为16个）
	GM_MapNodeCamera	m_MapNodeCamera;				// 监控通道配置信息(数据库CameraID）
	GM_MapNodeCourse	m_MapNodeCourse;				// 监控通道课表记录(数据库CourseID)
	OSMutex				m_MutexCourse;					// 专门用于课程表的互斥对象

	friend class CHaoYiView;
};
