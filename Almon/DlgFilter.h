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

	BOOL PreTranslateMessage(MSG* pMsg);
	
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_FILTER };
#endif

protected:
	virtual BOOL OnInitDialog();

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support	

	DECLARE_MESSAGE_MAP()

	void InitDefaultValues();
	void EnableTreeViewMode();
	UINT64 GetEditValue(int nIdControl); // Returns the value in an CEditBox, or ULLONG_MAX if the value is invalid.	
	RangeU64 GetValueRange(int nIdChkMin, int nIdEditMin, int nIdChkMax, int nIdEditMax);
	StringArray GetFilterFrames(int nCheckID, int nEditID);

public:
	afx_msg void OnBnClickedOk();
	afx_msg void UpdateControls();
	afx_msg void OnComboSelChange();
	afx_msg void OnComboSetFocus();

public:
	void SetTreeViewMode() { m_bTreeView = true; };

public:
	StringArray includeFrames, excludeFrames;
	RangeU64 rangeSize, rangeAllocs, rangeUse;

protected:
	bool m_bTreeView;
};
