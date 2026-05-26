#pragma once


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DlgMetrics
// Dialog that shows application metrics such as counters and the current size of queues, maps, etc.

class DlgMetrics : public CDialogEx
{
	DECLARE_DYNAMIC(DlgMetrics)

public:
	DlgMetrics(CWnd* pParent = nullptr);

	void SetCurrentMetrics(size_t totalAllocs, size_t totalFrees, size_t totalBytes, size_t allocQueue,
		size_t allocMap, size_t addrQueue, size_t addrMap, size_t callStackMap);

	BOOL PreTranslateMessage(MSG* pMsg);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_METRICS };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
	afx_msg void OnEditSetFocus(UINT nID);

public:
	CString m_totalAllocsLabel;
	CString m_totalFreesLabel;
	CString m_allocQueueLabel;
	CString m_allocMapLabel;
	CString m_addrQueueLabel;
	CString m_addrMapLabel;
	CString m_callStackMapLabel;
	CString m_totalBytesLabel;
};
