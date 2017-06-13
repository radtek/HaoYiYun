
#pragma once

#include "tinyxml.h"
#include "OSMutex.h"

typedef map<int, GM_MapData>	GM_MapNodeCamera;	// int => 是指本地ID
typedef map<int, GM_MapCourse>  GM_MapNodeCourse;	// int => 是指本地ID
typedef map<int, int>			GM_MapDBCamera;		// DBCameraID => LocalID

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

	void	doDelDVR(int nCameraID);

	string & GetCopyRight() { return m_strCopyRight; }
	string & GetVersion() { return m_strVersion; }
	string & GetPhone() { return m_strPhone; }
	string & GetWebSite() { return m_strWebSite; }
	string & GetAddress() { return m_strAddress; }

	string & GetMainName() { return m_strMainName; }
	string & GetSavePath() { return m_strSavePath; }
	int		 GetMainKbps() { return m_nMainKbps; }
	int      GetSubKbps()  { return m_nSubKbps; }
	int		 GetSnapStep() { return m_nSnapStep; }
	int		 GetRecSlice() { return m_nRecSlice; }
	BOOL	 GetAutoLinkDVR() { return m_bAutoLinkDVR; }
	BOOL	 GetAutoLinkFDFS() { return m_bAutoLinkFDFS; }
	int		 GetMaxCamera() { return m_nMaxCamera; }

	string & GetWebAddr() { return m_strWebAddr; }
	int		 GetWebPort() { return m_nWebPort; }

	// 这几个地址和端口是动态获取的，不会存入xml当中...
	string & GetRemoteAddr() { return m_strRemoteAddr; }
	int		 GetRemotePort() { return m_nRemotePort; }
	string & GetTrackerAddr() { return m_strTrackerAddr; }
	int		 GetTrackerPort() { return m_nTrackerPort; }
	void	 SetRemoteAddr(const string & strAddr) { m_strRemoteAddr = strAddr; }
	void     SetRemotePort(int nPort) { m_nRemotePort = nPort; }
	void	 SetTrackerAddr(const string & strAddr) { m_strTrackerAddr = strAddr; }
	void     SetTrackerPort(int nPort) { m_nTrackerPort = nPort; }

	void	 SetMaxCamera(int nMaxCamera) { m_nMaxCamera = nMaxCamera; }
	void	 SetMainName(const string & strName) { m_strMainName = strName; }
	void	 SetAutoLinkDVR(BOOL bAutoLinkDVR) { m_bAutoLinkDVR = bAutoLinkDVR; }
	void	 SetAutoLinkFDFS(BOOL bAutoLinkFDFS) { m_bAutoLinkFDFS = bAutoLinkFDFS; }
	void	 SetWebAddr(const string & strAddr) { m_strWebAddr = strAddr; }
	void     SetWebPort(int nPort) { m_nWebPort = nPort; }
	void	 SetSavePath(const string & strPath) { m_strSavePath = strPath; }
	void	 SetMainKbps(int nMainKbps) { m_nMainKbps = nMainKbps; } 
	void     SetSubKbps(int nSubKbps) { m_nSubKbps = nSubKbps; }
	void	 SetSnapStep(int nStep) { m_nSnapStep = nStep; }
	void     SetRecSlice(int nSlice) { m_nRecSlice = nSlice; }

	void	 SetCamera(int nCameraID, GM_MapData & inMapData) { m_MapNodeCamera[nCameraID] = inMapData; }
	void	 GetCamera(int nCameraID, GM_MapData & outMapData) { outMapData = m_MapNodeCamera[nCameraID]; }
	GM_MapNodeCamera & GetNodeCamera() { return m_MapNodeCamera; }

	void	 SetCourse(int nCameraID, GM_MapCourse & inMapCourse);
	void	 GetCourse(int nCameraID, GM_MapCourse & outMapCourse);
	GM_MapNodeCourse & GetNodeCourse();

	void	SetDBCameraID(int nDBCameraID, int inLocalID) { m_MapDBCamera[nDBCameraID] = inLocalID; }
	void	GetDBCameraID(int nDBCameraID, int & outLocalID);

	BOOL	StreamSnapJpeg(const CString & inSrcMP4File, const CString & inDestJpgName, int nRecSec);
private:
	BOOL	GenerateJpegFromMediaFile(const CString & inMP4File, int nPosSec, int nFrames);
	void	SendMPlayerCmd(const CString &cmdLine);
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
	int					m_nSnapStep;					// 截图间隔时间(秒)...
	int					m_nRecSlice;					// 录像切片时间(秒)...
	BOOL				m_bAutoLinkDVR;					// 自动重连DVR摄像头...
	BOOL				m_bAutoLinkFDFS;				// 自动重连FDFS服务器...
	string				m_strWebAddr;					// Web的IP地址...
	int					m_nWebPort;						// Web的端口地址...

	string				m_strRemoteAddr;				// 远程中转服务器的IP地址...
	int					m_nRemotePort;					// 远程中转服务器的端口地址...
	string				m_strTrackerAddr;				// FDFS-Tracker的IP地址...
	int					m_nTrackerPort;					// FDFS-Tracker的端口地址...

	int					m_nMaxCamera;					// 能够支持的最大摄像头数（默认为16个）
	GM_MapNodeCamera	m_MapNodeCamera;				// 监控通道配置信息(从1开始)
	GM_MapNodeCourse	m_MapNodeCourse;				// 监控通道课表记录(数据库ID)
	OSMutex				m_MutexCourse;					// 专门用于课程表的互斥对象
	GM_MapDBCamera		m_MapDBCamera;					// 摄像头数据编号与本地编号对应映射...

	friend class CHaoYiView;
};
