
#include "stdafx.h"
#include "HaoYi.h"
#include "LeftViewDlg.h"

IMPLEMENT_DYNCREATE(CLeftViewDlg, CFormView)

CLeftViewDlg::CLeftViewDlg()
	: CFormView(CLeftViewDlg::IDD)
	, m_hRootItem(NULL)
{
}

CLeftViewDlg::~CLeftViewDlg()
{
	m_ImageList.DeleteImageList();
}

void CLeftViewDlg::DoDataExchange(CDataExchange* pDX)
{
	CFormView::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_QR_CODE, m_QRStatic);
	DDX_Control(pDX, IDC_TREE_DEVICE, m_DeviceTree);
}

BEGIN_MESSAGE_MAP(CLeftViewDlg, CFormView)
	ON_WM_SIZE()
	ON_WM_TIMER()
	ON_WM_MOUSEMOVE()
	ON_NOTIFY(NM_RCLICK, IDC_TREE_DEVICE, &CLeftViewDlg::OnRclickTreeDevice)
	ON_NOTIFY(TVN_SELCHANGED, IDC_TREE_DEVICE, &CLeftViewDlg::OnSelchangedTreeDevice)
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

#ifdef _DEBUG
void CLeftViewDlg::AssertValid() const
{
	CFormView::AssertValid();
}

void CLeftViewDlg::Dump(CDumpContext& dc) const
{
	CFormView::Dump(dc);
}
#endif //_DEBUG

void CLeftViewDlg::OnInitialUpdate()
{
	CFormView::OnInitialUpdate();

	// 创建图片列表对象...
	CBitmap bitTemp;
	m_ImageList.Create(25, 23, ILC_COLORDDB |ILC_MASK, 1, 1);
	bitTemp.LoadBitmap(IDB_BITMAP_LEFTVIEW);
	m_ImageList.Add(&bitTemp, RGB(252,2,252));
	bitTemp.DeleteObject();

	// 设置设备树的背景色/文字色/图片对象...
	m_DeviceTree.SetBkColor(WND_BK_COLOR);
	m_DeviceTree.SetTextColor(RGB(255, 255, 255));
	m_DeviceTree.SetImageList(&m_ImageList, LVSIL_NORMAL);

	// 修改设备树的属性...
	m_DeviceTree.ModifyStyle(NULL,  WS_VISIBLE | WS_TABSTOP | WS_CHILD | WS_BORDER
		| TVS_HASBUTTONS | TVS_LINESATROOT | TVS_HASLINES|TVS_SHOWSELALWAYS   
		| TVS_DISABLEDRAGDROP|TVS_FULLROWSELECT|TVS_TRACKSELECT);

	// 初始化设备树的根节点...
	m_DeviceTree.DeleteAllItems();
	m_hRootItem	= m_DeviceTree.InsertItem("设备列表管理", 0, 0);		

	m_DeviceTree.InsertItem("摄像头-1", 1, 1, m_hRootItem);
	m_DeviceTree.InsertItem("摄像头-2", 1, 1, m_hRootItem);
	m_DeviceTree.InsertItem("摄像头-3", 1, 1, m_hRootItem);
	m_DeviceTree.InsertItem("摄像头-4", 1, 1, m_hRootItem);
	//this->SetTimer(1, 500, NULL);

	m_DeviceTree.Expand(m_hRootItem,TVE_EXPAND);
	m_DeviceTree.SelectItem(m_hRootItem);

	// 创建设备树的提示对象...
	m_tipTree.Create(this);
	m_tipTree.AddTool(&m_DeviceTree, "点鼠标右键可以弹出操作菜单");
	m_tipTree.SetDelayTime(TTDT_INITIAL, 100);
}

void CLeftViewDlg::OnSize(UINT nType, int cx, int cy)
{
	CFormView::OnSize(nType, cx, cy);

	// 对控件进行位置移动操作...
	if( m_DeviceTree.m_hWnd != NULL && m_QRStatic.m_hWnd != NULL ) {
		m_DeviceTree.SetWindowPos(this, 0, 0, cx, cy - QR_CODE_CY, SWP_NOZORDER | SWP_NOACTIVATE);
		m_QRStatic.SetWindowPos(this, 0,  cy - QR_CODE_CY, cx, QR_CODE_CY, SWP_NOZORDER | SWP_NOACTIVATE);
		m_QRStatic.Invalidate(true);
	}
}

void CLeftViewDlg::OnTimer(UINT_PTR nIDEvent)
{
	//m_DeviceTree.SetItemImage(g_hFirstItem, g_nIndex+2, g_nIndex+2);
	//++g_nIndex;
	//g_nIndex = g_nIndex % 3;

	CFormView::OnTimer(nIDEvent);
}

BOOL CLeftViewDlg::PreTranslateMessage(MSG* pMsg)
{
	// 让设备树有效显示信息...
	if( m_tipTree.m_hWnd != NULL ) {
		m_tipTree.RelayEvent(pMsg);
	}
	return CFormView::PreTranslateMessage(pMsg);
}

void CLeftViewDlg::OnMouseMove(UINT nFlags, CPoint point)
{
	CFormView::OnMouseMove(nFlags, point);
}

void CLeftViewDlg::OnRclickTreeDevice(NMHDR *pNMHDR, LRESULT *pResult)
{
	*pResult = 0;
}

void CLeftViewDlg::OnSelchangedTreeDevice(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);
	*pResult = 0;
}

BOOL CLeftViewDlg::OnEraseBkgnd(CDC* pDC)
{
	//return CFormView::OnEraseBkgnd(pDC);
	return true;
}
