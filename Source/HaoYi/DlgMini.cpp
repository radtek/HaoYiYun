
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
// ��ÿ�η��������ݽ����ۼ� => �п���һ������������Ƿֿ����͵�...
void CDlgMini::doPostCurl(char * pData, size_t nSize)
{
	m_strUTF8Data.append(pData, nSize);
	//TRACE("Curl: %s\n", pData);
	//TRACE����г������ƣ�̫����ض�...
}
//
// ����JSON����ͷ��Ϣ => �����΢��API���������� errcode �� errmsg...
BOOL CDlgMini::parseJson(Json::Value & outValue, BOOL bIsWeiXin)
{
	// �жϻ�ȡ�������Ƿ���Ч...
	if( m_strUTF8Data.size() <= 0 ) {
		m_strNotice = "û�л�ȡ����Ч����";
		MsgLogGM(GM_Err_Json);
		return false;
	}
	Json::Reader reader;
	// ��UTF8��վ����ת����ANSI��ʽ => ������php������ģ�ת������Ч����ȡ��������֮�󣬻�Ҫת��һ��...
	string strANSIData = m_strUTF8Data; //CUtilTool::UTF8_ANSI(m_strUTF8Data.c_str());
	// ����ת�����json���ݰ�...
	if( !reader.parse(strANSIData, outValue) ) {
		m_strNotice = "������ȡ����JSON����ʧ��";
		MsgLogGM(GM_Err_Json);
		return false;
	}
	// ��ȡ���صĲɼ��˱�źʹ�����Ϣ...
	LPCTSTR lpszCode = bIsWeiXin ? "errcode" : "err_code";
	LPCTSTR lpszMsg = bIsWeiXin ? "errmsg" : "err_msg";
	if( outValue[lpszCode].asBool() ) {
		string & strData = outValue[lpszMsg].asString();
		string & strMsg = CUtilTool::UTF8_ANSI(strData.c_str());
		string & strCode = CUtilTool::getJsonString(outValue[lpszCode]);
		m_strNotice.Format("�����룺%s��%s", strCode.c_str(), strMsg.c_str());
		MsgLogINFO(m_strNotice);
		return false;
	}
	// û�д��󣬷�����ȷ...
	return true;
}
//
// ��Ӧ��С���������¼�...
LRESULT	CDlgMini::OnMsgBindMini(WPARAM wParam, LPARAM lParam)
{
	// ���������ź��û����...
	m_nBindUserID = (int)lParam;
	m_eBindCmd = (BIND_CMD)wParam;
	// ��״̬���µ������...
	this->Invalidate();
	// ���ݴ��ݵ�������һЩ����...
	if( m_eBindCmd == kSaveCmd ) {
		// �����û����浽ȫ�ֶ���...
		CXmlConfig & theConfig = CXmlConfig::GMInstance();
		theConfig.SetDBHaoYiUserID(m_nBindUserID);
		// ��ȡ�û�ͷ��ͼƬ����...
		this->doWebGetUserHead();
	} else if( m_eBindCmd == kCancelCmd ) {
		// �����ȡ�����������ؽ���󶨰�ť...
		m_btnUnBind.ShowWindow(SW_HIDE);
	}
	return S_OK;
}
//
// ��Ӧ��С���������¼�...
LRESULT	CDlgMini::OnMsgUnBindMini(WPARAM wParam, LPARAM lParam)
{
	// ����ȫ�������ݱ���...
	this->ResetData();
	// ע�⣺���ﲻ�ܵ��ý���󶨽ӿڣ���С���򴥷���...
	// ����󶨳ɹ�������ȫ�ְ��û����...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.SetDBHaoYiUserID(-1);
	// ���ؽ���󶨰�ť...
	m_btnUnBind.ShowWindow(SW_HIDE);
	// ���������߳�...
	this->Start();
	return S_OK;
}
//
// ��������󶨰�ť...
void CDlgMini::CreateUnBindButton()
{
	this->DestoryUnBindButton();

	CRect rcButton;
	rcButton.SetRect(20, 20, 20 + kBtnWidth, 20 + kBtnHeight);
	m_btnUnBind.Create("�����", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonUnBind);
	(m_lpBtnFont != NULL) ? m_btnUnBind.SetFont(m_lpBtnFont, true) : NULL;
	m_btnUnBind.ShowWindow(SW_HIDE);
}
//
// ���ٽ���󶨰�ť...
void CDlgMini::DestoryUnBindButton()
{
	m_btnUnBind.DestroyWindow();
}
//
// �������󶨰�ť�Ĳ���...
void CDlgMini::OnClickUnBindButton()
{
	// ����ȫ�������ݱ���...
	this->ResetData();
	// ���ý���󶨽ӿ�...
	this->doWebUnBindMini();
	// ���ؽ���󶨰�ť...
	m_btnUnBind.ShowWindow(SW_HIDE);
	// ���������߳�...
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
	// �ȴ�������󶨰�ť...
	this->CreateUnBindButton();
	// ����ȫ�������ݱ���...
	this->ResetData();
	// �趨���ڴ�С => 500 * 450 => ��ҪԤ�ȼ������±߿�...
	CRect rcWndRect;
	this->GetWindowRect(rcWndRect);
	rcWndRect.SetRect(0, 0, 506, 476);
	// �ƶ��������ھ���...
	this->MoveWindow(rcWndRect);
	this->CenterWindow();
	// �����߳�...
	this->Start();
	return true;
}
//
// ��ʾ��ȡaccess_token...
void CDlgMini::DispMiniToken(CDC * pDC)
{
	// ��ȡ���ƿͻ�������...
	CRect rcRect, txtRect;
	this->GetClientRect(&rcRect);
	// ������ʾ��Ϣ��ʾλ��...
	int nTxtHeight = 22;
	txtRect = rcRect;
	txtRect.top = rcRect.Height() / 2 - nTxtHeight * 2;
	txtRect.bottom = txtRect.top + nTxtHeight * 3;
	txtRect.DeflateRect(20, 0);
	// ��ָ��������ʾ��ʾ��Ϣ...
	this->ShowText(pDC, m_strNotice, txtRect, RGB(0,0,0), 20, true);
}
//
// ��ʾС�����룬�ڲ��ж�...
void CDlgMini::DispMiniCode(CDC * pDC)
{
	// ���С�����������Ϊ�գ�ֱ����ʾ������Ϣ...
	if( m_strMiniPicCode.size() <= 0 ) {
		this->DispMiniToken(pDC);
		return;
	}
	// ��ȡ���ƿͻ�������...
	CString strQRNotice;
	CRect rcRect, qrRect, txtRect;
	this->GetClientRect(&rcRect);
	ASSERT( m_strMiniPicCode.size() > 0);
	// ��С�����������ʾ...
	qrRect.left = (rcRect.Width() - kQRCodeWidth) / 2;
	qrRect.top = (rcRect.Height() - kQRCodeWidth) / 2 - 40;
	qrRect.right = qrRect.left + kQRCodeWidth;
	qrRect.bottom = qrRect.top + kQRCodeWidth;
	// ��ʾС�������ͼƬ������...
	this->DrawImage(pDC, m_strMiniPicCode, qrRect);
	// ������ʾ��Ϣ��ʾλ��...
	txtRect = rcRect;
	txtRect.DeflateRect(20, 0);
	txtRect.top = qrRect.bottom + 40;
	txtRect.bottom = txtRect.top + 20;
	// ���ݰ������ʾ��ͬ����ʾ...
	if( m_eBindCmd == kScanCmd ) {
		// ɨ��ɹ�֮����¼�֪ͨ...
		strQRNotice = "ɨ��ɹ�������΢���е����ȷ�ϰ󶨡�";
		this->ShowText(pDC, strQRNotice, txtRect, RGB(0,0,0), 18, true);
	} else if( m_eBindCmd == kSaveCmd ) {
		// �󶨳ɹ�֮����¼�֪ͨ...
		strQRNotice = "�Ѿ��󶨳ɹ���������ת...";
		this->ShowText(pDC, strQRNotice, txtRect, RGB(0,0,0), 18, true);
	} else if( m_eBindCmd == kCancelCmd ) {
		// ȡ����֮����¼�֪ͨ...
		strQRNotice = "����ȡ���˴ΰ󶨣������ٴ�ɨ��󶨣���رմ���";
		txtRect.top = qrRect.bottom + 30;
		txtRect.bottom = txtRect.top + 50;
		txtRect.DeflateRect(100, 0);
		this->ShowText(pDC, strQRNotice, txtRect, RGB(235,87,34), 18, true);
	} else {
		// Ĭ���������ʾ��Ϣ...
		strQRNotice = "����΢��ɨ���С����";
		this->ShowText(pDC, strQRNotice, txtRect, RGB(0,0,0), 18, true);
	}
}
//
// ��ʾ�û�ͷ���ڲ��ж�...
void CDlgMini::DispMiniHead(CDC * pDC)
{
	// ����û�ͷ�������Ϊ�գ�ֱ����ʾ������Ϣ...
	if( m_strMiniPicHead.size() <= 0 ) {
		this->DispMiniToken(pDC);
		return;
	}
	// ��ȡ���ƿͻ�������...
	CString theTxtInfo;
	CRect rcRect, qrRect, txtRect, rcButton;
	this->GetClientRect(&rcRect);
	ASSERT( m_strMiniPicHead.size() > 0);
	// ��ͷ�������ʾ...
	qrRect.left = (rcRect.Width() - kQRHeadWidth) / 2;
	qrRect.top = (rcRect.Height() - kQRHeadWidth) / 2 - 100;
	qrRect.right = qrRect.left + kQRHeadWidth;
	qrRect.bottom = qrRect.top + kQRHeadWidth;
	// ��ʾ�û�ͷ���ͼƬ������...
	this->DrawImage(pDC, m_strMiniPicHead, qrRect);
	// �����û��ǳ���ʾλ��...
	txtRect = rcRect;
	txtRect.DeflateRect(40, 0);
	txtRect.top = qrRect.bottom + 20;
	txtRect.bottom = txtRect.top + 25;
	theTxtInfo = m_strUserName.c_str();
	this->ShowText(pDC, theTxtInfo, txtRect, RGB(0,0,0), 20, true);
	// �������󶨰�ť��ʾλ��...
	rcButton.left = (rcRect.Width() - kBtnWidth) / 2;
	rcButton.right = rcButton.left + kBtnWidth;
	rcButton.top = txtRect.bottom + 20;
	rcButton.bottom = rcButton.top + kBtnHeight;
	m_btnUnBind.MoveWindow(rcButton);
	m_btnUnBind.ShowWindow(SW_SHOW);
	// ��ʾ������ʾ��Ϣ...
	txtRect = rcRect;
	txtRect.DeflateRect(20, 0);
	txtRect.top = rcButton.bottom + 30;
	txtRect.bottom = txtRect.top + 18;
	theTxtInfo = "������ɰ󶨲��������ɵ��������󶨡����°󶨣���رմ���";
	this->ShowText(pDC, theTxtInfo, txtRect, RGB(51,51,51), 14, false);
	txtRect.top = txtRect.bottom + 8;
	txtRect.bottom = txtRect.top + 18;
	theTxtInfo = "�����ڿ�����΢��С��������ɼ���";
	this->ShowText(pDC, theTxtInfo, txtRect, RGB(51,51,51), 14, false);
}
//
// ���Ʊ���...
BOOL CDlgMini::OnEraseBkgnd(CDC* pDC)
{
	// ͳһ���Ʊ�����ɫ...
	CRect rcRect;
	this->GetClientRect(&rcRect);
	pDC->FillSolidRect(rcRect, RGB(255, 255, 255));//RGB(242, 245, 233));
	// ����״̬��ʾ��ͬ����Ϣ...
	if( m_eMiniState == kMiniToken ) {
		// ��ʾ��ȡaccess_token״̬...
		this->DispMiniToken(pDC);
	} else if( m_eMiniState == kMiniCode ) {
		// ��ʾ��ȡС���������...
		if( m_eBindCmd == kSaveCmd ) {
			// �󶨳ɹ�֮����ʾͷ��...
			this->DispMiniHead(pDC);
		} else {
			// ���ڰ󶨣���ʾС������...
			this->DispMiniCode(pDC);
		}
	} else if( m_eMiniState == kMiniHead ) {
		this->DispMiniHead(pDC);
	}
	return true;
}
//
// ��ʾС�������ͼƬ����...
void CDlgMini::DrawImage(CDC * pDC, string & inImgData, CRect & inDispRect)
{
	// �������Ϊ�գ�ֱ�ӷ���...
	if( inImgData.size() <= 0 )
		return;
	// ����һ��ȫ�ֻ�����...
	IStream * pIStream = NULL;
	LONG      cbSrcSize = inImgData.size();
	LPVOID	  lpSrcData = (LPVOID)inImgData.c_str();
	HGLOBAL   hGlobal = GlobalAlloc(GMEM_MOVEABLE, cbSrcSize);   
	// �õ��ڴ�ָ��...
	LPVOID    pvData = NULL;
	ASSERT( hGlobal != NULL );
	pvData = GlobalLock(hGlobal);
	ASSERT( pvData != NULL );
	// �������ݣ���������...
	memcpy(pvData, lpSrcData, cbSrcSize);   
	GlobalUnlock(hGlobal);
	// �����������ӿڶ���...
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
	// �ͷ�֮ǰ�������Դ...
	GlobalFree(hGlobal); hGlobal = NULL;
	nCount = ((pIStream != NULL) ? pIStream->Release() : 0);
	nCount = ((pIPicObj != NULL) ? pIPicObj->Release() : 0);
}
//
// ��ʾ������Ϣ => ��ʾ����������ɫ�������С���Ƿ����...
void CDlgMini::ShowText(CDC * pDC, CString & inTxtData, CRect & inDispRect, COLORREF inTxtColor, int inFontSize, bool isBold)
{
	// �������Ϊ�գ�ֱ�ӷ���...
	if( inTxtData.GetLength() <= 0 )
		return;
	// ׼���������...
	CFont   fontLogo;
	CFont * pOldFont = NULL;
	pDC->SetBkMode(TRANSPARENT);
	// �������� => ��������Ĳ�������...
	fontLogo.CreateFont(inFontSize, 0, 0, 0, (isBold ? FW_BOLD : FW_LIGHT), false, false,0,0,0,0,0,0, "����");
	// �������������������ɫ...
	pOldFont = pDC->SelectObject(&fontLogo);
	pDC->SetTextColor( inTxtColor );
	// ����������Ϣ => ������ʾ...
	pDC->DrawText(inTxtData, inDispRect, DT_EXTERNALLEADING | DT_WORDBREAK | DT_CENTER | DT_VCENTER);
	// ��������...
	pDC->SelectObject(pOldFont);
	fontLogo.DeleteObject();
	fontLogo.Detach();
}
//
// ��ȡ���û���ͷ������...
BOOL CDlgMini::doWebGetUserHead()
{
	// ������ʾ��Ϣ���� => �����µ�����...
	m_strNotice = "���ڻ�ȡ�û�ͷ��...";
	this->Invalidate();
	// �ж��û�ͷ�������Ƿ�Ϊ��...
	if( m_strUserHead.size() <= 0 ) {
		m_strNotice = "�Ѿ����û�ͷ���ַ��Ч...";
		MsgLogGM(GM_NotImplement);
		this->Invalidate();
		return false;
	}
	// �����һ�������Ļ���...
	m_strUTF8Data.clear();
	// ΢��ͷ������Ӷ���https��ַ...
	CString strUrl = m_strUserHead.c_str();
	// ����Curl�ӿڣ��㱨����ͷ����...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if( curl == NULL )
			break;
		// �����https://Э�飬��Ҫ��������...
		res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
		res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		// �趨curl����������getģʽ...
		res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		res = curl_easy_setopt(curl, CURLOPT_HEADER, false);
		res = curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
		res = curl_easy_setopt(curl, CURLOPT_URL, strUrl);
		// ������Ҫ������վ���ص�����...
		res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, miniPostCurl);
		res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this);
		res = curl_easy_perform(curl);
	} while ( false );
	// �ͷ���Դ...
	if( curl != NULL ) {
		curl_easy_cleanup(curl);
	}
	// ������泤��Ϊ0����ȡʧ��...
	if( m_strUTF8Data.size() <= 0 ) {
		m_strNotice = "��ȡ���û�ͷ��ʧ��...";
		MsgLogGM(GM_NotImplement);
		return false;
	}
	// ��ȡͷ��ͼƬ���ݳɹ� => �������������µ�����...
	m_strNotice = "��ȡ���û�ͷ��ɹ���������ʾ...";
	m_strMiniPicHead = m_strUTF8Data;
	m_strUTF8Data.clear();
	this->Invalidate();
	return true;
}
//
// ��ȡС�����access_token...
BOOL CDlgMini::doWebGetMiniToken()
{
	// ������ʾ��Ϣ���� => �����µ�����...
	m_strNotice = "���ڻ�ȡ����ƾ֤...";
	this->Invalidate();
	// ��ȡ��վ������Ϣ => GatherID�����ķ������еı��...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	int nDBHaoYiGatherID = theConfig.GetDBHaoYiGatherID();
	if( nDBHaoYiGatherID <= 0  ) {
		m_strNotice = "�ɼ���û��ע��ɹ����޷���С����";
		MsgLogGM(GM_NotImplement);
		this->Invalidate();
		return false;
	}
	// �����һ�������Ļ���...
 	m_strUTF8Data.clear();
	// ׼����Ҫ�Ļ㱨���� => POST���ݰ�...
	// ���� miniName ���� => С��������...
	CString strPost, strUrl;
	// 2018.03.09 - by jackey => �ɼ���ֻ�ܰ�һ��С����Ŀǰ�ǰ󶨵�"��һ�Ʒ���"��С������...
	strPost.Format("gather_id=%d&miniName=%s", nDBHaoYiGatherID, "device");
	//strPost.Format("gather_id=%d&miniName=%s", nDBHaoYiGatherID, "cloud");
	// ������Ҫ�õ� https ģʽ����Ϊ��myhaoyi.com ȫվ���� https ģʽ...
	strUrl.Format("%s/wxapi.php/Mini/getToken", DEF_WEB_HOME);
	// ����Curl�ӿڣ��㱨����ͷ����...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if( curl == NULL )
			break;
		// �����https://Э�飬��Ҫ��������...
		res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
		res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		// �趨curl����������postģʽ...
		res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strPost);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strPost.GetLength());
		res = curl_easy_setopt(curl, CURLOPT_HEADER, false);
		res = curl_easy_setopt(curl, CURLOPT_POST, true);
		res = curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
		res = curl_easy_setopt(curl, CURLOPT_URL, strUrl);
		// ������Ҫ������վ���ص����� => access_token...
		res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, miniPostCurl);
		res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this);
		res = curl_easy_perform(curl);
	}while( false );
	// �ͷ���Դ...
	if( curl != NULL ) {
		curl_easy_cleanup(curl);
	}
	Json::Value value;
	// ����JSONʧ�ܣ�֪ͨ�����...
	if( !this->parseJson(value, false) ) {
		MsgLogGM(GM_NotImplement);
		this->Invalidate();
		return false;
	}
	// ����JSON�ɹ�����һ���������� => ������Ҫת����ANSI��ʽ...
	m_nBindUserID = atoi(CUtilTool::getJsonString(value["user_id"]).c_str());
	m_strMiniToken = CUtilTool::getJsonString(value["access_token"]);
	m_strUserName = CUtilTool::UTF8_ANSI(CUtilTool::getJsonString(value["user_name"]).c_str());
	m_strUserHead = CUtilTool::getJsonString(value["user_head"]);
	m_strMiniPath = CUtilTool::getJsonString(value["mini_path"]);
	// ����ȡ���İ��û���Ÿ��µ�ȫ�������ļ�����...
	theConfig.SetDBHaoYiUserID(m_nBindUserID);
	return true;
}
//
// ��ȡ���ɼ�����С�����е�С������...
BOOL CDlgMini::doWebGetMiniCode()
{
	// ������ʾ��Ϣ���� => �����µ�����...
	m_strNotice = "���ڻ�ȡС������...";
	this->Invalidate();
	// ��ȡ��վ������Ϣ => GatherID�����ķ������еı��...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	int nDBHaoYiGatherID = theConfig.GetDBHaoYiGatherID();
	if( nDBHaoYiGatherID <= 0  ) {
		m_strNotice = "�ɼ���û��ע��ɹ����޷���С����";
		MsgLogGM(GM_NotImplement);
		this->Invalidate();
		return false;
	}
	// �ж�access_token��path�Ƿ���Ч...
	if( m_strMiniPath.size() <= 0 || m_strMiniToken.size() <= 0 ) {
		m_strNotice = "��ȡaccess_tokenʧ�ܣ��޷���ȡС������";
		MsgLogGM(GM_NotImplement);
		this->Invalidate();
		return false;
	}
	// �����һ�������Ļ���...
	m_strUTF8Data.clear();
	// ׼����Ҫ�Ļ㱨���� => POST���ݰ�...
	Json::Value itemData;
	CString strPost, strUrl, strGatherID;
	strGatherID.Format("%d", nDBHaoYiGatherID);
	itemData["scene"] = strGatherID.GetString();
	itemData["width"] = kQRCodeWidth;
	itemData["page"] = m_strMiniPath.c_str();
	strPost = itemData.toStyledString().c_str();
	// ֱ�ӵ���API�ӿڻ�ȡС�������ͼƬ����...
	strUrl.Format("https://api.weixin.qq.com/wxa/getwxacodeunlimit?access_token=%s", m_strMiniToken.c_str());
	// ����Curl�ӿڣ��㱨����ͷ����...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if( curl == NULL )
			break;
		// �����https://Э�飬��Ҫ��������...
		res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
		res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		// �趨curl����������postģʽ...
		res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strPost);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strPost.GetLength());
		res = curl_easy_setopt(curl, CURLOPT_HEADER, false);
		res = curl_easy_setopt(curl, CURLOPT_POST, true);
		res = curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
		res = curl_easy_setopt(curl, CURLOPT_URL, strUrl);
		// ������Ҫ������վ���ص����� => access_token...
		res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, miniPostCurl);
		res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this);
		res = curl_easy_perform(curl);
	}while( false );
	// �ͷ���Դ...
	if( curl != NULL ) {
		curl_easy_cleanup(curl);
	}
	// ������泤�ȴ���1K������Ϊ��ͼƬ���ݣ����ý���...
	if( m_strUTF8Data.size() >= 1024 ) {
		m_strNotice = "�ɹ���ȡС�����룬������ʾ...";
		m_strMiniPicCode = m_strUTF8Data;
		m_strUTF8Data.clear();
		this->Invalidate();
		return true;
	}
	Json::Value value;
	// ����JSONʧ�ܣ�֪ͨ�����...
	if( !this->parseJson(value, true) ) {
		MsgLogGM(GM_NotImplement);
		this->Invalidate();
		return false;
	}
	return true;
}
//
// ���ͽ��������...
BOOL CDlgMini::doWebUnBindMini()
{
	// ������ʾ��Ϣ���� => �����µ�����...
	m_strNotice = "���ڽ����...";
	this->Invalidate();
	// ��Ҫ׼��gather_id��user_id => �������ķ������ı��...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	int nDBHaoYiUserID = theConfig.GetDBHaoYiUserID();
	int nDBHaoYiGatherID = theConfig.GetDBHaoYiGatherID();
	if( nDBHaoYiGatherID <= 0 || nDBHaoYiUserID <= 0 ) {
		m_strNotice = "�ɼ���û��ע��ɹ����޷������";
		MsgLogGM(GM_NotImplement);
		this->Invalidate();
		return false;
	}
	// �����һ�������Ļ���...
	m_strUTF8Data.clear();
	// ׼����Ҫ�Ļ㱨���� => POST���ݰ�...
	CString strPost, strUrl;
	strPost.Format("gather_id=%d&user_id=%d", nDBHaoYiGatherID, nDBHaoYiUserID);
	// ������Ҫ�õ� https ģʽ����Ϊ��myhaoyi.com ȫվ���� https ģʽ...
	strUrl.Format("%s/wxapi.php/Mini/unbindGather", DEF_WEB_HOME);
	// ����Curl�ӿڣ��㱨����ͷ����...
	CURLcode res = CURLE_OK;
	CURL  *  curl = curl_easy_init();
	do {
		if( curl == NULL )
			break;
		// �����https://Э�飬��Ҫ��������...
		res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
		res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		// �趨curl����������postģʽ...
		res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strPost);
		res = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strPost.GetLength());
		res = curl_easy_setopt(curl, CURLOPT_HEADER, false);
		res = curl_easy_setopt(curl, CURLOPT_POST, true);
		res = curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
		res = curl_easy_setopt(curl, CURLOPT_URL, strUrl);
		// ������Ҫ������վ���ص����� => access_token...
		res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, miniPostCurl);
		res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this);
		res = curl_easy_perform(curl);
	}while( false );
	// �ͷ���Դ...
	if( curl != NULL ) {
		curl_easy_cleanup(curl);
	}
	Json::Value value;
	// ����JSONʧ�ܣ�֪ͨ�����...
	if( !this->parseJson(value, false) ) {
		MsgLogGM(GM_NotImplement);
		this->Invalidate();
		return false;
	}
	// ����󶨳ɹ�������ȫ�ְ��û����...
	theConfig.SetDBHaoYiUserID(-1);
	return true;
}
//
// ��ȡͼƬ�߳�...
void CDlgMini::Entry()
{
	// ���ȣ���ȡС����access_token���ݰ�...
	m_eMiniState = kMiniToken;
	if( !this->doWebGetMiniToken() )
		return;
	// ���ݻ�ȡ���û���Ž��д���...
	if( m_nBindUserID > 0 ) {
		// ����Ѿ�����΢���û�����ȡͷ��...
		m_eMiniState = kMiniHead;
		this->doWebGetUserHead();
	} else {
		// ���û�а󶨣���ȡ��ά��...
		m_eMiniState = kMiniCode;
		this->doWebGetMiniCode();
	}
}
//
// ��Ӧ���������¼�...
void CDlgMini::OnDestroy()
{
	// ֹͣ�̣߳��ȴ��˳�...
	this->StopAndWaitForThread();
	// ���ٽ���󶨰�ť...
	this->DestoryUnBindButton();
	// ������ر���...
	this->ResetData();
	// ���ø����ڵ����ٺ���...
	__super::OnDestroy();
}
//
// ��Ӧ������Ͻǹر��¼�...
void CDlgMini::OnClose()
{
	__super::OnClose();
	__super::OnCancel();
}
//
// ���ε��ESC�����¼�...
void CDlgMini::OnCancel()
{
}
//
// ���ε���س������¼�...
void CDlgMini::OnOK()
{
}
