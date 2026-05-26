#include "stdafx.h"
#include "TextView.h"
#include "Utils.h"

IMPLEMENT_DYNAMIC(TextView, StackView)

BEGIN_MESSAGE_MAP(TextView, StackView)
	ON_WM_LBUTTONDOWN()
	ON_WM_KEYDOWN()
	ON_WM_RBUTTONDOWN()
END_MESSAGE_MAP()

BOOL TextView::PreTranslateMessage(MSG* pMsg)
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
			if (m_idxSelected >= 0 && m_pCallStackInfo && m_idxSelected < m_pCallStackInfo->numFrames)
				CopyTextToClipboard(this, *(m_pCallStackInfo->frames[m_idxSelected]));
			return TRUE;
		}
	}
	return StackView::PreTranslateMessage(pMsg);
}

void TextView::DrawCallStack(int x, int y, CallStackInfo* pCallStackInfo, bool bClear)
{
	if (bClear)
		Clear();
	m_xOffset = x;
	m_yOffset = y;
	m_pCallStackInfo = pCallStackInfo;
	m_idxSelected = -1;
	Draw();
	UpdateScrollBars();
	Invalidate();
}

void TextView::Clear()
{
	StackView::Clear();
	if (m_dcMem.GetSafeHdc() != nullptr)
		DrawBackground();
	Invalidate();
}

void TextView::Draw()
{
	if (m_dcMem.GetSafeHdc() == nullptr)
	{
		CClientDC dc(this);
		GetClientRect(&m_rect);
		CreateBackBuffer(dc, m_rect);
	}

	if (m_fontHeight == 0)
		m_fontHeight = GetFontHeight(&m_font);

	DrawBackground();

	if (!m_pCallStackInfo)
		return;

	CFont* pOldFont = m_dcMem.SelectObject(&m_fontBold);
	COLORREF oldTextColor = m_dcMem.SetTextColor(CLR_TEXT);

	int y = m_yOffset;
	int xBase = m_xOffset - m_scrollPosH;

	for (int i = m_scrollPosV; i < m_pCallStackInfo->numFrames; i++)
	{
		if (y > m_rect.bottom - m_fontHeight)
			break;
		DrawColoredText(xBase, y, m_fontHeight, *(m_pCallStackInfo->frames[i]), m_fontBold, m_font, m_idxSelected == i);
		y += m_fontHeight;
	}

	m_dcMem.SetTextColor(oldTextColor);
	m_dcMem.SelectObject(pOldFont);
}

int TextView::GetMaxVisibleItems()
{
	if (m_fontHeight <= 0)
		return 0;
	int availableHeight = m_rect.bottom - m_yOffset;
	if (availableHeight <= 0)
		return 0;
	return availableHeight / m_fontHeight;
}

int TextView::GetNumItems()
{
	return m_pCallStackInfo ? m_pCallStackInfo->numFrames : 0;
}

int TextView::GetMaxItemWidth()
{
	if (!m_pCallStackInfo || m_dcMem.GetSafeHdc() == nullptr)
		return 0;

	CFont* pOldFont = m_dcMem.SelectObject(&m_fontBold);
	int maxWidth = 0;

	for (int i = 0; i < m_pCallStackInfo->numFrames; i++)
	{
		CSize sz = m_dcMem.GetTextExtent(*(m_pCallStackInfo->frames[i]));
		if (sz.cx > maxWidth)
			maxWidth = sz.cx;
	}

	m_dcMem.SelectObject(pOldFont);
	return maxWidth + m_xOffset;
}

void TextView::OnLButtonDown(UINT nFlags, CPoint point)
{
	SetFocus();

	if (m_pCallStackInfo && m_fontHeight > 0)
	{
		int clickY = point.y - m_yOffset;
		if (clickY >= 0)
		{
			int idx = m_scrollPosV + clickY / m_fontHeight;
			if (idx < m_pCallStackInfo->numFrames)
			{
				m_idxSelected = idx;
				Draw();
				Invalidate();
			}
		}
	}
}

void TextView::OnRButtonDown(UINT nFlags, CPoint point)
{
	if (m_pCallStackInfo && m_fontHeight > 0)
	{
		int clickY = point.y - m_yOffset;
		if (clickY >= 0)
		{
			int idx = m_scrollPosV + clickY / m_fontHeight;
			if (idx < m_pCallStackInfo->numFrames)
			{
				m_idxSelected = idx;
				Draw();
				Invalidate();
			}
			else
				return;
		}
	}

	CMenu menu;
	menu.CreatePopupMenu();

	BOOL bHasSelection = (m_idxSelected >= 0 && m_pCallStackInfo && m_idxSelected < m_pCallStackInfo->numFrames);

	if (!bHasSelection)
		return;

	menu.AppendMenu(MF_STRING, 1, "Copy Row");
	menu.AppendMenu(MF_STRING, 2, "Copy All");

	ClientToScreen(&point);
	int cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN, point.x, point.y, this);

	if (cmd == 1)
	{
		CopyTextToClipboard(this, *(m_pCallStackInfo->frames[m_idxSelected]));
	}
	else if (cmd == 2)
	{
		CString text;
		for (int i = 0; i < m_pCallStackInfo->numFrames; i++)
		{
			if (i > 0) text += "\r\n";
			text += *(m_pCallStackInfo->frames[i]);
		}
		CopyTextToClipboard(this, text);
	}
}

void TextView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (m_pCallStackInfo && m_pCallStackInfo->numFrames > 0)
	{
		int newIndex = m_idxSelected;

		if (nChar == VK_UP && newIndex > 0)
			newIndex--;
		else if (nChar == VK_DOWN && newIndex < m_pCallStackInfo->numFrames - 1)
			newIndex++;

		if (newIndex != m_idxSelected)
		{
			m_idxSelected = newIndex;

			// Scroll to keep selection visible.
			int maxVisible = GetMaxVisibleItems();
			if (m_idxSelected < m_scrollPosV)
				m_scrollPosV = m_idxSelected;
			else if (m_idxSelected >= m_scrollPosV + maxVisible)
				m_scrollPosV = m_idxSelected - maxVisible + 1;

			UpdateScrollBars();
			Draw();
			Invalidate();
		}
	}
}
