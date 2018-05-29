
#include "stdafx.h"
#include "HaoYi.h"
#include "DlgRoom.h"
#include "WebThread.h"
#include "XmlConfig.h"

IMPLEMENT_DYNAMIC(CDlgRoom, CDialogEx)

CDlgRoom::CDlgRoom(CWnd* pParent /*=NULL*/)
  : CDialogEx(CDlgRoom::IDD, pParent)
  , m_nState(kStateInit)
  , m_lpRoomThread(NULL)
  , m_nSelRoomID(-1)
{

}

CDlgRoom::~CDlgRoom()
{
	if( m_RoomImage.m_hImageList != NULL ) {
		m_RoomImage.DeleteImageList();
	}
}

void CDlgRoom::DoDataExchange(CDataExchange* pDX)
{
	DDX_Control(pDX, IDC_LIST_ROOM, m_ListRoom);
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CDlgRoom, CDialogEx)
	ON_WM_TIMER()
	ON_WM_ERASEBKGND()
	ON_BN_CLICKED(IDOK, &CDlgRoom::OnBtnClickedOk)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_ROOM, &CDlgRoom::OnItemchangedListRoom)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST_ROOM, &CDlgRoom::OnDblclkListRoom)
END_MESSAGE_MAP()

BOOL CDlgRoom::OnInitDialog()
{
	// 必须先调用基类的初始化接口...
	CDialogEx::OnInitDialog();
	// 设置图标...
	this->SetIcon(AfxGetApp()->LoadIcon(IDR_MAINFRAME), FALSE);
	// 加载任务图标文件...
	CBitmap bitTemp;
	m_RoomImage.DeleteImageList();
	m_RoomImage.Create(24, 24, ILC_COLOR8|ILC_MASK, 1, 1);
	
	bitTemp.LoadBitmap(IDB_BITMAP_CHECK);
	m_RoomImage.Add(&bitTemp, RGB(255, 255, 255));
	ASSERT( m_RoomImage.m_hImageList != NULL );
	bitTemp.DeleteObject();

	// 初始化直播间列表框...
	m_ListRoom.SetExtendedStyle(LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES);
	m_ListRoom.SetImageList(&m_RoomImage, LVSIL_SMALL);

	RECT rect = {0};
	m_ListRoom.GetClientRect(&rect);

	int nWidth = rect.right / 5;
	
	m_ListRoom.InsertColumn(kRoomID, "教室", LVCFMT_LEFT, nWidth - 40);
	m_ListRoom.InsertColumn(kRoomLesson, "课程", LVCFMT_LEFT, nWidth + 40);
	m_ListRoom.InsertColumn(kRoomTeacher, "讲师", LVCFMT_LEFT, nWidth - 40);
	m_ListRoom.InsertColumn(kRoomStartTime, "开始时间", LVCFMT_LEFT, nWidth + 10);
	m_ListRoom.InsertColumn(kRoomEndTime, "结束时间", LVCFMT_LEFT, nWidth + 10);

	// 发起一个查询直播间列表的时钟任务...
	this->SetTimer(kTimerLiveRoom, 100, NULL);

	// 隐藏控件对象，并激发画背景...
	m_ListRoom.ShowWindow(SW_HIDE);
	GetDlgItem(IDOK)->ShowWindow(SW_HIDE);
	GetDlgItem(IDCANCEL)->ShowWindow(SW_HIDE);
	this->Invalidate();

	return true;
}
//
// 绘制背景...
BOOL CDlgRoom::OnEraseBkgnd(CDC* pDC)
{
	// 根据不同状态显示不同的状态信息
	if( m_nState != kStateFinish ) {
		CString strText = "正在获取直播间列表...";
		if( m_nState == kStateError ) {
			strText = "获取直播间列表失败！";
		}
		this->ShowText(pDC, strText);
		return true;
	}
	// 如果获取直播间已完成，直接调用基础类...
	ASSERT( m_nState == kStateFinish );
	return CDialogEx::OnEraseBkgnd(pDC);
}
//
// 显示文字信息 => 显示区域、文字颜色、字体大小、是否粗体...
void CDlgRoom::ShowText(CDC * pDC, CString & inTxtData)
{
	// 如果文字为空，直接返回...
	if( inTxtData.GetLength() <= 0 )
		return;
	// 设置文字颜色和字体大小...
	COLORREF inTxtColor = RGB(0, 0, 0);
	int inFontSize = 18;
	bool isBold = true;
	// 获取文字显示区域...
	CRect inDispRect;
	this->GetClientRect(&inDispRect);
	inDispRect.top = inDispRect.Height() / 2 - 30;
	inDispRect.bottom = inDispRect.top + 30;
	// 准备字体对象...
	CFont   fontLogo;
	CFont * pOldFont = NULL;
	pDC->SetBkMode(TRANSPARENT);
	// 创建字体 => 根据输入的参数配置...
	fontLogo.CreateFont(inFontSize, 0, 0, 0, (isBold ? FW_BOLD : FW_LIGHT), false, false,0,0,0,0,0,0, "宋体");
	// 设置文字字体和文字颜色...
	pOldFont = pDC->SelectObject(&fontLogo);
	pDC->SetTextColor( inTxtColor );
	// 绘制文字信息 => 多行显示...
	pDC->DrawText(inTxtData, inDispRect, DT_EXTERNALLEADING | DT_WORDBREAK | DT_CENTER | DT_VCENTER);
	// 销毁字体...
	pDC->SelectObject(pOldFont);
	fontLogo.DeleteObject();
	fontLogo.Detach();
}

