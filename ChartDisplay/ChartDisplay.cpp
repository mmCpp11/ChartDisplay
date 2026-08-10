// Copyright (C) 2025 Matthew Moran
//
// This file is part of ChartDisplay.  This program is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 3, or (at your option)
// any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License 
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#include "framework.h"
#include "Resource.h"
#include "version.h"
#include <windowsx.h>
#include <shellapi.h>
#pragma comment(lib, "shell32")
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

//Cross-thread UI signals for the chart-update worker.
#define WM_APP_UPDATE_PROGRESS (WM_APP + 0x0010)  //-> progress dialog: refresh status text from update_status_text
#define WM_APP_UPDATE_DONE     (WM_APP + 0x0011)  //-> main window: wParam != 0 means success
//Posted by the app-update-check worker when it finishes; lParam is a heap-owned AppUpdateInfo* to delete.
#define WM_APP_UPDATECHECK_DONE (WM_APP + 0x0012)

import std;
import BasicWindowsWrapperModule;
import Charts;
import Downloader;

//Fixes for standard constructs not in intellisense
#ifdef __INTELLISENSE__
namespace std {
	namespace views = ranges::views;
}
std::size_t operator"" uz(unsigned long long p) { return p; }
#endif

using ApMsgMap = std::unordered_map<std::wstring, COLORREF>;

LRESULT ExtraWindowProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CustomChartProc(HWND, UINT, WPARAM, LPARAM);

charts::FAAChartProcessor* chartaccessor = nullptr;
using CCContainer = std::vector<Win64Wrapper::CommonControl>;
//Controls are grouped by lifecycle into sections. TopARTCC is created once in WM_CREATE and persists;
//AirportSelect is cleared/rebuilt whenever the ARTCC changes; ChartControls is cleared/rebuilt whenever
//an airport is chosen. This makes "redraw just this region" a single .clear() instead of the old
//keep-by-id list plus a boundary index (the now-deleted num_buttons / artcc_control_index).
enum class Section : std::size_t { TopARTCC, AirportSelect, ChartControls, Count };
//Per-window control state: the sectioned controls plus the UI state that used to be shuttled into the
//window proc via WM_APP custom messages. Writers mutate these directly through control_list.at(hwnd).
struct WindowControls {
	std::array<CCContainer, std::to_underlying(Section::Count)> sections;
	ApMsgMap airport_button_colors;
	std::wstring last_airport_clicked;
	long button_border_width = 0;
};
//Convenience accessor for a section's control vector.
inline CCContainer& Sec(WindowControls& wc, Section s) noexcept {
	return wc.sections[std::to_underlying(s)];
}
using CommonControlMap = std::unordered_map<HWND, WindowControls>;
CommonControlMap control_list;
//The custom-charts dialog now uses the ModelessDiagBox wrapper. Engaged while the dialog is open; its window is torn down in CustomChartProc's WM_CLOSE.
std::optional<Win64Wrapper::ModelessDiagBox> custdlg;

//Chart-update worker + its progress dialog. The worker runs UpdateCharts on its own thread; status text
//is marshaled to the dialog via WM_APP_UPDATE_PROGRESS and completion to the main window via
//WM_APP_UPDATE_DONE. update_status_text is shared between the worker (writer) and the dialog (reader).
HINSTANCE prog_hinst = nullptr;
std::optional<Win64Wrapper::ModelessDiagBox> progdlg;
std::jthread update_worker;
std::mutex update_status_mtx;
std::wstring update_status_text;
std::wstring update_done_label;   //"Chart update" / "Chart reload"; set by StartChartUpdate for the result box
bool update_cancellable = true;   //false for a reload (the organize step can't be interrupted) -> disable Cancel
//Run a chart operation on a worker thread behind the progress dialog. no_download=true reorganizes from the
//already-downloaded zips (the Reload path, no network); force=true re-downloads regardless of cycle date.
void StartChartUpdate(HWND main_window, bool no_download, bool force);
INT_PTR ProgressDlgProc(HWND, UINT, WPARAM, LPARAM);
enum class ControlIDList : WORD {
	StaticARTCC = 201,
	StaticBC,
	ComboARTCC,
	ButtonCustom,
	ButtonCWT,
	StaticD,
	StaticET,
	StaticE,
	ComboUntowered,
	ComboManualARTCC,
	StaticAirportName,
	StaticAirportsList,
	StaticSTAR,
	StaticSID,
	StaticIAP,
	StaticManualARTCC,
	ComboAp,
	ComboSID,
	ComboSTAR,
	ComboIAP,
	ComboManual,
	StaticAIRAC,
	DynamicButtonStart=500,
	Dummy=1000
};

//Locate a control by its id within a window's control container; returns end() if not present.
//Centralizes the by-id lookup that the dynamically (re)drawn controls need in many places.
inline CCContainer::iterator FindControl(CCContainer& ctrls, WORD id) {
	return std::ranges::find_if(ctrls, [id](const Win64Wrapper::CommonControl& cc) {
		return cc.GetControlParams().id == id;
		});
}
inline CCContainer::iterator FindControl(CCContainer& ctrls, ControlIDList id) {
	return FindControl(ctrls, std::to_underlying(id));
}
//Cross-section lookup: searches every section, returns a pointer to the control or nullptr. Use this
//where the caller doesn't know (or care) which section holds the id (e.g. owner-draw by CtlID, an
//airport-button click). Callers must copy any needed CommonControlParams immediately: a later
//emplace_back into that section can reallocate its vector and dangle the returned pointer.
inline Win64Wrapper::CommonControl* FindControl(WindowControls& wc, WORD id) {
	auto all = wc.sections | std::views::join;
	auto it = std::ranges::find_if(all, [id](const Win64Wrapper::CommonControl& cc) {
		return cc.GetControlParams().id == id;
		});
	return it != all.end() ? &*it : nullptr;
}
inline Win64Wrapper::CommonControl* FindControl(WindowControls& wc, ControlIDList id) {
	return FindControl(wc, std::to_underlying(id));
}

//border_ctrl_id is the position of the lowest control not wiped
void ClearWindowARTCC(const Win64Wrapper::Window& win);
//returns max width of the button/combo box are, the rest of the screen is for airport specifics
LONG PopulateWindowARTCC(const Win64Wrapper::Window& win, const charts::ARTCC artcc,const ControlIDList border_ctrl_id);
LONG DrawDynamicARTCC(const Win64Wrapper::Window& win, const charts::ARTCC artcc, const ControlIDList border_ctl_id) {
	ClearWindowARTCC(win);
	return PopulateWindowARTCC(win, artcc, border_ctl_id);
}
void OpenAirportCharts(std::wstring, HWND,LONG);

//Build the user-facing status line for a given update phase. Marquee bar + this text is the whole UX:
//the download is opaque per-byte and the organize step has no measurable progress, so text is honest.
std::wstring FormatUpdateStatus(const charts::UpdateStatus& s) {
	switch (s.phase) {
	case charts::UpdatePhase::Checking:    return L"Checking chart availability...";
	case charts::UpdatePhase::Downloading: return std::format(L"Downloading {} ({} of {})...", s.file_name, s.file_index, s.file_count);
	case charts::UpdatePhase::Organizing:  return L"Extracting and organizing charts. This can take a while...";
	}
	return L"";
}
//Dialog proc for IDD_DLGPROG. Runs on the UI thread; the worker only PostMessages into it.
INT_PTR ProgressDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_INITDIALOG:
		//The bar already carries PBS_MARQUEE in the template; start its animation (30ms step).
		SendDlgItemMessage(hwnd, IDC_PROGRESS1, PBM_SETMARQUEE, TRUE, 30);
		//A reload can't be canceled mid-organize, so grey out Cancel rather than offer a dead button.
		if (!update_cancellable) {
			EnableWindow(GetDlgItem(hwnd, IDCANCEL), FALSE);
		}
		return TRUE;
	case WM_APP_UPDATE_PROGRESS: {
		std::wstring text;
		{ std::scoped_lock lk(update_status_mtx); text = update_status_text; }
		SetDlgItemText(hwnd, IDC_ST_PROG, text.c_str());
		return TRUE;
	}
	case WM_COMMAND:
		if (LOWORD(wParam) == IDCANCEL) {
			update_worker.request_stop();
			EnableWindow(GetDlgItem(hwnd, IDCANCEL), FALSE);
			SetDlgItemText(hwnd, IDC_ST_PROG, L"Canceling, please wait...");
			return TRUE;
		}
		break;
	}
	return FALSE;
}
void StartChartUpdate(HWND main_window, bool no_download, bool force) {
	using Win64Wrapper::MessageBoxStyles;
	if (update_worker.joinable()) return; //an update is already running
	//A reload only re-organizes the existing zips (one opaque phase), so it shows a fixed message; a real
	//update reports per-file download progress via the reporter below.
	const std::wstring status = no_download ? L"Reloading charts..." : L"Preparing chart update...";
	update_done_label = no_download ? L"Chart reload" : L"Chart update";
	update_cancellable = !no_download; //a reload has nothing the worker can interrupt mid-organize
	{ std::scoped_lock lk(update_status_mtx); update_status_text = status; }
	progdlg.emplace(prog_hinst, IDD_DLGPROG, &ProgressDlgProc, main_window);
	if (!progdlg->Display()) {
		progdlg.reset();
		Win64Wrapper::CreateMessageBox(L"Could not open the progress dialog.", update_done_label, main_window,
			MessageBoxStyles::Ok, MessageBoxStyles::DefaultButton1, MessageBoxStyles::IconError);
		return;
	}
	progdlg->UpdateDialogText(IDC_ST_PROG, status);
	//Block the main window for the operation's duration: the worker owns the SQLite connection while it runs,
	//so the UI thread must not touch the DB until it finishes (this avoids a cross-thread data race).
	EnableWindow(main_window, FALSE);
	HWND dlg = (*progdlg)();
	update_worker = std::jthread([main_window, dlg, no_download, force](std::stop_token st) {
		//For a download, the reporter stages per-file text and nudges the dialog. A reload has no measurable
		//sub-steps, so it passes an empty reporter and the dialog keeps showing "Reloading charts...".
		charts::UpdateReporter reporter{};
		if (!no_download) {
			reporter = [dlg](const charts::UpdateStatus& s) {
				{ std::scoped_lock lk(update_status_mtx); update_status_text = FormatUpdateStatus(s); }
				PostMessage(dlg, WM_APP_UPDATE_PROGRESS, 0, 0);
			};
		}
		bool ok = false;
		try { ok = chartaccessor->UpdateCharts(no_download, force, reporter, st); }
		catch (...) { ok = false; }
		PostMessage(main_window, WM_APP_UPDATE_DONE, ok ? 1 : 0, 0);
	});
}

// ---- App self-update check: query GitHub Releases, notify on a newer version, open the download page ----

//Result of one update check, handed from the worker thread to the UI thread via WM_APP_UPDATECHECK_DONE.
struct AppUpdateInfo {
	bool checked_ok = false;        //network fetch + version parse both succeeded
	bool update_available = false;  //a newer release than this build exists
	std::wstring latest;            //latest version, e.g. L"2.1.0" (only meaningful when checked_ok)
	bool manual = false;            //true if the user asked (Help->Check for Updates); false for the silent startup check
};

