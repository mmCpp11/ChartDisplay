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
#include <windowsx.h>
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

//Application Specific Messages
//set the number of dynamic buttons for the window to render (unsigned int)
//WPARAM: number LPARAM: 0
#define WM_SETBTNDYNNUM (WM_APP + 0x0001)
//get the number of dynamic buttons currently set
//WPARAM: 0 LPARAM: pointer to unsigned long long to store the value
#define WM_GETBTNDYNNUM (WM_APP + 0x0002)
//clear the number of buttons WPARAM and LPARAM: 0
#define WM_CLEARBTNDYNNUM (WM_APP + 0x0003)
//transmit the colors of the buttons
//WPARAM: 0 LPARAM: pointer to std::unordered_map<std::wstring,COLORREF> with the button color and airport id
#define WM_BTNAPCLR (WM_APP + 0x0004)
//clear the loaded colors in the Window Proc to prepare for a new button set WPARAM and LPARAM: 0
#define WM_CLEARBTNAPCLR (WM_APP + 0x0005)
//index of the last position of the artcc-related controls to enable removing and redrawing the airport based ones
//WPARAM: index LPARAM: 0
#define WM_SETLASTNAPCC (WM_APP + 0x0006)

import std;
import BasicWindowsWrapperModule;
import Charts;

//Fixes for standard constructs not in intellisense
#ifdef __INTELLISENSE__
namespace std {
	namespace views = ranges::views;
}
std::size_t operator"" uz(unsigned long long p) { return p; }
#endif

using NumButtonsType = std::size_t;
using ApMsgMap = std::unordered_map<std::wstring, COLORREF>;

LRESULT ExtraWindowProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CustomChartProc(HWND, UINT, WPARAM, LPARAM);

charts::FAAChartProcessor* chartaccessor = nullptr;
using CommonControlMap = std::unordered_map<HWND, std::vector<Win64Wrapper::CommonControl>>;
using CCContainer = CommonControlMap::mapped_type;
CommonControlMap control_list;
std::unique_ptr < std::remove_pointer_t<HWND>, decltype([](HWND ptr) {if (ptr) DestroyWindow(ptr);}) > custdlg;
enum class ControlIDList : WORD {
	StaticARTCC = 101,
	StaticBC,
	ComboARTCC,
	ButtonForceUpdate,
	ButtonReload,
	ButtonCustom,
	ButtonCWT,
	CheckBoxAutoupdate,
	StaticD,
	StaticET,
	StaticE,
	ComboUntowered,
	ComboManualARTCC,
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
	DynamicButtonStart=500,
	Dummy=1000
};

//border_ctrl_id is the position of the lowest control not wiped
void ClearWindowARTCC(const Win64Wrapper::Window& win);
//returns max width of the button/combo box are, the rest of the screen is for airport specifics
LONG PopulateWindowARTCC(const Win64Wrapper::Window& win, const charts::ARTCC artcc,const ControlIDList border_ctrl_id);
LONG DrawDynamicARTCC(const Win64Wrapper::Window& win, const charts::ARTCC artcc, const ControlIDList border_ctl_id) {
	ClearWindowARTCC(win);
	return PopulateWindowARTCC(win, artcc, border_ctl_id);
}
void OpenAirportCharts(std::wstring, HWND,LONG);

int wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(nCmdShow);
	//deal with command line
	std::wstring_view cmdline(lpCmdLine);
	std::optional<std::filesystem::path> exepath;
	auto helpmessage = []() {
		Win64Wrapper::CreateMessageBox(L"Usage: ChartDisplay.exe [--downloader=path-to-ChartDisplayDownloadHelper.exe]", L"Help");
		};
	if (cmdline.contains(L"--help") || cmdline.contains(L"-h")) {
		helpmessage();
		return 0;
	}
	else if (cmdline.contains(L"--downloader=")) {
		auto p = cmdline.find(L"downloader");
		auto pe = cmdline.find_first_of(L'=', p);
		++pe;
		auto s = cmdline.find_first_of(L' ', pe);
		exepath = cmdline.substr(pe, s - pe);
	}
	charts::FAAChartProcessor chart(exepath);
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
		Win64Wrapper::ControlNames::ListView});
	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.hInstance = hInstance;
	wc.lpszClassName = L"MainWinClass";
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.hCursor = reinterpret_cast<HCURSOR>(LoadImage(NULL, MAKEINTRESOURCE(IDC_ARROW), IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE | LR_SHARED));
	if (!wc.hCursor) {
		OutputDebugString(std::format(L"LoadImage error with ID: {}, GLE Code: {}\n", 32512, GetLastError()).c_str());
	}
	//if the following line isn't included, WM_PAINT must be used at all times to redraw the window
	wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	Win64Wrapper::WindowSize sz{ 1623,933 }; //old size 831x595
	Win64Wrapper::WindowStyles wstyle;
	wstyle.extended_styles = WS_EX_APPWINDOW;
	Win64Wrapper::Window win(wc, &ExtraWindowProc, sz, L"FAA Chart Display", true, Win64Wrapper::WindowLogger(L"log.txt"),wstyle);
	auto res=win.DisplayWindow();
	if (res == false) {
		return 1;
	}

	MSG msg;
	auto acceltable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CHARTDISPLAY));
	while (GetMessage(&msg, NULL, 0, 0) > 0) {
		if (auto accmsg = TranslateAccelerator(msg.hwnd,acceltable,&msg);
			( accmsg==0 && !IsDialogMessage(custdlg.get(), &msg))) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return 0;
}
//handle specific messages first, then run DefWindowProc on specific messages (with defwinproc = true).
//If DefWindowProc inhibited return 0 to continue message processing
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
	static NumButtonsType num_buttons{};
	static ApMsgMap airport_button_colors{};
	static long button_border_width = {};
	static std::wstring last_airport_clicked = {};
	static size_t artcc_control_index = 0;
	auto ResetARTCCControls = [](CCContainer& winctrls) {
		if (artcc_control_index == 0) return;
		auto first_erased_pos = artcc_control_index + 1;
		if (winctrls.size() <= first_erased_pos) return;
		winctrls.erase(std::next(winctrls.begin(), first_erased_pos), winctrls.end());
		};
	auto OpenFileDefault = [](const std::filesystem::path& pth) {
		namespace fs = std::filesystem;
		fs::path viewerpath;
		std::wstring commandline;
		auto extension = pth.extension().native();
		DWORD nec_buf_size = {};
		std::wstring exe_fpath;
		AssocQueryString(ASSOCF_NONE, ASSOCSTR_EXECUTABLE, extension.c_str(), L"open", nullptr, &nec_buf_size);
		if (nec_buf_size == 0) {
			Win64Wrapper::CreateMessageBox(
				std::format(L"Please associate an application with the {} extension.", extension),
				L"Error Opening Chart", nullptr, Win64Wrapper::MessageBoxStyles::Ok,
				Win64Wrapper::MessageBoxStyles::DefaultButton1, Win64Wrapper::MessageBoxStyles::IconError);
			throw fs::filesystem_error("Could not find associated application for filename",
				std::error_code(static_cast<int>(std::errc::no_such_file_or_directory),std::system_category()));
		}
		exe_fpath.resize_and_overwrite(nec_buf_size, [&nec_buf_size, extension](wchar_t* buffer, size_t sz) -> size_t {
			DWORD newsz = nec_buf_size;
			auto comres = AssocQueryString(ASSOCF_NONE, ASSOCSTR_EXECUTABLE, extension.c_str(), L"open", buffer, &newsz);
			if (comres == S_OK) return newsz;
			else return 0;
			});
		exe_fpath.pop_back(); //remove unnecessary null terminator
		viewerpath = exe_fpath;
		auto command_line = std::format(LR"("{}" "{}")", viewerpath.filename().native(), pth.native());
		Win64Wrapper::ProcCreationInfo pi;
		auto ret = Win64Wrapper::CreateChildProcess(viewerpath, command_line, pi, true,
			std::to_underlying(Win64Wrapper::ProcCreationFlags::NoConsoleNormal));
		};
	switch (msg) {
	case WM_CREATE:
	{
		Win64Wrapper::Window& winclass = Win64Wrapper::GetWindowFromHWND(hwnd);
		//ARTCC static control label
		int lloffsetx = 10, lloffsety = 10;
		CCContainer winctrls;
		CommonControlParams p_sartcc{ ControlNames::Static,L"ARTCC:",std::to_underlying(ControlIDList::StaticARTCC),lloffsetx,lloffsety + 5,WindowSize(70,20) };
		winctrls.emplace_back(p_sartcc, winclass, CommonControl::CommonStyles::StaticLeft);

		//ARTCC combo box
		CommonControlParams p_artccbox{ ControlNames::ComboBox,L"Default Option",std::to_underlying(ControlIDList::ComboARTCC),
			lloffsetx + p_sartcc.sz.width + 10, lloffsety,WindowSize(150,1000) };
		winctrls.emplace_back(p_artccbox, winclass, CommonControl::CommonStyles::ComboBoxDDL);
		//Force Update Button
		CommonControlParams p_update{ ControlNames::Button,L"Force Chart Update",std::to_underlying(ControlIDList::ButtonForceUpdate),
			p_artccbox.posX + p_artccbox.sz.width + 10,lloffsety,WindowSize(180,30) };
		winctrls.emplace_back(p_update, winclass, CommonControl::CommonStyles::PushButton);
		//Autoupdate box
		CommonControlParams p_autoupdate{ ControlNames::Button,L"Autoupdate on start",std::to_underlying(ControlIDList::CheckBoxAutoupdate),
		p_update.posX,p_update.posY + p_update.sz.height + 5,WindowSize(180,30) };
		winctrls.emplace_back(p_autoupdate, winclass, CommonControl::CommonStyles::CheckBox);
		//ReloadCharts box
		CommonControlParams p_reload{ ControlNames::Button,L"Reload Charts",std::to_underlying(ControlIDList::ButtonReload),
		p_update.posX + p_update.sz.width + 5,p_update.posY,WindowSize(140,30) };
		winctrls.emplace_back(p_reload, winclass, CommonControl::CommonStyles::PushButton);
		//Reload User-Added Charts
		CommonControlParams p_rmanual{ ControlNames::Button,L"Custom Charts",std::to_underlying(ControlIDList::ButtonCustom),
		p_reload.posX + p_reload.sz.width + 5,p_reload.posY,WindowSize(140,30) };
		winctrls.emplace_back(p_rmanual, winclass, CommonControl::CommonStyles::PushButton);
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
		control_list.insert(std::make_pair(hwnd, std::move(winctrls)));
	}
		break;
	case WM_DESTROY:
	{
		defwinproc = true;
		if (IsDlgButtonChecked(hwnd, std::to_underlying(ControlIDList::CheckBoxAutoupdate))) {
			chartaccessor->AutoupdateState(true);
		}
		else {
			chartaccessor->AutoupdateState(false);
		}
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
		auto wincolor = static_cast<COLORREF>(GetSysColor(COLOR_WINDOW));
		auto sc_devc = reinterpret_cast<HDC>(wParam);
		SetTextColor(sc_devc, GetSysColor(COLOR_WINDOWTEXT));
		SetBkColor(sc_devc, wincolor);
		static HBRUSH st_bkgrd_brush = GetSysColorBrush(COLOR_WINDOW);
		return reinterpret_cast<LRESULT>(st_bkgrd_brush);
		break;
	}
	case WM_SIZE:
	{
		CCContainer& ctrls = control_list.at(hwnd);
		for (auto& cc : ctrls) {
			auto id = cc.GetControlParams().id;
			if (static_cast<ControlIDList>(id) == ControlIDList::ComboARTCC) {
				SendMessage(hwnd, static_cast<UINT>(WM_COMMAND), MAKEWPARAM(id, CBN_SELCHANGE), PtrToLP(cc()));
			}
			if (id == std::to_underlying(ControlIDList::ComboAp)) {
				OpenAirportCharts(last_airport_clicked, hwnd, button_border_width);
			}
		}
		break;
	}
	case WM_DRAWITEM:
	{
		auto draw_params = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam);
		auto& winctrls = control_list.at(hwnd);
		for (auto& cc : winctrls) {
			if (cc.GetControlParams().id == draw_params->CtlID) {
				if (draw_params->CtlType == ODT_BUTTON) {
					SIZE sz{};
					std::wstring text = cc.GetControlParams().text;
					GetTextExtentPoint32(draw_params->hDC, text.c_str(), static_cast<int>(text.size()), &sz);
					SetTextColor(draw_params->hDC, airport_button_colors.at(text));
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
				break;
			}
		}
		break;
	}
	case WM_COMMAND:
	{
		auto ctl_id = LOWORD(wParam);
		switch (HIWORD(wParam)) {
		case BN_CLICKED:
		{
			auto button_id_top = std::to_underlying(ControlIDList::DynamicButtonStart) + num_buttons;
			if (ctl_id == std::to_underlying(ControlIDList::ButtonForceUpdate)) {
				if (chartaccessor) {
					auto check = Win64Wrapper::CreateMessageBox(L"Confirm Chart Upgrade?", L"Chart Upgrade", hwnd,
						MessageBoxStyles::AppModal, MessageBoxStyles::IconWarning, MessageBoxStyles::DefaultButton2, MessageBoxStyles::YesNo);
					//If anything goes wrong in the creation or display of the Message Box, assume a No to prevent accidental updating of charts.
					if (check == Win64Wrapper::MessageBoxResponse::Yes) {
						chartaccessor->UpdateCharts(false,true);
						Win64Wrapper::CreateMessageBox(L"Full chart updating and processing complete", L"Force Chart Update", hwnd);
					}
				}
			}
			//reload button
			else if (ctl_id == std::to_underlying(ControlIDList::ButtonReload)) {
				if (chartaccessor) {
					chartaccessor->UpdateCharts(true);
					Win64Wrapper::CreateMessageBox(L"Full chart reload complete", L"Chart Reload", hwnd);
				}
			}
			//reload custom charts
			else if (ctl_id == std::to_underlying(ControlIDList::ButtonCustom)) {
				auto hinst = reinterpret_cast<HINSTANCE>(GetWindowLongPtr(hwnd, GWLP_HINSTANCE));
				custdlg.reset(CreateDialogW(hinst, MAKEINTRESOURCEW(IDD_CUSTCHART), hwnd, &CustomChartProc));
			}
			//CWT button
			else if (ctl_id == std::to_underlying(ControlIDList::ButtonCWT)) {
				auto cwtpath = chartaccessor->GetCWTItemPath();
				if (cwtpath) {
					OpenFileDefault(cwtpath.value());
				}
			}
			//handle airport buttons
			else if (ctl_id >= std::to_underlying(ControlIDList::DynamicButtonStart) && ctl_id < button_id_top) {
				auto& winctrls = control_list.at(hwnd);
				ResetARTCCControls(winctrls);
				for (auto& i : winctrls) {
					if (i.GetControlParams().id == ctl_id) {
						last_airport_clicked = i.GetControlParams().text;
						OpenAirportCharts(last_airport_clicked, hwnd, button_border_width);
						break;
					}
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
			auto cmb = reinterpret_cast<HWND>(lParam);
			unsigned short index = SendMessage(cmb, static_cast<UINT>(CB_GETCURSEL), 0_wp, 0LL);
			std::wstring selbuf;
			selbuf.resize_and_overwrite(SendMessage(cmb, static_cast<UINT>(CB_GETLBTEXTLEN), index, 0ll), [&cmb, &index](wchar_t* buf, size_t sz) noexcept
				->std::size_t {
					auto ret = SendMessage(cmb, static_cast<UINT>(CB_GETLBTEXT), index, PtrToLP(buf));
					if (ret == CB_ERR) return 0;
					else return ret;
				});
			auto boxid = LOWORD(wParam);
			std::array<ControlIDList, 5> chart_control_ids = { ControlIDList::ComboAp,ControlIDList::ComboSTAR,ControlIDList::ComboIAP,
				ControlIDList::ComboSID,ControlIDList::ComboManual };
			std::string sel = Win64Wrapper::convert_string(selbuf);
			//ARTCC selection change
			if (boxid == std::to_underlying(ControlIDList::ComboARTCC)) {
				const Win64Wrapper::Window& winclass = Win64Wrapper::GetWindowFromHWND(hwnd);
				last_airport_clicked.clear();
				if (sel == "Other") {
					sel = "ZAE";
				}
				button_border_width = DrawDynamicARTCC(winclass, charts::rev_artcc_names_map.at(sel), ControlIDList::StaticARTCC);
			}
			else if (boxid == std::to_underlying(ControlIDList::ComboUntowered)) {
				auto& winctrls = control_list.at(hwnd);
				ResetARTCCControls(winctrls);
				last_airport_clicked = selbuf;
				OpenAirportCharts(selbuf, hwnd,button_border_width);
			}
			else if (boxid == std::to_underlying(ControlIDList::ComboManualARTCC)) {
				//get artcc selection
				auto artcc_combo = GetDlgItem(hwnd, static_cast<int>(ControlIDList::ComboARTCC));
				unsigned short ai = SendMessage(artcc_combo,static_cast<UINT>(CB_GETCURSEL), 0_wp, 0_lp);
				std::wstring artccstr;
				artccstr.resize_and_overwrite(SendMessage(artcc_combo, static_cast<UINT>(CB_GETLBTEXTLEN), ai, 0_lp),
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
				auto acharts = chartaccessor->GetAirportCharts(Win64Wrapper::convert_string(last_airport_clicked));
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
	//*************************
	//Custom Messages
	case WM_CLEARBTNDYNNUM:
		num_buttons = 0;
		break;
	case WM_GETBTNDYNNUM:
	{
		auto* numbuf = reinterpret_cast<NumButtonsType*>(lParam);
		*numbuf = num_buttons;
		break;
	}
	case WM_SETBTNDYNNUM:
	{
		num_buttons = static_cast<NumButtonsType>(wParam);
		break;
	}
	case WM_BTNAPCLR:
		airport_button_colors = *reinterpret_cast<ApMsgMap*>(lParam);
		break;
	case WM_CLEARBTNAPCLR:
		airport_button_colors.clear();
		break;
	case WM_SETLASTNAPCC:
		artcc_control_index = wParam;
	//**************************
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	//run defwinproc for specific messages where default behavior is also wanted
	if (defwinproc) {
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	else return 0;
}
//StaticARTCC = 101,
//StaticBC,
//ComboARTCC,
//ButtonForceUpdate,
//ButtonReload,
//ButtonCustom,
//CheckBoxAutoupdate,
void ClearWindowARTCC(const Win64Wrapper::Window& win) {
	using Win64Wrapper::CommonControl;
	using namespace Win64Wrapper::literals;
	//erase all controls except for the ARTCC combo box and label and the Force Upgrade button (which are the first 3 in the vector
	auto& ctrls = control_list.at(win());
	std::erase_if(ctrls, [](const CommonControl& cc) {
		auto enum_id = static_cast<ControlIDList>(cc.GetControlParams().id);
		std::array<ControlIDList, 7> ids_to_keep = { ControlIDList::StaticARTCC,ControlIDList::ComboARTCC,ControlIDList::ButtonForceUpdate,
			ControlIDList::ButtonReload,ControlIDList::ButtonCustom,ControlIDList::CheckBoxAutoupdate };
		if (std::ranges::find(ids_to_keep, enum_id) != ids_to_keep.end()) {
			return false;
		}
		else return true;
		});
	SendMessage(win(), static_cast<UINT>(WM_CLEARBTNDYNNUM), 0_wp, 0_lp);
	SendMessage(win(), static_cast<UINT>(WM_CLEARBTNAPCLR), 0_wp, 0_lp);
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
	CCContainer& winctrls = control_list.at(win());
	CommonControlParams borderparams = {};
	for (auto& i : winctrls) {
		auto ccpar = i.GetControlParams();
		if (ccpar.id == std::to_underlying(border_ctrl_id)) {
			borderparams = ccpar;
			break;
		}
	}
	if (borderparams.id == 0) return 0;
	//Get Airport list and set number of buttons needed (all class b, c and d airports)
	auto alist = chartaccessor->GetAirspaceClassInfo(artcc);
	SendMessage(win(), static_cast<UINT>(WM_SETBTNDYNNUM),
		alist.class_b_airports.size() + alist.class_c_airports.size() + alist.class_d_airports.size(), 0_lp);
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
	//Send the Colors to the WindowProc
	SendMessage(win(), static_cast<UINT>(WM_BTNAPCLR), 0_wp, PtrToLP(&btn_colors));
	//Class B and C static control label
	CommonControlParams p_sbc{ ControlNames::Static,L"Class B and C Airports:",std::to_underlying(ControlIDList::StaticBC),
		borderparams.posX,borderparams.posY + 50,WindowSize(190,20) };
	winctrls.emplace_back(p_sbc , win , CommonControl::CommonStyles::StaticLeft);
	// Dynamic Buttons
	NumButtonsType num_buttons;
	SendMessage(win(), static_cast<UINT>(WM_GETBTNDYNNUM), 0, PtrToLP(&num_buttons));
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
	//send the message to set the artcc-wide controls to make re-drawing airports easier
	SendMessage(win(), static_cast<UINT>(WM_SETLASTNAPCC), winctrls.size() - 1, 0_lp);
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
	CCContainer& cwinctrls = control_list.at(main_window);
	CommonControlParams p_sbc = {};
	for (auto& cc : cwinctrls) {
		auto cc_params = cc.GetControlParams();
		if (cc_params.id == std::to_underlying(ControlIDList::StaticBC)) {
			p_sbc = cc_params;
			break;
		}
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
		else if (rec.procedure_type == charts::ChartType::SID || rec.procedure_type == charts::ChartType::SID) {
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
		std::wstring selbuf;
		selbuf.resize_and_overwrite(SendMessage(box_handle, static_cast<UINT>(CB_GETLBTEXTLEN), index, 0ll),
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
			return GetDlgItemTextW(hdlg, id, buffer, sz);
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
			auto cmb = reinterpret_cast<HWND>(lParam);
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
				}
				else if (sel == L"Airport Item") {
					EnableWindow(GetDlgItem(hdlg, IDC_BUTTONFILE), true);
					EnableWindow(GetDlgItem(hdlg, IDC_ADD), true);
					EnableWindow(GetDlgItem(hdlg, IDC_PATH), true);
					EnableWindow(GetDlgItem(hdlg, IDC_AP), true);
					EnableWindow(GetDlgItem(hdlg, IDC_ARTCC), true);
					EnableWindow(GetDlgItem(hdlg, IDC_TYPE), true);
					EnableWindow(GetDlgItem(hdlg, IDC_CLASS), true);
					EnableWindow(GetDlgItem(hdlg, IDC_NAME), true);
				}
			}
			break;
		}
		case BN_CLICKED:
		{
			auto lv_handle = GetDlgItem(hdlg, IDC_CLIST);
			switch (LOWORD(wParam)) {
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
					item.pszText = crt_strs.at(0).data();
					SendMessage(lv_handle, static_cast<UINT>(LVM_INSERTITEM), 0_wp, PtrToLP(&item));
					std::wstring name = get_control_text(IDC_NAME);
					std::wstring artccstr = get_cb_text(GetDlgItem(hdlg, IDC_ARTCC));
					std::wstring airport = get_control_text(IDC_AP);
					std::wstring ctype = charttype_conv_cb.at(get_cb_text(GetDlgItem(hdlg, IDC_TYPE)));
					std::wstring clsstr = get_cb_text(GetDlgItem(hdlg, IDC_CLASS));
					if (name.empty() || artccstr.empty() || airport.empty()) {
						Win64Wrapper::CreateMessageBox(L"Airport, ARTCC, and Name required", L"AirportItem Error", hdlg, MessageBoxStyles::Ok,
							MessageBoxStyles::DefaultButton1, MessageBoxStyles::IconError);
						return TRUE;
					}
					//get rid of ICAO
					std::wregex icao_regex(LR"((K|P)([A-Za-z]{3}))");
					std::wsmatch mtch;
					if (std::regex_match(airport, mtch, icao_regex)) {
						airport = mtch[2].str();
					}
					item.iSubItem = 1;
					item.pszText = name.data();
					SendMessage(lv_handle, static_cast<UINT>(LVM_SETITEM), 0_wp, PtrToLP(&item));
					item.iSubItem = 2;
					item.pszText = airport.data();
					SendMessage(lv_handle, static_cast<UINT>(LVM_SETITEM), 0_wp, PtrToLP(&item));
					item.iSubItem = 3;
					item.pszText = artccstr.data();
					SendMessage(lv_handle, static_cast<UINT>(LVM_SETITEM), 0_wp, PtrToLP(&item));
					item.iSubItem = 4;
					item.pszText = ctype.data();
					SendMessage(lv_handle, static_cast<UINT>(LVM_SETITEM), 0_wp, PtrToLP(&item));
					item.iSubItem = 5;
					item.pszText = clsstr.data();
					SendMessage(lv_handle, static_cast<UINT>(LVM_SETITEM), 0_wp, PtrToLP(&item));
					item.iSubItem = 6;
					item.pszText = pth.data();
					SendMessage(lv_handle, static_cast<UINT>(LVM_SETITEM), 0_wp, PtrToLP(&item));
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
					if (lvi.pszText != buf.data()) {
						buf.assign(lvi.pszText);
					}
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
							xmlvec.emplace_back(charts::CustomRecordType::AirportItem, pth, name, artcc, ap, ctype, static_cast<wchar_t>(cls));
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
		custdlg.reset();
		break;
	default:
		return FALSE;
	}
	return TRUE;
}