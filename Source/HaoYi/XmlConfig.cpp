
#include "StdAfx.h"
#include "UtilTool.h"
#include "XmlConfig.h"

CXmlConfig::CXmlConfig(void)
  : m_nMaxCamera(DEF_MAX_CAMERA)
  , m_nMainKbps(DEF_MAIN_KBPS)
  , m_nSubKbps(DEF_SUB_KBPS)
  , m_nSnapStep(DEF_SNAP_STEP)
  , m_nRecSlice(DEF_REC_SLICE)
  , m_bAutoLinkFDFS(false)
  , m_bAutoLinkDVR(false)
  , m_strSavePath(DEF_REC_PATH)
  , m_strMainName(DEF_MAIN_NAME)
  , m_strWebAddr(DEF_WEB_ADDR)
  , m_nWebPort(DEF_WEB_PORT)
  , m_strTrackerAddr("")
  , m_strRemoteAddr("")
  , m_nTrackerPort(0)
  , m_nRemotePort(0)
  , m_strWebName("")
  , m_nWebType(-1)
{
	CString strVersion;
	strVersion.Format("�� ����V%s - Build %s", CUtilTool::GetServerVersion(), __DATE__);
	m_strVersion = strVersion;
	m_strCopyRight = "������һ�Ƽ����޹�˾ ��Ȩ����(C) 2016-2020";
	m_strPhone = "�� ����15010119735";
	m_strWebSite = "�� վ��https://www.myhaoyi.com";	
	m_strAddress = "�� ַ�������к��������Ļ���·68��6��C16";
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
	TiXmlElement  * lpTrackElem = NULL;
	TiXmlElement  * lpCommElem = NULL;
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
		lpTrackElem = lpRootElem->FirstChildElement("Track");
		lpAboutElem = lpRootElem->FirstChildElement("About");
		// û�й��ڽڵ��û�й����ڵ㣬���¹���...
		if( lpCommElem == NULL || lpAboutElem == NULL ) {
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
	ASSERT( bLoadOK && lpAboutElem != NULL && lpRootElem != NULL && lpCommElem != NULL );
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
		} else if( strValue == "Address" ) {
			m_strAddress = ((lpszText != NULL && strlen(lpszText) > 0) ? CUtilTool::UTF8_ANSI(lpszText) : "");
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
		} else if( strValue == "MainName" ) {
			m_strMainName = ((lpszText != NULL && strlen(lpszText) > 0 ) ? CUtilTool::UTF8_ANSI(lpszText) : DEF_MAIN_NAME);
		} else if( strValue == "MainKbps" ) {
			m_nMainKbps = ((lpszText != NULL && strlen(lpszText) > 0 ) ? atoi(lpszText) : DEF_MAIN_KBPS);
		} else if( strValue == "SubKbps" ) {
			m_nSubKbps = ((lpszText != NULL && strlen(lpszText) > 0 ) ? atoi(lpszText) : DEF_SUB_KBPS);
		} else if( strValue == "SnapStep" ) {
			m_nSnapStep = ((lpszText != NULL && strlen(lpszText) > 0 ) ? atoi(lpszText) : DEF_SNAP_STEP);
		} else if( strValue == "RecSlice" ) {
			m_nRecSlice = ((lpszText != NULL && strlen(lpszText) > 0 ) ? atoi(lpszText) : DEF_REC_SLICE);
		} else if( strValue == "AutoLinkDVR" ) {
			m_bAutoLinkDVR = ((lpszText != NULL && strlen(lpszText) > 0 ) ? atoi(lpszText) : false);
		} else if( strValue == "AutoLinkFDFS" ) {
			m_bAutoLinkFDFS = ((lpszText != NULL && strlen(lpszText) > 0 ) ? atoi(lpszText) : false);
		} else if( strValue == "WebAddr" ) {
			m_strWebAddr = ((lpszText != NULL && strlen(lpszText) > 0 ) ? lpszText : DEF_WEB_ADDR);
		} else if( strValue == "WebPort" ) {
			m_nWebPort = ((lpszText != NULL && strlen(lpszText) > 0 ) ? atoi(lpszText) : DEF_WEB_PORT);
		} else if( strValue == "MaxCamera" ) {
			m_nMaxCamera = ((lpszText != NULL && strlen(lpszText) > 0 ) ? atoi(lpszText) : DEF_MAX_CAMERA);
		}
		lpChildElem = lpChildElem->NextSiblingElement();
	}
	// ���ţ���ȡƵ�����ýڵ���Ϣ...
	TiXmlElement * lpCameraElem = ((lpTrackElem != NULL) ? lpTrackElem->FirstChildElement() : NULL);
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
	}
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
	TiXmlElement	trackElem("Track");
	TiXmlElement	aboutElem("About");
	TiXmlElement	theElem("None");
	// �����ļ�ͷ...
	theDoc.Parse(XML_DECLARE_UTF8);
	// ���泣�����ýڵ���Ϣ...
	if( m_strSavePath.size() > 0 ) {
		theElem = this->BuildXmlElem("SavePath", CUtilTool::ANSI_UTF8(m_strSavePath.c_str()));
		commElem.InsertEndChild(theElem);
	}
	// ���������ڱ�������...
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
	theElem = this->BuildXmlElem("WebAddr", m_strWebAddr);
	commElem.InsertEndChild(theElem);
	theElem = this->BuildXmlElem("WebPort", m_nWebPort);
	commElem.InsertEndChild(theElem);
	theElem = this->BuildXmlElem("MaxCamera", m_nMaxCamera);
	commElem.InsertEndChild(theElem);
	// ����������ýڵ���Ϣ...
	theElem = this->BuildXmlElem("CopyRight", CUtilTool::ANSI_UTF8(m_strCopyRight.c_str()));
	aboutElem.InsertEndChild(theElem);
	theElem = this->BuildXmlElem("Version", CUtilTool::ANSI_UTF8(m_strVersion.c_str()));
	aboutElem.InsertEndChild(theElem);
	theElem = this->BuildXmlElem("Phone", CUtilTool::ANSI_UTF8(m_strPhone.c_str()));
	aboutElem.InsertEndChild(theElem);
	theElem = this->BuildXmlElem("WebSite", CUtilTool::ANSI_UTF8(m_strWebSite.c_str()));
	aboutElem.InsertEndChild(theElem);
	theElem = this->BuildXmlElem("Address", CUtilTool::ANSI_UTF8(m_strAddress.c_str()));
	aboutElem.InsertEndChild(theElem);
	// ��ʼ������ͨ���б�...
	GM_MapNodeCamera::iterator itorData;
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
	}
	// ��Ͻڵ��б�...
	rootElem.InsertEndChild(commElem);
	rootElem.InsertEndChild(aboutElem);
	rootElem.InsertEndChild(trackElem);
	theDoc.InsertEndChild(rootElem);
	// ��󣬽��д��̴���...
	return theDoc.SaveFile(m_strXMLFile);
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
void CXmlConfig::SetCourse(int nCameraID, GM_MapCourse & inMapCourse)
{
	OSMutexLocker theLock(&m_MutexCourse);
	m_MapNodeCourse[nCameraID] = inMapCourse;
}
//
// �γ̱��Ƕ�̬�洢������浽����...
void CXmlConfig::GetCourse(int nCameraID, GM_MapCourse & outMapCourse)
{
	OSMutexLocker theLock(&m_MutexCourse);
	outMapCourse = m_MapNodeCourse[nCameraID];
}
//
// �γ̱��Ƕ�̬�洢������浽����...
GM_MapNodeCourse & CXmlConfig::GetNodeCourse()
{
	OSMutexLocker theLock(&m_MutexCourse);
	return m_MapNodeCourse;
}
//
// ͨ�����ݿ��Ż�ȡ���ر��...
void CXmlConfig::GetDBCameraID(int nDBCameraID, int & outLocalID)
{
	// ���ó�ʼֵ...
	outLocalID = -1;
	// ����ҵ��˶�Ӧ�ı�ţ�ֱ�Ӹ�ֵ������...
	if( m_MapDBCamera.find(nDBCameraID) != m_MapDBCamera.end() ) {
		outLocalID = m_MapDBCamera[nDBCameraID];
	}
}
//
// ����ɾ��ָ��ͨ���Ĳ���...
void CXmlConfig::doDelDVR(int nCameraID)
{
	// ɾ����Ӧ��¼��γ̼�¼...
	OSMutexLocker theLock(&m_MutexCourse);
	m_MapNodeCourse.erase(nCameraID);
	// ���ҵ���Ӧ�� DBCameraID��Ȼ��ɾ����¼...
	GM_MapData & theData = m_MapNodeCamera[nCameraID];
	if( theData.find("DBCameraID") != theData.end() ) {
		int nDBCameraID = atoi(theData["DBCameraID"].c_str());
		m_MapDBCamera.erase(nDBCameraID);
	}
	// ɾ����Ӧ�������ļ���¼...
	m_MapNodeCamera.erase(nCameraID);
	// ��󣬽�������̵�xml���õ���...
	this->GMSaveConfig();
}

#define DEF_PER_WAIT_MS		10	// ÿ�εȴ�������
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
		
		if(((DWORD)-1) != ResumeThread(pi.hThread))
        {
            BOOL bRet = false;
            DWORD dwCode = 0;
			int  nCount = 0;
			while( true ) {
	            bRet = GetExitCodeProcess(pi.hProcess, &dwCode);
				if( dwCode != STILL_ACTIVE )
					break;
				// �����X��֮����Ȼû�н�ͼ�ɹ����жϽ���...
				if( ++nCount >= DEF_MAX_WAIT_COUNT ) {
					TerminateProcess(pi.hProcess, 0);
					break;
				}
				// ���� X ����֮�󣬼����ȴ�...
				::Sleep(DEF_PER_WAIT_MS);
			}
        }
		// ��ʼ��ȡ��ͼ�������...
		//outString.clear();
		//this->ReadHolePipe(pstdout, outString);
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

/*void CXmlConfig::ReadHolePipe(HANDLE hStdOut, string & strPipe)
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
}*/
