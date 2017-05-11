
#pragma once

#include "XPEdit.h"
#include "XPButton.h"

class CQRText  
{
public:
	CQRText();
	~CQRText();

	void SetLogoFont(CString Name, int nHeight = 24, int nWeight = FW_LIGHT,
					 BYTE bItalic = false, BYTE bUnderline = false);
	void SetLogoText(CString Text) { m_LogoText = Text;	}
public:
	void	SFDraw(CDC *pDC, CRect rect, COLORREF inColor);
private:
	CFont 	m_fontLogo;
	CString m_LogoText;
};

class CQRStatic : public CStatic
{
	DECLARE_DYNAMIC(CQRStatic)
public:
	CQRStatic();
	virtual ~CQRStatic();
protected:
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	DECLARE_MESSAGE_MAP()
private:
	bool		doGetCurl();
	bool		doNetJpgGif(CDC* pDC, int x, int y);
	bool		ShowJpgGif(CDC* pDC, CString strPath, int x, int y);
private:
	CQRText		m_qrText;
};
