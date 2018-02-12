
#include "stdafx.h"
#include "HaoYi.h"
#include "DlgPushDVR.h"
#include "StringParser.h"
#include "XmlConfig.h"
#include "UtilTool.h"
#include "LibMP4.h"
#include "md5.h"

IMPLEMENT_DYNAMIC(CDlgPushDVR, CDialogEx)

CDlgPushDVR::CDlgPushDVR(BOOL bIsEdit, int nDBCameraID, BOOL bIsLogin, CWnd* pParent/* = NULL*/)
  : CDialogEx(CDlgPushDVR::IDD, pParent)
  , m_nDBCameraID(nDBCameraID)
  , m_bIsLogin(bIsLogin)
  , m_bEditMode(bIsEdit)
  , m_bFileMode(false)
  , m_bFileLoop(false)
  , m_bPushAuto(false)
  , m_bUseTCP(false)
  , m_strDVRName(_T(""))
  , m_strRtspURL(_T(""))
  , m_strMP4File(_T(""))
{
}

CDlgPushDVR::~CDlgPushDVR()
{
}

void CDlgPushDVR::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_RTSP, m_strRtspURL);
	DDX_Text(pDX, IDC_EDIT_MP4, m_strMP4File);
	DDX_Check(pDX, IDC_CHECK_LOOP, m_bFileLoop);
	DDX_Check(pDX, IDC_CHECK_AUTO, m_bPushAuto);
	DDX_Check(pDX, IDC_RTSP_USE_TCP, m_bUseTCP);
	DDX_Text(pDX, IDC_EDIT_NAME, m_strDVRName);
	DDV_MaxChars(pDX, m_strDVRName, 64);
}

BEGIN_MESSAGE_MAP(CDlgPushDVR, CDialogEx)
	ON_BN_CLICKED(IDC_RADIO_RTSP, &CDlgPushDVR::OnBnClickedRadioRtsp)
	ON_BN_CLICKED(IDC_RADIO_FILE, &CDlgPushDVR::OnBnClickedRadioFile)
	ON_BN_CLICKED(IDC_BTN_CHOOSE, &CDlgPushDVR::OnBtnChoose)
	ON_BN_CLICKED(IDOK, &CDlgPushDVR::OnBnClickedOk)
END_MESSAGE_MAP()

void CDlgPushDVR::OnBnClickedOk()
{
	if( !this->UpdateData() )
		return;
	// 判断通道名称不能为空...
	if( m_strDVRName.IsEmpty() ) {
		MessageBox("[通道名称] 不能为空，请重新输入！", "错误", MB_OK|MB_ICONSTOP);
		GetDlgItem(IDC_EDIT_NAME)->SetFocus();
		return;
	}
	if( m_bFileMode ) {
		// 文件模式...
		if( m_strMP4File.IsEmpty() ) {
			MessageBox("[MP4文件] 不能为空，请重新输入！", "错误", MB_OK|MB_ICONSTOP);
			GetDlgItem(IDC_EDIT_MP4)->SetFocus();
			return;
		}
		// 文件是否有效...
		if( _access(m_strMP4File, 0) < 0 ) {
			MessageBox("[MP4文件] 文件无法访问，请重新输入！", "错误", MB_OK|MB_ICONSTOP);
			GetDlgItem(IDC_EDIT_MP4)->SetFocus();
			return;
		}
	} else {
		// RTSP模式...
		if( m_strRtspURL.IsEmpty() ) {
			MessageBox("[拉流地址] 不能为空，请重新输入！", "错误", MB_OK|MB_ICONSTOP);
			GetDlgItem(IDC_EDIT_RTSP)->SetFocus();
			return;
		}
		// 检测URL是否有效...	
		if( !this->CheckRtspURL(m_strRtspURL) ) {
			MessageBox("[拉流地址] 格式错误，请重新输入！", "错误", MB_OK|MB_ICONSTOP);
			GetDlgItem(IDC_EDIT_RTSP)->SetFocus();
			return;
		}
	}

	this->BuildPushRecord();

	CDialogEx::OnOK();
}

