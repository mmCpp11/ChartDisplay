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
	enum class DoDownloadMode {
		airac_dates,
		charts
	};
	//executable_path: path to ChartDisplayDownloadHelper.exe
	//outfile_path: temp xml to write data to or (for charts) directory to download files to
	bool DoDownload(DoDownloadMode obj, const std::filesystem::path& outpath, const std::filesystem::path& exe_path,
		std::optional<std::chrono::year_month_day> airacdate = std::nullopt);
	//Defines the schema and creates the database if not already created
	//Returns: a complex type handle to the sqlite_orm database
	auto GetDatabaseHandle();

	template<typename InputString, typename FormatString, typename ChronoType>
	ChronoType chrono_parse(InputString str, FormatString fmt, ChronoType out) {
		std::istringstream is{ str };
		is.imbue(std::locale("en_US.utf-8"));
		is >> std::chrono::parse(fmt, out);
		if (is.fail()) {
			throw std::exception("std::chrono::parse() failure");
		}
		return out;
	}
	std::chrono::year_month_day GetCurrentDate();

	struct NoDataException : std::exception {
		NoDataException(const char* msg) noexcept :message(msg) {}
		[[nodiscard]] virtual const char* what() const noexcept override {
			return message.c_str();
		}
	private:
		std::string message;
	};

	class AiracDates {
	public:
		struct AIRACInfo {
			long long cycle_id;
			std::chrono::year_month_day date;
			//here to use emplace_back, even though it should work otherwise
			AIRACInfo(long long cyclenum, std::chrono::year_month_day cycledate) :cycle_id(cyclenum), date(cycledate) {}
			AIRACInfo() :cycle_id(0) {} //ensure zero-initalization for cycle_id
		};

		AiracDates(std::optional<std::filesystem::path> downloader=std::nullopt);

		void UpdateAIRACDateList();
		[[nodiscard]] const AIRACInfo GetCurrentAiracCycle();
		[[nodiscard]] const AIRACInfo GetPreviousAiracCycle();
		[[nodiscard]] const AIRACInfo GetNextAiracCycle();
	private:

		//Get the XHTML, grab the tables and parse out the dates and cycle numbers
		void GetAIRACDates();
		void RetrieveData();
		//cpf_airac_dates_only: do not update next_airacdates_update (basically just update current,next and previous airac)
		void CommitData(bool control_only = false);
		//populate the previous, current and next AIRAC data members based off the stored cycles and the current date
		void PopulateReferenceAIRAC();
		std::vector<AIRACInfo> cycles;
		//read from file or parsed from AIRAC List, one before the last one on the list
		AIRACInfo next_autoupdate;
		AIRACInfo current;
		AIRACInfo previous;
		AIRACInfo next;
		std::filesystem::path downloader_path;
		std::regex airacnumber_regex;
		std::regex airacdate_regex;
		std::regex filedate_regex;
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
	//get this from the TPP xml file
	export struct ChartRecord {
		long pid;
		ARTCC artcc;
		std::string airport_id;
		ChartType procedure_type;
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
		wchar_t cls;
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
		//downloader path is the path to ChartDisplayDownloadHelper.exe if not standard
		FAAChartProcessor(std::optional<std::filesystem::path> downloader=std::nullopt);
		//force overrides no_download
		bool UpdateCharts(bool no_download = false, bool force=false);
		~FAAChartProcessor();
		ARTCCInfo GetAirspaceClassInfo(ARTCC artcc);
		//returns empty vector if the charts could not be returned
		std::vector<ChartRecord> GetAirportCharts(std::string airport_id);
		std::vector<ChartType> GetAirportChartType(std::string airport_id);
		//pass in a value to set the autoupdate state, otherwise, returns the current autoupdate state. Returns nullopt for a set operation
		std::optional<bool> AutoupdateState(std::optional<bool> set_autoupdate = std::nullopt);
		//get any additional artcc-wide items passed in the xml file. Returns empty vector if nothing found
		std::vector<ManualARTCCAddition> GetARTCCAdditions(std::optional<ARTCC> artcc = std::nullopt) const noexcept;
		void ReloadManualCharts();
		void WriteManualCharts(const std::vector<ManualXMLTag>& records);
		std::vector<ManualXMLTag> GenerateListViewItems();
		std::optional<std::filesystem::path> GetCWTItemPath() const noexcept;
		void ClearAllAdditions();
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
		std::filesystem::path downloader_path;
		AiracDates current_cycle;
		//tempdir: path to temp directory nasr data is downloaded to. Adds NASR data to database and then charts
		void GetChartsAndOrganize(const std::filesystem::path& tempdir);
		void CleanCharts(const std::filesystem::path& tempdir,bool keep_temp=false);
		void ParseManualCharts();
		AiracDates::AIRACInfo GetLastChartUpdate() const;
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