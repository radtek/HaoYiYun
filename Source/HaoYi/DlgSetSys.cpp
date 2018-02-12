
#include "stdafx.h"
#include "HaoYi.h"
#include "DlgSetSys.h"
#include "XmlConfig.h"
#include "HaoYiView.h"

IMPLEMENT_DYNAMIC(CDlgSetSys, CDialogEx)

CDlgSetSys::CDlgSetSys(CHaoYiView * pHaoYiView)
	: CDialogEx(CDlgSetSys::IDD, (CWnd*)pHaoYiView)
	, m_lpHaoYiVew(pHaoYiView)
	, m_nWebPort(DEF_WEB_PORT)
	, m_strSavePath(DEF_REC_PATH)
	, m_strMainName(DEF_MAIN_NAME)
	, m_nRecRate(DEF_MAIN_KBPS)
	, m_nLiveRate(DEF_SUB_KBPS)
	, m_bAutoLinkFDFS(false)
	, m_bAutoLinkDVR(false)
	, m_bWebChange(false)
	, m_nInterVal(0)
	, m_nSliceVal(0)
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
	DDX_Text(pDX, IDC_EDIT_REC_SLICE, m_nSliceVal);
	DDV_MinMaxInt(pDX, m_nSliceVal, 0, 30);
	DDX_Text(pDX, IDC_EDIT_REC_INTER, m_nInterVal);
	DDV_MinMaxInt(pDX, m_nInterVal, 0, 3);
	DDX_Check(pDX, IDC_CHECK_AUTO_DVR, m_bAutoLinkDVR);
	DDX_Check(pDX, IDC_CHECK_AUTO_FDFS, m_bAutoLinkFDFS);
	DDX_Text(pDX, IDC_EDIT_WEB_PORT, m_nWebPort);
	DDV_MinMaxInt(pDX, m_nWebPort, 1, 65535);
	DDX_Control(pDX, IDC_COMBO_ADDR, m_comAddress);
}

BEGIN_MESSAGE_MAP(CDlgSetSys, CDialogEx)
	ON_BN_CLICKED(IDOK, &CDlgSetSys::OnBnClickedOK)
	ON_CBN_SELCHANGE(IDC_COMBO_ADDR, &CDlgSetSys::OnCbnSelchangeComboAddr)
END_MESSAGE_MAP()

BOOL CDlgSetSys::OnInitDialog()
{
	// 必须要执行，否则，子对象无法创建...
	CDialogEx::OnInitDialog();
	// 设置图标...
	this->SetIcon(AfxGetApp()->LoadIcon(IDR_MAINFRAME), FALSE);
	// 获取系统配置...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	GM_MapServer & theServer = theConfig.GetServerList();
	// 网站端口、网站地址、存盘路径 是本地配置...
	m_nWebPort = theConfig.GetWebPort();
	m_strSavePath = theConfig.GetSavePath().c_str();
	// 将服务器列表写入ComboBox当中...
	GM_MapServer::iterator itorServer;
	for(itorServer = theServer.begin(); itorServer != theServer.end(); ++itorServer) {
		m_comAddress.AddString(itorServer->first.c_str());
	}
	// 选中当前已经指定的地址内容...
	m_comAddress.SelectString(0, theConfig.GetWebAddr().c_str());
	// 已经注册成功，从系统配置当中获取...
	if( theConfig.GetDBHaoYiGatherID() > 0 ) {
		m_strMainName = theConfig.GetMainName().c_str();
		m_nRecRate = theConfig.GetMainKbps();
		m_nLiveRate = theConfig.GetSubKbps();
		m_nSliceVal = theConfig.GetSliceVal();
		m_nInterVal = theConfig.GetInterVal();
		m_bAutoLinkDVR = theConfig.GetAutoLinkDVR();
		m_bAutoLinkFDFS = theConfig.GetAutoLinkFDFS();
	}
	// 如果没有注册成功，直接使用系统默认值...

	// 将配置写入界面当中...
	this->UpdateData(false);

	return TRUE;
}

void CDlgSetSys::OnBnClickedOK()
{
	// 更新数据到变量当中...
	if( !this->UpdateData(true) )
		return;
	// 获取当前选择或输入的地址信息...
	CString strComboText;
	m_comAddress.GetWindowText(strComboText);
	strComboText.MakeLower();
	strComboText.Trim();
	if( strComboText.GetLength() <= 0 ) {
		this->MessageBox("【网站地址】不能为空！", "错误", MB_ICONSTOP);
		m_comAddress.SetFocus();
		return;
	}
	// 判断 http:// 或 https:// 前缀...
	if( (strnicmp("https://", strComboText, strlen("https://")) != 0 ) && 
		(strnicmp("http://", strComboText, strlen("http://")) != 0 ) ) {
		this->MessageBox("【网站地址】必须包含 http:// 或 https:// 协议！", "错误", MB_ICONSTOP);
		m_comAddress.SetFocus();
		return;
	}
	// 如果包含了 https:// 前缀，端口需要设置成 443...
	if( (strnicmp("https://", strComboText, strlen("https://")) == 0) && (m_nWebPort != 443) ) {
		this->MessageBox("【网站地址】是 https:// 安全协议，【网站端口】需要设置成 443", "错误", MB_ICONSTOP);
		m_comAddress.SetFocus();
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
	if( strComboText.Compare(oldWebAddr.c_str()) != 0 || oldWebPort != m_nWebPort ) {
		//MessageBox("Web地址发生变化，重启采集端才能生效！", "提示", MB_ICONSTOP);
		//必须在地址存盘之前向网站发送命令，否则，就会向新网站发送命令...
		m_lpHaoYiVew->doGatherLogout();
		this->m_bWebChange = true;
	}
	// 将新选中的地址和端口更新到服务器列表，并设置成焦点...
	theConfig.SetFocusServer(strComboText.GetString(), m_nWebPort);
	// 注意：其它配置由网站服务器授权，不能改变...
	CDialogEx::OnOK();
}

void CDlgSetSys::OnCbnSelchangeComboAddr()
{
	CString strCurAddress;
	int nCurSel = m_comAddress.GetCurSel();
	m_comAddress.GetLBText(nCurSel, strCurAddress);

	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	GM_MapServer & theServer = theConfig.GetServerList();
	GM_MapServer::iterator itorServer = theServer.find(strCurAddress.GetString());
	if( itorServer == theServer.end() )
		return;
	// 将对应的端口号，写入界面当中...
	m_nWebPort = itorServer->second;
	this->UpdateData(false);
}
