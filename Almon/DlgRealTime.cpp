#include "stdafx.h"
#include "Almon.h"
#include "DlgRealTime.h"
#include "afxdialogex.h"
#include "Utils.h"


IMPLEMENT_DYNAMIC(DlgRealTime, CDialogEx)

DlgRealTime::DlgRealTime(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_REALTIME, pParent), DlgUtils(this)
{
	m_graphTopStacks.SetMaxSize(10);
}

DlgRealTime::~DlgRealTime()
{
}

void DlgRealTime::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_COMBO_ORDER_TOP, m_cbTopSorting);
	DDX_Control(pDX, IDC_CHECK_PIN_FIRST, m_chkPinFirst);
}

BEGIN_MESSAGE_MAP(DlgRealTime, CDialogEx)
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_BUTTON_SHOW_INTERNAL_STATS, &DlgRealTime::OnBnClickedShowInternalStats)
	ON_CBN_SELCHANGE(IDC_COMBO_ORDER_TOP, &DlgRealTime::OnCbnSelchangeComboOrderTop)
	ON_BN_CLICKED(IDC_CHECK_PIN_FIRST, &DlgRealTime::OnBnClickedCheckPinFirst)
	ON_WM_SIZE()
END_MESSAGE_MAP()


BOOL DlgRealTime::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	m_cbTopSorting.InsertString(0, "Size of allocations");
	m_cbTopSorting.InsertString(1, "Number of allocations");
	m_cbTopSorting.InsertString(2, "Memory in use");
	m_cbTopSorting.SetCurSel(0);
	m_chkPinFirst.SetCheck(BST_CHECKED);
	Attach(IDC_ALLOCTIME_GRAPH, IDC_TOPSTACKS_GRAPH, this);
	SetSortingMode(TopStacksManager::SortingType::Size);
	return FALSE;
}

void DlgRealTime::OnTimer(UINT_PTR)
{
	static DWORD numMessages{ 0 };
	static DWORD lastTime{ 0 };
	CString messages;

	RenderGraphs();

	if (::IsWindow(m_dlgInternalStats.GetSafeHwnd()) && m_dlgInternalStats.IsWindowVisible())
	{
		size_t totalAllocs;
		size_t totalFrees;
		size_t totalBytes;
		size_t allocQueue;
		size_t allocMap;
		size_t addrQueue;
		size_t addrMap;
		size_t callstackMap;
		m_pMonitor->GetData(totalAllocs, totalFrees, totalBytes, allocQueue, allocMap, addrQueue, addrMap, callstackMap);
		m_dlgInternalStats.SetData(totalAllocs, totalFrees, totalBytes, allocQueue, allocMap, addrQueue, addrMap, callstackMap);
	}

	if ((GetTicks() - lastTime) > 500)
	{
		if (m_pMonitor->GetTextMessages(messages, numMessages))
			GetDlgItem(IDC_EDIT_MSG)->SetWindowText(messages);
		lastTime = GetTicks();
	}
}

void DlgRealTime::OnBnClickedShowInternalStats()
{
	if (::IsWindow(m_dlgInternalStats.GetSafeHwnd()) == FALSE)
	{
		if (!m_dlgInternalStats.Create(IDD_INTERNAL_STATS, this))
			ShowError("Error creating window");
		else
			m_dlgInternalStats.ShowWindow(SW_SHOW);
	}
	else if (!m_dlgInternalStats.IsWindowVisible())
	{
		m_dlgInternalStats.ShowWindow(SW_SHOW);
	}
}

void DlgRealTime::RenderGraphs()
{
	m_graphAllocTime.Render();
	m_graphTopStacks.Render();
}

void DlgRealTime::Attach(int idAllocTimeGraph, int idTopStacksGraph, CWnd* pParent)
{
	m_graphAllocTime.Attach(idAllocTimeGraph, pParent);
	m_graphTopStacks.Attach(idTopStacksGraph, pParent);
}

