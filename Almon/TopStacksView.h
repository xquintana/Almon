#pragma once

#include "StackView.h"
#include "TopStacks.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TopStacksView
// Draws the N first call stacks sorted by some criterion (allocation count, size,...)

class TopStacksView : public StackView
{
	DECLARE_DYNAMIC(TopStacksView)

	struct CALLSTACK_QUEUE_ITEM
	{
		CALLSTACK_QUEUE_ITEM(const CallStack& stack, CallStackInfo* pInfo) : stack(stack), pInfo(pInfo) {}
		CallStack stack;
		CallStackInfo* pInfo;
	};

public:
	TopStacksView();
	~TopStacksView();

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

	void DrawPartition(RECT rcPartition, COLORREF color, const CString& sizeLabel, const CString& allocLabel, const CString& usageLabel);
	void DrawHeader(int x, int y, const CString& sizeLabel, const CString& allocLabel);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);

protected:
	CFont m_fontHeader;
	TopStacksManager  m_topCallStacks;

	std::deque<CALLSTACK_QUEUE_ITEM> m_queueCallStacks;
	SRWLOCK m_lockQueue;

	std::atomic<UINT64> m_totalAllocs{};
	std::atomic<UINT64> m_totalBytes{};
	double* m_pAllocsPct{};
	double* m_pBytesPct{};
	int m_barHeight{ 55 };
	TopStacksManager::SortingType m_sortingType{ TopStacksManager::SortingType::Size };

	CString m_partitionText; // The text displayed in a partition of the stack bar.

	POINT m_pointClicked{ -1 , -1 };
	HICON m_hSelectedIcon{};

	bool m_bPinToFirst{ true };
	HWND m_chkPinToFirst{};
	DWORD m_selItemId{ (DWORD)-1 };
};
