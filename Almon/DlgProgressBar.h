#pragma once

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DlgProgressBar
// A dialog containing a progress bar.

class DlgProgressBar : public CDialogEx
{
	DECLARE_DYNAMIC(DlgProgressBar)

public:
	DlgProgressBar(CWnd* pParent = nullptr);   // standard constructor
	virtual ~DlgProgressBar();

	void SetRange(int max) { SetMarqueeMode(max == 0); m_ProgressCtrl.SetRange(0, max); }
	void SetPos(int pos) { m_ProgressCtrl.SetPos(pos); }
	void SetTitle(const CString& title);
	void SetLabel(const CString& label);
	void SetText(const CString& text);
	BOOL IsCancelled() { return m_bCancelled; }
	void Increment();
	void SetMarqueeMode(bool bEnable);
	void EnableCancel(bool bAllowCancel);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_PROGRESS_BAR };
#endif

protected:
	virtual BOOL OnInitDialog();

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
	CProgressCtrl m_ProgressCtrl;

protected:
	bool m_bCancelled{};
public:
	afx_msg void OnBnClickedCancel();
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ProgressBar
// Wrapper for DlgProgressBar.

class ProgressBar
{
public:
	ProgressBar() = default;
	ProgressBar(CWnd* pParent, const CString& sLabel, const CString& sTitle, int nMax = 0);

	void Create(CWnd* pParent);
	void Show(bool bAllowCancel = true);
	void Hide();
	void SetRange(int nMax) { m_max = nMax; m_dlg.SetRange(nMax); } // A zero value sets marquee mode.
	void SetPos(int nPos);
	void SetTitle(const CString& sTitle) { m_dlg.SetTitle(sTitle); }
	void SetLabel(const CString& sLabel) { m_dlg.SetLabel(sLabel); }
	void SetText(const CString& sText) { m_dlg.SetText(sText); }
	void SetProgressStep(int nProgressStep) { m_progressStep = nProgressStep; } // The progress will change in steps (not continuously)
	void Increment() { m_dlg.Increment(); }
	BOOL IsCancelled() { return m_dlg.IsCancelled(); }
	bool Update(int nPos); // Updates the progress and returns false if the user cancelled.	
	void RunWithMarquee(std::function<void(void)> func); // Executes 'fun' in the background while showing a marquee progress bar.

protected:
	DlgProgressBar m_dlg;
	int m_max{};
	int m_progressStep{};
	ULONGLONG m_timeLastUpdated{};
	const ULONGLONG m_updatePeriod{ 200 }; //ms
};