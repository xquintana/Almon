#include "stdafx.h"
#include "ListView.h"
#include "resource.h"
#include "Utils.h"

IMPLEMENT_DYNAMIC(ListView, ScrollableView)

BEGIN_MESSAGE_MAP(ListView, ScrollableView)
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_SETCURSOR()
	ON_WM_KEYDOWN()
	ON_WM_RBUTTONDOWN()
END_MESSAGE_MAP()

ListView::ListView()
{
	m_colHeaders[0] = "Num Allocs";
	m_colHeaders[1] = "Total Size (Bytes)";
	m_colHeaders[2] = "Bytes in Use";

	m_colWidths[0] = COL_WIDTH_NUM_ALLOCS;
	m_colWidths[1] = COL_WIDTH_TOTAL_SIZE;
	m_colWidths[2] = COL_WIDTH_BYTES_USE;

	m_colSortOrders[0] = Order::Asc;
	m_colSortOrders[1] = Order::Asc;
	m_colSortOrders[2] = Order::Asc;

	SetFont("Calibri");
}

ListView::~ListView()
{
	m_fontHeader.DeleteObject();
}

BOOL ListView::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN)
	{
		switch (pMsg->wParam)
		{
		case VK_UP:
		case VK_DOWN:
		case VK_PRIOR:
		case VK_NEXT:
		case VK_HOME:
		case VK_END:
			// Prevent the dialog manager from using these keys for
			// control navigation. Dispatch them as WM_KEYDOWN instead.
			SendMessage(pMsg->message, pMsg->wParam, pMsg->lParam);
			return TRUE;
		case 'C':
			if (GetKeyState(VK_CONTROL) & 0x8000)
			{
				if (m_selectedIndex >= 0 && m_selectedIndex < GetNumItems())
				{
					CString text;
					for (int c = 0; c < NUM_COLUMNS; c++)
					{
						if (c > 0) text += "\t";
						text += GetItemText(m_selectedIndex, c);
					}
					CopyTextToClipboard(this, text);
				}
				return TRUE;
			}
			break;
		}
	}
	return BaseView::PreTranslateMessage(pMsg);
}

void ListView::SetFont(const CString& fontName, int height)
{
	BaseView::SetFont(fontName, height);

	m_fontHeader.DeleteObject();
	m_fontHeader.CreateFont(height, 0, 0, 0, FW_NORMAL,
		FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		DEFAULT_PITCH | FF_SWISS, fontName);

	UpdateColMinWidths();
}

void ListView::UpdateColMinWidths()
{
	CDC dc;
	dc.CreateCompatibleDC(nullptr);
	CFont* pOldFont = dc.SelectObject(&m_fontHeader);

	int arrowSpace = ARROW_WIDTH + ARROW_GAP;
	int padding = COL_PADDING * 2; // left + right DeflateRect

	for (int i = 0; i < NUM_COLUMNS; i++)
	{
		CSize sz = dc.GetTextExtent(m_colHeaders[i]);
		m_colMinWidths[i] = sz.cx + arrowSpace + padding;
	}

	dc.SelectObject(pOldFont);
}

int ListView::GetRowHeight()
{
	return GetFontHeight(&m_font) + 4;
}

int ListView::GetHeaderHeight()
{
	return GetFontHeight(&m_fontHeader) + 8;
}

int ListView::GetMaxVisibleItems()
{
	int rowHeight = GetRowHeight();
	if (rowHeight <= 0)
		return 0;

	int availableHeight = m_rect.Height() - GetHeaderHeight();
	if (availableHeight <= 0)
		return 0;

	return availableHeight / rowHeight;
}

int ListView::GetNumItems()
{
	return static_cast<int>(m_items.size());
}

int ListView::GetTotalColumnsWidth()
{
	int total = 0;
	for (int i = 0; i < NUM_COLUMNS; i++)
		total += m_colWidths[i];
	return total;
}

int ListView::GetMaxItemWidth()
{
	if (GetNumItems() == 0)
		return 0;
	return GetTotalColumnsWidth();
}

