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
module;

#ifdef _MSVC_LANG
#if _MSVC_LANG < 202302L
#error "Requires C++23"
#endif
#elif __cplusplus < 202302L
#error "Requires C++23"
#endif

#include <sqlite3.h>
#include <sqlite_orm/sqlite_orm.h>
#include <zip.h>
#pragma warning(disable:5260) //get rid of warnings for csv.hpp as an external library forced into a module

module Charts;

import std;
import BasicWindowsWrapperModule;
import Downloader;
import "csv.hpp";
import "pugixml.hpp";

namespace Net = Win64Wrapper::Net;

//necessary windows stuff TODO: implement progress bar for chart downloading
//struct HINSTANCE__ { int unused; }; typedef struct HINSTANCE__* HINSTANCE;
//struct HWND__ { int unused; }; typedef struct HWND__* HWND;
//typedef long long INT_PTR;
//typedef unsigned int UINT;
//typedef unsigned long long WPARAM;
//typedef long long LPARAM;

#ifdef __INTELLISENSE__
//fix for Intellisense, suppress invalid intellisense errors
//not sure why intellisense doesn't have this alias which is in the standard
namespace std {
	namespace views = ranges::views;
}
#pragma diag_suppress 304 //std::chrono::floor expression is valid
#pragma diag_suppress 165 //DoDownload has a default argument in the prototype in the module interface
#endif

namespace fs = std::filesystem;
namespace view = std::views;
namespace sql = sqlite_orm;

//sqlite_orm enum conversion functions (to string):
namespace SQLiteORM_conv_func {
	template<typename T>
	std::string to_string(T e) noexcept { return "invalid_enum"; }
	template<typename T>
	std::optional<T> from_string(const std::string& str) noexcept { return std::nullopt; }

	//for ARTCC
	template<>
	std::string to_string(charts::ARTCC a) noexcept {
		return charts::artcc_names_map.at(a);
	}
	template<>
	std::optional<charts::ARTCC> from_string(const std::string& str) noexcept {
		try {
			auto name = charts::rev_artcc_names_map.at(str);
			return name;
		}
		catch (std::out_of_range&) {
			return std::nullopt;
		}
	}
	//for ChartType
	const std::unordered_map<charts::ChartType, std::string> chart_type_str{
		{charts::ChartType::APD,"APD"},
		{charts::ChartType::SID,"SID"},
		{charts::ChartType::STAR,"STAR"},
		{charts::ChartType::ODP, "ODP"},
		{charts::ChartType::HOT,"HOT"},
		{charts::ChartType::DAU,"AAUP"},
		{charts::ChartType::IAP,"IAP"},
		{charts::ChartType::MIN,"MIN"},
		{charts::ChartType::LAHSO,"LAH"},
		{charts::ChartType::MANUAL,"MAN"}
	};
	const std::unordered_map<std::string, charts::ChartType> rev_chart_type_str{
		{"APD",charts::ChartType::APD},
		{"SID",charts::ChartType::SID},
		{"STAR",charts::ChartType::STAR},
		{"ODP",charts::ChartType::ODP},
		{"HOT",charts::ChartType::HOT},
		{"AAUP",charts::ChartType::DAU},
		{"IAP",charts::ChartType::IAP},
		{"MIN",charts::ChartType::MIN},
		{"LAH",charts::ChartType::LAHSO},
		{"MAN",charts::ChartType::MANUAL}
	};
	template<>
	std::string to_string(charts::ChartType c) noexcept {
		return chart_type_str.at(c);
	}
	template<>
	std::optional<charts::ChartType> from_string(const std::string& str) noexcept {
		try {
			auto name = rev_chart_type_str.at(str);
			return name;
		}
		catch (std::out_of_range&) {
			return std::nullopt;
		}
	}
	//for std::chrono::year_month_day
	template<>
	std::string to_string(std::chrono::year_month_day e) {
		std::string str = std::format("{:%F}", e);
		return str;
	}
	template<>
	std::optional<std::chrono::year_month_day> from_string(const std::string& str) noexcept {
		try {
			std::chrono::year_month_day ymd = charts::chrono_parse(str, "%F", std::chrono::year_month_day{});
			return std::optional<std::chrono::year_month_day>(ymd);
		}
		catch (std::exception&) {
			return std::nullopt;
		}
	}
	//for std::filesystem::path
	template<>
	std::string to_string(fs::path pt) {
		return pt.string();
	}
	template<>
	std::optional<fs::path> from_string(const std::string& pt) {

		auto fpth = fs::path(pt);
		return fpth;
	}
}
namespace sqlite_orm {
	//ARTCC
	template<>
	struct type_printer<charts::ARTCC> : public text_printer {};
	template<>
	struct statement_binder<charts::ARTCC> {
		int bind(sqlite3_stmt* stmt, int index, const charts::ARTCC& value) {
			return statement_binder<std::string>().bind(stmt, index, SQLiteORM_conv_func::to_string(value));
			//  or return sqlite3_bind_text(stmt, index++, GenderToString(value).c_str(), -1, SQLITE_TRANSIENT);
		}
	};
	template<>
	struct field_printer<charts::ARTCC> {
		std::string operator()(const charts::ARTCC& t) const {
			return SQLiteORM_conv_func::to_string(t);
		}
	};
	template<>
	struct row_extractor<charts::ARTCC> {
		charts::ARTCC extract(const char* columnText) const {
			if (auto artcc = SQLiteORM_conv_func::from_string<charts::ARTCC>(columnText)) {
				return *artcc;
			}
			else {
				throw std::runtime_error("incorrect ARTCC string (" + std::string(columnText) + ")");
			}
		}
		charts::ARTCC extract(sqlite3_stmt* stmt, int columnIndex) const {
			auto str = sqlite3_column_text(stmt, columnIndex);
			return this->extract(reinterpret_cast<const char*>(str));
		}
	};
	//ChartType
	template<>
	struct type_printer<charts::ChartType> : public text_printer {};
	template<>
	struct statement_binder<charts::ChartType> {
		int bind(sqlite3_stmt* stmt, int index, const charts::ChartType& value) {
			return statement_binder<std::string>().bind(stmt, index, SQLiteORM_conv_func::to_string(value));
			//  or return sqlite3_bind_text(stmt, index++, GenderToString(value).c_str(), -1, SQLITE_TRANSIENT);
		}
	};
	template<>
	struct field_printer<charts::ChartType> {
		std::string operator()(const charts::ChartType& t) const {
			return SQLiteORM_conv_func::to_string(t);
		}
	};
	template<>
	struct row_extractor<charts::ChartType> {
		charts::ChartType extract(const char* columnText) const {
			if (auto artcc = SQLiteORM_conv_func::from_string<charts::ChartType>(columnText)) {
				return *artcc;
			}
			else {
				throw std::runtime_error("incorrect ChartType (" + std::string(columnText) + ")");
			}
		}
		charts::ChartType extract(sqlite3_stmt* stmt, int columnIndex) const {
			auto str = sqlite3_column_text(stmt, columnIndex);
			return this->extract(reinterpret_cast<const char*>(str));
		}
	};
	//std::chrono::year_month_day
	template<>
	struct type_printer<std::chrono::year_month_day> : public text_printer {};
	template<>
	struct statement_binder<std::chrono::year_month_day> {
		int bind(sqlite3_stmt* stmt, int index, const std::chrono::year_month_day& value) {
			return statement_binder<std::string>().bind(stmt, index, SQLiteORM_conv_func::to_string(value));
			//  or return sqlite3_bind_text(stmt, index++, GenderToString(value).c_str(), -1, SQLITE_TRANSIENT);
		}
	};
	template<>
	struct field_printer<std::chrono::year_month_day> {
		std::string operator()(const std::chrono::year_month_day& t) const {
			return SQLiteORM_conv_func::to_string(t);
		}
	};
	template<>
	struct row_extractor<std::chrono::year_month_day> {
		std::chrono::year_month_day extract(const char* columnText) const {
			if (auto artcc = SQLiteORM_conv_func::from_string<std::chrono::year_month_day>(columnText)) {
				return *artcc;
			}
			else {
				throw std::runtime_error("incorrect date format for std::chrono::year_month_day (" + std::string(columnText) + ")");
			}
		}
		std::chrono::year_month_day extract(sqlite3_stmt* stmt, int columnIndex) const {
			auto str = sqlite3_column_text(stmt, columnIndex);
			return this->extract(reinterpret_cast<const char*>(str));
		}
	};
	//std::filesystem::path
	template<>
	struct type_printer<fs::path> : public text_printer {};
	template<>
	struct statement_binder<fs::path> {
		int bind(sqlite3_stmt* stmt, int index, const fs::path& value) {
			return statement_binder<std::string>().bind(stmt, index, SQLiteORM_conv_func::to_string(value));
			//  or return sqlite3_bind_text(stmt, index++, GenderToString(value).c_str(), -1, SQLITE_TRANSIENT);
		}
	};
	template<>
	struct field_printer<fs::path> {
		std::string operator()(const fs::path& t) const {
			return SQLiteORM_conv_func::to_string(t);
		}
	};
	template<>
	struct row_extractor<fs::path> {
		fs::path extract(const char* columnText) const {
			if (auto artcc = SQLiteORM_conv_func::from_string<fs::path>(columnText)) {
				return *artcc;
			}
			else {
				throw std::runtime_error("incorrect date format for fs::path (" + std::string(columnText) + ")");
			}
		}
		fs::path extract(sqlite3_stmt* stmt, int columnIndex) const {
			auto str = sqlite3_column_text(stmt, columnIndex);
			return this->extract(reinterpret_cast<const char*>(str));
		}
	};
}

