#include "stdafx.h"
#include "Almon.h"
#include "DlgListView.h"
#include "afxdialogex.h"
#include "Utils.h"
#include "DlgProgressBar.h"
#include <cctype>
#include <fstream>


static DlgListView::Column st_sortColumn = DlgListView::Column::Bytes;
static bool st_bSortAsc = false;

IMPLEMENT_DYNAMIC(DlgListView, CDialogEx)
BEGIN_MESSAGE_MAP(DlgListView, CDialogEx)
	ON_WM_SIZE()
	ON_WM_CTLCOLOR()
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_CALLSTACK_LIST, &DlgListView::OnLvnItemchangedCallstackList)
	ON_NOTIFY(LVN_COLUMNCLICK, IDC_CALLSTACK_LIST, OnColumnClick)
	ON_BN_CLICKED(IDC_BUTTON_APPLY_FILTER, &DlgListView::OnBnClickedButtonApplyFilter)
	ON_BN_CLICKED(IDC_BUTTON_EXPORT, &DlgListView::OnBnClickedButtonExport)
END_MESSAGE_MAP()

DlgListView::DlgListView(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_LISTVIEW, pParent), DlgUtils(this)
{
	m_pCallstackMap = nullptr;
}

DlgListView::~DlgListView()
{
}

void DlgListView::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_CALLSTACK_LIST, m_listCtrl);
}


BOOL DlgListView::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	SetColumns();
	m_listCtrl.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	return TRUE;
}

HBRUSH DlgListView::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	UNUSED_ALWAYS(nCtlColor);
	if (pWnd->GetDlgCtrlID() == IDC_EDIT_CALLSTACK)
		return (HBRUSH)GetStockObject(WHITE_BRUSH);
	return CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);
}

void DlgListView::SetColumns()
{
	m_listCtrl.InsertColumn(static_cast<int>(DlgListView::Column::Allocs), "Num Allocs", LVCFMT_RIGHT, 80);
	m_listCtrl.InsertColumn(static_cast<int>(DlgListView::Column::Bytes), "Total Size (Bytes)", LVCFMT_RIGHT, 110);
	m_listCtrl.InsertColumn(static_cast<int>(DlgListView::Column::Usage), "Bytes in Use", LVCFMT_RIGHT, 110);
}

bool DlgListView::SetInfo(CallStackMap* pCallstackMap, ProgressBar* pProgressBar)
{
	m_pCallstackMap = pCallstackMap;

	if (!UpdateList(pProgressBar))
		return false;

	SelectCallStack(0);
	m_listCtrl.SetItemState(0, LVIS_SELECTED, LVIS_SELECTED);
	m_listCtrl.SetSelectionMark(0);
	return true;
}

int CompareValues(CListCtrl* pListCtrl, int param1, int param2, int colIdx)
{
	CString item1 = pListCtrl->GetItemText(param1, colIdx);
	CString item2 = pListCtrl->GetItemText(param2, colIdx);
	int x1 = _tstoi(item1.GetBuffer());
	int x2 = _tstoi(item2.GetBuffer());
	int result{};
	if ((x1 - x2) < 0) result = st_bSortAsc ? -1 : 1;
	else if ((x1 - x2) == 0) result = 0;
	else result = st_bSortAsc ? 1 : -1;
	return result;
}

int CALLBACK ListCompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	int param1 = static_cast<int>(lParam1);
	int param2 = static_cast<int>(lParam2);
	CListCtrl* pListCtrl = (CListCtrl*)lParamSort;
	return CompareValues(pListCtrl, param1, param2, static_cast<int>(st_sortColumn));
}

void DlgListView::OnColumnClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	CWaitCursor waitCursor;
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	st_sortColumn = static_cast<DlgListView::Column>(pNMListView->iSubItem);
	st_bSortAsc = !st_bSortAsc;

	m_listCtrl.SortItemsEx(ListCompareFunc, (LPARAM)&m_listCtrl);
	*pResult = 0;
}

