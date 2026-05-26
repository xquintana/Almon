#include "stdafx.h"
#include "ScrollableView.h"

IMPLEMENT_DYNAMIC(ScrollableView, BaseView)

BEGIN_MESSAGE_MAP(ScrollableView, BaseView)
	ON_WM_VSCROLL()
	ON_WM_HSCROLL()
	ON_WM_MOUSEWHEEL()
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
	ON_WM_NCHITTEST()
	ON_WM_NCLBUTTONDOWN()
	ON_WM_NCLBUTTONDBLCLK()
END_MESSAGE_MAP()


void ScrollableView::Clear()
{
	m_scrollPosV = 0;
	m_scrollPosH = 0;
}

void ScrollableView::SetVerticalScrollEnabled(bool enable)
{
	LONG style = GetWindowLong(m_hWnd, GWL_STYLE);
	bool hasScroll = (style & WS_VSCROLL) != 0;
	if (hasScroll == enable)
		return;

	if (enable)
		style |= WS_VSCROLL;
	else
		style &= ~WS_VSCROLL;

	SetWindowLong(m_hWnd, GWL_STYLE, style);

	SetWindowPos(nullptr, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
		SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void ScrollableView::SetHorizontalScrollEnabled(bool enable)
{
	LONG style = GetWindowLong(m_hWnd, GWL_STYLE);
	bool hasScroll = (style & WS_HSCROLL) != 0;
	if (hasScroll == enable)
		return;

	if (enable)
		style |= WS_HSCROLL;
	else
		style &= ~WS_HSCROLL;

	SetWindowLong(m_hWnd, GWL_STYLE, style);

	SetWindowPos(nullptr, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
		SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

int ScrollableView::GetMaxVertScroll()
{
	return max(GetNumItems() - GetMaxVisibleItems(), 0);
}

int ScrollableView::GetMaxHorizScroll()
{
	return max(GetMaxItemWidth() - m_rect.Width(), 0);
}

void ScrollableView::UpdateScrollBars()
{
	if (GetSafeHwnd() == nullptr)
		return;

	// Vertical scrollbar.

	SCROLLINFO siVert{};
	siVert.cbSize = sizeof(SCROLLINFO);
	siVert.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;

	int maxVisible = GetMaxVisibleItems();
	int numItems = GetNumItems();

	if (maxVisible > 0 && numItems > maxVisible)
	{
		int maxScrollPos = numItems - maxVisible;
		if (m_scrollPosV > maxScrollPos)
			m_scrollPosV = maxScrollPos;
		siVert.nMax = numItems - 1;
		siVert.nPage = maxVisible;
		siVert.nPos = m_scrollPosV;
		SetVerticalScrollEnabled(true);
	}
	else
	{
		SetVerticalScrollEnabled(false);
		m_scrollPosV = 0;
	}
	SetScrollInfo(SB_VERT, &siVert, TRUE);

	// Horizontal scrollbar.	

	SCROLLINFO siHoriz{};
	siHoriz.cbSize = sizeof(SCROLLINFO);
	siHoriz.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;

	int maxItemWidth = GetMaxItemWidth();
	int clientWidth = m_rect.Width();

	if (maxItemWidth > clientWidth)
	{
		siHoriz.nMax = maxItemWidth - 1;
		siHoriz.nPage = clientWidth;
		siHoriz.nPos = m_scrollPosH;
		SetHorizontalScrollEnabled(true);
	}
	else
	{
		SetHorizontalScrollEnabled(false);
		m_scrollPosH = 0;
	}
	SetScrollInfo(SB_HORZ, &siHoriz, TRUE);
}

void ScrollableView::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	int maxVisible = GetMaxVisibleItems();
	int maxVScroll = GetMaxVertScroll();

	if (nSBCode == SB_THUMBTRACK || nSBCode == SB_THUMBPOSITION)
	{
		SCROLLINFO si{};
		si.cbSize = sizeof(si);
		si.fMask = SIF_TRACKPOS;
		GetScrollInfo(SB_VERT, &si);
		m_scrollPosV = si.nTrackPos;

		if (m_scrollPosV < 0) m_scrollPosV = 0;
		if (m_scrollPosV > maxVScroll) m_scrollPosV = maxVScroll;

		Draw();
		Invalidate(FALSE);
		return;
	}

	switch (nSBCode)
	{
	case SB_LINEUP:        m_scrollPosV--; break;
	case SB_LINEDOWN:      m_scrollPosV++; break;
	case SB_PAGEUP:        m_scrollPosV -= maxVisible; break;
	case SB_PAGEDOWN:      m_scrollPosV += maxVisible; break;
	case SB_TOP:           m_scrollPosV = 0; break;
	case SB_BOTTOM:        m_scrollPosV = maxVScroll; break;
	}

	if (m_scrollPosV < 0) m_scrollPosV = 0;
	if (m_scrollPosV > maxVScroll) m_scrollPosV = maxVScroll;

	SCROLLINFO siPos{};
	siPos.cbSize = sizeof(SCROLLINFO);
	siPos.fMask = SIF_POS;
	siPos.nPos = m_scrollPosV;
	SetScrollInfo(SB_VERT, &siPos, TRUE);

	Draw();
	Invalidate(FALSE);
}

void ScrollableView::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	int clientWidth = m_rect.Width();
	int maxHScroll = GetMaxHorizScroll();

	if (nSBCode == SB_THUMBTRACK || nSBCode == SB_THUMBPOSITION)
	{
		SCROLLINFO si{};
		si.cbSize = sizeof(si);
		si.fMask = SIF_TRACKPOS;
		GetScrollInfo(SB_HORZ, &si);
		m_scrollPosH = si.nTrackPos;

		if (m_scrollPosH < 0) m_scrollPosH = 0;
		if (m_scrollPosH > maxHScroll) m_scrollPosV = maxHScroll;

		Draw();
		Invalidate(FALSE);
		return;
	}

	switch (nSBCode)
	{
	case SB_LINELEFT:      m_scrollPosH -= 20; break;
	case SB_LINERIGHT:     m_scrollPosH += 20; break;
	case SB_PAGELEFT:      m_scrollPosH -= clientWidth; break;
	case SB_PAGERIGHT:     m_scrollPosH += clientWidth; break;
	case SB_LEFT:          m_scrollPosH = 0; break;
	case SB_RIGHT:         m_scrollPosH = maxHScroll; break;
	}

	if (m_scrollPosH < 0) m_scrollPosH = 0;
	if (m_scrollPosH > maxHScroll) m_scrollPosH = maxHScroll;

	SCROLLINFO siPos{};
	siPos.cbSize = sizeof(SCROLLINFO);
	siPos.fMask = SIF_POS;
	siPos.nPos = m_scrollPosH;
	SetScrollInfo(SB_HORZ, &siPos, TRUE);

	Draw();
	Invalidate(FALSE);
}

BOOL ScrollableView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	int lines = zDelta / WHEEL_DELTA * 3;
	m_scrollPosV -= lines;

	int maxVisible = GetMaxVisibleItems();
	int maxScroll = GetMaxVertScroll();

	if (m_scrollPosV < 0) m_scrollPosV = 0;
	if (m_scrollPosV > maxScroll) m_scrollPosV = maxScroll;

	UpdateScrollBars();
	Draw();
	Invalidate(FALSE);

	return TRUE;
}

BOOL ScrollableView::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;
}