namespace charts {
	//starts Download.exe process. For airac_dates, outpath must be the path to the temporary xml file
	//for charts, outpath must be a path to the directory to download the files to, usually tempdir\ChartDisplay
	//Build the set of FAA download URLs for a cycle. Centralized so DoDownload and CheckCycleAvailability
	//(and any future change to the FAA's URL scheme) stay in agreement.
	namespace {
		std::vector<std::wstring> CycleUrls(std::chrono::year_month_day airacdate) {
			auto nasr_date = std::format(L"{:%d_%b_%Y}", airacdate);
			const std::wstring nasr_base = L"https://nfdc.faa.gov/webContent/28DaySub/extra/";
			std::vector<std::wstring> urls{
				nasr_base + nasr_date + L"_APT_CSV.zip",
				nasr_base + nasr_date + L"_CLS_ARSP_CSV.zip"
			};
			for (wchar_t vol : std::wstring(L"ABCDE")) {
				urls.push_back(std::format(L"https://aeronav.faa.gov/upload_313-d/terminal/DDTPP{}_{:%y%m%d}.zip", vol, airacdate));
			}
			return urls;
		}
	}

	bool DoDownload(const fs::path& outpath, std::chrono::year_month_day airacdate,
		const UpdateReporter& report, std::stop_token st) {
		using namespace std::chrono_literals;
		try {
			fs::create_directories(outpath);
		}
		catch (fs::filesystem_error& e) {
			if (e.code() == std::errc::file_exists) {
				if (!fs::is_directory(outpath)) throw;
			}
			else throw;
		}
		auto urls = CycleUrls(airacdate);
		//local filenames matching the URL order from CycleUrls: NASR APT, NASR class airspace, then TPP A-E
		std::array<fs::path, 7> destinations{
			outpath / "APT_CSV.zip", outpath / "CLS_CSV.zip",
			outpath / "TPPA.zip", outpath / "TPPB.zip", outpath / "TPPC.zip", outpath / "TPPD.zip", outpath / "TPPE.zip"
		};
		for (std::size_t i = 0; i < urls.size(); ++i) {
			if (st.stop_requested()) return false; //canceled between files
			if (report) report({ UpdatePhase::Downloading, static_cast<int>(i + 1),
				static_cast<int>(urls.size()), destinations[i].filename().wstring() });
			//Per-file byte callback exists only to honor cancellation mid-stream (these are large files).
			auto res = Net::HttpDownloadToFile(urls[i], destinations[i],
				[&st](unsigned long long, unsigned long long) { return !st.stop_requested(); });
			if (st.stop_requested()) return false; //user canceled: not a real failure, so no error box
			if (!res) {
				std::wstring detail = res.transport_ok ? std::format(L"HTTP {}", res.status_code) : res.error;
				Win64Wrapper::CreateMessageBox(std::format(L"Failed to download:\n{}\n({})", urls[i], detail), L"Download Failure");
				return false;
			}
			//brief pause between the large TPP volumes to be polite to the FAA servers
			if (i >= 2) {
				std::this_thread::sleep_for(10s);
			}
		}
		return true;
	}

	int CheckCycleAvailability(std::chrono::year_month_day airacdate) {
		//Probe every file without downloading. 0 = all reachable, 44 = not yet published (404),
		//-1 = could not reach the server, otherwise the offending HTTP status.
		for (const auto& url : CycleUrls(airacdate)) {
			auto res = Net::HttpProbe(url);
			if (!res.transport_ok) {
				return -1;
			}
			if (res.status_code == 404) {
				return 44;
			}
			if (!(res.status_code >= 200 && res.status_code < 300)) {
				return static_cast<int>(res.status_code);
			}
		}
		return 0;
	}

	auto GetDatabaseHandle() {
		//Resolve the path and ensure its directory exists exactly once. GetDatabaseHandle is called very
		//frequently; doing SHGetKnownFolderPath + a filesystem stat on every call was needless overhead.
		static const std::string dbpath = [] {
			auto p = Win64Wrapper::GetSysConfDefaultFilepath(Win64Wrapper::KnownFolderID::LocalAppData, false, "ChartDisplay", L"chartdisplay.sqlite");
			try {
				fs::create_directories(p.parent_path());
			}
			catch (fs::filesystem_error& e) {
				if (e.code() != std::errc::file_exists) {
					throw;
				}
				else if (!fs::is_directory(p.parent_path())) {
					throw;
				}
			}
			return p.string();
		}();
		//specify database schema
		static auto db = sql::make_storage(dbpath, sql::make_table("control",
			sql::make_column("pkey", &ControlRecord::pid, sql::primary_key().autoincrement()),
			sql::make_column("item", &ControlRecord::control_item),
			sql::make_column("cycle", &ControlRecord::cycle),
			sql::make_column("date", &ControlRecord::date),
			sql::make_column("other", &ControlRecord::other), sql::check(sql::c(&ControlRecord::cycle) >= 1000)),
			sql::make_table("airports", 
				sql::make_column("pid", &AirportRecord::pid, sql::primary_key().autoincrement()),
				sql::make_column("ARTCC", &AirportRecord::artcc), 
				sql::make_column("AID", &AirportRecord::airport_id, sql::unique()),
				sql::make_column("AIDF",&AirportRecord::airport_id_filestring,sql::unique()),
				sql::make_column("Class", &AirportRecord::airspace_class),
				sql::make_column("has_charts",&AirportRecord::has_charts),sql::make_column("UserAdded",&AirportRecord::useradded),
				sql::check(sql::length(&AirportRecord::artcc) == 3),
				sql::check(sql::length(&AirportRecord::airport_id) >= 3)),
			sql::make_table("charts", 
				sql::make_column("pid", &ChartRecord::pid, sql::primary_key().autoincrement()), 
				sql::make_column("ARTCC", &ChartRecord::artcc),
				sql::make_column("AID", &ChartRecord::airport_id), 
				sql::make_column("ProcType", &ChartRecord::procedure_type),
				sql::make_column("ProcName", &ChartRecord::procedure_name), 
				sql::make_column("ProcCompID", &ChartRecord::procedure_computer_id),
				sql::make_column("ChartPath", &ChartRecord::chartpath), sql::make_column("UserAdded", &ChartRecord::useradded),
				sql::foreign_key(&ChartRecord::airport_id).references(&AirportRecord::airport_id),
				sql::check(sql::length(&ChartRecord::airport_id) >= 3), sql::check(sql::length(&ChartRecord::artcc) == 3)),
			sql::make_table("airac_dates", 
				sql::make_column("cycle", &AIRACInfo::cycle_id, sql::primary_key()),
				sql::make_column("date", &AIRACInfo::date), sql::check(sql::c(&AIRACInfo::cycle_id) >= 1000)));
		//sync_schema only needs to run once; running it on every call issued redundant table-info/PRAGMA
		//queries given how often GetDatabaseHandle is invoked.
		static std::once_flag schema_synced;
		std::call_once(schema_synced, [&] { db.sync_schema(); });
		return db;
	}

