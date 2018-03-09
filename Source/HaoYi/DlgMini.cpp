
#include "stdafx.h"
#include "HaoYi.h"
#include "DlgMini.h"
#include "XmlConfig.h"
#include "UtilTool.h"
#include <curl.h>

IMPLEMENT_DYNAMIC(CDlgMini, CDialogEx)

CDlgMini::CDlgMini(CWnd* pParent /*=NULL*/)
  : CDialogEx(CDlgMini::IDD, pParent)
  , m_eMiniState(kMiniToken)
  , m_eBindCmd(kNoneCmd)
  , m_nBindUserID(-1)
  , m_lpBtnFont(NULL)
{
}

CDlgMini::~CDlgMini()
{
}

void CDlgMini::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CDlgMini, CDialogEx)
	ON_WM_CLOSE()
	ON_WM_DESTROY()
	ON_WM_ERASEBKGND()
	ON_MESSAGE(WM_BIND_MINI_MSG, &CDlgMini::OnMsgBindMini)
	ON_MESSAGE(WM_UNBIND_MINI_MSG, &CDlgMini::OnMsgUnBindMini)
	ON_COMMAND(kButtonUnBind, &CDlgMini::OnClickUnBindButton)
END_MESSAGE_MAP()

size_t miniPostCurl(void *ptr, size_t size, size_t nmemb, void *stream)
{
	CDlgMini * lpThread = (CDlgMini*)stream;
	lpThread->doPostCurl((char*)ptr, size * nmemb);
	return size * nmemb;
}
//
// 将每次反馈的数据进行累加 => 有可能一次请求的数据是分开发送的...
void CDlgMini::doPostCurl(char * pData, size_t nSize)
{
	m_strUTF8Data.append(pData, nSize);
	//TRACE("Curl: %s\n", pData);
	//TRACE输出有长度限制，太长会截断...
}
//
// 解析JSON数据头信息 => 如果是微信API，错误码是 errcode 和 errmsg...
BOOL CDlgMini::parseJson(Json::Value & outValue, BOOL bIsWeiXin)
{
	// 判断获取的数据是否有效...
	if( m_strUTF8Data.size() <= 0 ) {
		m_strNotice = "没有获取到有效数据";
		MsgLogGM(GM_Err_Json);
		return false;
	}
	Json::Reader reader;
	// 将UTF8网站数据转换成ANSI格式 => 由于是php编码过的，转换后无效，获取具体数据之后，还要转换一遍...
	string strANSIData = m_strUTF8Data; //CUtilTool::UTF8_ANSI(m_strUTF8Data.c_str());
	// 解析转换后的json数据包...
	if( !reader.parse(strANSIData, outValue) ) {
		m_strNotice = "解析获取到的JSON数据失败";
		MsgLogGM(GM_Err_Json);
		return false;
	}
	// 获取返回的采集端编号和错误信息...
	LPCTSTR lpszCode = bIsWeiXin ? "errcode" : "err_code";
	LPCTSTR lpszMsg = bIsWeiXin ? "errmsg" : "err_msg";
	if( outValue[lpszCode].asBool() ) {
		string & strData = outValue[lpszMsg].asString();
		string & strMsg = CUtilTool::UTF8_ANSI(strData.c_str());
		string & strCode = CUtilTool::getJsonString(outValue[lpszCode]);
		m_strNotice.Format("错误码：%s，%s", strCode.c_str(), strMsg.c_str());
		MsgLogINFO(m_strNotice);
		return false;
	}
	// 没有错误，返回正确...
	return true;
}
//
// 响应绑定小程序命令事件...
LRESULT	CDlgMini::OnMsgBindMini(WPARAM wParam, LPARAM lParam)
{
	// 保存命令编号和用户编号...
	m_nBindUserID = (int)lParam;
	m_eBindCmd = (BIND_CMD)wParam;
	// 将状态更新到界面层...
	this->Invalidate();
	// 根据传递的命令做一些操作...
	if( m_eBindCmd == kSaveCmd ) {
		// 将绑定用户保存到全局对象...
		CXmlConfig & theConfig = CXmlConfig::GMInstance();
		theConfig.SetDBHaoYiUserID(m_nBindUserID);
		// 获取用户头像图片数据...
		this->doWebGetUserHead();
	} else if( m_eBindCmd == kCancelCmd ) {
		// 如果是取消操作，隐藏解除绑定按钮...
		m_btnUnBind.ShowWindow(SW_HIDE);
	}
	return S_OK;
}
//
// 响应绑定小程序命令事件...
LRESULT	CDlgMini::OnMsgUnBindMini(WPARAM wParam, LPARAM lParam)
{
	// 重置全部的数据变量...
	this->ResetData();
	// 注意：这里不能调用解除绑定接口，由小程序触发的...
	// 解除绑定成功，重置全局绑定用户编号...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.SetDBHaoYiUserID(-1);
	// 隐藏解除绑定按钮...
	m_btnUnBind.ShowWindow(SW_HIDE);
	// 重新启动线程...
	this->Start();
	return S_OK;
}
//
// 创建解除绑定按钮...
void CDlgMini::CreateUnBindButton()
{
	this->DestoryUnBindButton();

	CRect rcButton;
	rcButton.SetRect(20, 20, 20 + kBtnWidth, 20 + kBtnHeight);
	m_btnUnBind.Create("解除绑定", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonUnBind);
	(m_lpBtnFont != NULL) ? m_btnUnBind.SetFont(m_lpBtnFont, true) : NULL;
	m_btnUnBind.ShowWindow(SW_HIDE);
}
//
// 销毁解除绑定按钮...
void CDlgMini::DestoryUnBindButton()
{
	m_btnUnBind.DestroyWindow();
}
//
// 点击解除绑定按钮的操作...
void CDlgMini::OnClickUnBindButton()
{
	// 重置全部的数据变量...
	this->ResetData();
	// 调用解除绑定接口...
	this->doWebUnBindMini();
	// 隐藏解除绑定按钮...
	m_btnUnBind.ShowWindow(SW_HIDE);
	// 重新启动线程...
	this->Start();
}

