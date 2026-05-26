#pragma once
#include "CommonDefs.h"
#include "DlgUtils.h"
#include "DlgFilter.h"
#include "TreeView.h"
#include <algorithm>


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DlgTreeView
// Shows the call stack information in a tree view.

class DlgTreeView :
	public CDialogEx,
	public DlgUtils,
	public ExporterBase
{
	DECLARE_DYNAMIC(DlgTreeView)

public:
	DlgTreeView(CWnd* pParent = nullptr);

	void SetInfo(CallStackMap* pCallStackMap, ProgressBar* pProgressBar);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_TREEVIEW };
#endif	

protected:
	void UpdateTree(ProgressBar* pProgressBar = nullptr);

	// Export related methods.
	bool ExportTXT(std::ofstream& file, ProgressBar* pProgressBar) override;
	bool ExportXML(std::ofstream& file, ProgressBar* pProgressBar) override;
	bool IsFormatSupported(ExporterBase::Format fmt) override;
	int GetNumExportItems() override;

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support	

	virtual BOOL OnInitDialog();
	virtual void OnOK() override {}

	DECLARE_MESSAGE_MAP()
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnBnClickedButtonShowFilter();
	afx_msg void OnBnClickedButtonExport();

protected:
	TreeNode m_tree;
	TreeView m_treeView;
	CallStackMap* m_pCallStackMap{};
	DlgFilter m_filterDlg;
};
