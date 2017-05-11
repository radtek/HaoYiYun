
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
{
	CString strVersion;
	strVersion.Format("版 本：V%s - Build %s", CUtilTool::GetServerVersion(), __DATE__);
	m_strVersion = strVersion;
	m_strCopyRight = "北京世纪葵花数字传媒技术有限公司 版权所有(C) 2002-2017";
	m_strPhone = "电 话：400-050-1080";
	m_strWebSite = "网 站：http://www.kuihua.net";	
	m_strAddress = "地 址：北京市昌平区北清路1号珠江摩尔国际大厦3号楼2单元313";
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
	// 加载配置节点需要的变量...
	BOOL			bLoadOK = true;
	TiXmlDocument	theDoc;
	TiXmlElement  * lpRootElem = NULL;
	TiXmlElement  * lpTrackElem = NULL;
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
		lpTrackElem = lpRootElem->FirstChildElement("Track");
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
	// 接着，读取频道配置节点信息...
	TiXmlElement * lpCameraElem = ((lpTrackElem != NULL) ? lpTrackElem->FirstChildElement() : NULL);
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
	}
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
	TiXmlElement	trackElem("Track");
	TiXmlElement	aboutElem("About");
	TiXmlElement	theElem("None");
	// 构造文件头...
	theDoc.Parse(XML_DECLARE_UTF8);
	// 保存常规配置节点信息...
	if( m_strSavePath.size() > 0 ) {
		theElem = this->BuildXmlElem("SavePath", CUtilTool::ANSI_UTF8(m_strSavePath.c_str()));
		commElem.InsertEndChild(theElem);
	}
	// 更行主窗口标题名称...
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
	theElem = this->BuildXmlElem("WebAddr", m_strWebAddr);
	commElem.InsertEndChild(theElem);
	theElem = this->BuildXmlElem("WebPort", m_nWebPort);
	commElem.InsertEndChild(theElem);
	theElem = this->BuildXmlElem("MaxCamera", m_nMaxCamera);
	commElem.InsertEndChild(theElem);
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
	// 开始保存监控通道列表...
	GM_MapNodeCamera::iterator itorData;
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
	}
	// 组合节点列表...
	rootElem.InsertEndChild(commElem);
	rootElem.InsertEndChild(aboutElem);
	rootElem.InsertEndChild(trackElem);
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
void CXmlConfig::SetCourse(int nCameraID, GM_MapCourse & inMapCourse)
{
	OSMutexLocker theLock(&m_MutexCourse);
	m_MapNodeCourse[nCameraID] = inMapCourse;
}
//
// 课程表是动态存储，不会存到本地...
void CXmlConfig::GetCourse(int nCameraID, GM_MapCourse & outMapCourse)
{
	OSMutexLocker theLock(&m_MutexCourse);
	outMapCourse = m_MapNodeCourse[nCameraID];
}
//
// 课程表是动态存储，不会存到本地...
GM_MapNodeCourse & CXmlConfig::GetNodeCourse()
{
	OSMutexLocker theLock(&m_MutexCourse);
	return m_MapNodeCourse;
}
//
// 通过数据库编号获取本地编号...
void CXmlConfig::GetDBCameraID(int nDBCameraID, int & outLocalID)
{
	// 设置初始值...
	outLocalID = -1;
	// 如果找到了对应的编号，直接赋值，返回...
	if( m_MapDBCamera.find(nDBCameraID) != m_MapDBCamera.end() ) {
		outLocalID = m_MapDBCamera[nDBCameraID];
	}
}