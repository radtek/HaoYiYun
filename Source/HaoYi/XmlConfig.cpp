
#include "StdAfx.h"
#include "UtilTool.h"
#include "XmlConfig.h"

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
};

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CXmlConfig::CXmlConfig(void)
  : m_nMaxCamera(DEF_MAX_CAMERA)
  , m_nMainKbps(DEF_MAIN_KBPS)
  , m_nSubKbps(DEF_SUB_KBPS)
  , m_bAutoLinkFDFS(false)
  , m_bAutoLinkDVR(false)
  , m_bAuthLicense(false)
  , m_strSavePath(DEF_REC_PATH)
  , m_strMainName(DEF_MAIN_NAME)
  , m_strWebAddr(DEF_CLOUD_MONITOR)
  , m_nWebPort(DEF_WEB_PORT)
  , m_nDBHaoYiGatherID(-1)
  , m_nDBHaoYiNodeID(-1)
  , m_strAuthExpired("")
  , m_strTrackerAddr("")
  , m_strRemoteAddr("")
  , m_nTrackerPort(0)
  , m_nDBGatherID(-1)
  , m_nRemotePort(0)
  , m_strWebName("")
  , m_strWebTag("")
  , m_strWebVer("")
  , m_nWebType(-1)
  , m_nAuthDays(0)
  , m_nSliceVal(0)
  , m_nInterVal(0)
  , m_nSnapVal(2)
{
	CString strVersion;
	strVersion.Format("V%s - Build %s", CUtilTool::GetServerVersion(), __DATE__);
	m_strVersion = strVersion;
	m_strCopyRight = "������һ�Ƽ����޹�˾ ��Ȩ����(C) 2017-2020";
	m_strPhone = "15010119735";
	m_strWebSite = DEF_WEB_HOME;	
	//m_strAddress = "�����к��������Ļ���·68��6��C16";
}

CXmlConfig::~CXmlConfig(void)
{
	this->GMSaveConfig();
}

CXmlConfig & CXmlConfig::GMInstance()
{
	static CXmlConfig cGlobal;
	return cGlobal;
}

