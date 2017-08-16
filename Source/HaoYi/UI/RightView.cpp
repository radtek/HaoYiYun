
#include "stdafx.h"
#include "UtilTool.h"
#include "RightView.h"
#include "base64x.h"
#include "..\HaoYi.h"
#include "..\Camera.h"
#include "..\HaoYiView.h"
#include "..\XmlConfig.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////
// CRightView
//////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(CRightView, CStatic)

CRightView::CRightView(CHaoYiView * lpParent)
  : m_lpParentDlg(lpParent)
  , m_nCurAutoID(DEF_CAMERA_START_ID)
{
	ASSERT( m_lpParentDlg != NULL );
	m_strYunTitle = "云台";
	m_strSetTitle = "设置";
}

CRightView::~CRightView()
{
}

BEGIN_MESSAGE_MAP(CRightView, CStatic)
	ON_WM_SIZE()
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_COMMAND_RANGE(kButtonLogin, kButtonQuanPlus, OnClickButtonWnd)
	ON_MESSAGE(WM_DEVICE_LOGIN_RESULT, &CRightView::OnMsgDeviceLoginResult)
	ON_MESSAGE(WM_DEVICE_BUTTON_LDOWNUP, &CRightView::OnMsgDeviceBtnLDownUp)
END_MESSAGE_MAP()

void CRightView::OnPaint() 
{
	CPaintDC dc(this);
}

void CRightView::OnSize(UINT nType, int cx, int cy)
{
	CStatic::OnSize(nType, cx, cy);
}