void CDlgPushDVR::BuildPushRecord()
{
	CString	strItem;
	strItem.Format("%d", m_bFileMode ? kStreamMP4File : kStreamUrlLink);
	m_MapData["stream_prop"] = strItem;
	m_MapData["stream_url"] = m_strRtspURL;
	m_MapData["stream_mp4"] = m_strMP4File;
	m_MapData["camera_name"] = m_strDVRName;
	strItem.Format("%d", m_bFileLoop);
	m_MapData["stream_loop"] = strItem;
	strItem.Format("%d", m_bPushAuto);
	m_MapData["stream_auto"] = strItem;
	strItem.Format("%d", m_bUseTCP);
	m_MapData["use_tcp"] = strItem;
	// 如果是编辑模式，保存配置，直接返回...
	if( m_bEditMode ) {
		CXmlConfig & theConfig = CXmlConfig::GMInstance();
		theConfig.SetCamera(m_nDBCameraID, m_MapData);
		return;
	}
	ASSERT( !m_bEditMode );
	// 如果是添加模式，需要创建 DeviceSN 唯一标识数据...
	MD5	    md5;
	CString strTimeMicro;
	ULARGE_INTEGER	llTimCountCur = {0};
	::GetSystemTimeAsFileTime((FILETIME *)&llTimCountCur);
	strTimeMicro.Format("%I64d", llTimCountCur.QuadPart);
	md5.update(strTimeMicro, strTimeMicro.GetLength());
	m_MapData["device_sn"] = md5.toString();
	// 其它变量由通道创建是构造...
}

BOOL CDlgPushDVR::OnInitDialog()
{
	// 修改标题名称...
	CString	strTitle, strNew;
	this->GetWindowText(strTitle);
	strNew.Format(strTitle, m_bEditMode ? "修改" : "添加");
	this->SetWindowText(strNew);
	// 设置图标...
	this->SetIcon(AfxGetApp()->LoadIcon(IDR_MAINFRAME), FALSE);
	this->UpdateData(false);
	// 添加时的默认操作...
	CButton * lpBtn = (CButton *)GetDlgItem(IDC_RADIO_RTSP);
	lpBtn->SetCheck(true);
	m_bFileMode = false;
	// 默认是拉流模式...
	GetDlgItem(IDC_EDIT_RTSP)->EnableWindow(true);
	GetDlgItem(IDC_EDIT_MP4)->EnableWindow(false);
	GetDlgItem(IDC_BTN_CHOOSE)->EnableWindow(false);
	GetDlgItem(IDC_CHECK_LOOP)->EnableWindow(false);
	GetDlgItem(IDC_CHECK_AUTO)->EnableWindow(true);
	GetDlgItem(IDC_RTSP_USE_TCP)->EnableWindow(true);

	// 修改时的操作...
	( m_bEditMode ) ? this->FillEditValue() : NULL;

	// 如果通道已登录，将按钮置灰...
	if( m_bIsLogin ) {
		CEdit * lpWndRtsp = (CEdit*)GetDlgItem(IDC_EDIT_RTSP);
		CEdit * lpWndName = (CEdit*)GetDlgItem(IDC_EDIT_NAME);
		lpWndRtsp->SetReadOnly(true);
		lpWndName->SetReadOnly(true);
		GetDlgItem(IDC_RTSP_USE_TCP)->EnableWindow(false);
		GetDlgItem(IDC_RADIO_RTSP)->EnableWindow(false);
		GetDlgItem(IDC_RADIO_FILE)->EnableWindow(false);
		GetDlgItem(IDC_CHECK_LOOP)->EnableWindow(false);
		GetDlgItem(IDC_CHECK_AUTO)->EnableWindow(false);
		GetDlgItem(IDC_BTN_CHOOSE)->EnableWindow(false);
		GetDlgItem(IDOK)->EnableWindow(false);
	}

	return TRUE;
}

void CDlgPushDVR::FillEditValue()
{
	if( m_nDBCameraID <= 0 )
		return;
	ASSERT( m_nDBCameraID > 0 );
	// 重新设置窗口名称...
	CString strTitle;
	strTitle.Format("ID：%d - 通道设置 - 修改", m_nDBCameraID);
	this->SetWindowText(strTitle);
	// 需要首先判断 stream_prop 是否有效...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetCamera(m_nDBCameraID, m_MapData);
	if( m_MapData.find("stream_prop") == m_MapData.end() )
		return;
	// 设置流转发模式 => MP4或URL...
	int nStreamProp = atoi(m_MapData["stream_prop"].c_str());
	m_bFileMode = ((nStreamProp == kStreamMP4File) ? true : false);
	ASSERT( nStreamProp == kStreamMP4File || nStreamProp == kStreamUrlLink );
	// 设置其它流数据信息...
	m_bUseTCP = atoi(m_MapData["use_tcp"].c_str());
	m_bPushAuto = atoi(m_MapData["stream_auto"].c_str());
	m_bFileLoop = atoi(m_MapData["stream_loop"].c_str());
	m_strRtspURL = m_MapData["stream_url"].c_str();
	m_strMP4File = m_MapData["stream_mp4"].c_str();
	m_strDVRName = m_MapData["camera_name"].c_str();
	// 根据获取的信息，设置元素状态...
	CButton * lpBtnRtsp = (CButton *)GetDlgItem(IDC_RADIO_RTSP);
	CButton * lpBtnFile = (CButton *)GetDlgItem(IDC_RADIO_FILE);
	if( m_bFileMode ) {
		lpBtnFile->SetCheck(true);
		lpBtnRtsp->SetCheck(false);
		this->OnBnClickedRadioFile();
	} else { 
		lpBtnFile->SetCheck(false);
		lpBtnRtsp->SetCheck(true);
		this->OnBnClickedRadioRtsp();
	}
	// 将数据写入各个元素表现层当中...
	this->UpdateData(FALSE);
}

