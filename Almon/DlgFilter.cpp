#include "stdafx.h"
#include "Almon.h"
#include "DlgFilter.h"
#include "afxdialogex.h"
#include "Utils.h"

#define MAX_STR		"Max"

IMPLEMENT_DYNAMIC(DlgFilter, CDialogEx)
BEGIN_MESSAGE_MAP(DlgFilter, CDialogEx)
	ON_BN_CLICKED(IDC_CHECK_ALLOCS_MIN, &DlgFilter::UpdateControls)
	ON_BN_CLICKED(IDC_CHECK_ALLOCS_MAX, &DlgFilter::UpdateControls)
	ON_BN_CLICKED(IDC_CHECK_SIZE_MIN, &DlgFilter::UpdateControls)
	ON_BN_CLICKED(IDC_CHECK_SIZE_MAX, &DlgFilter::UpdateControls)
	ON_BN_CLICKED(IDC_CHECK_USE_MIN, &DlgFilter::UpdateControls)
	ON_BN_CLICKED(IDC_CHECK_USE_MAX, &DlgFilter::UpdateControls)
	ON_BN_CLICKED(IDC_CHECK_INCLUDE_TEXT, &DlgFilter::UpdateControls)
	ON_BN_CLICKED(IDC_CHECK_EXCLUDE_TEXT, &DlgFilter::UpdateControls)	
	ON_CBN_SELCHANGE(IDC_COMBO_ALLOCS_MAX, &DlgFilter::OnComboSelChange)
	ON_CBN_SELCHANGE(IDC_COMBO_SIZE_MAX, &DlgFilter::OnComboSelChange)
	ON_CBN_SELCHANGE(IDC_COMBO_USE_MAX, &DlgFilter::OnComboSelChange)
	ON_CBN_SETFOCUS(IDC_COMBO_ALLOCS_MAX, &DlgFilter::OnComboSetFocus)
	ON_CBN_SETFOCUS(IDC_COMBO_SIZE_MAX, &DlgFilter::OnComboSetFocus)
	ON_CBN_SETFOCUS(IDC_COMBO_USE_MAX, &DlgFilter::OnComboSetFocus)
	ON_BN_CLICKED(IDOK, &DlgFilter::OnBnClickedOk)
END_MESSAGE_MAP()

DlgFilter::DlgFilter(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_FILTER, pParent), 
	  DlgUtils(this), 
	  m_bTreeView{}
{

}

void DlgFilter::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BOOL DlgFilter::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	if (m_bTreeView)
		EnableTreeViewMode();
	InitDefaultValues();
	UpdateControls();
	return TRUE;
}

void DlgFilter::EnableTreeViewMode()
{		
	// Hide all these controls.
	static constexpr auto controls = { 
		IDC_CHECK_SIZE_MIN, IDC_EDIT_SIZE_MIN, IDC_CHECK_SIZE_MAX, IDC_COMBO_SIZE_MAX,
		IDC_CHECK_ALLOCS_MIN, IDC_EDIT_ALLOCS_MIN, IDC_CHECK_ALLOCS_MAX, IDC_COMBO_ALLOCS_MAX,
		IDC_CHECK_USE_MIN, IDC_EDIT_USE_MIN, IDC_CHECK_USE_MAX, IDC_COMBO_USE_MAX };

	for (auto id : controls)
		ShowControl(id, false);

	// Shrink the dialog. Take 'IDC_CHECK_SIZE_MIN' as a reference.
	CWnd* pCheckSizeMin = GetControl<CWnd>(IDC_CHECK_SIZE_MIN);

	CRect rc;
	GetWindowRect(&rc); // getting dialog coordinates
	
	CRect rcCheckAllocsMin;
	pCheckSizeMin->GetWindowRect(&rcCheckAllocsMin);
	
	int diff = rc.bottom - rcCheckAllocsMin.bottom;
	MoveWindow(rc.left, rc.top, rc.Width(), rc.Height() - diff + 20);

	CRect rcClient;
	GetClientRect(&rcClient);

	CRect rcBtnOk;
	GetDlgItem(IDOK)->GetWindowRect(&rcBtnOk);
	ScreenToClient(&rcBtnOk);
	int height = rcBtnOk.Height();
	rcBtnOk.bottom = rcClient.bottom - 10;
	rcBtnOk.top = rcBtnOk.bottom - height;
	GetDlgItem(IDOK)->MoveWindow(rcBtnOk);

	GetDlgItem(IDCANCEL)->GetWindowRect(&rcBtnOk);
	ScreenToClient(&rcBtnOk);	
	rcBtnOk.bottom = rcClient.bottom - 10;
	rcBtnOk.top = rcBtnOk.bottom - height;
	GetDlgItem(IDCANCEL)->MoveWindow(rcBtnOk);	
}

