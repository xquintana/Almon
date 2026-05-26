#pragma once

#include "CommonDefs.h"
#include "DlgUtils.h"
#include "DlgFilter.h"
#include "ListView.h"
#include "TextView.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DlgListView
// Shows the call stack information in a list view.

class DlgListView :
	public CDialogEx,
	public DlgUtils,
	public ExporterBase
{
	DECLARE_DYNAMIC(DlgListView)

public:
	DlgListView(CWnd* pParent = nullptr);

	enum class Column { Allocs, Bytes, Usage };

	bool SetInfo(CallStackMap* pCallStackMap, ProgressBar* pProgressBar);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_LISTVIEW };
#endif

protected:
	void SelectCallStack(int callStackIdx);
	bool UpdateList(ProgressBar* pProgressBar = nullptr);
	bool IsValueNotInRange(UINT64 value, UINT64 min, UINT64 max);
	CString GetCellText(int row, Column column);
	bool ExportTXT(std::ofstream& file, ProgressBar* pProgressBar) override;
	bool ExportXML(std::ofstream& file, ProgressBar* pProgressBar) override;
	bool IsFormatSupported(ExporterBase::Format fmt) override;
	int  GetNumExportItems() override;

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	virtual BOOL OnInitDialog();
	virtual void OnOK() override {}

	DECLARE_MESSAGE_MAP()
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnBnClickedButtonShowFilter();
	afx_msg void OnBnClickedButtonExport();
	afx_msg LRESULT OnListViewRowClicked(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnListViewHeaderClicked(WPARAM wParam, LPARAM lParam);

protected:
	ListView m_listView;
	TextView m_textView;
	CallStackMap* m_pCallStackMap{};
	DlgFilter m_filterDlg;
	DlgListView::Column m_sortColumn{ DlgListView::Column::Bytes };
};


