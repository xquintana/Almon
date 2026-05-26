#include "stdafx.h"
#include "Almon.h"
#include "DlgRealTime.h"
#include "Utils.h"


IMPLEMENT_DYNAMIC(DlgRealTime, CDialogEx)

BEGIN_MESSAGE_MAP(DlgRealTime, CDialogEx)
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_BUTTON_SHOW_METRICS, &DlgRealTime::OnBnClickedShowMetrics)
	ON_CBN_SELCHANGE(IDC_COMBO_ORDER_TOP, &DlgRealTime::OnCbnSelchangeComboOrderTop)
	ON_BN_CLICKED(IDC_CHECK_PIN_FIRST, &DlgRealTime::OnBnClickedCheckPinFirst)
	ON_WM_SIZE()
END_MESSAGE_MAP()

DlgRealTime::DlgRealTime(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_REALTIME, pParent), DlgUtils(this)
{
	m_topStacksView.SetMaxSize(10);
}

void DlgRealTime::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_COMBO_ORDER_TOP, m_cbTopSorting);
	DDX_Control(pDX, IDC_CHECK_PIN_FIRST, m_chkPinFirst);
}

BOOL DlgRealTime::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	m_cbTopSorting.InsertString(0, "Size of allocations");
	m_cbTopSorting.InsertString(1, "Number of allocations");
	m_cbTopSorting.InsertString(2, "Memory in use");
	m_cbTopSorting.SetCurSel(0);
	m_chkPinFirst.SetCheck(BST_CHECKED);
	m_topStacksView.SetFont(GetDialogFontName());
	Attach(IDC_ALLOCTIME_VIEW, IDC_TOPSTACKS_VIEW, this);
	SetSortingMode(TopStacksManager::SortingType::Size);
	return FALSE;
}

void DlgRealTime::OnTimer(UINT_PTR)
{
	static DWORD lastTime{ 0 };

	UpdateViews();

	if (::IsWindow(m_dlgMetrics.GetSafeHwnd()) && m_dlgMetrics.IsWindowVisible())
	{
		size_t totalAllocs, totalFrees, totalBytes, allocQueue, allocMap, addrQueue, addrMap, callStackMap;
		m_pMonitor->GetAllCurrentMetrics(totalAllocs, totalFrees, totalBytes, allocQueue, allocMap, addrQueue, addrMap, callStackMap);
		m_dlgMetrics.SetCurrentMetrics(totalAllocs, totalFrees, totalBytes, allocQueue, allocMap, addrQueue, addrMap, callStackMap);
	}

	if ((GetTicks() - lastTime) > 500)
	{
		CString messages;
		DWORD numMessages;
		if (m_pMonitor->GetTextMessages(messages, numMessages))
			SetControlText(IDC_EDIT_MSG, messages);
		lastTime = GetTicks();
	}
}

void DlgRealTime::OnBnClickedShowMetrics()
{
	if (::IsWindow(m_dlgMetrics.GetSafeHwnd()) == FALSE)
	{
		if (!m_dlgMetrics.Create(IDD_METRICS, this))
			ShowError("Error creating window");
		else
			m_dlgMetrics.ShowWindow(SW_SHOW);
	}
	else if (!m_dlgMetrics.IsWindowVisible())
	{
		m_dlgMetrics.ShowWindow(SW_SHOW);
	}
}

void DlgRealTime::UpdateViews()
{
	m_allocTimeView.Render();
	m_topStacksView.Render();
}

void DlgRealTime::Attach(int idAllocTimeView, int idTopStacksView, CWnd* pParent)
{
	m_allocTimeView.Attach(idAllocTimeView, pParent);
	m_topStacksView.Attach(idTopStacksView, pParent);
}

void DlgRealTime::Start()
{
	m_timerId = SetTimer(1, 200, nullptr);

	m_allocTimeView.Clear();
	m_topStacksView.Clear();

	m_allocTimeView.StartUpdateThread();
	m_topStacksView.StartUpdateThread();

	if (!m_threadUpdateData.IsStarted())
	{
		m_bExitThreadUpdateData = false;
		m_threadUpdateData.Start(&DlgRealTime::UpdateDataThread, this, "Real Time Views Data Updater");
	}
}

void DlgRealTime::Stop()
{
	KillTimer(m_timerId);
	m_allocTimeView.StopUpdateThread();
	m_topStacksView.StopUpdateThread();

	if (m_threadUpdateData.IsStarted())
	{
		m_bExitThreadUpdateData = true;
		m_threadUpdateData.Stop();
	}
}

void DlgRealTime::Clear()
{
	m_allocTimeView.Clear();
	m_topStacksView.Clear();
	UpdateViews();
	ProcessWinMessages();
	SetControlText(IDC_EDIT_MSG, "");
}

void DlgRealTime::OnCbnSelchangeComboOrderTop()
{
	SetSortingMode(static_cast<TopStacksManager::SortingType>(m_cbTopSorting.GetCurSel()));
	SetControlFocus(IDC_TOPSTACKS_VIEW);
}

void DlgRealTime::OnBnClickedCheckPinFirst()
{
	m_topStacksView.SetPinToFirst(m_chkPinFirst.GetCheck() == BST_CHECKED);
	SetControlFocus(IDC_TOPSTACKS_VIEW);
}

void DlgRealTime::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	if (!::IsWindow(m_chkPinFirst.GetSafeHwnd()))
		return;

	CRect rcClient;
	GetClientRect(&rcClient);

	CRect rcAllocTimeView = GetControlWindowRect(IDC_ALLOCTIME_VIEW);

	rcAllocTimeView.right = rcClient.right - 20;
	MoveControl(IDC_ALLOCTIME_VIEW, rcAllocTimeView);

	CRect rc = MoveControl(IDC_BUTTON_SHOW_METRICS, CALCULATE, CALCULATE, CALCULATE, rcAllocTimeView.right);
	rc = MoveControl(IDC_EDIT_MSG, CALCULATE, rcAllocTimeView.left, rcClient.bottom - 10, rcAllocTimeView.right);
	rc = MoveControl(IDC_STATIC_MESSAGES, CALCULATE, CALCULATE, rc.top - 10, CALCULATE);
	MoveControl(IDC_TOPSTACKS_VIEW, NO_MOVE, NO_MOVE, rc.top - 10, rcClient.right - 20);

	Invalidate(FALSE);
}

void DlgRealTime::UpdateDataThread()
{
	auto& [allocInfoQueue, lckAllocInfoQueue] = m_pMonitor->GetAllocInfoQueue();

	try
	{
		while (!m_bExitThreadUpdateData)
		{
			while (allocInfoQueue.size() > 0 && !m_bExitThreadUpdateData)
			{
				AcquireSRWLockExclusive(&lckAllocInfoQueue);
				AllocInfo allocInfo = allocInfoQueue.front();
				allocInfoQueue.pop();
				ReleaseSRWLockExclusive(&lckAllocInfoQueue);
				UpdateData(allocInfo);
			}
			Sleep(100);
		}
	}
	catch (CException* pEx)
	{
		TraceRel("DlgRealTime::UpdateDataThread-> Exception: %s", GetExceptionMessage(pEx));
	}
}

void DlgRealTime::UpdateData(AllocInfo& allocInfo)
{
	size_t totalAllocs, totalBytes;
	m_pMonitor->GetAllocMetrics(totalAllocs, totalBytes);
	m_allocTimeView.AddAlloc(allocInfo.numBytes, allocInfo.numTotalAllocs, allocInfo.numTotalBytes);
	m_topStacksView.Add(allocInfo.callStack, allocInfo.pCallStackInfo, totalAllocs, totalBytes);
}
