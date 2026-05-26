#include "stdafx.h"
#include "Almon.h"
#include "DlgMetrics.h"


IMPLEMENT_DYNAMIC(DlgMetrics, CDialogEx)

BEGIN_MESSAGE_MAP(DlgMetrics, CDialogEx)
	ON_CONTROL_RANGE(EN_SETFOCUS, IDC_EDIT_NUMALLOCS, IDC_EDIT_NUMBYTES, &DlgMetrics::OnEditSetFocus)
END_MESSAGE_MAP()

DlgMetrics::DlgMetrics(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_METRICS, pParent) {
}

void DlgMetrics::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_NUMALLOCS, m_totalAllocsLabel);
	DDX_Text(pDX, IDC_EDIT_NUMFREES, m_totalFreesLabel);
	DDX_Text(pDX, IDC_EDIT_ALLOCQUEUE, m_allocQueueLabel);
	DDX_Text(pDX, IDC_EDIT_ALLOCMAP, m_allocMapLabel);
	DDX_Text(pDX, IDC_EDIT_ADDRQUEUE, m_addrQueueLabel);
	DDX_Text(pDX, IDC_EDIT_ADDRMAP, m_addrMapLabel);
	DDX_Text(pDX, IDC_EDIT_CALLSTACKMAP, m_callStackMapLabel);
	DDX_Text(pDX, IDC_EDIT_NUMBYTES, m_totalBytesLabel);
}

BOOL DlgMetrics::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_GETDLGCODE && pMsg->lParam == WM_KEYDOWN && pMsg->wParam == VK_ESCAPE)
		return DLGC_WANTMESSAGE;
	return CWnd::PreTranslateMessage(pMsg);
}

void DlgMetrics::OnEditSetFocus(UINT nID)
{
	if (CWnd* pWnd = GetDlgItem(nID))
		pWnd->HideCaret();
}

void DlgMetrics::SetCurrentMetrics(size_t totalAllocs, size_t totalFrees, size_t totalBytes, size_t allocQueue, size_t allocMap, size_t addrQueue, size_t addrMap, size_t callStackMap)
{
	m_totalAllocsLabel.Format("%zu", totalAllocs);
	m_totalFreesLabel.Format("%zu", totalFrees);
	m_totalBytesLabel.Format("%zu", totalBytes);
	m_allocQueueLabel.Format("%zu", allocQueue);
	m_allocMapLabel.Format("%zu", allocMap);
	m_addrQueueLabel.Format("%zu", addrQueue);
	m_addrMapLabel.Format("%zu", addrMap);
	m_callStackMapLabel.Format("%zu", callStackMap);
	UpdateData(FALSE);
}
