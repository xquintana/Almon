#pragma once

#include "Utils.h"
#include <vector>


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BaseView
// Base rendering context for custom graphical elements.
// Provides a default font.

class BaseView : public CStatic
{
	DECLARE_DYNAMIC(BaseView)

	static constexpr COLORREF CLR_SELECTED_BG = RGB(0, 95, 184); // blue

public:
	BaseView();
	virtual ~BaseView();

	void StartUpdateThread();
	void StopUpdateThread();
	void Render();

	virtual void Attach(int nCtrlId, CWnd* pParent);
	virtual void UpdateThread() {};
	virtual void Clear() {};

	virtual void SetFont(const CString& fontName, int height = DEFAULT_FONT_SIZE);

	void PreSubclassWindow();

protected:
	void CreateBackBuffer(CDC& dc, const CRect& rcClient);
	void DeleteBackBuffer();
	int  GetFontHeight(CFont* pFont);

	void DrawBackground();

	virtual void Draw() {};

	DECLARE_MESSAGE_MAP()
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);

protected:
	std::atomic<bool> m_bExitThread{};
	Thread<BaseView> m_UpdateThread;
	CString m_updateThreadName;

	CDC m_dcMem;
	CBitmap m_bmpDC;
	SRWLOCK m_lockBackBuffer;
	CRect m_rect;
	CPen m_penBorders;
	CFont m_font;
};
