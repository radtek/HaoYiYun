
#include "stdafx.h"
#include "HaoYi.h"
#include "BitItem.h"
#include "MidView.h"
#include "MidViewDlg.h"

IMPLEMENT_DYNCREATE(CMidViewDlg, CFormView)
//IMPLEMENT_DYNAMIC(CMidViewDlg, CFormView)

CMidViewDlg::CMidViewDlg()
  : CFormView(CMidViewDlg::IDD)
  , m_lpMidView(NULL)
{
	memset(m_BitArray, 0, sizeof(void *) * CVideoWnd::kBitNum);
}

CMidViewDlg::~CMidViewDlg()
{
	this->DestroyResource();
}

void CMidViewDlg::DoDataExchange(CDataExchange* pDX)
{
	CFormView::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CMidViewDlg, CFormView)
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	//ON_WM_LBUTTONUP()
	//ON_WM_LBUTTONDOWN()
	//ON_WM_MOUSEMOVE()
END_MESSAGE_MAP()

void CMidViewDlg::OnInitialUpdate()
{
	CFormView::OnInitialUpdate();

	this->BuildResource();
}

void CMidViewDlg::BuildResource()
{
	this->DestroyResource();

	LOGFONT	 lf = {0};
	lf.lfHeight = 14;
	lf.lfWeight = 0;
	strcpy(lf.lfFaceName, "ËÎÌå");
	m_VideoFont.CreateFontIndirect(&lf);
	ASSERT( m_VideoFont.m_hObject != NULL );

	m_BitArray[CVideoWnd::kZoomOut] = new CBitItem(IDB_VIDEO_MAX, 18, 18);
	m_BitArray[CVideoWnd::kZoomIn]	= new CBitItem(IDB_VIDEO_MIN, 18, 18);
	m_BitArray[CVideoWnd::kClose]   = new CBitItem(IDB_VIDEO_CLOSE, 18, 18);

	ASSERT( m_lpMidView == NULL );
	m_lpMidView = new CMidView();
	ASSERT( m_lpMidView != NULL );

	CString strTitle = "¼à¿ØÍ¨µÀ";
	m_lpMidView->SetParentWnd(this);
	m_lpMidView->SetVideoTitle(strTitle);

	m_lpMidView->Create(WS_CHILD|WS_VISIBLE, CRect(0, 0, 0, 0),
						  this, ID_RIGHT_VIEW_BEGIN);
	ASSERT( m_lpMidView->m_hWnd != NULL );
	m_lpMidView->ShowWindow(SW_SHOW);
}

void CMidViewDlg::DestroyResource()
{
	if( m_lpMidView != NULL ) {
		delete m_lpMidView;
		m_lpMidView = NULL;
	}

	for(int i = 0; i < CVideoWnd::kBitNum; i++) {
		if( m_BitArray[i] == NULL )
			continue;
		delete m_BitArray[i];
		m_BitArray[i] = NULL;
	}

	(m_VideoFont.m_hObject != NULL) ? m_VideoFont.DeleteObject() : NULL;
}

void CMidViewDlg::OnSize(UINT nType, int cx, int cy)
{
	CFormView::OnSize(nType, cx, cy);

	if( m_lpMidView == NULL )
		return;
	CRect rectT;
	this->GetClientRect(rectT);
	m_lpMidView->doMoveWindow(rectT);
}

BOOL CMidViewDlg::OnEraseBkgnd(CDC* pDC)
{
	/*if (m_lpRightView != NULL)
	{
		RECT rectClient = {0};
		RECT rectView = {0};
		RECT rectTemp = {0};
		this->GetClientRect(&rectClient);
		m_lpRightView->GetWindowRect(&rectView);
		ScreenToClient(&rectView);
		SetRect(&rectTemp, 0, rectView.bottom, rectClient.right, rectClient.bottom);
		pDC->FillSolidRect(&rectTemp, RGB(224,224,224));
	}*/

	return true;
	//return CDialog::OnEraseBkgnd(pDC);
}

/*void CMidViewDlg::OnLButtonUp(UINT nFlags, CPoint point)
{
	CDialog::OnLButtonUp(nFlags, point);
}

void CMidViewDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
	CDialog::OnLButtonDown(nFlags, point);
}

void CMidViewDlg::OnMouseMove(UINT nFlags, CPoint point)
{
	CDialog::OnMouseMove(nFlags, point);
}

HCURSOR	CMidViewDlg::GetSysHandCursor()
{
	TCHAR		strWinDir[MAX_PATH] = {0};
	HCURSOR		hHandCursor			= NULL;
	hHandCursor = ::LoadCursor(NULL, MAKEINTRESOURCE(32649));
	
	// Still no cursor handle - load the WinHelp hand cursor
	if( hHandCursor == NULL )
	{
		GetWindowsDirectory(strWinDir, MAX_PATH);
		strcat(strWinDir, _T("\\winhlp32.exe"));
		
		// This retrieves cursor #106 from winhlp32.exe, which is a hand pointer
		HMODULE hModule = ::LoadLibrary(strWinDir);
		DWORD	dwErr = GetLastError();
		if( hModule != NULL )
		{
			HCURSOR	 hTempCur = ::LoadCursor(hModule, MAKEINTRESOURCE(106));
			hHandCursor = (hTempCur != NULL) ? CopyCursor(hTempCur) : NULL;
			FreeLibrary(hModule);
		}
	}
	return hHandCursor;
}*/