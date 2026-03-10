#pragma once


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DlgInternalStats
// Dialog that shows internal statistics such as counters and the current size of queues, maps, etc.

class DlgInternalStats : public CDialogEx
{
	DECLARE_DYNAMIC(DlgInternalStats)

public:
	DlgInternalStats(CWnd* pParent = nullptr);

	void SetData(size_t totalAllocs, size_t totalFrees, size_t totalBytes, size_t allocQueue, 
		size_t allocMap, size_t addrQueue, size_t addrMap, size_t callstackMap);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_INTERNAL_STATS };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

	BOOL PreTranslateMessage(MSG* pMsg);

public:
	CString m_totalAllocsLabel;
	CString m_totalFreesLabel;
	CString m_allocQueueLabel;
	CString m_allocMapLabel;
	CString m_addrQueueLabel;
	CString m_addrMapLabel;
	CString m_callstackMapLabel;
	CString m_totalBytesLabel;	
};
