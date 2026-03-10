#include "stdafx.h"
#include "Almon.h"
#include "DlgInternalStats.h"
#include "afxdialogex.h"


IMPLEMENT_DYNAMIC(DlgInternalStats, CDialogEx)

DlgInternalStats::DlgInternalStats(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_INTERNAL_STATS, pParent) {}

void DlgInternalStats::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_NUMALLOCS, m_totalAllocsLabel);
	DDX_Text(pDX, IDC_EDIT_NUMFREES, m_totalFreesLabel);
	DDX_Text(pDX, IDC_EDIT_ALLOCQUEUE, m_allocQueueLabel);
	DDX_Text(pDX, IDC_EDIT_ALLOCMAP, m_allocMapLabel);
	DDX_Text(pDX, IDC_EDIT_ADDRQUEUE, m_addrQueueLabel);
	DDX_Text(pDX, IDC_EDIT_ADDRMAP, m_addrMapLabel);
	DDX_Text(pDX, IDC_EDIT_CALLSTACKMAP, m_callstackMapLabel);
	DDX_Text(pDX, IDC_EDIT_NUMBYTES, m_totalBytesLabel);
}


BEGIN_MESSAGE_MAP(DlgInternalStats, CDialogEx)
END_MESSAGE_MAP()

BOOL DlgInternalStats::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_GETDLGCODE && pMsg->lParam == WM_KEYDOWN && pMsg->wParam == VK_ESCAPE)
		return DLGC_WANTMESSAGE;
	return CWnd::PreTranslateMessage(pMsg);
}

void DlgInternalStats::SetData(UINT64 totalAllocs, UINT64 totalFrees, UINT64 totalBytes, UINT64 allocQueue, UINT64 allocMap, UINT64 addrQueue, UINT64 addrMap, UINT64 callstackMap)
{
	m_totalAllocsLabel.Format("%llu", totalAllocs);
	m_totalFreesLabel.Format("%llu", totalFrees);
	m_totalBytesLabel.Format("%llu", totalBytes);
	m_allocQueueLabel.Format("%llu", allocQueue);
	m_allocMapLabel.Format("%llu", allocMap);
	m_addrQueueLabel.Format("%llu", addrQueue);
	m_addrMapLabel.Format("%llu", addrMap);
	m_callstackMapLabel.Format("%llu", callstackMap);	
	UpdateData(FALSE);
}
