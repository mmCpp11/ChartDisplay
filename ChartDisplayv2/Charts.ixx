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
export module Charts;
//Intellisense will mark non-exported functions as visible when importing the module, but cl correctly issues an error if one is used
import std;
import BasicWindowsWrapperModule;
namespace charts {
	export struct AIRACInfo {
		long long cycle_id;
		std::chrono::year_month_day date;
		//here to use emplace_back, even though it should work otherwise
		AIRACInfo(long long cyclenum, std::chrono::year_month_day cycledate) :cycle_id(cyclenum), date(cycledate) {}
		AIRACInfo() :cycle_id(0) {} //ensure zero-initalization for cycle_id
		explicit operator bool() {
			return cycle_id != 0;
		}
	};
	const size_t MAX_PER_OP = 800;
	export enum class ARTCC {
		Empty,
		ZAE, //Academy. For any special cases that don't fit into other ARTCCs
		ZAB,
		ZAN,
		ZTL,
		ZBW,
		ZAU,
		ZOB,
		ZDV,
		ZFW,
		HCF,
		ZHU,
		ZID,
		ZJX,
		ZKC,
		ZLA,
		ZME,
		ZMA,
		ZMP,
		ZNY,
		ZOA,
		ZLC,
		ZSE,
		ZSU,
		ZDC
	};
	export const std::unordered_map<ARTCC, std::string> artcc_names_map{
		{ARTCC::ZAE,"ZAE"},
		{ARTCC::ZAB,"ZAB"},
		{ARTCC::ZAN,"ZAN"},
		{ARTCC::ZTL,"ZTL"},
		{ARTCC::ZBW,"ZBW"},
		{ARTCC::ZAU,"ZAU"},
		{ARTCC::ZOB,"ZOB"},
		{ARTCC::ZDV,"ZDV"},
		{ARTCC::ZFW,"ZFW"},
		{ARTCC::HCF,"HCF"},
		{ARTCC::ZHU,"ZHU"},
		{ARTCC::ZID,"ZID"},
		{ARTCC::ZJX,"ZJX"},
		{ARTCC::ZKC,"ZKC"},
		{ARTCC::ZLA,"ZLA"},
		{ARTCC::ZME,"ZME"},
		{ARTCC::ZMA,"ZMA"},
		{ARTCC::ZMP,"ZMP"},
		{ARTCC::ZNY,"ZNY"},
		{ARTCC::ZOA,"ZOA"},
		{ARTCC::ZLC,"ZLC"},
		{ARTCC::ZSE,"ZSE"},
		{ARTCC::ZSU,"ZSU"},
		{ARTCC::ZDC,"ZDC"}
	};
	export const std::unordered_map<std::string, ARTCC> rev_artcc_names_map{
		{"ZAE",ARTCC::ZAE},
		{"ZAB",ARTCC::ZAB},
		{"ZAN",ARTCC::ZAN},
		{"ZAP",ARTCC::ZAN},//Anchorage Oceanic
		{"ZTL",ARTCC::ZTL},
		{"ZBW",ARTCC::ZBW},
		{"ZAU",ARTCC::ZAU},
		{"ZOB",ARTCC::ZOB},
		{"ZDV",ARTCC::ZDV},
		{"ZFW",ARTCC::ZFW},
		{"HCF",ARTCC::HCF},
		{"ZHN",ARTCC::HCF},//ZHN is the HCF ARTCC identifier
		{"ZUA",ARTCC::HCF},//ZUA is Guam Center, on VATSIM under the control of HCF
		{"ZHU",ARTCC::ZHU},
		{"ZID",ARTCC::ZID},
		{"ZJX",ARTCC::ZJX},
		{"ZKC",ARTCC::ZKC},
		{"ZLA",ARTCC::ZLA},
		{"ZME",ARTCC::ZME},
		{"ZMA",ARTCC::ZMA},
		{"ZMP",ARTCC::ZMP},
		{"ZNY",ARTCC::ZNY},
		{"ZOA",ARTCC::ZOA},
		{"ZAK",ARTCC::ZOA},
		{"ZLC",ARTCC::ZLC},
		{"ZSE",ARTCC::ZSE},
		{"ZSU",ARTCC::ZSU},
		{"ZDC",ARTCC::ZDC}
	};
	//outpath: directory to download the cycle's chart/NASR files to. Downloads in-process via WinHTTP.
	//Phase of a chart update, for the UI progress reporter below.
	export enum class UpdatePhase { Checking, Downloading, Organizing };
	export struct UpdateStatus {
		UpdatePhase phase;
		int file_index = 0;   //1-based; valid for Downloading
		int file_count = 0;
		std::wstring file_name;
	};
	//UI-agnostic progress sink. UpdateCharts/DoDownload call this (on the worker thread) to report status;
	//cancellation is separate, via the std::stop_token. Defaulted empty so non-UI callers are unaffected.
	export using UpdateReporter = std::function<void(const UpdateStatus&)>;
	//What kind of update the current DB state calls for (computed without side effects).
	export enum class UpdateNeed { None, Initial, AutoupdateStale };
	bool DoDownload(const std::filesystem::path& outpath, std::chrono::year_month_day airacdate,
		const UpdateReporter& report = {}, std::stop_token st = {});
	//Preflight: probes the FAA to see whether this cycle's files have been published, without downloading.
	//Returns 0 if all files are reachable, 44 if not yet published (HTTP 404), another HTTP code otherwise,
	//or -1 if the server could not be reached. Nothing is downloaded or modified.
	int CheckCycleAvailability(std::chrono::year_month_day airacdate);
	//Defines the schema and creates the database if not already created
	//Returns: a complex type handle to the sqlite_orm database
	auto GetDatabaseHandle();

