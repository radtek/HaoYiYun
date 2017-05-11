
#include "stdafx.h"
#include "HaoYi.h"
#include "QRStatic.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////
// CQRText
//////////////////////////////////////////////////////////

CQRText::CQRText()
{
	//this->SetLogoFont("Arial", 30);
}

CQRText::~CQRText()
{
	if( m_fontLogo.m_hObject ) {
		m_fontLogo.DeleteObject();
		m_fontLogo.Detach();
	}
}

void CQRText::SetLogoFont(CString Name, int nHeight/* = 24*/, int nWeight/* = FW_LIGHT*/,
						  BYTE bItalic/* = false*/, BYTE bUnderline/* = false*/)
{
	if( m_fontLogo.m_hObject ) {
		m_fontLogo.DeleteObject();
		m_fontLogo.Detach();
	}
	
	m_fontLogo.CreateFont(nHeight, 0, 0, 0, nWeight, bItalic, bUnderline,0,0,0,0,0,0, Name);
}

void CQRText::SFDraw(CDC *pDC, CRect rect, COLORREF inColor)
{
	if(m_LogoText.IsEmpty())
		return;
	
	pDC->SetBkMode(TRANSPARENT);
	
	COLORREF OldColor;
	CRect rectText = rect;
	CFont * OldFont = pDC->SelectObject(&m_fontLogo);
	
	// draw text in DC
	//OldColor = pDC->SetTextColor( ::GetSysColor( COLOR_3DHILIGHT));
	//pDC->DrawText( m_LogoText, rectText + CPoint(1,1), DT_SINGLELINE | DT_CENTER | DT_VCENTER);

	//pDC->SetTextColor( ::GetSysColor( COLOR_3DSHADOW));
	OldColor = pDC->SetTextColor( inColor );
	pDC->DrawText( m_LogoText, rectText, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
	
	// restore old text color
	pDC->SetTextColor( OldColor);
	// restore old font
	pDC->SelectObject(OldFont);
}

//////////////////////////////////////////////////////////
// CQRStatic
//////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(CQRStatic, CStatic)

CQRStatic::CQRStatic()
{
	m_qrText.SetLogoFont("Arial", 15);
	m_qrText.SetLogoText("请用微信扫码注册");
}

CQRStatic::~CQRStatic()
{
}

BEGIN_MESSAGE_MAP(CQRStatic, CStatic)
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
END_MESSAGE_MAP()

#include <curl.h>
#include <string>
size_t procDownShop(void *ptr, size_t size, size_t nmemb, void *stream)
{
	std::string * str = dynamic_cast<std::string*>((std::string *)stream);
	char* pData = (char*)ptr;  
    str->append(pData, size * nmemb);
	return size * nmemb;
}

bool CQRStatic::doGetCurl()
{
	std::string strResponse;
	CURLcode res;
	CURL  *  curl = curl_easy_init();
	res = curl_easy_setopt(curl, CURLOPT_URL,"http://www.happyhope.net/wxapi.php/Home/downShop/shop_id/1");
	res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, procDownShop);
	res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&strResponse);
	res = curl_easy_perform(curl); 
	curl_easy_cleanup(curl);
	return true;
}

BOOL CQRStatic::OnEraseBkgnd(CDC* pDC)
{
	//return CStatic::OnEraseBkgnd(pDC);
	
	//this->doGetCurl();

	CRect rcRect;
	this->GetClientRect(&rcRect);
	pDC->FillSolidRect(rcRect, WND_BK_COLOR);
	
	rcRect.DeflateRect(1, 1);
	pDC->DrawEdge(rcRect, EDGE_ETCHED, BF_RECT);

	rcRect.top = rcRect.bottom - 25;
	m_qrText.SFDraw(pDC, rcRect, RGB(255,255,0));

	//this->ShowJpgGif(pDC, "C:\\tu.jpg", 0, 0);
	//this->doNetJpgGif(pDC, 30, 12);
	return true;
}

void CQRStatic::OnPaint()
{
	CPaintDC dc(this);
	// 必须响应这个函数才能调用OnEraseBkgnd()...
	// 不为绘图消息调用 CStatic::OnPaint()
}

