#pragma once

#include "CommonDefs.h"
#include "DlgUtils.h"
#include "DlgFilter.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DlgListView
// Shows the call stacks in a list view.

class DlgListView :
	public CDialogEx,
	public DlgUtils,
	public ExporterBase
{
	DECLARE_DYNAMIC(DlgListView)

public:
	DlgListView(CWnd* pParent = nullptr);   // standard constructor
	virtual ~DlgListView();

	enum class Column { Allocs, Bytes, Usage };

	bool SetInfo(CallStackMap* pCallstackMap, ProgressBar* pProgressBar);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_LISTVIEW };
#endif

protected:
	virtual BOOL OnInitDialog();

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnLvnItemchangedCallstackList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnLvnItemchangedAllocList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnColumnClick(NMHDR* pNMHDR, LRESULT* pResult);

	DECLARE_MESSAGE_MAP()

	void SetColumns();
	bool UpdateList(ProgressBar* pProgressBar = nullptr);
	void SelectCallStack(int nItem);

	bool IsValueNotInRange(UINT64 value, UINT64 min, UINT64 max)
	{
		if (min == ULLONG_MAX && max == ULLONG_MAX)
			return false;
		return (max == ULLONG_MAX) ? value != min : (value < min || value > max);
	}

	CString GetTextValue(int row, Column column);

	bool ExportTXT(std::ofstream& file, ProgressBar* pProgressBar) override;
	bool ExportXML(std::ofstream& file, ProgressBar* pProgressBar) override;
	bool IsFormatSupported(ExporterBase::Format fmt) override;
	int  GetNumExportItems() override;

protected:
	CListCtrl m_listCtrl;
	CallStackMap* m_pCallstackMap;
	DlgFilter m_filterDlg;

public:
	afx_msg void OnBnClickedButtonApplyFilter();
	afx_msg void OnBnClickedButtonExport();
};