	std::chrono::year_month_day GetCurrentDate() {
		using namespace std::chrono;
		year_month_day now{ floor<days>(system_clock::now()) };
		return now;
	}

	AiracDates::AiracDates() {
		auto db = GetDatabaseHandle();
		auto rows = db.select(sql::columns(&ControlRecord::control_item, &ControlRecord::date),
			sql::where(sql::is_equal(&ControlRecord::control_item, "next_airacdates_update")));
		if (rows.size() == 0) {
			UpdateAIRACDateList();
			return;
		}
		const auto& [item, date] = rows.at(0);
		auto now = GetCurrentDate();
		if (date) {
			if (now >= date.value()) {
				UpdateAIRACDateList();
			}
			else {
				RetrieveData();
			}
		}
		else {
			UpdateAIRACDateList();
		}
	}
	void AiracDates::UpdateAIRACDateList() {
		GetAIRACDates();
		//set next_autoupdate_date, which should be the AIRAC date before the last one on the list
		auto it = cycles.rbegin();
		++it;
		next_autoupdate = *it;
		PopulateReferenceAIRAC();
		CommitData();
	}
	const AIRACInfo AiracDates::GetCurrentAiracCycle() const noexcept {
		return current;
	}
	const AIRACInfo AiracDates::GetPreviousAiracCycle() const noexcept {
		return previous;
	}
	const AIRACInfo AiracDates::GetNextAiracCycle() const noexcept {
		return next;
	}
	void AiracDates::GetAIRACDates() {
		using namespace std::chrono_literals;
		auto parse_cyclenum_string = [](std::string cyclenumstr) ->std::pair<int, int> {
			std::string yr = cyclenumstr.substr(0, 2);
			std::string num = cyclenumstr.substr(2, 2);
			return std::make_pair(std::stoi(yr), std::stoi(num));
			};
		cycles.clear();
		auto db = GetDatabaseHandle();
		auto rows = db.select(sql::columns(&ControlRecord::cycle,&ControlRecord::date),
			sql::where(sql::is_equal(&ControlRecord::control_item, "next_airacdates_update")));
		std::chrono::year_month_day base_airac_date;
		std::string base_cycle;
		if (rows.empty()) {
			base_airac_date = 2026y/std::chrono::June/11d;
			base_cycle = "2606";
		}
		else {
			base_airac_date = std::get<1>(rows.at(0)).value();
			base_cycle = std::to_string(std::get<0>(rows.at(0)).value());
		}
		std::chrono::year endyear = base_airac_date.year() + std::chrono::years{ 10 };
		std::chrono::year_month_day enddate{ endyear,std::chrono::December,31d };
		std::chrono::year_month_day previouscycle;
		auto cyclenumparts = parse_cyclenum_string(base_cycle);
		cycles.reserve(10 * 14);
		for (auto curdate = base_airac_date;curdate < enddate;) {
			if (cyclenumparts.second == 13) {
				previouscycle = curdate;
			}
			else if ((cyclenumparts.second == 14) && (curdate.year() != previouscycle.year())) {
				++cyclenumparts.first;
				cyclenumparts.second = 1;
				previouscycle = std::chrono::year_month_day{};
			}
			cycles.emplace_back(std::stoi(std::format("{}{:02d}", cyclenumparts.first, cyclenumparts.second)), curdate);
			curdate = std::chrono::sys_days{ curdate } + std::chrono::days{ 28 };
			if (cyclenumparts.second == 14) {
				++cyclenumparts.first;
				cyclenumparts.second = 1;
			}
			else {
				++cyclenumparts.second;
			}
		}
		PopulateReferenceAIRAC();
	}
	void AiracDates::RetrieveData() {
		auto db = GetDatabaseHandle();
		auto guard = db.transaction_guard();
		auto crows = db.get_all<ControlRecord>();
		auto arows = db.get_all<AIRACInfo>();
		guard.commit();
		if (!crows.size() || !arows.size()) {
			throw NoDataException("No Data in DB. Call UpdateAIRACDateList");
		}
		cycles = arows;
		PopulateReferenceAIRAC();
		for (auto& i : crows) {
			if (i.control_item == "next_airacdates_update") {
				next_autoupdate.cycle_id = i.cycle.value();
				next_autoupdate.date = i.date.value();
			}
			//if the database value for current next previous and the calculated value disagree, update the database
			if (i.control_item == "current_airac_cycle") {
				if (i.date != current.date) {
					CommitData(true);
					break;
				}
			}
		}
	}
	void AiracDates::CommitData(bool control_only) {
		if (next_autoupdate.date == std::chrono::year_month_day{}) {
			throw NoDataException("Call UpdateAIRACDateList or RetrieveData");
		}
		std::array < ControlRecord, 5> ctrl_items{
			ControlRecord{-1,"next_airacdates_update",next_autoupdate.cycle_id,next_autoupdate.date,std::nullopt},
			ControlRecord{-1,"current_airac_cycle",current.cycle_id,current.date,std::nullopt},
			ControlRecord{-1,"previous_airac_cycle",previous.cycle_id,previous.date,std::nullopt},
			ControlRecord{-1,"next_airac_cycle",next.cycle_id,next.date,std::nullopt},
			ControlRecord{-1,"last_control_update",std::nullopt,std::optional<decltype(GetCurrentDate())>(GetCurrentDate()),std::nullopt}
		};
		auto db = GetDatabaseHandle();
		ControlRecord chart_update_rec;
		auto chartupdatevec = db.select(
			sql::columns(&ControlRecord::cycle, &ControlRecord::date),
			sql::where(sql::is_equal(&ControlRecord::control_item, "last_charts_update")));
		//Whether a real prior chart-update row exists. On a fresh DB it won't, and we must NOT fabricate one:
		//FAAChartProcessor treats an absent last_charts_update as "never downloaded" and forces the initial download.
		const bool had_charts_update = !chartupdatevec.empty();
		if (had_charts_update) {
			auto [ccycle, cdate] = chartupdatevec.at(0);
			chart_update_rec.cycle = ccycle;
			chart_update_rec.date = cdate;
		}
		chart_update_rec.control_item = "last_charts_update";
		//Preserve the user's autoupdate preference across the remove_all below. That wipe deletes every
		//control row, and FAAChartProcessor only re-inserts a default ("false") when the row is missing,
		//so without carrying it through here an AIRAC-list refresh would silently reset the user's choice.
		ControlRecord autoupdate_rec;
		autoupdate_rec.control_item = "autoupdate";
		autoupdate_rec.other = "false";
		if (auto aur = db.select(&ControlRecord::other, sql::where(sql::is_equal(&ControlRecord::control_item, "autoupdate")));
			!aur.empty()) {
			autoupdate_rec.other = aur.at(0);
		}
		auto guard = db.transaction_guard();
		db.remove_all<ControlRecord>();
		db.insert_range(ctrl_items.begin(), ctrl_items.end());
		if (had_charts_update) {
			db.insert(chart_update_rec);
		}
		db.insert(autoupdate_rec);
		if (!control_only) {
			db.remove_all<AIRACInfo>();
			db.replace_range(cycles.begin(), cycles.end());
		}
		guard.commit();
	}
	void AiracDates::PopulateReferenceAIRAC() {
		if (cycles.size() == 0) {
			return;
		}
		const auto thisday = GetCurrentDate();
		//not using a range-based for as I need access to both the previous and next items
		for (auto it = cycles.begin();it != cycles.end();++it) {
			if (it->date >= thisday) {
				//it's AIRAC day!
				if (it->date == thisday) {
					current = *it;
					next = *(it + 1);
					previous = it != cycles.begin() ? *(it - 1) : *it;
				}
				//it's not, so we've got the next cycle, so current is it - 1, and previous is it - 2, unless that's the beginning
				else {
					auto it2 = it - 1;
					current = *it2;
					if (it2 == cycles.begin()) {
						previous = *it2;
					}
					else {
						previous = *(it2 - 1);
					}
					next = *it;
				}
				break;
			}
		}
	}

