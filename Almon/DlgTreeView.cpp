#include "stdafx.h"
#include "Almon.h"
#include "DlgTreeView.h"
#include "Utils.h"
#include "DlgUtils.h"
#include "DlgProgressBar.h"
#include <fstream>


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DlgTreeView

IMPLEMENT_DYNAMIC(DlgTreeView, CDialogEx)

BEGIN_MESSAGE_MAP(DlgTreeView, CDialogEx)
	ON_WM_SIZE()
	ON_BN_CLICKED(IDC_BUTTON_SHOW_FILTER, &DlgTreeView::OnBnClickedButtonShowFilter)
	ON_BN_CLICKED(IDC_BUTTON_EXPORT, &DlgTreeView::OnBnClickedButtonExport)
END_MESSAGE_MAP()

DlgTreeView::DlgTreeView(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_TREEVIEW, pParent), DlgUtils(this)
{
	m_filterDlg.SetTreeViewMode();
}

void DlgTreeView::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BOOL DlgTreeView::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	m_treeView.SetFont(GetDialogFontName());
	m_treeView.Attach(IDC_TREE_CTL, this);
	return TRUE;
}

void DlgTreeView::SetInfo(CallStackMap* pCallStackMap, ProgressBar* pProgressBar)
{
	m_pCallStackMap = pCallStackMap;
	UpdateTree(pProgressBar);
}

void DlgTreeView::UpdateTree(ProgressBar* pProgressBar)
{
	CWaitCursor waitCursor;

	m_tree.Clear();

	// Retrieve filter parameters.
	StringArray& includeFrames = m_filterDlg.includeFrames;
	StringArray& excludeFrames = m_filterDlg.excludeFrames;
	bool bFilterInclude = !includeFrames.empty();
	bool bFilterExclude = !excludeFrames.empty();

	int idx = 0;

	if (pProgressBar)
	{
		pProgressBar->SetRange((int)m_pCallStackMap->size());
		pProgressBar->SetLabel("Initializing tree view...");
	}

	if (bFilterInclude || bFilterExclude)
	{
		for (auto& [callStack, info] : *m_pCallStackMap)
		{
			if (bFilterInclude && !ContainsFrame(info, includeFrames))
				continue;
			if (bFilterExclude && ContainsFrame(info, excludeFrames))
				continue;

			m_tree.nodes[callStack[info.numFrames - 1]].Insert(callStack, info.numBytes, info.numFrames, info.frames);
			if (pProgressBar) { pProgressBar->Update(++idx); }
		}
	}
	else
	{
		for (auto& [callStack, info] : *m_pCallStackMap)
		{
			m_tree.nodes[callStack[info.numFrames - 1]].Insert(callStack, info.numBytes, info.numFrames, info.frames);
			if (pProgressBar) { pProgressBar->Update(++idx); }
		}
	}

	// Sort the entire tree by accumulated size (descending).
	m_tree.Sort();

	if (pProgressBar)
		pProgressBar->SetLabel("Rendering tree...");

	m_treeView.SetModel(&m_tree);
}

void DlgTreeView::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	if (GetDlgItem(IDC_TREE_CTL))
	{
		CRect rcClient;
		GetClientRect(&rcClient);
		CRect rcBtnFilter = MoveControl(IDC_BUTTON_SHOW_FILTER, CALCULATE, NO_MOVE, rcClient.bottom - 20, NO_MOVE);
		MoveControl(IDC_BUTTON_EXPORT, rcBtnFilter.top, NO_MOVE, rcBtnFilter.bottom, NO_MOVE);
		MoveControl(IDC_TREE_CTL, NO_MOVE, NO_MOVE, rcBtnFilter.top - 10, rcClient.right - 20);

		m_treeView.RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_FRAME);
	}
}

void DlgTreeView::OnBnClickedButtonShowFilter()
{
	if (m_filterDlg.DoModal() == IDOK)
		UpdateTree();
}

void DlgTreeView::OnBnClickedButtonExport()
{
	Export(this, this, "ExportedTree");
}

bool DlgTreeView::IsFormatSupported(ExporterBase::Format fmt)
{
	using Format = ExporterBase::Format;
	return (fmt == Format::TXT || fmt == Format::XML);
}

int DlgTreeView::GetNumExportItems()
{
	return m_tree.Count();
}

bool DlgTreeView::ExportTXT(std::ofstream& file, ProgressBar* pProgress)
{
	return m_tree.PrintTXT(file, pProgress);
}

bool DlgTreeView::ExportXML(std::ofstream& file, ProgressBar* pProgress)
{
	return m_tree.PrintXML(file, pProgress);
}