BOOL CXmlConfig::GMLoadConfig()
{
	// �����һЩ����...
	m_MapNodeCamera.clear();
	m_MapServer.clear();

	// �õ�xml�����ļ���ȫ·��...
	TCHAR	 szPath[MAX_PATH] = {0};
	CUtilTool::GetFilePath(szPath, XML_CONFIG);
	ASSERT( strlen(szPath) > 0 );
	m_strXMLFile = szPath;

	// ��ȡ��ͼ����ȫ·��...
	CUtilTool::GetFilePath(szPath, SNAP_TOOL_NAME);
	ASSERT( strlen(szPath) > 0 );
	m_strMPlayer = szPath;

	// �������ýڵ���Ҫ�ı���...
	BOOL			bLoadOK = true;
	TiXmlDocument	theDoc;
	TiXmlElement  * lpRootElem = NULL;
	TiXmlElement  * lpCommElem = NULL;
	TiXmlElement  * lpServerElem = NULL;
	TiXmlElement  * lpAboutElem = NULL;
	TiXmlElement  * lpChildElem = NULL;
	// ��ʼ�������ýڵ�...
	do {
		// ���������ļ�ʧ��...
		if( !theDoc.LoadFile(m_strXMLFile) ) {
			bLoadOK = false;
			break;
		}
		// �޷���ȡ���ڵ�...
		lpRootElem = theDoc.RootElement();
		if( lpRootElem == NULL ) {
			bLoadOK = false;
			break;
		}
		// ��ȡ���ڽڵ�/�����ڵ�/��ؽڵ�...
		lpCommElem = lpRootElem->FirstChildElement("Common");
		lpAboutElem = lpRootElem->FirstChildElement("About");
		lpServerElem = lpRootElem->FirstChildElement("Server");
		// û�й��ڽڵ��û�й����ڵ㣬���¹���...
		if( lpCommElem == NULL || lpAboutElem == NULL || lpServerElem == NULL ) {
			bLoadOK = false;
			break;
		}
		// ���������ļ��ɹ�...
		bLoadOK = true;
	}while( false );
	// �������ʧ�ܣ�����Ĭ�ϵ�������Ϣ...
	if( !bLoadOK ) {
		//����ע���
		/*HKEY  hKey = NULL;
		TCHAR szPath[MAX_PATH] = {0};		
		::GetModuleFileName(NULL, szPath, MAX_PATH);
		unsigned long nSize = strlen(szPath);
		if( RegOpenKey(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", &hKey) != ERROR_SUCCESS ) {
			return false;
		}
		if( RegSetValueEx(hKey, "HaoYi AutoRun", 0, REG_SZ, (BYTE*)szPath, nSize) != ERROR_SUCCESS ) {
			RegCloseKey(hKey);
			return false;
		}
		if( RegDeleteValue(hKey, "HaoYi AutoRun") != ERROR_SUCCESS ) {
			RegCloseKey(hKey);
			return false;
		}
		RegCloseKey(hKey);*/
		// ����Ĭ�ϵ���������...
		return this->GMSaveConfig();
	}
	ASSERT( bLoadOK && lpAboutElem != NULL && lpRootElem != NULL && lpCommElem != NULL && lpServerElem != NULL );
	// ���ȣ���ȡ�������ýڵ���Ϣ...
	lpChildElem = ((lpAboutElem != NULL) ? lpAboutElem->FirstChildElement() : NULL);
	while( lpChildElem != NULL ) {
		LPCTSTR	lpszText = lpChildElem->GetText();
		const string & strValue = lpChildElem->ValueStr();
		if( strValue == "CopyRight" ) {
			m_strCopyRight = ((lpszText != NULL && strlen(lpszText) > 0) ? CUtilTool::UTF8_ANSI(lpszText) : "");
		} else if( strValue == "Version" ) {
			m_strVersion = ((lpszText != NULL && strlen(lpszText) > 0) ? CUtilTool::UTF8_ANSI(lpszText) : "");
		} else if( strValue == "Phone" ) {
			m_strPhone = ((lpszText != NULL && strlen(lpszText) > 0) ? CUtilTool::UTF8_ANSI(lpszText) : "");
		} else if( strValue == "WebSite" ) {
			m_strWebSite = ((lpszText != NULL && strlen(lpszText) > 0) ? CUtilTool::UTF8_ANSI(lpszText) : "");
		//} else if( strValue == "Address" ) {
		//	m_strAddress = ((lpszText != NULL && strlen(lpszText) > 0) ? CUtilTool::UTF8_ANSI(lpszText) : "");
		}
		lpChildElem = lpChildElem->NextSiblingElement();
	}
	// Ȼ�󣬶�ȡ�������ýڵ���Ϣ...
	lpChildElem = lpCommElem->FirstChildElement();
	while( lpChildElem != NULL ) {
		LPCTSTR	lpszText = lpChildElem->GetText();
		const string & strValue = lpChildElem->ValueStr();
		if( strValue == "SavePath" ) {
			m_strSavePath = ((lpszText != NULL && strlen(lpszText) > 0 ) ? CUtilTool::UTF8_ANSI(lpszText) : DEF_REC_PATH);
		}/* else if( strValue == "WebAddr" ) {
			m_strWebAddr = ((lpszText != NULL && strlen(lpszText) > 0 ) ? lpszText : DEF_CLOUD_MONITOR);
		} else if( strValue == "WebPort" ) {
			m_nWebPort = ((lpszText != NULL && strlen(lpszText) > 0 ) ? atoi(lpszText) : DEF_WEB_PORT);
		} else if( strValue == "MainName" ) {
			m_strMainName = ((lpszText != NULL && strlen(lpszText) > 0 ) ? CUtilTool::UTF8_ANSI(lpszText) : DEF_MAIN_NAME);
		} else if( strValue == "MainKbps" ) {
			m_nMainKbps = ((lpszText != NULL && strlen(lpszText) > 0 ) ? atoi(lpszText) : DEF_MAIN_KBPS);
		} else if( strValue == "SubKbps" ) {
			m_nSubKbps = ((lpszText != NULL && strlen(lpszText) > 0 ) ? atoi(lpszText) : DEF_SUB_KBPS);
		} else if( strValue == "AutoLinkDVR" ) {
			m_bAutoLinkDVR = ((lpszText != NULL && strlen(lpszText) > 0 ) ? atoi(lpszText) : false);
		} else if( strValue == "AutoLinkFDFS" ) {
			m_bAutoLinkFDFS = ((lpszText != NULL && strlen(lpszText) > 0 ) ? atoi(lpszText) : false);
		} else if( strValue == "MaxCamera" ) {
			m_nMaxCamera = ((lpszText != NULL && strlen(lpszText) > 0 ) ? atoi(lpszText) : DEF_MAX_CAMERA);
		}*/
		lpChildElem = lpChildElem->NextSiblingElement();
	}
	// ��ȡ�������б�����...
	lpChildElem = lpServerElem->FirstChildElement();
	while( lpChildElem != NULL ) {
		// ��ȡÿ���ڵ����������Ԫ��...
		LPCTSTR lpszName = lpChildElem->Attribute("Name");
		LPCTSTR lpszPort = lpChildElem->Attribute("Port");
		LPCTSTR lpszFocus = lpChildElem->Attribute("Focus");
		// �����ݴ�ŵ����ϵ���...
		if( lpszName != NULL && lpszPort != NULL ) {
			m_MapServer[lpszName] = atoi(lpszPort);
		}
		// ����ǽ���ڵ㣬���µ��������� => ������Ч��ʹ��Ĭ��ֵ...
		if( lpszFocus != NULL && atoi(lpszFocus) > 0 ) {
			m_strWebAddr = ((lpszName != NULL) ? lpszName : DEF_CLOUD_MONITOR);
			m_nWebPort = ((lpszPort != NULL) ? atoi(lpszPort) : DEF_WEB_PORT);
		}
		// ������һ���������ڵ����...
		lpChildElem = lpChildElem->NextSiblingElement();
	}
	// 2017.10.27 - by jackey => ͨ�����ã�ȫ�����õ���վ��...
	// ���ţ���ȡƵ�����ýڵ���Ϣ...
	/*TiXmlElement * lpCameraElem = ((lpTrackElem != NULL) ? lpTrackElem->FirstChildElement() : NULL);
	while( lpCameraElem != NULL ) {
		TiXmlElement * lpElemID = lpCameraElem->FirstChildElement("ID");
		if( lpElemID == NULL ) {
			lpCameraElem = lpCameraElem->NextSiblingElement();
			continue;
		}
		// �жϽڵ����Ƿ���ȷ���������0...
		ASSERT( lpElemID != NULL );
		LPCTSTR	lpszValue = NULL;
		LPCTSTR lpszText = lpElemID->GetText();
		int nCameraID = ((lpszText != NULL && strlen(lpszText) > 0) ? atoi(lpszText) : 0);
		if( nCameraID <= 0 ) {
			lpCameraElem = lpCameraElem->NextSiblingElement();
			continue;
		}
		ASSERT( nCameraID > 0 );
		// ��ȡ�ڵ����ԣ������µļ�ؽڵ�...
		GM_MapData    theData;
		lpChildElem = lpCameraElem->FirstChildElement();
		while( lpChildElem != NULL ) {
			lpszValue = lpChildElem->Value();
			lpszText = lpChildElem->GetText();
			// ����ڵ����Ե����ƺ�����...
			if( lpszValue != NULL && lpszText != NULL ) {
				theData[lpszValue] = CUtilTool::UTF8_ANSI(lpszText);
			}
			// ��һ���ӽڵ�����...
			lpChildElem = lpChildElem->NextSiblingElement();
		}
		// �������ã���һ�����ͨ���ڵ�...
		m_MapNodeCamera[nCameraID] = theData;
		lpCameraElem = lpCameraElem->NextSiblingElement();
	}*/
	return true;
}

