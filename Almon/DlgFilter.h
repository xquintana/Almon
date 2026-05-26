#pragma once

#include "CommonDefs.h"
#include "DlgUtils.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DlgFilter
// Dialog used to filter the call stack list.

class DlgFilter :
	public CDialogEx,
	public DlgUtils
{
	DECLARE_DYNAMIC(DlgFilter)

public:
	DlgFilter(CWnd* pParent = nullptr);

	void SetTreeViewMode() { m_bTreeViewMode = true; };

	BOOL PreTranslateMessage(MSG* pMsg);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_FILTER };
#endif

protected:
	void InitDefaultValues();
	void EnableTreeViewMode();
	UINT64 GetEditValue(int nIdControl); // Returns the value in an CEditBox, or ULLONG_MAX if the value is invalid.	
	RangeU64 GetValueRange(int nIdChkMin, int nIdEditMin, int nIdChkMax, int nIdEditMax);
	StringArray GetFilterFrames(int nCheckID, int nEditID);

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
	afx_msg void OnBnClickedOk();
	afx_msg void UpdateControls();
	afx_msg void OnComboSelChange();
	afx_msg void OnComboSetFocus();

protected:
	bool m_bTreeViewMode{}; // If true, the layout shows the controls required for the tree view.
	StringArray includeFrames, excludeFrames;
	RangeU64 rangeSize, rangeNumAllocs, rangeUse;

	friend class DlgListView;
	friend class DlgTreeView;
};
