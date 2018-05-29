
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
	// �����ȵ��û���ĳ�ʼ���ӿ�...
	CDialogEx::OnInitDialog();
	// ����ͼ��...
	this->SetIcon(AfxGetApp()->LoadIcon(IDR_MAINFRAME), FALSE);
	// ��������ͼ���ļ�...
	CBitmap bitTemp;
	m_RoomImage.DeleteImageList();
	m_RoomImage.Create(24, 24, ILC_COLOR8|ILC_MASK, 1, 1);
	
	bitTemp.LoadBitmap(IDB_BITMAP_CHECK);
	m_RoomImage.Add(&bitTemp, RGB(255, 255, 255));
	ASSERT( m_RoomImage.m_hImageList != NULL );
	bitTemp.DeleteObject();

	// ��ʼ��ֱ�����б��...
	m_ListRoom.SetExtendedStyle(LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES);
	m_ListRoom.SetImageList(&m_RoomImage, LVSIL_SMALL);

	RECT rect = {0};
	m_ListRoom.GetClientRect(&rect);

	int nWidth = rect.right / 5;
	
	m_ListRoom.InsertColumn(kRoomID, "����", LVCFMT_LEFT, nWidth - 40);
	m_ListRoom.InsertColumn(kRoomLesson, "�γ�", LVCFMT_LEFT, nWidth + 40);
	m_ListRoom.InsertColumn(kRoomTeacher, "��ʦ", LVCFMT_LEFT, nWidth - 40);
	m_ListRoom.InsertColumn(kRoomStartTime, "��ʼʱ��", LVCFMT_LEFT, nWidth + 10);
	m_ListRoom.InsertColumn(kRoomEndTime, "����ʱ��", LVCFMT_LEFT, nWidth + 10);

	// ����һ����ѯֱ�����б��ʱ������...
	this->SetTimer(kTimerLiveRoom, 100, NULL);

	// ���ؿؼ����󣬲�����������...
	m_ListRoom.ShowWindow(SW_HIDE);
	GetDlgItem(IDOK)->ShowWindow(SW_HIDE);
	GetDlgItem(IDCANCEL)->ShowWindow(SW_HIDE);
	this->Invalidate();

	return true;
}
//
// ���Ʊ���...
BOOL CDlgRoom::OnEraseBkgnd(CDC* pDC)
{
	// ���ݲ�ͬ״̬��ʾ��ͬ��״̬��Ϣ
	if( m_nState != kStateFinish ) {
		CString strText = "���ڻ�ȡֱ�����б�...";
		if( m_nState == kStateError ) {
			strText = "��ȡֱ�����б�ʧ�ܣ�";
		}
		this->ShowText(pDC, strText);
		return true;
	}
	// �����ȡֱ��������ɣ�ֱ�ӵ��û�����...
	ASSERT( m_nState == kStateFinish );
	return CDialogEx::OnEraseBkgnd(pDC);
}
//
// ��ʾ������Ϣ => ��ʾ����������ɫ�������С���Ƿ����...
void CDlgRoom::ShowText(CDC * pDC, CString & inTxtData)
{
	// �������Ϊ�գ�ֱ�ӷ���...
	if( inTxtData.GetLength() <= 0 )
		return;
	// ����������ɫ�������С...
	COLORREF inTxtColor = RGB(0, 0, 0);
	int inFontSize = 18;
	bool isBold = true;
	// ��ȡ������ʾ����...
	CRect inDispRect;
	this->GetClientRect(&inDispRect);
	inDispRect.top = inDispRect.Height() / 2 - 30;
	inDispRect.bottom = inDispRect.top + 30;
	// ׼���������...
	CFont   fontLogo;
	CFont * pOldFont = NULL;
	pDC->SetBkMode(TRANSPARENT);
	// �������� => ��������Ĳ�������...
	fontLogo.CreateFont(inFontSize, 0, 0, 0, (isBold ? FW_BOLD : FW_LIGHT), false, false,0,0,0,0,0,0, "����");
	// �������������������ɫ...
	pOldFont = pDC->SelectObject(&fontLogo);
	pDC->SetTextColor( inTxtColor );
	// ����������Ϣ => ������ʾ...
	pDC->DrawText(inTxtData, inDispRect, DT_EXTERNALLEADING | DT_WORDBREAK | DT_CENTER | DT_VCENTER);
	// ��������...
	pDC->SelectObject(pOldFont);
	fontLogo.DeleteObject();
	fontLogo.Detach();
}