void CDlgMini::ResetData()
{
	m_eMiniState = kMiniToken;
	m_eBindCmd = kNoneCmd;
	m_nBindUserID = -1;
	m_strUTF8Data.clear();
	m_strMiniToken.clear();
	m_strUserName.clear();
	m_strUserHead.clear();
	m_strMiniPath.clear();
	m_strMiniPicCode.clear();
	m_strMiniPicHead.clear();
	m_strNotice.Empty();
}

BOOL CDlgMini::OnInitDialog()
{
	ASSERT( m_lpBtnFont != NULL );
	// 先创建解除绑定按钮...
	this->CreateUnBindButton();
	// 重置全部的数据变量...
	this->ResetData();
	// 设定窗口大小 => 500 * 450 => 需要预先加上上下边框...
	CRect rcWndRect;
	this->GetWindowRect(rcWndRect);
	rcWndRect.SetRect(0, 0, 506, 476);
	// 移动并将窗口居中...
	this->MoveWindow(rcWndRect);
	this->CenterWindow();
	// 启动线程...
	this->Start();
	return true;
}
//
// 显示获取access_token...
void CDlgMini::DispMiniToken(CDC * pDC)
{
	// 获取绘制客户区矩形...
	CRect rcRect, txtRect;
	this->GetClientRect(&rcRect);
	// 计算提示信息显示位置...
	int nTxtHeight = 22;
	txtRect = rcRect;
	txtRect.top = rcRect.Height() / 2 - nTxtHeight * 2;
	txtRect.bottom = txtRect.top + nTxtHeight * 3;
	txtRect.DeflateRect(20, 0);
	// 在指定区域显示提示信息...
	this->ShowText(pDC, m_strNotice, txtRect, RGB(0,0,0), 20, true);
}
//
// 显示小程序码，内部判断...
void CDlgMini::DispMiniCode(CDC * pDC)
{
	// 如果小程序码的数据为空，直接显示警告信息...
	if( m_strMiniPicCode.size() <= 0 ) {
		this->DispMiniToken(pDC);
		return;
	}
	// 获取绘制客户区矩形...
	CString strQRNotice;
	CRect rcRect, qrRect, txtRect;
	this->GetClientRect(&rcRect);
	ASSERT( m_strMiniPicCode.size() > 0);
	// 让小程序码剧中显示...
	qrRect.left = (rcRect.Width() - kQRCodeWidth) / 2;
	qrRect.top = (rcRect.Height() - kQRCodeWidth) / 2 - 40;
	qrRect.right = qrRect.left + kQRCodeWidth;
	qrRect.bottom = qrRect.top + kQRCodeWidth;
	// 显示小程序码的图片数据区...
	this->DrawImage(pDC, m_strMiniPicCode, qrRect);
	// 计算提示信息显示位置...
	txtRect = rcRect;
	txtRect.DeflateRect(20, 0);
	txtRect.top = qrRect.bottom + 40;
	txtRect.bottom = txtRect.top + 20;
	// 根据绑定命令，显示不同的提示...
	if( m_eBindCmd == kScanCmd ) {
		// 扫描成功之后的事件通知...
		strQRNotice = "扫描成功，请在微信中点击【确认绑定】";
		this->ShowText(pDC, strQRNotice, txtRect, RGB(0,0,0), 18, true);
	} else if( m_eBindCmd == kSaveCmd ) {
		// 绑定成功之后的事件通知...
		strQRNotice = "已经绑定成功，正在跳转...";
		this->ShowText(pDC, strQRNotice, txtRect, RGB(0,0,0), 18, true);
	} else if( m_eBindCmd == kCancelCmd ) {
		// 取消绑定之后的事件通知...
		strQRNotice = "您已取消此次绑定，您可再次扫描绑定，或关闭窗口";
		txtRect.top = qrRect.bottom + 30;
		txtRect.bottom = txtRect.top + 50;
		txtRect.DeflateRect(100, 0);
		this->ShowText(pDC, strQRNotice, txtRect, RGB(235,87,34), 18, true);
	} else {
		// 默认命令的显示信息...
		strQRNotice = "请用微信扫描绑定小程序";
		this->ShowText(pDC, strQRNotice, txtRect, RGB(0,0,0), 18, true);
	}
}
//
// 显示用户头像，内部判断...
void CDlgMini::DispMiniHead(CDC * pDC)
{
	// 如果用户头像的数据为空，直接显示警告信息...
	if( m_strMiniPicHead.size() <= 0 ) {
		this->DispMiniToken(pDC);
		return;
	}
	// 获取绘制客户区矩形...
	CString theTxtInfo;
	CRect rcRect, qrRect, txtRect, rcButton;
	this->GetClientRect(&rcRect);
	ASSERT( m_strMiniPicHead.size() > 0);
	// 让头像居中显示...
	qrRect.left = (rcRect.Width() - kQRHeadWidth) / 2;
	qrRect.top = (rcRect.Height() - kQRHeadWidth) / 2 - 100;
	qrRect.right = qrRect.left + kQRHeadWidth;
	qrRect.bottom = qrRect.top + kQRHeadWidth;
	// 显示用户头像的图片数据区...
	this->DrawImage(pDC, m_strMiniPicHead, qrRect);
	// 计算用户昵称显示位置...
	txtRect = rcRect;
	txtRect.DeflateRect(40, 0);
	txtRect.top = qrRect.bottom + 20;
	txtRect.bottom = txtRect.top + 25;
	theTxtInfo = m_strUserName.c_str();
	this->ShowText(pDC, theTxtInfo, txtRect, RGB(0,0,0), 20, true);
	// 计算解除绑定按钮显示位置...
	rcButton.left = (rcRect.Width() - kBtnWidth) / 2;
	rcButton.right = rcButton.left + kBtnWidth;
	rcButton.top = txtRect.bottom + 20;
	rcButton.bottom = rcButton.top + kBtnHeight;
	m_btnUnBind.MoveWindow(rcButton);
	m_btnUnBind.ShowWindow(SW_SHOW);
	// 显示更多提示信息...
	txtRect = rcRect;
	txtRect.DeflateRect(20, 0);
	txtRect.top = rcButton.bottom + 30;
	txtRect.bottom = txtRect.top + 18;
	theTxtInfo = "您已完成绑定操作，您可点击【解除绑定】重新绑定，或关闭窗口";
	this->ShowText(pDC, theTxtInfo, txtRect, RGB(51,51,51), 14, false);
	txtRect.top = txtRect.bottom + 8;
	txtRect.bottom = txtRect.top + 18;
	theTxtInfo = "您现在可以用微信小程序管理本采集端";
	this->ShowText(pDC, theTxtInfo, txtRect, RGB(51,51,51), 14, false);
}
//
// 绘制背景...
BOOL CDlgMini::OnEraseBkgnd(CDC* pDC)
{
	// 统一绘制背景颜色...
	CRect rcRect;
	this->GetClientRect(&rcRect);
	pDC->FillSolidRect(rcRect, RGB(255, 255, 255));//RGB(242, 245, 233));
	// 更具状态显示不同的信息...
	if( m_eMiniState == kMiniToken ) {
		// 显示获取access_token状态...
		this->DispMiniToken(pDC);
	} else if( m_eMiniState == kMiniCode ) {
		// 显示获取小程序码过程...
		if( m_eBindCmd == kSaveCmd ) {
			// 绑定成功之后，显示头像...
			this->DispMiniHead(pDC);
		} else {
			// 正在绑定，显示小程序码...
			this->DispMiniCode(pDC);
		}
	} else if( m_eMiniState == kMiniHead ) {
		this->DispMiniHead(pDC);
	}
	return true;
}
//
// 显示小程序码的图片数据...
void CDlgMini::DrawImage(CDC * pDC, string & inImgData, CRect & inDispRect)
{
	// 如果数据为空，直接返回...
	if( inImgData.size() <= 0 )
		return;
	// 创建一个全局缓存区...
	IStream * pIStream = NULL;
	LONG      cbSrcSize = inImgData.size();
	LPVOID	  lpSrcData = (LPVOID)inImgData.c_str();
	HGLOBAL   hGlobal = GlobalAlloc(GMEM_MOVEABLE, cbSrcSize);   
	// 得到内存指针...
	LPVOID    pvData = NULL;
	ASSERT( hGlobal != NULL );
	pvData = GlobalLock(hGlobal);
	ASSERT( pvData != NULL );
	// 拷贝数据，解锁缓存...
	memcpy(pvData, lpSrcData, cbSrcSize);   
	GlobalUnlock(hGlobal);
	// 创建数据流接口对象...
	IPicture  *  pIPicObj = NULL;
	HRESULT		 hResult = S_OK;
	LONG		 nCount = 0;
	do {
		hResult = CreateStreamOnHGlobal(hGlobal, TRUE, &pIStream);
		if( FAILED(hResult) ) break;
		hResult = OleLoadPicture(pIStream, cbSrcSize, TRUE, IID_IPicture, (LPVOID*)&pIPicObj);
		if( FAILED(hResult) ) break;
		OLE_XSIZE_HIMETRIC hmWidth = 0;
		OLE_YSIZE_HIMETRIC hmHeight = 0;
		hResult = pIPicObj->get_Width(&hmWidth);
		hResult = pIPicObj->get_Height(&hmHeight);
		hResult = pIPicObj->Render(*pDC, inDispRect.left, inDispRect.top, inDispRect.Width(), inDispRect.Height(), 0, hmHeight, hmWidth, -hmHeight, NULL);
	} while( false );
	// 释放之前分配的资源...
	GlobalFree(hGlobal); hGlobal = NULL;
	nCount = ((pIStream != NULL) ? pIStream->Release() : 0);
	nCount = ((pIPicObj != NULL) ? pIPicObj->Release() : 0);
}
//
// 显示文字信息 => 显示区域、文字颜色、字体大小、是否粗体...
void CDlgMini::ShowText(CDC * pDC, CString & inTxtData, CRect & inDispRect, COLORREF inTxtColor, int inFontSize, bool isBold)
{
	// 如果文字为空，直接返回...
	if( inTxtData.GetLength() <= 0 )
		return;
	// 准备字体对象...
	CFont   fontLogo;
	CFont * pOldFont = NULL;
	pDC->SetBkMode(TRANSPARENT);
	// 创建字体 => 根据输入的参数配置...
	fontLogo.CreateFont(inFontSize, 0, 0, 0, (isBold ? FW_BOLD : FW_LIGHT), false, false,0,0,0,0,0,0, "宋体");
	// 设置文字字体和文字颜色...
	pOldFont = pDC->SelectObject(&fontLogo);
	pDC->SetTextColor( inTxtColor );
	// 绘制文字信息 => 多行显示...
	pDC->DrawText(inTxtData, inDispRect, DT_EXTERNALLEADING | DT_WORDBREAK | DT_CENTER | DT_VCENTER);
	// 销毁字体...
	pDC->SelectObject(pOldFont);
	fontLogo.DeleteObject();
	fontLogo.Detach();
}
//
// 获取绑定用户的头像数据...
BOOL CDlgMini::doWebGetUserHead()
{
	// 更新提示信息内容 => 并更新到窗口...
	m_strNotice = "正在获取用户头像...";
	this->Invalidate();
	// 判断用户头像连接是否为空...
	if( m_strUserHead.size() <= 0 ) {
		m_strNotice = "已经绑定用户头像地址无效...";
		MsgLogGM(GM_NotImplement);
		this->Invalidate();
		return false;
	}
	// 清空上一次遗留的缓存...
	m_strUTF8Data.clear();
	// 微信头像的连接都是https地址...
	CString strUrl = m_strUserHead.c_str();
	// 调用Curl接口，汇报摄像头数据...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if( curl == NULL )
			break;
		// 如果是https://协议，需要新增参数...
		res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
		res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		// 设定curl参数，采用get模式...
		res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		res = curl_easy_setopt(curl, CURLOPT_HEADER, false);
		res = curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
		res = curl_easy_setopt(curl, CURLOPT_URL, strUrl);
		// 这里需要处理网站返回的数据...
		res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, miniPostCurl);
		res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this);
		res = curl_easy_perform(curl);
	} while ( false );
	// 释放资源...
	if( curl != NULL ) {
		curl_easy_cleanup(curl);
	}
	// 如果缓存长度为0，读取失败...
	if( m_strUTF8Data.size() <= 0 ) {
		m_strNotice = "获取绑定用户头像失败...";
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// 获取头像图片数据成功 => 缓存起来，更新到界面...
	m_strNotice = "获取绑定用户头像成功，正在显示...";
	m_strMiniPicHead = m_strUTF8Data;
	m_strUTF8Data.clear();
	this->Invalidate();
	return true;
}
//
// 获取小程序的access_token...
BOOL CDlgMini::doWebGetMiniToken()
{
	// 更新提示信息内容 => 并更新到窗口...
	m_strNotice = "正在获取访问凭证...";
	this->Invalidate();
	// 获取网站配置信息 => GatherID是中心服务器中的编号...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	int nDBHaoYiGatherID = theConfig.GetDBHaoYiGatherID();
	if( nDBHaoYiGatherID <= 0  ) {
		m_strNotice = "采集端没有注册成功，无法绑定小程序";
		MsgLogGM(GM_NotImplement);
		this->Invalidate();
		return false;
	}
	// 清空上一次遗留的缓存...
 	m_strUTF8Data.clear();
	// 准备需要的汇报数据 => POST数据包...
	// 新增 miniName 参数 => 小程序名称...
	CString strPost, strUrl;
	// 2018.03.09 - by jackey => 采集端只能绑定一个小程序，目前是绑定到"浩一云服务"的小程序上...
	strPost.Format("gather_id=%d&miniName=%s", nDBHaoYiGatherID, "device");
	//strPost.Format("gather_id=%d&miniName=%s", nDBHaoYiGatherID, "cloud");
	// 这里需要用到 https 模式，因为，myhaoyi.com 全站都用 https 模式...
	strUrl.Format("%s/wxapi.php/Mini/getToken", DEF_WEB_HOME);
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
		// 这里需要处理网站返回的数据 => access_token...
		res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, miniPostCurl);
		res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this);
		res = curl_easy_perform(curl);
	}while( false );
	// 释放资源...
	if( curl != NULL ) {
		curl_easy_cleanup(curl);
	}
	Json::Value value;
	// 解析JSON失败，通知界面层...
	if( !this->parseJson(value, false) ) {
		MsgLogGM(GM_NotImplement);
		this->Invalidate();
		return false;
	}
	// 解析JSON成功，进一步解析数据 => 名称需要转换成ANSI格式...
	m_nBindUserID = atoi(CUtilTool::getJsonString(value["user_id"]).c_str());
	m_strMiniToken = CUtilTool::getJsonString(value["access_token"]);
	m_strUserName = CUtilTool::UTF8_ANSI(CUtilTool::getJsonString(value["user_name"]).c_str());
	m_strUserHead = CUtilTool::getJsonString(value["user_head"]);
	m_strMiniPath = CUtilTool::getJsonString(value["mini_path"]);
	// 将获取到的绑定用户编号更新到全局配置文件当中...
	theConfig.SetDBHaoYiUserID(m_nBindUserID);
	return true;
}
//
// 获取本采集端在小程序中的小程序码...
BOOL CDlgMini::doWebGetMiniCode()
{
	// 更新提示信息内容 => 并更新到窗口...
	m_strNotice = "正在获取小程序码...";
	this->Invalidate();
	// 获取网站配置信息 => GatherID是中心服务器中的编号...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	int nDBHaoYiGatherID = theConfig.GetDBHaoYiGatherID();
	if( nDBHaoYiGatherID <= 0  ) {
		m_strNotice = "采集端没有注册成功，无法绑定小程序";
		MsgLogGM(GM_NotImplement);
		this->Invalidate();
		return false;
	}
	// 判断access_token与path是否有效...
	if( m_strMiniPath.size() <= 0 || m_strMiniToken.size() <= 0 ) {
		m_strNotice = "获取access_token失败，无法获取小程序码";
		MsgLogGM(GM_NotImplement);
		this->Invalidate();
		return false;
	}
	// 清空上一次遗留的缓存...
	m_strUTF8Data.clear();
	// 准备需要的汇报数据 => POST数据包...
	Json::Value itemData;
	CString strPost, strUrl, strGatherID;
	strGatherID.Format("%d", nDBHaoYiGatherID);
	itemData["scene"] = strGatherID.GetString();
	itemData["width"] = kQRCodeWidth;
	itemData["page"] = m_strMiniPath.c_str();
	strPost = itemData.toStyledString().c_str();
	// 直接调用API接口获取小程序码的图片数据...
	strUrl.Format("https://api.weixin.qq.com/wxa/getwxacodeunlimit?access_token=%s", m_strMiniToken.c_str());
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
		// 这里需要处理网站返回的数据 => access_token...
		res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, miniPostCurl);
		res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this);
		res = curl_easy_perform(curl);
	}while( false );
	// 释放资源...
	if( curl != NULL ) {
		curl_easy_cleanup(curl);
	}
	// 如果缓存长度大于1K，则认为是图片数据，不用解析...
	if( m_strUTF8Data.size() >= 1024 ) {
		m_strNotice = "成功获取小程序码，正在显示...";
		m_strMiniPicCode = m_strUTF8Data;
		m_strUTF8Data.clear();
		this->Invalidate();
		return true;
	}
	Json::Value value;
	// 解析JSON失败，通知界面层...
	if( !this->parseJson(value, true) ) {
		MsgLogGM(GM_NotImplement);
		this->Invalidate();
		return false;
	}
	return true;
}
//
// 发送解除绑定命令...
BOOL CDlgMini::doWebUnBindMini()
{
	// 更新提示信息内容 => 并更新到窗口...
	m_strNotice = "正在解除绑定...";
	this->Invalidate();
	// 需要准备gather_id和user_id => 都是中心服务器的编号...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	int nDBHaoYiUserID = theConfig.GetDBHaoYiUserID();
	int nDBHaoYiGatherID = theConfig.GetDBHaoYiGatherID();
	if( nDBHaoYiGatherID <= 0 || nDBHaoYiUserID <= 0 ) {
		m_strNotice = "采集端没有注册成功，无法解除绑定";
		MsgLogGM(GM_NotImplement);
		this->Invalidate();
		return false;
	}
	// 清空上一次遗留的缓存...
	m_strUTF8Data.clear();
	// 准备需要的汇报数据 => POST数据包...
	CString strPost, strUrl;
	strPost.Format("gather_id=%d&user_id=%d", nDBHaoYiGatherID, nDBHaoYiUserID);
	// 这里需要用到 https 模式，因为，myhaoyi.com 全站都用 https 模式...
	strUrl.Format("%s/wxapi.php/Mini/unbindGather", DEF_WEB_HOME);
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
		// 这里需要处理网站返回的数据 => access_token...
		res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, miniPostCurl);
		res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this);
		res = curl_easy_perform(curl);
	}while( false );
	// 释放资源...
	if( curl != NULL ) {
		curl_easy_cleanup(curl);
	}
	Json::Value value;
	// 解析JSON失败，通知界面层...
	if( !this->parseJson(value, false) ) {
		MsgLogGM(GM_NotImplement);
		this->Invalidate();
		return false;
	}
	// 解除绑定成功，重置全局绑定用户编号...
	theConfig.SetDBHaoYiUserID(-1);
	return true;
}
//
// 获取图片线程...
void CDlgMini::Entry()
{
	// 首先，获取小程序access_token数据包...
	m_eMiniState = kMiniToken;
	if( !this->doWebGetMiniToken() )
		return;
	// 根据获取的用户编号进行处理...
	if( m_nBindUserID > 0 ) {
		// 如果已经绑定了微信用户，获取头像...
		m_eMiniState = kMiniHead;
		this->doWebGetUserHead();
	} else {
		// 如果没有绑定，获取二维码...
		m_eMiniState = kMiniCode;
		this->doWebGetMiniCode();
	}
}
//
// 响应窗口销毁事件...
void CDlgMini::OnDestroy()
{
	// 停止线程，等待退出...
	this->StopAndWaitForThread();
	// 销毁解除绑定按钮...
	this->DestoryUnBindButton();
	// 重置相关变量...
	this->ResetData();
	// 调用父窗口的销毁函数...
	__super::OnDestroy();
}
//
// 响应点击右上角关闭事件...
void CDlgMini::OnClose()
{
	__super::OnClose();
	__super::OnCancel();
}
//
// 屏蔽点击ESC键的事件...
void CDlgMini::OnCancel()
{
}
//
// 屏蔽点击回车键的事件...
void CDlgMini::OnOK()
{
}