bool DlgListView::UpdateList(ProgressBar* pProgressBar)
{
	CWaitCursor waitCursor;

	// Retrieve filter parameters.
	StringArray& includeFrames = m_filterDlg.includeFrames;
	StringArray& excludeFrames = m_filterDlg.excludeFrames;
	bool bFilterInclude = !includeFrames.empty();
	bool bFilterExclude = !excludeFrames.empty();
	auto [nNumBytesMin, nNumBytesMax] = m_filterDlg.rangeSize;
	auto [nNumAllocsMin, nNumAllocsMax] = m_filterDlg.rangeAllocs;
	auto [nNumBytesUseMin, nNumBytesUseMax] = m_filterDlg.rangeUse;

	// Refresh the list view according to the filter parameters.
	m_listCtrl.DeleteAllItems();
	GetDlgItem(IDC_EDIT_CALLSTACK)->SetWindowText("");

	if (pProgressBar)
	{
		pProgressBar->SetRange((int)m_pCallstackMap->size());
		pProgressBar->SetLabel("Initializing list view...");
	}

	int itemIdx{}, count{};
	CString stackLabel, bytesLabel, numAllocsLabel, usageLabel;

	for (const auto& [callStack, info] : *m_pCallstackMap)
	{
		if (bFilterInclude && !ContainsFrame(info, includeFrames))
			continue;
		if (bFilterExclude && ContainsFrame(info, excludeFrames))
			continue;
		if (IsValueNotInRange(info.numAllocs, nNumAllocsMin, nNumAllocsMax))
			continue;
		if (IsValueNotInRange(info.numBytes, nNumBytesMin, nNumBytesMax))
			continue;
		if (IsValueNotInRange(info.numBytesInUse, nNumBytesUseMin, nNumBytesUseMax))
			continue;

		bytesLabel.Format("%zu", info.numBytes);
		numAllocsLabel.Format("%d", info.numAllocs);
		usageLabel.Format("%zu", info.numBytesInUse);
		itemIdx = m_listCtrl.InsertItem(m_listCtrl.GetItemCount(), stackLabel);

		m_listCtrl.SetItemText(itemIdx, 0, numAllocsLabel);
		m_listCtrl.SetItemText(itemIdx, 1, bytesLabel);
		m_listCtrl.SetItemText(itemIdx, 2, usageLabel);
		m_listCtrl.SetItemData(itemIdx, (DWORD_PTR)&info);

		if (pProgressBar && !pProgressBar->Update(++count))
		{
			MessageBox("The displayed information will be incomplete.", "Canceled", MB_OK);
			return false;
		}
	}

	if (pProgressBar)
	{
		pProgressBar->Show(false); // Disable cancel.
		pProgressBar->SetLabel("Sorting list...");
	}

	m_listCtrl.SortItemsEx(ListCompareFunc, (LPARAM)&m_listCtrl);

	if (m_listCtrl.GetItemCount() > 0)
	{
		SelectCallStack(0);
		m_listCtrl.SetItemState(0, LVIS_SELECTED, LVIS_SELECTED);
		m_listCtrl.SetSelectionMark(0);
	}

	if (pProgressBar) { pProgressBar->Show(true); } // Restore cancel.	

	return true;
}

void DlgListView::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	if (GetDlgItem(IDC_CALLSTACK_LIST))
	{
		CRect rcClient;
		GetClientRect(&rcClient);

		MoveControl(IDC_EDIT_CALLSTACK, NO_MOVE, NO_MOVE, rcClient.bottom - 20, rcClient.right - 20);
		CRect rcList = MoveControl(IDC_CALLSTACK_LIST, NO_MOVE, NO_MOVE, rcClient.bottom - 50, NO_MOVE);

		MoveControl(IDC_BUTTON_APPLY_STATS_FILTER, rcList.bottom + 5, NO_MOVE, CALCULATE, NO_MOVE);
		MoveControl(IDC_BUTTON_EXPORT, rcList.bottom + 5, NO_MOVE, CALCULATE, NO_MOVE);
	}
}

void DlgListView::OnLvnItemchangedCallstackList(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

	if ((pNMLV->uChanged & LVIF_STATE) && (pNMLV->uNewState & LVIS_SELECTED))
	{
		POSITION pos = m_listCtrl.GetFirstSelectedItemPosition();
		if (pos)
		{
			int nItem = m_listCtrl.GetNextSelectedItem(pos);
			SelectCallStack(nItem);
		}
	}
	m_listCtrl.SetFocus();

	*pResult = 0;
}

