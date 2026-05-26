#pragma once

#include "BaseView.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AllocTimeView
// Displays the evolution of allocation count and size over time.

class AllocTimeView : public BaseView
{
	DECLARE_DYNAMIC(AllocTimeView)

public:
	AllocTimeView();
	~AllocTimeView();

	void AddAlloc(UINT64 size, size_t totalAllocs, size_t totalBytes);
	void UpdateThread() override;
	void Clear() override;

protected:
	void Draw() override;
	void DrawLegend();

protected:
	UINT64 m_allocsPerSecond{};
	UINT64 m_sizePerSecond{};
	std::atomic<UINT64> m_totalAllocs{};
	std::atomic<UINT64> m_totalBytes{};

	SRWLOCK m_lockValues;

	std::vector<UINT64> m_listNumAllocs;
	std::vector<UINT64> m_listSizeAllocs;

	UINT64 m_listLastAllocsPerSecond[NUM_SAMPLES];
	UINT64 m_listLastSizePerSecond[NUM_SAMPLES];

	CPen m_penNumAllocs;
	CPen m_penSizeAllocs;
	COLORREF m_colorAllocs;
	COLORREF m_colorSize;
};
