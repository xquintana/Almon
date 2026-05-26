#include "stdafx.h"
#include "TreeView.h"
#include "DlgProgressBar.h"
#include <fstream>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TreeNode

void TreeNode::Insert(const CallStack& stack, size_t sizeLeaf, DWORD level, CString** pFrames)
{
	if (level == 0)
	{
		size = sizeLeaf;
		return;
	}
	level--;
	size += sizeLeaf;
	Address address = stack[level];
	pFrame = pFrames[level];
	if (level > 0)
		nodes[address].Insert(stack, sizeLeaf, level, pFrames);
}

void TreeNode::Sort()
{
	sorted.clear();
	sorted.reserve(nodes.size());
	for (auto& [address, node] : nodes)
	{
		sorted.push_back(&node);
		node.Sort();
	}
	std::sort(sorted.begin(), sorted.end(), [](const TreeNode* a, const TreeNode* b) {
		return a->size > b->size;
		});
}

void TreeNode::Clear()
{
	for (auto& [address, node] : nodes)
		node.Clear();
	nodes.clear();
	sorted.clear();
	pFrame = nullptr;
	size = 0;
}

int TreeNode::Count()
{
	int count = (int)sorted.size();
	for (TreeNode* pNode : sorted)
		count += pNode->Count();
	return count;
}

bool TreeNode::PrintTXT(std::ofstream& file, ProgressBar* pProgress)
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

void TreeNode::PrintTXT(std::vector<CString>& items, int indent, ProgressBar* pProgress)
{
	if (sorted.empty())
		return;
	indent++;
	for (TreeNode* pNode : sorted)
	{
		char tab[128]{};
		for (int i = 0; i < indent; i++)
			strcat_s(tab, 128, "\t");
		if (pNode->pFrame && pNode->pFrame->GetString())
		{
			sprintf_s(auxStr, auxStrMaxLen, "%s%s [%zu bytes]\n", tab, pNode->pFrame->GetString(), pNode->size);
			items.push_back(auxStr);
		}
		else
		{
			sprintf_s(auxStr, auxStrMaxLen, "%s[%zu bytes]\n", tab, pNode->size);
			items.push_back(auxStr);
		}
		if (pProgress)
			pProgress->Update((int)items.size());
		pNode->PrintTXT(items, indent, pProgress);
	}
}

bool TreeNode::PrintXML(std::ofstream& file, ProgressBar* pProgress)
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

void TreeNode::PrintXML(std::vector<CString>& items, int indent, ProgressBar* pProgress)
{
	if (sorted.empty())
		return;
	indent++;
	for (TreeNode* pNode : sorted)
	{
		char tab[128]{};
		for (int i = 0; i < indent; i++)
			strcat_s(tab, 128, "\t");
		if (pNode->pFrame && pNode->pFrame->GetString())
		{
			sprintf_s(auxStr, auxStrMaxLen, "%s<Item size=\"%zu\" frame=\"%s\" %s\n", tab, pNode->size, EscapeXmlString(*pNode->pFrame).GetString(), (!pNode->sorted.empty()) ? ">" : "/>");
			items.push_back(auxStr);
		}
		else
		{
			sprintf_s(auxStr, auxStrMaxLen, "%s<Item size=\"%zu\" %s\n", tab, pNode->size, (!pNode->sorted.empty()) ? ">" : "/>");
			items.push_back(auxStr);
		}
		if (pProgress)
			pProgress->Update((int)items.size());
		pNode->PrintXML(items, indent, pProgress);

		if (!pNode->sorted.empty())
		{
			sprintf_s(auxStr, auxStrMaxLen, "%s</Item>\n", tab);
			items.push_back(auxStr);
		}
	}
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TreeView

IMPLEMENT_DYNAMIC(TreeView, StackView)

BEGIN_MESSAGE_MAP(TreeView, StackView)
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_RBUTTONDOWN()
	ON_WM_KEYDOWN()
END_MESSAGE_MAP()


BOOL TreeView::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN)
	{
		if (pMsg->wParam == VK_UP || pMsg->wParam == VK_DOWN)
		{
			SendMessage(pMsg->message, pMsg->wParam, pMsg->lParam);
			return TRUE;
		}
		if (pMsg->wParam == 'C' && (GetKeyState(VK_CONTROL) & 0x8000))
		{
			if (m_selectedIndex >= 0 && m_selectedIndex < GetNumItems())
				CopyTextToClipboard(this, m_visibleItems[m_selectedIndex].GetItemText(m_pRoot));
			return TRUE;
		}
	}
	return StackView::PreTranslateMessage(pMsg);
}

