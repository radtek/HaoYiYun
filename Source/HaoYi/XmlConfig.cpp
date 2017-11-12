
#include "StdAfx.h"
#include "UtilTool.h"
#include "XmlConfig.h"

CXmlConfig::CXmlConfig(void)
  : m_nMaxCamera(DEF_MAX_CAMERA)
  , m_nMainKbps(DEF_MAIN_KBPS)
  , m_nSubKbps(DEF_SUB_KBPS)
  , m_bAutoLinkFDFS(false)
  , m_bAutoLinkDVR(false)
  , m_strSavePath(DEF_REC_PATH)
  , m_strMainName(DEF_MAIN_NAME)
  , m_strWebAddr(DEF_WEB_ADDR)
  , m_nWebPort(DEF_WEB_PORT)
  , m_nDBHaoYiGatherID(-1)
  , m_strTrackerAddr("")
  , m_strRemoteAddr("")
  , m_nTrackerPort(0)
  , m_nDBGatherID(-1)
  , m_nRemotePort(0)
  , m_strWebName("")
  , m_strWebTag("")
  , m_nWebType(-1)
  , m_nSliceVal(0)
  , m_nInterVal(0)
{
	CString strVersion;
	strVersion.Format("V%s - Build %s", CUtilTool::GetServerVersion(), __DATE__);
	m_strVersion = strVersion;
	m_strCopyRight = "北京浩一科技有限公司 版权所有(C) 2017-2020";
	m_strPhone = "15010119735";
	m_strWebSite = "https://www.myhaoyi.com";	
	m_strAddress = "北京市海淀区北四环西路68号6层C16";
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
	// 先清空一些数据...
	m_MapNodeCamera.clear();

	// 得到xml配置文件的全路径...
	TCHAR	 szPath[MAX_PATH] = {0};
	CUtilTool::GetFilePath(szPath, XML_CONFIG);
	ASSERT( strlen(szPath) > 0 );
	m_strXMLFile = szPath;

	// 获取截图工具全路径...
	CUtilTool::GetFilePath(szPath, SNAP_TOOL_NAME);
	ASSERT( strlen(szPath) > 0 );
	m_strMPlayer = szPath;

	// 加载配置节点需要的变量...
	BOOL			bLoadOK = true;
	TiXmlDocument	theDoc;
	TiXmlElement  * lpRootElem = NULL;
	TiXmlElement  * lpCommElem = NULL;
	TiXmlElement  * lpAboutElem = NULL;
	TiXmlElement  * lpChildElem = NULL;
	// 开始加载配置节点...
	do {
		// 加载配置文件失败...
		if( !theDoc.LoadFile(m_strXMLFile) ) {
			bLoadOK = false;
			break;
		}
		// 无法读取根节点...
		lpRootElem = theDoc.RootElement();
		if( lpRootElem == NULL ) {
			bLoadOK = false;
			break;
		}
		// 读取关于节点/公共节点/监控节点...
		lpCommElem = lpRootElem->FirstChildElement("Common");
		lpAboutElem = lpRootElem->FirstChildElement("About");
		// 没有关于节点或没有公共节点，重新构建...
		if( lpCommElem == NULL || lpAboutElem == NULL ) {
			bLoadOK = false;
			break;
		}
		// 加载配置文件成功...
		bLoadOK = true;
	}while( false );
	// 如果加载失败，保存默认的配置信息...
	if( !bLoadOK ) {
		//加入注册表
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
		// 保存默认的配置数据...
		return this->GMSaveConfig();
	}
	ASSERT( bLoadOK && lpAboutElem != NULL && lpRootElem != NULL && lpCommElem != NULL );
	// 首先，读取关于配置节点信息...
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
	// 然后，读取常规配置节点信息...
	lpChildElem = lpCommElem->FirstChildElement();
	while( lpChildElem != NULL ) {
		LPCTSTR	lpszText = lpChildElem->GetText();
		const string & strValue = lpChildElem->ValueStr();
		if( strValue == "SavePath" ) {
			m_strSavePath = ((lpszText != NULL && strlen(lpszText) > 0 ) ? CUtilTool::UTF8_ANSI(lpszText) : DEF_REC_PATH);
		} else if( strValue == "WebAddr" ) {
			m_strWebAddr = ((lpszText != NULL && strlen(lpszText) > 0 ) ? lpszText : DEF_WEB_ADDR);
		} else if( strValue == "WebPort" ) {
			m_nWebPort = ((lpszText != NULL && strlen(lpszText) > 0 ) ? atoi(lpszText) : DEF_WEB_PORT);
		}/* else if( strValue == "MainName" ) {
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
	// 2017.10.27 - by jackey => 通道配置，全部放置到网站端...
	// 接着，读取频道配置节点信息...
	/*TiXmlElement * lpCameraElem = ((lpTrackElem != NULL) ? lpTrackElem->FirstChildElement() : NULL);
	while( lpCameraElem != NULL ) {
		TiXmlElement * lpElemID = lpCameraElem->FirstChildElement("ID");
		if( lpElemID == NULL ) {
			lpCameraElem = lpCameraElem->NextSiblingElement();
			continue;
		}
		// 判断节点编号是否正确，必须大于0...
		ASSERT( lpElemID != NULL );
		LPCTSTR	lpszValue = NULL;
		LPCTSTR lpszText = lpElemID->GetText();
		int nCameraID = ((lpszText != NULL && strlen(lpszText) > 0) ? atoi(lpszText) : 0);
		if( nCameraID <= 0 ) {
			lpCameraElem = lpCameraElem->NextSiblingElement();
			continue;
		}
		ASSERT( nCameraID > 0 );
		// 读取节点属性，创建新的监控节点...
		GM_MapData    theData;
		lpChildElem = lpCameraElem->FirstChildElement();
		while( lpChildElem != NULL ) {
			lpszValue = lpChildElem->Value();
			lpszText = lpChildElem->GetText();
			// 保存节点属性的名称和数据...
			if( lpszValue != NULL && lpszText != NULL ) {
				theData[lpszValue] = CUtilTool::UTF8_ANSI(lpszText);
			}
			// 下一个子节点属性...
			lpChildElem = lpChildElem->NextSiblingElement();
		}
		// 保存配置，下一个监控通道节点...
		m_MapNodeCamera[nCameraID] = theData;
		lpCameraElem = lpCameraElem->NextSiblingElement();
	}*/
	return true;
}

BOOL CXmlConfig::GMSaveConfig()
{
	if( m_strXMLFile.size() <= 0 )
		return false;
	// 定义一些需要的变量...
	TiXmlDocument	theDoc;
	TiXmlElement	rootElem("Config");
	TiXmlElement	commElem("Common");
	TiXmlElement	aboutElem("About");
	TiXmlElement	theElem("None");
	// 构造文件头...
	theDoc.Parse(XML_DECLARE_UTF8);
	// 保存常规配置节点信息...
	if( m_strSavePath.size() > 0 ) {
		theElem = this->BuildXmlElem("SavePath", CUtilTool::ANSI_UTF8(m_strSavePath.c_str()));
		commElem.InsertEndChild(theElem);
	}
	// 只保存网站地址和端口，其它存放到内存和数据库当中...
	theElem = this->BuildXmlElem("WebAddr", m_strWebAddr);
	commElem.InsertEndChild(theElem);
	theElem = this->BuildXmlElem("WebPort", m_nWebPort);
	commElem.InsertEndChild(theElem);
	/*// 更行主窗口标题名称...
	theElem = this->BuildXmlElem("MainName", CUtilTool::ANSI_UTF8(m_strMainName.c_str()));
	commElem.InsertEndChild(theElem);
	// 更新主码流和子码流配置信息...
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
	// 保存关于配置节点信息...
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
	// 2017.10.27 - by jackey => 通道配置全部放置到网站端，不存盘到本地...
	// 开始保存监控通道列表...
	/*GM_MapNodeCamera::iterator itorData;
	for(itorData = m_MapNodeCamera.begin(); itorData != m_MapNodeCamera.end(); ++itorData) {
		TiXmlElement theCamera("Camera");
		GM_MapData & theData = itorData->second;
		// 为了保证 ID 和 Name 在前面显示，这个时候才开始转换成UTF8格式...
		TiXmlElement theID = this->BuildXmlElem("ID", theData["ID"]);
		TiXmlElement theName = this->BuildXmlElem("Name", CUtilTool::ANSI_UTF8(theData["Name"].c_str()));
		theCamera.InsertEndChild(theID);
		theCamera.InsertEndChild(theName);

		GM_MapData::iterator itorItem;
		for(itorItem = theData.begin(); itorItem != theData.end(); ++itorItem) {
			// ID 和 Name 已经存盘，DBCameraID 不存盘，每次从数据库获取...
			if( itorItem->first == "ID" || itorItem->first == "Name" || itorItem->first == "DBCameraID" )
				continue;
			TiXmlElement theNode = this->BuildXmlElem(itorItem->first, CUtilTool::ANSI_UTF8(itorItem->second.c_str()));
			theCamera.InsertEndChild(theNode);
		}

		// 将监控节点加入到主节点当中...
		trackElem.InsertEndChild(theCamera);
	}*/
	// 组合节点列表...
	rootElem.InsertEndChild(commElem);
	rootElem.InsertEndChild(aboutElem);
	theDoc.InsertEndChild(rootElem);
	// 最后，进行存盘处理...
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
// 课程表是动态存储，不会存到本地...
void CXmlConfig::SetCourse(int nDBCameraID, GM_MapCourse & inMapCourse)
{
	OSMutexLocker theLock(&m_MutexCourse);
	m_MapNodeCourse[nDBCameraID] = inMapCourse;
}
//
// 课程表是动态存储，不会存到本地...
void CXmlConfig::GetCourse(int nDBCameraID, GM_MapCourse & outMapCourse)
{
	OSMutexLocker theLock(&m_MutexCourse);
	outMapCourse = m_MapNodeCourse[nDBCameraID];
}
//
// 课程表是动态存储，不会存到本地...
GM_MapNodeCourse & CXmlConfig::GetNodeCourse()
{
	OSMutexLocker theLock(&m_MutexCourse);
	return m_MapNodeCourse;
}
//
// 通过通道编号获取通道名称...
CString CXmlConfig::GetDBCameraTitle(int nDBCameraID)
{
	CString strTitle;
	GM_MapNodeCamera::iterator itorItem = m_MapNodeCamera.find(nDBCameraID);
	if( itorItem != m_MapNodeCamera.end() ) {
		GM_MapData & theMapWeb = itorItem->second;
		/*if( this->m_nWebType != kCloudMonitor ) {
			strTitle.Format("%s %s %s", theMapWeb["grade_type"].c_str(), theMapWeb["grade_name"].c_str(), theMapWeb["camera_name"].c_str());
		} else {
			strTitle = theMapWeb["camera_name"].c_str();
		}*/
		strTitle = theMapWeb["camera_name"].c_str();
	}
	return strTitle;
}
//
// 发起删除指定通道的操作...
void CXmlConfig::doDelDVR(int nDBCameraID)
{
	// 删除对应的录像课程记录...
	OSMutexLocker theLock(&m_MutexCourse);
	m_MapNodeCourse.erase(nDBCameraID);
	// 删除对应的配置文件记录...
	m_MapNodeCamera.erase(nDBCameraID);
}

#define DEF_PER_WAIT_MS		50	// 每次等待毫秒数
#define DEF_MAX_WAIT_COUNT  50	// COUNT * MS = 总毫秒
#define DEF_SNAP_JPG_NAME   TEXT("00000001.jpg")
#define SafeCloseHandle(handle)	do{ if( handle ) { CloseHandle(handle); handle = 0L; } }while(0)

//
// 对外的流转发模式的截图接口函数...
BOOL CXmlConfig::StreamSnapJpeg(const CString & inSrcMP4File, const CString & inDestJpgName, int nRecSec)
{
	// 只截取 2 分钟以内的数据...
	int nSnapSize = ((nRecSec >= 120) ? 120 : nRecSec);
	int nSnapPoint = 0;
	if( nSnapSize > 5 ) {
		// 随机截取总时间的一半...
		srand((unsigned)time(NULL));
		nSnapPoint = (rand() % (nSnapSize/2))+3;
		// 永远不要操作总时间的一半，否则，截图工具会截取不到数据...
		nSnapPoint = min(nSnapPoint, (nSnapSize/2));
	}
	// 准备截图存放位置...
	CString strSrcJpgName;
	TCHAR szCurDir[MAX_PATH] = {0};
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	if( ::GetCurrentDirectory(MAX_PATH, szCurDir) ) {
		strSrcJpgName.Format("%s\\%s", szCurDir, DEF_SNAP_JPG_NAME);
	} else {
		strSrcJpgName = DEF_SNAP_JPG_NAME;
	}
	// 只能用 ANSI 字符格式截图...
	this->GenerateJpegFromMediaFile(inSrcMP4File, nSnapPoint, 1);
	do {
		// 如果截图已经存在，直接跳出...
		if( _access(strSrcJpgName, 0) >= 0 )
			break;
		// 截图失败，如果截图时间点是0点，直接跳出...
		if( nSnapPoint <= 0 )
			break;
		// 尝试使用 0 点进行截图...
		this->GenerateJpegFromMediaFile(inSrcMP4File, 0, 1);
	}while( false );
	// 再次判断截图文件是否存在，不存在直接返回...
	if( _access(strSrcJpgName, 0) < 0 )
		return false;
	// 将截图文件拷贝到上传目录当中 => 覆盖模式...
	::CopyFile(strSrcJpgName, inDestJpgName, false);
	// 截图完成，删除原始截图文件...
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

		// 构造命令行...
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
		
		// 2017.08.22 - by jackey => 等待时间增加到50*50毫秒...
		if(((DWORD)-1) != ResumeThread(pi.hThread))
        {
            BOOL bRet = false;
            DWORD dwCode = 0;
			int  nCount = 0;
			while( true ) {
	            bRet = GetExitCodeProcess(pi.hProcess, &dwCode);
				if( dwCode != STILL_ACTIVE )
					break;
				// 如果在X毫秒之后，仍然没有截图成功，中断进程...
				if( ++nCount >= DEF_MAX_WAIT_COUNT ) {
					TerminateProcess(pi.hProcess, 0);
					break;
				}
				// 休整 X 毫秒之后，继续等待...
				::Sleep(DEF_PER_WAIT_MS);
			}
        }
		// 开始读取截图输出数据 => TRACE 有长度限制...
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
