
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
	// �������ݵ���������...
	if( !this->UpdateData(true) )
		return;
	// Web��ַ����Ϊ��...
	if( m_strWebAddr.GetLength() <= 0 ) {
		this->MessageBox("����վ��ַ������Ϊ�գ�", "����", MB_ICONSTOP);
		GetDlgItem(IDC_EDIT_WEB_ADDR)->SetFocus();
		return;
	}
	// �ж� http:// �� https:// ǰ׺...
	if( (strnicmp("https://", m_strWebAddr, strlen("https://")) != 0 ) && 
		(strnicmp("http://", m_strWebAddr, strlen("http://")) != 0 ) ) {
		this->MessageBox("����վ��ַ��������� http:// �� https:// Э�飡", "����", MB_ICONSTOP);
		GetDlgItem(IDC_EDIT_WEB_ADDR)->SetFocus();
		return;
	}
	// ��������� https:// ǰ׺���˿���Ҫ���ó� 443...
	if( (strnicmp("https://", m_strWebAddr, strlen("https://")) == 0) && (m_nWebPort != 443) ) {
		this->MessageBox("����վ��ַ���� https:// ��ȫЭ�飬����վ�˿ڡ���Ҫ���ó� 443", "����", MB_ICONSTOP);
		GetDlgItem(IDC_EDIT_WEB_ADDR)->SetFocus();
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
	if( m_strWebAddr.Compare(oldWebAddr.c_str()) != 0 || oldWebPort != m_nWebPort ) {
		MessageBox("Web��ַ�����仯�������ɼ��˲�����Ч��", "��ʾ", MB_ICONSTOP);
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
	// �رնԻ���...
	CDialogEx::OnOK();
}
