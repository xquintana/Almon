#include "stdafx.h"
#include "AllocTimeView.h"
#include <assert.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AllocTimeView

IMPLEMENT_DYNAMIC(AllocTimeView, BaseView)

AllocTimeView::AllocTimeView()
{
	InitializeSRWLock(&m_lockValues);

	m_updateThreadName = "AllocTimeView Updater";

	m_colorAllocs = RGB(0x00, 0x80, 0xFF);
	m_colorSize = RGB(0xFE, 0x9A, 0x2E);

	m_penNumAllocs.CreatePen(PS_SOLID, 1, m_colorAllocs);
	m_penSizeAllocs.CreatePen(PS_SOLID, 1, m_colorSize);

	ZeroMemory(m_listLastAllocsPerSecond, NUM_SAMPLES * sizeof(UINT64));
	ZeroMemory(m_listLastSizePerSecond, NUM_SAMPLES * sizeof(UINT64));

	SetFont("Arial", 12);
}

AllocTimeView::~AllocTimeView()
{
	DeleteObject(m_penNumAllocs);
	DeleteObject(m_penSizeAllocs);
}

void AllocTimeView::AddAlloc(UINT64 size, size_t totalAllocs, size_t totalBytes)
{
	AcquireSRWLockExclusive(&m_lockValues);
	m_allocsPerSecond++;
	m_sizePerSecond += size;
	m_totalAllocs = totalAllocs;
	m_totalBytes = totalBytes;
	ReleaseSRWLockExclusive(&m_lockValues);
}

void AllocTimeView::UpdateThread()
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

			AcquireSRWLockExclusive(&m_lockBackBuffer);
			m_listNumAllocs.push_back(allocsPerSecond);
			m_listSizeAllocs.push_back(sizePerSecond);
			Draw();
			GdiFlush();
			ReleaseSRWLockExclusive(&m_lockBackBuffer);

			startTime = GetTicks();
		}
		else Sleep(100);
	}
}

void AllocTimeView::Clear()
{
	AcquireSRWLockExclusive(&m_lockValues);
	m_totalAllocs = 0;
	m_totalBytes = 0;
	m_allocsPerSecond = 0;
	m_sizePerSecond = 0;
	ReleaseSRWLockExclusive(&m_lockValues);
	AcquireSRWLockExclusive(&m_lockBackBuffer);
	m_listNumAllocs.clear();
	m_listSizeAllocs.clear();
	Draw();
	ReleaseSRWLockExclusive(&m_lockBackBuffer);
}

void AllocTimeView::Draw()
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

	int viewHeight = (int)(m_rect.Height() * 0.7);
	int yOffset = m_rect.Height() - viewHeight;

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

		int ySizeAllocs = (int)(((double)m_listLastSizePerSecond[i] * (double)(viewHeight - 4)) / maxAllocSize);
		int x = (int)(((double)(i * m_rect.Width()) / (double)(NUM_SAMPLES - 1)));
		if (index == -1)
			m_dcMem.MoveTo(x, viewHeight - 2 + yOffset);
		else if (index >= 0)
		{
			if (i == 0)
				m_dcMem.MoveTo(x, viewHeight - ySizeAllocs - 2 + yOffset);
			else
				m_dcMem.LineTo(x, viewHeight - ySizeAllocs - 2 + yOffset);
		}
	}

	// Draw num allocs	
	m_dcMem.SelectObject(&m_penNumAllocs);
	for (int i = 0; i < NUM_SAMPLES; i++)
	{
		int index = size - NUM_SAMPLES + i;
		int yNumAllocs = (int)(((double)m_listLastAllocsPerSecond[i] * (double)(viewHeight - 4)) / maxAllocNum);
		int x = (int)(((double)(i * m_rect.Width()) / (double)(NUM_SAMPLES - 1)));
		if (index == -1)
			m_dcMem.MoveTo(x, viewHeight - 2 + yOffset);
		else if (index >= 0)
		{
			if (i == 0)
				m_dcMem.MoveTo(x, viewHeight - yNumAllocs - 2 + yOffset);
			else
				m_dcMem.LineTo(x, viewHeight - yNumAllocs - 2 + yOffset);
		}
	}
	DrawLegend();
	m_dcMem.SelectObject(pOldPen);
}

void AllocTimeView::DrawLegend()
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
	label.Format("Number of allocations (total: %llu)\0", m_totalAllocs.load());
	m_dcMem.SetBkColor(RGB(255, 255, 255));
	m_dcMem.DrawText(label.GetBuffer(), label.GetLength() + 1, &rcLabel, DT_LEFT);
	m_dcMem.SelectObject(pOldPen);
	m_dcMem.SelectObject(pOldFont);
}