	FAAChartProcessor::FAAChartProcessor()
		:chartdir(Win64Wrapper::GetSysConfDefaultFilepath(Win64Wrapper::KnownFolderID::LocalAppData,true,L"ChartDisplay\\Charts")),
		manchartxml(chartdir.parent_path() / "custom_charts.xml") {
		OutputDebugString(L"Beginning Chart intialization...\n");
		if (fs::exists(manchartxml)) {
			ParseManualCharts();
		}

		//get last update date
		auto db = GetDatabaseHandle();
		ControlRecord default_autoupdate;
		default_autoupdate.control_item = "autoupdate";
		default_autoupdate.other = "false";
		auto auresp = db.select(&ControlRecord::other, sql::where(sql::is_equal(&ControlRecord::control_item, "autoupdate")));
		if (auresp.empty()) {
			db.insert(default_autoupdate);
		}
		//Downloads no longer happen in the constructor: doing ~4GB of I/O here froze startup before any
		//window existed, so a progress dialog was impossible. The UI queries ChartUpdateNeeded() after the
		//main window is up and runs the update on a worker thread with a progress dialog.
		OutputDebugString(L"End Chart initalization.\n");
	}
	UpdateNeed FAAChartProcessor::ChartUpdateNeeded() const {
		auto db = GetDatabaseHandle();
		auto lcu = db.select(&ControlRecord::date, sql::where(sql::is_equal(&ControlRecord::control_item, "last_charts_update")));
		if (lcu.empty()) {
			return UpdateNeed::Initial; //no charts have ever been downloaded
		}
		auto au = db.select(&ControlRecord::other, sql::where(sql::is_equal(&ControlRecord::control_item, "autoupdate")));
		const bool autoupdate_on = !au.empty() && au.at(0).has_value() && au.at(0).value() == "true";
		if (autoupdate_on && lcu.at(0).has_value() && lcu.at(0).value() < current_cycle.GetCurrentAiracCycle().date) {
			return UpdateNeed::AutoupdateStale;
		}
		return UpdateNeed::None;
	}
	bool FAAChartProcessor::UpdateCharts(bool no_download, bool force, const UpdateReporter& report, std::stop_token st) {
		using Win64Wrapper::MessageBoxStyles;
		auto tempdir = Win64Wrapper::GetSysConfDefaultFilepath(Win64Wrapper::KnownFolderID::LocalAppData, false, R"(ChartDisplay\download)");
		try {
			fs::create_directories(tempdir);
		}
		catch (fs::filesystem_error& e) {
			if (e.code() != std::errc::file_exists) {
				throw;
			}
			else if (!fs::is_directory(tempdir)) {
				throw;
			}
		}
		auto cdate = current_cycle.GetCurrentAiracCycle().date;
		//nodownload = true force = true download: yes (force=true)
		//nodownload = false force = true downlaod: yes (force: true)
		//no donwload = true force = false download: no (no_download=true)
		//no download = false force = false download: depending on check
		bool check = false;
		auto trigger_download = [this, &report, &st](const fs::path& tempdir,std::chrono::year_month_day cdate) -> bool {
			//Preflight: confirm the FAA has actually published this cycle before touching anything.
			//Nothing has been wiped yet, so an unavailable cycle leaves the existing charts fully intact.
			if (report) report({ UpdatePhase::Checking });
			auto avail = CheckCycleAvailability(cdate);
			if (avail == 44) {
				Win64Wrapper::CreateMessageBox(
					L"The charts for this AIRAC cycle have not been published by the FAA yet. Your current "
					L"charts have been kept; try again closer to the cycle's effective date.",
					L"Charts Not Yet Available", nullptr, MessageBoxStyles::Ok,
					MessageBoxStyles::DefaultButton1, MessageBoxStyles::IconInformation);
				return false;
			}
			else if (avail != 0) {
				Win64Wrapper::CreateMessageBox(
					std::format(L"Could not verify chart availability (code {}). Your current charts have been kept.", avail),
					L"Download Check Failed", nullptr, MessageBoxStyles::Ok,
					MessageBoxStyles::DefaultButton1, MessageBoxStyles::IconError);
				return false;
			}
			try {
				fs::create_directories(tempdir);
			}
			catch (fs::filesystem_error& e) {
				if (e.code() != std::errc::file_exists) {
					throw;
				}
				else if (!fs::is_directory(tempdir)) {
					throw;
				}
			}
			//The helper overwrites each zip in place (truncate on open), so no pre-clean is needed and we
			//never use 2x disk. The organized chart tree stays untouched until GetChartsAndOrganize succeeds.
			return DoDownload(tempdir, cdate, report, st);
		};
		if (force) {
			check = trigger_download(tempdir, cdate);
		}
		else if (no_download) {
			if (!fs::exists(tempdir / "TPPA.zip")) {
				Win64Wrapper::CreateMessageBox(L"Downloaded files do not exist. Please use the Force Chart Update button to-redownload them.",
					L"Files Missing", nullptr, MessageBoxStyles::Ok, MessageBoxStyles::DefaultButton1, MessageBoxStyles::IconError);
				check = false;
			}
			else {
				//chartdir is wiped/rebuilt inside GetChartsAndOrganize now, so nothing to clean here.
				check = true;
			}
		}
		else {
			auto lcu = GetLastChartUpdate();
			if (lcu.cycle_id == 0) {
				return false;
			}
			if (lcu.date < cdate) {
				check = trigger_download(tempdir, cdate);
			}
		}
		if (!check) return false;
		if (st.stop_requested()) return false; //canceled after download, before the long organize step
		if (report) report({ UpdatePhase::Organizing });
		GetChartsAndOrganize(tempdir);
		return true;
	}

	using ZipError = std::unique_ptr < zip_error_t, decltype([](zip_error_t* err) {zip_error_fini(err);}) > ;
	using ZipArchive = std::unique_ptr < zip_t, decltype([](zip_t* ar) {zip_close(ar);}) > ;
	using ZipContainedFile = std::unique_ptr < zip_file_t, decltype([](zip_file_t* zf) {zip_fclose(zf);}) > ;