void ScrollableView::OnSize(UINT nType, int cx, int cy)
{
	int prevScrollPosV = m_scrollPosV;
	BaseView::OnSize(nType, cx, cy);
	// Give derived views a chance to keep their selection visible after
	// the available space has changed (e.g. when the dialog is restored
	// from maximized to a smaller size).
	EnsureSelectionVisible();
	UpdateScrollBars();
	// BaseView::OnSize() drew the back buffer using the pre-clamp scroll
	// position. If UpdateScrollBars() clamped m_scrollPosV (e.g. when the
	// window grew enough to expose space below the last item), the back
	// buffer is now stale. Redraw it so the content reflects the new
	// scroll position.
	if (m_scrollPosV != prevScrollPosV)
		Draw();
	// Ensure the NC area is erased
	RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME | RDW_ERASE | RDW_ALLCHILDREN);
}

LRESULT ScrollableView::OnNcHitTest(CPoint point)
{
	// Bypass the static control's WM_NCHITTEST which returns HTCLIENT for
	// all hits (when SS_NOTIFY is set), preventing scrollbar interaction.
	// DefWindowProc correctly identifies HTVSCROLL for the scrollbar region.
	return ::DefWindowProc(m_hWnd, WM_NCHITTEST, 0, MAKELPARAM(point.x, point.y));
}

void ScrollableView::OnNcLButtonDown(UINT nHitTest, CPoint point)
{
	if (nHitTest == HTVSCROLL || nHitTest == HTHSCROLL)
	{
		// DefWindowProc enters a modal tracking loop that processes WM_VSCROLL/
		// WM_HSCROLL messages until the mouse is released.
		::DefWindowProc(m_hWnd, WM_NCLBUTTONDOWN, nHitTest, MAKELPARAM(point.x, point.y));

		// After the modal loop exits, resync the scrollbar position.
		// Without this, Windows may briefly render the thumb at a stale
		// track position before the next non-client repaint picks up our
		// SIF_POS value.
		SCROLLINFO si{};
		si.cbSize = sizeof(SCROLLINFO);
		si.fMask = SIF_POS;
		if (nHitTest == HTVSCROLL)
		{
			si.nPos = m_scrollPosV;
			SetScrollInfo(SB_VERT, &si, TRUE);
		}
		else
		{
			si.nPos = m_scrollPosH;
			SetScrollInfo(SB_HORZ, &si, TRUE);
		}
		return;
	}

	CStatic::OnNcLButtonDown(nHitTest, point);
}

void ScrollableView::OnNcLButtonDblClk(UINT nHitTest, CPoint point)
{
	// CStatic's window class has CS_DBLCLKS, so rapid clicks on the scrollbar
	// alternate between WM_NCLBUTTONDOWN and WM_NCLBUTTONDBLCLK. Handle both
	// identically so every click produces a scroll action.
	OnNcLButtonDown(nHitTest, point);
}