void DlgFilter::UpdateControls()
{
	auto UpdateCheck = [&](int idChkMin, int idEdMin, int idChkMax, int idEdMax)
	{
		bool bMinChecked = IsButtonChecked(idChkMin);
		GetDlgItem(idEdMin)->EnableWindow(bMinChecked);
		GetDlgItem(idChkMax)->EnableWindow(bMinChecked);
		GetDlgItem(idEdMax)->EnableWindow(bMinChecked && IsButtonChecked(idChkMax));
	};

	UpdateCheck(IDC_CHECK_ALLOCS_MIN, IDC_EDIT_ALLOCS_MIN, IDC_CHECK_ALLOCS_MAX, IDC_COMBO_ALLOCS_MAX);
	UpdateCheck(IDC_CHECK_SIZE_MIN, IDC_EDIT_SIZE_MIN, IDC_CHECK_SIZE_MAX, IDC_COMBO_SIZE_MAX);
	UpdateCheck(IDC_CHECK_USE_MIN, IDC_EDIT_USE_MIN, IDC_CHECK_USE_MAX, IDC_COMBO_USE_MAX);

	GetDlgItem(IDC_EDIT_INCLUDE_TEXT)->EnableWindow(IsButtonChecked(IDC_CHECK_INCLUDE_TEXT));
	GetDlgItem(IDC_EDIT_EXCLUDE_TEXT)->EnableWindow(IsButtonChecked(IDC_CHECK_EXCLUDE_TEXT));
}

void DlgFilter::OnComboSelChange()
{
	if (auto pComboBox = GetCurrentControl<CComboBox>(); pComboBox && pComboBox->GetCurSel() == 0)
		GetDlgItem(IDC_BUTTON_APPLY_STATS_FILTER)->SetFocus();
}

void DlgFilter::OnComboSetFocus()
{
	if (auto pComboBox = GetCurrentControl<CComboBox>(); pComboBox)
		if (CString sTxt = GetControlText(pComboBox); sTxt.CompareNoCase(MAX_STR) == 0)
			pComboBox->SetWindowText("");	
}

void DlgFilter::InitDefaultValues()
{
	auto FillFilter = [&](const StringArray& filter, int idCheck, int idEdit)
	{
		if (!filter.empty())
		{
			SetButtonCheck(idCheck, true);
			CString sFilter;
			for (auto frame : filter)
				sFilter += frame + ";";
			sFilter.TrimRight(';');
			SetControlText(idEdit, sFilter);
		}
	};

	auto FillRange = [&](RangeU64 range, int idCheckMin, int idEditMin, int idCheckMax, int idEditMax)
	{
		bool bMin = range.from != ULLONG_MAX;
		bool bMax = range.to != ULLONG_MAX;

		SetButtonCheck(idCheckMin, bMin);
		SetButtonCheck(idCheckMax, bMin && bMax);

		if (bMin)
			SetControlText(idEditMin, NumberToText(range.from));
		if (bMax)
			SetControlText(idEditMax, NumberToText(range.to));
	};

	FillFilter(includeFrames, IDC_CHECK_INCLUDE_TEXT, IDC_EDIT_INCLUDE_TEXT);
	FillFilter(excludeFrames, IDC_CHECK_EXCLUDE_TEXT, IDC_EDIT_EXCLUDE_TEXT);

	FillRange(rangeSize, IDC_CHECK_SIZE_MIN, IDC_EDIT_SIZE_MIN, IDC_CHECK_SIZE_MAX, IDC_COMBO_SIZE_MAX);
	FillRange(rangeAllocs, IDC_CHECK_ALLOCS_MIN, IDC_EDIT_ALLOCS_MIN, IDC_CHECK_ALLOCS_MAX, IDC_COMBO_ALLOCS_MAX);
	FillRange(rangeUse, IDC_CHECK_USE_MIN, IDC_EDIT_USE_MIN, IDC_CHECK_USE_MAX, IDC_COMBO_USE_MAX);
}

