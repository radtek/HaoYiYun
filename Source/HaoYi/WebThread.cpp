
#include "stdafx.h"
#include "HaoYiView.h"
#include "WebThread.h"
#include "SocketUtils.h"
#include "StringParser.h"
#include "XmlConfig.h"
#include "UtilTool.h"
#include "tinyxml.h"
#include <curl.h>

CWebThread::CWebThread(CHaoYiView * lpView)
  : m_eRegState(kRegHaoYi)
  , m_lpHaoYiView(lpView)
  , m_strDBCameraName("")
  , m_nDBCameraID(-1)
  , m_nDBGatherID(-1)
  , m_nMyHaoYiID(-1)
  , m_nRemotePort(0)
  , m_nTrackerPort(0)
  , m_strRemoteAddr("")
  , m_strTrackerAddr("")
  , m_nCurCameraCount(0)
{
	ASSERT( m_lpHaoYiView != NULL );
}

CWebThread::~CWebThread()
{
	// 停止线程，等待退出...
	this->StopAndWaitForThread();
}

GM_Error CWebThread::Initialize()
{
	GM_Error theErr = GM_NoErr;

	// 启动组播接收线程...
	this->Start();
	return theErr;
}

size_t procPostCurl(void *ptr, size_t size, size_t nmemb, void *stream)
{
	CWebThread * lpThread = (CWebThread*)stream;
	lpThread->doPostCurl((char*)ptr, size * nmemb);
	return size * nmemb;
}
//
// 返回一个json数据包...
void CWebThread::doPostCurl(char * pData, size_t nSize)
{
	// 准备解析需要的变量...
	string strUTF8Data;
	string strANSIData;
	Json::Reader reader;
	Json::Value  value;
	// 将UTF8网站数据转换成ANSI格式 => 由于是php编码过的，转换后无效，获取具体数据之后，还要转换一遍...
	strUTF8Data.assign(pData, nSize);
	strANSIData = CUtilTool::UTF8_ANSI(strUTF8Data.c_str());
	// 解析转换后的json数据包...
	if( !reader.parse(strANSIData, value) ) {
		MsgLogGM(GM_Err_Json);
		return;
	}
	// 获取返回的采集端编号和错误信息...
	if( value["err_code"].asBool() ) {
		string & strData = value["err_msg"].asString();
		string & strMsg = CUtilTool::UTF8_ANSI(strData.c_str());
		MsgLogINFO(strMsg.c_str());
		return;
	}
	// 获取有效的反馈数据信息...
	if( m_eRegState == kRegHaoYi ) {
		// 正在处理验证许可过程...
		Json::Value & theID = value["gather_id"];
		if( theID.isInt() ) {
			m_nMyHaoYiID = theID.asInt();
		} else if( theID.isString() ) {
			m_nMyHaoYiID = atoi(theID.asString().c_str());
		}
	} else if( m_eRegState == kRegGather ) {
		// 正在处理注册采集端过程...
		Json::Value & theID = value["gather_id"];
		if( theID.isInt() ) {
			m_nDBGatherID = theID.asInt();
		} else if( theID.isString() ) {
			m_nDBGatherID = atoi(theID.asString().c_str());
		}
		// 获取Tracker|Remote|Local，并存放到配置文件，但不存盘...
		Json::Value & theRemoteAddr = value["transmit_addr"];
		Json::Value & theRemotePort = value["transmit_port"];
		Json::Value & theTrackerAddr = value["tracker_addr"];
		Json::Value & theTrackerPort = value["tracker_port"];
		Json::Value & theLocalTime   = value["local_time"];
		if( theTrackerAddr.isString() ) {
			m_strTrackerAddr = theTrackerAddr.asString();
		}
		if( theTrackerPort.isString() ) {
			m_nTrackerPort = atoi(theTrackerPort.asString().c_str());
		}
		if( theRemoteAddr.isString() ) {
			m_strRemoteAddr = theRemoteAddr.asString();
		}
		if( theRemotePort.isString() ) {
			m_nRemotePort = atoi(theRemotePort.asString().c_str());
		}
		// 同步网站服务器时钟...
		if( theLocalTime.isString() ) {
			COleDateTime theDate;
			SYSTEMTIME   theST = {0};
			string strLocalTime = theLocalTime.asString();
			// 解析正确，并且得到系统时间正确，才进行设置...
			if( theDate.ParseDateTime(strLocalTime.c_str()) && theDate.GetAsSystemTime(theST) ) {
				::SetLocalTime(&theST);
			}
		}
	} else if( m_eRegState == kRegCamera ) {
		// 正在处理注册摄像头过程...
		Json::Value & theID = value["camera_id"];
		if( theID.isInt() ) {
			m_nDBCameraID = theID.asInt();
		} else if( theID.isString() ) {
			m_nDBCameraID = atoi(theID.asString().c_str());
		}
		// 获取通道名称...
		Json::Value & theCameraName = value["camera_name"];
		if( theCameraName.isString() ) {
			string & strData = theCameraName.asString();
			m_strDBCameraName = CUtilTool::UTF8_ANSI(strData.c_str());
		}
		// 获取录像课程表...
		Json::Value & theCourse = value["course"];
		if( theCourse.isArray() ) {
			for(int i = 0; i < theCourse.size(); ++i) {
				int     nCourseID;
				string  strValue;
				GM_MapData theMapData;
				strValue = CUtilTool::getJsonString(theCourse[i]["course_id"]);
				theMapData["course_id"] = strValue;
				nCourseID = atoi(strValue.c_str());
				strValue = CUtilTool::getJsonString(theCourse[i]["camera_id"]);
				theMapData["camera_id"] = strValue;
				strValue = CUtilTool::getJsonString(theCourse[i]["subject_id"]);
				theMapData["subject_id"] = strValue;
				strValue = CUtilTool::getJsonString(theCourse[i]["teacher_id"]);
				theMapData["teacher_id"] = strValue;
				strValue = CUtilTool::getJsonString(theCourse[i]["repeat_id"]);
				theMapData["repeat_id"] = strValue;
				strValue = CUtilTool::getJsonString(theCourse[i]["elapse_sec"]);
				theMapData["elapse_sec"] = strValue;
				strValue = CUtilTool::getJsonString(theCourse[i]["start_time"]);
				theMapData["start_time"] = strValue;
				strValue = CUtilTool::getJsonString(theCourse[i]["end_time"]);
				theMapData["end_time"] = strValue;
				m_dbMapCourse[nCourseID] = theMapData;
			}
		}
	} else if( m_eRegState == kDelCamera ) {
		// 获取返回的已删除的摄像头在数据库中的编号...
		Json::Value & theID = value["camera_id"];
		if( theID.isInt() ) {
			m_nDBCameraID = theID.asInt();
		} else if( theID.isString() ) {
			m_nDBCameraID = atoi(theID.asString().c_str());
		}
	}
}
//
// 验证许可证...
BOOL CWebThread::RegisterHaoYi()
{
	// 先设置当前状态信息...
	m_eRegState = kRegHaoYi;
	// 判断数据是否有效...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	CString & strMacAddr = m_lpHaoYiView->m_strMacAddr;
	CString & strIPAddr = m_lpHaoYiView->m_strIPAddr;
	if( strMacAddr.GetLength() <= 0 || strIPAddr.GetLength() <= 0 )
		return false;
	// 准备需要的汇报数据 => POST数据包...
	CString strPost, strUrl;
	TCHAR	szDNS[MAX_PATH] = {0};
	// 先对频道名称进行UTF8转换，再进行URI编码...
	string  strDNSName = CUtilTool::GetServerDNSName();
	string  strUTF8Name = CUtilTool::ANSI_UTF8(strDNSName.c_str());
	StringParser::EncodeURI(strUTF8Name.c_str(), strUTF8Name.size(), szDNS, MAX_PATH);
	strPost.Format("mac_addr=%s&ip_addr=%s&max_camera=%d&name_pc=%s&version=%s", 
					strMacAddr, strIPAddr, theConfig.GetMaxCamera(),
					szDNS, _T(SZ_VERSION_NAME));
	strUrl.Format("http://%s/wxapi.php/Gather/verify", "www.myhaoyi.com");
	// 调用Curl接口，汇报采集端信息...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if( curl == NULL )
			break;
		// 设定curl参数，采用post模式，设置5秒超时...
		res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strPost);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strPost.GetLength());
		res = curl_easy_setopt(curl, CURLOPT_HEADER, false);
		res = curl_easy_setopt(curl, CURLOPT_POST, true);
		res = curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
		res = curl_easy_setopt(curl, CURLOPT_URL, strUrl);
		res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, procPostCurl);
		res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this);
		res = curl_easy_perform(curl);
	}while( false );
	// 释放资源...
	if( curl != NULL ) {
		curl_easy_cleanup(curl);
	}
	// 通知主窗口授权过期验证结果...
	m_lpHaoYiView->PostMessage(WM_WEB_AUTH_RESULT, kAuthExpired, ((m_nMyHaoYiID > 0) ? true : false));
	// 返回授权验证结果...
	return ((m_nMyHaoYiID > 0) ? true : false);
}
//
// 在网站上注册采集端...
BOOL CWebThread::RegisterGather()
{
	// 先设置当前状态信息...
	m_eRegState = kRegGather;
	// 判断数据是否有效...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	int nWebPort = theConfig.GetWebPort();
	string  & strWebAddr = theConfig.GetWebAddr();
	CString & strMacAddr = m_lpHaoYiView->m_strMacAddr;
	CString & strIPAddr = m_lpHaoYiView->m_strIPAddr;
	if( strWebAddr.size() <= 0 || nWebPort <= 0 )
		return false;
	if( strMacAddr.GetLength() <= 0 || strIPAddr.GetLength() <= 0 )
		return false;
	// 准备需要的汇报数据 => POST数据包...
	CString strPost, strUrl;
	TCHAR	szDNS[MAX_PATH] = {0};
	// 先对频道名称进行UTF8转换，再进行URI编码...
	string  strDNSName = CUtilTool::GetServerDNSName();
	string  strUTF8Name = CUtilTool::ANSI_UTF8(strDNSName.c_str());
	StringParser::EncodeURI(strUTF8Name.c_str(), strUTF8Name.size(), szDNS, MAX_PATH);
	strPost.Format("mac_addr=%s&ip_addr=%s&max_camera=%d&name_pc=%s", 
					strMacAddr, strIPAddr, theConfig.GetMaxCamera(), szDNS);
	strUrl.Format("http://%s:%d/wxapi.php/Gather/index", strWebAddr.c_str(), nWebPort);
	// 调用Curl接口，汇报采集端信息...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if( curl == NULL )
			break;
		// 设定curl参数，采用post模式，设置5秒超时...
		res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strPost);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strPost.GetLength());
		res = curl_easy_setopt(curl, CURLOPT_HEADER, false);
		res = curl_easy_setopt(curl, CURLOPT_POST, true);
		res = curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
		res = curl_easy_setopt(curl, CURLOPT_URL, strUrl);
		res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, procPostCurl);
		res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this);
		res = curl_easy_perform(curl);
	}while( false );
	// 释放资源...
	if( curl != NULL ) {
		curl_easy_cleanup(curl);
	}
	// 通知主窗口授权网站注册结果...
	m_lpHaoYiView->PostMessage(WM_WEB_AUTH_RESULT, kAuthRegiter, ((m_nDBGatherID > 0) ? true : false));
	// 判断采集端是否注册成功...
	if( m_nDBGatherID <= 0 )
		return false;
	ASSERT( m_nDBGatherID > 0 );
	// 判断Tracker地址是否已经正确获取得到...
	if( m_strTrackerAddr.size() <= 0 || m_nTrackerPort <= 0 )
		return false;
	if( m_strRemoteAddr.size() <= 0 || m_nRemotePort <= 0 )
		return false;
	// 存放到配置文件，但并不存盘...
	theConfig.SetRemoteAddr(m_strRemoteAddr);
	theConfig.SetRemotePort(m_nRemotePort);
	theConfig.SetTrackerAddr(m_strTrackerAddr);
	theConfig.SetTrackerPort(m_nTrackerPort);
	return true;
}
//
// 采集端退出汇报...
BOOL CWebThread::doWebGatherLogout()
{
	// 获取网站配置信息...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	int nWebPort = theConfig.GetWebPort();
	string & strWebAddr = theConfig.GetWebAddr();
	if( m_nDBGatherID <= 0 || nWebPort <= 0 || strWebAddr.size() <= 0 )
		return false;
	// 先设置当前状态信息...
	m_eRegState = kGatherLogout;
	// 准备需要的汇报数据 => POST数据包...
	CString strPost, strUrl;
	strPost.Format("gather_id=%d", m_nDBGatherID);
	// 组合访问链接地址...
	strUrl.Format("http://%s:%d/wxapi.php/Gather/logout", strWebAddr.c_str(), nWebPort);
	// 调用Curl接口，汇报摄像头数据...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if( curl == NULL )
			break;
		// 设定curl参数，采用post模式...
		res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strPost);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strPost.GetLength());
		res = curl_easy_setopt(curl, CURLOPT_HEADER, false);
		res = curl_easy_setopt(curl, CURLOPT_POST, true);
		res = curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
		res = curl_easy_setopt(curl, CURLOPT_URL, strUrl);
		// 这里不需要网站返回的数据...
		//res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, procPostCurl);
		//res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this);
		res = curl_easy_perform(curl);
	}while( false );
	// 释放资源...
	if( curl != NULL ) {
		curl_easy_cleanup(curl);
	}
	return true;
}
//
// 在网站上注册摄像头...
BOOL CWebThread::RegisterCamera()
{
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	GM_MapNodeCamera & theMapCamera = theConfig.GetNodeCamera();
	GM_MapNodeCamera::iterator itorItem = theMapCamera.begin();
	while( itorItem != theMapCamera.end() ) {
		// 已注册摄像头数目不够，进行网站注册操作...
		GM_MapData & theData = itorItem->second;
		if( !this->doWebRegCamera(theData) ) {
			// 注册摄像头失败，删除摄像头配置...
			theMapCamera.erase(itorItem++);
		} else {
			// 注册摄像头成功，累加配置...
			++itorItem;
		}
	}
	return true;
}
//
// 在网站上具体执行注册或更新摄像头操作...
BOOL CWebThread::doWebRegCamera(GM_MapData & inData)
{
	// 输入数据中必须包含ID字段...
	GM_MapData::iterator itorID, itorProp;
	itorProp = inData.find("StreamProp");
	itorID = inData.find("ID");
	if( itorID == inData.end() || itorProp == inData.end() )
		return false;
	// 获取摄像头编号，用于存放和定位课表内容...
	int nStreamProp = atoi(itorProp->second.c_str());
	int nCameraID = atoi(itorID->second.c_str());
	if( nCameraID <= 0 )
		return false;
	ASSERT( nCameraID > 0 );
	// 获取网站配置信息...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	int nWebPort = theConfig.GetWebPort();
	string & strWebAddr = theConfig.GetWebAddr();
	if( nWebPort <= 0 || strWebAddr.size() <= 0 )
		return false;
	// 如果当前已注册摄像头数目超过了最大支持数，不用再注册...
	if( m_nCurCameraCount >= theConfig.GetMaxCamera() )
		return false;
	// 初始化数据库里的摄像头编号...
	m_dbMapCourse.clear();
	m_strDBCameraName = "";
	m_nDBCameraID = -1;
	// 先设置当前状态信息...
	m_eRegState = kRegCamera;
	// 准备需要的汇报数据 => POST数据包...
	CString strPost, strUrl;
	TCHAR	szEncName[MAX_PATH] = {0};
	// 先对频道名称进行UTF8转换，再进行URI编码...
	string  strUTF8Name = CUtilTool::ANSI_UTF8(inData["Name"].c_str());
	StringParser::EncodeURI(strUTF8Name.c_str(), strUTF8Name.size(), szEncName, MAX_PATH);
	if( nStreamProp == kStreamDevice ) {
		// 处理通道是摄像头的情况，设置默认的状态为0(离线)...
		strPost.Format("gather_id=%d&stream_prop=%d&camera_type=%s&camera_name=%s&device_sn=%s&device_ip=%s&device_mac=%s&device_type=%s&status=0",
			m_nDBGatherID, nStreamProp, inData["CameraType"].c_str(), szEncName, inData["DeviceSN"].c_str(), 
			inData["IPv4Address"].c_str(), inData["MAC"].c_str(), inData["DeviceType"].c_str());
	} else {
		// 对需要的数据进行编码处理 => 这里需要注意文件名过长时的内存溢出问题...
		TCHAR  szMP4File[MAX_PATH * 3] = {0};
		TCHAR  szUrlLink[MAX_PATH * 2] = {0};
		string strUTF8MP4 = CUtilTool::ANSI_UTF8(inData["StreamMP4"].c_str());
		string strUTF8Url = CUtilTool::ANSI_UTF8(inData["StreamUrl"].c_str());
		StringParser::EncodeURI(strUTF8MP4.c_str(), strUTF8MP4.size(), szMP4File, MAX_PATH * 3);
		StringParser::EncodeURI(strUTF8Url.c_str(), strUTF8Url.size(), szUrlLink, MAX_PATH * 2);
		// 处理通道是流转发的情况...
		ASSERT( nStreamProp == kStreamMP4File || nStreamProp == kStreamUrlLink );
		strPost.Format("gather_id=%d&stream_prop=%d&camera_type=%s&camera_name=%s&device_sn=%s&stream_mp4=%s&stream_url=%s&status=0",
			m_nDBGatherID, nStreamProp, inData["CameraType"].c_str(), szEncName, inData["DeviceSN"].c_str(), szMP4File, szUrlLink);
	}
	// 组合访问链接地址...
	strUrl.Format("http://%s:%d/wxapi.php/Gather/camera", strWebAddr.c_str(), nWebPort);
	// 调用Curl接口，汇报摄像头数据...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if( curl == NULL )
			break;
		// 设定curl参数，采用post模式...
		res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strPost);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strPost.GetLength());
		res = curl_easy_setopt(curl, CURLOPT_HEADER, false);
		res = curl_easy_setopt(curl, CURLOPT_POST, true);
		res = curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
		res = curl_easy_setopt(curl, CURLOPT_URL, strUrl);
		res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, procPostCurl);
		res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this);
		res = curl_easy_perform(curl);
	}while( false );
	// 释放资源...
	if( curl != NULL ) {
		curl_easy_cleanup(curl);
	}
	// 判断摄像头是否注册成功...
	if( m_nDBCameraID <= 0 )
		return false;
	ASSERT( m_nDBCameraID > 0 );
	// 将数据库中记录编号更新到摄像头配置当中，但不存入Config.xml当中...
	TCHAR szDBCamera[32] = {0};
	sprintf(szDBCamera, "%d", m_nDBCameraID);
	inData["DBCameraID"] = szDBCamera;
	// 保存数据库中通道名称...
	if( m_strDBCameraName.size() > 0 ) {
		inData["Name"] = m_strDBCameraName;
	}
	// 将数据库编号与本地的对应关系存放到集合当中...
	theConfig.SetDBCameraID(m_nDBCameraID, nCameraID);
	// 将获取得到的录像课程表存放起来，直接覆盖原来的记录，用本地编号定位...
	// 录像课程表都是记录到内存当中，不存入Config.xml当中...
	theConfig.SetCourse(nCameraID, m_dbMapCourse);
	// 注册摄像头成功，摄像头累加计数...
	++m_nCurCameraCount;
	return true;
}
//
// 向网站删除摄像头...
BOOL CWebThread::doWebDelCamera(string & inDeviceSN)
{
	// 获取网站配置信息...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	int nWebPort = theConfig.GetWebPort();
	string & strWebAddr = theConfig.GetWebAddr();
	if( nWebPort <= 0 || strWebAddr.size() <= 0 )
		return false;
	// 先设置当前状态信息...
	m_nDBCameraID = -1;
	m_eRegState = kDelCamera;
	// 准备需要的汇报数据 => POST数据包...
	CString strPost, strUrl;
	strPost.Format("device_sn=%s", inDeviceSN.c_str());
	// 组合访问链接地址...
	strUrl.Format("http://%s:%d/wxapi.php/Gather/delCamera", strWebAddr.c_str(), nWebPort);
	// 调用Curl接口，汇报摄像头数据...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if( curl == NULL )
			break;
		// 设定curl参数，采用post模式...
		res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strPost);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strPost.GetLength());
		res = curl_easy_setopt(curl, CURLOPT_HEADER, false);
		res = curl_easy_setopt(curl, CURLOPT_POST, true);
		res = curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
		res = curl_easy_setopt(curl, CURLOPT_URL, strUrl);
		res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, procPostCurl);
		res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this);
		res = curl_easy_perform(curl);
	}while( false );
	// 释放资源...
	if( curl != NULL ) {
		curl_easy_cleanup(curl);
	}
	// 判断摄像头是否删除成功...
	if( m_nDBCameraID <= 0 )
		return false;
	ASSERT( m_nDBCameraID > 0 );
	// 摄像头计数器减少...
	m_nCurCameraCount -= 1;
	return true;
}
//
// 向网站汇报通道的运行状态 => 0(等待) 1(运行) 2(录像)...
BOOL CWebThread::doWebStatCamera(int nDBCamera, int nStatus)
{
	// 获取网站配置信息...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	int nWebPort = theConfig.GetWebPort();
	string & strWebAddr = theConfig.GetWebAddr();
	if( nWebPort <= 0 || strWebAddr.size() <= 0 )
		return false;
	// 先设置当前状态信息...
	m_eRegState = kStatCamera;
	// 准备需要的汇报数据 => POST数据包...
	CString strPost, strUrl;
	strPost.Format("camera_id=%d&status=%d", nDBCamera, nStatus);
	// 组合访问链接地址...
	strUrl.Format("http://%s:%d/wxapi.php/Gather/saveCamera", strWebAddr.c_str(), nWebPort);
	// 调用Curl接口，汇报摄像头数据...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if( curl == NULL )
			break;
		// 设定curl参数，采用post模式...
		res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strPost);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strPost.GetLength());
		res = curl_easy_setopt(curl, CURLOPT_HEADER, false);
		res = curl_easy_setopt(curl, CURLOPT_POST, true);
		res = curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
		res = curl_easy_setopt(curl, CURLOPT_URL, strUrl);
		// 这里不需要网站返回的数据...
		//res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, procPostCurl);
		//res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this);
		res = curl_easy_perform(curl);
	}while( false );
	// 释放资源...
	if( curl != NULL ) {
		curl_easy_cleanup(curl);
	}
	return true;
}
//
// 在网站上更新摄像头名称...
/*BOOL CWebThread::doSaveCameraName(string & strDBCameraID, CString & strCameraName)
{
	// 获取网站配置信息...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	int nWebPort = theConfig.GetWebPort();
	string & strWebAddr = theConfig.GetWebAddr();
	if( nWebPort <= 0 || strWebAddr.size() <= 0 )
		return false;
	// 先设置当前状态信息...
	m_eRegState = kSaveName;
	// 准备需要的汇报数据 => POST数据包...
	CString strPost, strUrl;
	TCHAR	szEncName[MAX_PATH] = {0};
	// 先对频道名称进行UTF8转换，再进行URI编码...
	string  strUTF8Name = CUtilTool::ANSI_UTF8(strCameraName);
	StringParser::EncodeURI(strUTF8Name.c_str(), strUTF8Name.size(), szEncName, MAX_PATH);
	strPost.Format("camera_id=%s&camera_name=%s", strDBCameraID.c_str(), szEncName);
	strUrl.Format("http://%s:%d/wxapi.php/Gather/saveCameraName", strWebAddr.c_str(), nWebPort);
	// 调用Curl接口，汇报摄像头数据...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if( curl == NULL )
			break;
		// 设定curl参数，采用post模式...
		res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strPost);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strPost.GetLength());
		res = curl_easy_setopt(curl, CURLOPT_HEADER, false);
		res = curl_easy_setopt(curl, CURLOPT_POST, true);
		res = curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
		res = curl_easy_setopt(curl, CURLOPT_URL, strUrl);
		res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, procPostCurl);
		res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this);
		res = curl_easy_perform(curl);
	}while( false );
	// 释放资源...
	if( curl != NULL ) {
		curl_easy_cleanup(curl);
	}
	// 直接返回...
	return true;
}*/

void CWebThread::Entry()
{
	// 首先，需要验证授权是否已经过期...
	if( !this->RegisterHaoYi() ) {
		return;
	}
	// 授权成功之后，进行下面的操作...
	while( !this->IsStopRequested() ) {
		// 在网站上注册采集端信息，失败继续注册...
		if( !this->RegisterGather() ) {
			::Sleep(300);
			continue;
		}
		// 开始注册摄像头，这里只能注册已知的，新建的不能注册，因此，需要在新扫描出来的地方增加注册功能...
		if( !this->RegisterCamera() ) {
			::Sleep(300);
			continue;
		}
		// 主视图启动组播频道自动搜索线程，启动Tracker自动连接，中间视图创建等等...
		m_lpHaoYiView->PostMessage(WM_WEB_LOAD_RESOURCE);
		break;
	}
}