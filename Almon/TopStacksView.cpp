#include "stdafx.h"
#include "resource.h"
#include "TopStacksView.h"
#include "CommonDefs.h"
#include <assert.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TopStacksView

IMPLEMENT_DYNAMIC(TopStacksView, StackView)

BEGIN_MESSAGE_MAP(TopStacksView, StackView)
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEWHEEL()
END_MESSAGE_MAP()

TopStacksView::TopStacksView()
{
	m_updateThreadName = "TopStacksView Updater";

	m_hSelectedIcon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_SELECTED_CALLSTACK));
	InitializeSRWLock(&m_lockQueue);
	m_fontHeader.CreateFont(DEFAULT_FONT_SIZE, 0, 0, 0, FW_BOLD,
		FALSE, FALSE, FALSE, 0, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Verdana");
}

TopStacksView::~TopStacksView()
{
	m_fontHeader.DeleteObject();
	if (m_pAllocsPct)
		delete[] m_pAllocsPct;
	if (m_pBytesPct)
		delete[] m_pBytesPct;
}

void TopStacksView::SetMaxSize(int nMaxSize)
{
	m_topCallStacks.SetMaxSize(nMaxSize);
	m_pAllocsPct = new double[nMaxSize];
	m_pBytesPct = new double[nMaxSize];
}

void TopStacksView::SetSortingMode(TopStacksManager::SortingType sortingType)
{
	m_sortingType = sortingType;
	AcquireSRWLockExclusive(&m_lockBackBuffer);
	Draw();
	ReleaseSRWLockExclusive(&m_lockBackBuffer);
	Invalidate();
}

void TopStacksView::Add(const CallStack& stack, CallStackInfo* pInfo, size_t totalAllocs, size_t totalBytes)
{
	m_topCallStacks.Add(stack, pInfo);
	m_totalAllocs = totalAllocs;
	m_totalBytes = totalBytes;

	AcquireSRWLockExclusive(&m_lockQueue);
	m_queueCallStacks.emplace_back(stack, pInfo);
	ReleaseSRWLockExclusive(&m_lockQueue);
}

void TopStacksView::UpdateThread()
{
	DWORD startTime = GetTicks();

	constexpr int nRefreshPeriod = 60000 / NUM_SAMPLES; // milliseconds		

	while (!m_bExitThread)
	{
		if (!m_queueCallStacks.empty())
		{
			AcquireSRWLockExclusive(&m_lockQueue);
			if (!m_queueCallStacks.empty())
			{
				CALLSTACK_QUEUE_ITEM item = m_queueCallStacks.front();
				m_queueCallStacks.pop_front();
				ReleaseSRWLockExclusive(&m_lockQueue);
				m_topCallStacks.Add(item.stack, item.pInfo);
			}
			else
				ReleaseSRWLockExclusive(&m_lockQueue);
		}
		else
			Sleep(50);

		if (GetTicks() - startTime >= 100)
		{
			AcquireSRWLockExclusive(&m_lockBackBuffer);
			Draw();
			GdiFlush();
			ReleaseSRWLockExclusive(&m_lockBackBuffer);
			startTime = GetTicks();
		}
	}
}

void TopStacksView::Draw()
{
	CString sizeLabel, allocLabel, usageLabel;
	std::vector<TopStack> topStacks;
	auto sortingType = m_sortingType; // Use a  local copy since the member variable may change
	POINT pointClicked = m_pointClicked;

	if (m_dcMem.GetSafeHdc() == nullptr)
		return;

	if (m_fontHeight == 0)
		m_fontHeight = GetFontHeight(&m_font);

	m_topCallStacks.UpdateTopStacks(topStacks, sortingType);

	int numItems = (int)topStacks.size();
	if (numItems == 0)
	{
		DrawBackground();
		return;
	}

	if (m_bPinToFirst)
		m_selItemId = topStacks[0].GetId();

	int width = m_rect.Width();
	RECT rcPartition{ 0, 0, 0, m_barHeight };

	CPen* pOldPen = m_dcMem.SelectObject(&m_penBorders);
	m_dcMem.Rectangle(&m_rect);
	CFont* pOldFont = m_dcMem.SelectObject(&m_font);

	int itemWidth = width / numItems;
	for (int i = 0; i < numItems; i++)
	{
		rcPartition.right = (i == numItems - 1) ? width : rcPartition.left + itemWidth;

		GetLabels(topStacks[i].stackInfo, sizeLabel, allocLabel, usageLabel);

		DrawPartition(rcPartition, topStacks[i].color.rgb, sizeLabel, allocLabel, usageLabel);

		//if (pointClicked.x >= 0 && PtInRect(&rcPartition, pointClicked))
		if (m_selItemId == -1 && PtInRect(&rcPartition, pointClicked))
		{
			m_selItemId = topStacks[i].GetId();
			m_topCallStacks.SelectItem(m_selItemId);
			m_pointClicked.x = m_pointClicked.y = -1;
		}

		if (m_selItemId == topStacks[i].GetId())
		{
			m_dcMem.DrawIcon((rcPartition.right - rcPartition.left) / 2 + rcPartition.left - 16, rcPartition.bottom, m_hSelectedIcon);

			constexpr int topMargin = 15; // Separation between the partition label and the first line of the call stack.
			constexpr int leftMargin = 5; // Separation from the left border.			

			DrawHeader(leftMargin, m_barHeight + topMargin, sizeLabel, allocLabel);
			DrawCallStack(leftMargin, m_barHeight + topMargin * 2 + m_fontHeight, &(topStacks[i].stackInfo), false);
		}

		rcPartition.left += itemWidth;
	}
	m_dcMem.SelectObject(pOldFont);
	m_dcMem.SelectObject(pOldPen);
}