void ListView::SetVertScrollPos(int pos)
{
	m_scrollPosV = pos;
	if (m_scrollPosV < 0)
		m_scrollPosV = 0;

	int maxScroll = GetMaxVertScroll();
	if (m_scrollPosV > maxScroll)
		m_scrollPosV = maxScroll;
}

void ListView::EnsureVisible(int index)
{
	if (index < 0 || index >= GetNumItems())
		return;

	int maxVisible = GetMaxVisibleItems();
	if (maxVisible <= 0)
		return;

	if (index < m_scrollPosV)
		m_scrollPosV = index;
	else if (index >= m_scrollPosV + maxVisible)
		m_scrollPosV = index - maxVisible + 1;
}

void ListView::Refresh()
{
	if (m_dcMem.GetSafeHdc() == nullptr && GetSafeHwnd() != nullptr)
	{
		CClientDC dc(this);
		CRect rcClient;
		GetClientRect(&rcClient);
		CreateBackBuffer(dc, rcClient);
		m_rect = rcClient;
	}

	if (m_selectedIndex >= 0)
		EnsureVisible(m_selectedIndex);

	UpdateScrollBars();
	Draw();
	Invalidate();
}

void ListView::Draw()
{
	if (m_dcMem.GetSafeHdc() == nullptr || m_bSorting)
		return;

	DrawBackground();
	DrawHeader();
	DrawRows();

	// Frame the entire client area
	CPen penFrame(PS_SOLID, 1, CLR_GRID);
	CPen* pOldPen = m_dcMem.SelectObject(&penFrame);
	CBrush* pOldBrush = (CBrush*)m_dcMem.SelectStockObject(NULL_BRUSH);
	m_dcMem.Rectangle(&m_rect);
	m_dcMem.SelectObject(pOldBrush);
	m_dcMem.SelectObject(pOldPen);
}

void ListView::DrawHeader()
{
	int headerHeight = GetHeaderHeight();
	CFont* pOldFont = m_dcMem.SelectObject(&m_fontHeader);
	int oldBkMode = m_dcMem.SetBkMode(TRANSPARENT);
	COLORREF oldTextColor = m_dcMem.SetTextColor(CLR_TEXT);

	// Header background
	CBrush brushHeader(CLR_HEADER_BG);
	CRect rcHeader(m_rect.left, m_rect.top, m_rect.right, m_rect.top + headerHeight);
	m_dcMem.FillRect(&rcHeader, &brushHeader);

	// Header grid lines
	CPen penGrid(PS_SOLID, 1, CLR_GRID);
	CPen* pOldPen = m_dcMem.SelectObject(&penGrid);

	// Bottom border of header
	m_dcMem.MoveTo(m_rect.left, m_rect.top + headerHeight);
	m_dcMem.LineTo(m_rect.right, m_rect.top + headerHeight);

	// Column headers text and vertical separators
	int x = m_rect.left - m_scrollPosH;
	for (int i = 0; i < NUM_COLUMNS; i++)
	{
		CRect rcCol(x, m_rect.top, x + m_colWidths[i], m_rect.top + headerHeight);
		rcCol.DeflateRect(4, 0, 4, 0);

		// Draw sort arrow if this column is the sorted one
		if (i == m_sortedColIdx)
		{
			int arrowW = 8;
			int arrowH = 5;
			int arrowX = rcCol.left;
			int arrowY = rcCol.top + (rcCol.Height() - arrowH) / 2;

			POINT pts[3];
			if (m_colSortOrders[i] == Order::Asc)
			{
				pts[0] = { arrowX + arrowW / 2, arrowY };
				pts[1] = { arrowX,               arrowY + arrowH };
				pts[2] = { arrowX + arrowW,      arrowY + arrowH };
			}
			else
			{
				pts[0] = { arrowX,               arrowY };
				pts[1] = { arrowX + arrowW,      arrowY };
				pts[2] = { arrowX + arrowW / 2,  arrowY + arrowH };
			}

			Gdiplus::Graphics gfx(m_dcMem.GetSafeHdc());
			gfx.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
			Gdiplus::SolidBrush brushArrow(Gdiplus::Color(128, 128, 128));
			Gdiplus::PointF gdiPts[3] = {
				{ (float)pts[0].x, (float)pts[0].y },
				{ (float)pts[1].x, (float)pts[1].y },
				{ (float)pts[2].x, (float)pts[2].y }
			};
			gfx.FillPolygon(&brushArrow, gdiPts, 3);

			rcCol.left += arrowW + 4;
		}

		m_dcMem.DrawText(m_colHeaders[i], &rcCol, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);

		x += m_colWidths[i];

		// Vertical separator
		m_dcMem.MoveTo(x, m_rect.top);
		m_dcMem.LineTo(x, m_rect.top + headerHeight);
	}

	m_dcMem.SelectObject(pOldPen);
	m_dcMem.SetTextColor(oldTextColor);
	m_dcMem.SetBkMode(oldBkMode);
	m_dcMem.SelectObject(pOldFont);
}

