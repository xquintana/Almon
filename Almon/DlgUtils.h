#pragma once

#define NO_MOVE		-1
#define CALCULATE	-2

class ProgressBar;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ExporterBase
// Provides an interface to export to multiple formats. 

class ExporterBase
{
public:
	enum class Format { TXT, XML, MAX };
	virtual bool ExportTXT(std::ofstream& file, ProgressBar* pProgressBar) { return false; };
	virtual bool ExportXML(std::ofstream& file, ProgressBar* pProgressBar) { return false; };
	virtual bool IsFormatSupported(Format fmt) { return false; };
	virtual int  GetNumExportItems() { return 0; };
	static Format GetFormatFromFile(const CString& sFile); // Unknown extensions return 'Format::MAX'	
	static CString GetFormatName(Format format);
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DlgUtils
// Provides helper methods for handling dialog controls.

class DlgUtils
{
public:

	DlgUtils(CWnd* pDlg) : m_pDlg(pDlg) { };

	CWnd* GetControl(int id);

	template<typename T>
	T* GetControl(int id)
	{
		if (CWnd* pWnd = GetControl(id); pWnd)
			return static_cast<T*>(pWnd);
		return nullptr;
	}

	template<typename T>
	T* GetCurrentControl() { return static_cast<T*>(m_pDlg->GetFocus()); }

	CString GetControlText(CWnd* pWnd);
	CString GetControlText(int id) { return GetControlText(GetControl(id)); }
	void SetControlText(int id, const CString& sText);

	CRect GetControlClientRect(int id);
	CRect GetControlWindowRect(int id); // Returns client coordinates.	
	CRect GetControlWindowRect(CWnd* pWnd) { return GetControlWindowRect(pWnd->GetDlgCtrlID()); }
	CRect MoveControl(int id, int top, int left, int bottom, int right); // Moves the control to the new coordinates. Negative values are ignored. Returns the new position.
	int GetControlWidth(int id);	
	void SetControlFocus(int id);
	void ShowControl(int id, bool bShow);
	bool IsButtonChecked(int id); // Returns true if the button 'id' is checked.
	void SetButtonCheck(int id, bool bChecked); // Sets the check state of a button.

	CString SelectExportFile(CWnd* pParent, ExporterBase* pExporter, CString defaultFileName);

	void Export(CWnd* pWnd, ExporterBase* pExporter, CString defaultFileName);

private:
	CWnd* m_pDlg;
};