void CDlgRoom::OnTimer(UINT_PTR nIDEvent)
{
	if( nIDEvent == kTimerLiveRoom ) {
		// �ȹر�ʱ������...
		KillTimer( kTimerLiveRoom );
		// ����̶߳�����Ч��ֱ�ӷ���...
		if( m_lpRoomThread == NULL ) {
			m_nState = kStateError;
			return;
		}
		// ����ӷ�������ȡֱ�����б������...
		if( !m_lpRoomThread->doWebGetLiveRoom() ) {
			m_nState = kStateError;
			return;
		}
		// �����ȡֱ�����б�ɹ�����ʾ�ڽ�����...
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
			// �����ǰֱ������ǹ��ص�ֱ���䣬ѡ��������¼...
			if( nCurSelRoomID == nRoomID ) {
				m_ListRoom.SetItemState(0, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
				m_nSelRoomID = nCurSelRoomID;
			}
		}
		// ���ñ�������...
		CString strTitle;
		strTitle.Format("��ǰ���������� %d ��ֱ���䣬��ѡ��һ��Ҫ�����ֱ���䡣", theMapRoom.size());
		this->SetWindowText(strTitle);
		// ����״̬����ʾ��ؿؼ�...
		m_nState = kStateFinish;
		m_ListRoom.ShowWindow(SW_SHOW);
		GetDlgItem(IDOK)->ShowWindow(SW_SHOW);
		GetDlgItem(IDCANCEL)->ShowWindow(SW_SHOW);
		// ˢ�±�����...
		this->Invalidate();
	}
}

void CDlgRoom::OnBtnClickedOk()
{
	// �ж��Ƿ��Ѿ���ѡ�еļ�¼...
	if( m_ListRoom.GetItemCount() > 0 && m_nSelRoomID < 0 ) {
		MessageBox("��ѡ��һ��Ҫ�����ֱ���䣡", "����", MB_ICONSTOP);
		return;
	}
	// ���ýӿڽ���ǰ�ɼ��ˣ����ص�ѡ�е�ֱ������ȥ...
	if( m_lpRoomThread != NULL ) {
		m_lpRoomThread->doWebSetLiveRoom(m_nSelRoomID);
	}
	CDialogEx::OnOK();
}


void CDlgRoom::OnItemchangedListRoom(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	if ((pNMLV->uChanged & LVIF_STATE) && (pNMLV->uNewState & LVIS_SELECTED)) {
		// ��ȡֱ������ʼ���...
		CXmlConfig & theConfig = CXmlConfig::GMInstance();
		int nBeginRoomID = theConfig.GetBeginRoomID();
		int nCurSelItem  = pNMLV->iItem;
		// ��ǰѡ�б�Ŵ�����ʼ��ţ�˵����ǰ��ѡ��...
		if( m_nSelRoomID > nBeginRoomID ) {
			// �������м�¼���ҵ���ѡ�м�¼���ָ�ͼ��״̬...
			for(int i = 0; i < m_ListRoom.GetItemCount(); ++i) {
				if( m_ListRoom.GetItemData(i) == m_nSelRoomID ) {
					m_ListRoom.SetItem(i, 0, LVIF_IMAGE, NULL, kImageWait, 0, 0, 0);
					break;
				}
			}
		}
		// ��ȡ��ѡ�еļ�¼���ݣ����޸���ѡ���е�ͼ��...
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