void TreeView::SetModel(TreeNode* pRoot)
{
	m_pRoot = pRoot;
	m_scrollPosV = 0;
	m_expanded.clear();
	if (m_pRoot)
	{
		m_expanded.insert(m_pRoot);
		ExpandToDepth(m_pRoot, 0, 1);
	}
	Refresh();
}

void TreeView::Refresh()
{
	if (m_dcMem.GetSafeHdc() == nullptr && GetSafeHwnd() != nullptr)
	{
		CClientDC dc(this);
		CRect rcClient;
		GetClientRect(&rcClient);
		CreateBackBuffer(dc, rcClient);
		m_rect = rcClient;
	}

	RebuildVisibleItems();
	RecalcMaxItemWidth();
	UpdateScrollBars();
	Draw();
	Invalidate();
}

void TreeView::Clear()
{
	m_pRoot = nullptr;
	m_visibleItems.clear();
	m_expanded.clear();
	m_scrollPosV = 0;
	m_selectedIndex = -1;
	m_cachedMaxItemWidth = 0;
}

int TreeView::GetRowHeight()
{
	return GetFontHeight(&m_font) + 4;
}

int TreeView::GetMaxVisibleItems()
{
	int rowHeight = GetRowHeight();
	if (rowHeight <= 0)
		return 0;
	if (m_rect.Height() <= 0)
		return 0;
	return m_rect.Height() / rowHeight;
}

int TreeView::GetNumItems()
{
	return static_cast<int>(m_visibleItems.size());
}

int TreeView::GetMaxItemWidth()
{
	return m_cachedMaxItemWidth;
}

void TreeView::RecalcMaxItemWidth()
{
	m_cachedMaxItemWidth = 0;

	if (m_visibleItems.empty() || m_dcMem.GetSafeHdc() == nullptr)
		return;

	CFont* pOldFont = m_dcMem.SelectObject(&m_fontBold);
	int maxWidth = 0;

	for (const auto& item : m_visibleItems)
	{
		CString text = item.GetItemText(m_pRoot);

		int x = item.depth * INDENT_WIDTH + HORIZ_PADDING + INDICATOR_MARGIN;
		CSize sz = m_dcMem.GetTextExtent(text);
		int totalWidth = x + sz.cx;
		if (totalWidth > maxWidth)
			maxWidth = totalWidth;
	}

	m_dcMem.SelectObject(pOldFont);
	m_cachedMaxItemWidth = maxWidth;
}

void TreeView::EnsureSelectionVisible()
{
	if (m_selectedIndex < 0)
		return;
	int maxVisible = GetMaxVisibleItems();
	if (maxVisible <= 0)
		return;
	if (m_selectedIndex < m_scrollPosV)
		m_scrollPosV = m_selectedIndex;
	else if (m_selectedIndex >= m_scrollPosV + maxVisible)
		m_scrollPosV = m_selectedIndex - maxVisible + 1;
}

void TreeView::RebuildVisibleItems()
{
	m_visibleItems.clear();
	if (!m_pRoot)
		return;
	RebuildVisibleItems(m_pRoot, 0);
}