	void FAAChartProcessor::GetChartsAndOrganize(const std::filesystem::path& tempdir) {
		//Regenerate the organized chart tree from scratch. Deferred to here (rather than before the
		//download) so a failed or unpublished download leaves the user's existing charts intact.
		//keep_temp=true preserves the downloaded zips for the no-download reload path.
		CleanCharts(tempdir, true);

		std::array<std::pair<fs::path, std::string>, 2> zipfns = { std::make_pair(tempdir / "APT_CSV.zip","APT_BASE.csv"),
			std::make_pair(tempdir / "CLS_CSV.zip","CLS_ARSP.csv") };
		std::vector<csv::CSVReader> csvs;
		csv::CSVFormat cfmt;
		cfmt.delimiter(',').quote(R"(")").header_row(0);
		std::unordered_map<std::string, AirportRecord> recmap;
		const int aptindex = 0;
		const int clsindex = 1;
		//parse the csv files from the downloadaded zips
		auto getzip = [&csvs,&cfmt](std::pair<fs::path, std::string>& fileinfo) {
			int errcode = {};
			auto& [fn,name] = fileinfo;
			ZipArchive archive(zip_open(fn.string().c_str(), ZIP_RDONLY, &errcode));
			zip_error_t ze;
			ZipError zerror;
			if (errcode != ZIP_ER_OK) {
				zip_error_init_with_code(&ze, errcode);
				zerror.reset(std::move(&ze));
				std::println("Cannot open zip archive: {} with error: {}", fn.string(), zip_error_strerror(zerror.get()));
				return false;
			}
			struct zip_stat st;
			std::string zip_file_raw;
			zip_stat_init(&st);
			if (zip_stat(archive.get(), name.c_str(), 0, &st) < 0) {
				std::println("Unable to open {}", name);
				return false;
			}
			ZipContainedFile zfile(zip_fopen(archive.get(), name.c_str(), 0));
			if (!zfile) {
				std::println("Cannot open file in zip archive: {}", name);
				return false;
			}
			zip_file_raw.clear();
			zip_file_raw.resize_and_overwrite(st.size, [&zfile](char* buf, size_t sz) -> std::size_t {
				auto ret = zip_fread(zfile.get(), buf, sz);
				if (ret != -1) {
					return ret;
				}
				else {
					return 0;
				}
				});
			if (zip_file_raw.size() == 0) {
				return false;
			}
			std::istringstream iraw(std::move(zip_file_raw));
			csvs.emplace_back(iraw, cfmt);
			return true;
			};
		for (auto& i : zipfns) {
			auto ret = getzip(i);
			if (!ret) {
				throw NoDataException("Unable to parse NASR zip files");
			}
		}
		//populate the AirportRecords from the parsed CSV
		std::string allowable;
		int count = 0;
		for (auto& [artcc, _] : rev_artcc_names_map) {
			allowable += artcc;
			allowable += ',';
			++count;
		}
		allowable.pop_back();
		for (auto& row : csvs.at(aptindex)) {
			AirportRecord rec;
			std::string aid = row["ARPT_ID"].get<std::string>();
			rec.airport_id = aid;
			if (Win64Wrapper::windows_reserved_names.contains(aid)) {
				rec.airport_id_filestring = std::format("{}X", aid);
			}
			else {
				rec.airport_id_filestring = aid;
			}
			auto artcc = row["RESP_ARTCC_ID"].get<std::string>();
			//special cases
			//BLI is under Vancouver FIR, but the ATCT is run by the FAA and is managed by ZSE on VATSIM
			if (aid == "BLI") artcc = "ZSE";
			if (!allowable.contains(artcc)) {
				continue;
			}
			rec.artcc = rev_artcc_names_map.at(artcc);
			//identify all towered airports by using the class E towered designation
			//if the airport (most likely) is class B-D, that will be overwritten in the next loop
			if (row["TWR_TYPE_CODE"] != "NON-ATCT") {
				rec.airspace_class = 'T';
			}
			recmap.try_emplace(std::move(aid), std::move(rec));
		}
		//fix records for class B-D airports.
		for (auto& row : csvs.at(clsindex)) {
			char airspace = 'E';
			if (!row["CLASS_D_AIRSPACE"].is_null()) {
				airspace = 'D';
			}
			else if (!row["CLASS_C_AIRSPACE"].is_null()) {
				airspace = 'C';
			}
			else if (!row["CLASS_B_AIRSPACE"].is_null()) {
				airspace = 'B';
			}
			if (airspace != 'E') {
				//Use find, not at: an airport can appear in CLS_ARSP without being in recmap (e.g. its
				//RESP_ARTCC is outside the allowable set, or it isn't in APT_BASE). at() would throw and
				//abort the entire chart update mid-write. Skip anything we don't already track.
				if (auto it = recmap.find(row["ARPT_ID"].get<std::string>()); it != recmap.end()) {
					it->second.airspace_class = airspace;
				}
			}
		}

		//add manual AirportRecords for fictional airports/closed airports if necessary and do all checks here
		//if (!manual_additions.empty()) {
		//	for (auto& m : manual_additions) {
		//		if (!recmap.contains(m.airport_id)) {
		//			auto aid = m.airport_id;
		//			AirportRecord ar;
		//			ar.artcc = m.artcc;
		//			ar.airport_id = aid;
		//			ar.useradded = true;
		//			ar.has_charts = true;
		//			if (auto it = std::ranges::find_if(manual_higher_class_additions, [&aid](const std::pair<std::string, char>& mc) {
		//				if (mc.first == aid) return true;
		//				else return false;
		//				});it != manual_higher_class_additions.end()) {
		//				ar.airspace_class = it->second;
		//			}
		//			if (Win64Wrapper::windows_reserved_names.contains(aid)) {
		//				ar.airport_id_filestring = std::format("{}X", aid);
		//			}
		//			recmap.try_emplace(std::move(aid), std::move(ar));
		//		}
		//	}
		//}

		auto vmap = std::views::values(recmap); //get a view of all the AirportRecords for adding to the database later

		//Charts
		std::array < fs::path, 4 > zip_paths{ tempdir / "TPPA.zip",tempdir / "TPPB.zip", tempdir / "TPPC.zip",tempdir / "TPPD.zip" };
		//parse the XML Metafile, input is a std::string for the buffer (all data here will be overwritten so preferably an empty string
		auto getxml = [&tempdir](std::string& xmlfilebuffer) -> std::expected<pugi::xml_document,bool>{
			auto xmlzippath = tempdir / "TPPE.zip";
			std::string xmlfn = "d-TPP_Metafile.xml";
			zip_error_t ze;
			ZipError zerror;
			int errcode = {};
			ZipArchive xzar(zip_open(xmlzippath.string().c_str(), ZIP_RDONLY, &errcode));
			if (errcode != ZIP_ER_OK) {
				zip_error_init_with_code(&ze, errcode);
				zerror.reset(std::move(&ze));
				std::println("Cannot open zip archive: {} with error: {}", xmlfn, zip_error_strerror(zerror.get()));
				return std::unexpected(false);
			}
			struct zip_stat st;
			zip_stat_init(&st);
			if (zip_stat(xzar.get(), xmlfn.c_str(), 0, &st) < 0) {
				std::println("Unable to open {}", xmlfn);
				return std::unexpected(false);
			}
			ZipContainedFile zfile(zip_fopen(xzar.get(), xmlfn.c_str(), 0));
			if (!zfile) {
				std::println("Cannot open file in zip archive: {}", xmlfn);
				return std::unexpected(false);
			}
			xmlfilebuffer.clear();
			xmlfilebuffer.resize_and_overwrite(st.size, [&zfile](char* buf, size_t sz) {
				auto ret = zip_fread(zfile.get(), buf, sz);
				if (ret != -1) {
					return ret;
				}
				else {
					return static_cast<zip_int64_t>(0);
				}
				});
			if (xmlfilebuffer.size() == 0) {
				return std::unexpected(false);
			}
			std::erase_if(xmlfilebuffer, [](char c) { return ((c == '\r') ? true : false);});
			pugi::xml_document doc;
			auto res = doc.load_buffer_inplace(xmlfilebuffer.data(), xmlfilebuffer.size());
			if (!res) {
				return std::unexpected(false);
			}
			else {
				return doc;
			}
		};
		std::string xmldocbuffer;
		auto xmldoc = getxml(xmldocbuffer);
		//A corrupt/truncated TPPE.zip (e.g. a partial download) makes getxml return unexpected; calling
		//.value() here would throw bad_expected_access and kill the update. Surface it instead.
		if (!xmldoc) {
			throw NoDataException("Unable to read or parse the TPP metafile (d-TPP_Metafile.xml). "
				"The chart archive may be missing or corrupt; try a Force Chart Update.");
		}
		constexpr std::array<const char*, 5> nodes{ "digital_tpp","state_code","city_name","airport_name","record" };
		std::vector<ChartRecord> charts_to_add;
		auto tpp = xmldoc.value().child(nodes[0]);
		for (auto& state : tpp.children(nodes[1]))
		{
			for (auto& city : state.children(nodes[2]))
			{
				for (auto& airport : city.children(nodes[3]))
				{
					auto aname = airport.attribute("apt_ident").as_string();
					auto arecit = recmap.find(aname);
					if (arecit == recmap.end()) {
						continue;
					}
					auto& [_, arec] = *arecit;
					arec.has_charts = true;
					for (auto& c : airport.children(nodes[4]))
					{
						ChartRecord cr;
						auto artccstr = artcc_names_map.at(arec.artcc);
						auto newchartpath = chartdir / artccstr / arec.airport_id_filestring; //directory
						std::string testpath = c.child_value("pdf_name");
						//catch any removed procedures whose metadata hasn't been removed
						if (testpath.contains("DELETED_JOB"))
							continue;
						newchartpath /= testpath; //filename
						auto ctypestr = c.child_value("chart_code");
						ChartType ctype;
						try { //check for missed chart_codes in the enum
							ctype = charttype_names_map.at(ctypestr);
						}
						catch (std::out_of_range&) {
							OutputDebugStringA(std::format("{} Chart Type not supported.\n",ctypestr).c_str());
							continue;
						}
						std::string faanfd = c.child_value("faanfd18");
						if (faanfd.empty()) {
							charts_to_add.emplace_back(arec.airport_id, arec.artcc, ctype,
								c.child_value("chart_name"), newchartpath);
						}
						else {
							charts_to_add.emplace_back(arec.airport_id, arec.artcc, ctype,
								c.child_value("chart_name"), newchartpath, faanfd);
						}
					}
				}
			}
		}		
		//add manual charts
		//if (!manual_additions.empty()) {
		//	charts_to_add.append_range(std::move(manual_additions));
		//}
		//copy charts from zip files to dirs
		std::array<ZipArchive, 4> tppzip;
		zip_error_t ze;
		ZipError zerror;
		for (int tz = 0;tz < 4; ++tz) {
			int err_zip_code = {};
			tppzip[tz].reset(zip_open(zip_paths[tz].string().c_str(), ZIP_RDONLY, &err_zip_code));
			if (err_zip_code != ZIP_ER_OK) {
				zip_error_init_with_code(&ze, err_zip_code);
				zerror.reset(std::move(&ze));
				std::println("Cannot open zip archive: {} with error: {}", zip_paths[tz].filename().string(), zip_error_strerror(zerror.get()));
				throw NoDataException("TPP Zip open error.");
			}
		}
		std::vector<std::pair<std::string,std::string>> bugfix_remove;
		for (auto& c : charts_to_add) {
			//if (c.useradded) {
			//	//manual charts aren't in zips
			//	auto newfp = chartdir / artcc_names_map.at(c.artcc) / c.airport_id / c.chartpath.filename();
			//	std::error_code ec;
			//	fs::create_directories(newfp.parent_path());
			//	fs::copy_file(c.chartpath, newfp,fs::copy_options::overwrite_existing,ec);
			//	if (!ec) {
			//		c.chartpath = std::move(newfp); //update chartpath to new path now that the copy has been made
			//	}
			//}
			//else {
			if (c.airport_id == "JFK" && c.procedure_name == "PAWLING TWO") {
				bugfix_remove.push_back(std::make_pair(c.airport_id, c.procedure_name));
				continue;
			}
			for (auto& tz : tppzip) {
				//first find if this is the correct archive
				auto cfilename = c.chartpath.filename().string();
				auto pos = zip_name_locate(tz.get(), cfilename.c_str(), 0);
				if (pos != -1) {//found the file
					struct zip_stat st;
					zip_stat_init(&st);
					if (zip_stat_index(tz.get(), pos, 0, &st) < 0) {
						throw NoDataException(std::format("Unable to stat {}", cfilename).c_str());
					}
					std::vector<std::byte> data(st.size);
					ZipContainedFile zfile(zip_fopen_index(tz.get(), st.index, 0));
					if (!zfile) {
						throw NoDataException(std::format("Unable to open {}", cfilename).c_str());
					}
					auto zfret = zip_fread(zfile.get(), data.data(), data.size());
					if (zfret == -1) {
						throw NoDataException(std::format("Unable to read {}", cfilename).c_str());
					}
					try {
						fs::create_directories(c.chartpath.parent_path());
					}
					catch (fs::filesystem_error& e) {
						OutputDebugStringA(std::format("Unable to Create: {}\nError: {}, Code: {}\n", c.chartpath.parent_path().string(),
							e.code().message(), e.code().value()).c_str());
						throw;
					}
					std::ofstream cfileout(c.chartpath, std::ios::binary | std::ios::trunc);
					if (!cfileout) {
						throw fs::filesystem_error(std::format("Unable to open output file {} for writing.",c.chartpath.string()),
							std::make_error_code(std::errc::bad_file_descriptor));
					}
					cfileout.write(reinterpret_cast<char*>(data.data()), data.size());
					break;
				}
			}
		//	}
		}
		if (!bugfix_remove.empty()) {
			for (auto& [bfrai,bfrpn] : bugfix_remove) {
				std::erase_if(charts_to_add, [&](ChartRecord item) {
					if (item.airport_id == bfrai && item.procedure_name == bfrpn) {
						return true;
					}
					else return false;
					});
			}
		}
		//copy artcc-wide additions, if any
		//if (!artcc_wide_additions.empty()) {
		//	for (auto& i : artcc_wide_additions) {
		//		auto a = std::get<0>(i);
		//		auto p = std::get<2>(i);
		//		auto outpath = chartdir / artcc_names_map.at(a) / "Additions" / p.filename();
		//		std::error_code ec;
		//		fs::create_directory(outpath.parent_path());
		//		fs::copy_file(p, outpath, fs::copy_options::overwrite_existing,ec);
		//		if (!ec) {
		//			std::get<2>(i) = outpath;
		//		}
		//	}
		//}
		//copy cwt file if any
		//if (!cwt_file.empty()) {
		//	auto outpath = chartdir / cwt_file.filename();
		//	fs::copy_file(cwt_file, outpath, fs::copy_options::overwrite_existing);
		//	cwt_file = outpath;
		//}
		//Prep Control Records
		auto cycle = current_cycle.GetCurrentAiracCycle();
		ControlRecord lcontrol = {};
		lcontrol.control_item = "last_control_update";
		lcontrol.date = GetCurrentDate();
		ControlRecord lcharts = {};
		lcharts.control_item = "last_charts_update";
		lcharts.cycle = cycle.cycle_id;
		lcharts.date = GetCurrentDate();
		//add NASR Data and Chart Data to database
		auto db = GetDatabaseHandle();
		{
			auto guard = db.transaction_guard();
			try {
				//remove chart records first because of the foreign key to the airports table in the charts table
				db.remove_all<ChartRecord>();
				db.remove_all<AirportRecord>();
			}
			catch (std::system_error& e) {
				std::wstring errstr = Win64Wrapper::convert_string(std::string(e.what()));
				Win64Wrapper::CreateMessageBox(std::format(L"remove_all<AirportRecord> error.\n{}\nCode: {}",
					errstr, e.code().value()), L"SQL error");
				throw;
			}
			//need to split airports to be under the SQLite limit
			for (const auto& chunk : vmap | std::views::chunk(MAX_PER_OP)) {
				auto vecvmap = std::ranges::to<std::vector>(chunk);
				db.insert_range(vecvmap.begin(), vecvmap.end());
			}
			try {
				guard.commit();
			}
			catch (std::system_error& e) {
				OutputDebugStringA(std::format("SQL System Error on insert_range in GetChartsAndOrganize: {}, Code: {}\n", e.what(), e.code().value()).c_str());
				throw;
			}
		}
		{
			auto guard = db.transaction_guard();
			for (const auto& chunk : charts_to_add | std::views::chunk(MAX_PER_OP)) {
				auto vecharts = std::ranges::to<std::vector>(chunk);
				db.insert_range(vecharts.begin(),vecharts.end());
			}
			try {
				guard.commit();
			}
			catch (std::system_error& e) {
				OutputDebugStringA(std::format("SQL System Error on insert_range in GetChartsAndOrganize: {}, Code: {}\n", e.what(), e.code().value()).c_str());
				throw;
			}
			//add manual items
			ReloadManualCharts();

			if (auto i = db.select(&ControlRecord::pid, sql::where(sql::is_equal(&ControlRecord::control_item, "last_charts_update")));!i.empty()) {
				auto id = i.at(0);
				lcharts.pid = id;
				db.update(lcharts);
			}
			else {
				db.insert(lcharts);
			}
			db.update(lcontrol);
		}
	}
	ARTCCInfo FAAChartProcessor::GetAirspaceClassInfo(ARTCC artcc) {
		using sqlite_orm::where;
		using sqlite_orm::is_equal;
		ARTCCInfo ainfo;
		ainfo.artcc = artcc;
		ainfo.artccstr = artcc_names_map.at(artcc);
		auto db = GetDatabaseHandle();
		//get all towered airports: SELECT * FROM airports WHERE ARTCC=artcc AND AIRSPACE_CLASS != 'E' (towered E/G airports have airspace_class T)
		auto recs = db.get_all<AirportRecord>(where(is_equal(&AirportRecord::artcc, artcc) &&
			is_equal(&AirportRecord::has_charts,true)));
		std::ranges::sort(recs, [](AirportRecord a, AirportRecord b) {
			return ((a.airport_id < b.airport_id) ? true : false);
			});
		for (auto& r : recs) {
			if (r.airspace_class == 'B') {
				ainfo.class_b_airports.push_back(r.airport_id);
			}
			else if (r.airspace_class == 'C') {
				ainfo.class_c_airports.push_back(r.airport_id);
			}
			else if (r.airspace_class == 'D') {
				ainfo.class_d_airports.push_back(r.airport_id);
			}
			else if (r.airspace_class == 'T') {
				ainfo.other_towered.push_back(r.airport_id);
			}
			else {
				ainfo.class_eg_untowered.push_back(r.airport_id);
			}
			if (r.airport_id != r.airport_id_filestring) {
				ainfo.foldername_modification_airports.emplace_back(r.airport_id, r.airport_id_filestring);
			}
		}
		return ainfo;
	}
	std::vector<ChartRecord> FAAChartProcessor::GetAirportCharts(std::string airport_id) {
		auto db = GetDatabaseHandle();
		return db.get_all<ChartRecord>(sql::where(sql::is_equal(&ChartRecord::airport_id, airport_id)));
	}
	std::vector<ChartType> FAAChartProcessor::GetAirportChartType(std::string airport_id) {
		auto db = GetDatabaseHandle();
		return db.select(&ChartRecord::procedure_type, sql::where(sql::is_equal(&ChartRecord::airport_id, airport_id)));
	}
	std::optional<AirportLookup> FAAChartProcessor::GetRealAirportByLID(std::string lid) const {
		auto db = GetDatabaseHandle();
		//useradded == false restricts the match to real NASR airports, never fictional user-added ones.
		auto recs = db.get_all<AirportRecord>(sql::where(
			sql::is_equal(&AirportRecord::airport_id, lid) && sql::is_equal(&AirportRecord::useradded, false)));
		if (recs.empty()) {
			return std::nullopt;
		}
		return AirportLookup{ recs.front().artcc, recs.front().airspace_class };
	}
	void FAAChartProcessor::CleanCharts(const std::filesystem::path& tempdir, bool keep_temp) {
		//first clean up organized charts
		if (fs::exists(chartdir)) {
			fs::remove_all(chartdir);
			fs::create_directories(chartdir);
		}
		//now remove temporary files
		if (!keep_temp) {
			std::array<fs::path, 7> chpaths{ tempdir / "TPPA.zip",tempdir / "TPPB.zip",tempdir / "TPPC.zip",tempdir / "TPPD.zip",tempdir / "TPPE.zip",
				tempdir / "APT_CSV.zip", tempdir / "CLS_CSV.zip" };
			for (auto& i : chpaths) {
				if (fs::exists(i)) {
					fs::remove(i);
				}
			}
			if (fs::is_empty(tempdir)) {
				fs::remove(tempdir);
			}
		}
	}
	FAAChartProcessor::~FAAChartProcessor() {
		auto db = GetDatabaseHandle();
		db.vacuum();
	}
	std::optional<bool> FAAChartProcessor::AutoupdateState(std::optional<bool> set_autoupdate) {
		auto db = GetDatabaseHandle();
		if (set_autoupdate) {
			std::string state;
			if (set_autoupdate.value()) {
				state = "true";
			}
			else {
				state = "false";
			}
			db.update_all(sql::set(sql::c(&ControlRecord::other) = state), sql::where(sql::c(&ControlRecord::control_item) == "autoupdate"));
			return std::nullopt;
		}
		else {
			auto aur = db.select(&ControlRecord::other, sql::where(sql::is_equal(&ControlRecord::control_item, "autoupdate")));
			return ((aur.at(0) == "true") ? true : false);
		}
	}
	AIRACInfo FAAChartProcessor::GetLastChartUpdate() const {
		auto db = GetDatabaseHandle();
		auto retvec = db.select(sql::columns(&ControlRecord::cycle, &ControlRecord::date),
			sql::where(sql::is_equal(&ControlRecord::control_item, "last_charts_update")));
		AIRACInfo ai;
		if (retvec.empty()) {
			return ai; //no charts downloaded yet; default-constructed (cycle_id 0) reads as "never updated"
		}
		auto& [c,d] = retvec.at(0);
		ai.cycle_id = c.value_or(0);
		ai.date = d.value_or(std::chrono::year_month_day{});
		return ai;
	}
	void FAAChartProcessor::ParseManualCharts() {
		using Win64Wrapper::MessageBoxStyles;
		ClearAllAdditions();
		pugi::xml_document customdoc;
		customdoc.load_file(manchartxml.string().c_str());
		constexpr std::array<const char*, 7> nodes = { "custom_charts","chart","title","path","type","artccrecord","cwt"};
		auto first_node = customdoc.child(nodes[0]);
		for(auto& i : first_node.children(nodes[1])) {
			ChartRecord rec;
			bool lookup_airport = true;
			rec.useradded = true;
			rec.procedure_type = ChartType::MANUAL;
			rec.procedure_computer_id = std::nullopt;
			rec.airport_id = i.attribute("airport").as_string();
			rec.artcc = rev_artcc_names_map.at(i.attribute("artcc").as_string());
			if (rec.airport_id.empty()) {
				Win64Wrapper::CreateMessageBox(
					L"chart element must have airport, artcc and fictional attributes.\nFictional attribute must have a value of yes or no.",
					L"Custom Chart XML Error",
					nullptr, MessageBoxStyles::Ok, MessageBoxStyles::DefaultButton1, MessageBoxStyles::IconError);
				return;
			}
			rec.procedure_name = i.child_value(nodes[2]);
			rec.chartpath = i.child_value(nodes[3]);
			rec.procedure_type = ChartType::MANUAL;
			std::string checkstr = i.child_value(nodes[4]);
			if (!checkstr.empty()) {
				rec.procedure_type = SQLiteORM_conv_func::rev_chart_type_str.at(checkstr);
			}
			if (rec.procedure_name == "" || rec.chartpath == "") {
				std::ignore = Win64Wrapper::CreateMessageBox(L"chart element must have child elements title and path.", 
					L"Custom Chart XML Error", nullptr, MessageBoxStyles::Ok, MessageBoxStyles::DefaultButton1, MessageBoxStyles::IconError);
				return;
			}
			if (!fs::exists(rec.chartpath)) continue;
			manual_additions.push_back(rec);
			if (std::string iclass = i.attribute("class").as_string();!iclass.empty()) {
				if (iclass != "E") {
					manual_higher_class_additions.emplace_back(rec.airport_id, iclass.at(0));
				}
			}
		}
		//gather any artcc-wide items if present
		for (auto& i : first_node.children(nodes[5])) {
			ARTCC artcc = ARTCC::Empty;
			std::string title = i.child_value(nodes[2]);
			fs::path pth = i.child_value(nodes[3]);
			try {
				artcc = rev_artcc_names_map.at(i.attribute("artcc").as_string());
			}
			catch (std::out_of_range&) {
				Win64Wrapper::CreateMessageBox(L"artccrecord element must have artcc attribute", L"artccrecord parse error",
					nullptr, MessageBoxStyles::Ok, MessageBoxStyles::DefaultButton1, MessageBoxStyles::IconError);
				return;
			}
			if (title.empty() || pth.empty()) {
				Win64Wrapper::CreateMessageBox(L"artccrecord element must have title and path child elements", 
					L"artccrecord parse error",
					nullptr, MessageBoxStyles::Ok, MessageBoxStyles::DefaultButton1, MessageBoxStyles::IconError);
				return;
			}
			artcc_wide_additions.emplace_back(artcc,title,pth);
		}
		//get the cwt file if present
		if (auto cwtfile = first_node.child(nodes[6]);cwtfile) {
			cwt_file = cwtfile.child_value(nodes[3]);
		}
	}
	void FAAChartProcessor::ReloadManualCharts() {
		using Win64Wrapper::MessageBoxStyles;
		ParseManualCharts();
		auto db = GetDatabaseHandle();
		std::vector<AirportRecord> arecs;
		for (auto& c: manual_additions) {
			//Add airport if necessary
			std::vector<std::string> ap_item = db.select(&AirportRecord::airport_id,
				sql::where(sql::is_equal(&AirportRecord::airport_id, c.airport_id)));
			if (ap_item.empty()) {
				AirportRecord ar;
				ar.artcc = c.artcc;
				ar.airport_id = c.airport_id;
				ar.useradded = true;
				ar.has_charts = true;
				if (auto it = std::ranges::find_if(manual_higher_class_additions, [&c](const std::pair<std::string, char>& mc) {
					if (mc.first == c.airport_id) return true;
					else return false;
					});it != manual_higher_class_additions.end()) {
					ar.airspace_class = it->second;
				}
				if (Win64Wrapper::windows_reserved_names.contains(c.airport_id)) {
					ar.airport_id_filestring = std::format("{}X", c.airport_id);
				}
				arecs.push_back(ar);
			}
		}
		auto guard = db.transaction_guard();
		//insert_range over an empty range throws in sqlite_orm (it builds an INSERT with no value tuples).
		//ReloadManualCharts runs at the end of every GetChartsAndOrganize, so a user with no custom charts
		//would hit this on every update unless we guard it.
		if (!manual_additions.empty()) {
			db.insert_range(manual_additions.begin(), manual_additions.end());
		}
		if (!arecs.empty()) {
			db.insert_range(arecs.begin(), arecs.end());
		}
		guard.commit();
	}
	std::vector<ManualARTCCAddition> FAAChartProcessor::GetARTCCAdditions(std::optional<ARTCC> artcc) const noexcept {
		std::vector<ManualARTCCAddition> ret;
		if (artcc) {
			ARTCC a = artcc.value();
			ret = artcc_wide_additions | std::views::filter([&a](ManualARTCCAddition item) {return item.artcc == a;}) |
				std::ranges::to<std::vector>();
		}
		else {
			ret = artcc_wide_additions;
		}
		return ret;
	}
	std::optional<fs::path> FAAChartProcessor::GetCWTItemPath() const noexcept {
		if (cwt_file.empty()) {
			return std::nullopt;
		}
		else {
			return cwt_file;
		}
	}