BOOL CXmlConfig::GMSaveConfig()
{
	if( m_strXMLFile.size() <= 0 )
		return false;
	// ����һЩ��Ҫ�ı���...
	TiXmlDocument	theDoc;
	TiXmlElement	rootElem("Config");
	TiXmlElement	commElem("Common");
	TiXmlElement	aboutElem("About");
	TiXmlElement	serverElem("Server");
	TiXmlElement	theElem("None");
	// �����ļ�ͷ...
	theDoc.Parse(XML_DECLARE_UTF8);
	// ���泣�����ýڵ���Ϣ...
	if( m_strSavePath.size() > 0 ) {
		theElem = this->BuildXmlElem("SavePath", CUtilTool::ANSI_UTF8(m_strSavePath.c_str()));
		commElem.InsertEndChild(theElem);
	}
	// ����������б�Ϊ�գ��Զ�����Ĭ�ϵ����ӵ�ַ...
	if( m_MapServer.size() <= 0 ) {
		m_MapServer[DEF_CLOUD_MONITOR] = DEF_WEB_PORT;
		m_MapServer[DEF_CLOUD_RECORDER] = DEF_WEB_PORT;
	}
	// ��չ����վ��ַ�Ͷ˿ڣ����Դ�Ŷ�����¼...
	GM_MapServer::iterator itorServer;
	for(itorServer = m_MapServer.begin(); itorServer != m_MapServer.end(); ++itorServer) {
		// �½�һ���ڵ����...
		TiXmlElement theAddress("Address");
		// ���õ�ַ�Ͷ˿�����Ԫ��...
		theAddress.SetAttribute("Name", itorServer->first);
		theAddress.SetAttribute("Port", itorServer->second);
		// �жϵ�ǰʹ�õ�ַ�Ƿ��ǽ����ַ => ��Сд�ڴ���ǰ�Ѿ�������...
		BOOL theFocus = ((m_strWebAddr.compare(itorServer->first) == 0) ? true : false);
		theAddress.SetAttribute("Focus", theFocus);
		serverElem.InsertEndChild(theAddress);
	}
	//////////////////////////////////////////////////////////
	// ���³������ö���ŵ��ڴ�����ݿ⵱��...
	//////////////////////////////////////////////////////////
	/*// ���������ڱ�������...
	theElem = this->BuildXmlElem("MainName", CUtilTool::ANSI_UTF8(m_strMainName.c_str()));
	commElem.InsertEndChild(theElem);
	// ������������������������Ϣ...
	theElem = this->BuildXmlElem("MainKbps", m_nMainKbps);
	commElem.InsertEndChild(theElem);
	theElem = this->BuildXmlElem("SubKbps", m_nSubKbps);
	commElem.InsertEndChild(theElem);
	theElem = this->BuildXmlElem("SnapStep", m_nSnapStep);
	commElem.InsertEndChild(theElem);
	theElem = this->BuildXmlElem("RecSlice", m_nRecSlice);
	commElem.InsertEndChild(theElem);
	theElem = this->BuildXmlElem("AutoLinkDVR", m_bAutoLinkDVR);
	commElem.InsertEndChild(theElem);
	theElem = this->BuildXmlElem("AutoLinkFDFS", m_bAutoLinkFDFS);
	commElem.InsertEndChild(theElem);
	theElem = this->BuildXmlElem("MaxCamera", m_nMaxCamera);
	commElem.InsertEndChild(theElem);*/
	// ����������ýڵ���Ϣ...
	theElem = this->BuildXmlElem("CopyRight", CUtilTool::ANSI_UTF8(m_strCopyRight.c_str()));
	aboutElem.InsertEndChild(theElem);
	theElem = this->BuildXmlElem("Version", CUtilTool::ANSI_UTF8(m_strVersion.c_str()));
	aboutElem.InsertEndChild(theElem);
	theElem = this->BuildXmlElem("Phone", CUtilTool::ANSI_UTF8(m_strPhone.c_str()));
	aboutElem.InsertEndChild(theElem);
	theElem = this->BuildXmlElem("WebSite", CUtilTool::ANSI_UTF8(m_strWebSite.c_str()));
	aboutElem.InsertEndChild(theElem);
	//theElem = this->BuildXmlElem("Address", CUtilTool::ANSI_UTF8(m_strAddress.c_str()));
	//aboutElem.InsertEndChild(theElem);
	// 2017.10.27 - by jackey => ͨ������ȫ�����õ���վ�ˣ������̵�����...
	// ��ʼ������ͨ���б�...
	/*GM_MapNodeCamera::iterator itorData;
	for(itorData = m_MapNodeCamera.begin(); itorData != m_MapNodeCamera.end(); ++itorData) {
		TiXmlElement theCamera("Camera");
		GM_MapData & theData = itorData->second;
		// Ϊ�˱�֤ ID �� Name ��ǰ����ʾ�����ʱ��ſ�ʼת����UTF8��ʽ...
		TiXmlElement theID = this->BuildXmlElem("ID", theData["ID"]);
		TiXmlElement theName = this->BuildXmlElem("Name", CUtilTool::ANSI_UTF8(theData["Name"].c_str()));
		theCamera.InsertEndChild(theID);
		theCamera.InsertEndChild(theName);

		GM_MapData::iterator itorItem;
		for(itorItem = theData.begin(); itorItem != theData.end(); ++itorItem) {
			// ID �� Name �Ѿ����̣�DBCameraID �����̣�ÿ�δ����ݿ��ȡ...
			if( itorItem->first == "ID" || itorItem->first == "Name" || itorItem->first == "DBCameraID" )
				continue;
			TiXmlElement theNode = this->BuildXmlElem(itorItem->first, CUtilTool::ANSI_UTF8(itorItem->second.c_str()));
			theCamera.InsertEndChild(theNode);
		}

		// ����ؽڵ���뵽���ڵ㵱��...
		trackElem.InsertEndChild(theCamera);
	}*/
	// ��Ͻڵ��б�...
	rootElem.InsertEndChild(commElem);
	rootElem.InsertEndChild(serverElem);
	rootElem.InsertEndChild(aboutElem);
	theDoc.InsertEndChild(rootElem);
	// ��󣬽��д��̴���...
	return theDoc.SaveFile(m_strXMLFile);
}
//
// ����ѡ�еĵ�ַ�Ͷ˿ڸ��µ��������б������óɽ��㣬Ȼ�����...
void CXmlConfig::SetFocusServer(const string & inWebAddr, int inWebPort)
{
	// ֱ�Ӹ��¼�¼��û���Զ������������Զ�����...
	m_MapServer[inWebAddr] = inWebPort;
	// ��������ĵ�ַ�Ͷ˿ڵ����������...
	m_strWebAddr = inWebAddr;
	m_nWebPort = inWebPort;
	// �����յĽ�����������ļ�����...
	this->GMSaveConfig();
}