void TreeView::ExpandToDepth(TreeNode* pNode, int depth, int maxDepth)
{
	if (depth >= maxDepth)
		return;
	for (TreeNode* pChild : pNode->sorted)
	{
		m_expanded.insert(pChild);
		ExpandToDepth(pChild, depth + 1, maxDepth);
	}
}

void TreeView::ExpandAll(TreeNode* pNode)
{
	m_expanded.insert(pNode);
	for (TreeNode* pChild : pNode->sorted)
		ExpandAll(pChild);
}

void TreeView::RebuildVisibleItems(TreeNode* pNode, int depth)
{
	m_visibleItems.push_back({ pNode, depth });
	if (m_expanded.count(pNode) && !pNode->sorted.empty())
	{
		for (TreeNode* pChild : pNode->sorted)
			RebuildVisibleItems(pChild, depth + 1);
	}
}

void TreeView::Draw()
{
	if (m_dcMem.GetSafeHdc() == nullptr)
		return;

	DrawBackground();
	DrawRows();
}

void TreeView::DrawRows()
{
	int rowHeight = GetRowHeight();
	int maxVisible = GetMaxVisibleItems();
	int numItems = GetNumItems();

	CFont* pOldFont = m_dcMem.SelectObject(&m_fontBold);
	int oldBkMode = m_dcMem.SetBkMode(TRANSPARENT);
	COLORREF oldTextColor = m_dcMem.SetTextColor(CLR_TEXT);

	int endIdx = min(m_scrollPosV + maxVisible, numItems);

	for (int i = m_scrollPosV; i < endIdx; i++)
	{
		const VisibleItem& item = m_visibleItems[i];
		int rowIdx = i - m_scrollPosV;
		int y = m_rect.top + rowIdx * rowHeight;
		int x = m_rect.left + item.depth * INDENT_WIDTH + HORIZ_PADDING - m_scrollPosH;

		// Reset text color before drawing the indicator, since DrawItemText
		// may have left it as CLR_SELECTED_TEXT (white) from the previous row.
		m_dcMem.SetTextColor(CLR_TEXT);

		// Draw expand/collapse indicator for nodes with children.
		if (!item.pNode->sorted.empty())
		{
			CString indicator = m_expanded.count(item.pNode) ? "-" : "+";
			CRect rcIndicator(x, y, x + 12, y + rowHeight);
			m_dcMem.DrawText(indicator, &rcIndicator, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
		}
		x += INDICATOR_MARGIN;

		bool selected = (i == m_selectedIndex);

		CString text = item.GetItemText(m_pRoot);

		DrawColoredText(x, y, rowHeight, text, m_fontBold, m_font, selected);
	}

	m_dcMem.SetTextColor(oldTextColor);
	m_dcMem.SetBkMode(oldBkMode);
	m_dcMem.SelectObject(pOldFont);
}

void TreeView::OnLButtonDown(UINT nFlags, CPoint point)
{
	SetFocus();

	int rowHeight = GetRowHeight();
	if (rowHeight <= 0)
		return;

	int rowIdx = point.y / rowHeight;
	int itemIdx = m_scrollPosV + rowIdx;
	if (itemIdx < 0 || itemIdx >= GetNumItems())
		return;

	VisibleItem& item = m_visibleItems[itemIdx];

	// Toggle expand/collapse when single-clicking the +/- indicator.
	int indentX = m_rect.left + item.depth * INDENT_WIDTH + HORIZ_PADDING;
	if (point.x >= indentX && point.x <= indentX + INDICATOR_MARGIN && !item.pNode->sorted.empty())
	{
		if (m_expanded.count(item.pNode))
			m_expanded.erase(item.pNode);
		else
			m_expanded.insert(item.pNode);
	}

	m_selectedIndex = itemIdx;
	Refresh();
}

void TreeView::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	int rowHeight = GetRowHeight();
	if (rowHeight <= 0)
		return;

	int rowIdx = point.y / rowHeight;
	int itemIdx = m_scrollPosV + rowIdx;
	if (itemIdx < 0 || itemIdx >= GetNumItems())
		return;

	VisibleItem& item = m_visibleItems[itemIdx];

	// Toggle expand/collapse on double-click.
	if (!item.pNode->sorted.empty())
	{
		if (m_expanded.count(item.pNode))
			m_expanded.erase(item.pNode);
		else
			m_expanded.insert(item.pNode);
	}

	m_selectedIndex = itemIdx;
	Refresh();
}

