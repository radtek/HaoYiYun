
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
	// �ж�ͨ�����Ʋ���Ϊ��...
	if( m_strDVRName.IsEmpty() ) {
		MessageBox("[ͨ������] ����Ϊ�գ����������룡", "����", MB_OK|MB_ICONSTOP);
		GetDlgItem(IDC_EDIT_NAME)->SetFocus();
		return;
	}
	if( m_bFileMode ) {
		// �ļ�ģʽ...
		if( m_strMP4File.IsEmpty() ) {
			MessageBox("[MP4�ļ�] ����Ϊ�գ����������룡", "����", MB_OK|MB_ICONSTOP);
			GetDlgItem(IDC_EDIT_MP4)->SetFocus();
			return;
		}
		// �ļ��Ƿ���Ч...
		if( _access(m_strMP4File, 0) < 0 ) {
			MessageBox("[MP4�ļ�] �ļ��޷����ʣ����������룡", "����", MB_OK|MB_ICONSTOP);
			GetDlgItem(IDC_EDIT_MP4)->SetFocus();
			return;
		}
	} else {
		// RTSPģʽ...
		if( m_strRtspURL.IsEmpty() ) {
			MessageBox("[������ַ] ����Ϊ�գ����������룡", "����", MB_OK|MB_ICONSTOP);
			GetDlgItem(IDC_EDIT_RTSP)->SetFocus();
			return;
		}
		// ���URL�Ƿ���Ч...	
		if( !this->CheckRtspURL(m_strRtspURL) ) {
			MessageBox("[������ַ] ��ʽ�������������룡", "����", MB_OK|MB_ICONSTOP);
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
	// ����Ǳ༭ģʽ���������ã�ֱ�ӷ���...
	if( m_bEditMode ) {
		CXmlConfig & theConfig = CXmlConfig::GMInstance();
		theConfig.SetCamera(m_nDBCameraID, m_MapData);
		return;
	}
	ASSERT( !m_bEditMode );
	// ��������ģʽ����Ҫ���� DeviceSN Ψһ��ʶ����...
	MD5	    md5;
	CString strTimeMicro;
	ULARGE_INTEGER	llTimCountCur = {0};
	::GetSystemTimeAsFileTime((FILETIME *)&llTimCountCur);
	strTimeMicro.Format("%I64d", llTimCountCur.QuadPart);
	md5.update(strTimeMicro, strTimeMicro.GetLength());
	m_MapData["device_sn"] = md5.toString();
	// ����������ͨ�������ǹ���...
}

BOOL CDlgPushDVR::OnInitDialog()
{
	// �޸ı�������...
	CString	strTitle, strNew;
	this->GetWindowText(strTitle);
	strNew.Format(strTitle, m_bEditMode ? "�޸�" : "���");
	this->SetWindowText(strNew);
	// ����ͼ��...
	this->SetIcon(AfxGetApp()->LoadIcon(IDR_MAINFRAME), FALSE);
	this->UpdateData(false);
	// ���ʱ��Ĭ�ϲ���...
	CButton * lpBtn = (CButton *)GetDlgItem(IDC_RADIO_RTSP);
	lpBtn->SetCheck(true);
	m_bFileMode = false;
	// Ĭ��������ģʽ...
	GetDlgItem(IDC_EDIT_RTSP)->EnableWindow(true);
	GetDlgItem(IDC_EDIT_MP4)->EnableWindow(false);
	GetDlgItem(IDC_BTN_CHOOSE)->EnableWindow(false);
	GetDlgItem(IDC_CHECK_LOOP)->EnableWindow(false);
	GetDlgItem(IDC_CHECK_AUTO)->EnableWindow(true);
	GetDlgItem(IDC_RTSP_USE_TCP)->EnableWindow(true);

	// �޸�ʱ�Ĳ���...
	( m_bEditMode ) ? this->FillEditValue() : NULL;

	// ���ͨ���ѵ�¼������ť�û�...
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
	// �������ô�������...
	CString strTitle;
	strTitle.Format("ID��%d - ͨ������ - �޸�", m_nDBCameraID);
	this->SetWindowText(strTitle);
	// ��Ҫ�����ж� stream_prop �Ƿ���Ч...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetCamera(m_nDBCameraID, m_MapData);
	if( m_MapData.find("stream_prop") == m_MapData.end() )
		return;
	// ������ת��ģʽ => MP4��URL...
	int nStreamProp = atoi(m_MapData["stream_prop"].c_str());
	m_bFileMode = ((nStreamProp == kStreamMP4File) ? true : false);
	ASSERT( nStreamProp == kStreamMP4File || nStreamProp == kStreamUrlLink );
	// ����������������Ϣ...
	m_bUseTCP = atoi(m_MapData["use_tcp"].c_str());
	m_bPushAuto = atoi(m_MapData["stream_auto"].c_str());
	m_bFileLoop = atoi(m_MapData["stream_loop"].c_str());
	m_strRtspURL = m_MapData["stream_url"].c_str();
	m_strMP4File = m_MapData["stream_mp4"].c_str();
	m_strDVRName = m_MapData["camera_name"].c_str();
	// ���ݻ�ȡ����Ϣ������Ԫ��״̬...
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
	// ������д�����Ԫ�ر��ֲ㵱��...
	this->UpdateData(FALSE);
}

BOOL CDlgPushDVR::CheckRtspURL(LPCTSTR lpszFullURL)
{
	// �������������URL��ַ => rtmp://IP:Port/URI
	if( lpszFullURL == NULL || strlen(lpszFullURL) <= 0 ) {
		return false;
	}
	// Э�鿪ͷ�� rtsp:// �� rtmp://
	if( (strnicmp("rtsp://", lpszFullURL, strlen("rtsp://")) != 0 ) && 
		(strnicmp("rtmp://", lpszFullURL, strlen("rtmp://")) != 0 ) ) {
		return false;
	}
	// ����� rtsp:// Э�飬ֱ�ӷ���...
	if( strnicmp("rtsp://", lpszFullURL, strlen("rtsp://")) == 0 )
		return true;
	// ����� rtmp:// Э�飬Ҫ�ϸ�һЩ...
	char * lpszData = (char *)lpszFullURL + strlen("rtmp://");
	StrPtrLen	 theKeyWord, theLive, theURI;
	StrPtrLen	 theData(lpszData, strlen(lpszData));
	StringParser theParse(&theData);
	if( !theParse.GetThru(&theKeyWord, '/') ) {
		return false;
	}
	// �õ����ӵ�ַ�Ͷ˿�...
	if( theKeyWord.Ptr == NULL || theKeyWord.Len <= 0 ) {
		return false;
	}
	// ��ַ��ȷ...
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
		strError.Format("%s \n������Ч��MP4�ļ���������ѡ��", strFile);
		MessageBox(strError, "����", MB_OK|MB_ICONSTOP);
		return;
	}
	// �ͷ���Դ����ʾ·���ڶԻ�������...
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
