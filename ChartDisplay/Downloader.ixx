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
//
// Small WinHTTP-based HTTP GET wrapper. Replaces the external cpr/libcurl helper executable: WinHTTP is a
// Windows system component (no DLLs to deploy, nothing external to relocate) and coexists cleanly with
// windows.h, which the curl headers do not.
module;
#if !defined(_WIN64) || !defined(_UNICODE)
#error Downloader module supports only Win64 with Unicode
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

export module Downloader;

import std;

namespace Net {
    namespace {
        //RAII for WinHTTP HINTERNET handles
        struct WinHttpHandleDeleter {
            void operator()(HINTERNET h) const noexcept {
                if (h) WinHttpCloseHandle(h);
            }
        };
        using HInternet = std::unique_ptr<std::remove_pointer_t<HINTERNET>, WinHttpHandleDeleter>;
    }

    export struct DownloadResult {
        bool transport_ok = false;   //true if the HTTP exchange completed (regardless of status code)
        unsigned status_code = 0;    //HTTP status, or 0 if the exchange never completed
        std::wstring error;          //human-readable transport error, when transport_ok is false
        //true only for a completed 2xx response
        explicit operator bool() const noexcept {
            return transport_ok && status_code >= 200 && status_code < 300;
        }
    };

    //(bytes_received_so_far, total_bytes_or_0_if_unknown) -> return false to abort the download
    export using DownloadProgress = std::function<bool(unsigned long long, unsigned long long)>;

    namespace {
        //Shared GET core. If dest has a value the body is streamed to that file; else if body_sink is non-null
        //the body is accumulated in memory there. If range_probe is true only the first byte is requested
        //(availability check) and nothing is written.
        DownloadResult HttpGet(const std::wstring& url, const std::optional<std::filesystem::path>& dest,
            std::string* body_sink, bool range_probe, const DownloadProgress& progress) {
            DownloadResult result;
            //break the URL into host + path
            URL_COMPONENTS uc{};
            uc.dwStructSize = sizeof(uc);
            std::array<wchar_t, 256> hostbuf{};
            std::array<wchar_t, 4096> pathbuf{};
            uc.lpszHostName = hostbuf.data();
            uc.dwHostNameLength = static_cast<DWORD>(hostbuf.size());
            uc.lpszUrlPath = pathbuf.data();
            uc.dwUrlPathLength = static_cast<DWORD>(pathbuf.size());
            if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &uc)) {
                result.error = L"WinHttpCrackUrl failed";
                return result;
            }
            std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
            std::wstring path(uc.lpszUrlPath, uc.dwUrlPathLength);
            HInternet session(WinHttpOpen(L"ChartDisplay/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
            if (!session) { result.error = L"WinHttpOpen failed"; return result; }
            HInternet connect(WinHttpConnect(session.get(), host.c_str(), uc.nPort, 0));
            if (!connect) { result.error = L"WinHttpConnect failed"; return result; }
            DWORD reqflags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
            HInternet request(WinHttpOpenRequest(connect.get(), L"GET", path.c_str(),
                nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, reqflags));
            if (!request) { result.error = L"WinHttpOpenRequest failed"; return result; }
            if (range_probe) {
                //single byte is enough to confirm the resource exists without downloading it
                WinHttpAddRequestHeaders(request.get(), L"Range: bytes=0-0", static_cast<DWORD>(-1),
                    WINHTTP_ADDREQ_FLAG_ADD);
            }
            if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
                result.error = L"WinHttpSendRequest failed"; return result;
            }
            if (!WinHttpReceiveResponse(request.get(), nullptr)) {
                result.error = L"WinHttpReceiveResponse failed"; return result;
            }
            DWORD status = 0, statussz = sizeof(status);
            if (!WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &status, &statussz, WINHTTP_NO_HEADER_INDEX)) {
                result.error = L"WinHttpQueryHeaders (status) failed"; return result;
            }
            result.transport_ok = true;
            result.status_code = status;
            //a probe, or any non-2xx, never writes a file
            if (range_probe || !(status >= 200 && status < 300)) {
                return result;
            }
            //content length is best-effort (32-bit query; may be absent or wrong for >4GB bodies)
            unsigned long long total = 0;
            DWORD clen = 0, clensz = sizeof(clen);
            if (WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &clen, &clensz, WINHTTP_NO_HEADER_INDEX)) {
                total = clen;
            }
            std::ofstream out;
            if (dest) {
                out.open(*dest, std::ios::binary | std::ios::trunc);
                if (!out) { result.transport_ok = false; result.error = L"Unable to open output file"; return result; }
            }
            std::vector<char> buffer(64 * 1024);
            unsigned long long received = 0;
            for (;;) {
                DWORD avail = 0;
                if (!WinHttpQueryDataAvailable(request.get(), &avail)) {
                    result.transport_ok = false; result.error = L"WinHttpQueryDataAvailable failed"; return result;
                }
                if (avail == 0) break; //end of response body
                if (avail > buffer.size()) buffer.resize(avail);
                DWORD read = 0;
                if (!WinHttpReadData(request.get(), buffer.data(), avail, &read)) {
                    result.transport_ok = false; result.error = L"WinHttpReadData failed"; return result;
                }
                if (read == 0) break;
                if (dest) {
                    out.write(buffer.data(), read);
                    if (!out) { result.transport_ok = false; result.error = L"Write to output file failed"; return result; }
                }
                else if (body_sink) {
                    body_sink->append(buffer.data(), read);
                }
                received += read;
                if (progress && !progress(received, total)) {
                    result.transport_ok = false; result.error = L"Download canceled"; return result;
                }
            }
            return result;
        }
    }

    //Download url to dest, streaming the body to disk. Optional progress callback (return false to cancel).
    export DownloadResult HttpDownloadToFile(const std::wstring& url, const std::filesystem::path& dest,
        const DownloadProgress& progress = {}) {
        return HttpGet(url, dest, nullptr, false, progress);
    }

    //GET url and return the response body in memory (out_body). For small responses (e.g. a JSON API reply),
    //not multi-GB downloads. Optional progress callback (return false to cancel).
    export DownloadResult HttpGetToString(const std::wstring& url, std::string& out_body,
        const DownloadProgress& progress = {}) {
        return HttpGet(url, std::nullopt, &out_body, false, progress);
    }

    //Probe url with a single-byte range request (no file written). Inspect status_code for availability.
    export DownloadResult HttpProbe(const std::wstring& url) {
        return HttpGet(url, std::nullopt, nullptr, true, {});
    }
}
