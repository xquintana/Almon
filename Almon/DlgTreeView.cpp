#include "stdafx.h"
#include "Almon.h"
#include "DlgTreeView.h"
#include "afxdialogex.h"
#include "Utils.h"
#include "DlgUtils.h"
#include "DlgProgressBar.h"
#include <fstream>


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Node

void Node::Insert(const CallStack& stack, size_t sizeLeaf, DWORD level, CString** pFrames)
{
	if (level == 0)
	{
		size = sizeLeaf;
		return;
	}
	level--;
	size += sizeLeaf;
	UINT_PTR address = stack[level];
	pFrame = pFrames[level];
	nodes[address].Insert(stack, sizeLeaf, level, pFrames);
}

bool Node::PrintTXT(std::ofstream& file, ProgressBar* pProgress)
{
	std::vector<CString> items;
	PrintTXT(items, -1, pProgress);

	if (pProgress)
		pProgress->SetLabel("Writing to file...");

	for (auto& item : items)
		if (!(file << item.GetString()))
			return false;
	return true;
}

void Node::PrintTXT(std::vector<CString>& items, int indent, ProgressBar* pProgress)
{
	if (nodes.empty())
		return;
	indent++;
	for (auto& [address, node] : nodes)
	{
		char tab[128]{};
		for (int i = 0; i < indent; i++)
			strcat_s(tab, 128, "\t");
		if (node.pFrame && node.pFrame->GetString())
		{
			sprintf_s(auxStr, auxStrMaxLen, "%s[-] %s [%zu]\n", tab, node.pFrame->GetString(), node.size);
			items.push_back(auxStr);
		}
		else
		{
			sprintf_s(auxStr, auxStrMaxLen, "%s[-] address: 0x%p [%zu]\n", tab, (void*)address, node.size);
			items.push_back(auxStr);
		}
		if (pProgress)
			pProgress->Update((int)items.size());
		node.PrintTXT(items, indent, pProgress);
	}
}

bool Node::PrintXML(std::ofstream& file, ProgressBar* pProgress)
{
	std::vector<CString> items;
	PrintXML(items, -1, pProgress);

	if (pProgress)
		pProgress->SetLabel("Writing to file...");

	for (auto& item : items)
		if (!(file << item.GetString()))
			return false;
	return true;
}

void Node::PrintXML(std::vector<CString>& items, int indent, ProgressBar* pProgress)
{
	if (nodes.empty())
		return;
	indent++;
	for (auto& [address, node] : nodes)
	{
		char tab[128]{};
		for (int i = 0; i < indent; i++)
			strcat_s(tab, 128, "\t");
		if (node.pFrame && node.pFrame->GetString())
		{
			sprintf_s(auxStr, auxStrMaxLen, "%s<Item size=\"%zu\" frame=\"%s\" %s\n", tab, node.size, node.pFrame->GetString(), (node.nodes.size() > 0) ? ">" : "/>");
			items.push_back(auxStr);
		}
		else
		{
			sprintf_s(auxStr, auxStrMaxLen, "%s<Item size=\"%zu\" address=\"0x%p\" %s\n", tab, node.size, (void*)address, (node.nodes.size() > 0) ? ">" : "/>");
			items.push_back(auxStr);
		}
		if (pProgress)
			pProgress->Update((int)items.size());
		node.PrintXML(items, indent, pProgress);

		if (node.nodes.size() > 0)
		{
			sprintf_s(auxStr, auxStrMaxLen, "%s</Item>\n", tab);
			items.push_back(auxStr);
		}
	}
}

void Node::Draw(CTreeCtrl& control, int level, HTREEITEM hParent)
{
	if (nodes.empty())
		return;
	level++;
	for (auto& [address, node] : nodes)
	{
		if (node.pFrame)
		{
			sprintf_s(auxStr, auxStrMaxLen, "%s [%zu]\n", node.pFrame->GetString(), node.size);
			HTREEITEM hItem = control.InsertItem(auxStr, hParent);
			if (level < 3)
				control.Expand(hParent, TVE_EXPAND);

			node.Draw(control, level, hItem);
		}
	}
}

void Node::Clear()
{
	for (auto& [address, node] : nodes)
		node.Clear();
	nodes.clear();
	pFrame = nullptr;
	size = 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DlgTreeView

IMPLEMENT_DYNAMIC(DlgTreeView, CDialogEx)

DlgTreeView::DlgTreeView(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_TREEVIEW, pParent), DlgUtils(this)
{
	m_filterDlg.SetTreeViewMode();
}

void DlgTreeView::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TREE_CTL, m_treeCtrl);
}


BEGIN_MESSAGE_MAP(DlgTreeView, CDialogEx)
	ON_WM_SIZE()
	ON_BN_CLICKED(IDC_BUTTON_SHOW_FILTER, &DlgTreeView::OnBnClickedButtonShowFilter)
	ON_BN_CLICKED(IDC_BUTTON_EXPORT, &DlgTreeView::OnBnClickedButtonExport)
END_MESSAGE_MAP()


BOOL DlgTreeView::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	return TRUE;
}

void DlgTreeView::SetInfo(CallStackMap* pCallstackMap, ProgressBar* pProgressBar)
{
	m_pCallstackMap = pCallstackMap;
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
	
	ULONGLONG lastUpdateTime = 0;
	int idx = 0;

	if (pProgressBar)
	{
		pProgressBar->SetRange((int)m_pCallstackMap->size());
		pProgressBar->SetLabel("Initializing tree view...");
	}

	if (bFilterInclude || bFilterExclude)
	{
		for (auto& [callstack, info] : *m_pCallstackMap)
		{
			if (bFilterInclude && !ContainsFrame(info, includeFrames))
				continue;
			if (bFilterExclude && ContainsFrame(info, excludeFrames))
				continue;

			m_tree.nodes[callstack[info.numFrames - 1]].Insert(callstack, info.numBytes, info.numFrames, info.frames);			
			if (pProgressBar) { pProgressBar->Update(++idx); }
		}
	}
	else
	{
		for (auto& [callstack, info] : *m_pCallstackMap)
		{
			m_tree.nodes[callstack[info.numFrames - 1]].Insert(callstack, info.numBytes, info.numFrames, info.frames);			
			if (pProgressBar) { pProgressBar->Update(++idx); }
		}			
	}

	m_treeCtrl.DeleteAllItems();

	if (pProgressBar)
		pProgressBar->SetLabel("Rendering tree...");

	HTREEITEM hRoot = m_treeCtrl.InsertItem("Root", TVI_ROOT);
	m_tree.Draw(m_treeCtrl, 0, hRoot);
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
	return m_treeCtrl.GetCount() - 1; // Do not count the Root node.
}

bool DlgTreeView::ExportTXT(std::ofstream& file, ProgressBar* pProgress)
{
	return m_tree.PrintTXT(file, pProgress);
}

bool DlgTreeView::ExportXML(std::ofstream& file, ProgressBar* pProgress)
{
	return m_tree.PrintXML(file, pProgress);
}
