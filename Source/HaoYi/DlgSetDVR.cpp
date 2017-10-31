
#include "stdafx.h"
#include "HaoYi.h"
#include "base64x.h"
#include "DlgSetDVR.h"
#include "XmlConfig.h"

IMPLEMENT_DYNAMIC(CDlgSetDVR, CDialogEx)

CDlgSetDVR::CDlgSetDVR(int nDBCameraID, BOOL bIsLogin, CWnd* pParent /*=NULL*/)
	: CDialogEx(CDlgSetDVR::IDD, pParent)
	, m_nDBCameraID(nDBCameraID)
	, m_bIsLogin(bIsLogin)
	, m_strDVRName(_T(""))
	, m_strDVRAddr(_T(""))
	, m_strLoginUser(_T(""))
	, m_strLoginPass(_T(""))
	, m_bOpenMirror(false)
	, m_bOpenOSD(true)
	, m_nDVRPort(0)
{
	ASSERT( m_nDBCameraID > 0 );
}

CDlgSetDVR::~CDlgSetDVR()
{
}

void CDlgSetDVR::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_DVR_ID, m_nDBCameraID);
	DDX_Text(pDX, IDC_EDIT_DVR_NAME, m_strDVRName);
	DDV_MaxChars(pDX, m_strDVRName, 32);
	DDX_Text(pDX, IDC_EDIT_DVR_ADDR, m_strDVRAddr);
	DDX_Text(pDX, IDC_EDIT_DVR_PORT, m_nDVRPort);
	DDV_MinMaxInt(pDX, m_nDVRPort, 1, 65535);
	DDX_Text(pDX, IDC_EDIT_DVR_USER, m_strLoginUser);
	DDX_Text(pDX, IDC_EDIT_DVR_PASS, m_strLoginPass);
	DDX_Check(pDX, IDC_CHECK_OPEN_OSD, m_bOpenOSD);
	DDX_Check(pDX, IDC_CHECK_OPEN_MIRROR, m_bOpenMirror);
}

BEGIN_MESSAGE_MAP(CDlgSetDVR, CDialogEx)
	ON_BN_CLICKED(IDOK, &CDlgSetDVR::OnBnClickedOK)
END_MESSAGE_MAP()

BOOL CDlgSetDVR::OnInitDialog()
{
	//CDialogEx::OnInitDialog();

	SetIcon(AfxGetApp()->LoadIcon(IDR_MAINFRAME), FALSE);

	ASSERT( m_nDBCameraID > 0 );
	GM_MapData theDataWeb;
	TCHAR szDecodePass[MAX_PATH] = {0};
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetCamera(m_nDBCameraID, theDataWeb);
	string & strCmdPort = theDataWeb["device_cmd_port"];
	string & strMirror = theDataWeb["device_mirror"];
	string & strOSD = theDataWeb["device_osd"];
	string & strLoginPass = theDataWeb["device_pass"];
	int nDecLen = Base64decode(szDecodePass, strLoginPass.c_str());
	m_strDVRName = theDataWeb["camera_name"].c_str();
	m_strDVRAddr = theDataWeb["device_ip"].c_str();
	m_nDVRPort = ((strCmdPort.size() > 0) ? atoi(strCmdPort.c_str()) : 8000);
	m_strLoginUser = theDataWeb["device_user"].c_str();
	m_strLoginPass = szDecodePass;
	m_bOpenMirror = ((strMirror.size() > 0) ? atoi(strMirror.c_str()) : false);
	m_bOpenOSD = ((strOSD.size() > 0) ? atoi(strOSD.c_str()) : true);

	this->UpdateData(false);

	// ���ͨ���ѵ�¼������ť�û�...
	if( m_bIsLogin ) {
		((CEdit*)GetDlgItem(IDC_EDIT_DVR_NAME))->SetReadOnly(true);
		((CEdit*)GetDlgItem(IDC_EDIT_DVR_ADDR))->SetReadOnly(true);
		((CEdit*)GetDlgItem(IDC_EDIT_DVR_PORT))->SetReadOnly(true);
		((CEdit*)GetDlgItem(IDC_EDIT_DVR_USER))->SetReadOnly(true);
		((CEdit*)GetDlgItem(IDC_EDIT_DVR_PASS))->SetReadOnly(true);
		GetDlgItem(IDC_CHECK_OPEN_OSD)->EnableWindow(false);
		GetDlgItem(IDC_CHECK_OPEN_MIRROR)->EnableWindow(false);
		GetDlgItem(IDOK)->EnableWindow(false);
	}
	return TRUE;
}

void CDlgSetDVR::OnBnClickedOK()
{
	// �������ݵ���������...
	if( m_nDBCameraID <= 0 || !this->UpdateData(true) )
		return;
	// �ж��������ݵ���Ч��...
	if( m_strDVRName.IsEmpty() ) {
		this->MessageBox("��ͨ�����ơ�����Ϊ�գ�", "����", MB_ICONSTOP);
		GetDlgItem(IDC_EDIT_DVR_NAME)->SetFocus();
		return;
	}
	if( m_strDVRAddr.IsEmpty() ) {
		this->MessageBox("����¼��ַ������Ϊ�գ�", "����", MB_ICONSTOP);
		GetDlgItem(IDC_EDIT_DVR_ADDR)->SetFocus();
		return;
	}
	if( m_strLoginUser.IsEmpty() || m_strLoginPass.IsEmpty() ) {
		this->MessageBox("����¼�û����򡾵�¼���롿����Ϊ�գ�", "����", MB_ICONSTOP);
		GetDlgItem(IDC_EDIT_DVR_USER)->SetFocus();
		return;
	}
	// �����������ֱ�Ӵ���...
	GM_MapData theDataWeb;
	TCHAR szBuffer[MAX_PATH] = {0};
	TCHAR szEncode[MAX_PATH] = {0};
	int nEncLen = Base64encode(szEncode, m_strLoginPass, m_strLoginPass.GetLength());
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetCamera(m_nDBCameraID, theDataWeb);
	theDataWeb["camera_name"] = m_strDVRName;
	theDataWeb["device_ip"] = m_strDVRAddr;
	sprintf(szBuffer, "%d", m_nDVRPort);
	theDataWeb["device_cmd_port"] = szBuffer;
	theDataWeb["device_user"] = m_strLoginUser;
	theDataWeb["device_pass"] = szEncode;
	sprintf(szBuffer, "%d", m_bOpenOSD);
	theDataWeb["device_osd"] = szBuffer;
	sprintf(szBuffer, "%d", m_bOpenMirror);
	theDataWeb["device_mirror"] = szBuffer;
	// ֻ��Ҫ�����ø��µ��ڴ浱�оͿ�����...
	theConfig.SetCamera(m_nDBCameraID, theDataWeb);

	CDialogEx::OnOK();
}
