
#include "stdafx.h"
#include "HaoYi.h"
#include "BitItem.h"
#include "RightView.h"
#include "PaneContent.h"

IMPLEMENT_DYNAMIC(CPaneContent, CDialog)

CPaneContent::CPaneContent(CWnd* pParent /*=NULL*/)
  : CDialog(CPaneContent::IDD, pParent)
  , m_rcSplitMidNew(0,0,0,0)
  , m_rcSplitMidSrc(0,0,0,0)
  , m_bMidDraging(false)
  , m_hCurVerSplit(NULL)
  , m_lpRightView(NULL)
  , m_hHandCur(NULL)
{
	m_hCurVerSplit = AfxGetApp()->LoadStandardCursor(IDC_SIZENS);
	ASSERT( m_hCurVerSplit != NULL );

	memset(m_BitArray, 0, sizeof(void *) * CVideoWnd::kBitNum);
	m_hVideoBack  = ::CreateSolidBrush(RGB(96, 123, 189));
}

CPaneContent::~CPaneContent()
{
	this->DestroyResource();
}

void CPaneContent::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PREV_WND, m_PrevWnd);
	DDX_Control(pDX, IDC_LIST_CTRL, m_ListCtrl);
}

BEGIN_MESSAGE_MAP(CPaneContent, CDialog)
	ON_WM_SIZE()
	ON_WM_LBUTTONUP()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

HCURSOR	CPaneContent::GetSysHandCursor()
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
}

BOOL CPaneContent::OnInitDialog()
{
	CDialog::OnInitDialog();

	this->BuildResource();
	this->BuildListCtrl();

	return TRUE;
}

void CPaneContent::BuildResource()
{
	this->DestroyResource();

	CBitmap bitTemp;
	bitTemp.LoadBitmap(IDB_BITMAP_LIST);
	m_ListImage.Create(16, 16, ILC_COLORDDB|ILC_MASK, 1, 1);
	m_ListImage.Add(&bitTemp, RGB(252,2,252));
	bitTemp.DeleteObject();

	LOGFONT	 lf = {0};
	lf.lfHeight = 14;
	lf.lfWeight = 0;
	strcpy(lf.lfFaceName, "宋体");
	m_VideoFont.CreateFontIndirect(&lf);
	ASSERT( m_VideoFont.m_hObject != NULL );

	ASSERT( m_hHandCur == NULL );
	m_hHandCur  = this->GetSysHandCursor();
	ASSERT( m_hHandCur != NULL );	
	
	m_BitArray[CVideoWnd::kZoomOut] = new CBitItem(IDB_VIDEO_MAX, 18, 18);
	m_BitArray[CVideoWnd::kZoomIn]	= new CBitItem(IDB_VIDEO_MIN, 18, 18);
	m_BitArray[CVideoWnd::kClose]   = new CBitItem(IDB_VIDEO_CLOSE, 18, 18);

	ASSERT( m_lpRightView == NULL );
	m_lpRightView = new CRightView();
	ASSERT( m_lpRightView != NULL );

	CString strTitle = "监控通道";
	m_lpRightView->SetParentWnd(this);
	m_lpRightView->SetVideoTitle(strTitle);

	m_lpRightView->Create(WS_CHILD|WS_VISIBLE, CRect(0, 0, 0, 0),
						  &m_PrevWnd, ID_RIGHT_VIEW_BEGIN);
	ASSERT( m_lpRightView->m_hWnd != NULL );
	m_lpRightView->ShowWindow(SW_SHOW);
}

void CPaneContent::BuildListCtrl()
{
	CRect		crRect;
	this->GetClientRect(&crRect);

	int			nWidth = crRect.Width() / 5;
	CString		strColumn;

	strColumn.Format("频道名称,%d;开始时间,%d;发送码率(Kbps),%d;音视频信息,%d;发布地址,%d;接入端口,%d;运行方式,%d;", 
					 nWidth + 50, nWidth + 60, nWidth + 30, nWidth + 20, nWidth + 90, nWidth, nWidth + 90);

	m_ListCtrl.SetHeadings(strColumn, LVCFMT_LEFT);
	m_ListCtrl.LoadColumnInfo();
	m_ListCtrl.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_HEADERDRAGDROP); //| LVS_EX_GRIDLINES);

	COLORREF corBack = _COLOR_BK;
	m_ListCtrl.SetBkColor(corBack);
	m_ListCtrl.SetTextBkColor(corBack);
	m_ListCtrl.SetTextColor(RGB(255, 255, 255));
	m_ListCtrl.SetImageList(&m_ListImage, LVSIL_SMALL);
}

void CPaneContent::DestroyResource()
{
	if( m_lpRightView != NULL ) {
		delete m_lpRightView;
		m_lpRightView = NULL;
	}

	for(int i = 0; i < CVideoWnd::kBitNum; i++) {
		if( m_BitArray[i] == NULL )
			continue;
		delete m_BitArray[i];
		m_BitArray[i] = NULL;
	}

	(m_ListImage.m_hImageList != NULL ) ? m_ListImage.DeleteImageList() : NULL;
	(m_VideoFont.m_hObject != NULL) ? m_VideoFont.DeleteObject() : NULL;
	(m_hHandCur != NULL) ? DestroyCursor(m_hHandCur) : NULL;
	m_hHandCur = NULL;
}

