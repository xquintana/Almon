#pragma once

#include "Utils.h"
#include <vector>
#include "TopStacks.h"

#define NUM_SAMPLES			200

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Canvas
// Base rendering context for custom graphical elements.

class Canvas : public CStatic
{
	DECLARE_DYNAMIC(Canvas)

public:
	Canvas();
	virtual ~Canvas();

	void Start();
	void Stop();
	void Render();

	virtual void Attach(int nCtrlId, CWnd* pParent);
	virtual void UpdateThread() {};
	virtual void Clear() {};

	void PreSubclassWindow();

protected:
	void DrawBackground();
	virtual void Draw() {};

	DECLARE_MESSAGE_MAP()

public:
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);

protected:
	std::atomic<bool> m_bExitThread{};
	Thread<Canvas> m_UpdateThread;

	CDC m_dcMem;
	CBitmap m_bmpDC;
	SRWLOCK m_lockDCMem;
	CRect m_rect;
	CPen m_penBorders;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AllocTimeGraph
// Draws a graph showing the number and size of allocations in real time.

class AllocTimeGraph : public Canvas
{
	DECLARE_DYNAMIC(AllocTimeGraph)

public:
	AllocTimeGraph();
	~AllocTimeGraph();

	void AddAlloc(UINT64 size, size_t totalAllocs, size_t totalBytes);
	void UpdateThread() override;
	void Clear() override;

protected:
	void Draw() override;
	void DrawLegend();

protected:
	UINT64 m_allocsPerSecond{};
	UINT64 m_sizePerSecond{};
	UINT64 m_totalAllocs{};
	UINT64 m_totalBytes{};

	SRWLOCK m_lockValues;

	std::vector<UINT64> m_listNumAllocs;
	std::vector<UINT64> m_listSizeAllocs;

	UINT64 m_listLastAllocsPerSecond[NUM_SAMPLES];
	UINT64 m_listLastSizePerSecond[NUM_SAMPLES];

	CPen m_penNumAllocs;
	CPen m_penSizeAllocs;
	COLORREF m_colorAllocs;
	COLORREF m_colorSize;
	CFont m_font;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TopStacksGraph
// Draws the N first call stacks sorted by some criterion (allocation number, size, etc)

class TopStacksGraph : public Canvas
{
	DECLARE_DYNAMIC(TopStacksGraph)

	struct CALLSTACK_QUEUE_ITEM
	{
		CALLSTACK_QUEUE_ITEM(const CallStack& stack, CallStackInfo* pInfo) : stack(stack), pInfo(pInfo) {}
		CallStack stack;
		CallStackInfo* pInfo;
	};

public:
	TopStacksGraph();
	~TopStacksGraph();
	void UpdateThread() override;

	void SetMaxSize(int nMaxSize);
	void SetSortingMode(TopStacksManager::SortingType sortingType);
	void SetPinToFirst(bool bPinToFirst) { m_bPinToFirst = bPinToFirst; }
	void Add(const CallStack& stack, CallStackInfo* pInfo, size_t totalAllocs, size_t totalBytes);
	void Attach(int ctrlId, CWnd* pParent) override;
	void Clear() override;

protected:
	void Draw() override;
	void GetLabels(CallStackInfo& callStack, CString& sizeLabel, CString& allocLabel, CString& usageLabel);

	void DrawCallStack(int x, int y, CallStackInfo& callStack, const CString& sizeLabel, const CString& allocLabel);
	void DrawPartition(RECT rcPartition, COLORREF color, const CString& sizeLabel, const CString& allocLabel, const CString& usageLabel);

	DECLARE_MESSAGE_MAP()

	void OnLButtonDown(UINT nFlags, CPoint point);

protected:
	CFont m_font;
	CFont m_fontBold;
	TopStacksManager  m_topCallStacks;

	std::deque<CALLSTACK_QUEUE_ITEM> m_queueCallstacks;
	SRWLOCK m_lockQueue;

	UINT64 m_totalAllocs{};
	UINT64 m_totalBytes{};
	double* m_pAllocsPct{};
	double* m_pBytesPct{};
	int m_barHeight{ 55 };
	TopStacksManager::SortingType m_sortingType{ TopStacksManager::SortingType::Size };

	CString m_partitionText; // The text displayed in a partition of the stack bar.

	POINT m_pointClicked{ -1 , -1 };
	HICON m_hSelectedIcon{};

	bool m_bPinToFirst{ true };
	HWND m_chkPinToFirst{};
};
