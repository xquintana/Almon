#include "stdafx.h"
#include "resource.h"
#include "Canvas.h"
#include "CommonDefs.h"
#include "Utils.h"
#include <assert.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Canvas

IMPLEMENT_DYNAMIC(Canvas, CStatic)

BEGIN_MESSAGE_MAP(Canvas, CStatic)
	ON_WM_PAINT()
	ON_WM_SIZE()
END_MESSAGE_MAP()

Canvas::Canvas()
{
	InitializeSRWLock(&m_lockDCMem);
	m_penBorders.CreatePen(PS_SOLID, 1, GetSysColor(COLOR_ACTIVEBORDER));
}

Canvas::~Canvas()
{
	Stop();
	m_bmpDC.DeleteObject();
	m_dcMem.DeleteDC();
	DeleteObject(m_penBorders);
}

void Canvas::Start()
{
	m_bExitThread = false;
	m_UpdateThread.Start(&Canvas::UpdateThread, this, "Canvas Update");
}

void Canvas::Stop()
{
	m_bExitThread = true;
	m_UpdateThread.Stop();
}

void Canvas::Attach(int ctrlId, CWnd* pParent)
{
	SubclassDlgItem(ctrlId, pParent);
	ModifyStyle(0, SS_NOTIFY);
	Render();
}

void Canvas::Render()
{
	Invalidate();
}

void Canvas::DrawBackground()
{
	CPen* pOldPen = m_dcMem.SelectObject(&m_penBorders);
	m_dcMem.Rectangle(&m_rect);
	m_dcMem.SelectObject(pOldPen);
}

void Canvas::OnPaint()
{
	CPaintDC dc(this); // device context for painting

	CRect rcClient;
	GetClientRect(&rcClient);

	AcquireSRWLockExclusive(&m_lockDCMem);

	if (m_dcMem.GetSafeHdc() == nullptr)
	{
		m_dcMem.CreateCompatibleDC(&dc);
		m_bmpDC.CreateCompatibleBitmap(&dc, rcClient.Width(), rcClient.Height());
		m_dcMem.SelectObject(&m_bmpDC);
		m_rect = rcClient;
		DrawBackground();
	}

	dc.BitBlt(0, 0, rcClient.Width(), rcClient.Height(), &m_dcMem, 0, 0, SRCCOPY);
	ReleaseSRWLockExclusive(&m_lockDCMem);
}

void Canvas::OnSize(UINT nType, int cx, int cy)
{
	CRect rcClient;
	GetClientRect(&rcClient);

	if (m_dcMem.GetSafeHdc() != nullptr && rcClient != m_rect)
	{
		AcquireSRWLockExclusive(&m_lockDCMem);

		CPaintDC dc(this); // device context for painting		
		m_rect = rcClient;
		m_bmpDC.DeleteObject();
		m_dcMem.DeleteDC();
		m_dcMem.CreateCompatibleDC(&dc);
		m_bmpDC.CreateCompatibleBitmap(&dc, rcClient.Width(), rcClient.Height());
		m_dcMem.SelectObject(&m_bmpDC);

		Draw();

		ReleaseSRWLockExclusive(&m_lockDCMem);

		Invalidate();
	}
}