void DlgListView::OnLvnItemchangedAllocList(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	*pResult = 0;
}

void DlgListView::SelectCallStack(int callStackIdx)
{
	if (callStackIdx >= 0 && callStackIdx < m_listCtrl.GetItemCount())
	{
		CString sStack;
		CallStackInfo* pCallStackInfo = (CallStackInfo*)m_listCtrl.GetItemData(callStackIdx);
		if (pCallStackInfo)
		{
			for (int i = 0; i < pCallStackInfo->numFrames; i++)
			{
				if (pCallStackInfo->frames[i])
					sStack.AppendFormat("%s\r\n", *(pCallStackInfo->frames[i]));
			}

			GetDlgItem(IDC_EDIT_CALLSTACK)->SetWindowText(sStack);
		}
	}
	else
		GetDlgItem(IDC_EDIT_CALLSTACK)->SetWindowText("");

}

CString DlgListView::GetTextValue(int row, Column column)
{
	const int maxTextLen{ 2048 };
	char value[maxTextLen];
	m_listCtrl.GetItemText(row, static_cast<int>(column), value, maxTextLen);
	return CString(value);
}

void DlgListView::OnBnClickedButtonApplyFilter()
{
	if (m_filterDlg.DoModal() == IDOK)
		UpdateList();
}

void DlgListView::OnBnClickedButtonExport()
{
	Export(this, this, "ExportedList");
}

bool DlgListView::IsFormatSupported(ExporterBase::Format fmt)
{
	using Format = ExporterBase::Format;
	return (fmt == Format::TXT || fmt == Format::XML);
}

int DlgListView::GetNumExportItems()
{
	return m_listCtrl.GetItemCount();
}

bool DlgListView::ExportXML(std::ofstream& file, ProgressBar* pProgressBar)
{
	int numItems = m_listCtrl.GetItemCount();

	file << "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n\t<Items count=\"" << numItems << "\" >\n";

	for (int i = 0; i < numItems; i++)
	{
		file << "\t\t<Item allocs=\"" << GetTextValue(i, Column::Allocs) << "\" "
			<< "size=\"" << GetTextValue(i, Column::Bytes) << "\" "
			<< "use=\"" << GetTextValue(i, Column::Usage) << "\" "
			<< ">\n\t\t\t<CallStack>\n";
		;
		if (auto pCallStackInfo = (CallStackInfo*)m_listCtrl.GetItemData(i); pCallStackInfo)
		{
			file << "\t\t\t\t<![CDATA[\n";
			for (int i = 0; i < pCallStackInfo->numFrames; i++)
				if (pCallStackInfo->frames[i])
					file << "\t\t\t\t" << *pCallStackInfo->frames[i] << "\n";
			file << "\t\t\t\t]]>\n";
		}
		file << "\t\t\t</CallStack>\n\t\t</Item>\n";

		if (pProgressBar && !pProgressBar->Update(i))
			return false;
	}
	file << "</Items>";
	return true;

}

bool DlgListView::ExportTXT(std::ofstream& file, ProgressBar* pProgressBar)
{
	for (int i = 0; i < m_listCtrl.GetItemCount(); i++)
	{
		file << "NumAllocs: " << GetTextValue(i, Column::Allocs) << "\n"
			<< "Size: " << GetTextValue(i, Column::Bytes) << "\n"
			<< "Use: " << GetTextValue(i, Column::Usage) << "\n"
			<< "Call Stack:\n";

		if (auto pCallStackInfo = (CallStackInfo*)m_listCtrl.GetItemData(i); pCallStackInfo)
		{
			for (int i = 0; i < pCallStackInfo->numFrames; i++)
			{
				if (pCallStackInfo->frames[i])
					file << "\t" << *pCallStackInfo->frames[i] << "\n";
			}
		}
		if (pProgressBar && !pProgressBar->Update(i))
			return false;
		file << "\n";
	}
	return true;
}