	template<typename InputString, typename FormatString, typename ChronoType>
	ChronoType chrono_parse(InputString str, FormatString fmt, ChronoType out) {
		std::istringstream is{ str };
		is.imbue(std::locale("en_US.utf-8"));
		is >> std::chrono::parse(fmt, out);
		if (is.fail()) {
			throw std::runtime_error("std::chrono::parse() failure");
		}
		return out;
	}
	std::chrono::year_month_day GetCurrentDate();

	//thrown when a required data source (database rows, downloaded archive, parsed XML) is missing or unusable
	struct NoDataException : std::runtime_error {
		using std::runtime_error::runtime_error;
	};

	class AiracDates {
	public:

		AiracDates();

		void UpdateAIRACDateList();
		//populate the previous, current and next AIRAC data members based off the stored cycles and the current date
		void PopulateReferenceAIRAC();
		[[nodiscard]] const AIRACInfo GetCurrentAiracCycle() const noexcept;
		[[nodiscard]] const AIRACInfo GetPreviousAiracCycle() const noexcept;
		[[nodiscard]] const AIRACInfo GetNextAiracCycle() const noexcept;
	private:

		//Generate the AIRAC cycle list arithmetically (fixed 28-day cadence) from a base cycle:
		//the stored next_airacdates_update row if present, otherwise a hardcoded fallback base. No network access.
		void GetAIRACDates();
		void RetrieveData();
		//cpf_airac_dates_only: do not update next_airacdates_update (basically just update current,next and previous airac)
		void CommitData(bool control_only = false);
		std::vector<AIRACInfo> cycles;
		//second-to-last cycle in the generated list; when the current date passes it, the list is regenerated forward
		AIRACInfo next_autoupdate;
		AIRACInfo current;
		AIRACInfo previous;
		AIRACInfo next;
	};
	export enum class ChartType {
		APD,
		SID,
		ODP,
		STAR,
		IAP,
		MIN,
		LAHSO,
		HOT,
		DAU,
		MANUAL
	};
	export const std::unordered_map<std::string, ChartType> charttype_names_map{
		{"APD",ChartType::APD},
		{"DP",ChartType::SID},
		{"ODP",ChartType::ODP},
		//In cycle 2606, FAA metadata changed STAR to STR
		{"STR",ChartType::STAR},
		{"STAR",ChartType::STAR},
		{"IAP",ChartType::IAP},
		{"MIN",ChartType::MIN},
		{"LAH",ChartType::LAHSO},
		{"DAU",ChartType::DAU},
		{"HOT",ChartType::HOT},
		{"MAN",ChartType::MANUAL}
	};
	//list of airports in each class
	export struct ARTCCInfo {
		ARTCC artcc;
		std::string artccstr;
		std::vector<std::string> class_b_airports;
		std::vector<std::string> class_c_airports;
		std::vector<std::string> class_d_airports;
		std::vector<std::string> other_towered;
		std::vector<std::string> class_eg_untowered;
		//for airports whose FAA LID is a Windows reserved name, contains pairs of (AID,Modified filename AID) e.g. (NUL,NULX)
		std::vector<std::pair<std::string,std::string>> foldername_modification_airports;
	};
	struct ControlRecord {
		int pid = -1;
		std::string control_item;
		std::optional<long long> cycle;
		std::optional<std::chrono::year_month_day> date;
		std::optional<std::string> other;
	};
	//get this from nasr data
	struct AirportRecord {
		long pid;
		ARTCC artcc;
		std::string airport_id;
		std::string airport_id_filestring;
		char airspace_class;
		bool has_charts;
		bool useradded;
		AirportRecord() :pid(-1), airspace_class('E'), artcc(ARTCC::Empty),has_charts(false),useradded(false) {}
	};
	//Minimal exported view of a real airport for the custom-charts dialog (AirportRecord is internal).
	export struct AirportLookup {
		ARTCC artcc;
		char airspace_class;
	};
	//get this from the TPP xml file
	export struct ChartRecord {
		long pid;
		ARTCC artcc;
		std::string airport_id;
		ChartType procedure_type = ChartType::MANUAL; //default-member-init so a default-constructed record never has an indeterminate type
		std::string procedure_name;
		std::optional<std::string> procedure_computer_id;
		std::filesystem::path chartpath;
		bool useradded;
		ChartRecord() :pid(-1), artcc(ARTCC::Empty),useradded(false) {}
		//for emplace_back
		ChartRecord(std::string aid, ARTCC artcc, ChartType type, std::string name, std::filesystem::path filepath,
			std::optional<std::string> compcode = std::nullopt, bool useradd = false)
			:pid(-1), artcc(artcc), airport_id(aid), procedure_type(type), procedure_name(name), chartpath(filepath), procedure_computer_id(compcode),
			useradded(useradd) {}
	};
	export struct ManualARTCCAddition {
		ARTCC artcc;
		std::string name;
		std::filesystem::path filepath;
		ManualARTCCAddition(ARTCC artcc=ARTCC::Empty,std::string name="", std::filesystem::path p = "")
			:artcc(artcc),name(name),filepath(p) {}
	};
	export enum class CustomRecordType {
		AirportItem,
		ARTCCItem,
		CWT
	};
	export struct ManualXMLTag {
		CustomRecordType rectype;
		std::string airport;
		ARTCC artcc;
		ChartType ctype;
		char cls;
		std::string name;
		std::filesystem::path filepath;
		ManualXMLTag(CustomRecordType rectype, const std::filesystem::path& fp,std::optional<std::string> name,
			std::optional<ARTCC> atc, std::optional<std::string> ap=std::nullopt, std::optional<ChartType> ct=std::nullopt,std::optional<wchar_t> cls=std::nullopt)
			:rectype(rectype),airport(ap.value_or("")),ctype(ct.value_or(ChartType::MANUAL)),
			name(name.value_or("")),filepath(fp),artcc(atc.value_or(ARTCC::Empty)),cls(cls.value_or(L'E')) {}
		ManualXMLTag(const std::filesystem::path& fp)
			:ManualXMLTag(CustomRecordType::CWT,fp,std::nullopt,std::nullopt,std::nullopt,std::nullopt) {}
	};