void ListView::DrawRows()
{
	int rowHeight = GetRowHeight();
	int headerHeight = GetHeaderHeight();
	int maxVisible = GetMaxVisibleItems();
	int numItems = GetNumItems();

	CFont* pOldFont = m_dcMem.SelectObject(&m_font);
	int oldBkMode = m_dcMem.SetBkMode(TRANSPARENT);
	COLORREF oldTextColor = m_dcMem.SetTextColor(CLR_TEXT);

	CPen penGrid(PS_SOLID, 1, CLR_GRID);
	CPen* pOldPen = m_dcMem.SelectObject(&penGrid);

	CBrush brushRow(CLR_ROW_BG);
	CBrush brushSelected(CLR_ROW_SELECTED);

	int endIdx = min(m_scrollPosV + maxVisible, numItems);

	for (int i = m_scrollPosV; i < endIdx; i++)
	{
		int rowIdx = i - m_scrollPosV;
		int y = m_rect.top + headerHeight + rowIdx * rowHeight;

		CRect rcRow(m_rect.left, y, m_rect.right, y + rowHeight);

		// Row background
		if (i == m_selectedIndex)
			m_dcMem.FillRect(&rcRow, &brushSelected);
		else
			m_dcMem.FillRect(&rcRow, &brushRow);

		// Column values
		CallStackInfo* pInfo = m_items[i];
		CString values[NUM_COLUMNS];
		values[0].Format("%d", pInfo->numAllocs);
		values[1].Format("%zu", pInfo->numBytes);
		values[2].Format("%zu", pInfo->numBytesInUse);

		m_dcMem.SetTextColor(i == m_selectedIndex ? CLR_TEXT_SELECTED : CLR_TEXT);

		int x = m_rect.left - m_scrollPosH;
		for (int c = 0; c < NUM_COLUMNS; c++)
		{
			CRect rcCell(x, y, x + m_colWidths[c], y + rowHeight);
			rcCell.DeflateRect(4, 0, 4, 0);
			m_dcMem.DrawText(values[c], &rcCell, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);

			x += m_colWidths[c];

			// Vertical separator
			m_dcMem.MoveTo(x, y);
			m_dcMem.LineTo(x, y + rowHeight);
		}
	}

	// Draw horizontal grid lines between rows after all backgrounds are filled,
	// so they are not overwritten by the next row's FillRect.
	for (int i = m_scrollPosV + 1; i < endIdx; i++)
	{
		int rowIdx = i - m_scrollPosV;
		int y = m_rect.top + headerHeight + rowIdx * rowHeight;
		m_dcMem.MoveTo(m_rect.left, y);
		m_dcMem.LineTo(m_rect.right, y);
	}

	m_dcMem.SelectObject(pOldPen);
	m_dcMem.SetTextColor(oldTextColor);
	m_dcMem.SetBkMode(oldBkMode);
	m_dcMem.SelectObject(pOldFont);
}

int ListView::HitTestColumn(int x)
{
	int colX = m_rect.left - m_scrollPosH;
	for (int i = 0; i < NUM_COLUMNS; i++)
	{
		colX += m_colWidths[i];
		if (x < colX)
			return i;
	}
	return -1;
}

