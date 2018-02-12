
#include "stdafx.h"
#include "HaoYi.h"
#include "DlgJump.h"

IMPLEMENT_DYNAMIC(CDlgJump, CDialogEx)

CDlgJump::CDlgJump(int inCurPage, int inMaxPage, CWnd* pParent /*=NULL*/)
  : CDialogEx(CDlgJump::IDD, pParent)
  , m_nCurPage(inCurPage)
  , m_nMaxPage(inMaxPage)
  , m_nSelPage(inCurPage-1)
{
	m_nSelPage = ((m_nSelPage <= 0) ? 0 : m_nSelPage);
}

CDlgJump::~CDlgJump()
{
}

void CDlgJump::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_COMBO_PAGE, m_comPage);
	DDX_CBIndex(pDX, IDC_COMBO_PAGE, m_nSelPage);
}

BEGIN_MESSAGE_MAP(CDlgJump, CDialogEx)
END_MESSAGE_MAP()

BOOL CDlgJump::OnInitDialog()
{
	// ����Ҫִ�У������Ӷ����޷�����...
	// ToolWindow��񴰿ڣ����Բ�����...
	CDialogEx::OnInitDialog();
	// ����combox������...
	CString strTitle, strPage;
	for(int i = 1; i <= m_nMaxPage; ++i) {
		strPage.Format("�� %d ҳ", i);
		m_comPage.AddString(strPage);
	}
	// ѡ�е�ǰҳ�� => ��ż�1...
	m_comPage.SetCurSel(m_nSelPage);
	// ���ô��ڱ�������...
	this->GetWindowText(strTitle);
	strTitle.Format(strTitle, m_nCurPage);
	this->SetWindowText(strTitle);

	return TRUE;
}

void CDlgJump::OnOK()
{
	// ������Ӧ�õ���������...
	this->UpdateData();
	// ����OK�����˳�...
	CDialog::OnOK();
}