
#include "stdafx.h"
#include "HaoYi.h"
#include "InfoBar.h"

IMPLEMENT_DYNAMIC(CInfoBar, CDialog)

CInfoBar::CInfoBar(CWnd* pParent /*=NULL*/)
  : CDialog(CInfoBar::IDD, pParent)
{

}

CInfoBar::~CInfoBar()
{
}

void CInfoBar::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CInfoBar, CDialog)
END_MESSAGE_MAP()


BOOL CInfoBar::OnInitDialog()
{
	CDialog::OnInitDialog();

	return TRUE;
}
