
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
	m_strYunTitle = "��̨";
	m_strSetTitle = "����";
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
	// ��ȡ��ǰ�Ľ���ͨ�����...
	ASSERT( m_lpParentDlg != NULL );
	int nFocusCameraID = m_lpParentDlg->GetFocusCamera();
	CCamera * lpCamera = m_lpParentDlg->FindCameraByID(nFocusCameraID);
	CString & strLogStatus = ((lpCamera != NULL) ? lpCamera->GetLogStatus() : "��¼");

	// ����������������...
	CRect rcRect;
	this->GetClientRect(rcRect);
	pDC->FillSolidRect(&rcRect, WND_BK_COLOR);
	
	// ��������豸����ֱ�����ذ�ť�����ƻ���������Ϣ...
	if( lpCamera != NULL && !lpCamera->IsCameraDevice() ) {
		this->ShowButton(false);
		this->DrawStreamText(pDC);
		return TRUE;
	}
	// ��ʾ�����ذ�ť����...
	this->ShowButton((nFocusCameraID <= 0) ? false : true);
	// ��ʾ��ص�������Ϣ...
	this->ShowText(pDC, (nFocusCameraID <= 0) ? false : true, strLogStatus);

	return TRUE;
}
//
// ��ʾ��ת��ʱ��������Ϣ...
void CRightView::DrawStreamText(CDC * pDC)
{
	// ��ȡ����������...
	CRect rcRect;
	CSize rcPos, rcSize;
	this->GetClientRect(rcRect);

	// ׼��������Ϣ...
	CFont * lpFont = this->GetFont();
	CFont * hOldFont = pDC->SelectObject(lpFont);

	// ��������ģʽ...
	pDC->SetTextColor(RGB(255, 255, 255));
	pDC->SetBkMode(TRANSPARENT);

	// ��¼ - ���ⱳ��ɫ...
	//COLORREF clrValue = RGB( 96, 123, 189);
	//rcRect.bottom = rcRect.top + kGroundHigh;
	//rcRect.DeflateRect(1,1);
	//pDC->FillSolidRect(rcRect, clrValue);
	// ��¼ - ��������...
	//rcSize = pDC->GetOutputTextExtent(inLogStatus);
	//rcPos.cx = (rcRect.Width() - rcSize.cx) / 2;
	//rcPos.cy = (rcRect.Height() - rcSize.cy) / 2;
	//pDC->TextOut(kLeftMargin - 5, rcPos.cy+1, inLogStatus);

	// ������ת����Ϣ...
	this->GetClientRect(rcRect);
	CString strNotice = "����ͷ�豸������";
	rcSize = pDC->GetOutputTextExtent(strNotice);
	rcPos.cx = (rcRect.Width() - rcSize.cx) / 2;
	rcPos.cy = (rcRect.Height() - rcSize.cy) / 2;
	pDC->SetTextColor(RGB(255, 255, 0));
	pDC->TextOut(rcPos.cx, rcPos.cy - 50, strNotice);

	// �ָ������ʽ...
	pDC->SelectObject(hOldFont);
}
//
// ��ʾ������Ϣ...
void CRightView::ShowText(CDC * pDC, BOOL bFocus, CString & inLogStatus)
{
	// ���û��ͨ����ֱ�ӷ���...
	if( !bFocus ) return;

	// ��ȡ����������...
	CRect rcRect;
	CSize rcPos, rcSize;
	this->GetClientRect(rcRect);

	// ׼��������Ϣ...
	CFont * lpFont = this->GetFont();
	CFont * hOldFont = pDC->SelectObject(lpFont);

	// ��������ģʽ...
	pDC->SetTextColor(RGB(255, 255, 255));
	pDC->SetBkMode(TRANSPARENT);

	// ���Ƿǽ��������ֱ��� => ���У�������չ15������...
	/*CRect rcNone;
	rcNone = rcRect;
	rcNone.top = rcRect.Height()/2 - 15;
	rcNone.bottom = rcRect.Height()/2 + 15;
	pDC->FillSolidRect(&rcNone, WND_BK_COLOR);*/

	// ��¼ - ���ⱳ��ɫ...
	COLORREF clrValue = RGB( 96, 123, 189);
	rcRect.bottom = rcRect.top + kGroundHigh;
	rcRect.DeflateRect(1,1);
	pDC->FillSolidRect(rcRect, clrValue);
	// ��¼ - ��������...
	rcSize = pDC->GetOutputTextExtent(inLogStatus);
	rcPos.cx = (rcRect.Width() - rcSize.cx) / 2;
	rcPos.cy = (rcRect.Height() - rcSize.cy) / 2;
	pDC->TextOut(kLeftMargin - 5, rcPos.cy+1, inLogStatus);

	// ��ʾ�༭������...
	rcPos.SetSize(25, kGroundHigh + 20);
	pDC->TextOut(rcPos.cx, rcPos.cy, "IP��ַ��");
	rcPos.cy += kEditHigh + 5;
	pDC->TextOut(rcPos.cx, rcPos.cy, "��  �ڣ�");
	rcPos.cy += kEditHigh + 5;
	pDC->TextOut(rcPos.cx, rcPos.cy, "�û�����");
	rcPos.cy += kEditHigh + 5;
	pDC->TextOut(rcPos.cx, rcPos.cy, "��  �룺");

	// ��̨ - ���ⱳ��ɫ...
	rcRect.top = 235;
	rcRect.bottom = rcRect.top + kGroundHigh;
	pDC->FillSolidRect(rcRect, clrValue);
	// ��̨ - ��������...
	rcSize = pDC->GetOutputTextExtent(m_strYunTitle);
	rcPos.cx = (rcRect.Width() - rcSize.cx) / 2;
	rcPos.cy = (rcRect.Height() - rcSize.cy) / 2;
	pDC->TextOut(kLeftMargin - 5, rcRect.top + rcPos.cy + 1, m_strYunTitle);

	// ���� - ���ⱳ��...
	/*rcRect.top = 445;
	rcRect.bottom = rcRect.top + kGroundHigh;
	pDC->FillSolidRect(rcRect, clrValue);
	// ���� - ��������...
	rcSize = pDC->GetOutputTextExtent(m_strSetTitle);
	rcPos.cx = (rcRect.Width() - rcSize.cx) / 2;
	rcPos.cy = (rcRect.Height() - rcSize.cy) / 2;
	pDC->TextOut(kLeftMargin - 5, rcRect.top + rcPos.cy + 1, m_strSetTitle);*/

	// ��ʾ����/�Ŵ�/��Ȧ...
	rcSize = pDC->GetOutputTextExtent("����");
	rcPos.cx = kLeftMargin + kBtnYunWidth + 10 + (kBtnYunWidth - rcSize.cx) / 2;
	rcPos.cy = 340 + (kBtnYunHigh - rcSize.cy) / 2;
	pDC->TextOut(rcPos.cx, rcPos.cy + 1 +  0, "����");
	pDC->TextOut(rcPos.cx, rcPos.cy + 1 + (kBtnYunHigh + 10)*1, "�Ŵ�");
	pDC->TextOut(rcPos.cx, rcPos.cy + 1 + (kBtnYunHigh + 10)*2, "��Ȧ");
	
	// �ָ������ʽ...
	pDC->SelectObject(hOldFont);
}
//
// ��ʾ��ť����...
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

	// 2017.07.31 - by jackey => ����������ͷ��������...
	bDispFlag = false;
	m_btnSetVPreview.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);
	m_btnSetVParam.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);
	m_btnSetRecord.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);
	m_btnSetReview.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);
	m_btnSetAlert.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);
	m_btnSetCamera.ShowWindow(bDispFlag ? SW_SHOW : SW_HIDE);
}
//
// ��Ӧ��Ƶ���㴰�ڱ��...
void CRightView::doFocusCamera(int nCameraID)
{
	// ��ȡ CCamera ����...
	CCamera * lpCamera = m_lpParentDlg->FindCameraByID(nCameraID);
	if( lpCamera == NULL )
		return;
	ASSERT( lpCamera != NULL );
	// �����������ͷ�豸��ֱ���ػ�ͻ���...
	if( !lpCamera->IsCameraDevice() ) {
		this->Invalidate(true);
		return;
	}
	// �������ļ��л�ȡԭʼ������...
	ASSERT( m_lpParentDlg != NULL );
	GM_MapData theMapLoc;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetCamera(nCameraID, theMapLoc);
	string & strName = theMapLoc["Name"];
	// ������ϱ���������...
	m_strYunTitle.Format("��̨ - %s", strName.c_str());
	m_strSetTitle.Format("���� - %s", strName.c_str());
	// �ػ汳�������ֺͰ�ť...
	/*CDC * pDC = this->GetDC();
	CString & strLogStatus = lpCamera->GetLogStatus();
	this->ShowText(pDC, true, strLogStatus);
	this->ShowButton(true);
	this->ReleaseDC(pDC);*/
	// ����Ѿ���¼���Ե�¼���Ͱ�ť���д���...
	this->StateButton(lpCamera->IsLogin());
	// ��IP��ַ/�˿�/�û���/���룬��ʾ��Edit����...
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
	// ֪ͨ�ػ�ǿͻ���...
	//m_editIPAddr.SendMessage(WM_NCPAINT, 0, 0);
	//m_editIPPort.SendMessage(WM_NCPAINT, 0, 0);
	//m_editUserName.SendMessage(WM_NCPAINT, 0, 0);
	//m_editPassWord.SendMessage(WM_NCPAINT, 0, 0);
	// ֪ͨ�ػ�������������...
	this->Invalidate(true);
}
//
// ���ݵ�¼״̬�����ð�ť״̬...
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
// ��ʼ������������ť...
BOOL CRightView::InitButton(CFont * lpFont)
{
	// ���õ�ǰ���ڵ�����...
	this->SetFont(lpFont);

	//TEXTMETRIC tm = {0};
	//pDC->GetTextMetrics(&tm);

	// ���㴹ֱ�߶�...
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

	// ������¼���༭�����...
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

	// ����������¼���İ�ť...
	rcButton.SetRect(40, 180, 40 + kBtnLogWidth, 180 + kBtnLogHigh);
	m_btnLogin.Create("�� ¼", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonLogin);
	m_btnLogin.SetFont(lpFont, true);
	rcButton.left  = rcButton.right + 30;
	rcButton.right = rcButton.left  + kBtnLogWidth;
	m_btnLogout.Create("ע ��", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonLogout);
	m_btnLogout.SetFont(lpFont, true);

	// ������̨���İ�ť...
	rcButton.SetRect(kLeftMargin, 280, kLeftMargin + kBtnYunWidth, 280 + kBtnYunHigh);
	m_btnYunAuto.Create("�� ��", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonYunAuto);
	m_btnYunAuto.SetFont(lpFont, true);
	rcButton.left  = rcButton.right + 10;
	rcButton.right = rcButton.left  + kBtnYunWidth;
	m_btnYunTop.Create("������", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonYunUp);
	m_btnYunTop.SetFont(lpFont, true);
	rcButton.left  = rcButton.right + 10;
	rcButton.right = rcButton.left  + kBtnYunWidth;
	m_btnYunReset.Create("�� ��", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonYunReset);
	m_btnYunReset.SetFont(lpFont, true);

	rcButton.bottom += 10;
	rcButton.SetRect(kLeftMargin, rcButton.bottom, kLeftMargin + kBtnYunWidth, rcButton.bottom + kBtnYunHigh);
	m_btnYunLeft.Create("������", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonYunLeft);
	m_btnYunLeft.SetFont(lpFont, true);

	rcButton.left  = rcButton.right + 10;
	rcButton.right = rcButton.left  + kBtnYunWidth;
	m_btnYunBottom.Create("������", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonYunDown);
	m_btnYunBottom.SetFont(lpFont, true);

	rcButton.left  = rcButton.right + 10;
	rcButton.right = rcButton.left  + kBtnYunWidth;
	m_btnYunRight.Create("������", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonYunRight);
	m_btnYunRight.SetFont(lpFont, true);

	rcButton.bottom += 10;
	rcButton.SetRect(kLeftMargin, rcButton.bottom, kLeftMargin + kBtnYunWidth, rcButton.bottom + kBtnYunHigh);
	m_btnJiaoMinus.Create("- ����", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonJiaoMinus);
	m_btnJiaoMinus.SetFont(lpFont, true);

	rcButton.left  = rcButton.right + 10 + 10 + kBtnYunWidth;
	rcButton.right = rcButton.left  + kBtnYunWidth;
	m_btnJiaoPlus.Create("+ ����", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonJiaoPlus);
	m_btnJiaoPlus.SetFont(lpFont, true);

	rcButton.bottom += 10;
	rcButton.SetRect(kLeftMargin, rcButton.bottom, kLeftMargin + kBtnYunWidth, rcButton.bottom + kBtnYunHigh);
	m_btnFangMinus.Create("- ����", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonFangMinus);
	m_btnFangMinus.SetFont(lpFont, true);

	rcButton.left  = rcButton.right + 10 + 10 + kBtnYunWidth;
	rcButton.right = rcButton.left  + kBtnYunWidth;
	m_btnFangPlus.Create("+ ����", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonFangPlus);
	m_btnFangPlus.SetFont(lpFont, true);

	rcButton.bottom += 10;
	rcButton.SetRect(kLeftMargin, rcButton.bottom, kLeftMargin + kBtnYunWidth, rcButton.bottom + kBtnYunHigh);
	m_btnQuanMinus.Create("- ����", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonQuanMinus);
	m_btnQuanMinus.SetFont(lpFont, true);

	rcButton.left  = rcButton.right + 10 + 10 + kBtnYunWidth;
	rcButton.right = rcButton.left  + kBtnYunWidth;
	m_btnQuanPlus.Create("+ ����", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonQuanPlus);
	m_btnQuanPlus.SetFont(lpFont, true);
	//m_btnQuanPlus.EnableWindow(false);

	// ������������ť...
	rcButton.SetRect(kLeftMargin, 490, kLeftMargin + kBtnSetWidth, 490 + kBtnSetHigh);
	m_btnSetVPreview.Create("��ƵԤ��", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonSetVPreview);
	m_btnSetVPreview.SetFont(lpFont, true);

	rcButton.left  = rcButton.right + 10;
	rcButton.right = rcButton.left  + kBtnSetWidth;
	m_btnSetVParam.Create("��Ƶ����", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonSetVParam);
	m_btnSetVParam.SetFont(lpFont, true);

	rcButton.bottom += 10;
	rcButton.SetRect(kLeftMargin, rcButton.bottom, kLeftMargin + kBtnSetWidth, rcButton.bottom + kBtnSetHigh);
	m_btnSetRecord.Create("�洢����", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonSetRecord);
	m_btnSetRecord.SetFont(lpFont, true);

	rcButton.left  = rcButton.right + 10;
	rcButton.right = rcButton.left  + kBtnSetWidth;
	m_btnSetReview.Create("�洢�ط�", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonSetReview);
	m_btnSetReview.SetFont(lpFont, true);

	rcButton.bottom += 10;
	rcButton.SetRect(kLeftMargin, rcButton.bottom, kLeftMargin + kBtnSetWidth, rcButton.bottom + kBtnSetHigh);
	m_btnSetAlert.Create("��������", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonSetAlert);
	m_btnSetAlert.SetFont(lpFont, true);

	rcButton.left  = rcButton.right + 10;
	rcButton.right = rcButton.left  + kBtnSetWidth;
	m_btnSetCamera.Create("Ӳ������", WS_VISIBLE|WS_TABSTOP, rcButton, this, kButtonSetCamera);
	m_btnSetCamera.SetFont(lpFont, true);

	return true;
}
//
// ��Ӧ����¼�����...
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
// ��Ӧ��ť�������|̧��Ĳ��� => ����CXPButton���͵���Ϣ֪ͨ...
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
// �����첽��¼�ɹ�����Ϣ֪ͨ...
LRESULT	CRightView::OnMsgDeviceLoginResult(WPARAM wParam, LPARAM lParam)
{
	// ���Ƶ�������Чʱ�Ĵ���...
	ASSERT( wParam > 0 );
	DWORD dwReturn = GM_NoErr;
	CCamera * lpCamera = m_lpParentDlg->FindCameraByID(wParam);
	if( lpCamera == NULL ) {
		this->Invalidate(true);
		return S_OK;
	}
	// �첽��¼ʧ�ܵĴ������...
	if( lParam <= 0 || !lpCamera->IsLogin() ) {
		// ���㴰�����¼������һ�µģ����°�ť״̬������״̬...
		if( wParam == m_lpParentDlg->GetFocusCamera() ) {
			this->StateButton(lpCamera->IsLogin());
			this->Invalidate(true);
		}
		// ���ùر��Զ�����ʱ�ӣ�����ݴ������Զ����...
		return S_OK;
	}
	// ֪ͨDVR���첽��¼�ɹ�...
	ASSERT( lParam > 0 && lpCamera->IsLogin() );
	dwReturn = lpCamera->onDeviceLoginSuccess();
	// ���㴰�����¼���ڲ�һ�£�ֱ�ӷ���...
	if( wParam != m_lpParentDlg->GetFocusCamera() ) {
		this->Invalidate(true);
		return S_OK;
	}
	// ���㴰�����¼����һ��...
	ASSERT( wParam == m_lpParentDlg->GetFocusCamera() );
	if( dwReturn != GM_NoErr ) {
		// ��¼ʧ�ܵĴ������...
		m_editIPAddr.SetReadOnly(false);
		m_editIPPort.SetReadOnly(false);
		m_editUserName.SetReadOnly(false);
		m_editPassWord.SetReadOnly(false);
		m_btnLogin.EnableWindow(true);
	} else {
		// ��¼�ɹ��Ĵ�����̣���ʾ���ܰ�ť...
		this->StateButton(lpCamera->IsLogin());
	}
	// ����ˢ�±�������...
	this->Invalidate(true);
	return S_OK;
}
//
// �Զ�����DVR...
void CRightView::doAutoCheckDVR()
{
	// �ж����ϵ�¼��DVR�Ƿ���ǽ���DVR...
	int  nCameraID = m_nCurAutoID;
	BOOL bIsFocusCamera = ((m_lpParentDlg->GetFocusCamera() == nCameraID) ? true : false);
	do {
		// ����ID��Ӧ��DVR����...
		CCamera * lpCamera = m_lpParentDlg->FindCameraByID(nCameraID);
		if( lpCamera == NULL || lpCamera->IsLogin() )
			break;
		ASSERT( !lpCamera->IsLogin() );
		// ��Ҫ�Բ�ͬ���͵Ĵ��ڽ��в�ͬ�Ĵ���...
		if( lpCamera->IsCameraDevice() ) {
			// ���������������������Ѿ���������ֱ������...
			DWORD dwErrCode = lpCamera->GetHKErrCode();
			if( dwErrCode == NET_DVR_PASSWORD_ERROR || dwErrCode == NET_DVR_USER_LOCKED )
				break;
			// ����������������ݵ�׼������...
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
			// ���㴰�� => �����еĵ�¼����ť��Ϊֻ�����ɫ...
			if( bIsFocusCamera ) {
				m_editIPAddr.SetReadOnly(true);
				m_editIPPort.SetReadOnly(true);
				m_editUserName.SetReadOnly(true);
				m_editPassWord.SetReadOnly(true);
				m_btnLogin.EnableWindow(false);
			}
			// ��ʼ���е�¼����...
			DWORD dwReturn = lpCamera->doDeviceLogin(this->m_hWnd, strIPAddr.c_str(), nCmdPort, strLoginUser.c_str(), szDecodePass);
			if( dwReturn != GM_NoErr ) {
				// ���㴰�� => ��¼ʧ�ܵĴ������...
				if( bIsFocusCamera ) {
					m_editIPAddr.SetReadOnly(false);
					m_editIPPort.SetReadOnly(false);
					m_editUserName.SetReadOnly(false);
					m_editPassWord.SetReadOnly(false);
					m_btnLogin.EnableWindow(true);
				}
				// ��¼������...
				MsgLogGM(dwReturn);
				break;
			}
		} else {
			// ������ת��ģʽ���Զ����Ӳ���...
			GM_Error theErr = GM_NoErr;
			ASSERT( !lpCamera->IsCameraDevice() );
			theErr = lpCamera->doStreamLogin();
			// ����ʧ�ܣ�ֻ�Ǵ�ӡ��Ϣ...
			if( theErr != GM_NoErr ) {
				MsgLogGM(theErr);
			}
		}
		// ���㴰�� => ��¼�ɹ��Ĵ�����̣���ʾ���ܰ�ť������ˢ�±�������...
		if( bIsFocusCamera ) {
			this->StateButton(lpCamera->IsLogin());
			this->Invalidate(true);
		}
	}while( false );
	// �Զ��ۼ�DVR��ţ�ע���Żع�...
	m_nCurAutoID = m_lpParentDlg->GetNextAutoID(nCameraID);
}
//
// �������ͷ�豸��¼�¼�...
void CRightView::doDeviceLogin()
{
	// ��ȡ��¼���ò���...
	CString strIPAddr, strCmdPort;
	CString strLoginUser, strLoginPass;
	m_editIPAddr.GetWindowText(strIPAddr);
	m_editIPPort.GetWindowText(strCmdPort);
	m_editUserName.GetWindowText(strLoginUser);
	m_editPassWord.GetWindowText(strLoginPass);
	// �жϵ�¼��������Ч��...
	if( strIPAddr.IsEmpty() ) {
		MessageBox("��IP��ַ������Ϊ�գ�", "����", MB_OK | MB_ICONSTOP);
		m_editIPAddr.SetFocus();
		return;
	}
	int nCmdPort = atoi(strCmdPort);
	if( nCmdPort <= 0 || nCmdPort >= 65535 ) {
		MessageBox("���˿ڡ���Ч����ȷ�Ϻ��������룡", "����", MB_OK | MB_ICONSTOP);
		m_editIPPort.SetFocus();
		return;
	}
	if( strLoginUser.IsEmpty() || strLoginPass.IsEmpty() ) {
		MessageBox("���û����������롿����Ϊ�գ�", "����", MB_OK | MB_ICONSTOP);
		m_editUserName.SetFocus();
		return;
	}
	// ��������м򵥵ı��ؼ��ܴ���...
	TCHAR szEncode[MAX_PATH] = {0};
	int nEncLen = Base64encode(szEncode, strLoginPass, strLoginPass.GetLength());
	// ��ȡ��Ч��DVR����...
	GM_MapData theMapLoc;
	int nCameraID = m_lpParentDlg->GetFocusCamera();
	CCamera * lpCamera = m_lpParentDlg->FindCameraByID(nCameraID);
	// ����Ѿ����ڵ�¼״̬��ֱ�ӷ���...
	if( lpCamera == NULL || lpCamera->IsLogin() )
		return;
	ASSERT( lpCamera != NULL && !lpCamera->IsLogin() );
	// ������Щ�Ѿ���֤������Ч����...
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetCamera(nCameraID, theMapLoc);
	theMapLoc["IPv4Address"] = strIPAddr;
	theMapLoc["CommandPort"] = strCmdPort;
	theMapLoc["LoginUser"] = strLoginUser;
	theMapLoc["LoginPass"] = szEncode;
	theConfig.SetCamera(nCameraID, theMapLoc);
	theConfig.GMSaveConfig();
	// �����еİ�ť��Ϊֻ�����ɫ...
	m_editIPAddr.SetReadOnly(true);
	m_editIPPort.SetReadOnly(true);
	m_editUserName.SetReadOnly(true);
	m_editPassWord.SetReadOnly(true);
	m_btnLogin.EnableWindow(false);
	// ��ʼ���е�¼����...
	DWORD dwReturn = lpCamera->doDeviceLogin(this->m_hWnd, strIPAddr, nCmdPort, strLoginUser, strLoginPass);
	if( dwReturn != GM_NoErr ) {
		// ��¼ʧ�ܵĴ������...
		m_editIPAddr.SetReadOnly(false);
		m_editIPPort.SetReadOnly(false);
		m_editUserName.SetReadOnly(false);
		m_editPassWord.SetReadOnly(false);
		m_btnLogin.EnableWindow(true);
	} else {
		// ��¼�ɹ��Ĵ�����̣���ʾ���ܰ�ť...
		this->StateButton(lpCamera->IsLogin());
	}
	// ����ˢ�±�������...
	this->Invalidate(true);
}
//
// �������ͷ�豸ע���¼�...
void CRightView::doDeviceLogout()
{
	// ��ȡ��Ч��DVR����...
	int nCameraID = m_lpParentDlg->GetFocusCamera();
	CCamera * lpCamera = m_lpParentDlg->FindCameraByID(nCameraID);
	if( lpCamera == NULL )
		return;
	ASSERT( lpCamera != NULL );
	// ����DVR�˳��ӿں���...
	lpCamera->doDeviceLogout();
	// ��ȡDVR�ĵ�ǰ������Ϣ...
	GM_MapData   theMapLoc;
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	theConfig.GetCamera(nCameraID, theMapLoc);
	// ���ð�ť������״̬...
	this->StateButton(lpCamera->IsLogin());
	// ����ˢ�±�������...
	this->Invalidate(true);
}
//
// ������̨����...
void CRightView::doClickBtnPTZ(DWORD dwPTZCmd, BOOL bStop)
{
	//TRACE("Command = %lu, Stop = %lu\n", dwPTZCmd, bStop);

	// ��ȡ��Ч��DVR����...
	int nCameraID = m_lpParentDlg->GetFocusCamera();
	CCamera * lpCamera = m_lpParentDlg->FindCameraByID(nCameraID);
	if( lpCamera == NULL )
		return;
	ASSERT( lpCamera != NULL );
	lpCamera->doDevicePTZCmd(dwPTZCmd, bStop);
}
//
// �����ƵԤ��
void CRightView::doClickSetVPreview()
{
}
//
// �����Ƶ����
void CRightView::doClickSetVParam()
{
}
//
// ����洢����
void CRightView::doClickSetRecord()
{
}
//
// ����洢�ط�
void CRightView::doClickSetReview()
{
}
//
// �����������
void CRightView::doClickSetAlert()
{
}
//
// �������ͷ���� => Ӳ������
void CRightView::doClickSetCamera()
{
}