void DlgRealTime::Start()
{
	m_timerId = SetTimer(1, 200, nullptr);

	m_graphAllocTime.Clear();
	m_graphTopStacks.Clear();

	m_graphAllocTime.Start();
	m_graphTopStacks.Start();

	if (!m_threadUpdateGraphs.IsStarted())
	{
		m_bExitUpdateGraphsThread = false;
		m_threadUpdateGraphs.Start(&DlgRealTime::UpdateGraphsThread, this, "Graph Updater");
	}
}

void DlgRealTime::Stop()
{
	KillTimer(m_timerId);
	m_graphAllocTime.Stop();
	m_graphTopStacks.Stop();

	if (m_threadUpdateGraphs.IsStarted())
	{
		m_bExitUpdateGraphsThread = true;
		m_threadUpdateGraphs.Stop();
	}
}

void DlgRealTime::OnCbnSelchangeComboOrderTop()
{
	SetSortingMode(static_cast<TopStacksManager::SortingType>(m_cbTopSorting.GetCurSel()));
	GetDlgItem(IDC_TOPSTACKS_GRAPH)->SetFocus();
}

void DlgRealTime::OnBnClickedCheckPinFirst()
{
	m_graphTopStacks.SetPinToFirst(m_chkPinFirst.GetCheck() == BST_CHECKED);
	GetDlgItem(IDC_TOPSTACKS_GRAPH)->SetFocus();
}

void DlgRealTime::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	if (!::IsWindow(m_chkPinFirst.GetSafeHwnd()))
		return;

	CRect rcClient;
	GetClientRect(&rcClient);

	CRect rcTimeGraph;
	GetDlgItem(IDC_ALLOCTIME_GRAPH)->GetWindowRect(&rcTimeGraph);
	ScreenToClient(&rcTimeGraph);
	rcTimeGraph.right = rcClient.right - 20;
	GetDlgItem(IDC_ALLOCTIME_GRAPH)->MoveWindow(rcTimeGraph);

	CRect rc = MoveControl(IDC_BUTTON_SHOW_INTERNAL_STATS, CALCULATE, CALCULATE, CALCULATE, rcTimeGraph.right);
	rc = MoveControl(IDC_EDIT_MSG, CALCULATE, rcTimeGraph.left, rcClient.bottom - 10, rcTimeGraph.right);
	rc = MoveControl(IDC_STATIC_MESSAGES, CALCULATE, CALCULATE, rc.top - 10, CALCULATE);
	MoveControl(IDC_TOPSTACKS_GRAPH, NO_MOVE, NO_MOVE, rc.top - 10, rcClient.right - 20);

	Invalidate(FALSE);
}

void DlgRealTime::UpdateGraphsThread()
{
	auto& [graphQueue, lckGraphQueue] = m_pMonitor->GetGraphQueue();

	try
	{
		while (!m_bExitUpdateGraphsThread)
		{
			while (graphQueue.size() > 0 && !m_bExitUpdateGraphsThread)
			{
				AcquireSRWLockExclusive(&lckGraphQueue);
				GraphInfo graphInfo = graphQueue.front();
				graphQueue.pop();
				ReleaseSRWLockExclusive(&lckGraphQueue);
				UpdateGraphs(graphInfo);
			}
			Sleep(100);
		}
	}
	catch (CException* pEx)
	{
		TraceRel("DlgRealTime::UpdateGraphsThread-> Exception: %s", GetExceptionMessage(pEx));
	}
}

void DlgRealTime::UpdateGraphs(GraphInfo& graphInfo)
{
	size_t totalAllocs, totalBytes;
	m_pMonitor->GetAllocData(totalAllocs, totalBytes);
	m_graphAllocTime.AddAlloc(graphInfo.numBytes, graphInfo.numTotalAllocs, graphInfo.numTotalBytes);
	m_graphTopStacks.Add(graphInfo.callStack, graphInfo.pCallStackInfo, totalAllocs, totalBytes);
}
