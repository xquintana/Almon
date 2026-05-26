#pragma once

#include "Monitor.h"
#include "DlgUtils.h"
#include "DlgMetrics.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DlgRealTime
// Visualizes allocation performance trends and ranked call stack metrics.

class DlgRealTime :
	public CDialogEx,
	public DlgUtils
{
	DECLARE_DYNAMIC(DlgRealTime)

public:
	DlgRealTime(CWnd* pParent = nullptr);

	void Set(Monitor* pMonitor) { m_pMonitor = pMonitor; }
	void Start();
	void Stop();
	void Clear();

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_REALTIME };
#endif

protected:
	void Attach(int idAllocTimeView, int idTopStacksView, CWnd* pParent);
	void UpdateDataThread();
	void UpdateData(AllocInfo& allocInfo); // Updates the data of the views
	void UpdateViews(); // Refreshes the views
	void SetSortingMode(TopStacksManager::SortingType bShowTopBytes) { m_topStacksView.SetSortingMode(bShowTopBytes); }

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
	afx_msg void OnTimer(UINT_PTR);
	afx_msg void OnBnClickedShowMetrics();
	afx_msg void OnCbnSelchangeComboOrderTop();
	afx_msg void OnBnClickedCheckPinFirst();
	afx_msg void OnSize(UINT nType, int cx, int cy);

protected:
	UINT_PTR m_timerId{};
	CButton m_chkPinFirst;
	DlgMetrics m_dlgMetrics;
	CComboBox m_cbTopSorting;
	Thread<DlgRealTime> m_threadUpdateData;
	std::atomic<bool> m_bExitThreadUpdateData{};

	Monitor* m_pMonitor{};
	AllocTimeView m_allocTimeView;
	TopStacksView m_topStacksView;
};
