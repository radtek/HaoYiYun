
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
	// 必须要执行，否则，子对象无法创建...
	// ToolWindow风格窗口，可以不调用...
	CDialogEx::OnInitDialog();
	// 设置combox的内容...
	CString strTitle, strPage;
	for(int i = 1; i <= m_nMaxPage; ++i) {
		strPage.Format("第 %d 页", i);
		m_comPage.AddString(strPage);
	}
	// 选中当前页码 => 序号减1...
	m_comPage.SetCurSel(m_nSelPage);
	// 设置窗口标题名称...
	this->GetWindowText(strTitle);
	strTitle.Format(strTitle, m_nCurPage);
	this->SetWindowText(strTitle);

	return TRUE;
}

void CDlgJump::OnOK()
{
	// 将数据应用到变量当中...
	this->UpdateData();
	// 调用OK函数退出...
	CDialog::OnOK();
}