BOOL CDlgPushDVR::CheckRtspURL(LPCTSTR lpszFullURL)
{
	// 解析输入的完整URL地址 => rtmp://IP:Port/URI
	if( lpszFullURL == NULL || strlen(lpszFullURL) <= 0 ) {
		return false;
	}
	// 协议开头是 rtsp:// 或 rtmp://
	if( (strnicmp("rtsp://", lpszFullURL, strlen("rtsp://")) != 0 ) && 
		(strnicmp("rtmp://", lpszFullURL, strlen("rtmp://")) != 0 ) ) {
		return false;
	}
	// 如果是 rtsp:// 协议，直接返回...
	if( strnicmp("rtsp://", lpszFullURL, strlen("rtsp://")) == 0 )
		return true;
	// 这里对 rtmp:// 协议，要严格一些...
	char * lpszData = (char *)lpszFullURL + strlen("rtmp://");
	StrPtrLen	 theKeyWord, theLive, theURI;
	StrPtrLen	 theData(lpszData, strlen(lpszData));
	StringParser theParse(&theData);
	if( !theParse.GetThru(&theKeyWord, '/') ) {
		return false;
	}
	// 得到链接地址和端口...
	if( theKeyWord.Ptr == NULL || theKeyWord.Len <= 0 ) {
		return false;
	}
	// 地址正确...
	return true;
}

void CDlgPushDVR::OnBtnChoose() 
{
	this->UpdateData();

	CFileDialog file(true, "");
	file.m_ofn.lpstrFilter = "*.mp4\0*.mp4\0"; //"*.wmv\0*.wmv\0*.asf\0*.asf\0";
	if( file.DoModal() != IDOK )
		return;
	CString strError;
	CString strFile = file.GetPathName();
	string strUTF8 = CUtilTool::ANSI_UTF8(strFile);
	MP4FileHandle mp4File = MP4Read( strUTF8.c_str() );
	if( mp4File == MP4_INVALID_FILE_HANDLE ) {
		MP4Close( mp4File );
		strError.Format("%s \n不是有效的MP4文件，请重新选择！", strFile);
		MessageBox(strError, "错误", MB_OK|MB_ICONSTOP);
		return;
	}
	// 释放资源，显示路径在对话框里面...
	MP4Close( mp4File );
	m_strMP4File = strFile;
	this->UpdateData(false);
}

void CDlgPushDVR::OnBnClickedRadioRtsp()
{
	m_bFileMode = false;
	GetDlgItem(IDC_RTSP_USE_TCP)->EnableWindow(true);
	GetDlgItem(IDC_CHECK_AUTO)->EnableWindow(true);
	GetDlgItem(IDC_EDIT_RTSP)->EnableWindow(true);
	GetDlgItem(IDC_EDIT_MP4)->EnableWindow(false);
	GetDlgItem(IDC_CHECK_LOOP)->EnableWindow(false);
	GetDlgItem(IDC_BTN_CHOOSE)->EnableWindow(false);
}

void CDlgPushDVR::OnBnClickedRadioFile()
{
	m_bFileMode = true;
	GetDlgItem(IDC_RTSP_USE_TCP)->EnableWindow(false);
	GetDlgItem(IDC_CHECK_AUTO)->EnableWindow(false);
	GetDlgItem(IDC_EDIT_RTSP)->EnableWindow(false);
	GetDlgItem(IDC_EDIT_MP4)->EnableWindow(true);
	GetDlgItem(IDC_CHECK_LOOP)->EnableWindow(true);
	GetDlgItem(IDC_BTN_CHOOSE)->EnableWindow(true);
}
