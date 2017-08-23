
#include "stdafx.h"
#include "HaoYi.h"
#include "DlgSetSys.h"
#include "XmlConfig.h"

IMPLEMENT_DYNAMIC(CDlgSetSys, CDialogEx)

CDlgSetSys::CDlgSetSys(CWnd* pParent /*=NULL*/)
	: CDialogEx(CDlgSetSys::IDD, pParent)
	, m_nWebPort(DEF_WEB_PORT)
	, m_strWebAddr(DEF_WEB_ADDR)
	, m_strMainName(DEF_MAIN_NAME)
	, m_strSavePath(DEF_REC_PATH)
	, m_nRecRate(DEF_MAIN_KBPS)
	, m_nLiveRate(DEF_SUB_KBPS)
	, m_nSnapStep(DEF_SNAP_STEP)
	, m_nRecSlice(DEF_REC_SLICE)
	, m_bAutoLinkFDFS(false)
	, m_bAutoLinkDVR(false)
	, m_nMaxCamera(0)
{
}

CDlgSetSys::~CDlgSetSys()
{
}

void CDlgSetSys::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_MAIN_NAME, m_strMainName);
	DDX_Text(pDX, IDC_EDIT_SAVE_PATH, m_strSavePath);
	DDX_Text(pDX, IDC_EDIT_REC_RATE, m_nRecRate);
	DDV_MinMaxInt(pDX, m_nRecRate, 200, 4096);
	DDX_Text(pDX, IDC_EDIT_LIVE_RATE, m_nLiveRate);
	DDV_MinMaxInt(pDX, m_nLiveRate, 100, 4096);
	DDX_Text(pDX, IDC_EDIT_SNAP_STEP, m_nSnapStep);
	DDV_MinMaxInt(pDX, m_nSnapStep, 1, 600);
	DDX_Text(pDX, IDC_EDIT_REC_SLICE, m_nRecSlice);
	DDV_MinMaxInt(pDX, m_nRecSlice, 30, 3600);
	DDX_Check(pDX, IDC_CHECK_AUTO_DVR, m_bAutoLinkDVR);
	DDX_Check(pDX, IDC_CHECK_AUTO_FDFS, m_bAutoLinkFDFS);
	DDX_Text(pDX, IDC_EDIT_WEB_ADDR, m_strWebAddr);
	DDX_Text(pDX, IDC_EDIT_WEB_PORT, m_nWebPort);
	DDV_MinMaxInt(pDX, m_nWebPort, 1, 65535);
	DDX_Text(pDX, IDC_EDIT_MAX_CAMERA, m_nMaxCamera);
	DDV_MinMaxInt(pDX, m_nMaxCamera, 1, 36);
}

BEGIN_MESSAGE_MAP(CDlgSetSys, CDialogEx)
	ON_BN_CLICKED(IDOK, &CDlgSetSys::OnBnClickedOK)
END_MESSAGE_MAP()

BOOL CDlgSetSys::OnInitDialog()
{
	//CDialogEx::OnInitDialog();

	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	m_strMainName = theConfig.GetMainName().c_str();
	m_strSavePath = theConfig.GetSavePath().c_str();
	m_nRecRate = theConfig.GetMainKbps();
	m_nLiveRate = theConfig.GetSubKbps();
	m_nSnapStep = theConfig.GetSnapStep();
	m_nRecSlice = theConfig.GetRecSlice();
	m_nMaxCamera = theConfig.GetMaxCamera();
	m_bAutoLinkDVR = theConfig.GetAutoLinkDVR();
	m_bAutoLinkFDFS = theConfig.GetAutoLinkFDFS();
	m_nWebPort = theConfig.GetWebPort();
	m_strWebAddr = theConfig.GetWebAddr().c_str();

	this->UpdateData(false);

	return TRUE;
}

void CDlgSetSys::OnBnClickedOK()
{
	// 更新数据到变量当中...
	if( !this->UpdateData(true) )
		return;
	// Web地址不能为空...
	if( m_strWebAddr.GetLength() <= 0 ) {
		this->MessageBox("【网站地址】不能为空！", "错误", MB_ICONSTOP);
		GetDlgItem(IDC_EDIT_WEB_ADDR)->SetFocus();
		return;
	}
	// 判断 http:// 或 https:// 前缀...
	if( (strnicmp("https://", m_strWebAddr, strlen("https://")) != 0 ) && 
		(strnicmp("http://", m_strWebAddr, strlen("http://")) != 0 ) ) {
		this->MessageBox("【网站地址】必须包含 http:// 或 https:// 协议！", "错误", MB_ICONSTOP);
		GetDlgItem(IDC_EDIT_WEB_ADDR)->SetFocus();
		return;
	}
	// 如果包含了 https:// 前缀，端口需要设置成 443...
	if( (strnicmp("https://", m_strWebAddr, strlen("https://")) == 0) && (m_nWebPort != 443) ) {
		this->MessageBox("【网站地址】是 https:// 安全协议，【网站端口】需要设置成 443", "错误", MB_ICONSTOP);
		GetDlgItem(IDC_EDIT_WEB_ADDR)->SetFocus();
		return;
	}
	// 限制标题名称的长度...
	if( m_strMainName.GetLength() >= 64 ) {
		this->MessageBox("【标题名称】过长！", "错误", MB_ICONSTOP);
		GetDlgItem(IDC_EDIT_MAIN_NAME)->SetFocus();
		return;
	}
	// 标题名称和录像目录不能为空...
	if( m_strMainName.GetLength() <= 0 || m_strSavePath.GetLength() <= 0 ) {
		this->MessageBox("【标题名称】或【录像目录】不能为空！", "错误", MB_ICONSTOP);
		GetDlgItem(IDC_EDIT_MAIN_NAME)->SetFocus();
		return;
	}
	// 将输入的数据直接存盘...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	string & oldWebAddr = theConfig.GetWebAddr();
	int oldWebPort = theConfig.GetWebPort();
	// 设置Web地址是否发生变化...
	if( m_strWebAddr.Compare(oldWebAddr.c_str()) != 0 || oldWebPort != m_nWebPort ) {
		MessageBox("Web地址发生变化，重启采集端才能生效！", "提示", MB_ICONSTOP);
	}
	theConfig.SetMainName(m_strMainName.GetString());
	theConfig.SetAutoLinkDVR(m_bAutoLinkDVR);
	theConfig.SetAutoLinkFDFS(m_bAutoLinkFDFS);
	theConfig.SetWebAddr(m_strWebAddr.GetString());
	theConfig.SetWebPort(m_nWebPort);
	theConfig.SetSavePath(m_strSavePath.GetString());
	theConfig.SetMaxCamera(m_nMaxCamera);
	theConfig.SetMainKbps(m_nRecRate);
	theConfig.SetSubKbps(m_nLiveRate);
	theConfig.SetSnapStep(m_nSnapStep);
	theConfig.SetRecSlice(m_nRecSlice);
	theConfig.GMSaveConfig();
	// 关闭对话框...
	CDialogEx::OnOK();
}
