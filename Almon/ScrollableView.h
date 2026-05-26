#pragma once

#include "BaseView.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ScrollableView
// Represents a view with scroll bar support.

class ScrollableView : public BaseView
{
	DECLARE_DYNAMIC(ScrollableView)

protected:
	void SetVerticalScrollEnabled(bool enable);
	void SetHorizontalScrollEnabled(bool enable);
	int GetMaxVertScroll();
	int GetMaxHorizScroll();
	void UpdateScrollBars();

	virtual int  GetNumItems() { return 0; }
	virtual int  GetMaxVisibleItems() { return 0; }
	virtual int  GetMaxItemWidth() { return 0; }
	virtual void EnsureSelectionVisible() {}
	void Clear() override;

	DECLARE_MESSAGE_MAP()
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg LRESULT OnNcHitTest(CPoint point);
	afx_msg void OnNcLButtonDown(UINT nHitTest, CPoint point);
	afx_msg void OnNcLButtonDblClk(UINT nHitTest, CPoint point);

protected:
	int m_scrollPosV{};
	int m_scrollPosH{};
};
