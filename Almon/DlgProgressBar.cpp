#include "stdafx.h"
#include "Almon.h"
#include "DlgProgressBar.h"
#include "Utils.h"


/////////////////////////////////////////////////////////////////////////////
// DlgProgressBar

IMPLEMENT_DYNAMIC(DlgProgressBar, CDialogEx)

BEGIN_MESSAGE_MAP(DlgProgressBar, CDialogEx)
	ON_BN_CLICKED(IDCANCEL, &DlgProgressBar::OnBnClickedCancel)
END_MESSAGE_MAP()

DlgProgressBar::DlgProgressBar(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_PROGRESS_BAR, pParent), DlgUtils(this) {
}

void DlgProgressBar::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PROGRESS, m_ProgressCtrl);
}

BOOL DlgProgressBar::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	CenterWindow();
	SetTitle("");
	SetLabel("");
	SetText("");
	return TRUE;
}

void DlgProgressBar::OnBnClickedCancel()
{
	if (IDYES == MessageBox("Do you want to cancel?", "Cancel", MB_ICONQUESTION | MB_YESNO))
	{
		m_bCancelled = TRUE;
		CDialogEx::OnCancel();
	}
}

void DlgProgressBar::SetTitle(const CString& title)
{
	SetWindowText(title);
	ProcessWinMessages();
}

void DlgProgressBar::SetLabel(const CString& label)
{
	SetControlText(IDC_LABEL, label);
	ProcessWinMessages();
}

void DlgProgressBar::SetText(const CString& text)
{
	SetControlText(IDC_STATIC_TEXT, text);
	ProcessWinMessages();
}

void DlgProgressBar::EnableCancel(bool bEnableCancel)
{
	ShowControl(IDCANCEL, bEnableCancel);
	ProcessWinMessages();
}

void DlgProgressBar::Increment()
{
	m_ProgressCtrl.SetPos(m_ProgressCtrl.GetPos() + 1);
	ProcessWinMessages();
}

void DlgProgressBar::SetMarqueeMode(bool bEnable)
{
	if (bEnable)
	{
		m_ProgressCtrl.ModifyStyle(0, PBS_MARQUEE);
		::SendMessage(m_ProgressCtrl, PBM_SETMARQUEE, (WPARAM)TRUE, (LPARAM)100);
	}
}


/////////////////////////////////////////////////////////////////////////////
// ProgressBar

ProgressBar::ProgressBar(CWnd* pParent, const CString& label, const CString& title, int max) :
	m_pParent(pParent), m_max(max)
{
	Create(pParent);
	SetTitle(title);
	SetLabel(label);
	if (max > 0)
		SetRange(max);
	else
		m_dlg.SetMarqueeMode(true);
}

void ProgressBar::Create(CWnd* pParent)
{
	m_dlg.Create(IDD_PROGRESS_BAR, pParent);
	m_pParent = pParent;
}

void ProgressBar::Show(bool bEnableCancel)
{
	m_dlg.EnableCancel(bEnableCancel);
	m_dlg.ShowWindow(SW_SHOW);
	m_dlg.SetActiveWindow();
	if (m_pParent) m_pParent->EnableWindow(FALSE);
}

void ProgressBar::Hide()
{
	if (m_pParent)
	{
		m_pParent->EnableWindow(TRUE);
		m_pParent->SetActiveWindow();
	}
	m_dlg.ShowWindow(SW_HIDE);
}

void ProgressBar::SetPos(int pos)
{
	m_dlg.SetPos(pos > m_max ? m_max : pos);
	ProcessWinMessages();
}

bool ProgressBar::Update(int pos)
{
	if ((GetTicks() - m_timeLastUpdated) > m_updatePeriod)
	{
		m_timeLastUpdated = GetTicks();

		if (IsCancelled())
			return false;

		if (m_progressStep == 0 || pos % m_progressStep)
			SetPos(pos);
	}
	return true;
}

void ProgressBar::RunWithMarquee(std::function<void(void)> func)
{
	m_dlg.SetMarqueeMode(true);
	auto future = std::async(std::launch::async, func);
	while (future.wait_for(std::chrono::milliseconds(200)) != std::future_status::ready)
	{
		ProcessWinMessages();
		if (!Update(1))
		{
			ShowInfo("Launch cancelled.");
			return;
		}
	}
}