TiXmlElement CXmlConfig::BuildXmlElem(const string & strNode, int inData)
{
	string	strValue;
	TCHAR	szBuf[80] = {0};
	sprintf(szBuf, "%d", inData);
	strValue = szBuf;
	return this->BuildXmlElem(strNode, strValue);	
}

TiXmlElement CXmlConfig::BuildXmlElem(const string & strNode, const string & strData)
{
	TiXmlText  txtElem(strData);
	TiXmlElement itemElem(strNode);
	itemElem.InsertEndChild(txtElem);
	return itemElem;
}
//
// �γ̱��Ƕ�̬�洢������浽����...
void CXmlConfig::SetCourse(int nDBCameraID, GM_MapCourse & inMapCourse)
{
	OSMutexLocker theLock(&m_MutexCourse);
	m_MapNodeCourse[nDBCameraID] = inMapCourse;
}
//
// �γ̱��Ƕ�̬�洢������浽����...
void CXmlConfig::GetCourse(int nDBCameraID, GM_MapCourse & outMapCourse)
{
	OSMutexLocker theLock(&m_MutexCourse);
	outMapCourse = m_MapNodeCourse[nDBCameraID];
}
//
// �γ̱��Ƕ�̬�洢������浽����...
GM_MapNodeCourse & CXmlConfig::GetNodeCourse()
{
	OSMutexLocker theLock(&m_MutexCourse);
	return m_MapNodeCourse;
}
//
// ͨ��ͨ����Ż�ȡͨ������...
CString CXmlConfig::GetDBCameraTitle(int nDBCameraID)
{
	CString strTitle;
	GM_MapNodeCamera::iterator itorItem = m_MapNodeCamera.find(nDBCameraID);
	if( itorItem != m_MapNodeCamera.end() ) {
		GM_MapData & theMapWeb = itorItem->second;
		strTitle = theMapWeb["camera_name"].c_str();
	}
	return strTitle;
}
//
// ����ɾ��ָ��ͨ���Ĳ���...
void CXmlConfig::doDelDVR(int nDBCameraID)
{
	// ɾ����Ӧ��¼��γ̼�¼...
	OSMutexLocker theLock(&m_MutexCourse);
	m_MapNodeCourse.erase(nDBCameraID);
	// ɾ����Ӧ�������ļ���¼...
	m_MapNodeCamera.erase(nDBCameraID);
}