	void FAAChartProcessor::WriteManualCharts(const std::vector<ManualXMLTag>& records) {
		using Win64Wrapper::MessageBoxStyles;
		//Guard against a catastrophic overwrite: writing an EMPTY set over a populated config. This happens
		//when every custom chart's source file is momentarily unreachable (e.g. an offline/renamed drive) so
		//ParseManualCharts drops them all, the dialog looks empty, and Load would then erase everything.
		//Deleting a single chart still writes the remaining N-1, so this only trips on a true wipe.
		if (records.empty() && fs::exists(manchartxml)) {
			pugi::xml_document existing;
			if (existing.load_file(manchartxml.string().c_str())) {
				auto root = existing.child("custom_charts");
				if (root.child("chart") || root.child("artccrecord") || root.child("cwt")) {
					auto resp = Win64Wrapper::CreateMessageBox(
						L"This will remove ALL saved custom charts (the list is currently empty). If your chart "
						L"files are just on an unavailable drive, cancel, reconnect it, and reopen this window.\n\nProceed?",
						L"Custom Charts", nullptr, MessageBoxStyles::YesNo, MessageBoxStyles::IconWarning, MessageBoxStyles::DefaultButton2);
					if (resp != Win64Wrapper::MessageBoxResponse::Yes) {
						return;
					}
				}
			}
		}
		//One-level backup before overwriting, so an unexpected loss is always recoverable.
		if (fs::exists(manchartxml)) {
			std::error_code ec;
			fs::copy_file(manchartxml, manchartxml.parent_path() / "custom_charts.bak.xml",
				fs::copy_options::overwrite_existing, ec);
		}
		pugi::xml_document doc;
		constexpr std::array<const char*, 7> nodes = { "custom_charts","chart","title","path","type","artccrecord","cwt" };
		constexpr std::array<const char*, 3> attributes = { "airport","artcc","class" };

		auto base_node = doc.append_child(nodes[0]);
		bool no_cwt_record = true;
		for (auto& r : records) {
			if (r.rectype == CustomRecordType::AirportItem) {
				if (r.name.empty() || (r.artcc == ARTCC::Empty) || r.airport.empty()) return;
				auto aprec = base_node.append_child(nodes[1]);
				aprec.append_child(nodes[2]).append_child(pugi::node_pcdata).set_value(r.name.c_str());
				aprec.append_child(nodes[3]).append_child(pugi::node_pcdata).set_value(r.filepath.string().c_str());
				if (r.ctype != ChartType::MANUAL) {
					aprec.append_child(nodes[4]).append_child(pugi::node_pcdata).set_value(SQLiteORM_conv_func::chart_type_str.at(r.ctype));
				}
				aprec.append_attribute(attributes[0]).set_value(r.airport);
				aprec.append_attribute(attributes[1]).set_value(artcc_names_map.at(r.artcc));
				aprec.append_attribute(attributes[2]).set_value(std::string{r.cls});
			}
			else if (r.rectype == CustomRecordType::ARTCCItem) {
				if (r.name.empty() || (r.artcc == ARTCC::Empty)) return;
				auto artccrecord = base_node.append_child(nodes[5]);
				artccrecord.append_child(nodes[2]).append_child(pugi::node_pcdata).set_value(r.name.c_str());
				artccrecord.append_child(nodes[3]).append_child(pugi::node_pcdata).set_value(r.filepath.string().c_str());
				artccrecord.append_attribute(attributes[1]).set_value(artcc_names_map.at(r.artcc));
			}
			if (r.rectype == CustomRecordType::CWT && no_cwt_record) {
				//take the first cwt record only
				base_node.append_child(nodes[6]).append_child(nodes[3]).append_child(pugi::node_pcdata).set_value(r.filepath.string().c_str());
			}
		}
		pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
		decl.append_attribute("version") = "1.0";
		decl.append_attribute("encoding") = "utf-8";
		doc.save_file(manchartxml.c_str());
	}
	std::vector<ManualXMLTag> FAAChartProcessor::GenerateListViewItems() {
		std::vector<ManualXMLTag> ret;
		//First CWT, then ARTCC, then Airport
		if (!cwt_file.empty()) {
			ret.emplace_back(cwt_file);
		}
		if (!artcc_wide_additions.empty()) {
			for (auto& [artcc, name, pth] : artcc_wide_additions) {
				ret.emplace_back(CustomRecordType::ARTCCItem, pth, name, artcc);
			}
		}
		auto manadd_copy = manual_additions;
		if (!manual_higher_class_additions.empty()) {
			for (auto& [ap,cls] : manual_higher_class_additions) {
				auto it = std::ranges::find_if(manadd_copy, [&ap](const ChartRecord& cr) {
					if (ap == cr.airport_id) {
						return true;
					}
					else {
						return false;
					}
					});
				ret.emplace_back(CustomRecordType::AirportItem, it->chartpath, it->procedure_name, it->artcc, it->airport_id, it->procedure_type, cls);
				manadd_copy.erase(it);
			}
		}
		if (!manadd_copy.empty()) {
			for (auto& i : manadd_copy) {
				ret.emplace_back(CustomRecordType::AirportItem, i.chartpath, i.procedure_name, i.artcc, i.airport_id, i.procedure_type);
			}
		}
		return ret;
	}
	void FAAChartProcessor::ClearAllAdditions() {
		auto db = GetDatabaseHandle();
		if (!manual_additions.empty()) {
			auto guard = db.transaction_guard();
			db.remove_all<ChartRecord>(sql::where(sql::is_equal(&ChartRecord::useradded, true)));
			db.remove_all<AirportRecord>(sql::where(sql::is_equal(&AirportRecord::useradded, true)));
			guard.commit();
			manual_additions.clear();
			manual_higher_class_additions.clear();
		}
		artcc_wide_additions.clear();
		cwt_file.clear();
	}

#ifdef _DEBUG
	void FAAChartProcessor::TestFunc() {
		OutputDebugStringW(L"FAAChartProcessorAccessed\n");
	}
#endif
}