
#pragma once

#include "tinyxml.h"
#include "OSMutex.h"

typedef map<int, GM_MapData>	GM_MapNodeCamera;	// int => ��ָ����ID
typedef map<int, GM_MapCourse>  GM_MapNodeCourse;	// int => ��ָ����ID
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

	/////////////////////////////////////////////////////////////
	// �⼸����ַ�Ͷ˿��Ƕ�̬��ȡ�ģ��������xml����...
	/////////////////////////////////////////////////////////////
	string & GetWebName() { return m_strWebName; }
	int		 GetWebType() { return m_nWebType; }
	string & GetRemoteAddr() { return m_strRemoteAddr; }
	int		 GetRemotePort() { return m_nRemotePort; }
	string & GetTrackerAddr() { return m_strTrackerAddr; }
	int		 GetTrackerPort() { return m_nTrackerPort; }
	void	 SetRemoteAddr(const string & strAddr) { m_strRemoteAddr = strAddr; }
	void     SetRemotePort(int nPort) { m_nRemotePort = nPort; }
	void	 SetTrackerAddr(const string & strAddr) { m_strTrackerAddr = strAddr; }
	void     SetTrackerPort(int nPort) { m_nTrackerPort = nPort; }
	void	 SetWebType(int nWebType) { m_nWebType = nWebType; }
	void	 SetWebName(const string & strWebName) { m_strWebName = strWebName; }

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
	int					m_nSnapStep;					// ��ͼ���ʱ��(��)...
	int					m_nRecSlice;					// ¼����Ƭʱ��(��)...
	BOOL				m_bAutoLinkDVR;					// �Զ�����DVR����ͷ...
	BOOL				m_bAutoLinkFDFS;				// �Զ�����FDFS������...
	string				m_strWebAddr;					// Web��IP��ַ...
	int					m_nWebPort;						// Web�Ķ˿ڵ�ַ...

	string				m_strWebName;					// ע��ʱ��ȡ����վ����
	int					m_nWebType;						// ע��ʱ��ȡ����վ����
	string				m_strRemoteAddr;				// Զ����ת��������IP��ַ...
	int					m_nRemotePort;					// Զ����ת�������Ķ˿ڵ�ַ...
	string				m_strTrackerAddr;				// FDFS-Tracker��IP��ַ...
	int					m_nTrackerPort;					// FDFS-Tracker�Ķ˿ڵ�ַ...

	int					m_nMaxCamera;					// �ܹ�֧�ֵ��������ͷ����Ĭ��Ϊ16����
	GM_MapNodeCamera	m_MapNodeCamera;				// ���ͨ��������Ϣ(��1��ʼ)
	GM_MapNodeCourse	m_MapNodeCourse;				// ���ͨ���α��¼(���ݿ�ID)
	OSMutex				m_MutexCourse;					// ר�����ڿγ̱�Ļ������
	GM_MapDBCamera		m_MapDBCamera;					// ����ͷ���ݱ���뱾�ر�Ŷ�Ӧӳ��...

	friend class CHaoYiView;
};