#define DEF_PER_WAIT_MS		50	// ÿ�εȴ�������
#define DEF_MAX_WAIT_COUNT  50	// COUNT * MS = �ܺ���
#define DEF_SNAP_JPG_NAME   TEXT("00000001.jpg")
#define SafeCloseHandle(handle)	do{ if( handle ) { CloseHandle(handle); handle = 0L; } }while(0)

//
// �������ת��ģʽ�Ľ�ͼ�ӿں���...
BOOL CXmlConfig::StreamSnapJpeg(const CString & inSrcMP4File, const CString & inDestJpgName, int nRecSec)
{
	// ֻ��ȡ 2 �������ڵ�����...
	int nSnapSize = ((nRecSec >= 120) ? 120 : nRecSec);
	int nSnapPoint = 0;
	if( nSnapSize > 5 ) {
		// �����ȡ��ʱ���һ��...
		srand((unsigned)time(NULL));
		nSnapPoint = (rand() % (nSnapSize/2))+3;
		// ��Զ��Ҫ������ʱ���һ�룬���򣬽�ͼ���߻��ȡ��������...
		nSnapPoint = min(nSnapPoint, (nSnapSize/2));
	}
	// ׼����ͼ���λ��...
	CString strSrcJpgName;
	TCHAR szCurDir[MAX_PATH] = {0};
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	if( ::GetCurrentDirectory(MAX_PATH, szCurDir) ) {
		strSrcJpgName.Format("%s\\%s", szCurDir, DEF_SNAP_JPG_NAME);
	} else {
		strSrcJpgName = DEF_SNAP_JPG_NAME;
	}
	// ֻ���� ANSI �ַ���ʽ��ͼ...
	this->GenerateJpegFromMediaFile(inSrcMP4File, nSnapPoint, 1);
	do {
		// �����ͼ�Ѿ����ڣ�ֱ������...
		if( _access(strSrcJpgName, 0) >= 0 )
			break;
		// ��ͼʧ�ܣ������ͼʱ�����0�㣬ֱ������...
		if( nSnapPoint <= 0 )
			break;
		// ����ʹ�� 0 ����н�ͼ...
		this->GenerateJpegFromMediaFile(inSrcMP4File, 0, 1);
	}while( false );
	// �ٴ��жϽ�ͼ�ļ��Ƿ���ڣ�������ֱ�ӷ���...
	if( _access(strSrcJpgName, 0) < 0 )
		return false;
	// ����ͼ�ļ��������ϴ�Ŀ¼���� => ����ģʽ...
	::CopyFile(strSrcJpgName, inDestJpgName, false);
	// ��ͼ��ɣ�ɾ��ԭʼ��ͼ�ļ�...
	::DeleteFile(strSrcJpgName);
	return true;
}

BOOL CXmlConfig::GenerateJpegFromMediaFile(const CString & inMP4File, int nPosSec, int nFrames)
{
	if( m_strMPlayer.size() <= 0 || inMP4File.GetLength() <= 0 )
		return false;
    CString strCmdLine;
    strCmdLine.Format(TEXT("-ss %d -nosound -vo jpeg -frames %d \"%s\""), nPosSec, nFrames, inMP4File);
    TRACE("%s %s\n", m_strMPlayer.c_str(), strCmdLine);

	this->SendMPlayerCmd(strCmdLine);
	
	return true;
}

