#pragma once

#include "StackView.h"
#include "ScrollableView.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TextView
// Displays a call stack with copy and scroll support.

class TextView : public StackView
{
	DECLARE_DYNAMIC(TextView)

public:
	void DrawCallStack(int x, int y, CallStackInfo* pCallStackInfo, bool bClear) override;
	void Clear() override;

	BOOL PreTranslateMessage(MSG* pMsg) override;

protected:
	void Draw() override;

	DECLARE_MESSAGE_MAP()
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);

private:
	int  GetNumItems() override;
	int  GetMaxVisibleItems() override;
	int  GetMaxItemWidth() override;
};
