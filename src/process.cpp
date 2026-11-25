#include "process.hpp"

#include <tlhelp32.h>

#include <algorithm>
#include <cctype>
#include <iterator>

#include "config.hpp"

namespace rr::win {

std::string narrow(const std::wstring& wide) {
    if (wide.empty()) return {};
    const int needed = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                                           static_cast<int>(wide.size()), nullptr, 0,
                                           nullptr, nullptr);
    if (needed <= 0) return {};
    std::string out(static_cast<std::size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), out.data(),
                        needed, nullptr, nullptr);
    return out;
}

std::string to_lower_copy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

static std::string file_name_of(const std::string& path) {
    const std::size_t cut = path.find_last_of("\\/");
    if (cut == std::string::npos) return path;
    return path.substr(cut + 1);
}

std::optional<DWORD> find_process(const std::string& exe_name_lower) {
    Handle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snap.valid()) return std::nullopt;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (!Process32FirstW(snap.get(), &entry)) return std::nullopt;
    do {
        const std::string name = to_lower_copy(narrow(entry.szExeFile));
        if (name == exe_name_lower) return entry.th32ProcessID;
    } while (Process32NextW(snap.get(), &entry));
    return std::nullopt;
}

std::vector<std::string> list_processes() {
    std::vector<std::string> out;
    Handle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snap.valid()) return out;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (!Process32FirstW(snap.get(), &entry)) return out;
    do {
        out.push_back(to_lower_copy(narrow(entry.szExeFile)));
    } while (Process32NextW(snap.get(), &entry));
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

bool Process::open(DWORD pid) {
    handle_.close();
    pid_ = 0;
    wow64_ = false;

    Handle h(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid));
    if (!h.valid()) return false;

    BOOL wow = FALSE;
    if (IsWow64Process(h.get(), &wow)) wow64_ = (wow != FALSE);

    handle_ = std::move(h);
    pid_ = pid;
    return true;
}

bool Process::alive() const {
    if (!handle_.valid()) return false;
    DWORD code = 0;
    if (!GetExitCodeProcess(handle_.get(), &code)) return false;
    return code == STILL_ACTIVE;
}

std::string Process::image_path() const {
    if (!handle_.valid()) return {};
    wchar_t buffer[MAX_PATH * 2]{};
    DWORD size = static_cast<DWORD>(std::size(buffer));
    if (!QueryFullProcessImageNameW(handle_.get(), 0, buffer, &size)) return {};
    return narrow(std::wstring(buffer, size));
}

std::vector<ModuleInfo> Process::modules() const {
    std::vector<ModuleInfo> out;
    if (pid_ == 0) return out;

    Handle snap(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid_));
    if (!snap.valid()) return out;

    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (!Module32FirstW(snap.get(), &entry)) return out;
    do {
        ModuleInfo info;
        info.path = narrow(entry.szExePath);
        info.name = to_lower_copy(file_name_of(narrow(entry.szModule)));
        info.base = reinterpret_cast<std::uint64_t>(entry.modBaseAddr);
        info.size = static_cast<std::size_t>(entry.modBaseSize);
        out.push_back(std::move(info));
    } while (Module32NextW(snap.get(), &entry));
    return out;
}

std::optional<ModuleInfo> Process::find_module(const std::string& name_lower) const {
    for (const auto& m : modules()) {
        if (m.name == name_lower) return m;
    }
    return std::nullopt;
}

bool Process::read_raw(std::uint64_t address, void* dst, std::size_t length) const {
    if (!handle_.valid() || dst == nullptr || length == 0) return false;
    if (!config::plausible_pointer(address)) return false;
    if (address > config::kUserSpaceMax - length) return false;

    SIZE_T moved = 0;
    const BOOL ok = ReadProcessMemory(handle_.get(), reinterpret_cast<LPCVOID>(address), dst,
                                      length, &moved);
    return ok != FALSE && moved == length;
}

std::optional<ImageBuffer> Process::read_image(const ModuleInfo& mod) const {
    if (!handle_.valid()) return std::nullopt;
    if (mod.size == 0 || mod.size > config::kMaxImageBytes) return std::nullopt;
    if (!config::plausible_pointer(mod.base)) return std::nullopt;

    ImageBuffer buf;
    buf.page_size = config::kPageSize;
    buf.bytes.assign(mod.size, 0);
    const std::size_t pages = (mod.size + buf.page_size - 1) / buf.page_size;
    buf.page_present.assign(pages, 0);

    for (std::size_t p = 0; p < pages; ++p) {
        const std::size_t offset = p * buf.page_size;
        const std::size_t want = std::min(buf.page_size, mod.size - offset);
        SIZE_T moved = 0;
        const BOOL ok = ReadProcessMemory(handle_.get(),
                                          reinterpret_cast<LPCVOID>(mod.base + offset),
                                          buf.bytes.data() + offset, want, &moved);
        if (ok != FALSE && moved == want) {
            buf.page_present[p] = 1;
        } else {
            std::fill_n(buf.bytes.data() + offset, want, std::uint8_t{0});
            ++buf.pages_missing;
        }
    }
    if (buf.pages_missing == pages) return std::nullopt;
    return buf;
}

} // namespace rr::win