int ListView::HitTestColumnBorder(int x)
{
	int colX = m_rect.left - m_scrollPosH;
	for (int i = 0; i < NUM_COLUMNS; i++)
	{
		colX += m_colWidths[i];
		if (abs(x - colX) <= COL_BORDER_TOLERANCE)
			return i;
	}
	return -1;
}

int ListView::HitTestRow(int y)
{
	int rowHeight = GetRowHeight();
	int headerHeight = GetHeaderHeight();

	if (y < m_rect.top + headerHeight)
		return -1; // In header area

	int rowIdx = (y - m_rect.top - headerHeight) / rowHeight;
	int itemIdx = m_scrollPosV + rowIdx;

	if (itemIdx >= 0 && itemIdx < GetNumItems())
		return itemIdx;

	return -2; // Out of range
}

void ListView::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (GetNumItems() == 0)
		return;

	SetFocus();
	int headerHeight = GetHeaderHeight();

	if (point.y < m_rect.top + headerHeight)
	{
		// Check for column border drag first
		int borderCol = HitTestColumnBorder(point.x);
		if (borderCol >= 0)
		{
			m_bResizing = true;
			m_resizeColIdx = borderCol;
			m_resizeStartX = point.x;
			m_resizeOrigWidth = m_colWidths[borderCol];
			SetCapture();
			return;
		}

		// Header click
		int col = HitTestColumn(point.x);
		if (col >= 0)
		{
			CWnd* pParent = GetParent();
			if (pParent)
				pParent->PostMessage(WM_LISTVIEW_HEADER_CLICKED, (WPARAM)col, 0);
		}
	}
	else
	{
		// Row click
		int itemIdx = HitTestRow(point.y);
		if (itemIdx >= 0)
		{
			m_selectedIndex = itemIdx;
			Draw();
			Invalidate();
			CWnd* pParent = GetParent();
			if (pParent)
				pParent->PostMessage(WM_LISTVIEW_ROW_CLICKED, (WPARAM)itemIdx, 0);
		}
	}

	BaseView::OnLButtonDown(nFlags, point);
}

void ListView::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_bResizing)
	{
		m_bResizing = false;
		m_resizeColIdx = -1;
		ReleaseCapture();
	}

	BaseView::OnLButtonUp(nFlags, point);
}

void ListView::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_bResizing && m_resizeColIdx >= 0)
	{
		int delta = point.x - m_resizeStartX;
		int newWidth = m_resizeOrigWidth + delta;
		if (newWidth < m_colMinWidths[m_resizeColIdx])
			newWidth = m_colMinWidths[m_resizeColIdx];

		m_colWidths[m_resizeColIdx] = newWidth;
		UpdateScrollBars();
		Draw();
		Invalidate();
	}

	BaseView::OnMouseMove(nFlags, point);
}

BOOL ListView::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	if (nHitTest == HTCLIENT)
	{
		CPoint pt;
		GetCursorPos(&pt);
		ScreenToClient(&pt);

		int headerHeight = GetHeaderHeight();
		if (pt.y < m_rect.top + headerHeight)
		{
			if (HitTestColumnBorder(pt.x) >= 0)
			{
				::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZEWE));
				return TRUE;
			}
		}
	}
	return BaseView::OnSetCursor(pWnd, nHitTest, message);
}