bool CQRStatic::doNetJpgGif(CDC* pDC, int x, int y)
{
	std::string strResponse;
	CURLcode res;
	CURL  *  curl = curl_easy_init();
	res = curl_easy_setopt(curl, CURLOPT_URL,"http://www.happyhope.net/wxapi.php/Home/downShop/shop_id/1");
	res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, procDownShop);
	res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&strResponse);
	res = curl_easy_perform(curl); 
	curl_easy_cleanup(curl);

	LONG cb = strResponse.size();   
	IStream * pStm = NULL;   
	HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, cb);   
	LPVOID pvData = NULL;   
	if( hGlobal != NULL )
	{   
		pvData = GlobalLock(hGlobal);  
		if (pvData != NULL)   
		{   
			memcpy(pvData, strResponse.c_str(), cb);   
			GlobalUnlock(hGlobal);   
			CreateStreamOnHGlobal(hGlobal, TRUE, &pStm);   
		}  
	}  
	IPicture * pPic = NULL;  
	//load image from file stream  
	if(SUCCEEDED(OleLoadPicture(pStm,cb,TRUE,IID_IPicture,(LPVOID*)&pPic)))  
	{  
		OLE_XSIZE_HIMETRIC hmWidth;   
		OLE_YSIZE_HIMETRIC hmHeight;   
		pPic->get_Width(&hmWidth);   
		pPic->get_Height(&hmHeight);   
		double fX,fY;
		/*fX = pDC->GetDeviceCaps(HORZRES);
		fY = pDC->GetDeviceCaps(HORZSIZE);
		fX = pDC->GetDeviceCaps(VERTRES);
		fY = pDC->GetDeviceCaps(VERTSIZE);
		//get image height and width  
		fX = (double)pDC->GetDeviceCaps(HORZRES)*(double)hmWidth/((double)pDC->GetDeviceCaps(HORZSIZE)*100.0);   
		fY = (double)pDC->GetDeviceCaps(VERTRES)*(double)hmHeight/((double)pDC->GetDeviceCaps(VERTSIZE)*100.0);*/
		fX = fY = 200;
		//use render function display image  
		if(FAILED(pPic->Render(*pDC,x,y,(DWORD)fX,(DWORD)fY,0,hmHeight,hmWidth,-hmHeight,NULL)))   
		{  
			pPic->Release();  
			return false;  
		}  
		pPic->Release();  
	} else {  
		return false;   
	}
	return true;
}

bool CQRStatic::ShowJpgGif(CDC* pDC, CString strPath, int x, int y)  
{  
	LONG cb;   
	CFile file;   
	IStream *pStm;   
	CFileStatus fstatus;   
	if (file.Open(strPath,CFile::modeRead)&&file.GetStatus(strPath,fstatus)&&((cb = fstatus.m_size) != -1))   
	{   
		HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, cb);   
		LPVOID pvData = NULL;   
		if (hGlobal != NULL)   
		{   
			pvData = GlobalLock(hGlobal);  
			if (pvData != NULL)   
			{   
				file.Read(pvData, cb);   
				GlobalUnlock(hGlobal);   
				CreateStreamOnHGlobal(hGlobal, TRUE, &pStm);   
			}  
		}  
	} else {  
		return false;
	}
	//显示JPEG和GIF格式的图片，GIF只能显示一帧，还不能显示动画，  
	//要显示动画GIF请使用ACTIVE控//件。  
	IPicture *pPic;  
	//load image from file stream  
	if(SUCCEEDED(OleLoadPicture(pStm,fstatus.m_size,TRUE,IID_IPicture,(LPVOID*)&pPic)))  
	{  
		OLE_XSIZE_HIMETRIC hmWidth;   
		OLE_YSIZE_HIMETRIC hmHeight;   
		pPic->get_Width(&hmWidth);   
		pPic->get_Height(&hmHeight);   
		double fX,fY;   
		//get image height and width  
		fX = (double)pDC->GetDeviceCaps(HORZRES)*(double)hmWidth/((double)pDC->GetDeviceCaps(HORZSIZE)*100.0);   
		fY = (double)pDC->GetDeviceCaps(VERTRES)*(double)hmHeight/((double)pDC->GetDeviceCaps(VERTSIZE)*100.0);   
		//use render function display image  
		if(FAILED(pPic->Render(*pDC,x,y,(DWORD)fX,(DWORD)fY,0,hmHeight,hmWidth,-hmHeight,NULL)))   
		{  
			pPic->Release();  
			return false;  
		}  
		pPic->Release();  
	} else {  
		return false;   
	}
	return true;  
}
