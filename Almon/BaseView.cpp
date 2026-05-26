#include "stdafx.h"
#include "resource.h"
#include "BaseView.h"
#include "CommonDefs.h"
#include "Utils.h"
#include <assert.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BaseView

IMPLEMENT_DYNAMIC(BaseView, CStatic)

BEGIN_MESSAGE_MAP(BaseView, CStatic)
	ON_WM_PAINT()
	ON_WM_SIZE()
END_MESSAGE_MAP()

BaseView::BaseView() : m_updateThreadName("BaseView Updater")
{
	InitializeSRWLock(&m_lockBackBuffer);
	m_penBorders.CreatePen(PS_SOLID, 1, GetSysColor(COLOR_ACTIVEBORDER));
}

BaseView::~BaseView()
{
	StopUpdateThread();
	m_bmpDC.DeleteObject();
	m_dcMem.DeleteDC();
	DeleteObject(m_penBorders);
	m_font.DeleteObject();
}

void BaseView::SetFont(const CString& fontName, int height)
{
	m_font.DeleteObject();
	m_font.CreateFont(height, 0, 0, 0, FW_NORMAL,
		FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		DEFAULT_PITCH | FF_SWISS, fontName);
}

void BaseView::StartUpdateThread()
{
	m_bExitThread = false;
	m_UpdateThread.Start(&BaseView::UpdateThread, this, m_updateThreadName);
}

void BaseView::StopUpdateThread()
{
	m_bExitThread = true;
	m_UpdateThread.Stop();
}

void BaseView::Attach(int ctrlId, CWnd* pParent)
{
	SubclassDlgItem(ctrlId, pParent);
	ModifyStyle(0, SS_NOTIFY);
	Render();
}

void BaseView::Render()
{
	Invalidate();
}

void BaseView::CreateBackBuffer(CDC& dc, const CRect& rcClient)
{
	m_dcMem.CreateCompatibleDC(&dc);
	m_bmpDC.CreateCompatibleBitmap(&dc, rcClient.Width(), rcClient.Height());
	m_dcMem.SelectObject(&m_bmpDC);
	DrawBackground();
}

void BaseView::DeleteBackBuffer()
{
	m_bmpDC.DeleteObject();
	m_dcMem.DeleteDC();
}

int BaseView::GetFontHeight(CFont* pFont)
{
	if (m_dcMem.GetSafeHdc() == nullptr)
		return 14;

	CFont* pOldFont = m_dcMem.SelectObject(pFont);
	TEXTMETRIC tm{};
	m_dcMem.GetTextMetrics(&tm);
	m_dcMem.SelectObject(pOldFont);

	return tm.tmHeight + tm.tmExternalLeading;
}

void BaseView::DrawBackground()
{
	CPen* pOldPen = m_dcMem.SelectObject(&m_penBorders);
	m_dcMem.Rectangle(&m_rect);
	m_dcMem.SelectObject(pOldPen);
}

void BaseView::OnPaint()
{
	CPaintDC dc(this);

	AcquireSRWLockExclusive(&m_lockBackBuffer);
	if (m_dcMem.GetSafeHdc() == nullptr)
	{
		GetClientRect(&m_rect);
		CreateBackBuffer(dc, m_rect);
		DrawBackground();
	}
	dc.BitBlt(0, 0, m_rect.Width(), m_rect.Height(), &m_dcMem, 0, 0, SRCCOPY);
	ReleaseSRWLockExclusive(&m_lockBackBuffer);
}

void BaseView::OnSize(UINT nType, int cx, int cy)
{
	CRect rcClient;
	GetClientRect(&rcClient);

	AcquireSRWLockExclusive(&m_lockBackBuffer);
	if (m_dcMem.GetSafeHdc() != nullptr && rcClient != m_rect)
	{
		CClientDC dc(this);
		m_rect = rcClient;
		DeleteBackBuffer();
		CreateBackBuffer(dc, m_rect);
		Draw();
	}
	ReleaseSRWLockExclusive(&m_lockBackBuffer);
	Invalidate();
}

void BaseView::PreSubclassWindow()
{
	CStatic::PreSubclassWindow();
	ModifyStyle(0, BS_OWNERDRAW);
}
