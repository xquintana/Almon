#include "stdafx.h"
#include "Utils.h"
#include "DlgUtils.h"
#include "DlgProgressBar.h"
#include <fstream>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ExporterBase

ExporterBase::Format ExporterBase::GetFormatFromFile(const CString& file)
{
	CString ext = GetFileExtension(file);
	if (ext == ".txt")
		return Format::TXT;
	else if (ext == ".xml")
		return Format::XML;
	return Format::Unknown;
}

CString ExporterBase::GetFormatName(Format format)
{
	switch (format)
	{
	case Format::TXT: return "TXT";
	case Format::XML: return "XML";
	}
	return "Unknown";
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DlgUtils

CWnd* DlgUtils::GetControl(int id)
{
	return m_pDlg->GetDlgItem(id);
}

CString DlgUtils::GetControlText(CWnd* pWnd)
{
	CString txt;
	pWnd->GetWindowText(txt);
	return txt;
}

void DlgUtils::SetControlText(int id, const CString& sText)
{
	if (CWnd* pCtrl = GetControl(id); pCtrl)
		pCtrl->SetWindowText(sText.GetString());
}

CRect DlgUtils::GetControlClientRect(int id)
{
	CRect rc;
	GetControl(id)->GetClientRect(&rc);
	return rc;
}

CRect DlgUtils::GetControlWindowRect(int id)
{
	CRect rc;
	GetControl(id)->GetWindowRect(&rc);
	m_pDlg->ScreenToClient(&rc);
	return rc;
}

int DlgUtils::GetControlWidth(int id)
{
	CRect rc;
	GetControl(id)->GetWindowRect(&rc);
	return rc.right - rc.left;
}

void DlgUtils::MoveControl(int id, const CRect& rc)
{
	if (CWnd* pControl = GetControl(id); pControl)
		pControl->MoveWindow(rc);
}

CRect DlgUtils::MoveControl(int id, int top, int left, int bottom, int right)
{
	CRect rc;
	if (CWnd* pControl = GetControl(id); pControl)
	{
		pControl->GetWindowRect(&rc);
		m_pDlg->ScreenToClient(&rc);

		int width = rc.Width();
		int height = rc.Height();

		if (top >= 0) rc.top = top;
		if (left >= 0) rc.left = left;
		if (bottom >= 0) rc.bottom = bottom;
		if (right >= 0) rc.right = right;

		if (top == CALCULATE) rc.top = rc.bottom - height;
		if (left == CALCULATE) rc.left = rc.right - width;
		if (bottom == CALCULATE) rc.bottom = rc.top + height;
		if (right == CALCULATE) rc.right = rc.left + width;

		pControl->MoveWindow(rc);
	}
	return rc;
}

void DlgUtils::ShowControl(int id, bool bShow)
{
	if (CWnd* pCtrl = GetControl(id); pCtrl)
		pCtrl->ShowWindow(bShow);
}

void DlgUtils::EnableControl(int id, bool bEnable)
{
	if (CWnd* pCtrl = GetControl(id); pCtrl)
		pCtrl->EnableWindow(bEnable);
}

void DlgUtils::SetControlFocus(int id)
{
	if (CWnd* pCtrl = GetControl(id); pCtrl)
		pCtrl->SetFocus();
}

bool DlgUtils::IsButtonChecked(int id)
{
	CButton* pButton = GetControl<CButton>(id);
	return (pButton && pButton->GetCheck() == BST_CHECKED);
}

void DlgUtils::SetButtonCheck(int id, bool bChecked)
{
	if (CButton* pButton = GetControl<CButton>(id); pButton)
		pButton->SetCheck(bChecked ? BST_CHECKED : BST_UNCHECKED);
}

void DlgUtils::SetButtonIcon(int id, HICON hIcon)
{
	if (CButton* pButton = GetControl<CButton>(id); pButton && hIcon)
		pButton->SetIcon(hIcon);
}

CString DlgUtils::GetDialogFontName()
{
	CString fontName;
	if (m_pDlg)
	{
		CFont* pFont = m_pDlg->GetFont();
		if (pFont)
		{
			LOGFONT lf{};
			pFont->GetLogFont(&lf);
			fontName = lf.lfFaceName;
		}
	}
	return fontName;
}

CString DlgUtils::SelectExportFile(CWnd* pParent, ExporterBase* pExporter, CString defaultFileName)
{
	using Format = ExporterBase::Format;

	CString filter;

	auto AddExtension = [&](const CString& type, const CString& ext)
		{
			if (!filter.IsEmpty())
				filter.Append("|");
			filter.AppendFormat("%s|*%s", type.GetString(), ext.GetString());
		};

	if (pExporter->IsFormatSupported(Format::XML))
		AddExtension("XML File", ".xml");
	if (pExporter->IsFormatSupported(Format::TXT))
		AddExtension("TXT File", ".txt");

	if (!filter.IsEmpty())
	{
		filter.Append("||");
		CFileDialog dlgFile(FALSE, "", defaultFileName, OFN_OVERWRITEPROMPT, filter, pParent);
		dlgFile.m_ofn.lpstrTitle = "Export to File";
		dlgFile.m_ofn.lpstrInitialDir = GetCurDirectory();
		if (dlgFile.DoModal() == IDOK)
			return dlgFile.GetPathName();
	}
	return "";
}

void DlgUtils::Export(CWnd* pWnd, ExporterBase* pExporter, CString defaultFileName)
{
	using Format = ExporterBase::Format;
	constexpr static int minItemsProgress = 1000; // Minimum number of items required to show a progress bar.
	constexpr static int numSteps = 10; // Number of steps of the progress bar to finish.	

	if (!pWnd || !pExporter)
		return;

	CString fileFullPath = SelectExportFile(pWnd, pExporter, defaultFileName);

	if (!fileFullPath.IsEmpty())
	{
		Format format = pExporter->GetFormatFromFile(fileFullPath);
		if (format == Format::Unknown)
			ShowError("Unsupported extension '%s'", GetFileExtension(fileFullPath));

		std::ofstream file(fileFullPath);
		if (file.is_open())
		{
			bool bOk = false;
			std::unique_ptr<ProgressBar> progressBar;
			int numItems = pExporter->GetNumExportItems();
			if (numItems > minItemsProgress)
			{
				progressBar = std::make_unique<ProgressBar>(pWnd,
					StringFmt("Export to '%s'", pExporter->GetFormatName(format)),
					"Exporting data...",
					numItems);
				progressBar->SetProgressStep(numItems / numSteps);
				progressBar->Show();
			}

			switch (format)
			{
			case Format::TXT:
				bOk = pExporter->ExportTXT(file, progressBar.get());
				break;
			case Format::XML:
				bOk = pExporter->ExportXML(file, progressBar.get());
				break;
			}
			file.close();

			if (bOk)
				ShowInfo("File succesfully exported at:\n\n%s", fileFullPath);
			else
				ShowError("Error exporting file.");
		}
		else
			ShowError("Error creating output file.");
	}
}
