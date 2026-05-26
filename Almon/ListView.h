#pragma once

#include "ScrollableView.h"

#define WM_LISTVIEW_ROW_CLICKED		(WM_APP + 100)
#define WM_LISTVIEW_HEADER_CLICKED	(WM_APP + 101)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ListView
// A list control that shows, for each call stack, the count and size of its allocations, as well as the memory in use.

class ListView : public ScrollableView
{
	DECLARE_DYNAMIC(ListView)

	enum class Order
	{
		Same,
		Asc,
		Desc,
		Toggle
	};

	static constexpr int COL_WIDTH_NUM_ALLOCS = 81;
	static constexpr int COL_WIDTH_TOTAL_SIZE = 115;
	static constexpr int COL_WIDTH_BYTES_USE = 115;

	static constexpr int NUM_COLUMNS = 3;
	static constexpr int COL_BORDER_TOLERANCE = 4;
	static constexpr int ARROW_WIDTH = 8;
	static constexpr int ARROW_HEIGHT = 5;
	static constexpr int ARROW_GAP = 6;
	static constexpr int COL_PADDING = 4; // left and right padding inside each column.

	static constexpr COLORREF CLR_HEADER_BG = RGB(240, 240, 240); // light gray
	static constexpr COLORREF CLR_ROW_BG = RGB(255, 255, 255); // white
	static constexpr COLORREF CLR_ROW_SELECTED = CLR_SELECTED_BG; // blue
	static constexpr COLORREF CLR_GRID = RGB(240, 240, 240);// light gray
	static constexpr COLORREF CLR_TEXT = RGB(0, 0, 0); // black
	static constexpr COLORREF CLR_TEXT_SELECTED = RGB(255, 255, 255); // white

public:
	ListView();
	~ListView();

	std::vector<CallStackInfo*>& GetItems() { return m_items; }
	CallStackInfo* GetItem(int idx) { return m_items[idx]; }
	int  GetNumItems() override;
	void Clear() { m_items.clear(); }
	void SetSelectedItem(int idx) { m_selectedIndex = idx; Invalidate(); }
	void SetFont(const CString& fontName, int height = DEFAULT_FONT_SIZE) override;
	void SetVertScrollPos(int pos);
	void Refresh();
	void SortByColumn(int colIdx, Order order = Order::Same);
	CString GetItemText(int rowIdx, int colIdx);

	BOOL PreTranslateMessage(MSG* pMsg) override;

protected:
	void Draw() override;

	DECLARE_MESSAGE_MAP()
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);

private:
	int  GetMaxVisibleItems() override;
	int  GetMaxItemWidth() override;
	int  GetRowHeight();
	int  GetHeaderHeight();
	void UpdateColMinWidths();
	void EnsureSelectionVisible() override { if (m_selectedIndex >= 0) EnsureVisible(m_selectedIndex); }
	int  GetTotalColumnsWidth();
	int  HitTestColumn(int x);
	int  HitTestColumnBorder(int x);
	int  HitTestRow(int y);
	void EnsureVisible(int index);
	void DrawHeader();
	void DrawRows();

private:
	std::vector<CallStackInfo*> m_items;

	CFont m_fontHeader;
	int m_colWidths[NUM_COLUMNS];
	int m_colMinWidths[NUM_COLUMNS]{};
	CString m_colHeaders[NUM_COLUMNS];
	Order m_colSortOrders[NUM_COLUMNS]; // parallel to columns, tracks current sort order for each column.

	int m_selectedIndex{ -1 }; // index into m_items, -1 means none.
	int m_sortedColIdx{ -1 };  // which column is currently sorted, -1 means none.
	bool m_bSorting{};

	// Column resize.
	bool m_bResizing{};
	int  m_resizeColIdx{ -1 };
	int  m_resizeStartX{};
	int  m_resizeOrigWidth{};
};
