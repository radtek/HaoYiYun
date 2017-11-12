
#pragma once

#include "tinyxml.h"
#include "OSMutex.h"

typedef map<int, GM_MapData>	GM_MapNodeCamera;	// int => ��ָ���ݿ�DBCameraID
typedef map<int, GM_MapCourse>  GM_MapNodeCourse;	// int => ��ָ���ݿ�DBCameraID

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
	// �⼸����ַ�Ͷ˿��Ƕ�̬��ȡ�ģ��������xml����...
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
	string				m_strCopyRight;					// ��Ȩ
	string				m_strVersion;					// �汾
	string				m_strPhone;						// �绰
	string				m_strWebSite;					// ��վ
	string				m_strAddress;					// ��ַ

	string				m_strMPlayer;					// ��ͼ���ߵ�ȫ·��...
	string				m_strXMLFile;					// XML�����ļ�
	string				m_strMainName;					// �����ڱ�������...
	string				m_strSavePath;					// ¼����ͼ���·��...
	int					m_nMainKbps;					// ��������С...
	int					m_nSubKbps;						// ��������С...
	int					m_nSliceVal;					// ¼����Ƭʱ��(1~30����)
	int					m_nInterVal;					// ��Ƭ����ؼ�֡(1~3��)
	BOOL				m_bAutoLinkDVR;					// �Զ�����DVR����ͷ...
	BOOL				m_bAutoLinkFDFS;				// �Զ�����FDFS������...
	string				m_strWebAddr;					// Web��IP��ַ...
	int					m_nWebPort;						// Web�Ķ˿ڵ�ַ...
	
	string				m_strWebTag;					// ע��ʱ��ȡ����վ��־
	string				m_strWebName;					// ע��ʱ��ȡ����վ����
	int					m_nWebType;						// ע��ʱ��ȡ����վ����
	string				m_strRemoteAddr;				// Զ����ת��������IP��ַ...
	int					m_nRemotePort;					// Զ����ת�������Ķ˿ڵ�ַ...
	string				m_strTrackerAddr;				// FDFS-Tracker��IP��ַ...
	int					m_nTrackerPort;					// FDFS-Tracker�Ķ˿ڵ�ַ...
	int                 m_nDBGatherID;					// ���ݿ��вɼ��˱��...
	int                 m_nDBHaoYiGatherID;				// �����ķ������ϵĲɼ��˱��...
	string				m_strAuthExpired;				// ���ķ�������������Ȩ����ʱ��...

	int					m_nMaxCamera;					// �ܹ�֧�ֵ��������ͷ����Ĭ��Ϊ16����
	GM_MapNodeCamera	m_MapNodeCamera;				// ���ͨ��������Ϣ(���ݿ�CameraID��
	GM_MapNodeCourse	m_MapNodeCourse;				// ���ͨ���α��¼(���ݿ�CourseID)
	OSMutex				m_MutexCourse;					// ר�����ڿγ̱�Ļ������

	friend class CHaoYiView;
};