void CXmlConfig::SendMPlayerCmd(const CString &cmdLine)
{
	ASSERT( m_strMPlayer.size() > 0 );

	SECURITY_ATTRIBUTES lsa = { 0 };
	PROCESS_INFORMATION pi = { 0 };
	STARTUPINFO si = { 0 };

	HANDLE dupIn = 0L, dupOut = 0L;
	HANDLE stdChildIn = 0L, stdChildOut = 0L, stdChildErr = 0L;
	HANDLE stdoldIn = 0L, stdoldOut = 0L, stdoldErr = 0L;
	HANDLE stddupIn = 0L, stddupOut = 0L, stddupErr = 0L;
	
	HANDLE my_read = NULL;						/*!< Application read file descriptor */
	HANDLE my_write = NULL;						/*!< Application write file descriptor */

	HANDLE his_read = NULL;						/*!< Process read file descriptor */
	HANDLE his_write = NULL;					/*!< Process write file descriptor */

	HANDLE pstdin = NULL;						/*!< stdin descriptor used by the child */
	HANDLE pstdout = NULL;						/*!< stdout descriptor used by the child */
	HANDLE pstderr = NULL;						/*!< stderr descriptor used by the child */
	
	lsa.nLength = sizeof(SECURITY_ATTRIBUTES);
	lsa.lpSecurityDescriptor = NULL;
	lsa.bInheritHandle = TRUE;

	do {
		// Create the pipes used for the cvsgui protocol
		if( !CreatePipe(&his_read, &dupIn, &lsa, 0) ) break;
		if( !CreatePipe(&dupOut, &his_write, &lsa, 0) ) break;

		// Duplicate the application side handles so they lose the inheritance
		if( !DuplicateHandle(GetCurrentProcess(), dupIn,
			GetCurrentProcess(), &my_write, 0,
			FALSE, DUPLICATE_SAME_ACCESS) ) break;
		SafeCloseHandle(dupIn);
		
		if( !DuplicateHandle(GetCurrentProcess(), dupOut,
			GetCurrentProcess(), &my_read, 0,
			FALSE, DUPLICATE_SAME_ACCESS) ) break;
		SafeCloseHandle(dupOut);

		// Redirect stdout, stderr, stdin
		if( !CreatePipe(&stdChildIn, &stddupIn, &lsa, 0) ) break;
		if( !CreatePipe(&stddupOut, &stdChildOut, &lsa, 0) ) break;
		if( !CreatePipe(&stddupErr, &stdChildErr, &lsa, 0) ) break;

		// Same thing as above
		if( !DuplicateHandle(GetCurrentProcess(), stddupIn,
			GetCurrentProcess(), &pstdin, 0,
			FALSE, DUPLICATE_SAME_ACCESS) ) break;
		SafeCloseHandle(stddupIn);

		if( !DuplicateHandle(GetCurrentProcess(), stddupOut,
			GetCurrentProcess(), &pstdout, 0,
			FALSE, DUPLICATE_SAME_ACCESS) ) break;
		SafeCloseHandle(stddupOut);

		if( !DuplicateHandle(GetCurrentProcess(), stddupErr,
			GetCurrentProcess(), &pstderr, 0,
			FALSE, DUPLICATE_SAME_ACCESS) ) break;
		SafeCloseHandle(stddupErr);
		
		si.cb = sizeof(STARTUPINFO);
		si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;
		si.hStdInput = stdChildIn;
		si.hStdOutput = stdChildOut;
		si.hStdError = stdChildErr;

		stdoldIn  = GetStdHandle(STD_INPUT_HANDLE);
		stdoldOut = GetStdHandle(STD_OUTPUT_HANDLE);
		stdoldErr = GetStdHandle(STD_ERROR_HANDLE);

		SetStdHandle(STD_INPUT_HANDLE,  stdChildIn);
		SetStdHandle(STD_OUTPUT_HANDLE, stdChildOut);
		SetStdHandle(STD_ERROR_HANDLE,  stdChildErr);

		// ����������...
        CString strCmdLine;
        strCmdLine.Format(TEXT("\"%s\" %s"), m_strMPlayer.c_str(), cmdLine);
        LPSTR lpszScript = (LPSTR)strCmdLine.GetString();
		//LPSTR	lpszScript = "d:\\mplayer2008.02.06\\mplayer.exe -identify d:\\film\\3.rmvb -nosound -vc dummy -vo null";
		if( !CreateProcess(
			NULL,
			lpszScript,
			NULL,
			NULL,
			TRUE,
			NORMAL_PRIORITY_CLASS | CREATE_SUSPENDED,
			NULL,
			NULL,
			&si,
			&pi) ) break;
		SetStdHandle(STD_INPUT_HANDLE,  stdoldIn);
		SetStdHandle(STD_OUTPUT_HANDLE, stdoldOut);
		SetStdHandle(STD_ERROR_HANDLE,  stdoldErr);

		SafeCloseHandle(my_read);
		SafeCloseHandle(my_write);
		SafeCloseHandle(his_read);
		SafeCloseHandle(his_write);
		SafeCloseHandle(stdChildIn);
		SafeCloseHandle(stdChildOut);
		SafeCloseHandle(stdChildErr);
		
		// 2017.08.22 - by jackey => �ȴ�ʱ�����ӵ�50*50����...
		if(((DWORD)-1) != ResumeThread(pi.hThread))
        {
            BOOL bRet = false;
            DWORD dwCode = 0;
			int  nCount = 0;
			while( true ) {
	            bRet = GetExitCodeProcess(pi.hProcess, &dwCode);
				if( dwCode != STILL_ACTIVE )
					break;
				// �����X����֮����Ȼû�н�ͼ�ɹ����жϽ���...
				if( ++nCount >= DEF_MAX_WAIT_COUNT ) {
					TerminateProcess(pi.hProcess, 0);
					break;
				}
				// ���� X ����֮�󣬼����ȴ�...
				::Sleep(DEF_PER_WAIT_MS);
			}
        }
		// ��ʼ��ȡ��ͼ������� => TRACE �г�������...
#ifdef _DEBUG
		string outString;
		this->ReadHolePipe(pstdout, outString);
		OutputDebugString(outString.c_str());
#endif // _DEBUG
		SafeCloseHandle(pi.hThread);  
		SafeCloseHandle(pi.hProcess);
	} while( FALSE );
	
	SafeCloseHandle(dupIn);
	SafeCloseHandle(dupOut);

	SafeCloseHandle(stdChildIn);
	SafeCloseHandle(stdChildOut);
	SafeCloseHandle(stdChildErr);

	SafeCloseHandle(stddupIn);
	SafeCloseHandle(stddupOut);
	SafeCloseHandle(stddupErr);
	
	SafeCloseHandle(my_read);
	SafeCloseHandle(my_write);
	SafeCloseHandle(his_read);
	SafeCloseHandle(his_write);
	SafeCloseHandle(pstdin);
	SafeCloseHandle(pstdout);
	SafeCloseHandle(pstderr);
}