void Canvas::PreSubclassWindow()
{
	CStatic::PreSubclassWindow();
	ModifyStyle(0, BS_OWNERDRAW);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AllocTimeGraph

IMPLEMENT_DYNAMIC(AllocTimeGraph, Canvas)

AllocTimeGraph::AllocTimeGraph()
{
	InitializeSRWLock(&m_lockValues);

	m_colorAllocs = RGB(0x00, 0x80, 0xFF);
	m_colorSize = RGB(0xFE, 0x9A, 0x2E);

	m_penNumAllocs.CreatePen(PS_SOLID, 1, m_colorAllocs);
	m_penSizeAllocs.CreatePen(PS_SOLID, 1, m_colorSize);

	ZeroMemory(m_listLastAllocsPerSecond, NUM_SAMPLES * sizeof(UINT64));
	ZeroMemory(m_listLastSizePerSecond, NUM_SAMPLES * sizeof(UINT64));

	m_font.CreateFont(12, 0, 0, 0, FW_NORMAL,
		FALSE, FALSE, FALSE, 0, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_ROMAN, "Arial");
}

AllocTimeGraph::~AllocTimeGraph()
{
	DeleteObject(m_penNumAllocs);
	DeleteObject(m_penSizeAllocs);
	m_font.DeleteObject();
}

void AllocTimeGraph::AddAlloc(UINT64 size, size_t totalAllocs, size_t totalBytes)
{
	AcquireSRWLockExclusive(&m_lockValues);
	m_allocsPerSecond++;
	m_sizePerSecond += size;
	m_totalAllocs = totalAllocs;
	m_totalBytes = totalBytes;
	ReleaseSRWLockExclusive(&m_lockValues);
}

void AllocTimeGraph::UpdateThread()
{
	DWORD startTime = GetTicks();
	UINT64 allocsPerSecond = 0, sizePerSecond = 0;
	size_t totalAllocs = 0, totalBytes = 0;

	constexpr int refreshPeriod = 60000 / NUM_SAMPLES; // in milliseconds	
	UINT64 maxVal = 0;

	while (!m_bExitThread)
	{
		if (GetTicks() - startTime >= refreshPeriod)
		{
			AcquireSRWLockExclusive(&m_lockValues);
			allocsPerSecond = m_allocsPerSecond;
			sizePerSecond = m_sizePerSecond;
			m_allocsPerSecond = 0;
			m_sizePerSecond = 0;
			if (allocsPerSecond > maxVal)
				maxVal = allocsPerSecond;
			ReleaseSRWLockExclusive(&m_lockValues);

			m_listNumAllocs.push_back(allocsPerSecond);
			m_listSizeAllocs.push_back(sizePerSecond);

			AcquireSRWLockExclusive(&m_lockDCMem);
			Draw();
			ReleaseSRWLockExclusive(&m_lockDCMem);
			
			startTime = GetTicks();
		}
		else Sleep(100);
	}
}

void AllocTimeGraph::Clear()
{	
	AcquireSRWLockExclusive(&m_lockValues);
	m_allocsPerSecond = 0;
	m_sizePerSecond = 0;
	ReleaseSRWLockExclusive(&m_lockValues);
	m_totalAllocs = 0;
	m_totalBytes = 0;
	m_listNumAllocs.clear();
	m_listSizeAllocs.clear();
}

void AllocTimeGraph::Draw()
{
	if (m_dcMem.GetSafeHdc() == nullptr)
		return;

	DrawBackground();

	int size = (int)m_listNumAllocs.size();
	if (size == 0)
		return;

	int numAllocs = 0, sizeAllocs = 0;
	double maxAllocNum = 1, maxAllocSize = 1;
	assert(size == m_listSizeAllocs.size());

	int graphHeight = (int)(m_rect.Height() * 0.7);
	int yOffset = m_rect.Height() - graphHeight;	

	ZeroMemory(m_listLastAllocsPerSecond, NUM_SAMPLES * sizeof(UINT64));
	ZeroMemory(m_listLastSizePerSecond, NUM_SAMPLES * sizeof(UINT64));

	// Compute max values
	for (int i = 0; i < NUM_SAMPLES; i++)
	{
		int index = size - 1 - i;
		int indexLast = NUM_SAMPLES - 1 - i;
		m_listLastAllocsPerSecond[indexLast] = (index >= 0) ? m_listNumAllocs[index] : 0;
		if (m_listLastAllocsPerSecond[indexLast] > maxAllocNum)
			maxAllocNum = (double)m_listNumAllocs[index];

		m_listLastSizePerSecond[indexLast] = (index >= 0) ? m_listSizeAllocs[index] : 0;
		if (m_listLastSizePerSecond[indexLast] > maxAllocSize)
			maxAllocSize = (double)m_listSizeAllocs[index];
	}
	
	// Draw size allocs
	CPen* pOldPen = m_dcMem.SelectObject(&m_penSizeAllocs);
	for (int i = 0; i < NUM_SAMPLES; i++)
	{
		int index = size - NUM_SAMPLES + i;

		int ySizeAllocs = (int)(((double)m_listLastSizePerSecond[i] * (double)(graphHeight - 4)) / maxAllocSize);
		int x = (int)(((double)(i * m_rect.Width()) / (double)(NUM_SAMPLES - 1)));
		if (index == -1)
			m_dcMem.MoveTo(x, graphHeight - 2 + yOffset);
		else if (index >= 0)
		{
			if (i == 0)
				m_dcMem.MoveTo(x, graphHeight - ySizeAllocs - 2 + yOffset);
			else
				m_dcMem.LineTo(x, graphHeight - ySizeAllocs - 2 + yOffset);
		}	
	}

	// Draw num allocs	
	m_dcMem.SelectObject(&m_penNumAllocs);
	for (int i = 0; i < NUM_SAMPLES; i++)
	{
		int index = size - NUM_SAMPLES + i;
		int yNumAllocs = (int)(((double)m_listLastAllocsPerSecond[i] * (double)(graphHeight - 4)) / maxAllocNum);
		int x = (int)(((double)(i * m_rect.Width()) / (double)(NUM_SAMPLES - 1)));
		if (index == -1)
			m_dcMem.MoveTo(x, graphHeight - 2 + yOffset);
		else if (index >= 0)
		{
			if (i == 0)
				m_dcMem.MoveTo(x, graphHeight - yNumAllocs - 2 + yOffset);
			else
				m_dcMem.LineTo(x, graphHeight - yNumAllocs - 2 + yOffset);
		}
	}
	DrawLegend();
	m_dcMem.SelectObject(pOldPen);
}

void AllocTimeGraph::DrawLegend()
{
	CString label;
	CString units;
	GetUnits(m_totalBytes, units);
	m_dcMem.SetTextColor(RGB(0, 0, 0));
	CPen* pOldPen = m_dcMem.SelectObject(&m_penSizeAllocs);
	CFont* pOldFont = m_dcMem.SelectObject(&m_font);

	int offset = 180; // Separation of both labels

	RECT rcLine;
	RECT rcLabel;
	rcLine.left = 5;
	rcLine.right = 16;
	rcLine.bottom = 10;
	rcLine.top = 7;

	rcLabel.left = 20;
	rcLabel.right = offset;
	rcLabel.bottom = 20;
	rcLabel.top = 3;

	m_dcMem.FillSolidRect(&rcLine, m_colorSize);
	label.Format("Size of allocations (total: %s)\0", units.GetString());

	m_dcMem.SetBkColor(RGB(255, 255, 255));
	m_dcMem.DrawText(label.GetBuffer(), label.GetLength() + 1, &rcLabel, DT_LEFT);

	rcLine.left += offset;
	rcLine.right += offset;
	rcLabel.left += offset;
	rcLabel.right = m_rect.Width();

	m_dcMem.SelectObject(&m_penNumAllocs);
	m_dcMem.FillSolidRect(&rcLine, m_colorAllocs);
	label.Format("Number of allocations (total: %llu)\0", m_totalAllocs);
	m_dcMem.SetBkColor(RGB(255, 255, 255));
	m_dcMem.DrawText(label.GetBuffer(), label.GetLength() + 1, &rcLabel, DT_LEFT);
	m_dcMem.SelectObject(pOldPen);
	m_dcMem.SelectObject(pOldFont);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TopStacksGraph

IMPLEMENT_DYNAMIC(TopStacksGraph, Canvas)

BEGIN_MESSAGE_MAP(TopStacksGraph, Canvas)	
	ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

TopStacksGraph::TopStacksGraph()
{
	m_font.CreateFont(14, 0, 0, 0, FW_NORMAL,
		FALSE, FALSE, FALSE, 0, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Calibri");
	m_fontBold.CreateFont(13, 0, 0, 0, FW_BOLD,
		FALSE, FALSE, FALSE, 0, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Verdana");

	m_hSelectedIcon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_SELECTED_CALLSTACK));
	InitializeSRWLock(&m_lockQueue);
}

TopStacksGraph::~TopStacksGraph()
{
	m_font.DeleteObject();
	m_fontBold.DeleteObject();

	if (m_pAllocsPct)
		delete[] m_pAllocsPct;
	if (m_pBytesPct)
		delete[] m_pBytesPct;
}

void TopStacksGraph::SetMaxSize(int nMaxSize)
{
	m_topCallStacks.SetMaxSize(nMaxSize);
	m_pAllocsPct = new double[nMaxSize];
	m_pBytesPct = new double[nMaxSize];
}

void TopStacksGraph::SetSortingMode(TopStacksManager::SortingType sortingType)
{
	m_sortingType = sortingType;
	AcquireSRWLockExclusive(&m_lockDCMem);
	Draw();
	ReleaseSRWLockExclusive(&m_lockDCMem);
	Invalidate();
}

void TopStacksGraph::Add(const CallStack& stack, CallStackInfo* pInfo, size_t totalAllocs, size_t totalBytes)              
{	
	m_topCallStacks.Add(stack, pInfo);
	m_totalAllocs = totalAllocs;
	m_totalBytes = totalBytes;

	AcquireSRWLockExclusive(&m_lockQueue);
	m_queueCallstacks.emplace_back(stack, pInfo);
	ReleaseSRWLockExclusive(&m_lockQueue);
}

void TopStacksGraph::UpdateThread()
{
	DWORD startTime = GetTicks();

	constexpr int nRefreshPeriod = 60000 / NUM_SAMPLES; // milliseconds		

	while (!m_bExitThread)
	{
		if (!m_queueCallstacks.empty())
		{
			AcquireSRWLockExclusive(&m_lockQueue);
			if (!m_queueCallstacks.empty())
			{
				CALLSTACK_QUEUE_ITEM item = m_queueCallstacks.front();
				m_queueCallstacks.pop_front();
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
			AcquireSRWLockExclusive(&m_lockDCMem);
			Draw();
			ReleaseSRWLockExclusive(&m_lockDCMem);
			startTime = GetTicks();
		}
	}
}

void TopStacksGraph::Draw()
{
	CString sizeLabel, allocLabel, usageLabel;
	std::vector<TopStack> topStacks;
	auto sortingType = m_sortingType; // Use a  local copy since the member variable may change
	POINT pointClicked = m_pointClicked;

	if (m_dcMem.GetSafeHdc() == nullptr)
		return;

	m_topCallStacks.UpdateTopStacks(topStacks, sortingType);

	int numItems = (int)topStacks.size();
	if (numItems == 0)
	{
		DrawBackground();
		return;
	}

	DWORD selItem = m_bPinToFirst ? topStacks[0].color.id : m_topCallStacks.GetSelectedItem();

	int width = m_rect.Width();
	RECT rcPartition{ 0, 0, 0, m_barHeight };

	CPen* pOldPen = m_dcMem.SelectObject(&m_penBorders);
	m_dcMem.Rectangle(&m_rect);

	HBRUSH hOldBrush = (HBRUSH)m_dcMem.SelectStockObject(WHITE_BRUSH);
	SelectObject(m_dcMem.GetSafeHdc(), hOldBrush);
	CFont* pOldFont = m_dcMem.SelectObject(&m_font);	

	int itemWidth = width / numItems;
	for (int i = 0; i < numItems; i++)
	{
		rcPartition.right = (i == numItems - 1) ? width : rcPartition.left + itemWidth;

		GetLabels(topStacks[i].stackInfo, sizeLabel, allocLabel, usageLabel);

		DrawPartition(rcPartition, topStacks[i].color.rgb, sizeLabel, allocLabel, usageLabel);

		if (pointClicked.x >= 0 && PtInRect(&rcPartition, pointClicked))
		{			
			selItem = topStacks[i].color.id;
			m_topCallStacks.SelectItem(selItem);

			if (m_bPinToFirst && i != 0)
			{
				m_bPinToFirst = false;
				::PostMessage(m_chkPinToFirst, BM_SETCHECK, BST_UNCHECKED, 0);
			}
		}

		if (topStacks[i].color.id == selItem)
		{
			m_dcMem.DrawIcon((rcPartition.right - rcPartition.left) / 2 + rcPartition.left - 16, rcPartition.bottom, m_hSelectedIcon);
			DrawCallStack(0, m_barHeight, topStacks[i].stackInfo, sizeLabel, allocLabel);
		}

		rcPartition.left += itemWidth;
	}
	m_dcMem.SelectObject(pOldFont);

	if (m_pointClicked.x >= 0)
		m_pointClicked.x = m_pointClicked.y = -1;

	m_dcMem.SelectObject(pOldPen);
}

void TopStacksGraph::GetLabels(CallStackInfo& callStack, CString& sizeLabel, CString& allocLabel, CString& usageLabel)
{
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

void TopStacksGraph::DrawCallStack(int x, int y, CallStackInfo& callStack, const CString& sizeLabel, const CString& allocLabel)
{
	CString text;
	y += 15;

	CFont* pOldFont = m_dcMem.SelectObject(&m_fontBold);
	text.Format("%s - %s", sizeLabel.GetString(), allocLabel.GetString());
	m_dcMem.TextOut(x + 5, y, text);
	m_dcMem.SelectObject(pOldFont);

	y += 14; //TODO_XQ: measure font
	for (int i = 0; i < callStack.numFrames; i++)
	{
		text.Format("%s", (*(callStack.frames[i])).GetString());
		m_dcMem.TextOut(x + 5, y, text);
		y += 14;
	}
}

void TopStacksGraph::DrawPartition(RECT rcPartition, COLORREF color, const CString& sizeLabel, const CString& allocLabel, const CString& usageLabel)
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

void TopStacksGraph::OnLButtonDown(UINT nFlags, CPoint point)
{	
	if (point.y < m_barHeight && m_pointClicked.x == -1)
	{
		m_pointClicked = point;
		m_topCallStacks.SelectItem(0);
		AcquireSRWLockExclusive(&m_lockDCMem);
		Draw();
		ReleaseSRWLockExclusive(&m_lockDCMem);
		Invalidate();
	}
}

void TopStacksGraph::Attach(int ctrlId, CWnd* pParent)
{
	Canvas::Attach(ctrlId, pParent);
	m_chkPinToFirst = pParent->GetDlgItem(IDC_CHECK_PIN_FIRST)->GetSafeHwnd();
	assert(::IsWindow(m_chkPinToFirst));
}

void TopStacksGraph::Clear()
{	
	AcquireSRWLockExclusive(&m_lockQueue);
	ClearContainer(m_queueCallstacks);
	ReleaseSRWLockExclusive(&m_lockQueue);
	m_topCallStacks.Clear();
	m_totalAllocs = m_totalBytes = 0;
}