void ListView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	int maxVisible = GetMaxVisibleItems();
	int numItems = GetNumItems();
	if (numItems == 0)
	{
		BaseView::OnKeyDown(nChar, nRepCnt, nFlags);
		return;
	}

	int maxScroll = numItems - maxVisible;
	if (maxScroll < 0) maxScroll = 0;
	bool changed = false;

	switch (nChar)
	{
	case VK_UP:
		if (m_selectedIndex > 0)
		{
			m_selectedIndex--;
			EnsureVisible(m_selectedIndex);
			changed = true;

			CWnd* pParent = GetParent();
			if (pParent)
				pParent->PostMessage(WM_LISTVIEW_ROW_CLICKED, (WPARAM)m_selectedIndex, 0);
		}
		break;
	case VK_DOWN:
		if (m_selectedIndex < numItems - 1)
		{
			m_selectedIndex++;
			EnsureVisible(m_selectedIndex);
			changed = true;

			CWnd* pParent = GetParent();
			if (pParent)
				pParent->PostMessage(WM_LISTVIEW_ROW_CLICKED, (WPARAM)m_selectedIndex, 0);
		}
		break;
	case VK_PRIOR: // Page Up
		m_scrollPosV -= maxVisible;
		if (m_scrollPosV < 0) m_scrollPosV = 0;
		changed = true;
		break;
	case VK_NEXT: // Page Down
		m_scrollPosV += maxVisible;
		if (m_scrollPosV > maxScroll) m_scrollPosV = maxScroll;
		changed = true;
		break;
	case VK_HOME:
		m_scrollPosV = 0;
		changed = true;
		break;
	case VK_END:
		m_scrollPosV = maxScroll;
		changed = true;
		break;
	}

	if (changed)
	{
		UpdateScrollBars();
		Draw();
		Invalidate();
	}

	BaseView::OnKeyDown(nChar, nRepCnt, nFlags);
}

void ListView::SortByColumn(int colIdx, Order order)
{
	if (colIdx < 0 || colIdx >= NUM_COLUMNS)
		return;

	if (order == Order::Toggle)
		order = (m_colSortOrders[colIdx] == Order::Asc) ? Order::Desc : Order::Asc;

	m_colSortOrders[colIdx] = order;
	m_sortedColIdx = colIdx;

	m_bSorting = true;

	auto items = &m_items;
	auto future = std::async(std::launch::async, [items, colIdx, order]()
		{
			std::sort(items->begin(), items->end(),
				[colIdx, order](const CallStackInfo* a, const CallStackInfo* b)
				{
					switch (colIdx)
					{
					case 0:
						return (order == Order::Asc) ? (a->numAllocs < b->numAllocs) : (a->numAllocs > b->numAllocs);
					case 1:
						return (order == Order::Asc) ? (a->numBytes < b->numBytes) : (a->numBytes > b->numBytes);
					case 2:
						return (order == Order::Asc) ? (a->numBytesInUse < b->numBytesInUse) : (a->numBytesInUse > b->numBytesInUse);
					default:
						return false;
					}
				});
		});

	while (future.wait_for(std::chrono::milliseconds(200)) != std::future_status::ready)
		ProcessWinMessages();

	m_bSorting = false;
	m_selectedIndex = -1;
	Refresh();
}

void ListView::OnRButtonDown(UINT nFlags, CPoint point)
{
	int rowIdx = HitTestRow(point.y);
	int colIdx = HitTestColumn(point.x);
	if (rowIdx < 0 || colIdx < 0)
		return;

	m_selectedIndex = rowIdx;
	Refresh();

	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(MF_STRING, 1, "Copy Value");
	menu.AppendMenu(MF_STRING, 2, "Copy Row");

	ClientToScreen(&point);
	int cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN, point.x, point.y, this);

	CString text;
	switch (cmd)
	{
	case 1:
		text = GetItemText(rowIdx, colIdx);
		break;
	case 2:
		for (int c = 0; c < NUM_COLUMNS; c++)
		{
			if (c > 0) text += "\t";
			text += GetItemText(rowIdx, c);
		}
		break;
	default:
		return;
	}

	CopyTextToClipboard(this, text);
}

CString ListView::GetItemText(int rowIdx, int colIdx)
{
	if (rowIdx < 0 || rowIdx >= GetNumItems() || colIdx < 0 || colIdx >= NUM_COLUMNS)
		return CString();

	CallStackInfo* pInfo = m_items[rowIdx];
	CString text;
	switch (colIdx)
	{
	case 0: text.Format("%d", pInfo->numAllocs); break;
	case 1: text.Format("%zu", pInfo->numBytes); break;
	case 2: text.Format("%zu", pInfo->numBytesInUse); break;
	}
	return text;
}
