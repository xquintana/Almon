#include "stdafx.h"
#include "StackView.h"

IMPLEMENT_DYNAMIC(StackView, BaseView)

StackView::StackView()
{
	SetFont("Calibri");
}

StackView::~StackView()
{
	m_fontBold.DeleteObject();
}

void StackView::SetFont(const CString& fontName, int height)
{
	BaseView::SetFont(fontName, height);

	m_fontBold.DeleteObject();
	m_fontBold.CreateFont(height, 0, 0, 0, FW_BOLD,
		FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		DEFAULT_PITCH | FF_SWISS, fontName);
}

void StackView::Clear()
{
	m_xOffset = 0;
	m_yOffset = 0;
	m_idxSelected = -1;
	m_pCallStackInfo = nullptr;
	ScrollableView::Clear();
}

void StackView::Draw()
{
	if (m_dcMem.GetSafeHdc() == nullptr)
	{
		CClientDC dc(this);
		GetClientRect(&m_rect);
		CreateBackBuffer(dc, m_rect);
	}

	if (m_fontHeight == 0)
		m_fontHeight = GetFontHeight(&m_font);

	if (!m_pCallStackInfo)
		return;

	CFont* pOldFont = m_dcMem.SelectObject(&m_fontBold);
	COLORREF oldTextColor = m_dcMem.SetTextColor(CLR_TEXT);

	int y{ m_yOffset };

	for (int i = 0; i < m_pCallStackInfo->numFrames; i++)
	{
		if (y > m_rect.bottom - m_fontHeight)
			break;
		DrawColoredText(m_xOffset, y, m_fontHeight, *(m_pCallStackInfo->frames[i]), m_fontBold, m_font, m_idxSelected == i);
		y += m_fontHeight;
	}

	m_dcMem.SetTextColor(oldTextColor);
	m_dcMem.SelectObject(pOldFont);
}

void StackView::DrawCallStack(int x, int y, CallStackInfo* pCallStackInfo, bool bClear)
{
	m_xOffset = x;
	m_yOffset = y;
	m_pCallStackInfo = pCallStackInfo;
	m_idxSelected = -1;
	if (bClear)
		Clear();
	StackView::Draw();
}

void StackView::DrawColoredText(int x, int y, int rowHeight, const CString& text, CFont& fontBold, CFont& fontNormal, bool selected)
{
	if (selected)
	{
		CRect rcSel(x, y, m_rect.right - 1, y + rowHeight);
		m_dcMem.FillSolidRect(&rcSel, CLR_SELECTED_BG);
	}

	const char* str = text.GetString();
	int len = text.GetLength();
	int rightEdge = m_rect.right - 4;

	// Find '----' separator. Text before it is bold; text after it is normal weight.
	int dashPos = len;
	const char* dashPtr = strstr(str, "----");
	if (dashPtr)
		dashPos = (int)(dashPtr - str);

	// Find the trailing '[...]' where ']' is the last character, by scanning backward.
	int bracketPos = -1;
	if (len > 1 && str[len - 1] == ']')
	{
		int j = len - 2;
		while (j >= 0 && str[j] != '[')
			j--;
		if (j >= 0)
			bracketPos = j;
	}

	int oldBkMode = m_dcMem.SetBkMode(TRANSPARENT);

	int i = 0;
	while (i < len && x < rightEdge)
	{
		CFont* pFont;
		COLORREF color;
		int segEnd;

		if (i == bracketPos)
		{
			segEnd = len;
			pFont = &fontBold;
			color = selected ? CLR_SELECTED_TEXT : CLR_BRACKET;
		}
		else if (i < dashPos)
		{
			segEnd = dashPos;
			if (bracketPos > i && bracketPos < segEnd)
				segEnd = bracketPos;
			pFont = &fontBold;
			color = selected ? CLR_SELECTED_TEXT : CLR_TEXT;
		}
		else
		{
			segEnd = len;
			if (bracketPos > i)
				segEnd = bracketPos;
			pFont = &fontNormal;
			color = selected ? CLR_SELECTED_TEXT : CLR_TEXT;
		}

		if (segEnd > i)
		{
			CString segment(str + i, segEnd - i);
			m_dcMem.SelectObject(pFont);
			m_dcMem.SetTextColor(color);
			CSize sz = m_dcMem.GetTextExtent(segment);
			m_dcMem.TextOut(x, y + (rowHeight - sz.cy) / 2, segment);
			x += sz.cx;
		}
		i = segEnd;
	}

	m_dcMem.SetBkMode(oldBkMode);
}
