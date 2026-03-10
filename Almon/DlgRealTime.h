#pragma once

#include "Monitor.h"
#include "DlgUtils.h"
#include "DlgInternalStats.h"


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
	virtual ~DlgRealTime();

	void Set(Monitor* pMonitor) { m_pMonitor = pMonitor; }
	void Start();
	void Stop();

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_REALTIME };
#endif

protected:
	virtual BOOL OnInitDialog();
	afx_msg void OnTimer(UINT_PTR);
	afx_msg void OnBnClickedShowInternalStats();
	afx_msg void OnCbnSelchangeComboOrderTop();
	afx_msg void OnBnClickedCheckPinFirst();
	afx_msg void OnSize(UINT nType, int cx, int cy);

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	void RenderGraphs();
	void Attach(int nIdAllocTimeGraph, int nIdTopStacksGraph, CWnd* pDlg);

	void UpdateGraphsThread();
	void UpdateGraphs(GraphInfo& graphInfo);

	void SetSortingMode(TopStacksManager::SortingType bShowTopBytes) { m_graphTopStacks.SetSortingMode(bShowTopBytes); }

	DECLARE_MESSAGE_MAP()

protected:
	UINT_PTR m_timerId{};
	CButton m_chkPinFirst;
	DlgInternalStats m_dlgInternalStats;
	CComboBox m_cbTopSorting;
	Monitor* m_pMonitor{};

	AllocTimeGraph m_graphAllocTime;
	TopStacksGraph m_graphTopStacks;

	Thread<DlgRealTime> m_threadUpdateGraphs;
	std::atomic<bool> m_bExitUpdateGraphsThread{};
};
