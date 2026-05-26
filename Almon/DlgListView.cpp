#include "stdafx.h"
#include "Almon.h"
#include "DlgListView.h"
#include "DlgProgressBar.h"
#include <fstream>


IMPLEMENT_DYNAMIC(DlgListView, CDialogEx)

BEGIN_MESSAGE_MAP(DlgListView, CDialogEx)
	ON_WM_SIZE()
	ON_WM_CTLCOLOR()
	ON_BN_CLICKED(IDC_BUTTON_SHOW_FILTER, &DlgListView::OnBnClickedButtonShowFilter)
	ON_BN_CLICKED(IDC_BUTTON_EXPORT, &DlgListView::OnBnClickedButtonExport)
	ON_MESSAGE(WM_LISTVIEW_ROW_CLICKED, &DlgListView::OnListViewRowClicked)
	ON_MESSAGE(WM_LISTVIEW_HEADER_CLICKED, &DlgListView::OnListViewHeaderClicked)
END_MESSAGE_MAP()

DlgListView::DlgListView(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_LISTVIEW, pParent), DlgUtils(this) {
}

void DlgListView::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BOOL DlgListView::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	m_listView.SetFont(GetDialogFontName());
	m_textView.SetFont(GetDialogFontName());
	m_listView.Attach(IDC_CALLSTACK_LIST, this);
	m_textView.Attach(IDC_CALLSTACK_TEXT, this);
	return TRUE;
}

HBRUSH DlgListView::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	UNUSED_ALWAYS(nCtlColor);
	if (pWnd->GetDlgCtrlID() == IDC_CALLSTACK_TEXT)
		return (HBRUSH)GetStockObject(WHITE_BRUSH);
	return CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);
}

bool DlgListView::SetInfo(CallStackMap* pCallStackMap, ProgressBar* pProgressBar)
{
	m_pCallStackMap = pCallStackMap;

	if (!UpdateList(pProgressBar))
		return false;
	return true;
}

bool DlgListView::IsValueNotInRange(UINT64 value, UINT64 min, UINT64 max)
{
	if (min == ULLONG_MAX && max == ULLONG_MAX)
		return false;
	return (max == ULLONG_MAX) ? value != min : (value < min || value > max);
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
	auto [nNumAllocsMin, nNumAllocsMax] = m_filterDlg.rangeNumAllocs;
	auto [nNumBytesUseMin, nNumBytesUseMax] = m_filterDlg.rangeUse;

	// Refresh the list view according to the filter parameters.	
	m_listView.Clear();
	m_textView.Clear();

	if (pProgressBar)
	{
		pProgressBar->SetRange(static_cast<int>(m_pCallStackMap->size()));
		pProgressBar->SetLabel("Initializing list view...");
	}

	int itemIdx{}, count{};
	CString stackLabel, bytesLabel, numAllocsLabel, usageLabel;
	auto& items = m_listView.GetItems();

	items.reserve(m_pCallStackMap->size());

	for (auto& [callStack, info] : *m_pCallStackMap)
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

		items.push_back(&info);

		if (pProgressBar && !pProgressBar->Update(++count))
		{
			MessageBox("The displayed information will be incomplete.", "Canceled", MB_OK);
			return false;
		}
	}

	if (m_listView.GetNumItems() > 0)
	{
		if (pProgressBar)
		{
			pProgressBar->Show(false); // Disable cancel.
			pProgressBar->SetLabel("Sorting list...");
		}
		m_listView.SortByColumn(static_cast<int>(m_sortColumn));
		m_listView.SetVertScrollPos(0);
		m_listView.SetSelectedItem(0);
		SelectCallStack(0);

		if (pProgressBar) { pProgressBar->Show(true); } // Restore cancel.
	}
	else
		m_listView.Refresh();

	return true;
}

void DlgListView::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	if (GetDlgItem(IDC_CALLSTACK_LIST))
	{
		CRect rcClient;
		GetClientRect(&rcClient);

		MoveControl(IDC_CALLSTACK_TEXT, NO_MOVE, NO_MOVE, rcClient.bottom - 20, rcClient.right - 20);
		CRect rcList = MoveControl(IDC_CALLSTACK_LIST, NO_MOVE, NO_MOVE, rcClient.bottom - 50, NO_MOVE);

		MoveControl(IDC_BUTTON_SHOW_FILTER, rcList.bottom + 5, NO_MOVE, CALCULATE, NO_MOVE);
		MoveControl(IDC_BUTTON_EXPORT, rcList.bottom + 5, NO_MOVE, CALCULATE, NO_MOVE);

		m_textView.RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_FRAME);
		m_listView.RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_FRAME);
	}
}

void DlgListView::SelectCallStack(int callStackIdx)
{
	if (callStackIdx >= 0 && callStackIdx < m_listView.GetNumItems())
	{
		if (auto pCallStackInfo = m_listView.GetItem(callStackIdx); pCallStackInfo)
			m_textView.DrawCallStack(10, 5, pCallStackInfo, true);
	}
	else
		m_textView.Clear();
}

CString DlgListView::GetCellText(int row, Column column)
{
	return m_listView.GetItemText(row, static_cast<int>(column));
}

void DlgListView::OnBnClickedButtonShowFilter()
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
	return m_listView.GetNumItems();
}

bool DlgListView::ExportXML(std::ofstream& file, ProgressBar* pProgressBar)
{
	int numItems = m_listView.GetNumItems();

	file << "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n\t<Items count=\"" << numItems << "\" >\n";

	for (int i = 0; i < numItems; i++)
	{
		file << "\t\t<Item allocs=\"" << GetCellText(i, Column::Allocs) << "\" "
			<< "size=\"" << GetCellText(i, Column::Bytes) << "\" "
			<< "use=\"" << GetCellText(i, Column::Usage) << "\" "
			<< ">\n\t\t\t<CallStack>\n";
		;
		if (auto pCallStackInfo = m_listView.GetItem(i); pCallStackInfo)
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
	for (int i = 0; i < m_listView.GetNumItems(); i++)
	{
		file << "NumAllocs: " << GetCellText(i, Column::Allocs) << "\n"
			<< "Size: " << GetCellText(i, Column::Bytes) << "\n"
			<< "Use: " << GetCellText(i, Column::Usage) << "\n"
			<< "Call Stack:\n";

		if (auto pCallStackInfo = m_listView.GetItem(i); pCallStackInfo)
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

LRESULT DlgListView::OnListViewRowClicked(WPARAM wParam, LPARAM lParam)
{
	// wParam contains the index of the clicked row.
	SelectCallStack(static_cast<int>(wParam));
	return 0;
}

LRESULT DlgListView::OnListViewHeaderClicked(WPARAM wParam, LPARAM lParam)
{
	// wParam contains the column index (0, 1, or 2).
	CWaitCursor waitCursor;
	m_listView.SortByColumn(static_cast<int>(wParam), ListView::Order::Toggle);
	m_textView.Clear();
	return 0;
}