void CDlgRoom::OnTimer(UINT_PTR nIDEvent)
{
	if( nIDEvent == kTimerLiveRoom ) {
		// 先关闭时钟任务...
		KillTimer( kTimerLiveRoom );
		// 如果线程对象无效，直接返回...
		if( m_lpRoomThread == NULL ) {
			m_nState = kStateError;
			return;
		}
		// 发起从服务器获取直播间列表的命令...
		if( !m_lpRoomThread->doWebGetLiveRoom() ) {
			m_nState = kStateError;
			return;
		}
		// 如果获取直播间列表成功，显示在界面上...
		GM_MapRoom::iterator itorRoom;
		CXmlConfig & theConfig = CXmlConfig::GMInstance();
		GM_MapRoom & theMapRoom = theConfig.GetMapLiveRoom();
		int nCurSelRoomID = theConfig.GetCurSelRoomID();
		for(itorRoom = theMapRoom.begin(); itorRoom != theMapRoom.end(); ++itorRoom) {
			CString strRoomID;
			int nRoomID = itorRoom->first;
			GM_MapData & theData = itorRoom->second;
			strRoomID.Format("%d", nRoomID);
			m_ListRoom.InsertItem(0, strRoomID, kImageWait);
			m_ListRoom.SetItemData(0, nRoomID);

			m_ListRoom.SetItemText(0, kRoomLesson, theData["lesson_name"].c_str());
			m_ListRoom.SetItemText(0, kRoomTeacher, theData["teacher_name"].c_str());
			m_ListRoom.SetItemText(0, kRoomStartTime, theData["start_time"].c_str());
			m_ListRoom.SetItemText(0, kRoomEndTime, theData["end_time"].c_str());

			/*GM_MapData::iterator itorData;
			for(itorData = theData.begin(); itorData != theData.end(); ++itorData) {
				if( stricmp("lesson_name", itorData->first.c_str() ) == 0 ) {
					m_ListRoom.SetItemText(0, kRoomLesson, itorData->second.c_str());
				} else if( stricmp("teacher_name", itorData->first.c_str() ) == 0 ) {
					m_ListRoom.SetItemText(0, kRoomTeacher, itorData->second.c_str());
				} else if( stricmp("start_time", itorData->first.c_str() ) == 0 ) {
					m_ListRoom.SetItemText(0, kRoomStartTime, itorData->second.c_str());
				} else if( stricmp("end_time", itorData->first.c_str() ) == 0 ) {
					m_ListRoom.SetItemText(0, kRoomEndTime, itorData->second.c_str());
				}
			}*/
			// 如果当前直播间就是挂载的直播间，选中这条记录...
			if( nCurSelRoomID == nRoomID ) {
				m_ListRoom.SetItemState(0, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
				m_nSelRoomID = nCurSelRoomID;
			}
		}
		// 设置标题名称...
		CString strTitle;
		strTitle.Format("当前服务器上有 %d 个直播间，请选择一个要加入的直播间。", theMapRoom.size());
		this->SetWindowText(strTitle);
		// 设置状态，显示相关控件...
		m_nState = kStateFinish;
		m_ListRoom.ShowWindow(SW_SHOW);
		GetDlgItem(IDOK)->ShowWindow(SW_SHOW);
		GetDlgItem(IDCANCEL)->ShowWindow(SW_SHOW);
		// 刷新背景层...
		this->Invalidate();
	}
}

void CDlgRoom::OnBtnClickedOk()
{
	// 判断是否已经有选中的记录...
	if( m_ListRoom.GetItemCount() > 0 && m_nSelRoomID < 0 ) {
		MessageBox("请选择一个要加入的直播间！", "错误", MB_ICONSTOP);
		return;
	}
	// 调用接口将当前采集端，挂载到选中的直播间上去...
	if( m_lpRoomThread != NULL ) {
		m_lpRoomThread->doWebSetLiveRoom(m_nSelRoomID);
	}
	CDialogEx::OnOK();
}


void CDlgRoom::OnItemchangedListRoom(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	if ((pNMLV->uChanged & LVIF_STATE) && (pNMLV->uNewState & LVIS_SELECTED)) {
		// 获取直播间起始编号...
		CXmlConfig & theConfig = CXmlConfig::GMInstance();
		int nBeginRoomID = theConfig.GetBeginRoomID();
		int nCurSelItem  = pNMLV->iItem;
		// 当前选中编号大于起始编号，说明当前已选中...
		if( m_nSelRoomID > nBeginRoomID ) {
			// 遍历所有记录，找到已选中记录，恢复图标状态...
			for(int i = 0; i < m_ListRoom.GetItemCount(); ++i) {
				if( m_ListRoom.GetItemData(i) == m_nSelRoomID ) {
					m_ListRoom.SetItem(i, 0, LVIF_IMAGE, NULL, kImageWait, 0, 0, 0);
					break;
				}
			}
		}
		// 获取新选中的记录数据，再修改新选中行的图标...
		m_nSelRoomID = m_ListRoom.GetItemData(nCurSelItem);
		m_ListRoom.SetItem(nCurSelItem, 0, LVIF_IMAGE, NULL, kImageCheck, 0, 0, 0);
	}
	*pResult = 0;
}

void CDlgRoom::OnDblclkListRoom(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	if( pNMItemActivate->iItem >= 0 ) {
		this->OnBtnClickedOk();
	}
	*pResult = 0;
}
