
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
	// ����Ҫִ�У������Ӷ����޷�����...
	CDialogEx::OnInitDialog();
	// ����ͼ��...
	this->SetIcon(AfxGetApp()->LoadIcon(IDR_MAINFRAME), FALSE);
	// ��ȡϵͳ����...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	GM_MapServer & theServer = theConfig.GetServerList();
	// ��վ�˿ڡ���վ��ַ������·�� �Ǳ�������...
	m_nWebPort = theConfig.GetWebPort();
	m_strSavePath = theConfig.GetSavePath().c_str();
	// ���������б�д��ComboBox����...
	GM_MapServer::iterator itorServer;
	for(itorServer = theServer.begin(); itorServer != theServer.end(); ++itorServer) {
		m_comAddress.AddString(itorServer->first.c_str());
	}
	// ѡ�е�ǰ�Ѿ�ָ���ĵ�ַ����...
	m_comAddress.SelectString(0, theConfig.GetWebAddr().c_str());
	// �Ѿ�ע��ɹ�����ϵͳ���õ��л�ȡ...
	if( theConfig.GetDBHaoYiGatherID() > 0 ) {
		m_strMainName = theConfig.GetMainName().c_str();
		m_nRecRate = theConfig.GetMainKbps();
		m_nLiveRate = theConfig.GetSubKbps();
		m_nSliceVal = theConfig.GetSliceVal();
		m_nInterVal = theConfig.GetInterVal();
		m_bAutoLinkDVR = theConfig.GetAutoLinkDVR();
		m_bAutoLinkFDFS = theConfig.GetAutoLinkFDFS();
	}
	// ���û��ע��ɹ���ֱ��ʹ��ϵͳĬ��ֵ...

	// ������д����浱��...
	this->UpdateData(false);

	return TRUE;
}

void CDlgSetSys::OnBnClickedOK()
{
	// �������ݵ���������...
	if( !this->UpdateData(true) )
		return;
	// ��ȡ��ǰѡ�������ĵ�ַ��Ϣ...
	CString strComboText;
	m_comAddress.GetWindowText(strComboText);
	strComboText.MakeLower();
	strComboText.Trim();
	if( strComboText.GetLength() <= 0 ) {
		this->MessageBox("����վ��ַ������Ϊ�գ�", "����", MB_ICONSTOP);
		m_comAddress.SetFocus();
		return;
	}
	// �ж� http:// �� https:// ǰ׺...
	if( (strnicmp("https://", strComboText, strlen("https://")) != 0 ) && 
		(strnicmp("http://", strComboText, strlen("http://")) != 0 ) ) {
		this->MessageBox("����վ��ַ��������� http:// �� https:// Э�飡", "����", MB_ICONSTOP);
		m_comAddress.SetFocus();
		return;
	}
	// ��������� https:// ǰ׺���˿���Ҫ���ó� 443...
	if( (strnicmp("https://", strComboText, strlen("https://")) == 0) && (m_nWebPort != 443) ) {
		this->MessageBox("����վ��ַ���� https:// ��ȫЭ�飬����վ�˿ڡ���Ҫ���ó� 443", "����", MB_ICONSTOP);
		m_comAddress.SetFocus();
		return;
	}
	// ���Ʊ������Ƶĳ���...
	if( m_strMainName.GetLength() >= 64 ) {
		this->MessageBox("���������ơ�������", "����", MB_ICONSTOP);
		GetDlgItem(IDC_EDIT_MAIN_NAME)->SetFocus();
		return;
	}
	// �������ƺ�¼��Ŀ¼����Ϊ��...
	if( m_strMainName.GetLength() <= 0 || m_strSavePath.GetLength() <= 0 ) {
		this->MessageBox("���������ơ���¼��Ŀ¼������Ϊ�գ�", "����", MB_ICONSTOP);
		GetDlgItem(IDC_EDIT_MAIN_NAME)->SetFocus();
		return;
	}
	// �����������ֱ�Ӵ���...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	string & oldWebAddr = theConfig.GetWebAddr();
	int oldWebPort = theConfig.GetWebPort();
	// ����Web��ַ�Ƿ����仯...
	if( strComboText.Compare(oldWebAddr.c_str()) != 0 || oldWebPort != m_nWebPort ) {
		//MessageBox("Web��ַ�����仯�������ɼ��˲�����Ч��", "��ʾ", MB_ICONSTOP);
		//�����ڵ�ַ����֮ǰ����վ����������򣬾ͻ�������վ��������...
		m_lpHaoYiVew->doGatherLogout();
		this->m_bWebChange = true;
	}
	// ����ѡ�еĵ�ַ�Ͷ˿ڸ��µ��������б������óɽ���...
	theConfig.SetFocusServer(strComboText.GetString(), m_nWebPort);
	// ע�⣺������������վ��������Ȩ�����ܸı�...
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
	// ����Ӧ�Ķ˿ںţ�д����浱��...
	m_nWebPort = itorServer->second;
	this->UpdateData(false);
}
