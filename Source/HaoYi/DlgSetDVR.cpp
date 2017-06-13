
#include "stdafx.h"
#include "HaoYi.h"
#include "base64x.h"
#include "DlgSetDVR.h"
#include "XmlConfig.h"

IMPLEMENT_DYNAMIC(CDlgSetDVR, CDialogEx)

CDlgSetDVR::CDlgSetDVR(int nCameraID, BOOL bIsLogin, CWnd* pParent /*=NULL*/)
	: CDialogEx(CDlgSetDVR::IDD, pParent)
	, m_bNameChanged(false)
	, m_bIsLogin(bIsLogin)
	, m_nDVRID(nCameraID)
	, m_strDVRName(_T(""))
	, m_strDVRAddr(_T(""))
	, m_strLoginUser(_T(""))
	, m_strLoginPass(_T(""))
	, m_bOpenMirror(false)
	, m_bOpenOSD(true)
	, m_nDVRPort(0)
{
	ASSERT( m_nDVRID > 0 );
}

CDlgSetDVR::~CDlgSetDVR()
{
}

void CDlgSetDVR::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_DVR_ID, m_nDVRID);
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

	ASSERT( m_nDVRID > 0 );
	GM_MapData theDataLoc;
	TCHAR szDecodePass[MAX_PATH] = {0};
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetCamera(m_nDVRID, theDataLoc);
	string & strCmdPort = theDataLoc["CommandPort"];
	string & strMirror = theDataLoc["OpenMirror"];
	string & strOSD = theDataLoc["OpenOSD"];
	string & strLoginPass = theDataLoc["LoginPass"];
	int nDecLen = Base64decode(szDecodePass, strLoginPass.c_str());
	m_strDVRName = theDataLoc["Name"].c_str();
	m_strDVRAddr = theDataLoc["IPv4Address"].c_str();
	m_nDVRPort = ((strCmdPort.size() > 0) ? atoi(strCmdPort.c_str()) : 8000);
	m_strLoginUser = theDataLoc["LoginUser"].c_str();
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
	if( m_nDVRID <= 0 || !this->UpdateData(true) )
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
	GM_MapData theMapLoc;
	TCHAR szBuffer[MAX_PATH] = {0};
	TCHAR szEncode[MAX_PATH] = {0};
	int nEncLen = Base64encode(szEncode, m_strLoginPass, m_strLoginPass.GetLength());
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetCamera(m_nDVRID, theMapLoc);
	string strOldName = theMapLoc["Name"];
	theMapLoc["Name"] = m_strDVRName;
	theMapLoc["IPv4Address"] = m_strDVRAddr;
	sprintf(szBuffer, "%d", m_nDVRPort);
	theMapLoc["CommandPort"] = szBuffer;
	theMapLoc["LoginUser"] = m_strLoginUser;
	theMapLoc["LoginPass"] = szEncode;
	sprintf(szBuffer, "%d", m_bOpenOSD);
	theMapLoc["OpenOSD"] = szBuffer;
	sprintf(szBuffer, "%d", m_bOpenMirror);
	theMapLoc["OpenMirror"] = szBuffer;
	theConfig.SetCamera(m_nDVRID, theMapLoc);
	theConfig.GMSaveConfig();

	// 如果频道名称发生了改变，设置变化标志...
	if( strOldName.compare(m_strDVRName) != 0 ) {
		m_strDBCameraID = theMapLoc["DBCameraID"];
		m_bNameChanged = true;
	}

	CDialogEx::OnOK();
}
