
#pragma once

class CInfoBar : public CDialog
{
	DECLARE_DYNAMIC(CInfoBar)

public:
	CInfoBar(CWnd* pParent = NULL);
	virtual ~CInfoBar();
	enum { IDD = IDD_INFOBAR };
protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	DECLARE_MESSAGE_MAP()
};
