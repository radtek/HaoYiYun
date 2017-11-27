
#include "stdafx.h"
#include "HaoYiView.h"
#include "WebThread.h"
#include "SocketUtils.h"
#include "StringParser.h"
#include "XmlConfig.h"
#include "UtilTool.h"
#include "MainFrm.h"
#include "tinyxml.h"
#include <curl.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CWebThread::CWebThread(CHaoYiView * lpView)
  : m_eRegState(kRegHaoYi)
  , m_lpHaoYiView(lpView)
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
// 将每次反馈的数据进行累加 => 有可能一次请求的数据是分开发送的...
void CWebThread::doPostCurl(char * pData, size_t nSize)
{
	m_strUTF8Data.append(pData, nSize);
	//TRACE("Curl: %s\n", pData);
	//TRACE输出有长度限制，太长会截断...
}
//
// 解析JSON数据头信息...
BOOL CWebThread::parseJson(Json::Value & outValue)
{
	// 判断获取的数据是否有效...
	if( m_strUTF8Data.size() <= 0 ) {
		MsgLogGM(GM_Err_Json);
		return false;
	}
	Json::Reader reader;
	// 将UTF8网站数据转换成ANSI格式 => 由于是php编码过的，转换后无效，获取具体数据之后，还要转换一遍...
	string strANSIData = CUtilTool::UTF8_ANSI(m_strUTF8Data.c_str());
	// 解析转换后的json数据包...
	if( !reader.parse(strANSIData, outValue) ) {
		MsgLogGM(GM_Err_Json);
		return false;
	}
	// 获取返回的采集端编号和错误信息...
	if( outValue["err_code"].asBool() ) {
		string & strData = outValue["err_msg"].asString();
		string & strMsg = CUtilTool::UTF8_ANSI(strData.c_str());
		MsgLogINFO(strMsg.c_str());
		return false;
	}
	// 没有错误，返回正确...
	return true;
}
//
// 验证许可证...
BOOL CWebThread::RegisterHaoYi()
{
	// 先设置当前状态信息...
	m_eRegState = kRegHaoYi;
	m_strUTF8Data.clear();
	// 判断数据是否有效...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	int nWebType = theConfig.GetWebType();
	string  & strWebTag = theConfig.GetWebTag();
	string  & strWebName = theConfig.GetWebName();
	string  & strWebAddr = theConfig.GetWebAddr();
	CString & strMacAddr = m_lpHaoYiView->m_strMacAddr;
	CString & strIPAddr = m_lpHaoYiView->m_strIPAddr;
	if( strMacAddr.GetLength() <= 0 || strIPAddr.GetLength() <= 0 ) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 网站节点标记不能为空 => 必须先通过RegisterGather过程...
	if( strWebTag.size() <= 0 || nWebType < 0 || strWebName.size() <= 0 ) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 准备需要的汇报数据 => POST数据包...
	CString strPost, strUrl;
	TCHAR	szDNS[MAX_PATH] = {0};
	TCHAR	szWebName[MAX_PATH] = {0};
	// 先对频道名称进行UTF8转换，再进行URI编码...
	string  strDNSName = CUtilTool::GetServerDNSName();
	string  strUTF8Name = CUtilTool::ANSI_UTF8(strDNSName.c_str());
	string	strUTF8Web = CUtilTool::ANSI_UTF8(strWebName.c_str());
	StringParser::EncodeURI(strUTF8Name.c_str(), strUTF8Name.size(), szDNS, MAX_PATH);
	StringParser::EncodeURI(strUTF8Web.c_str(), strUTF8Web.size(), szWebName, MAX_PATH);
	strPost.Format("mac_addr=%s&ip_addr=%s&name_pc=%s&version=%s&node_tag=%s&node_type=%d&node_addr=%s:%d&node_name=%s&os_name=%s", 
					strMacAddr, strIPAddr, szDNS, _T(SZ_VERSION_NAME), strWebTag.c_str(), nWebType, strWebAddr.c_str(),
					theConfig.GetWebPort(), szWebName, CUtilTool::GetServerOS());
	// 这里需要用到 https 模式，因为，myhaoyi.com 全站都用 https 模式...
	strUrl.Format("https://%s/wxapi.php/Gather/verify", "www.myhaoyi.com");
	// 调用Curl接口，汇报采集端信息...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if( curl == NULL )
			break;
		// 设定curl参数，采用post模式，设置5秒超时，忽略证书检查...
		res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
		res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
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
	Json::Value value;
	// 解析JSON失败，通知界面层...
	if( !this->parseJson(value) ) {
		m_lpHaoYiView->PostMessage(WM_WEB_AUTH_RESULT, kAuthExpired, false);
		return false;
	}
	// 解析JSON成功，进一步解析数据...
	int nMaxCameraNum = atoi(CUtilTool::getJsonString(value["max_camera"]).c_str());
	int nHaoYiGatherID = atoi(CUtilTool::getJsonString(value["gather_id"]).c_str());
	int nHaoYiNodeID = atoi(CUtilTool::getJsonString(value["node_id"]).c_str());
	string strExpired = CUtilTool::getJsonString(value["auth_expired"]);
	// 通知主窗口授权过期验证结果...
	m_lpHaoYiView->PostMessage(WM_WEB_AUTH_RESULT, kAuthExpired, ((nHaoYiGatherID > 0) ? true : false));
	// 存放到配置对象，返回授权验证结果...
	theConfig.SetAuthExpired(strExpired);
	theConfig.SetMaxCamera(nMaxCameraNum);
	theConfig.SetDBHaoYiNodeID(nHaoYiNodeID);
	theConfig.SetDBHaoYiGatherID(nHaoYiGatherID);
	return ((nHaoYiGatherID > 0) ? true : false);
}
//
// 在网站上注册采集端...
BOOL CWebThread::RegisterGather()
{
	// 先设置当前状态信息...
	m_eRegState = kRegGather;
	m_strUTF8Data.clear();
	// 判断数据是否有效...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	int nWebPort = theConfig.GetWebPort();
	string  & strWebAddr = theConfig.GetWebAddr();
	CString & strMacAddr = m_lpHaoYiView->m_strMacAddr;
	CString & strIPAddr = m_lpHaoYiView->m_strIPAddr;
	if( strWebAddr.size() <= 0 || nWebPort <= 0 ) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	if( strMacAddr.GetLength() <= 0 || strIPAddr.GetLength() <= 0 ) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 准备需要的汇报数据 => POST数据包...
	CString strPost, strUrl;
	TCHAR	szDNS[MAX_PATH] = {0};
	// 先对频道名称进行UTF8转换，再进行URI编码...
	string  strDNSName = CUtilTool::GetServerDNSName();
	string  strUTF8Name = CUtilTool::ANSI_UTF8(strDNSName.c_str());
	StringParser::EncodeURI(strUTF8Name.c_str(), strUTF8Name.size(), szDNS, MAX_PATH);
	strPost.Format("mac_addr=%s&ip_addr=%s&name_pc=%s&os_name=%s", 
					strMacAddr, strIPAddr, szDNS, CUtilTool::GetServerOS());
	strUrl.Format("%s:%d/wxapi.php/Gather/index", strWebAddr.c_str(), nWebPort);
	// 调用Curl接口，汇报采集端信息...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if( curl == NULL )
			break;
		// 如果是https://协议，需要新增参数...
		if( theConfig.IsWebHttps() ) {
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		}
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
	Json::Value value;
	// 解析JSON失败，通知界面层...
	if( !this->parseJson(value) ) {
		m_lpHaoYiView->PostMessage(WM_WEB_AUTH_RESULT, kAuthRegister, false);
		return false;
	}
	// 处理新增的配置内容 => name_set | web_name 需要转换成ANSI格式...
	string strMainName = CUtilTool::UTF8_ANSI(CUtilTool::getJsonString(value["name_set"]).c_str());
	if( strMainName.size() <= 0 ) { strMainName = DEF_MAIN_NAME; }
	int nMainKbps = atoi(CUtilTool::getJsonString(value["main_rate"]).c_str());
	int nSubKbps = atoi(CUtilTool::getJsonString(value["sub_rate"]).c_str());
	int nSliceVal = atoi(CUtilTool::getJsonString(value["slice_val"]).c_str());
	int nInterVal = atoi(CUtilTool::getJsonString(value["inter_val"]).c_str());
	int nSnapVal = atoi(CUtilTool::getJsonString(value["snap_val"]).c_str());
	BOOL bAutoLinkDVR = atoi(CUtilTool::getJsonString(value["auto_dvr"]).c_str());
	BOOL bAutoLinkFDFS = atoi(CUtilTool::getJsonString(value["auto_fdfs"]).c_str());
	// 获取Tracker|Remote|Local，并存放到配置文件，但不存盘...
	int nDBGatherID = atoi(CUtilTool::getJsonString(value["gather_id"]).c_str());
	int nWebType = atoi(CUtilTool::getJsonString(value["web_type"]).c_str());
	string strRemoteAddr = CUtilTool::getJsonString(value["transmit_addr"]);
	int nRemotePort = atoi(CUtilTool::getJsonString(value["transmit_port"]).c_str());
	string strTrackerAddr = CUtilTool::getJsonString(value["tracker_addr"]);
	int nTrackerPort = atoi(CUtilTool::getJsonString(value["tracker_port"]).c_str());
	string strWebName = CUtilTool::UTF8_ANSI(CUtilTool::getJsonString(value["web_name"]).c_str());
	string strWebTag = CUtilTool::getJsonString(value["web_tag"]);
	// 获取通道记录列表，先清除旧的列表...
	GM_MapNodeCamera & theNode = theConfig.GetNodeCamera();
	Json::Value & theCamera = value["camera"];
	theNode.clear();
	// 解析通道记录列表...
	if( theCamera.isArray() ) {
		for(int i = 0; i < theCamera.size(); ++i) {
			int nDBCameraID; GM_MapData theMapData;
			theMapData["camera_id"] = CUtilTool::getJsonString(theCamera[i]["camera_id"]);
			nDBCameraID = atoi(theMapData["camera_id"].c_str());
			theNode[nDBCameraID] = theMapData;
		}
	}
	// 同步网站服务器时钟...
#ifndef _DEBUG
	Json::Value & theLocalTime = value["local_time"];
	if( theLocalTime.isString() ) {
		COleDateTime theDate;
		SYSTEMTIME   theST = {0};
		string strLocalTime = theLocalTime.asString();
		// 解析正确，并且得到系统时间正确，才进行设置...
		if( theDate.ParseDateTime(strLocalTime.c_str()) && theDate.GetAsSystemTime(theST) ) {
			::SetLocalTime(&theST);
		}
	}
#endif // _DEBUG
	// 通知主窗口授权网站注册结果...
	m_lpHaoYiView->PostMessage(WM_WEB_AUTH_RESULT, kAuthRegister, ((nDBGatherID > 0) ? true : false));
	// 判断采集端是否注册成功...
	if( nDBGatherID <= 0 || strWebTag.size() <= 0 ) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	ASSERT( nDBGatherID > 0 && strWebTag.size() > 0 );
	if( nWebType < 0 || strWebName.size() <= 0 ) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 判断Tracker地址是否已经正确获取得到...
	if( strTrackerAddr.size() <= 0 || nTrackerPort <= 0 ) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	if( strRemoteAddr.size() <= 0 || nRemotePort <= 0 ) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 录像切片、切片交错，可以为0，0表示不切片，不交错...
	if( nInterVal < 0 || nSliceVal < 0 ) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 主码流和子码流必须有效...
	if( nMainKbps <= 0 || nSubKbps <= 0 ) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 设置默认的截图时间间隔 => 不要超过10分钟...
	nSnapVal = ((nSnapVal <= 0) ? 2 : nSnapVal);
	nSnapVal = ((nSnapVal >= 10) ? 10 : nSnapVal);
	// 存放到配置文件，但并不存盘...
	theConfig.SetDBGatherID(nDBGatherID);
	theConfig.SetWebTag(strWebTag);
	theConfig.SetWebType(nWebType);
	theConfig.SetWebName(strWebName);
	theConfig.SetRemoteAddr(strRemoteAddr);
	theConfig.SetRemotePort(nRemotePort);
	theConfig.SetTrackerAddr(strTrackerAddr);
	theConfig.SetTrackerPort(nTrackerPort);
	// 存放新增的采集端配置信息...
	theConfig.SetMainName(strMainName);
	theConfig.SetMainKbps(nMainKbps);
	theConfig.SetSubKbps(nSubKbps);
	theConfig.SetInterVal(nInterVal);
	theConfig.SetSliceVal(nSliceVal);
	theConfig.SetSnapVal(nSnapVal);
	theConfig.SetAutoLinkFDFS(bAutoLinkFDFS);
	theConfig.SetAutoLinkDVR(bAutoLinkDVR);
	// 注意：已经获取了通道编号列表...
	return true;
}
//
// 采集端从中心退出...
BOOL CWebThread::LogoutHaoYi()
{
	// 获取网站配置信息 => GatherID是中心服务器中的编号...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	int nDBHaoYiGatherID = theConfig.GetDBHaoYiGatherID();
	if( nDBHaoYiGatherID <= 0  ) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 先设置当前状态信息...
	m_eRegState = kGatherLogout;
	m_strUTF8Data.clear();
	// 准备需要的汇报数据 => POST数据包...
	CString strPost, strUrl;
	strPost.Format("gather_id=%d", nDBHaoYiGatherID);
	// 这里需要用到 https 模式，因为，myhaoyi.com 全站都用 https 模式...
	strUrl.Format("https://%s/wxapi.php/Gather/logout", "www.myhaoyi.com");
	// 调用Curl接口，汇报摄像头数据...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if( curl == NULL )
			break;
		// 如果是https://协议，需要新增参数...
		res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
		res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		// 设定curl参数，采用post模式...
		res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strPost);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strPost.GetLength());
		res = curl_easy_setopt(curl, CURLOPT_HEADER, false);
		res = curl_easy_setopt(curl, CURLOPT_POST, true);
		res = curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
		res = curl_easy_setopt(curl, CURLOPT_URL, strUrl);
		// 这里不需要处理网站返回的数据...
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
// 采集端从节点退出...
BOOL CWebThread::LogoutGather()
{
	// 获取网站配置信息...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	int nWebPort = theConfig.GetWebPort();
	int nDBGatherID = theConfig.GetDBGatherID();
	string & strWebAddr = theConfig.GetWebAddr();
	if( nDBGatherID <= 0 || nWebPort <= 0 || strWebAddr.size() <= 0 ) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 先设置当前状态信息...
	m_eRegState = kGatherLogout;
	m_strUTF8Data.clear();
	// 准备需要的汇报数据 => POST数据包...
	CString strPost, strUrl;
	strPost.Format("gather_id=%d", nDBGatherID);
	// 组合访问链接地址...
	strUrl.Format("%s:%d/wxapi.php/Gather/logout", strWebAddr.c_str(), nWebPort);
	// 调用Curl接口，汇报摄像头数据...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if( curl == NULL )
			break;
		// 如果是https://协议，需要新增参数...
		if( theConfig.IsWebHttps() ) {
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		}
		// 设定curl参数，采用post模式...
		res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strPost);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strPost.GetLength());
		res = curl_easy_setopt(curl, CURLOPT_HEADER, false);
		res = curl_easy_setopt(curl, CURLOPT_POST, true);
		res = curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
		res = curl_easy_setopt(curl, CURLOPT_URL, strUrl);
		// 这里不需要处理网站返回的数据...
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
// 采集端退出汇报...
BOOL CWebThread::doWebGatherLogout()
{
	this->LogoutGather();
	this->LogoutHaoYi();
	return true;
}
//
// 从网站上获取通道配置信息...
BOOL CWebThread::GetAllCameraData()
{
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	GM_MapNodeCamera & theMapCamera = theConfig.GetNodeCamera();
	GM_MapNodeCamera::iterator itorItem = theMapCamera.begin();
	while( itorItem != theMapCamera.end() ) {
		// 已注册摄像头数目不够，进行网站注册操作...
		int nDBCameraID = itorItem->first;
		GM_MapData & theData = itorItem->second;
		if( !this->doWebGetCamera(nDBCameraID) ) {
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
// 从网站获取通道配置和通道下的录像配置...
BOOL CWebThread::doWebGetCamera(int nDBCameraID)
{
	// 获取网站配置信息...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	int nWebPort = theConfig.GetWebPort();
	int nDBGatherID = theConfig.GetDBGatherID();
	string & strWebAddr = theConfig.GetWebAddr();
	if( nWebPort <= 0 || strWebAddr.size() <= 0 ) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 如果当前已注册摄像头数目超过了最大支持数，不用再注册...
	if( m_nCurCameraCount >= theConfig.GetMaxCamera() ) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 先设置当前状态信息...
	m_eRegState = kGetCamera;
	m_strUTF8Data.clear();
	// 准备需要的汇报数据 => POST数据包...
	CString strPost, strUrl;
	strPost.Format("gather_id=%d&camera_id=%d", nDBGatherID, nDBCameraID);
	strUrl.Format("%s:%d/wxapi.php/Gather/getCamera", strWebAddr.c_str(), nWebPort);
	// 调用Curl接口，汇报摄像头数据...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if( curl == NULL )
			break;
		// 如果是https://协议，需要新增参数...
		if( theConfig.IsWebHttps() ) {
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		}
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
	Json::Value value;
	// 解析JSON失败，通知界面层...
	if( !this->parseJson(value) ) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 初始化数据库里的通道配置和录像配置...
	GM_MapCourse	dbMapCourse;			// 通道下的录像课表...
	GM_MapData		dbMapCamera;			// 通道下的配置集合...
	// 获取通道配置信息 => 注意：某些字段需要转换成ANSI格式...
	Json::Value & theDBCamera = value["camera"];
	if( theDBCamera.isObject() ) {
		// 算子itorItem必须放在内部定义，否则，会出现越界问题...
		for(Json::Value::iterator itorItem = theDBCamera.begin(); itorItem != theDBCamera.end(); ++itorItem) {
			const char * theKey = itorItem.memberName();
			// 包含中文的Key需要进行UTF8格式转换...
			if( stricmp(theKey, "stream_url") == 0 || stricmp(theKey, "stream_mp4") == 0 || 
				stricmp(theKey, "camera_name") == 0 || stricmp(theKey, "device_user") == 0 ) {
				dbMapCamera[theKey] = CUtilTool::UTF8_ANSI(CUtilTool::getJsonString(theDBCamera[theKey]).c_str());
			} else {
				dbMapCamera[theKey] = CUtilTool::getJsonString(theDBCamera[theKey]);
			}
		}
		//Json::Value::Members arrayMember = theDBCamera.getMemberNames();
	}
	// 获取录像课程表...
	Json::Value & theCourse = value["course"];
	if( theCourse.isArray() ) {
		for(int i = 0; i < theCourse.size(); ++i) {
			int nCourseID; GM_MapData theMapData;
			for(Json::Value::iterator itorItem = theCourse[i].begin(); itorItem != theCourse[i].end(); ++itorItem) {
				const char * theKey = itorItem.memberName();
				theMapData[theKey] = CUtilTool::getJsonString(theCourse[i][theKey]);
			}
			// 获取记录编号，存放到集合当中...
			nCourseID = atoi(theMapData["course_id"].c_str());
			dbMapCourse[nCourseID] = theMapData;
		}
	}
	// 判断摄像头是否注册成功...
	if( dbMapCamera.size() <= 0 ) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 将获取到的通道配置，直接存放到内存当中，用数据库编号定位...
	theConfig.SetCamera(nDBCameraID, dbMapCamera);
	// 将获取得到的录像课程表存放起来，直接覆盖原来的记录，用数据库编号定位...
	// 录像课程表都是记录到内存当中，不存入Config.xml当中...
	if( dbMapCourse.size() > 0 ) {
		theConfig.SetCourse(nDBCameraID, dbMapCourse);
	}
	// 注册摄像头成功，摄像头累加计数...
	++m_nCurCameraCount;
	return true;
}
//
// 在网站上具体执行添加或更新摄像头操作...
BOOL CWebThread::doWebRegCamera(GM_MapData & inData)
{
	// 输入数据中必须包含device_sn和stream_prop字段...
	GM_MapData::iterator itorSN, itorProp;
	itorProp = inData.find("stream_prop");
	itorSN = inData.find("device_sn");
	if( itorSN == inData.end() || itorProp == inData.end() ) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 获取网站配置信息...
	int nStreamProp = atoi(itorProp->second.c_str());
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	int nWebPort = theConfig.GetWebPort();
	int nDBGatherID = theConfig.GetDBGatherID();
	string & strWebAddr = theConfig.GetWebAddr();
	if( nWebPort <= 0 || strWebAddr.size() <= 0 ) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 如果当前已注册摄像头数目超过了最大支持数，不用再注册...
	if( m_nCurCameraCount >= theConfig.GetMaxCamera() ) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 先设置当前状态信息...
	m_eRegState = kRegCamera;
	m_strUTF8Data.clear();
	// 准备需要的汇报数据 => POST数据包...
	CString strPost, strUrl;
	TCHAR	szEncName[MAX_PATH] = {0};
	// 先对频道名称进行UTF8转换，再进行URI编码...
	string  strUTF8Name = CUtilTool::ANSI_UTF8(inData["camera_name"].c_str());
	StringParser::EncodeURI(strUTF8Name.c_str(), strUTF8Name.size(), szEncName, MAX_PATH);
	if( nStreamProp == kStreamDevice ) {
		// 对user和pass进行编码处理...
		TCHAR szDeviceUser[MAX_PATH] = {0};
		TCHAR szDevicePass[MAX_PATH] = {0};
		string strUTF8User = CUtilTool::ANSI_UTF8(inData["device_user"].c_str());
		string strUTF8Pass = CUtilTool::ANSI_UTF8(inData["device_pass"].c_str());
		StringParser::EncodeURI(strUTF8User.c_str(), strUTF8User.size(), szDeviceUser, MAX_PATH);
		StringParser::EncodeURI(strUTF8Pass.c_str(), strUTF8Pass.size(), szDevicePass, MAX_PATH);
		// 处理通道是摄像头的情况...
		strPost.Format("gather_id=%d&stream_prop=%d&camera_type=%s&camera_name=%s&device_sn=%s&device_ip=%s&device_mac=%s&device_type=%s&device_user=%s&"
			"device_pass=%s&device_cmd_port=%s&deive_http_port=%s&device_mirror=%s&device_osd=%s&device_desc=%s&device_twice=%s&device_boot=%s",
			nDBGatherID, nStreamProp, inData["camera_type"].c_str(), szEncName, inData["device_sn"].c_str(), 
			inData["device_ip"].c_str(), inData["device_mac"].c_str(), inData["device_type"].c_str(),
			szDeviceUser, szDevicePass, inData["device_cmd_port"].c_str(), inData["device_http_port"].c_str(),
			inData["device_mirror"].c_str(), inData["device_osd"].c_str(), inData["device_desc"].c_str(),
			inData["device_twice"].c_str(), inData["device_boot"].c_str());
	} else {
		// 对需要的数据进行编码处理 => 这里需要注意文件名过长时的内存溢出问题...
		TCHAR  szMP4File[MAX_PATH * 3] = {0};
		TCHAR  szUrlLink[MAX_PATH * 2] = {0};
		string strUTF8MP4 = CUtilTool::ANSI_UTF8(inData["stream_mp4"].c_str());
		string strUTF8Url = CUtilTool::ANSI_UTF8(inData["stream_url"].c_str());
		StringParser::EncodeURI(strUTF8MP4.c_str(), strUTF8MP4.size(), szMP4File, MAX_PATH * 3);
		StringParser::EncodeURI(strUTF8Url.c_str(), strUTF8Url.size(), szUrlLink, MAX_PATH * 2);
		// 处理通道是流转发的情况...
		ASSERT( nStreamProp == kStreamMP4File || nStreamProp == kStreamUrlLink );
		strPost.Format("gather_id=%d&stream_prop=%d&camera_type=%s&camera_name=%s&device_sn=%s&stream_mp4=%s&stream_url=%s",
			nDBGatherID, nStreamProp, inData["camera_type"].c_str(), szEncName, inData["device_sn"].c_str(), szMP4File, szUrlLink);
	}
	// 组合访问链接地址...
	strUrl.Format("%s:%d/wxapi.php/Gather/regCamera", strWebAddr.c_str(), nWebPort);
	// 调用Curl接口，汇报摄像头数据...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if( curl == NULL )
			break;
		// 如果是https://协议，需要新增参数...
		if( theConfig.IsWebHttps() ) {
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		}
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
	Json::Value value;
	// 解析JSON失败，通知界面层...
	if( !this->parseJson(value) ) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 获取已更新的通道在数据库中的编号...
	string strDBCamera = CUtilTool::getJsonString(value["camera_id"]);
	int nDBCameraID = atoi(strDBCamera.c_str());
	// 判断摄像头是否删除成功...
	if( nDBCameraID <= 0 ) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 将更新后的通道信息写入集合当中 => 添加或修改...
	ASSERT( nDBCameraID > 0 );
	inData["camera_id"] = strDBCamera;
	theConfig.SetCamera(nDBCameraID, inData);
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
	if( nWebPort <= 0 || strWebAddr.size() <= 0 ) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 先设置当前状态信息...
	int nDBCameraID = -1;
	m_eRegState = kDelCamera;
	m_strUTF8Data.clear();
	// 准备需要的汇报数据 => POST数据包...
	CString strPost, strUrl;
	strPost.Format("device_sn=%s", inDeviceSN.c_str());
	// 组合访问链接地址...
	strUrl.Format("%s:%d/wxapi.php/Gather/delCamera", strWebAddr.c_str(), nWebPort);
	// 调用Curl接口，汇报摄像头数据...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if( curl == NULL )
			break;
		// 如果是https://协议，需要新增参数...
		if( theConfig.IsWebHttps() ) {
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		}
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
	Json::Value value;
	// 解析JSON失败，通知界面层...
	if( !this->parseJson(value) ) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 获取返回的已删除的摄像头在数据库中的编号...
	nDBCameraID = atoi(CUtilTool::getJsonString(value["camera_id"]).c_str());
	// 判断摄像头是否删除成功...
	if( nDBCameraID <= 0 ) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	ASSERT( nDBCameraID > 0 );
	// 摄像头计数器减少...
	m_nCurCameraCount -= 1;
	return true;
}
//
// 向网站汇报通道的运行状态 => 0(等待) 1(运行) 2(录像)...
BOOL CWebThread::doWebStatCamera(int nDBCamera, int nStatus, int nErrCode/* = 0*/, LPCTSTR lpszErrMsg/* = NULL*/)
{
	// 获取网站配置信息...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	int nWebPort = theConfig.GetWebPort();
	string & strWebAddr = theConfig.GetWebAddr();
	if( nWebPort <= 0 || strWebAddr.size() <= 0 ) {
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 先设置当前状态信息...
	m_eRegState = kStatCamera;
	m_strUTF8Data.clear();
	// 准备需要的汇报数据 => POST数据包...
	CString strPost, strUrl;
	TCHAR szErrMsg[MAX_PATH] = {0};
	string strUTF8Err = ((lpszErrMsg != NULL) ? CUtilTool::ANSI_UTF8(lpszErrMsg) : "");
	StringParser::EncodeURI(strUTF8Err.c_str(), strUTF8Err.size(), szErrMsg, MAX_PATH);
	strPost.Format("camera_id=%d&status=%d&err_code=%d&err_msg=%s", nDBCamera, nStatus, nErrCode, szErrMsg);
	// 组合访问链接地址...
	strUrl.Format("%s:%d/wxapi.php/Gather/saveCamera", strWebAddr.c_str(), nWebPort);
	// 调用Curl接口，汇报摄像头数据...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if( curl == NULL )
			break;
		// 如果是https://协议，需要新增参数...
		if( theConfig.IsWebHttps() ) {
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		}
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

void CWebThread::Entry()
{
	// 首先，在网站上注册采集端信息...
	if( !this->RegisterGather() ) {
		return;
	}
	// 然后，需要验证授权是否已经过期...
	if( !this->RegisterHaoYi() ) {
		return;
	}
	// 开始注册摄像头，这里只能注册已知的，新建的不能注册，因此，需要在新扫描出来的地方增加注册功能...
	if( !this->GetAllCameraData() ) {
		return;
	}
	// 主视图启动组播频道自动搜索线程，启动Tracker自动连接，中间视图创建等等...
	m_lpHaoYiView->PostMessage(WM_WEB_LOAD_RESOURCE);
}