	export class FAAChartProcessor {
	public:
		//option to add manual charts for things like old taxi diagrams or procedures. All manual charts must have ChartType::MANUAL
		//charts will be moved from location specified by chartpath to MANUAL subdirectory next to all the other ARTCCs
		//If chartpath does not exist or ChartType is not manual, it will be ignored.
		FAAChartProcessor();
		//force overrides no_download. report/st are optional: report sinks progress text, st cancels.
		bool UpdateCharts(bool no_download = false, bool force = false,
			const UpdateReporter& report = {}, std::stop_token st = {});
		//What kind of update the current DB state calls for, computed without side effects so the UI can
		//decide whether to launch the (worker-thread) update after the main window exists.
		UpdateNeed ChartUpdateNeeded() const;
		~FAAChartProcessor();
		ARTCCInfo GetAirspaceClassInfo(ARTCC artcc);
		//returns empty vector if the charts could not be returned
		std::vector<ChartRecord> GetAirportCharts(std::string airport_id);
		std::vector<ChartType> GetAirportChartType(std::string airport_id);
		//The real (NASR, non-user-added) airport for an FAA LID, or nullopt. The custom-charts dialog uses
		//it to derive ARTCC/airspace class for a real airport, and to reject a "custom" LID that is real.
		std::optional<AirportLookup> GetRealAirportByLID(std::string lid) const;
		//pass in a value to set the autoupdate state, otherwise, returns the current autoupdate state. Returns nullopt for a set operation
		std::optional<bool> AutoupdateState(std::optional<bool> set_autoupdate = std::nullopt);
		//get any additional artcc-wide items passed in the xml file. Returns empty vector if nothing found
		std::vector<ManualARTCCAddition> GetARTCCAdditions(std::optional<ARTCC> artcc = std::nullopt) const noexcept;
		void ReloadManualCharts();
		void WriteManualCharts(const std::vector<ManualXMLTag>& records);
		std::vector<ManualXMLTag> GenerateListViewItems();
		std::optional<std::filesystem::path> GetCWTItemPath() const noexcept;
		void ClearAllAdditions();
		AIRACInfo GetLastChartUpdate() const;
#ifdef _DEBUG
		void TestFunc();
#endif
	private:
		std::filesystem::path chartdir;
		std::filesystem::path manchartxml;
		std::filesystem::path cwt_file;
		std::vector<ChartRecord> manual_additions;
		std::vector<std::pair<std::string, char>> manual_higher_class_additions;
		std::vector<ManualARTCCAddition> artcc_wide_additions;
		AiracDates current_cycle;
		//tempdir: path to temp directory nasr data is downloaded to. Adds NASR data to database and then charts
		void GetChartsAndOrganize(const std::filesystem::path& tempdir);
		void CleanCharts(const std::filesystem::path& tempdir,bool keep_temp=false);
		void ParseManualCharts();
	};
}
//for std::format
template<>
struct std::formatter<charts::ARTCC> {
	std::formatter<std::string> _formatter;
	constexpr auto parse(std::format_parse_context& parse_context) {
		return _formatter.parse(parse_context);
	}
	auto format(const charts::ARTCC& artcc,std::format_context& format_context) const {
//	std:;string mapval = charts::artcc_names_map.at(artcc);
		std::string output = std::format("{}", charts::artcc_names_map.at(artcc));
		return _formatter.format(output, format_context);
	}
};

template<>
struct std::formatter<charts::ChartType> {
	std::formatter<std::string> _formatter;
	constexpr auto parse(std::format_parse_context& parse_context) {
		return _formatter.parse(parse_context);
	}
	auto format(const charts::ChartType& ct, std::format_context& format_context) const {
		std::string output;
		for (auto& [str, ect] : charts::charttype_names_map) {
			if (ct == ect) {
				output = std::format("{}", str);
				break;
			}
		}
		return _formatter.format(output, format_context);
	}
};