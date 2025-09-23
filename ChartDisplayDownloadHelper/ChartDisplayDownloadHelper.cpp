// Copyright (C) 2025 Matthew Moran
//
// ChartDisplayDownloadHelper is free software; 
// you can redistribute it and/or modify it under the
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
// 
// Usage: .EXE airac_dates temp_xml_file  OR  charts temp_download_dir airac_effective_date (yy-mm-dd)
#ifdef _MSVC_LANG
#if _MSVC_LANG < 202302L
#error "Requires C++23"
#endif
#elif __cplusplus < 202302L
#error "Requires C++23"
#endif

#include <cpr/cpr.h>
#include <memory>
#include <string>
#include <string_view>
#include <print>
#include <filesystem>
#include <chrono>
#include <regex>
#include <sstream>
#include <iostream>
#include <thread>
#include <fstream>


int main(int argc, char** argv) {
	if (argc >= 3) {
		std::println("Usage: ChartDisplayDownloadHelper.exe <airac_dates|charts> <filepath> <date> (for charts");
	}
	std::string type(argv[1]);
	if (type == "airac_dates") {
		std::string outfilepath(argv[2]);
		auto r = cpr::Get(cpr::Url{ "https://www.nm.eurocontrol.int/RAD/common/airac_dates.html" });
		//HTML specifies Success can be a code anywhere from 200-299
		if (!(r.status_code >= 200 && r.status_code < 300)) {
			return r.status_code;
		}
		std::ofstream outfile(outfilepath,std::ios::out|std::ios::trunc);
		if (!outfile) {
			return 10;
		}
		outfile.write(r.text.c_str(), r.text.size());
	}
	else if (type == "charts") {
		//First download NASR data (APT and CLS_ARSP data)
		std::filesystem::path outfiledir(argv[2]);
		std::istringstream sbuf(argv[3]);
		sbuf.imbue(std::locale("en_US.utf-8"));
		std::chrono::year_month_day cdate;
		sbuf >> std::chrono::parse("%Y-%m-%d", cdate);
		if (sbuf.fail() || !cdate.ok()) {
			std::println("Date must be valid and of the form dd-mm-yy from std::chrono");
			return 1;
		}
		auto nasr_date_string = std::format("{:%d_%b_%Y}", cdate);
		std::array<cpr::Url, 2> nasr_urls;
		std::string base_url("https://nfdc.faa.gov/webContent/28DaySub/extra/");
		nasr_urls[0] = base_url + nasr_date_string + "_APT_CSV.zip";
		nasr_urls[1] = base_url + nasr_date_string + "_CLS_ARSP_CSV.zip";
		cpr::Session session;
		for (auto& i : nasr_urls) {
			session.SetUrl(i);
			auto resp=session.Get();
			if (!(resp.status_code >= 200 && resp.status_code < 300)) {
				return resp.status_code;
			}
			std::filesystem::path outfn;
			if (i.str().contains("APT")) {
				outfn = outfiledir / "APT_CSV.zip";
			}
			else if (i.str().contains("ARSP")) {
				outfn = outfiledir / "CLS_CSV.zip";
			}
			std::ofstream out(outfn,std::ios::binary);
			out.write(resp.text.c_str(), resp.text.size());
		}
		//then download chart data
		std::string tppletters("ABCDE");
		for (const auto& i : tppletters) {
			auto tppurl = std::format("https://aeronav.faa.gov/upload_313-d/terminal/DDTPP{}_{:%y%m%d}.zip", i, cdate);
			auto tppfnfmt = std::format("TPP{}.zip", i);
			auto tppfn = outfiledir / tppfnfmt;
			std::ofstream outf(tppfn,std::ios::binary);
			if (!outf) {
				return 10;
			}
			auto cb = [&outf](const std::string_view& data, intptr_t userdata) {
				outf.write(data.data(), data.size());
				if (!outf) {
					return false;
				}
				else {
					return true;
				}
			};
			session.SetUrl(tppurl);
			session.SetWriteCallback(cpr::WriteCallback{ cb });
			auto tppres=session.Get();
			if (!(tppres.status_code >= 200 && tppres.status_code < 300)) {
				return tppres.status_code;
			}
			using namespace std::chrono_literals;
			std::this_thread::sleep_for(10s);
		}
	}
	else {
		std::println("First parameter can only be airac_dates or charts.");
		return 1;
	}
}