void CXmlConfig::ReadHolePipe(HANDLE hStdOut, string & strPipe)
{
    const   int MAX_SIZE = 4096;

	BOOL	bRet = FALSE;
	DWORD	dwRead = 0;
	TCHAR	szBuf[MAX_SIZE + 1] = {0};
	bRet = PeekNamedPipe(hStdOut, NULL, 0, NULL, &dwRead, NULL);
	while( bRet && dwRead > 0 )
	{
		ASSERT( hStdOut != NULL );
		memset(szBuf, 0, MAX_SIZE);
		if( !ReadFile(hStdOut, szBuf, MAX_SIZE, &dwRead, NULL) || dwRead == 0 )
			break;
		ASSERT( MAX_SIZE >= dwRead );
		strPipe.append(szBuf); ::Sleep(5);
		bRet = PeekNamedPipe(hStdOut, NULL, 0, NULL, &dwRead, NULL);
	}
}
//
// ����ffmpeg�ӿڣ����ж�̬��ͼ...
BOOL CXmlConfig::FFmpegSnapJpeg(const string & inSrcFrame, const CString & inDestJpgName)
{
	if( inSrcFrame.size() <= 0 || inDestJpgName.GetLength() <= 0 )
		return false;
	// ׼��һЩ�ض��Ĳ���...
	AVCodecID src_codec_id = AV_CODEC_ID_H264;
	AVCodecParserContext * lpSrcCodecParserCtx = NULL;
    AVCodecContext * lpSrcCodecCtx = NULL;
	AVCodec * lpSrcCodec = NULL;
    AVFrame	* lpSrcFrame = NULL;
	AVPacket  theSrcPacket = {0};
	bool      bReturn = false;
	// ����ffmpeg��log���𣬲�ע��������ͱ�����...
	av_log_set_level(AV_LOG_VERBOSE);
	av_register_all();
	do {
		// ������Ҫ�Ľ����������������������...
		lpSrcCodec = avcodec_find_decoder(src_codec_id);
		if( lpSrcCodec == NULL )
			break;
		lpSrcCodecCtx = avcodec_alloc_context3(lpSrcCodec);
		if( lpSrcCodecCtx == NULL )
			break;
		lpSrcCodecParserCtx = av_parser_init(src_codec_id);
		if( lpSrcCodecParserCtx == NULL )
			break;
		// �򿪻�ȡ���Ľ�����...
		if( avcodec_open2(lpSrcCodecCtx, lpSrcCodec, NULL) < 0 )
			break;
		// ��ʼ��ffmpeg����֡...
		lpSrcFrame = av_frame_alloc();
		av_init_packet(&theSrcPacket);
		// �������ݹ�����h264����֡...
		uint8_t * lpCurPtr = (uint8_t*)inSrcFrame.c_str();
		int nCurSize = inSrcFrame.size();
		int got_picture = 0;
		int	nResult = 0;
		while( nCurSize > 0 ) {
			// ������Ҫ��ν�����ֱ�����������еĻ���Ϊֹ...
			nResult = av_parser_parse2( lpSrcCodecParserCtx, lpSrcCodecCtx,
							  &theSrcPacket.data, &theSrcPacket.size,
							  lpCurPtr, nCurSize, AV_NOPTS_VALUE,
							  AV_NOPTS_VALUE, AV_NOPTS_VALUE);
			lpCurPtr += nResult;
			nCurSize -= nResult;
			// û�н�����packet������...
			if( theSrcPacket.size == 0 )
				continue;
			// �Խ�����ȷ��packet���н������...
			nResult = avcodec_decode_video2(lpSrcCodecCtx, lpSrcFrame, &got_picture, &theSrcPacket);
			// ����ʧ�ܻ�û�еõ�����ͼ�񣬼�������...
			if( nResult < 0 || !got_picture )
				continue;
			// ����ɹ������һ�ȡ��һ��ͼ�񣬽��д��̲���...
			this->FFmpegSaveJpeg(lpSrcCodecCtx, lpSrcFrame, inDestJpgName);
			// ���óɹ���־���ж�ѭ��...
			bReturn = true;
			break;
		}
	}while( false );
	// ���õ������ݽ���������...
	av_free_packet(&theSrcPacket);
	// �ͷ�Ԥ���Ѿ�����Ŀռ�...
	if( lpSrcCodecParserCtx != NULL ) {
		av_parser_close(lpSrcCodecParserCtx);
		lpSrcCodecParserCtx = NULL;
	}
	if( lpSrcFrame != NULL ) {
		av_frame_free(&lpSrcFrame);
		lpSrcFrame = NULL;
	}
	if( lpSrcCodecCtx != NULL ) {
		avcodec_close(lpSrcCodecCtx);
		av_free(lpSrcCodecCtx);
	}
	// �������յĽ��...
	return bReturn;
}
//
// ��yuv���ݴ��̳�jpg�ļ�...
BOOL CXmlConfig::FFmpegSaveJpeg(AVCodecContext * pOrigCodecCtx, AVFrame * pOrigFrame, LPCTSTR lpszJpgName)
{
    AVOutputFormat * avOutputFormat = av_guess_format("mjpeg", NULL, NULL); //av_guess_format (0, lpszJpgName, 0);
    AVCodec * pOutAVCodec = avcodec_find_encoder(avOutputFormat->video_codec);
	if( pOutAVCodec == NULL )
		return false;
	int  nBufSize = 0;
	int  nEncSize = 0;
	BOOL bReturn = false;
	uint8_t * lpEncBuf = NULL;
	AVCodecContext * pOutCodecCtx = NULL;
	do {
		pOutCodecCtx = avcodec_alloc_context3(pOutAVCodec);
		if( pOutCodecCtx == NULL )
			break;
		// ׼�����ݽṹ��Ҫ�Ĳ���...
		pOutCodecCtx->bit_rate = pOrigCodecCtx->bit_rate;
		pOutCodecCtx->width = pOrigCodecCtx->width;
		pOutCodecCtx->height = pOrigCodecCtx->height;
		pOutCodecCtx->pix_fmt = avcodec_find_best_pix_fmt_of_list(pOutAVCodec->pix_fmts, pOrigCodecCtx->pix_fmt, 1, 0); //AV_PIX_FMT_YUVJ420P;  
		pOutCodecCtx->codec_id = avOutputFormat->video_codec; //AV_CODEC_ID_MJPEG;  
		pOutCodecCtx->codec_type = pOrigCodecCtx->codec_type; //AVMEDIA_TYPE_VIDEO;  
		pOutCodecCtx->time_base.num = 1; //pOrigCodecCtx->time_base.num;  
		pOutCodecCtx->time_base.den = 25; //pOrigCodecCtx->time_base.den;
		// ��ѹ����...
		if( avcodec_open2 (pOutCodecCtx, pOutAVCodec, 0) < 0 )
			break;
		pOutCodecCtx->mb_lmin = pOutCodecCtx->qmin * FF_QP2LAMBDA;  
		pOutCodecCtx->mb_lmax = pOutCodecCtx->qmax * FF_QP2LAMBDA;  
		pOutCodecCtx->flags = CODEC_FLAG_QSCALE;  
		pOutCodecCtx->global_quality = pOutCodecCtx->qmin * FF_QP2LAMBDA;  
		pOrigFrame->pts = 1;  
		pOrigFrame->quality = pOutCodecCtx->global_quality;
		// ׼�����ջ��棬��ʼѹ��jpg����...
		nBufSize = avpicture_get_size(pOutCodecCtx->pix_fmt, pOutCodecCtx->width, pOutCodecCtx->height); 
		lpEncBuf = (uint8_t *)malloc(nBufSize);
		nEncSize = avcodec_encode_video(pOutCodecCtx, lpEncBuf, nBufSize, pOrigFrame);
		if( nEncSize <= 0 )
			break;
		// ���浽jpg�ļ�����...
		FILE * pFile = fopen(lpszJpgName, "wb");
		if( pFile == NULL )
			break;
		fwrite(lpEncBuf, 1, nEncSize, pFile);
		fclose(pFile); pFile = NULL;
		// �ͷ��м���Դ�����سɹ�...
		free(lpEncBuf); lpEncBuf = NULL;
		bReturn = true;
	}while( false );
	// �����Ѿ�����Ŀռ�...
	if( lpEncBuf != NULL ) {
		free(lpEncBuf);
		lpEncBuf = NULL;
	}
	// �����м�����Ķ���...
	if( pOutCodecCtx != NULL ) {
		avcodec_close(pOutCodecCtx);
		av_free(pOutCodecCtx);
	}
	return bReturn;
}