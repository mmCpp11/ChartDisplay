// Copyright (C) 2025-2026 Matthew Moran
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
// Small WinHTTP-based HTTP GET wrapper. Replaces the external cpr/libcurl helper,
module;
#if !defined(_WIN64) || !defined(_UNICODE)
#error Downloader module supports only Win64 with Unicode
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
//must follow bcrypt.h
#include <wil/resource.h>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")

export module Downloader;

import std;
namespace Win64Wrapper
{
    namespace Net
    {
        namespace
        {
            //RAII for WinHTTP HINTERNET handles.
            using HInternet = wil::unique_winhttp_hinternet;
        }

        export struct DownloadResult
        {
            bool transport_ok = false; //true if the HTTP exchange completed (regardless of status code)
            unsigned status_code = 0; //HTTP status, or 0 if the exchange never completed
            std::wstring error; //human-readable transport error, when transport_ok is false
            //true only for a completed 2xx response
            explicit operator bool() const noexcept
            {
                return transport_ok && status_code >= 200 && status_code < 300;
            }
        };

        //(bytes_received_so_far, total_bytes_or_0_if_unknown) -> return false to abort the download
        export using DownloadProgress = std::function<bool(unsigned long long, unsigned long long)>;

        namespace
        {
            //Shared GET core. If dest has a value the body is streamed to that file; else if body_sink is non-null
            //the body is accumulated in memory there. If range_probe is true only the first byte is requested
            //(availability check) and nothing is written.
            DownloadResult HttpGet(const std::wstring& url, const std::optional<std::filesystem::path>& dest,
                                   std::string* body_sink, bool range_probe, const DownloadProgress& progress)
            {
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
                if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &uc))
                {
                    result.error = L"WinHttpCrackUrl failed";
                    return result;
                }
                std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
                std::wstring path(uc.lpszUrlPath, uc.dwUrlPathLength);
                HInternet session(WinHttpOpen(L"ChartDisplay/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                              WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
                if (!session)
                {
                    result.error = L"WinHttpOpen failed";
                    return result;
                }
                HInternet connect(WinHttpConnect(session.get(), host.c_str(), uc.nPort, 0));
                if (!connect)
                {
                    result.error = L"WinHttpConnect failed";
                    return result;
                }
                DWORD reqflags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
                HInternet request(WinHttpOpenRequest(connect.get(), L"GET", path.c_str(),
                                                     nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                     reqflags));
                if (!request)
                {
                    result.error = L"WinHttpOpenRequest failed";
                    return result;
                }
                if (range_probe)
                {
                    //single byte is enough to confirm the resource exists without downloading it
                    WinHttpAddRequestHeaders(request.get(), L"Range: bytes=0-0", static_cast<DWORD>(-1),
                                             WINHTTP_ADDREQ_FLAG_ADD);
                }
                if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                        WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
                {
                    result.error = L"WinHttpSendRequest failed";
                    return result;
                }
                if (!WinHttpReceiveResponse(request.get(), nullptr))
                {
                    result.error = L"WinHttpReceiveResponse failed";
                    return result;
                }
                DWORD status = 0, statussz = sizeof(status);
                if (!WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                         WINHTTP_HEADER_NAME_BY_INDEX, &status, &statussz, WINHTTP_NO_HEADER_INDEX))
                {
                    result.error = L"WinHttpQueryHeaders (status) failed";
                    return result;
                }
                result.transport_ok = true;
                result.status_code = status;
                //a probe, or any non-2xx, never writes a file
                if (range_probe || !(status >= 200 && status < 300))
                {
                    return result;
                }
                //content length is best-effort (32-bit query; may be absent or wrong for >4GB bodies)
                unsigned long long total = 0;
                DWORD clen = 0, clensz = sizeof(clen);
                if (WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                                        WINHTTP_HEADER_NAME_BY_INDEX, &clen, &clensz, WINHTTP_NO_HEADER_INDEX))
                {
                    total = clen;
                }
                std::ofstream out;
                if (dest)
                {
                    out.open(*dest, std::ios::binary | std::ios::trunc);
                    if (!out)
                    {
                        result.transport_ok = false;
                        result.error = L"Unable to open output file";
                        return result;
                    }
                }
                std::vector<char> buffer(64 * 1024);
                unsigned long long received = 0;
                for (;;)
                {
                    DWORD avail = 0;
                    if (!WinHttpQueryDataAvailable(request.get(), &avail))
                    {
                        result.transport_ok = false;
                        result.error = L"WinHttpQueryDataAvailable failed";
                        return result;
                    }
                    if (avail == 0) break; //end of response body
                    if (avail > buffer.size()) buffer.resize(avail);
                    DWORD read = 0;
                    if (!WinHttpReadData(request.get(), buffer.data(), avail, &read))
                    {
                        result.transport_ok = false;
                        result.error = L"WinHttpReadData failed";
                        return result;
                    }
                    if (read == 0) break;
                    if (dest)
                    {
                        out.write(buffer.data(), read);
                        if (!out)
                        {
                            result.transport_ok = false;
                            result.error = L"Write to output file failed";
                            return result;
                        }
                    }
                    else if (body_sink)
                    {
                        body_sink->append(buffer.data(), read);
                    }
                    received += read;
                    if (progress && !progress(received, total))
                    {
                        result.transport_ok = false;
                        result.error = L"Download canceled";
                        return result;
                    }
                }
                return result;
            }
        }

        //Download url to dest, streaming the body to disk. Optional progress callback (return false to cancel).
        export DownloadResult HttpDownloadToFile(const std::wstring& url, const std::filesystem::path& dest,
                                                 const DownloadProgress& progress = {})
        {
            return HttpGet(url, dest, nullptr, false, progress);
        }

        //GET url and return the response body in memory (out_body). For small responses (e.g. a JSON API reply),
        //not multi-GB downloads. Optional progress callback (return false to cancel).
        export DownloadResult HttpGetToString(const std::wstring& url, std::string& out_body,
                                              const DownloadProgress& progress = {})
        {
            return HttpGet(url, std::nullopt, &out_body, false, progress);
        }

        //Probe url with a single-byte range request (no file written). Inspect status_code for availability.
        export DownloadResult HttpProbe(const std::wstring& url)
        {
            return HttpGet(url, std::nullopt, nullptr, true, {});
        }

        //RAII for a Win32 file handle. unique_hfile is unique_any_handle_invalid, so its empty state is
        //INVALID_HANDLE_VALUE rather than null
        export using FileHandle = wil::unique_hfile;

        export struct VerifyResult
        {
            bool signature_ok = false;
            //Why the check could not be run. Empty with signature_ok false means the check ran and the signature
            //did not match
            std::wstring error;
            //Locked verified file to ensure that it is not swapped after verification
            FileHandle locked_file;
            explicit operator bool() const noexcept { return signature_ok; }
        };

        namespace
        {
            //unique_bcrypt_algorithm closes via BCryptCloseAlgorithmProviderNoFlags, which supplies the
            //flags argument CNG requires. unique_any skips the closer on a null handle, so the null
            //checks the hand-written deleters did are already covered.
            using AlgHandle = wil::unique_bcrypt_algorithm;
            using HashHandle = wil::unique_bcrypt_hash;
            using KeyHandle = wil::unique_bcrypt_key;

            //NTSTATUS reports success opposite of HRESULT. To avoid an annoying include,
            //extract necessary machinery here
            constexpr bool NtOk(NTSTATUS status) noexcept { return status >= 0; }
            constexpr NTSTATUS kNtInvalidSignature = static_cast<NTSTATUS>(0xC000A000UL);
            //A signature is a few hundred bytes; the cap stops a wrong URL from becoming a huge allocation.
            constexpr std::uintmax_t kMaxSignatureBytes = 64 * 1024;

            std::optional<std::vector<UCHAR>> ReadSignatureFile(const std::filesystem::path& path, std::wstring& error)
            {
                std::error_code ec;
                const auto size = std::filesystem::file_size(path, ec);
                if (ec)
                {
                    error = L"Unable to size the signature file";
                    return std::nullopt;
                }
                if (size == 0 || size > kMaxSignatureBytes)
                {
                    error = std::format(L"Signature file is an implausible size ({} bytes)", size);
                    return std::nullopt;
                }
                std::ifstream in(path, std::ios::binary);
                if (!in)
                {
                    error = L"Unable to open the signature file";
                    return std::nullopt;
                }
                std::vector<UCHAR> bytes(static_cast<std::size_t>(size));
                in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
                if (in.gcount() != static_cast<std::streamsize>(bytes.size()))
                {
                    error = L"Short read of the signature file";
                    return std::nullopt;
                }
                return bytes;
            }

            //Streams the file through SHA-256, so the installer never has to be resident to be hashed.
            bool HashFile(HANDLE file, std::vector<UCHAR>& digest, std::wstring& error)
            {
                AlgHandle alg;
                if (!NtOk(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0)))
                {
                    error = L"BCryptOpenAlgorithmProvider(SHA256) failed";
                    return false;
                }
                DWORD hash_len = 0;
                DWORD copied = 0;
                if (!NtOk(BCryptGetProperty(alg.get(), BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hash_len),
                                            sizeof(hash_len), &copied, 0)))
                {
                    error = L"BCryptGetProperty(HASH_LENGTH) failed";
                    return false;
                }
                digest.assign(hash_len, 0);
                HashHandle hash;
                //A null hash-object buffer asks CNG to allocate one itself (Windows 8 and later).
                if (!NtOk(BCryptCreateHash(alg.get(), &hash, nullptr, 0, nullptr, 0, 0)))
                {
                    error = L"BCryptCreateHash failed";
                    return false;
                }
                //The handle is the caller's, so its file pointer is not assumed to be at the start.
                if (LARGE_INTEGER origin{}; !SetFilePointerEx(file, origin, nullptr, FILE_BEGIN))
                {
                    error = L"SetFilePointerEx failed";
                    return false;
                }
                std::vector<UCHAR> buffer(64 * 1024);
                for (;;)
                {
                    DWORD read = 0;
                    if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr))
                    {
                        error = L"ReadFile failed while hashing";
                        return false;
                    }
                    if (read == 0) break; //end of file
                    if (!NtOk(BCryptHashData(hash.get(), buffer.data(), read, 0)))
                    {
                        error = L"BCryptHashData failed";
                        return false;
                    }
                }
                if (!NtOk(BCryptFinishHash(hash.get(), digest.data(), static_cast<ULONG>(digest.size()), 0)))
                {
                    error = L"BCryptFinishHash failed";
                    return false;
                }
                return true;
            }
        }

        //Checks a detached signature over file: SHA-256 hash, RSA PKCS#1 v1.5.
        //public_key_blob stored in the binary
        export VerifyResult VerifyDetachedSignature(const std::filesystem::path& file,
                                                    std::span<const UCHAR> signature,
                                                    std::span<const UCHAR> public_key_blob)
        {
            VerifyResult result;
            //FILE_SHARE_READ alone: for as long as this handle lives nothing else can write to or delete the
            //file, which is what keeps the answer true through to the moment the caller launches it.
            FileHandle target(CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                          FILE_ATTRIBUTE_NORMAL, nullptr));
            if (!target)
            {
                result.error = L"Unable to open the file for verification";
                return result;
            }
            if (signature.empty())
            {
                result.error = L"The signature is empty";
                return result;
            }
            std::vector<UCHAR> digest;
            if (!HashFile(target.get(), digest, result.error)) return result;

            AlgHandle rsa;
            if (!NtOk(BCryptOpenAlgorithmProvider(&rsa, BCRYPT_RSA_ALGORITHM, nullptr, 0)))
            {
                result.error = L"BCryptOpenAlgorithmProvider(RSA) failed";
                return result;
            }
            KeyHandle key;
            //The blob is read only
            if (!NtOk(BCryptImportKeyPair(rsa.get(), nullptr, BCRYPT_RSAPUBLIC_BLOB, &key,
                                          const_cast<PUCHAR>(public_key_blob.data()),
                                          static_cast<ULONG>(public_key_blob.size()), 0)))
            {
                result.error = L"BCryptImportKeyPair failed: the public key blob is not a BCRYPT_RSAKEY_BLOB";
                return result;
            }
            BCRYPT_PKCS1_PADDING_INFO padding{ BCRYPT_SHA256_ALGORITHM };
            const NTSTATUS verified = BCryptVerifySignature(key.get(), &padding, digest.data(),
                                                           static_cast<ULONG>(digest.size()),
                                                           const_cast<PUCHAR>(signature.data()),
                                                           static_cast<ULONG>(signature.size()),
                                                           BCRYPT_PAD_PKCS1);
            //A mismatch is a valid answer rather than a malfunction. ECDSA triggers this too, so do not
            //use keys with that algorithm.
            if (verified == kNtInvalidSignature) return result;
            if (!NtOk(verified))
            {
                result.error = std::format(L"BCryptVerifySignature failed: 0x{:08X}",
                                           static_cast<unsigned long>(verified));
                return result;
            }
            result.signature_ok = true;
            result.locked_file = std::move(target);
            return result;
        }

        //Same check with the signature read from disk. Prefer the span overload as signatures are small.
        export VerifyResult VerifyDetachedSignature(const std::filesystem::path& file,
                                                    const std::filesystem::path& signature_file,
                                                    std::span<const UCHAR> public_key_blob)
        {
            VerifyResult result;
            const auto signature = ReadSignatureFile(signature_file, result.error);
            if (!signature) return result;
            return VerifyDetachedSignature(file, *signature, public_key_blob);
        }
    }
}