BOOL CRightView::OnEraseBkgnd(CDC* pDC) 
{
	// 获取当前的焦点通道编号...
	ASSERT( m_lpParentDlg != NULL );
	int nFocusCameraID = m_lpParentDlg->GetFocusCamera();
	CCamera * lpCamera = m_lpParentDlg->FindCameraByID(nFocusCameraID);
	CString & strLogStatus = ((lpCamera != NULL) ? lpCamera->GetLogStatus() : "登录");

	// 绘制整个背景区域...
	CRect rcRect;
	this->GetClientRect(rcRect);
	pDC->FillSolidRect(&rcRect, WND_BK_COLOR);
	
	// 如果不是设备流，直接隐藏按钮，绘制基础文字信息...
	if( lpCamera != NULL && !lpCamera->IsCameraDevice() ) {
		this->ShowButton(false);
		this->DrawStreamText(pDC);
		return TRUE;
	}
	// 显示或隐藏按钮对象...
	this->ShowButton((nFocusCameraID <= 0) ? false : true);
	// 显示相关的文字信息...
	this->ShowText(pDC, (nFocusCameraID <= 0) ? false : true, strLogStatus);

	return TRUE;
}
//
// 显示流转发时的文字信息...
void CRightView::DrawStreamText(CDC * pDC)
{
	// 获取矩形区数据...
	CRect rcRect;
	CSize rcPos, rcSize;
	this->GetClientRect(rcRect);

	// 准备文字信息...
	CFont * lpFont = this->GetFont();
	CFont * hOldFont = pDC->SelectObject(lpFont);

	// 设置文字模式...
	pDC->SetTextColor(RGB(255, 255, 255));
	pDC->SetBkMode(TRANSPARENT);

	// 登录 - 标题背景色...
	//COLORREF clrValue = RGB( 96, 123, 189);
	//rcRect.bottom = rcRect.top + kGroundHigh;
	//rcRect.DeflateRect(1,1);
	//pDC->FillSolidRect(rcRect, clrValue);
	// 登录 - 标题名称...
	//rcSize = pDC->GetOutputTextExtent(inLogStatus);
	//rcPos.cx = (rcRect.Width() - rcSize.cx) / 2;
	//rcPos.cy = (rcRect.Height() - rcSize.cy) / 2;
	//pDC->TextOut(kLeftMargin - 5, rcPos.cy+1, inLogStatus);

	// 绘制流转发信息...
	this->GetClientRect(rcRect);
	CString strNotice = "摄像头设备操作区";
	rcSize = pDC->GetOutputTextExtent(strNotice);
	rcPos.cx = (rcRect.Width() - rcSize.cx) / 2;
	rcPos.cy = (rcRect.Height() - rcSize.cy) / 2;
	pDC->SetTextColor(RGB(255, 255, 0));
	pDC->TextOut(rcPos.cx, rcPos.cy - 50, strNotice);

	// 恢复字体格式...
	pDC->SelectObject(hOldFont);
}
//
// 显示文字信息...
void CRightView::ShowText(CDC * pDC, BOOL bFocus, CString & inLogStatus)
{
	// 如果没有通道，直接返回...
	if( !bFocus ) return;

	// 获取矩形区数据...
	CRect rcRect;
	CSize rcPos, rcSize;
	this->GetClientRect(rcRect);

	// 准备文字信息...
	CFont * lpFont = this->GetFont();
	CFont * hOldFont = pDC->SelectObject(lpFont);

	// 设置文字模式...
	pDC->SetTextColor(RGB(255, 255, 255));
	pDC->SetBkMode(TRANSPARENT);

	// 覆盖非焦点区文字背景 => 居中，上下扩展15个像素...
	/*CRect rcNone;
	rcNone = rcRect;
	rcNone.top = rcRect.Height()/2 - 15;
	rcNone.bottom = rcRect.Height()/2 + 15;
	pDC->FillSolidRect(&rcNone, WND_BK_COLOR);*/

	// 登录 - 标题背景色...
	COLORREF clrValue = RGB( 96, 123, 189);
	rcRect.bottom = rcRect.top + kGroundHigh;
	rcRect.DeflateRect(1,1);
	pDC->FillSolidRect(rcRect, clrValue);
	// 登录 - 标题名称...
	rcSize = pDC->GetOutputTextExtent(inLogStatus);
	rcPos.cx = (rcRect.Width() - rcSize.cx) / 2;
	rcPos.cy = (rcRect.Height() - rcSize.cy) / 2;
	pDC->TextOut(kLeftMargin - 5, rcPos.cy+1, inLogStatus);

	// 显示编辑框文字...
	rcPos.SetSize(25, kGroundHigh + 20);
	pDC->TextOut(rcPos.cx, rcPos.cy, "IP地址：");
	rcPos.cy += kEditHigh + 5;
	pDC->TextOut(rcPos.cx, rcPos.cy, "端  口：");
	rcPos.cy += kEditHigh + 5;
	pDC->TextOut(rcPos.cx, rcPos.cy, "用户名：");
	rcPos.cy += kEditHigh + 5;
	pDC->TextOut(rcPos.cx, rcPos.cy, "密  码：");

	// 云台 - 标题背景色...
	rcRect.top = 235;
	rcRect.bottom = rcRect.top + kGroundHigh;
	pDC->FillSolidRect(rcRect, clrValue);
	// 云台 - 标题名称...
	rcSize = pDC->GetOutputTextExtent(m_strYunTitle);
	rcPos.cx = (rcRect.Width() - rcSize.cx) / 2;
	rcPos.cy = (rcRect.Height() - rcSize.cy) / 2;
	pDC->TextOut(kLeftMargin - 5, rcRect.top + rcPos.cy + 1, m_strYunTitle);

	// 设置 - 标题背景...
	/*rcRect.top = 445;
	rcRect.bottom = rcRect.top + kGroundHigh;
	pDC->FillSolidRect(rcRect, clrValue);
	// 设置 - 标题名称...
	rcSize = pDC->GetOutputTextExtent(m_strSetTitle);
	rcPos.cx = (rcRect.Width() - rcSize.cx) / 2;
	rcPos.cy = (rcRect.Height() - rcSize.cy) / 2;
	pDC->TextOut(kLeftMargin - 5, rcRect.top + rcPos.cy + 1, m_strSetTitle);*/

	// 显示焦距/放大/光圈...
	rcSize = pDC->GetOutputTextExtent("焦距");
	rcPos.cx = kLeftMargin + kBtnYunWidth + 10 + (kBtnYunWidth - rcSize.cx) / 2;
	rcPos.cy = 340 + (kBtnYunHigh - rcSize.cy) / 2;
	pDC->TextOut(rcPos.cx, rcPos.cy + 1 +  0, "焦距");
	pDC->TextOut(rcPos.cx, rcPos.cy + 1 + (kBtnYunHigh + 10)*1, "放大");
	pDC->TextOut(rcPos.cx, rcPos.cy + 1 + (kBtnYunHigh + 10)*2, "光圈");
	
	// 恢复字体格式...
	pDC->SelectObject(hOldFont);
}
//
// 显示按钮对象...
void CRightView::ShowButton(BOOL bDispFlag)
{
	m_editIPAddr.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);
	m_editIPPort.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);
	m_editUserName.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);
	m_editPassWord.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);
	
	m_btnLogin.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);
	m_btnLogout.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);
	
	m_btnYunAuto.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);
	m_btnYunTop.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);
	m_btnYunReset.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);
	m_btnYunLeft.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);
	m_btnYunBottom.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);
	m_btnYunRight.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);
	
	m_btnJiaoMinus.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);
	m_btnJiaoPlus.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);
	m_btnFangMinus.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);
	m_btnFangPlus.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);
	m_btnQuanMinus.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);
	m_btnQuanPlus.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);

	// 2017.07.31 - by jackey => 屏蔽了摄像头复杂设置...
	bDispFlag = false;
	m_btnSetVPreview.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);
	m_btnSetVParam.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);
	m_btnSetRecord.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);
	m_btnSetReview.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);
	m_btnSetAlert.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);
	m_btnSetCamera.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);
}
//
// 响应视频焦点窗口编号...
void CRightView::doFocusCamera(int nCameraID)
{
	// 获取 CCamera 对象...
	CCamera * lpCamera = m_lpParentDlg->FindCameraByID(nCameraID);
	if( lpCamera == NULL )
		return;
	ASSERT( lpCamera != NULL );
	// 如果不是摄像头设备，直接重绘客户区...
	if( !lpCamera->IsCameraDevice() ) {
		this->Invalidate(true);
		return;
	}
	// 从配置文件中获取原始标题栏...
	ASSERT( m_lpParentDlg != NULL );
	GM_MapData theMapLoc;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetCamera(nCameraID, theMapLoc);
	string & strName = theMapLoc["Name"];
	// 重新组合标题栏名称...
	m_strYunTitle.Format("云台 - %s", strName.c_str());
	m_strSetTitle.Format("设置 - %s", strName.c_str());
	// 重绘背景的文字和按钮...
	/*CDC * pDC = this->GetDC();
	CString & strLogStatus = lpCamera->GetLogStatus();
	this->ShowText(pDC, true, strLogStatus);
	this->ShowButton(true);
	this->ReleaseDC(pDC);*/
	// 如果已经登录，对登录区和按钮进行处理...
	this->StateButton(lpCamera->IsLogin());
	// 把IP地址/端口/用户名/密码，显示在Edit框中...
	TCHAR szDecodePass[MAX_PATH] = {0};
	string & strIPAddr = theMapLoc["IPv4Address"];
	string & strCmdPort = theMapLoc["CommandPort"];
	string & strLoginUser = theMapLoc["LoginUser"];
	string & strLoginPass = theMapLoc["LoginPass"];
	int nDecLen = Base64decode(szDecodePass, strLoginPass.c_str());
	m_editIPAddr.SetWindowText(strIPAddr.c_str());
	m_editIPPort.SetWindowText(strCmdPort.c_str());
	m_editUserName.SetWindowText(strLoginUser.c_str());
	m_editPassWord.SetWindowText(szDecodePass);
	// 通知重绘非客户区...
	//m_editIPAddr.SendMessage(WM_NCPAINT, 0, 0);
	//m_editIPPort.SendMessage(WM_NCPAINT, 0, 0);
	//m_editUserName.SendMessage(WM_NCPAINT, 0, 0);
	//m_editPassWord.SendMessage(WM_NCPAINT, 0, 0);
	// 通知重绘整个背景区域...
	this->Invalidate(true);
}
//
// 根据登录状态来设置按钮状态...
void CRightView::StateButton(BOOL bIsLogin)
{
	m_editIPAddr.SetReadOnly(bIsLogin);
	m_editIPPort.SetReadOnly(bIsLogin);
	m_editUserName.SetReadOnly(bIsLogin);
	m_editPassWord.SetReadOnly(bIsLogin);
	
	m_btnLogin.EnableWindow(!bIsLogin);
	m_btnLogout.EnableWindow(bIsLogin);

	m_btnYunAuto.EnableWindow(bIsLogin);
	m_btnYunTop.EnableWindow(bIsLogin);
	m_btnYunReset.EnableWindow(bIsLogin);
	m_btnYunLeft.EnableWindow(bIsLogin);
	m_btnYunBottom.EnableWindow(bIsLogin);
	m_btnYunRight.EnableWindow(bIsLogin);

	m_btnJiaoMinus.EnableWindow(bIsLogin);
	m_btnJiaoPlus.EnableWindow(bIsLogin);
	m_btnFangMinus.EnableWindow(bIsLogin);
	m_btnFangPlus.EnableWindow(bIsLogin);
	m_btnQuanMinus.EnableWindow(bIsLogin);
	m_btnQuanPlus.EnableWindow(bIsLogin);

	m_btnSetVPreview.EnableWindow(bIsLogin);
	m_btnSetVParam.EnableWindow(bIsLogin);
	m_btnSetRecord.EnableWindow(bIsLogin);
	m_btnSetReview.EnableWindow(bIsLogin);
	m_btnSetAlert.EnableWindow(bIsLogin);
	m_btnSetCamera.EnableWindow(bIsLogin);
}
//
// 初始化各个操作按钮...
BOOL CRightView::InitButton(CFont * lpFont)
{
	// 设置当前窗口的字体...
	this->SetFont(lpFont);

	//TEXTMETRIC tm = {0};
	//pDC->GetTextMetrics(&tm);

	// 计算垂直高度...
	CRect rectText, rcClient, rcButton;
	int   nVClientHeight = 0;
	ASSERT( lpFont != NULL );
	rectText.SetRectEmpty();
    CDC *pDC = this->GetDC();
    CFont *pOld = pDC->SelectObject(lpFont);
    pDC->DrawText("Ky", rectText, DT_CALCRECT | DT_LEFT);
	nVClientHeight = rectText.Height();
	pDC->SelectObject(pOld);
    this->ReleaseDC(pDC);

	// 创建登录区编辑框对象...
	rcClient.SetRect(85, 45, 85 + kEditWidth, 45 + kEditHigh);
	m_editIPAddr.SetVClientHeight(nVClientHeight);
	m_editIPAddr.Create(WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_CENTER, rcClient, this, kEditIPAddr);
	m_editIPAddr.SetFont(lpFont, true);
	//m_editIPAddr.SetMargins(5, 0);

	rcClient.top = rcClient.bottom + 5;
	rcClient.bottom = rcClient.top + kEditHigh;
	m_editIPPort.SetVClientHeight(nVClientHeight);
	m_editIPPort.Create(WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_CENTER|ES_NUMBER, rcClient, this, kEditIPPort);
	m_editIPPort.SetFont(lpFont, true);

	rcClient.top = rcClient.bottom + 5;
	rcClient.bottom = rcClient.top + kEditHigh;
	m_editUserName.SetVClientHeight(nVClientHeight);
	m_editUserName.Create(WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_CENTER, rcClient, this, kEditUserName);
	m_editUserName.SetFont(lpFont, true);
	
	rcClient.top = rcClient.bottom + 5;
	rcClient.bottom = rcClient.top + kEditHigh;
	m_editPassWord.SetVClientHeight(nVClientHeight);
	m_editPassWord.Create(WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_CENTER|ES_PASSWORD, rcClient, this, kEditUserPass);
	m_editPassWord.SetFont(lpFont, true);
	//m_editPassWord.EnableWindow(false);
	//m_editPassWord.SetReadOnly(true);

	// 创建创建登录区的按钮...
	rcButton.SetRect(40, 180, 40 + kBtnLogWidth, 180 + kBtnLogHigh);
	m_btnLogin.Create("登 录", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonLogin);
	m_btnLogin.SetFont(lpFont, true);
	rcButton.left  = rcButton.right + 30;
	rcButton.right = rcButton.left  + kBtnLogWidth;
	m_btnLogout.Create("注 销", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonLogout);
	m_btnLogout.SetFont(lpFont, true);

	// 创建云台区的按钮...
	rcButton.SetRect(kLeftMargin, 280, kLeftMargin + kBtnYunWidth, 280 + kBtnYunHigh);
	m_btnYunAuto.Create("自 动", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonYunAuto);
	m_btnYunAuto.SetFont(lpFont, true);
	rcButton.left  = rcButton.right + 10;
	rcButton.right = rcButton.left  + kBtnYunWidth;
	m_btnYunTop.Create("↑上移", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonYunUp);
	m_btnYunTop.SetFont(lpFont, true);
	rcButton.left  = rcButton.right + 10;
	rcButton.right = rcButton.left  + kBtnYunWidth;
	m_btnYunReset.Create("自 动", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonYunReset);
	m_btnYunReset.SetFont(lpFont, true);

	rcButton.bottom += 10;
	rcButton.SetRect(kLeftMargin, rcButton.bottom, kLeftMargin + kBtnYunWidth, rcButton.bottom + kBtnYunHigh);
	m_btnYunLeft.Create("←左移", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonYunLeft);
	m_btnYunLeft.SetFont(lpFont, true);

	rcButton.left  = rcButton.right + 10;
	rcButton.right = rcButton.left  + kBtnYunWidth;
	m_btnYunBottom.Create("↓下移", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonYunDown);
	m_btnYunBottom.SetFont(lpFont, true);

	rcButton.left  = rcButton.right + 10;
	rcButton.right = rcButton.left  + kBtnYunWidth;
	m_btnYunRight.Create("→右移", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonYunRight);
	m_btnYunRight.SetFont(lpFont, true);

	rcButton.bottom += 10;
	rcButton.SetRect(kLeftMargin, rcButton.bottom, kLeftMargin + kBtnYunWidth, rcButton.bottom + kBtnYunHigh);
	m_btnJiaoMinus.Create("- 减少", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonJiaoMinus);
	m_btnJiaoMinus.SetFont(lpFont, true);

	rcButton.left  = rcButton.right + 10 + 10 + kBtnYunWidth;
	rcButton.right = rcButton.left  + kBtnYunWidth;
	m_btnJiaoPlus.Create("+ 增加", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonJiaoPlus);
	m_btnJiaoPlus.SetFont(lpFont, true);

	rcButton.bottom += 10;
	rcButton.SetRect(kLeftMargin, rcButton.bottom, kLeftMargin + kBtnYunWidth, rcButton.bottom + kBtnYunHigh);
	m_btnFangMinus.Create("- 减少", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonFangMinus);
	m_btnFangMinus.SetFont(lpFont, true);

	rcButton.left  = rcButton.right + 10 + 10 + kBtnYunWidth;
	rcButton.right = rcButton.left  + kBtnYunWidth;
	m_btnFangPlus.Create("+ 增加", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonFangPlus);
	m_btnFangPlus.SetFont(lpFont, true);

	rcButton.bottom += 10;
	rcButton.SetRect(kLeftMargin, rcButton.bottom, kLeftMargin + kBtnYunWidth, rcButton.bottom + kBtnYunHigh);
	m_btnQuanMinus.Create("- 减少", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonQuanMinus);
	m_btnQuanMinus.SetFont(lpFont, true);

	rcButton.left  = rcButton.right + 10 + 10 + kBtnYunWidth;
	rcButton.right = rcButton.left  + kBtnYunWidth;
	m_btnQuanPlus.Create("+ 增加", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonQuanPlus);
	m_btnQuanPlus.SetFont(lpFont, true);
	//m_btnQuanPlus.EnableWindow(false);

	// 创建设置区按钮...
	rcButton.SetRect(kLeftMargin, 490, kLeftMargin + kBtnSetWidth, 490 + kBtnSetHigh);
	m_btnSetVPreview.Create("视频预览", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonSetVPreview);
	m_btnSetVPreview.SetFont(lpFont, true);

	rcButton.left  = rcButton.right + 10;
	rcButton.right = rcButton.left  + kBtnSetWidth;
	m_btnSetVParam.Create("视频参数", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonSetVParam);
	m_btnSetVParam.SetFont(lpFont, true);

	rcButton.bottom += 10;
	rcButton.SetRect(kLeftMargin, rcButton.bottom, kLeftMargin + kBtnSetWidth, rcButton.bottom + kBtnSetHigh);
	m_btnSetRecord.Create("存储设置", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonSetRecord);
	m_btnSetRecord.SetFont(lpFont, true);

	rcButton.left  = rcButton.right + 10;
	rcButton.right = rcButton.left  + kBtnSetWidth;
	m_btnSetReview.Create("存储回放", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonSetReview);
	m_btnSetReview.SetFont(lpFont, true);

	rcButton.bottom += 10;
	rcButton.SetRect(kLeftMargin, rcButton.bottom, kLeftMargin + kBtnSetWidth, rcButton.bottom + kBtnSetHigh);
	m_btnSetAlert.Create("报警布防", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonSetAlert);
	m_btnSetAlert.SetFont(lpFont, true);

	rcButton.left  = rcButton.right + 10;
	rcButton.right = rcButton.left  + kBtnSetWidth;
	m_btnSetCamera.Create("硬件设置", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonSetCamera);
	m_btnSetCamera.SetFont(lpFont, true);

	return true;
}
//
// 响应点击事件操作...
void CRightView::OnClickButtonWnd(UINT nItemID)
{
	switch( nItemID )
	{
	case kButtonLogin:		this->doDeviceLogin();  break;
	case kButtonLogout:		this->doDeviceLogout(); break;

	case kButtonSetVPreview: this->doClickSetVPreview(); break;
	case kButtonSetVParam:   this->doClickSetVParam(); break;
	case kButtonSetRecord:   this->doClickSetRecord(); break;
	case kButtonSetReview:   this->doClickSetReview(); break;
	case kButtonSetAlert:    this->doClickSetAlert(); break;
	case kButtonSetCamera:   this->doClickSetCamera(); break;
	default:				 /* do nothing... */ break;
	} 
}
//
// 响应按钮左键按下|抬起的操作 => 是由CXPButton发送的消息通知...
LRESULT	CRightView::OnMsgDeviceBtnLDownUp(WPARAM wParam, LPARAM lParam)
{
	switch(wParam)
	{
	case kButtonYunAuto:  this->doClickBtnPTZ(PAN_AUTO, lParam);  break;
	case kButtonYunReset: this->doClickBtnPTZ(PAN_AUTO, lParam);  break;
	case kButtonYunUp:    this->doClickBtnPTZ(TILT_UP, lParam);   break;
	case kButtonYunDown:  this->doClickBtnPTZ(TILT_DOWN, lParam); break;
	case kButtonYunLeft:  this->doClickBtnPTZ(PAN_LEFT, lParam);  break;
	case kButtonYunRight: this->doClickBtnPTZ(PAN_RIGHT, lParam); break;
	case kButtonJiaoMinus: this->doClickBtnPTZ(ZOOM_IN, lParam);  break;
	case kButtonJiaoPlus:  this->doClickBtnPTZ(ZOOM_OUT, lParam); break;
	case kButtonFangMinus: this->doClickBtnPTZ(FOCUS_NEAR, lParam); break;
	case kButtonFangPlus:  this->doClickBtnPTZ(FOCUS_FAR, lParam); break;
	case kButtonQuanMinus: this->doClickBtnPTZ(IRIS_OPEN, lParam); break;
	case kButtonQuanPlus:  this->doClickBtnPTZ(IRIS_CLOSE, lParam); break;
	default:  /* do nothing... */ break;
	}
	return S_OK;
}
//
// 处理异步登录成功的消息通知...
LRESULT	CRightView::OnMsgDeviceLoginResult(WPARAM wParam, LPARAM lParam)
{
	// 针对频道编号有效时的处理...
	ASSERT( wParam > 0 );
	DWORD dwReturn = GM_NoErr;
	CCamera * lpCamera = m_lpParentDlg->FindCameraByID(wParam);
	if( lpCamera == NULL ) {
		this->Invalidate(true);
		return S_OK;
	}
	// 异步登录失败的处理过程...
	if( lParam <= 0 || !lpCamera->IsLogin() ) {
		// 焦点窗口与登录窗口是一致的，更新按钮状态和文字状态...
		if( wParam == m_lpParentDlg->GetFocusCamera() ) {
			this->StateButton(lpCamera->IsLogin());
			this->Invalidate(true);
		}
		// 不用关闭自动连接时钟，会根据错误码自动检测...
		return S_OK;
	}
	// 通知DVR：异步登录成功...
	ASSERT( lParam > 0 && lpCamera->IsLogin() );
	dwReturn = lpCamera->onDeviceLoginSuccess();
	// 焦点窗口与登录窗口不一致，直接返回...
	if( wParam != m_lpParentDlg->GetFocusCamera() ) {
		this->Invalidate(true);
		return S_OK;
	}
	// 焦点窗口与登录窗口一致...
	ASSERT( wParam == m_lpParentDlg->GetFocusCamera() );
	if( dwReturn != GM_NoErr ) {
		// 登录失败的处理过程...
		m_editIPAddr.SetReadOnly(false);
		m_editIPPort.SetReadOnly(false);
		m_editUserName.SetReadOnly(false);
		m_editPassWord.SetReadOnly(false);
		m_btnLogin.EnableWindow(true);
	} else {
		// 登录成功的处理过程，显示功能按钮...
		this->StateButton(lpCamera->IsLogin());
	}
	// 重新刷新背景区域...
	this->Invalidate(true);
	return S_OK;
}
//
// 自动连接DVR...
void CRightView::doAutoCheckDVR()
{
	// 判断马上登录的DVR是否就是焦点DVR...
	int  nCameraID = m_nCurAutoID;
	BOOL bIsFocusCamera = ((m_lpParentDlg->GetFocusCamera() == nCameraID) ? true : false);
	do {
		// 查找ID对应的DVR对象...
		CCamera * lpCamera = m_lpParentDlg->FindCameraByID(nCameraID);
		if( lpCamera == NULL || lpCamera->IsLogin() )
			break;
		ASSERT( !lpCamera->IsLogin() );
		// 需要对不同类型的窗口进行不同的处理...
		if( lpCamera->IsCameraDevice() ) {
			// 如果是摄像机的密码错误或已经被锁定，直接跳过...
			DWORD dwErrCode = lpCamera->GetHKErrCode();
			if( dwErrCode == NET_DVR_PASSWORD_ERROR || dwErrCode == NET_DVR_USER_LOCKED )
				break;
			// 进行摄像机连接数据的准备工作...
			GM_MapData theMapLoc;
			CXmlConfig & theConfig = CXmlConfig::GMInstance();
			theConfig.GetCamera(nCameraID, theMapLoc);
			TCHAR szDecodePass[MAX_PATH] = {0};
			string & strIPAddr = theMapLoc["IPv4Address"];
			string & strCmdPort = theMapLoc["CommandPort"];
			string & strLoginUser = theMapLoc["LoginUser"];
			string & strLoginPass = theMapLoc["LoginPass"];
			int nDecLen = Base64decode(szDecodePass, strLoginPass.c_str());
			int nCmdPort = ((strCmdPort.size() <= 0) ? 0 : atoi(strCmdPort.c_str()));
			// 焦点窗口 => 将所有的登录区按钮置为只读或灰色...
			if( bIsFocusCamera ) {
				m_editIPAddr.SetReadOnly(true);
				m_editIPPort.SetReadOnly(true);
				m_editUserName.SetReadOnly(true);
				m_editPassWord.SetReadOnly(true);
				m_btnLogin.EnableWindow(false);
			}
			// 开始进行登录操作...
			DWORD dwReturn = lpCamera->doDeviceLogin(this->m_hWnd, strIPAddr.c_str(), nCmdPort, strLoginUser.c_str(), szDecodePass);
			if( dwReturn != GM_NoErr ) {
				// 焦点窗口 => 登录失败的处理过程...
				if( bIsFocusCamera ) {
					m_editIPAddr.SetReadOnly(false);
					m_editIPPort.SetReadOnly(false);
					m_editUserName.SetReadOnly(false);
					m_editPassWord.SetReadOnly(false);
					m_btnLogin.EnableWindow(true);
				}
				// 记录错误编号...
				MsgLogGM(dwReturn);
				break;
			}
		} else {
			// 处理流转发模式的自动连接操作...
			GM_Error theErr = GM_NoErr;
			ASSERT( !lpCamera->IsCameraDevice() );
			theErr = lpCamera->doStreamLogin();
			// 连接失败，只是打印信息...
			if( theErr != GM_NoErr ) {
				MsgLogGM(theErr);
			}
		}
		// 焦点窗口 => 登录成功的处理过程，显示功能按钮，重新刷新背景区域...
		if( bIsFocusCamera ) {
			this->StateButton(lpCamera->IsLogin());
			this->Invalidate(true);
		}
	}while( false );
	// 自动累加DVR编号，注意编号回滚...
	m_nCurAutoID = m_lpParentDlg->GetNextAutoID(nCameraID);
}
//
// 点击摄像头设备登录事件...
void CRightView::doDeviceLogin()
{
	// 获取登录配置参数...
	CString strIPAddr, strCmdPort;
	CString strLoginUser, strLoginPass;
	m_editIPAddr.GetWindowText(strIPAddr);
	m_editIPPort.GetWindowText(strCmdPort);
	m_editUserName.GetWindowText(strLoginUser);
	m_editPassWord.GetWindowText(strLoginPass);
	// 判断登录参数的有效性...
	if( strIPAddr.IsEmpty() ) {
		MessageBox("【IP地址】不能为空！", "错误", MB_OK | MB_ICONSTOP);
		m_editIPAddr.SetFocus();
		return;
	}
	int nCmdPort = atoi(strCmdPort);
	if( nCmdPort <= 0 || nCmdPort >= 65535 ) {
		MessageBox("【端口】无效，请确认后重新输入！", "错误", MB_OK | MB_ICONSTOP);
		m_editIPPort.SetFocus();
		return;
	}
	if( strLoginUser.IsEmpty() || strLoginPass.IsEmpty() ) {
		MessageBox("【用户名】或【密码】不能为空！", "错误", MB_OK | MB_ICONSTOP);
		m_editUserName.SetFocus();
		return;
	}
	// 对密码进行简单的本地加密处理...
	TCHAR szEncode[MAX_PATH] = {0};
	int nEncLen = Base64encode(szEncode, strLoginPass, strLoginPass.GetLength());
	// 获取有效的DVR对象...
	GM_MapData theMapLoc;
	int nCameraID = m_lpParentDlg->GetFocusCamera();
	CCamera * lpCamera = m_lpParentDlg->FindCameraByID(nCameraID);
	// 如果已经处于登录状态，直接返回...
	if( lpCamera == NULL || lpCamera->IsLogin() )
		return;
	ASSERT( lpCamera != NULL && !lpCamera->IsLogin() );
	// 保存这些已经验证过的有效参数...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetCamera(nCameraID, theMapLoc);
	theMapLoc["IPv4Address"] = strIPAddr;
	theMapLoc["CommandPort"] = strCmdPort;
	theMapLoc["LoginUser"] = strLoginUser;
	theMapLoc["LoginPass"] = szEncode;
	theConfig.SetCamera(nCameraID, theMapLoc);
	theConfig.GMSaveConfig();
	// 将所有的按钮置为只读或灰色...
	m_editIPAddr.SetReadOnly(true);
	m_editIPPort.SetReadOnly(true);
	m_editUserName.SetReadOnly(true);
	m_editPassWord.SetReadOnly(true);
	m_btnLogin.EnableWindow(false);
	// 开始进行登录操作...
	DWORD dwReturn = lpCamera->doDeviceLogin(this->m_hWnd, strIPAddr, nCmdPort, strLoginUser, strLoginPass);
	if( dwReturn != GM_NoErr ) {
		// 登录失败的处理过程...
		m_editIPAddr.SetReadOnly(false);
		m_editIPPort.SetReadOnly(false);
		m_editUserName.SetReadOnly(false);
		m_editPassWord.SetReadOnly(false);
		m_btnLogin.EnableWindow(true);
	} else {
		// 登录成功的处理过程，显示功能按钮...
		this->StateButton(lpCamera->IsLogin());
	}
	// 重新刷新背景区域...
	this->Invalidate(true);
}
//
// 点击摄像头设备注销事件...
void CRightView::doDeviceLogout()
{
	// 获取有效的DVR对象...
	int nCameraID = m_lpParentDlg->GetFocusCamera();
	CCamera * lpCamera = m_lpParentDlg->FindCameraByID(nCameraID);
	if( lpCamera == NULL )
		return;
	ASSERT( lpCamera != NULL );
	// 调用DVR退出接口函数...
	lpCamera->doDeviceLogout();
	// 获取DVR的当前配置信息...
	GM_MapData   theMapLoc;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetCamera(nCameraID, theMapLoc);
	// 设置按钮和文字状态...
	this->StateButton(lpCamera->IsLogin());
	// 重新刷新背景区域...
	this->Invalidate(true);
}
//
// 处理云台操作...
void CRightView::doClickBtnPTZ(DWORD dwPTZCmd, BOOL bStop)
{
	//TRACE("Command = %lu, Stop = %lu\n", dwPTZCmd, bStop);

	// 获取有效的DVR对象...
	int nCameraID = m_lpParentDlg->GetFocusCamera();
	CCamera * lpCamera = m_lpParentDlg->FindCameraByID(nCameraID);
	if( lpCamera == NULL )
		return;
	ASSERT( lpCamera != NULL );
	lpCamera->doDevicePTZCmd(dwPTZCmd, bStop);
}
//
// 点击视频预览
void CRightView::doClickSetVPreview()
{
}
//
// 点击视频参数
void CRightView::doClickSetVParam()
{
}
//
// 点击存储设置
void CRightView::doClickSetRecord()
{
}
//
// 点击存储回放
void CRightView::doClickSetReview()
{
}
//
// 点击报警布防
void CRightView::doClickSetAlert()
{
}
//
// 点击摄像头设置 => 硬件设置
void CRightView::doClickSetCamera()
{
}