UINT64 DlgFilter::GetEditValue(int nIdControl)
{
	CString text = GetControlText(nIdControl);

	if (text.IsEmpty())
	{
		SetControlFocus(nIdControl);
		throw std::runtime_error("Please enter a value.");
	}

	char *endptr = nullptr;
	errno = 0;
	UINT64 value = strtoull(text.GetString(), &endptr, 10);
	if (errno == ERANGE)
	{
		SetControlFocus(nIdControl);
		throw std::runtime_error(StringFmt("Invalid value '%s'.\r\nThe maximum value is %llu.", text, ULLONG_MAX - 1));
	}	
	if (!endptr || endptr == text.GetString() || *endptr != 0)
	{
		SetControlFocus(nIdControl);
		throw std::runtime_error(StringFmt("Invalid value '%s'.", text));
	}		
	return value;
}

RangeU64 DlgFilter::GetValueRange(int nIdChkMin, int nIdEditMin, int nIdChkMax, int nIdEditMax)
{
	bool bMin = IsButtonChecked(nIdChkMin);
	bool bMax = bMin && IsButtonChecked(nIdChkMax);
	UINT64 min = bMin ? GetEditValue(nIdEditMin) : ULLONG_MAX;
	UINT64 max = bMax ? GetEditValue(nIdEditMax) : ULLONG_MAX;
	if (min > max)
	{
		SetControlFocus(nIdEditMin);
		throw std::runtime_error(StringFmt("The minimum value (%llu) cannot exceed the maximum (%llu).", min, max));		
	}
	return { min, max };
}

StringArray DlgFilter::GetFilterFrames(int nCheckID, int nEditID)
{
	CString filter;
	if (IsButtonChecked(nCheckID))
	{
		GetDlgItem(nEditID)->GetWindowText(filter);
		filter.MakeUpper();
	}
	return Split(filter);
};

BOOL DlgFilter::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN)
	{
		// User cannot type letters in these combos.
		CWnd* pWnd = GetFocus()->GetParent();
		int key = (int)pMsg->wParam;
		if ((pWnd == GetDlgItem(IDC_COMBO_ALLOCS_MAX) || pWnd == GetDlgItem(IDC_COMBO_SIZE_MAX) || pWnd == GetDlgItem(IDC_COMBO_USE_MAX))
			&& key >= 0x39)
			return TRUE;
	}
	return __super::PreTranslateMessage(pMsg);
}

void DlgFilter::OnBnClickedOk()
{
	RangeU64 rangeSizeNew, rangeAllocsNew, rangeUseNew;
	try
	{
		rangeSizeNew = GetValueRange(IDC_CHECK_SIZE_MIN, IDC_EDIT_SIZE_MIN, IDC_CHECK_SIZE_MAX, IDC_COMBO_SIZE_MAX);
		rangeAllocsNew = GetValueRange(IDC_CHECK_ALLOCS_MIN, IDC_EDIT_ALLOCS_MIN, IDC_CHECK_ALLOCS_MAX, IDC_COMBO_ALLOCS_MAX);
		rangeUseNew = GetValueRange(IDC_CHECK_USE_MIN, IDC_EDIT_USE_MIN, IDC_CHECK_USE_MAX, IDC_COMBO_USE_MAX);
	}
	catch (const std::exception &e)
	{		
		ShowError(e.what());
		return;
	}

	rangeSize = rangeSizeNew;
	rangeAllocs = rangeAllocsNew;
	rangeUse = rangeUseNew;

	includeFrames = GetFilterFrames(IDC_CHECK_INCLUDE_TEXT, IDC_EDIT_INCLUDE_TEXT);
	excludeFrames = GetFilterFrames(IDC_CHECK_EXCLUDE_TEXT, IDC_EDIT_EXCLUDE_TEXT);

	CDialogEx::OnOK();
}