void TopStacksView::DrawHeader(int x, int y, const CString& sizeLabel, const CString& allocLabel)
{
	CString text;
	CFont* pOldFont = m_dcMem.SelectObject(&m_fontHeader);
	text.Format("%s - %s", sizeLabel.GetString(), allocLabel.GetString());
	m_dcMem.TextOut(x, y, text);
	m_dcMem.SelectObject(pOldFont);
}

void TopStacksView::GetLabels(CallStackInfo& callStack, CString& sizeLabel, CString& allocLabel, CString& usageLabel)
{
	if (m_totalBytes == 0 || m_totalAllocs == 0)
		return;

	CString unitsSizeLabel, unitsUsageLabel;
	size_t numBytes = callStack.numBytes;
	int numAllocs = callStack.numAllocs;
	size_t numBytesInUse = callStack.numBytesInUse;
	double pctBytes = (double)numBytes / (double)m_totalBytes;
	double pctAlloc = (double)numAllocs / (double)m_totalAllocs;

	GetUnits(numBytes, unitsSizeLabel);
	GetUnits(numBytesInUse, unitsUsageLabel);

	sizeLabel.Format("Size: %d%% (%s)", (int)(pctBytes * 100), unitsSizeLabel.GetString());
	allocLabel.Format("Allocs: %d%% (%d)", (int)(pctAlloc * 100), numAllocs);
	usageLabel.Format("Usage: %s", unitsUsageLabel.GetString());
}

void TopStacksView::DrawPartition(RECT rcPartition, COLORREF color, const CString& sizeLabel, const CString& allocLabel, const CString& usageLabel)
{
	assert(rcPartition.left >= 0);
	assert(rcPartition.bottom > 0);

	m_partitionText.Format("%s\n%s\n%s\n ", sizeLabel.GetString(), allocLabel.GetString(), usageLabel.GetString()); // The last new line and blank space are intentional.

	HBRUSH hBrush = CreateSolidBrush(color);
	HBRUSH hOldBrush = (HBRUSH)SelectObject(m_dcMem.GetSafeHdc(), hBrush);

	m_dcMem.Rectangle(&rcPartition);

	COLORREF oldColor = m_dcMem.SetBkColor(color);
	m_dcMem.SetTextColor(RGB(0, 0, 0));

	// Adjust the area for displaying the text.
	rcPartition.left += 5;
	rcPartition.top += 3;
	rcPartition.bottom -= 3;
	m_dcMem.DrawText(m_partitionText.GetBuffer(), m_partitionText.GetLength(), &rcPartition, DT_LEFT);

	SelectObject(m_dcMem.GetSafeHdc(), hOldBrush);
	m_dcMem.SetBkColor(oldColor);
	DeleteObject(hBrush);
}

void TopStacksView::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (point.y < m_barHeight && m_pointClicked.x == -1)
	{
		AcquireSRWLockExclusive(&m_lockBackBuffer);
		if (m_bPinToFirst)
		{
			m_bPinToFirst = false;
			::PostMessage(m_chkPinToFirst, BM_SETCHECK, BST_UNCHECKED, 0);
		}
		m_pointClicked = point;
		m_selItemId = -1;
		Draw();
		ReleaseSRWLockExclusive(&m_lockBackBuffer);
		Invalidate();
	}
}

BOOL TopStacksView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	return BaseView::OnMouseWheel(nFlags, zDelta, pt);
}

void TopStacksView::Attach(int ctrlId, CWnd* pParent)
{
	BaseView::Attach(ctrlId, pParent);
	m_chkPinToFirst = pParent->GetDlgItem(IDC_CHECK_PIN_FIRST)->GetSafeHwnd();
	assert(::IsWindow(m_chkPinToFirst));
}

void TopStacksView::Clear()
{
	AcquireSRWLockExclusive(&m_lockQueue);
	ClearContainer(m_queueCallStacks);
	ReleaseSRWLockExclusive(&m_lockQueue);
	AcquireSRWLockExclusive(&m_lockBackBuffer);
	m_topCallStacks.Clear();
	m_totalAllocs = 0;
	m_totalBytes = 0;
	Draw();
	ReleaseSRWLockExclusive(&m_lockBackBuffer);
}
