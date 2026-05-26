#pragma once

#include "ScrollableView.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// StackView
// The purpose of this view is to display a call stack.

class StackView : public ScrollableView
{
	DECLARE_DYNAMIC(StackView)

	static constexpr COLORREF CLR_TEXT = RGB(50, 50, 50);
	static constexpr COLORREF CLR_BRACKET = RGB(0, 95, 184);
	static constexpr COLORREF CLR_SELECTED_TEXT = RGB(255, 255, 255);

public:
	StackView();
	~StackView();
	virtual void DrawCallStack(int x, int y, CallStackInfo* m_pCallStackInfo, bool bClear);
	void Clear() override;
	void Draw() override;
	void SetFont(const CString& fontName, int height = DEFAULT_FONT_SIZE) override;

protected:
	virtual void DrawColoredText(int x, int y, int rowHeight, const CString& text, CFont& fontBold, CFont& fontNormal, bool selected = false);

protected:
	CFont m_fontBold;
	int m_fontHeight{};
	int m_xOffset{};
	int m_yOffset{};
	int m_idxSelected{ -1 };
	CallStackInfo* m_pCallStackInfo{};
};
