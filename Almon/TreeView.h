#pragma once

#include "StackView.h"
#include <unordered_set>
#include <iosfwd>

class ProgressBar;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TreeNode
// Represents an element in the tree hierarchy.

struct TreeNode
{
	void Insert(const CallStack& stack, size_t sizeLeaf, DWORD level, CString** pFrames);
	void Sort();
	void Clear();
	int Count();

	bool PrintTXT(std::ofstream& file, ProgressBar* pProgress);
	void PrintTXT(std::vector<CString>& items, int indent, ProgressBar* pProgress);
	bool PrintXML(std::ofstream& file, ProgressBar* pProgress);
	void PrintXML(std::vector<CString>& items, int indent, ProgressBar* pProgress);

	size_t size{}; // Accumulated size of this frame and all the children.
	CString* pFrame{}; // The frame string of this node. It can be null for the root node or if the frame string is not available.
	std::unordered_map<Address, TreeNode> nodes{}; // Used during building for fast O(1) lookups.
	std::vector<TreeNode*> sorted{}; // Children sorted by size descending, populated by Sort().
	static constexpr int auxStrMaxLen{ 2048 };
	inline static char auxStr[auxStrMaxLen]{};
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TreeView
// A control that displays the call stack information as a tree view with copy and scroll support.

class TreeView : public StackView
{
	DECLARE_DYNAMIC(TreeView)

	static constexpr int INDENT_WIDTH = 16;
	static constexpr int INDICATOR_MARGIN = 14; // Space between the expand/collapse indicator and the text.
	static constexpr int HORIZ_PADDING = 4; // Additional horizontal padding to ensure text is not too close to the right edge.

	static constexpr COLORREF CLR_GRID = RGB(240, 240, 240);

public:
	void SetModel(TreeNode* pRoot);
	void Refresh();
	void Clear() override;

	BOOL PreTranslateMessage(MSG* pMsg) override;

protected:
	void Draw() override;

	DECLARE_MESSAGE_MAP()
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);

private:
	struct VisibleItem
	{
		TreeNode* pNode;
		int depth;
		CString GetItemText(TreeNode* pRoot) const
		{
			if (pNode == pRoot)
				return "Root";
			else if (pNode->pFrame)
				return StringFmt("%s [%zu bytes]", pNode->pFrame->GetString(), pNode->size);
			else
				return StringFmt("[%zu bytes]", pNode->size);
		}
	};

	int  GetNumItems() override;
	int  GetMaxVisibleItems() override;
	int  GetMaxItemWidth() override;
	void EnsureSelectionVisible() override;
	int  GetRowHeight();
	void RebuildVisibleItems();
	void RebuildVisibleItems(TreeNode* pNode, int depth);
	void ExpandToDepth(TreeNode* pNode, int depth, int maxDepth);
	void ExpandAll(TreeNode* pNode);
	void DrawRows();

private:
	TreeNode* m_pRoot{};
	std::vector<VisibleItem> m_visibleItems;
	std::unordered_set<TreeNode*> m_expanded;
	int m_selectedIndex{ -1 };
	int m_cachedMaxItemWidth{};
	void RecalcMaxItemWidth();
};
