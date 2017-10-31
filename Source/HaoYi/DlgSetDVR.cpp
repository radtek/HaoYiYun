
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

	// 如果通道已登录，将按钮置灰...
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
	// 更新数据到变量当中...
	if( m_nDBCameraID <= 0 || !this->UpdateData(true) )
		return;
	// 判断输入数据的有效性...
	if( m_strDVRName.IsEmpty() ) {
		this->MessageBox("【通道名称】不能为空！", "错误", MB_ICONSTOP);
		GetDlgItem(IDC_EDIT_DVR_NAME)->SetFocus();
		return;
	}
	if( m_strDVRAddr.IsEmpty() ) {
		this->MessageBox("【登录地址】不能为空！", "错误", MB_ICONSTOP);
		GetDlgItem(IDC_EDIT_DVR_ADDR)->SetFocus();
		return;
	}
	if( m_strLoginUser.IsEmpty() || m_strLoginPass.IsEmpty() ) {
		this->MessageBox("【登录用户】或【登录密码】不能为空！", "错误", MB_ICONSTOP);
		GetDlgItem(IDC_EDIT_DVR_USER)->SetFocus();
		return;
	}
	// 将输入的数据直接存盘...
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
	// 只需要将配置更新到内存当中就可以了...
	theConfig.SetCamera(m_nDBCameraID, theDataWeb);

	CDialogEx::OnOK();
}