namespace {
	constexpr wchar_t kLatestReleaseApi[] = L"https://api.github.com/repos/mmCpp11/ChartDisplay/releases/latest";
	constexpr wchar_t kReleasesPage[] = L"https://github.com/mmCpp11/ChartDisplay/releases/latest";

	//Pull the "tag_name" value out of the GitHub release JSON without a full JSON parser: the field is a
	//simple "tag_name":"vX.Y.Z" pair, so find the key and read the next quoted token.
	std::optional<std::string> ExtractTagName(const std::string& json) {
		auto key = json.find("\"tag_name\"");
		if (key == std::string::npos) return std::nullopt;
		auto colon = json.find(':', key + 10);
		if (colon == std::string::npos) return std::nullopt;
		auto q1 = json.find('"', colon);
		if (q1 == std::string::npos) return std::nullopt;
		auto q2 = json.find('"', q1 + 1);
		if (q2 == std::string::npos) return std::nullopt;
		return json.substr(q1 + 1, q2 - (q1 + 1));
	}

	//Parse "vX.Y.Z" (or "X.Y.Z", with missing components defaulting to 0) into {major, minor, patch}.
	std::optional<std::array<int, 3>> ParseVersion(std::string_view tag) {
		if (!tag.empty() && (tag.front() == 'v' || tag.front() == 'V')) tag.remove_prefix(1);
		std::array<int, 3> v{ 0, 0, 0 };
		std::size_t part = 0, i = 0;
		while (part < v.size()) {
			std::size_t dot = tag.find('.', i);
			std::string_view seg = tag.substr(i, dot == std::string_view::npos ? std::string_view::npos : dot - i);
			int value = 0;
			auto [ptr, ec] = std::from_chars(seg.data(), seg.data() + seg.size(), value);
			if (ec != std::errc{}) return std::nullopt;   //non-numeric segment -> not a version we understand
			v[part++] = value;
			if (dot == std::string_view::npos) break;
			i = dot + 1;
		}
		return v;
	}
}

std::jthread appupdate_worker;   //one app-update check at a time; reset in WM_APP_UPDATECHECK_DONE

//Kick off a GitHub release check on a worker thread. manual=true is a user request (report every outcome);
//manual=false is the silent startup check (only speaks up when a newer version exists). The result is
//marshaled back to main_window via WM_APP_UPDATECHECK_DONE with a heap-owned AppUpdateInfo* in lParam.
void StartAppUpdateCheck(HWND main_window, bool manual) {
	if (appupdate_worker.joinable()) return;   //a check is already running
	appupdate_worker = std::jthread([main_window, manual] {
		auto info = std::make_unique<AppUpdateInfo>();
		info->manual = manual;
		std::string body;
		if (Net::HttpGetToString(kLatestReleaseApi, body)) {
			if (auto tag = ExtractTagName(body)) {
				if (auto remote = ParseVersion(*tag)) {
					info->checked_ok = true;
					const std::array<int, 3> local{ CD_VER_MAJOR, CD_VER_MINOR, CD_VER_PATCH };
					info->update_available = (*remote > local);
					info->latest = std::format(L"{}.{}.{}", (*remote)[0], (*remote)[1], (*remote)[2]);
				}
			}
		}
		PostMessage(main_window, WM_APP_UPDATECHECK_DONE, 0, reinterpret_cast<LPARAM>(info.release()));
	});
}

//UI-thread handler for a finished update check (called from WM_APP_UPDATECHECK_DONE).
void HandleAppUpdateResult(HWND hwnd, const AppUpdateInfo& info) {
	using Win64Wrapper::MessageBoxStyles;
	if (info.update_available) {
		const auto current = std::format(L"{}.{}.{}", CD_VER_MAJOR, CD_VER_MINOR, CD_VER_PATCH);
		auto resp = Win64Wrapper::CreateMessageBox(
			std::format(L"A new version of ChartDisplay is available.\n\nInstalled: {}\nLatest: {}\n\nOpen the download page now?",
				current, info.latest),
			L"Update Available", hwnd,
			MessageBoxStyles::YesNo, MessageBoxStyles::DefaultButton1, MessageBoxStyles::IconInformation);
		if (resp == Win64Wrapper::MessageBoxResponse::Yes) {
			ShellExecuteW(hwnd, L"open", kReleasesPage, nullptr, nullptr, SW_SHOWNORMAL);
		}
		return;
	}
	//No newer version: only bother the user when they explicitly asked. The startup check stays silent.
	if (!info.manual) return;
	if (info.checked_ok) {
		Win64Wrapper::CreateMessageBox(L"You're running the latest version of ChartDisplay.", L"No Updates",
			hwnd, MessageBoxStyles::Ok, MessageBoxStyles::DefaultButton1, MessageBoxStyles::IconInformation);
	}
	else {
		Win64Wrapper::CreateMessageBox(
			L"Could not check for updates. Please check your internet connection and try again.",
			L"Update Check Failed", hwnd, MessageBoxStyles::Ok, MessageBoxStyles::DefaultButton1, MessageBoxStyles::IconWarning);
	}
}

//Help->About: a version-aware box, kept in code so the version stays single-sourced from version.h.
void ShowAboutBox(HWND hwnd) {
	using Win64Wrapper::MessageBoxStyles;
	Win64Wrapper::CreateMessageBox(
		std::format(L"FAA Chart Display\nVersion {}.{}.{}\n\nCopyright (C) 2025 Matthew Moran\nLicensed under the GNU GPL v3.",
			CD_VER_MAJOR, CD_VER_MINOR, CD_VER_PATCH),
		L"About ChartDisplay", hwnd, MessageBoxStyles::Ok, MessageBoxStyles::DefaultButton1, MessageBoxStyles::IconInformation);
}

int wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(nCmdShow);
	//deal with command line
	std::wstring_view cmdline(lpCmdLine);
	if (cmdline.contains(L"--help") || cmdline.contains(L"-h")) {
		Win64Wrapper::CreateMessageBox(L"Usage: ChartDisplay.exe", L"Help");
		return 0;
	}
	prog_hinst = hInstance;
	charts::FAAChartProcessor chart;
	chartaccessor = &chart;
	//Init COM, then CommonControls
	Win64Wrapper::COMInit com_init;
	if (FAILED(com_init.hr)) {
		Win64Wrapper::CreateMessageBox(L"COM initalization failed. Exiting.", L"COM Error", nullptr,
			Win64Wrapper::MessageBoxStyles::IconError, Win64Wrapper::MessageBoxStyles::Ok,
			Win64Wrapper::MessageBoxStyles::DefaultButton1);
		return 1;
	}
	Win64Wrapper::CommonControl::InitalizeCommonControls({ Win64Wrapper::ControlNames::Static,Win64Wrapper::ControlNames::ComboBox,
		Win64Wrapper::ControlNames::ListView,Win64Wrapper::ControlNames::ProgBar});
	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.hInstance = hInstance;
	wc.lpszClassName = L"ChartDisplayWinClass";
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.hCursor = std::bit_cast<HCURSOR>(LoadImage(NULL, MAKEINTRESOURCE(IDC_ARROW), IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE | LR_SHARED));
	if (!wc.hCursor) {
		OutputDebugString(std::format(L"LoadImage error with ID: {}, GLE Code: {}\n", 32512, GetLastError()).c_str());
	}
	//if the following line isn't included, WM_PAINT must be used at all times to redraw the window
	wc.hbrBackground = Win64Wrapper::ColorToHBRUSH(COLOR_WINDOW);
	Win64Wrapper::WindowSize sz{ 1623,933 }; //old size 831x595
	Win64Wrapper::WindowStyles wstyle;
	wstyle.extended_styles = WS_EX_APPWINDOW;
	auto window_title = std::format(L"FAA Chart Display {}.{}.{}", CD_VER_MAJOR, CD_VER_MINOR, CD_VER_PATCH);
	Win64Wrapper::Window win(wc, &ExtraWindowProc, sz, window_title, true, Win64Wrapper::WindowLogger(L"log.txt"),wstyle);
	auto res=win.DisplayWindow();
	if (res == false) {
		return 1;
	}

	if (auto need = chart.ChartUpdateNeeded(); need != charts::UpdateNeed::None) {
		StartChartUpdate(win(), false, need == charts::UpdateNeed::Initial);
	}

	//Silent app-update check: notifies only if a newer release exists (result handled in WM_APP_UPDATECHECK_DONE).
	StartAppUpdateCheck(win(), false);

	MSG msg;
	auto acceltable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CHARTDISPLAY));
	while (GetMessage(&msg, NULL, 0, 0) > 0) {
		HWND prog_hwnd = progdlg ? (*progdlg)() : nullptr;
		HWND cust_hwnd = custdlg ? (*custdlg)() : nullptr;
		if (auto accmsg = TranslateAccelerator(msg.hwnd,acceltable,&msg);
			( accmsg==0 && !IsDialogMessage(cust_hwnd, &msg) && !IsDialogMessage(prog_hwnd, &msg))) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return 0;
}
LRESULT ExtraWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	using Win64Wrapper::CommonControl;
	using Win64Wrapper::ControlNames;
	using Win64Wrapper::WindowSize;
	using Win64Wrapper::CommonControlParams;
	using Win64Wrapper::MessageBoxStyles;
	using Win64Wrapper::PtrToLP;
	using namespace Win64Wrapper::literals;
	bool defwinproc = false;
	//Per-window state now lives in control_list.at(hwnd) (a WindowControls), not in proc statics.
	//Switching airports just wipes the per-airport chart controls; the airport-select section stays put.
	auto ResetARTCCControls = [](WindowControls& wc) {
		Sec(wc, Section::ChartControls).clear();
		};
	auto OpenFileDefault = [hwnd](const std::filesystem::path& pth) {
		const std::wstring file = pth.native();
		SHELLEXECUTEINFOW sei{ .cbSize = sizeof(SHELLEXECUTEINFOW) };
		sei.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI; //suppress the shell's own error UI; we handle it
		sei.hwnd = hwnd;
		sei.lpVerb = L"open";
		sei.lpFile = file.c_str();
		sei.nShow = SW_SHOWNORMAL;
		if (ShellExecuteExW(&sei)) {
			return;
		}
		if (const DWORD err = GetLastError(); err == ERROR_NO_ASSOCIATION || err == SE_ERR_NOASSOC) {
			SHELLEXECUTEINFOW openas{ .cbSize = sizeof(SHELLEXECUTEINFOW) };
			openas.fMask = SEE_MASK_NOASYNC;
			openas.hwnd = hwnd;
			openas.lpVerb = L"openas"; //the "Open with..." dialog
			openas.lpFile = file.c_str();
			openas.nShow = SW_SHOWNORMAL;
			if (ShellExecuteExW(&openas)) {
				return;
			}
		}
		Win64Wrapper::CreateMessageBox(
			std::format(L"Could not open the file:\n{}\n(error {})", file, GetLastError()),
			L"Error Opening Chart", hwnd, Win64Wrapper::MessageBoxStyles::Ok,
			Win64Wrapper::MessageBoxStyles::DefaultButton1, Win64Wrapper::MessageBoxStyles::IconError);
		};
	//Draw Chart AIRAC Cycle label. During WM_CREATE the control_list item has not been created yet, so allow explict pass in
	auto DrawAIRACLabel = [&hwnd](std::optional<std::reference_wrapper<CCContainer>> ctrls=std::nullopt,bool wmcreate = false){
		Win64Wrapper::Window& winclass = Win64Wrapper::GetWindowFromHWND(hwnd);
		CCContainer& winctrls = ctrls ? ctrls->get() : Sec(control_list.at(hwnd), Section::TopARTCC);
		if (!wmcreate) {
			auto sairac_ctl = FindControl(winctrls, ControlIDList::StaticAIRAC);
			if (sairac_ctl == winctrls.end())
				return;
			winctrls.erase(sairac_ctl);
		}
		std::wstring airac_text = L"Chart AIRAC Cycle: ";
		charts::AIRACInfo ai = chartaccessor->GetLastChartUpdate();
		if (ai) {
			airac_text += std::to_wstring(ai.cycle_id);
		}
		else {
			airac_text += L"N/A";
		}
		auto sartcc_ctl = FindControl(winctrls, ControlIDList::StaticARTCC);
		if (sartcc_ctl == winctrls.end())
			return;
		auto p_sartcc = sartcc_ctl->GetControlParams();
		CommonControlParams p_sairac{ ControlNames::Static,airac_text.c_str(),std::to_underlying(ControlIDList::StaticAIRAC),
		p_sartcc.posX, p_sartcc.posY + 25,WindowSize(200,20) };
		winctrls.emplace_back(p_sairac, winclass, CommonControl::CommonStyles::StaticLeft);
	};
	switch (msg) {
	case WM_CREATE:
	{
		Win64Wrapper::Window& winclass = Win64Wrapper::GetWindowFromHWND(hwnd);
		//Attach the main menu bar. Item IDs are the IDM_*
		//commands routed in WM_COMMAND below.
		if (HMENU menu = LoadMenu(GetModuleHandleW(nullptr), MAKEINTRESOURCE(IDC_CHARTDISPLAY))) {
			SetMenu(hwnd, menu);
			//Seed the "Auto-update Charts on Start" checkmark from the persisted setting. The getter throws
			//when the control row is absent (fresh DB), so treat any failure as off.
			bool autoupd = false;
			try
			{
				autoupd = chartaccessor && chartaccessor->AutoupdateState().value_or(false);
			}
			catch (...)
			{
				autoupd = false;
			}
			CheckMenuItem(menu, IDM_AUTOUPDATE, MF_BYCOMMAND | (autoupd ? MF_CHECKED : MF_UNCHECKED));
		}
		//ARTCC static control label
		int lloffsetx = 10, lloffsety = 10;
		CCContainer winctrls;
		CommonControlParams p_sartcc{ ControlNames::Static,L"ARTCC:",std::to_underlying(ControlIDList::StaticARTCC),lloffsetx,lloffsety + 5,WindowSize(70,20) };
		winctrls.emplace_back(p_sartcc, winclass, CommonControl::CommonStyles::StaticLeft);

		//ARTCC combo box
		CommonControlParams p_artccbox{ ControlNames::ComboBox,L"Default Option",std::to_underlying(ControlIDList::ComboARTCC),
			lloffsetx + p_sartcc.sz.width + 10, lloffsety,WindowSize(150,1000) };
		winctrls.emplace_back(p_artccbox, winclass, CommonControl::CommonStyles::ComboBoxDDL);
		int btncol_x = p_artccbox.posX + p_artccbox.sz.width + 10;
		//Custom (user-added) charts button
		CommonControlParams p_rmanual{ ControlNames::Button,L"Custom Charts",std::to_underlying(ControlIDList::ButtonCustom),
		btncol_x,lloffsety,WindowSize(140,30) };
		winctrls.emplace_back(p_rmanual, winclass, CommonControl::CommonStyles::PushButton);
		//AIRAC static control label
		DrawAIRACLabel(winctrls, true);
		//populate the ARTCC combo box
		std::vector<std::string> artcclist = charts::artcc_names_map | std::views::values | std::ranges::to<std::vector>();
		//remove ZAE (add it back later as other)
		std::erase_if(artcclist, [](std::string artcc) {
			return ((artcc == "ZAE") ? true : false);
			});

		//deal with HCF by adding a Z in front
		std::ranges::sort(artcclist, [](const std::string& a, const std::string& b)-> bool {
			if (a == "HCF" || b == "HCF") {
				if (a == "HCF") {
					std::string newa = "ZHCF";
					return newa < b;
				}
				else if (b == "HCF") {
					std::string newb = "ZHCF";
					return a < newb;
				}
			}
			return a < b;
			});
		artcclist.emplace_back("Other");
		for (auto& i : artcclist) {
			//convert artcc names to wide strings
			std::wstring wdartcc = Win64Wrapper::convert_string(i);
			//add ARTCC name to the drop down
			auto addstr = wdartcc.c_str();
			auto lerr = SendMessage(GetDlgItem(hwnd, static_cast<int>(ControlIDList::ComboARTCC)), static_cast<UINT>(CB_ADDSTRING),
				0_wp, PtrToLP(addstr));
			if (lerr == CB_ERR || lerr == CB_ERRSPACE) {
				OutputDebugString(std::format(L"Error adding {} to ComboBox, Error Code: {:d}\n", wdartcc, lerr).c_str());
				return FALSE;
			}
		}
		WindowControls wc;
		Sec(wc, Section::TopARTCC) = std::move(winctrls);
		control_list.emplace(hwnd, std::move(wc));
	}
		break;
	case WM_DESTROY:
	{
		defwinproc = true;
		try {
			//don't care about the vector there, just care if it throws or not
			auto& ctrls=control_list.at(hwnd);
			control_list.erase(hwnd);
		}
		catch (std::out_of_range&) {}
	}
		break;
	case WM_CTLCOLORSTATIC:
	{
		auto wincolor = GetSysColor(COLOR_WINDOW);
		auto sc_devc = std::bit_cast<HDC>(wParam);
		SetTextColor(sc_devc, GetSysColor(COLOR_WINDOWTEXT));
		SetBkColor(sc_devc, wincolor);
		static HBRUSH st_bkgrd_brush = GetSysColorBrush(COLOR_WINDOW);
		return std::bit_cast<LRESULT>(st_bkgrd_brush);
	}
	case WM_SIZE:
	{
		auto& wc = control_list.at(hwnd);
		for (auto& cc : wc.sections | std::views::join) {
			auto id = cc.GetControlParams().id;
			if (static_cast<ControlIDList>(id) == ControlIDList::ComboARTCC) {
				SendMessage(hwnd, static_cast<UINT>(WM_COMMAND), MAKEWPARAM(id, CBN_SELCHANGE), PtrToLP(cc()));
			}
			if (id == std::to_underlying(ControlIDList::ComboAp)) {
				OpenAirportCharts(wc.last_airport_clicked, hwnd, wc.button_border_width);
			}
		}
		break;
	}
	case WM_DRAWITEM:
	{
		auto draw_params = std::bit_cast<LPDRAWITEMSTRUCT>(lParam);
		auto& wc = control_list.at(hwnd);
		if (auto* cc = FindControl(wc, static_cast<WORD>(draw_params->CtlID))) {
			if (draw_params->CtlType == ODT_BUTTON) {
				SIZE sz{};
				std::wstring text = cc->GetControlParams().text;
				GetTextExtentPoint32(draw_params->hDC, text.c_str(), static_cast<int>(text.size()), &sz);
				SetTextColor(draw_params->hDC, wc.airport_button_colors.at(text));
				SIZE coord_pos{ ((draw_params->rcItem.right - draw_params->rcItem.left) - sz.cx) / 2,
					((draw_params->rcItem.bottom - draw_params->rcItem.top) - sz.cy) / 2 };
				ExtTextOut(draw_params->hDC, coord_pos.cx, coord_pos.cy, ETO_OPAQUE | ETO_CLIPPED, &draw_params->rcItem, text.c_str(), text.size(), nullptr);
				UINT edgestate = {};
				if (draw_params->itemState & ODS_SELECTED) {
					edgestate = static_cast<UINT>(EDGE_SUNKEN);
				}
				else {
					edgestate = static_cast<UINT>(EDGE_RAISED);
				}
				DrawEdge(draw_params->hDC, &draw_params->rcItem, edgestate, BF_RECT);
			}
		}
		break;
	}
	case WM_COMMAND:
	{
		auto ctl_id = LOWORD(wParam);
		//Menu bar / accelerator commands carry no control notification and use IDM_* IDs, which never collide
		//with control IDs (>=201). Handle them up front so both menu clicks and accelerators are covered.
		switch (ctl_id) {
		case IDM_EXIT:
			DestroyWindow(hwnd);   //-> WM_DESTROY -> wrapper posts WM_QUIT
			return 0;
		case IDM_ABOUT:
			ShowAboutBox(hwnd);
			return 0;
		case IDM_FORCEUPDATE:
			if (chartaccessor) {
				auto check = Win64Wrapper::CreateMessageBox(L"Confirm Chart Upgrade?", L"Chart Upgrade", hwnd,
					MessageBoxStyles::AppModal, MessageBoxStyles::IconWarning, MessageBoxStyles::DefaultButton2, MessageBoxStyles::YesNo);
				//Assume No on any message-box failure, to avoid an accidental chart re-download.
				if (check == Win64Wrapper::MessageBoxResponse::Yes) {
					//Worker thread behind a progress dialog; completion lands in WM_APP_UPDATE_DONE.
					StartChartUpdate(hwnd, false, true);
				}
			}
			return 0;
		case IDM_RELOAD:
			//Reorganize the already-downloaded zips (no network); completion lands in WM_APP_UPDATE_DONE.
			if (chartaccessor) StartChartUpdate(hwnd, true, false);
			return 0;
		case IDM_CHECKUPDATE:
			StartAppUpdateCheck(hwnd, true);   //manual check: always report the result
			return 0;
		case IDM_AUTOUPDATE:
			if (chartaccessor) {
				//Toggle the checkable menu item and persist the new state immediately.
				const bool nowon = !(GetMenuState(GetMenu(hwnd), IDM_AUTOUPDATE, MF_BYCOMMAND) & MF_CHECKED);
				CheckMenuItem(GetMenu(hwnd), IDM_AUTOUPDATE, MF_BYCOMMAND | (nowon ? MF_CHECKED : MF_UNCHECKED));
				chartaccessor->AutoupdateState(nowon);
			}
			return 0;
		}
		switch (HIWORD(wParam)) {
		case BN_CLICKED:
		{
			auto& wc = control_list.at(hwnd);
			//reload custom charts
			if (ctl_id == std::to_underlying(ControlIDList::ButtonCustom)) {
				//emplace replaces any previous instance (its window, if still open, is torn down by the dtor).
				custdlg.emplace(prog_hinst, IDD_CUSTCHART, &CustomChartProc, hwnd);
				custdlg->Display();
			}
			//CWT button
			else if (ctl_id == std::to_underlying(ControlIDList::ButtonCWT)) {
				auto cwtpath = chartaccessor->GetCWTItemPath();
				if (cwtpath) {
					OpenFileDefault(cwtpath.value());
				}
			}
			//handle airport buttons
			//Dynamic airport buttons are the only controls with id >= DynamicButtonStart (fixed controls are
			//in the low 200s), so the lower bound alone identifies them; a stale id just yields a null FindControl.
			else if (ctl_id >= std::to_underlying(ControlIDList::DynamicButtonStart)) {
				ResetARTCCControls(wc);
				if (auto* cc = FindControl(wc, static_cast<WORD>(ctl_id))) {
					wc.last_airport_clicked = cc->GetControlParams().text;
					OpenAirportCharts(wc.last_airport_clicked, hwnd, wc.button_border_width);
				}
			}
			else {
				defwinproc = true;
			}
			break;
		}
		case CBN_SELCHANGE:
		{
			//get text selection
			auto cmb = std::bit_cast<HWND>(lParam);
			//CB_GETCURSEL returns CB_ERR (-1) when nothing is selected (e.g. the WM_SIZE-synthesized
			//CBN_SELCHANGE that fires before any ARTCC is picked). Storing -1 in an unsigned index and
			//passing it to CB_GETLBTEXTLEN gives CB_ERR -> resize_and_overwrite((size_t)-1) -> bad_alloc.
			auto index = SendMessage(cmb, static_cast<UINT>(CB_GETCURSEL), 0_wp, 0LL);
			if (index == CB_ERR) break;
			auto textlen = SendMessage(cmb, static_cast<UINT>(CB_GETLBTEXTLEN), index, 0ll);
			if (textlen == CB_ERR) break;
			std::wstring selbuf;
			selbuf.resize_and_overwrite(textlen, [&cmb, &index](wchar_t* buf, size_t sz) noexcept
				->std::size_t {
					auto ret = SendMessage(cmb, static_cast<UINT>(CB_GETLBTEXT), index, PtrToLP(buf));
					if (ret == CB_ERR) return 0;
					else return ret;
				});
			auto boxid = LOWORD(wParam);
			std::array<ControlIDList, 5> chart_control_ids = { ControlIDList::ComboAp,ControlIDList::ComboSTAR,ControlIDList::ComboIAP,
				ControlIDList::ComboSID,ControlIDList::ComboManual };
			std::string sel = Win64Wrapper::convert_string(selbuf);
			auto& wc = control_list.at(hwnd);
			//ARTCC selection change
			if (boxid == std::to_underlying(ControlIDList::ComboARTCC)) {
				const Win64Wrapper::Window& winclass = Win64Wrapper::GetWindowFromHWND(hwnd);
				wc.last_airport_clicked.clear();
				if (sel == "Other") {
					sel = "ZAE";
				}
				wc.button_border_width = DrawDynamicARTCC(winclass, charts::rev_artcc_names_map.at(sel), ControlIDList::StaticARTCC);
			}
			else if (boxid == std::to_underlying(ControlIDList::ComboUntowered)) {
				ResetARTCCControls(wc);
				wc.last_airport_clicked = selbuf;
				OpenAirportCharts(selbuf, hwnd, wc.button_border_width);
			}
			else if (boxid == std::to_underlying(ControlIDList::ComboManualARTCC)) {
				//get artcc selection
				auto artcc_combo = GetDlgItem(hwnd, static_cast<int>(ControlIDList::ComboARTCC));
				auto ai = SendMessage(artcc_combo,static_cast<UINT>(CB_GETCURSEL), 0_wp, 0_lp);
				if (ai == CB_ERR) break; //no ARTCC selected; nothing to resolve
				auto artcc_textlen = SendMessage(artcc_combo, static_cast<UINT>(CB_GETLBTEXTLEN), ai, 0_lp);
				if (artcc_textlen == CB_ERR) break;
				std::wstring artccstr;
				artccstr.resize_and_overwrite(artcc_textlen,
					[&artcc_combo, &ai](wchar_t* buf, size_t sz) noexcept
					->std::size_t {
						auto ret = SendMessage(artcc_combo, static_cast<UINT>(CB_GETLBTEXT), ai, PtrToLP(buf));
						if (ret == CB_ERR) return 0;
						else return ret;
					});
				charts::ARTCC selartcc = charts::rev_artcc_names_map.at(Win64Wrapper::convert_string(artccstr));
				auto aa_artcc = chartaccessor->GetARTCCAdditions(selartcc);
				auto iit = std::ranges::find_if(aa_artcc, [&sel](charts::ManualARTCCAddition item) {
					if (sel == item.name) {
						return true;
					}
					else {
						return false;
					}
					});
			std::filesystem::path filepath = iit->filepath;
			OpenFileDefault(filepath);
			}
			//chart selection change
			else if (auto it = std::ranges::find_if(chart_control_ids, [&boxid](ControlIDList cid) {
				if (boxid == std::to_underlying(cid)) return true;
				else return false;
				});it != chart_control_ids.end()) {
				auto acharts = chartaccessor->GetAirportCharts(Win64Wrapper::convert_string(wc.last_airport_clicked));
				for (auto& c : acharts) {
					if (c.procedure_name == sel) {
						OpenFileDefault(c.chartpath);
						break;
					}
				}
			}
			break;
		}
		}
		break;
	}
	//Posted by the chart-update worker thread when it finishes (wParam != 0 on success). Runs on the UI
	//thread, so it is safe to touch the DB/controls again here.
	case WM_APP_UPDATE_DONE:
	{
		const bool ok = wParam != 0;
		EnableWindow(hwnd, TRUE);   //re-enable the main window before tearing down the dialog
		progdlg.reset();            //destroy the progress dialog
		update_worker = {};         //join the (already finished) worker
		DrawAIRACLabel();           //chart cycle may have changed; refresh the label
		if (ok) {
			Win64Wrapper::CreateMessageBox(std::format(L"{} complete.", update_done_label), update_done_label, hwnd);
		}
		else {
			Win64Wrapper::CreateMessageBox(
				L"The operation did not complete (it was canceled or failed). Your existing charts were kept.",
				update_done_label, hwnd, MessageBoxStyles::Ok, MessageBoxStyles::DefaultButton1, MessageBoxStyles::IconWarning);
		}
		break;
	}
	//Posted by the app-update-check worker; lParam owns a heap AppUpdateInfo. Runs on the UI thread.
	case WM_APP_UPDATECHECK_DONE:
	{
		std::unique_ptr<AppUpdateInfo> info(reinterpret_cast<AppUpdateInfo*>(lParam));
		appupdate_worker = {};   //join the finished worker so a later check can start
		if (info) HandleAppUpdateResult(hwnd, *info);
		break;
	}
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	//run defwinproc for specific messages where default behavior is also wanted
	if (defwinproc) {
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	else return 0;
}
void ClearWindowARTCC(const Win64Wrapper::Window& win) {
	//Changing ARTCC keeps the persistent TopARTCC controls and drops the per-ARTCC airport-select controls
	//along with any per-airport chart controls. Sectioning replaces the old keep-by-id list with two clears.
	auto& wc = control_list.at(win());
	Sec(wc, Section::AirportSelect).clear();
	Sec(wc, Section::ChartControls).clear();
	wc.airport_button_colors.clear();
}
LONG PopulateWindowARTCC(const Win64Wrapper::Window& win, const charts::ARTCC artcc, const ControlIDList border_ctrl_id) {
	using Win64Wrapper::CommonControl;
	using Win64Wrapper::CommonControlParams;
	using Win64Wrapper::ControlNames;
	using Win64Wrapper::WindowSize;
	using Win64Wrapper::PtrToLP;
	using namespace Win64Wrapper::literals;
	enum class BtnColors : COLORREF {
		ClassB = RGB(38, 70, 173),
		ClassC = RGB(122, 47, 47),
		ClassD = RGB(37, 212, 59),
		ClassEGTowered = RGB(79, 17, 140)
	};
	const double MAX_BUTTON_AREA_SCALE = 0.45;
	auto& wc = control_list.at(win());
	CCContainer& winctrls = Sec(wc, Section::AirportSelect);
	auto* border_cc = FindControl(wc, border_ctrl_id);
	if (!border_cc) return 0;
	CommonControlParams borderparams = border_cc->GetControlParams();
	auto alist = chartaccessor->GetAirspaceClassInfo(artcc);
	//assign colors
	ApMsgMap btn_colors;
	auto assign_colors = [&btn_colors](const std::vector<std::string>& airports, BtnColors color_to_use) {
		if (airports.empty()) return;
		std::wstring convap;
		for (auto& i : airports) {
			convap = Win64Wrapper::convert_string(i);
			btn_colors.try_emplace(convap, std::to_underlying(color_to_use));
		}
		};
	assign_colors(alist.class_b_airports, BtnColors::ClassB);
	assign_colors(alist.class_c_airports, BtnColors::ClassC);
	assign_colors(alist.class_d_airports, BtnColors::ClassD);
	assign_colors(alist.other_towered, BtnColors::ClassEGTowered);
	//Hand the owner-draw colors to the proc by storing them on the window's state.
	wc.airport_button_colors = std::move(btn_colors);
	//Class B and C static control label
	CommonControlParams p_sbc{ ControlNames::Static,L"Class B and C Airports:",std::to_underlying(ControlIDList::StaticBC),
		borderparams.posX,borderparams.posY + 50,WindowSize(190,20) };
	winctrls.emplace_back(p_sbc , win , CommonControl::CommonStyles::StaticLeft);
	// Dynamic Buttons
	WindowSize btnsz{ 60,40 };
	RECT winsz{};
	GetClientRect(win(), &winsz);
	winsz.right *= MAX_BUTTON_AREA_SCALE;
	auto num_per_line = std::div(winsz.right, static_cast<long>(btnsz.width)).quot - 1;
	//Class C and B airports
	std::vector<std::string> airport_list;
	if (alist.class_b_airports.size() != 0) {
		airport_list.reserve(alist.class_b_airports.size() + alist.class_c_airports.size());
		airport_list.assign_range(alist.class_b_airports);
		airport_list.append_range(alist.class_c_airports);
	}
	else {
		airport_list = alist.class_c_airports;
	}
	short perline_counter = 1;
	auto pos_x = p_sbc.posX;
	auto pos_y = p_sbc.posY + p_sbc.sz.height + 5;
	auto ctl_id = std::to_underlying(ControlIDList::DynamicButtonStart);
	auto add_button = [&](std::string airport, COLORREF clr) {
		std::wstring ap_str = Win64Wrapper::convert_string(airport);
		CommonControlParams p_btn_next{ ControlNames::Button,ap_str,ctl_id++,pos_x,pos_y,btnsz };
		winctrls.emplace_back(p_btn_next, win, CommonControl::CommonStyles::PushButton, BS_OWNERDRAW, false);
		++perline_counter;
		if (perline_counter == num_per_line) {
			pos_x = p_sbc.posX;
			pos_y += (p_btn_next.sz.height + 5);
			perline_counter = 0;
		}
		else {
			pos_x += (p_btn_next.sz.width + 5);
		}
		};
	for (auto& ap : airport_list) {
		COLORREF clr = std::to_underlying(BtnColors::ClassC);
		if (std::ranges::find(alist.class_b_airports, ap) != alist.class_b_airports.end()) {
			clr = std::to_underlying(BtnColors::ClassB);
		}
		add_button(ap, clr);
	}
	pos_x = p_sbc.posX;
	if (pos_y != (pos_y + btnsz.height + 5)) {
		pos_y += (btnsz.height + 5);
	}
	//Class D static control label and buttons
	CommonControlParams p_sd{ ControlNames::Static,L"Class D Airports:",std::to_underlying(ControlIDList::StaticD),
		pos_x,pos_y,WindowSize{150,20} };
	winctrls.emplace_back(p_sd, win, CommonControl::CommonStyles::StaticLeft);
	pos_y += (p_sd.sz.height + 5);
	perline_counter = 1;
	std::ranges::for_each(alist.class_d_airports, std::bind(add_button, std::placeholders::_1, std::to_underlying(BtnColors::ClassD)));
	//Class E towered airports, if any
	if (alist.other_towered.size() != 0) {
		pos_x = p_sbc.posX;
		if (pos_y != (pos_y + btnsz.height + 5)) {
			pos_y += (btnsz.height + 5);
		}
		CommonControlParams p_set{ ControlNames::Static,L"Class E/G Towered Airports:",std::to_underlying(ControlIDList::StaticET),
			pos_x,pos_y,WindowSize{250,20} };
		winctrls.emplace_back(p_set, win, CommonControl::CommonStyles::StaticLeft);
		pos_y += (p_set.sz.height + 5);
		perline_counter = 1;
		std::ranges::for_each(alist.other_towered, std::bind(add_button, std::placeholders::_1, std::to_underlying(BtnColors::ClassEGTowered)));
	}
	//Untowered Airport Static Control and Combo Box
	pos_x = p_sbc.posX;
	//give a little extra space to separate the combo box
	if (pos_y != (pos_y + btnsz.height + 5)) {
		pos_y += (btnsz.height + 10);
	}
	else {
		pos_y += 5;
	}
	CommonControlParams p_se{ ControlNames::Static,L"Untowered Airports:",std::to_underlying(ControlIDList::StaticE),
		pos_x,pos_y + 5,WindowSize(200,20) }; //the additional y is to align with the center of the combobox
	winctrls.emplace_back(p_se, win, CommonControl::CommonStyles::StaticLeft);
	CommonControlParams p_ebox{ ControlNames::ComboBox,L"Default Option",std::to_underlying(ControlIDList::ComboUntowered),
	pos_x + p_se.sz.width + 5, pos_y,WindowSize(150,1000) };
	winctrls.emplace_back(p_ebox, win, CommonControl::CommonStyles::ComboBoxDDL);

	//Add button to bring up CWT chart
	CommonControlParams p_cwt{ ControlNames::Button,L"CWT Reference",std::to_underlying(ControlIDList::ButtonCWT),
		p_ebox.posX + p_ebox.sz.width + 10,pos_y,WindowSize(130,30)};
	winctrls.emplace_back(p_cwt, win, CommonControl::CommonStyles::PushButton);

	pos_y += (p_se.sz.height + 20);

	for (auto& gt : alist.class_eg_untowered) {
		std::wstring wap = Win64Wrapper::convert_string(gt);
		SendMessage(GetDlgItem(win(), static_cast<int>(ControlIDList::ComboUntowered)), static_cast<UINT>(CB_ADDSTRING), 0_wp,
			PtrToLP(wap.c_str()));
	}
	//add artcc-wide stuff
	auto artcc_additions = chartaccessor->GetARTCCAdditions(artcc);
	if (!artcc_additions.empty()) {
		CommonControlParams p_saa{ ControlNames::Static,L"User Added ARTCC Content:",std::to_underlying(ControlIDList::StaticManualARTCC),
		pos_x,pos_y + 5,WindowSize(250,20) }; //the additional y is to align with the center of the combobox
		winctrls.emplace_back(p_saa, win, CommonControl::CommonStyles::StaticLeft);
		auto next_ctrl_pos = p_saa.posX + p_saa.sz.width + 5;
		auto uac_width = 400;
		if ((next_ctrl_pos + uac_width) > winsz.right) {
			uac_width = winsz.right - next_ctrl_pos;
		}
		CommonControlParams p_aabox{ ControlNames::ComboBox,L"Default Option",std::to_underlying(ControlIDList::ComboManualARTCC),
		next_ctrl_pos, pos_y,WindowSize(static_cast<int>(uac_width),100)};
		winctrls.emplace_back(p_aabox, win, CommonControl::CommonStyles::ComboBoxDDL);
		for (auto& aa : artcc_additions) {
			auto& title = aa.name;
			auto wtitle = Win64Wrapper::convert_string(title);
			SendMessage(GetDlgItem(win(), static_cast<int>(ControlIDList::ComboManualARTCC)), static_cast<UINT>(CB_ADDSTRING), 0_wp,
				PtrToLP(wtitle.c_str()));
		}
		pos_y += (p_saa.sz.height + 20);
	}
	return winsz.right;
}
void OpenAirportCharts(std::wstring airport,HWND main_window,LONG label_end_pos) {
	Win64Wrapper::WindowSize sz{ 1600,900 };
	Win64Wrapper::WindowStyles winstyles;
	using Win64Wrapper::CommonControl;
	using Win64Wrapper::ControlNames;
	using Win64Wrapper::WindowSize;
	using Win64Wrapper::CommonControlParams;
	using Win64Wrapper::PtrToLP;
	using namespace Win64Wrapper::literals;
	using charts::ChartType;
	Win64Wrapper::Window& win = Win64Wrapper::GetWindowFromHWND(main_window);
	auto& wc = control_list.at(main_window);
	CCContainer& cwinctrls = Sec(wc, Section::ChartControls);
	CommonControlParams p_sbc = {};
	CommonControlParams p_custom = {};
	//get the class b/c label for y positioning
	if (auto* cc = FindControl(wc, ControlIDList::StaticBC)) {
		p_sbc = cc->GetControlParams();
	}
	//get the custom charts button for airport label positioning
	if (auto* cc = FindControl(wc, ControlIDList::ButtonCustom)) {
		p_custom = cc->GetControlParams();
	}
	std::string airportstr = Win64Wrapper::convert_string(airport);
	auto ctypes = chartaccessor->GetAirportChartType(airportstr);
	std::ranges::sort(ctypes, [](ChartType cr_a, ChartType cr_b) {
		return std::to_underlying(cr_a) < std::to_underlying(cr_b);
		});
	auto [ctype_beg, ctype_end] = std::ranges::unique(ctypes);
	ctypes.erase(ctype_beg, ctype_end);

	//Airport/Other Charts
	int offx = label_end_pos + 10;
	std::wstring static_airport = L"Airport: " + airport;
	CommonControlParams p_acode{ControlNames::Static, static_airport.c_str(),std::to_underlying(ControlIDList::StaticAirportName), offx, p_custom.posY, WindowSize(100,20)};
	cwinctrls.emplace_back(p_acode, win, CommonControl::CommonStyles::StaticLeft);
	CommonControlParams p_sap{ ControlNames::Static,L"Airport Diagrams and Other Charts:",std::to_underlying(ControlIDList::StaticAirportsList),
		offx,p_sbc.posY,WindowSize(400,20) };
	cwinctrls.emplace_back(p_sap, win, CommonControl::CommonStyles::StaticLeft);

	CommonControlParams p_apbox{ ControlNames::ComboBox,L"Default Option",std::to_underlying(ControlIDList::ComboAp),
		offx,p_sap.posY + p_sap.sz.height + 5,WindowSize(400,100) };
	cwinctrls.emplace_back(p_apbox, win, CommonControl::CommonStyles::ComboBoxDDL);

	int next_y_pos = p_apbox.posY + p_apbox.sz.height + 5;

	//STARs
	if (std::ranges::find(ctypes, ChartType::STAR) != ctypes.end()) {
		CommonControlParams p_sstar{ ControlNames::Static,L"Standard Terminal Arrival Routes:",std::to_underlying(ControlIDList::StaticSTAR),
		offx,next_y_pos,WindowSize(400,20) };
		cwinctrls.emplace_back(p_sstar, win, CommonControl::CommonStyles::StaticLeft);

		CommonControlParams p_starbox{ ControlNames::ComboBox,L"Default Option",std::to_underlying(ControlIDList::ComboSTAR),
			offx,p_sstar.posY + p_sstar.sz.height + 5,WindowSize(400,100) };
		cwinctrls.emplace_back(p_starbox, win, CommonControl::CommonStyles::ComboBoxDDL);
		next_y_pos = p_starbox.posY + p_starbox.sz.height + 5;
	}
	//IAPs
	if (std::ranges::find(ctypes, ChartType::IAP) != ctypes.end()) {
		CommonControlParams p_siap{ ControlNames::Static,L"Instrument Approach Procedures (IAP):",std::to_underlying(ControlIDList::StaticIAP),
		offx,next_y_pos,WindowSize(400,20) };
		cwinctrls.emplace_back(p_siap, win, CommonControl::CommonStyles::StaticLeft);

		CommonControlParams p_iapbox{ ControlNames::ComboBox,L"Default Option",std::to_underlying(ControlIDList::ComboIAP),
			offx,p_siap.posY + p_siap.sz.height + 5,WindowSize(400,100) };
		cwinctrls.emplace_back(p_iapbox, win, CommonControl::CommonStyles::ComboBoxDDL);
		next_y_pos = p_iapbox.posY + p_iapbox.sz.height + 5;
	}
	//SIDs
	if ((std::ranges::find(ctypes, ChartType::SID) != ctypes.end()) || (std::ranges::find(ctypes, ChartType::ODP) != ctypes.end())) {
		CommonControlParams p_ssid{ ControlNames::Static,L"Standard Instrument Departures/Charted ODPs:",std::to_underlying(ControlIDList::StaticSID),
		offx,next_y_pos,WindowSize(400,20) };
		cwinctrls.emplace_back(p_ssid, win, CommonControl::CommonStyles::StaticLeft);

		CommonControlParams p_sidbox{ ControlNames::ComboBox,L"Default Option",std::to_underlying(ControlIDList::ComboSID),
			offx,p_ssid.posY + p_ssid.sz.height + 5,WindowSize(400,100) };
		cwinctrls.emplace_back(p_sidbox, win, CommonControl::CommonStyles::ComboBoxDDL);
		next_y_pos = p_sidbox.posY + p_sidbox.sz.height + 5;
	}
	if (std::ranges::find(ctypes, ChartType::MANUAL) != ctypes.end()) {
		CommonControlParams p_sman{ ControlNames::Static,L"User Added Charts:",std::to_underlying(ControlIDList::StaticManualARTCC),
				offx,next_y_pos,WindowSize(400,20) };
		cwinctrls.emplace_back(p_sman, win, CommonControl::CommonStyles::StaticLeft);

		CommonControlParams p_manbox{ ControlNames::ComboBox,L"Default Option",std::to_underlying(ControlIDList::ComboManual),
		offx,p_sman.posY + p_sman.sz.height + 5,WindowSize(400,100) };
		cwinctrls.emplace_back(p_manbox, win, CommonControl::CommonStyles::ComboBoxDDL);
	}
	//Fill Combo Boxes
	auto chartvec = chartaccessor->GetAirportCharts(airportstr);
	if (chartvec.empty()) return;
	for (auto& rec : chartvec) {
		std::wstring str = Win64Wrapper::convert_string(rec.procedure_name);
		const wchar_t* send_str = str.c_str();
//			OutputDebugString(std::format(L"Adding Chart: {}\n", send_str).c_str());
		if (rec.procedure_type == charts::ChartType::STAR) {
			auto ret = SendMessage(GetDlgItem(main_window, std::to_underlying(ControlIDList::ComboSTAR)),
				static_cast<UINT>(CB_ADDSTRING), 0_wp, PtrToLP(send_str));
			if (ret == CB_ERR || ret == CB_ERRSPACE) {
				OutputDebugString(std::format(L"Adding ComboBox charts failed. Code: {}\n", GetLastError()).c_str());
				return;
			}
		}
		else if (rec.procedure_type == charts::ChartType::IAP) {
			auto ret = SendMessage(GetDlgItem(main_window, std::to_underlying(ControlIDList::ComboIAP)),
				static_cast<UINT>(CB_ADDSTRING), 0_wp, PtrToLP(send_str));
			if (ret == CB_ERR || ret == CB_ERRSPACE) {
				OutputDebugString(std::format(L"Adding ComboBox charts failed. Code: {}\n", GetLastError()).c_str());
				return;
			}
		}
		else if (rec.procedure_type == charts::ChartType::SID || rec.procedure_type == charts::ChartType::ODP) {
			auto ret = SendMessage(GetDlgItem(main_window, std::to_underlying(ControlIDList::ComboSID)),
				static_cast<UINT>(CB_ADDSTRING), 0_wp, PtrToLP(send_str));
			if (ret == CB_ERR || ret == CB_ERRSPACE) {
				OutputDebugString(std::format(L"Adding ComboBox charts failed. Code: {}\n", GetLastError()).c_str());
				return;
			}
		}
		else if (rec.procedure_type == charts::ChartType::MANUAL) {
			auto ret = SendMessage(GetDlgItem(main_window, std::to_underlying(ControlIDList::ComboManual)),
				static_cast<UINT>(CB_ADDSTRING), 0_wp, PtrToLP(send_str));
			if (ret == CB_ERR || ret == CB_ERRSPACE) {
				OutputDebugString(std::format(L"Adding User-added ComboBox charts failed. Code: {}\n", GetLastError()).c_str());
				return;
			}
		}
		else {
			auto ret = SendMessage(GetDlgItem(main_window, std::to_underlying(ControlIDList::ComboAp)),
				static_cast<UINT>(CB_ADDSTRING), 0_wp, PtrToLP(send_str));
			if (ret == CB_ERR || ret == CB_ERRSPACE) {
				OutputDebugString(std::format(L"Adding ComboBox charts failed. Code: {}\n", GetLastError()).c_str());
				return;
			}
		}
	}

}

INT_PTR CustomChartProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	using namespace Win64Wrapper::literals;
	using Win64Wrapper::CheckCOMResult;
	using Win64Wrapper::GenericCOMPtr;
	using Win64Wrapper::PtrToLP;
	using Win64Wrapper::MessageBoxStyles;
	std::array<std::wstring, 4> crt_strs = { L"AirportItem",L"ARTCCItem",L"CWT",L"N/A"};
	const int AIRPORT_ITEM = 0, ARTCC_ITEM = 1, CWT = 2;
	static int lv_index = 0;
	static int lv_item_max_length = 512;
	auto get_cb_text = [](HWND box_handle) -> std::wstring {
		auto index = SendMessage(box_handle, static_cast<UINT>(CB_GETCURSEL), 0_wp, 0_lp);
		//No selection -> CB_ERR. Guard before CB_GETLBTEXTLEN, otherwise resize_and_overwrite((size_t)-1).
		if (index == CB_ERR) return std::wstring{};
		auto textlen = SendMessage(box_handle, static_cast<UINT>(CB_GETLBTEXTLEN), index, 0ll);
		if (textlen == CB_ERR) return std::wstring{};
		std::wstring selbuf;
		selbuf.resize_and_overwrite(textlen,
			[&box_handle, &index](wchar_t* buf, size_t sz) noexcept->std::size_t {
				auto ret = SendMessage(box_handle, static_cast<UINT>(CB_GETLBTEXT), index, PtrToLP(buf));
				if (ret == CB_ERR) return 0;
				else return ret;
			});
		return selbuf;
		};
	auto get_control_text = [&hdlg](int id) ->std::wstring {
		auto txtlen = GetWindowTextLength(GetDlgItem(hdlg, id));
		if (txtlen == 0) return std::wstring{};
		std::wstring buf;
		buf.resize_and_overwrite(++txtlen, [&hdlg, &id](wchar_t* buffer, size_t sz) {
			return GetDlgItemTextW(hdlg, id, buffer, static_cast<DWORD>(sz));
			});
		if (buf.empty()) return std::wstring{};
		return buf;
		};
	auto autoenable_remove_button = [&hdlg]() {
		auto rembutton = GetDlgItem(hdlg, IDC_REMOVE);
		auto clearbutton = GetDlgItem(hdlg, IDC_CLEAR);
		auto submitbutton = GetDlgItem(hdlg, IDC_LOAD);
		if (SendMessage(GetDlgItem(hdlg, IDC_CLIST), static_cast<UINT>(LVM_GETITEMCOUNT), 0_wp, 0_lp) > 0) {
			EnableWindow(rembutton, true);
			EnableWindow(clearbutton, true);
			EnableWindow(submitbutton, true);
		}
		else {
			EnableWindow(rembutton, false);
			EnableWindow(clearbutton, false);
			EnableWindow(submitbutton, false);
			lv_index = 0;
			chartaccessor->ClearAllAdditions();
		}
		};
	auto reset_controls = [&hdlg,&autoenable_remove_button]() {
		SetDlgItemText(hdlg, IDC_AP, nullptr);
		SetDlgItemText(hdlg, IDC_NAME, nullptr);
		SetDlgItemText(hdlg, IDC_PATH, nullptr);
		SendMessage(GetDlgItem(hdlg, IDC_TYPE), static_cast<UINT>(CB_SETCURSEL), SendMessage(GetDlgItem(hdlg, IDC_TYPE),
			static_cast<UINT>(CB_FINDSTRINGEXACT), -1, PtrToLP(L"User Added")), 0_lp);
		SendMessage(GetDlgItem(hdlg, IDC_CLASS), static_cast<UINT>(CB_SETCURSEL), SendMessage(GetDlgItem(hdlg, IDC_CLASS),
			static_cast<UINT>(CB_FINDSTRINGEXACT), -1, PtrToLP(L"E/G")), 0_lp);
		SendMessage(GetDlgItem(hdlg, IDC_RECTYPE), static_cast<UINT>(CB_SETCURSEL), -1, 0_lp);
		SendMessage(GetDlgItem(hdlg, IDC_ARTCC), static_cast<UINT>(CB_SETCURSEL), -1, 0_lp);
		EnableWindow(GetDlgItem(hdlg, IDC_RECTYPE), true);
		EnableWindow(GetDlgItem(hdlg, IDC_AP), false);
		EnableWindow(GetDlgItem(hdlg, IDC_ARTCC), false);
		EnableWindow(GetDlgItem(hdlg, IDC_TYPE), false);
		EnableWindow(GetDlgItem(hdlg, IDC_CLASS), false);
		EnableWindow(GetDlgItem(hdlg, IDC_NAME), false);
		EnableWindow(GetDlgItem(hdlg, IDC_PATH), false);
		EnableWindow(GetDlgItem(hdlg, IDC_ADD), false);
		CheckDlgButton(hdlg, IDC_CUSTOMAP, BST_UNCHECKED);
		EnableWindow(GetDlgItem(hdlg, IDC_CUSTOMAP), false);
		autoenable_remove_button();
		};
	std::unordered_map<std::wstring, std::wstring> charttype_conv_cb;
	charttype_conv_cb.insert({L"Airport Diagram",L"APD"});
	charttype_conv_cb.insert({L"SID",L"SID"});
	charttype_conv_cb.insert({L"STAR",L"STAR"});
	charttype_conv_cb.insert({L"Obstacle DP",L"ODP"});
	charttype_conv_cb.insert({L"Hot Spot",L"HOT"});
	charttype_conv_cb.insert({L"AAUP/DAU",L"DAU"});
	charttype_conv_cb.insert({L"Approach Chart",L"IAP"});
	charttype_conv_cb.insert({L"Takeoff Minimums",L"MIN"});
	charttype_conv_cb.insert({L"LAHSO Chart",L"LAH"});
	charttype_conv_cb.insert({L"User Added",L"MAN"});
	switch (msg) {
	case WM_INITDIALOG:
	{
		lv_index = 0;
		//initalize all the fixed combo boxes
		//Record Type:
		auto rc_cmb = GetDlgItem(hdlg, static_cast<int>(IDC_RECTYPE));
		SendMessage(rc_cmb, static_cast<UINT>(CB_ADDSTRING), 0_wp, PtrToLP(L"Airport Item"));
		SendMessage(rc_cmb, static_cast<UINT>(CB_ADDSTRING), 0_wp, PtrToLP(L"ARTCC Item"));
		SendMessage(rc_cmb, static_cast<UINT>(CB_ADDSTRING), 0_wp, PtrToLP(L"CWT Reference"));
		//Class
		auto cls_cmb = GetDlgItem(hdlg, static_cast<int>(IDC_CLASS));
		SendMessage(cls_cmb, static_cast<UINT>(CB_ADDSTRING), 0_wp, PtrToLP(L"B"));
		SendMessage(cls_cmb, static_cast<UINT>(CB_ADDSTRING), 0_wp, PtrToLP(L"C"));
		SendMessage(cls_cmb, static_cast<UINT>(CB_ADDSTRING), 0_wp, PtrToLP(L"D"));
		auto defpos = SendMessage(cls_cmb, static_cast<UINT>(CB_ADDSTRING), 0_wp, PtrToLP(L"E/G"));
		if (defpos != CB_ERR && defpos != CB_ERRSPACE) {
			//set default
			SendMessage(cls_cmb, static_cast<UINT>(CB_SETCURSEL), defpos, 0_lp);
			defpos = -1;
		}
		//Chart Type
		auto type_cmb = GetDlgItem(hdlg, static_cast<int>(IDC_TYPE));
		SendMessage(type_cmb, static_cast<UINT>(CB_ADDSTRING), 0_wp, PtrToLP(L"Airport Diagram"));
		SendMessage(type_cmb, static_cast<UINT>(CB_ADDSTRING), 0_wp, PtrToLP(L"SID"));
		SendMessage(type_cmb, static_cast<UINT>(CB_ADDSTRING), 0_wp, PtrToLP(L"STAR"));
		SendMessage(type_cmb, static_cast<UINT>(CB_ADDSTRING), 0_wp, PtrToLP(L"Obstacle DP"));
		SendMessage(type_cmb, static_cast<UINT>(CB_ADDSTRING), 0_wp, PtrToLP(L"Hot Spot"));
		SendMessage(type_cmb, static_cast<UINT>(CB_ADDSTRING), 0_wp, PtrToLP(L"AAUP/DAU"));
		SendMessage(type_cmb, static_cast<UINT>(CB_ADDSTRING), 0_wp, PtrToLP(L"Approach Chart"));
		SendMessage(type_cmb, static_cast<UINT>(CB_ADDSTRING), 0_wp, PtrToLP(L"Takeoff Minimums"));
		SendMessage(type_cmb, static_cast<UINT>(CB_ADDSTRING), 0_wp, PtrToLP(L"LAHSO Chart"));
		defpos = SendMessage(type_cmb, static_cast<UINT>(CB_ADDSTRING), 0_wp, PtrToLP(L"User Added"));
		if (defpos != CB_ERR && defpos != CB_ERRSPACE) {
			//set default
			SendMessage(type_cmb, static_cast<UINT>(CB_SETCURSEL), defpos, 0_lp);
			defpos = -1;
		}
		auto artcc_cmb = GetDlgItem(hdlg, static_cast<int>(IDC_ARTCC));
		for (auto& artcc : std::views::values(charts::artcc_names_map)) {
			auto wstr = Win64Wrapper::convert_string(artcc);
			SendMessage(artcc_cmb, static_cast<UINT>(CB_ADDSTRING), 0_wp, PtrToLP(wstr.c_str()));
		}
		//Set up ListView
		std::array<std::pair<std::wstring, int>, 7> column_headers = {
			std::make_pair<std::wstring, int>(L"Type",80),
			std::make_pair<std::wstring, int>(L"Name",400),
			std::make_pair<std::wstring, int>(L"Airport",80),
			std::make_pair<std::wstring, int>(L"ARTCC",80),
			std::make_pair<std::wstring, int>(L"Chart Type",80),
			std::make_pair<std::wstring, int>(L"Class",50),
			std::make_pair<std::wstring, int>(L"Path",300)
		};
		auto create_column = [](std::wstring& column_name,int width, int counter) {
			LVCOLUMNW lv{ 0 };
			lv.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
			lv.fmt = LVCFMT_CENTER;
			lv.cx = width;
			lv.pszText = column_name.data();
			lv.cchTextMax = static_cast<int>(column_name.size());
			lv.iSubItem = counter;
			return lv;
			};
		auto lv_hwnd = GetDlgItem(hdlg, IDC_CLIST);
		for (int i = 0;i < column_headers.size(); ++i) {
			auto& [txt, w] = column_headers.at(i);
			auto lvcol = create_column(txt, w, i);
			auto ret = SendMessage(lv_hwnd, static_cast<UINT>(LVM_INSERTCOLUMN), i, PtrToLP(&lvcol));
			if (ret == -1) {
				OutputDebugString(std::format(L"LV_INSERTITEM failure: {}\n", i).c_str());
			}
		}
		//check if there is anything in the xml file, and if so, populate the box
		auto xmlfilecontents = chartaccessor->GenerateListViewItems();
		if (!xmlfilecontents.empty()) {
			auto lv_handle = GetDlgItem(hdlg, IDC_CLIST);
			for (auto& r : xmlfilecontents) {
				LVITEM item = { 0 };
				item.mask = LVIF_TEXT;
				item.iItem = lv_index++;
				std::wstring convstr;
				LRESULT x;
				switch (r.rectype) {
				case charts::CustomRecordType::AirportItem:
				{
					item.pszText = crt_strs.at(0).data();
					auto x = SendMessage(lv_handle, static_cast<UINT>(LVM_INSERTITEM), 0_wp, PtrToLP(&item));
					item.iSubItem = 1;
					convstr = Win64Wrapper::convert_string(r.name.data());
					item.pszText = convstr.data();
					SendMessage(lv_handle, static_cast<UINT>(LVM_SETITEM), 0_wp, PtrToLP(&item));
					item.iSubItem = 2;
					convstr = Win64Wrapper::convert_string(r.airport.data());
					item.pszText = convstr.data();
					SendMessage(lv_handle, static_cast<UINT>(LVM_SETITEM), 0_wp, PtrToLP(&item));
					item.iSubItem = 3;
					convstr = Win64Wrapper::convert_string(charts::artcc_names_map.at(r.artcc));
					item.pszText = convstr.data();
					SendMessage(lv_handle, static_cast<UINT>(LVM_SETITEM), 0_wp, PtrToLP(&item));
					item.iSubItem = 4;
					for (auto& [txt, ct] : charts::charttype_names_map) {
						if (ct == r.ctype) {
							convstr = Win64Wrapper::convert_string(txt);
							break;
						}
					}
					item.pszText = convstr.data();
					SendMessage(lv_handle, static_cast<UINT>(LVM_SETITEM), 0_wp, PtrToLP(&item));
					item.iSubItem = 5;
					std::wstring clsstr(1, r.cls);
					item.pszText = clsstr.data();
					SendMessage(lv_handle, static_cast<UINT>(LVM_SETITEM), 0_wp, PtrToLP(&item));
					break;
				}
				case charts::CustomRecordType::ARTCCItem:
					item.pszText = crt_strs.at(1).data();
					x = SendMessage(lv_handle, static_cast<UINT>(LVM_INSERTITEM), 0_wp, PtrToLP(&item));
					item.iSubItem = 1;
					convstr = Win64Wrapper::convert_string(r.name.data());
					item.pszText = convstr.data();
					SendMessage(lv_handle, static_cast<UINT>(LVM_SETITEM), 0_wp, PtrToLP(&item));
					item.iSubItem = 3;
					convstr = Win64Wrapper::convert_string(charts::artcc_names_map.at(r.artcc));
					item.pszText = convstr.data();
					SendMessage(lv_handle, static_cast<UINT>(LVM_SETITEM), 0_wp, PtrToLP(&item));
					break;
				case charts::CustomRecordType::CWT:
					item.pszText = crt_strs.at(2).data();
					x = SendMessage(lv_handle, static_cast<UINT>(LVM_INSERTITEM), 0_wp, PtrToLP(&item));
				}
				item.iSubItem = 6;
				auto pthstr = r.filepath.wstring();
				item.pszText = pthstr.data();
				SendMessage(lv_handle, static_cast<UINT>(LVM_SETITEM), 0_wp, PtrToLP(&item));
				autoenable_remove_button();
			}
		}
		else {
			EnableWindow(GetDlgItem(hdlg, IDC_LOAD), false);
		}
		break;
	}
	case WM_COMMAND:
	{
		switch (HIWORD(wParam)) {
		case CBN_SELCHANGE:
		{
			//get text selection
			auto cmb = std::bit_cast<HWND>(lParam);
			std::wstring sel = get_cb_text(cmb);
			auto boxid = LOWORD(wParam);
			if (boxid == IDC_RECTYPE) {
				if (sel == L"CWT Reference") {
					EnableWindow(GetDlgItem(hdlg, IDC_BUTTONFILE), true);
					EnableWindow(GetDlgItem(hdlg, IDC_ADD), true);
					EnableWindow(GetDlgItem(hdlg, IDC_PATH), true);
					EnableWindow(GetDlgItem(hdlg, IDC_AP), false);
					EnableWindow(GetDlgItem(hdlg, IDC_ARTCC), false);
					EnableWindow(GetDlgItem(hdlg, IDC_TYPE), false);
					EnableWindow(GetDlgItem(hdlg, IDC_CLASS), false);
					EnableWindow(GetDlgItem(hdlg, IDC_NAME), false);
					CheckDlgButton(hdlg, IDC_CUSTOMAP, BST_UNCHECKED);
					EnableWindow(GetDlgItem(hdlg, IDC_CUSTOMAP), false);
				}
				else if (sel == L"ARTCC Item") {
					EnableWindow(GetDlgItem(hdlg, IDC_BUTTONFILE), true);
					EnableWindow(GetDlgItem(hdlg, IDC_ADD), true);
					EnableWindow(GetDlgItem(hdlg, IDC_PATH), true);
					EnableWindow(GetDlgItem(hdlg, IDC_AP), false);
					EnableWindow(GetDlgItem(hdlg, IDC_ARTCC), true);
					EnableWindow(GetDlgItem(hdlg, IDC_TYPE), false);
					EnableWindow(GetDlgItem(hdlg, IDC_CLASS), false);
					EnableWindow(GetDlgItem(hdlg, IDC_NAME), true);
					CheckDlgButton(hdlg, IDC_CUSTOMAP, BST_UNCHECKED);
					EnableWindow(GetDlgItem(hdlg, IDC_CUSTOMAP), false);
				}
				else if (sel == L"Airport Item") {
					EnableWindow(GetDlgItem(hdlg, IDC_BUTTONFILE), true);
					EnableWindow(GetDlgItem(hdlg, IDC_ADD), true);
					EnableWindow(GetDlgItem(hdlg, IDC_PATH), true);
					EnableWindow(GetDlgItem(hdlg, IDC_AP), true);
					EnableWindow(GetDlgItem(hdlg, IDC_TYPE), true);
					EnableWindow(GetDlgItem(hdlg, IDC_NAME), true);
					EnableWindow(GetDlgItem(hdlg, IDC_CUSTOMAP), true);
					//ARTCC + class are derived from the LID for a real airport; only a Custom Airport (a
					//fictional one) supplies them, so they unlock only when that box is checked.
					const bool custom = IsDlgButtonChecked(hdlg, IDC_CUSTOMAP) == BST_CHECKED;
					EnableWindow(GetDlgItem(hdlg, IDC_ARTCC), custom);
					EnableWindow(GetDlgItem(hdlg, IDC_CLASS), custom);
				}
			}
			break;
		}
		case BN_CLICKED:
		{
			auto lv_handle = GetDlgItem(hdlg, IDC_CLIST);
			switch (LOWORD(wParam)) {
			case IDC_CUSTOMAP:
			{
				//Toggle whether ARTCC/class are user-supplied. AUTOCHECKBOX has already flipped its state
				//by the time BN_CLICKED arrives, so just read it.
				const bool custom = IsDlgButtonChecked(hdlg, IDC_CUSTOMAP) == BST_CHECKED;
				EnableWindow(GetDlgItem(hdlg, IDC_ARTCC), custom);
				EnableWindow(GetDlgItem(hdlg, IDC_CLASS), custom);
				break;
			}
			case IDC_BUTTONFILE:
			{
				//Open File Dialog and Place the text in the edit box
				IFileOpenDialog* temp_fo_ptr = nullptr;
				IShellItem* temp_item = nullptr;
				try {
					CheckCOMResult(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileOpenDialog,
						reinterpret_cast<void**>(&temp_fo_ptr)));
					GenericCOMPtr<IFileOpenDialog> fileopen(temp_fo_ptr);
					temp_fo_ptr = nullptr;
					CheckCOMResult(fileopen->Show(hdlg));
					CheckCOMResult(fileopen->GetResult(&temp_item));
					GenericCOMPtr<IShellItem> resitem(temp_item);
					wchar_t* temp_buffer = nullptr;
					CheckCOMResult(resitem->GetDisplayName(SIGDN_FILESYSPATH, &temp_buffer));
					std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> fn_selected_buffer(temp_buffer, &CoTaskMemFree);
					temp_buffer = nullptr;
					SetDlgItemText(hdlg, IDC_PATH, fn_selected_buffer.get());
				}
				catch (_com_error& e) {
					if (e.Error() != HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
						Win64Wrapper::CreateMessageBox(std::format(L"Error in Open File Dialog. Error: {}", e.ErrorMessage()), L"COM Error",
							nullptr, MessageBoxStyles::Ok, MessageBoxStyles::DefaultButton1, MessageBoxStyles::IconError);
					}
				}
				break;
			}
			case IDC_ADD:
			{
				LVITEM item = { 0 };
				item.mask = LVIF_TEXT;
				item.iItem = lv_index++;
				item.iSubItem = 0;
				auto index = SendMessage(GetDlgItem(hdlg,IDC_RECTYPE), static_cast<UINT>(CB_GETCURSEL), 0_wp, 0_lp);
				std::wstring pth = get_control_text(IDC_PATH);
				if (pth.empty()) {
					Win64Wrapper::CreateMessageBox(L"Path required.", L"Custom Error", hdlg, MessageBoxStyles::Ok, MessageBoxStyles::DefaultButton1,
						MessageBoxStyles::IconError);
					return TRUE;
				}
				switch (index) {
				case AIRPORT_ITEM:
				{
					std::wstring name = get_control_text(IDC_NAME);
					std::wstring airport = get_control_text(IDC_AP);
					std::wstring typesel = get_cb_text(GetDlgItem(hdlg, IDC_TYPE));
					if (name.empty() || airport.empty() || typesel.empty()) {
						Win64Wrapper::CreateMessageBox(L"Airport, Name, and Type are required.", L"AirportItem Error", hdlg, MessageBoxStyles::Ok,
							MessageBoxStyles::DefaultButton1, MessageBoxStyles::IconError);
						--lv_index;
						return TRUE;
					}
					std::wstring ctype = charttype_conv_cb.at(typesel);
					//Normalize the identifier: upper-case, then strip an ICAO prefix (K/P + 3 letters) to the FAA LID.
					std::ranges::transform(airport, airport.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towupper(c)); });
					if (std::wsmatch mtch; std::regex_match(airport, mtch, std::wregex(LR"((K|P)([A-Z]{3}))"))) {
						airport = mtch[2].str();
					}
					//Real airport -> derive ARTCC/class from NASR. Custom (fictional) -> take them from the form,
					//but only if the LID is NOT a real airport (so fictional ids can't collide with real ones).
					const bool custom = IsDlgButtonChecked(hdlg, IDC_CUSTOMAP) == BST_CHECKED;
					auto real = chartaccessor->GetRealAirportByLID(Win64Wrapper::convert_string(airport));
					std::wstring artccstr, clsstr;
					if (custom) {
						if (real) {
							Win64Wrapper::CreateMessageBox(std::format(L"{} is a real FAA airport. Uncheck Custom Airport to add a chart for it.", airport),
								L"AirportItem Error", hdlg, MessageBoxStyles::Ok, MessageBoxStyles::DefaultButton1, MessageBoxStyles::IconError);
							--lv_index;
							return TRUE;
						}
						artccstr = get_cb_text(GetDlgItem(hdlg, IDC_ARTCC));
						clsstr = get_cb_text(GetDlgItem(hdlg, IDC_CLASS));
						if (artccstr.empty() || clsstr.empty()) {
							Win64Wrapper::CreateMessageBox(L"ARTCC and Class are required for a custom airport.", L"AirportItem Error", hdlg,
								MessageBoxStyles::Ok, MessageBoxStyles::DefaultButton1, MessageBoxStyles::IconError);
							--lv_index;
							return TRUE;
						}
					}
					else {
						if (!real) {
							Win64Wrapper::CreateMessageBox(std::format(L"{} is not a known FAA airport. Check Custom Airport to add a fictional one.", airport),
								L"AirportItem Error", hdlg, MessageBoxStyles::Ok, MessageBoxStyles::DefaultButton1, MessageBoxStyles::IconError);
							--lv_index;
							return TRUE;
						}
						artccstr = Win64Wrapper::convert_string(charts::artcc_names_map.at(real->artcc));
						const char cls = real->airspace_class;
						clsstr = (cls == 'B') ? L"B" : (cls == 'C') ? L"C" : (cls == 'D') ? L"D" : L"E/G";
					}
					//Everything validated -> commit the row. Insert at the actual end of the list and use the
					//index LVM_INSERTITEM returns for the subitems: lv_index can drift (the empty-path check, or
					//a prior validation bail, increments it without inserting), so reusing the stale item.iItem
					//would target a nonexistent row and leave every subitem blank.
					item.iItem = static_cast<int>(SendMessage(lv_handle, static_cast<UINT>(LVM_GETITEMCOUNT), 0_wp, 0_lp));
					item.iSubItem = 0;
					item.pszText = crt_strs.at(0).data();
					const auto row = static_cast<int>(SendMessage(lv_handle, static_cast<UINT>(LVM_INSERTITEM), 0_wp, PtrToLP(&item)));
					auto set_sub = [&](int sub, std::wstring& text) {
						item.iItem = row;
						item.iSubItem = sub;
						item.pszText = text.data();
						SendMessage(lv_handle, static_cast<UINT>(LVM_SETITEM), 0_wp, PtrToLP(&item));
					};
					set_sub(1, name);
					set_sub(2, airport);
					set_sub(3, artccstr);
					set_sub(4, ctype);
					set_sub(5, clsstr);
					set_sub(6, pth);
					autoenable_remove_button();
					break;
				}
				case ARTCC_ITEM:
				{
					LVITEM item = { 0 };
					item.mask = LVIF_TEXT;
					item.pszText = crt_strs.at(1).data();
					SendMessage(lv_handle, static_cast<UINT>(LVM_INSERTITEM), 0_wp, PtrToLP(&item));
					std::wstring name = get_control_text(IDC_NAME);
					std::wstring artccstr = get_cb_text(GetDlgItem(hdlg, IDC_ARTCC));
					if (name.empty() || artccstr.empty()) {
						Win64Wrapper::CreateMessageBox(L"ARTCC and Name required", L"ARTCCItem Error", hdlg, MessageBoxStyles::Ok, 
							MessageBoxStyles::DefaultButton1, MessageBoxStyles::IconError);
						return TRUE;
					}
					item.iSubItem = 1;
					item.pszText = name.data();
					SendMessage(lv_handle, static_cast<UINT>(LVM_SETITEM), 0_wp, PtrToLP(&item));
					item.iSubItem = 3;
					item.pszText = artccstr.data();
					SendMessage(lv_handle, static_cast<UINT>(LVM_SETITEM), 0_wp, PtrToLP(&item));
					item.iSubItem = 6;
					item.pszText = pth.data();
					SendMessage(lv_handle, static_cast<UINT>(LVM_SETITEM), 0_wp, PtrToLP(&item));
					autoenable_remove_button();
					break;
				}
				case CWT:
				{ //only one CWT is permitted
					LVITEM item = { 0 };
					item.mask = LVIF_TEXT;
					LVFINDINFO lfi = { 0 };
					lfi.flags = LVFI_STRING;
					lfi.psz = crt_strs.at(2).data();
					if (SendMessage(lv_handle, static_cast<UINT>(LVM_FINDITEM), -1, PtrToLP(&lfi)) != -1) {
						break;
					}
					item.pszText = crt_strs.at(2).data();
					SendMessage(lv_handle, static_cast<UINT>(LVM_INSERTITEM), 0_wp, PtrToLP(&item));
					item.iSubItem = 6;
					item.pszText = pth.data();
					SendMessage(lv_handle, static_cast<UINT>(LVM_SETITEM), 0_wp, PtrToLP(&item));
					autoenable_remove_button();
					break;
				}
				}
				reset_controls();
				break;
			}
			case IDC_REMOVE:
			{
				for (auto index = SendMessage(lv_handle, static_cast<UINT>(LVM_GETNEXTITEM), -1, LVNI_SELECTED); index != -1;) {
					SendMessage(lv_handle, static_cast<UINT>(LVM_DELETEITEM), index, 0_lp);
					--lv_index;
					index = SendMessage(lv_handle, static_cast<UINT>(LVM_GETNEXTITEM), -1, LVNI_SELECTED);
				}
				autoenable_remove_button();
				break;
			}
			case IDC_CLEAR:
				SendMessage(lv_handle, static_cast<UINT>(LVM_DELETEALLITEMS), 0_wp, 0_lp);
				autoenable_remove_button();
				break;
			case IDC_LOAD:
			{
				auto get_text = [&lv_handle](LVITEM& lvi, int subindex = 0) {
					std::wstring buf(lv_item_max_length,L' ');
					lvi.cchTextMax = lv_item_max_length;
					lvi.iSubItem = subindex;
					lvi.pszText = buf.data();
					auto retval = SendMessage(lv_handle, static_cast<UINT>(LVM_GETITEMTEXT), lvi.iItem, PtrToLP(&lvi));
					//LVM_GETITEMTEXT copies the text into buf and returns its actual length; trim to that. The
					//old code only trimmed when the listview redirected pszText (it doesn't here), so buf stayed
					//padded to lv_item_max_length, every cell compared unequal to crt_strs, and Load captured
					//nothing -> empty set -> the empty-overwrite guard fires.
					buf.resize(retval > 0 ? static_cast<size_t>(retval) : 0);
					return Win64Wrapper::convert_string(buf);
					};
				std::vector<charts::ManualXMLTag> xmlvec;
				for (auto index = SendMessage(lv_handle, static_cast<UINT>(LVM_GETNEXTITEM), -1, LVNI_ALL); index != -1;) {
					LVITEM item = { 0 };
					item.mask = LVIF_TEXT;
					item.iItem = index;
					auto rect = Win64Wrapper::convert_string(get_text(item));
					auto pth = get_text(item, 6);
					if (rect == crt_strs.at(0) || rect == crt_strs.at(1)) {
						auto artcc = charts::rev_artcc_names_map.at(get_text(item, 3));
						auto name = get_text(item, 1);
						if (rect == crt_strs.at(0)) {
							auto ap = get_text(item, 2);
							auto ctype = charts::charttype_names_map.at(get_text(item, 4));
							char cls = get_text(item, 5).at(0);
							xmlvec.emplace_back(charts::CustomRecordType::AirportItem, pth, name, artcc, ap, ctype, cls);
						}
						else {
							xmlvec.emplace_back(charts::CustomRecordType::ARTCCItem, pth, name, artcc);
						}
					}
					else if (rect == crt_strs.at(2)) {
						xmlvec.emplace_back(pth);
					}
					index = SendMessage(lv_handle, static_cast<UINT>(LVM_GETNEXTITEM), index, LVNI_ALL);
				}
				autoenable_remove_button();
				chartaccessor->WriteManualCharts(xmlvec);
				chartaccessor->ReloadManualCharts();
				Win64Wrapper::CreateMessageBox(L"Custom Chart Reload Completed", L"Custom Chart", hdlg);
			}
			}
			break;
		}
		}
		break;
	}
	case WM_CLOSE:
		//Destroy just the window here, not custdlg itself:
		DestroyWindow(hdlg);
		break;
	default:
		return FALSE;
	}
	return TRUE;
}