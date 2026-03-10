#pragma once
#include "CommonDefs.h"
#include "DlgUtils.h"
#include "DlgFilter.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Node
// Element of the call stack tree.

struct Node
{
	void Insert(const CallStack& stack, size_t sizeLeaf, DWORD level, CString** pFrames);
	bool PrintTXT(std::ofstream& file, ProgressBar* pProgress);
	void PrintTXT(std::vector<CString>& items, int indent, ProgressBar* pProgress);
	bool PrintXML(std::ofstream& file, ProgressBar* pProgress);
	void PrintXML(std::vector<CString>& items, int indent, ProgressBar* pProgress);
	void Draw(CTreeCtrl& control, int indent, HTREEITEM hParent);
	void Clear();

	size_t size{}; // Size of this frame and all the children.	
	std::map<UINT_PTR, Node> nodes{};
	CString* pFrame{};
	static constexpr int auxStrMaxLen{ 2048 };
	inline static char auxStr[auxStrMaxLen]{};
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DlgTreeView
// Shows the call stacks in a list view.

class DlgTreeView : public CDialogEx,
	public DlgUtils,
	public ExporterBase
{
	DECLARE_DYNAMIC(DlgTreeView)

public:
	DlgTreeView(CWnd* pParent = nullptr);	

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_TREEVIEW };
#endif

	void SetInfo(CallStackMap* pCallstackMap, ProgressBar* pProgressBar);

protected:
	virtual BOOL OnInitDialog();

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	afx_msg void OnSize(UINT nType, int cx, int cy);

	DECLARE_MESSAGE_MAP()

	void UpdateTree(ProgressBar* pProgressBar = nullptr);

	bool ExportTXT(std::ofstream& file, ProgressBar* pProgressBar) override;
	bool ExportXML(std::ofstream& file, ProgressBar* pProgressBar) override;
	bool IsFormatSupported(ExporterBase::Format fmt) override;
	int GetNumExportItems() override;

protected:
	Node m_tree;
	CTreeCtrl m_treeCtrl;
	CallStackMap* m_pCallstackMap;
	DlgFilter m_filterDlg;

public:
	afx_msg void OnBnClickedButtonShowFilter();
	afx_msg void OnBnClickedButtonExport();
};