void CPaneContent::DrawSplitBar(LPRECT lpRect)
{
	CDC dc;
	dc.Attach(::GetDC(NULL));
	ASSERT( dc.GetSafeHdc() );
	
	CRect	rect(lpRect);
	this->ClientToScreen(rect);
	do
	{
		dc.DrawFocusRect(rect);
		rect.DeflateRect(1, 1);
	}while (rect.Size().cx > 0 && rect.Size().cy > 0);
	
	HDC hDC = dc.Detach();
	::ReleaseDC(NULL, hDC);
}

void CPaneContent::OnMidSplitEnd(int nOffset)
{
	CRect	rectTemp, rcMain;

	m_ListCtrl.GetWindowRect(rectTemp);
	this->ScreenToClient(rectTemp);
	m_ListCtrl.SetWindowPos(NULL, rectTemp.left, rectTemp.top + nOffset, rectTemp.Width(), rectTemp.Height() - nOffset, SWP_NOZORDER);
	
	m_PrevWnd.GetWindowRect(rectTemp);
	this->ScreenToClient(rectTemp);
	m_PrevWnd.SetWindowPos(NULL, 0, 0, rectTemp.Width(), rectTemp.Height() + nOffset, SWP_NOZORDER | SWP_NOMOVE);
	m_PrevWnd.GetClientRect(&rectTemp);
	
	m_lpRightView->doMoveWindow(rectTemp);
	
	this->GetClientRect(rcMain);
	m_PrevWnd.GetWindowRect(rectTemp);
	this->ScreenToClient(rectTemp);
	
	m_rcSplitMidSrc.SetRect(0, rectTemp.bottom, rcMain.Width() - 2, rectTemp.bottom + kSplitterHeight);
	m_rcSplitMidNew.SetRect(0, rectTemp.bottom, rcMain.Width() - 2, rectTemp.bottom + kSplitterHeight);

	m_ListCtrl.Invalidate();
}

BOOL CPaneContent::IsMidCanSplit(CPoint & point)
{
	CRect	rcTop, rcBot;
	CPoint	ptTop, ptBot;
	
	ptTop = point;
	ptBot = point;
	
	m_ListCtrl.GetClientRect(rcTop);
	//rcTop.top += 30;
	m_ListCtrl.ScreenToClient(&ptTop);
	
	m_PrevWnd.GetClientRect(rcBot);
	//rcBot.bottom -= 20;
	m_PrevWnd.ScreenToClient(&ptBot);
	
	return (rcTop.PtInRect(ptTop) || rcBot.PtInRect(ptBot));
}

void CPaneContent::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);

	if( m_ListCtrl.m_hWnd == NULL || m_PrevWnd.m_hWnd == NULL )
		return;
	int nListHeight = QR_CODE_HIGH - kSplitterHeight*2;
	int nPrevHeight = cy - kSplitterHeight - nListHeight;
	m_PrevWnd.MoveWindow(0, 0, cx, nPrevHeight);
	m_ListCtrl.MoveWindow(0, nPrevHeight + kSplitterHeight, cx, nListHeight);
	
	CRect	rectT;
	m_PrevWnd.GetClientRect(rectT);
	m_lpRightView->doMoveWindow(rectT);

	m_rcSplitMidSrc.SetRect(0, nPrevHeight, cx - 2, nPrevHeight + kSplitterHeight);
	m_rcSplitMidNew.SetRect(0, nPrevHeight, cx - 2, nPrevHeight + kSplitterHeight);
}

void CPaneContent::OnLButtonUp(UINT nFlags, CPoint point)
{
	if( m_bMidDraging )
	{
		::ReleaseCapture();
		this->DrawSplitBar(m_rcSplitMidNew);
		this->DrawSplitBar(m_rcSplitMidNew);
		
		this->OnMidSplitEnd(m_rcSplitMidNew.bottom - m_rcSplitMidSrc.bottom);
		m_bMidDraging = FALSE;
	}
	CDialog::OnLButtonUp(nFlags, point);
}

void CPaneContent::OnLButtonDown(UINT nFlags, CPoint point)
{
	if( m_rcSplitMidNew.PtInRect(point) )
	{
		this->SetCapture();
		m_bMidDraging = TRUE;
		::SetCursor(m_hCurVerSplit);
		//TRACE("OnLButtonDown(Drag)\n");
	}
	CDialog::OnLButtonDown(nFlags, point);
}

void CPaneContent::OnMouseMove(UINT nFlags, CPoint point)
{
	// 是否需要设置拖动图标...
	if( m_rcSplitMidNew.PtInRect(point) || m_bMidDraging ) {
		::SetCursor(m_hCurVerSplit);
	}

	if( m_bMidDraging )
	{
		//TRACE("Enter OnMouseMove(Drag)\n");
		CRect	rcDraw = m_rcSplitMidNew;
		CPoint	ptTemp = point;
		this->ClientToScreen(&ptTemp);
		if( !this->IsMidCanSplit(ptTemp) )
			return;
		rcDraw.bottom	= point.y + rcDraw.Height()/2;
		rcDraw.top		= rcDraw.bottom - m_rcSplitMidNew.Height();
		DrawSplitBar(m_rcSplitMidNew);
		m_rcSplitMidNew = rcDraw;
		this->DrawSplitBar(m_rcSplitMidNew);
		//TRACE("Exit OnMouseMove(Drag)\n");
	}
	//TRACE("OnMouseMove()\n");
	CDialog::OnMouseMove(nFlags, point);
}

BOOL CPaneContent::OnEraseBkgnd(CDC* pDC)
{
	// 绘制分隔符...
	pDC->FillSolidRect(&m_rcSplitMidSrc, RGB(224,224,224));
	pDC->FillSolidRect(&m_rcSplitMidNew, RGB(224,224,224));

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