void TreeView::OnRButtonDown(UINT nFlags, CPoint point)
{
	int rowHeight = GetRowHeight();
	if (rowHeight <= 0)
		return;

	int rowIdx = point.y / rowHeight;
	int itemIdx = m_scrollPosV + rowIdx;
	if (itemIdx < 0 || itemIdx >= GetNumItems())
		return;

	m_selectedIndex = itemIdx;
	Refresh();

	VisibleItem& item = m_visibleItems[itemIdx];

	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(MF_STRING | (item.depth == 0 ? MF_GRAYED : 0), 1, "Copy Row");
	menu.AppendMenu(MF_STRING | (item.pNode->sorted.empty() ? MF_GRAYED : 0), 2, "Expand All Child Nodes");
	menu.AppendMenu(MF_STRING | (item.depth == 0 ? MF_GRAYED : 0), 3, "Go To Parent Node");
	menu.AppendMenu(MF_STRING | (item.depth <= 1 ? MF_GRAYED : 0), 4, "Go To Top-Level Node");

	ClientToScreen(&point);
	int cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN, point.x, point.y, this);

	switch (cmd)
	{
	case 1:
	{
		CopyTextToClipboard(this, item.GetItemText(m_pRoot));
		break;
	}
	case 2:
		ExpandAll(item.pNode);
		Refresh();
		break;
	case 3:
	{
		// Find parent: nearest preceding visible item with depth - 1.
		int parentIdx = -1;
		for (int i = itemIdx - 1; i >= 0; i--)
		{
			if (m_visibleItems[i].depth == item.depth - 1)
			{
				parentIdx = i;
				break;
			}
		}
		if (parentIdx >= 0)
		{
			m_selectedIndex = parentIdx;
			EnsureSelectionVisible();
			// Ensure horizontal visibility of the parent node label.
			int parentX = m_visibleItems[parentIdx].depth * INDENT_WIDTH + HORIZ_PADDING + INDICATOR_MARGIN;
			if (m_scrollPosH > parentX)
				m_scrollPosH = parentX;
			Refresh();
		}
		break;
	}
	case 4:
	{
		// Find top-level node: nearest preceding visible item with depth == 1.
		int topIdx = -1;
		for (int i = itemIdx; i >= 0; i--)
		{
			if (m_visibleItems[i].depth == 1)
			{
				topIdx = i;
				break;
			}
		}
		if (topIdx >= 0)
		{
			m_selectedIndex = topIdx;
			EnsureSelectionVisible();
			int topX = m_visibleItems[topIdx].depth * INDENT_WIDTH + HORIZ_PADDING + INDICATOR_MARGIN;
			if (m_scrollPosH > topX)
				m_scrollPosH = topX;
			Refresh();
		}
		break;
	}
	}
}

void TreeView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	int numItems = GetNumItems();
	if (numItems == 0)
		return;

	int newIndex = m_selectedIndex;

	if (nChar == VK_UP && newIndex > 0)
		newIndex--;
	else if (nChar == VK_DOWN && newIndex < numItems - 1)
		newIndex++;
	else
		return;

	m_selectedIndex = newIndex;

	// Scroll to keep the selection visible.
	int maxVisible = GetMaxVisibleItems();
	if (m_selectedIndex < m_scrollPosV)
		m_scrollPosV = m_selectedIndex;
	else if (m_selectedIndex >= m_scrollPosV + maxVisible)
		m_scrollPosV = m_selectedIndex - maxVisible + 1;

	UpdateScrollBars();
	Draw();
	Invalidate();
}
