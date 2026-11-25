#pragma once
// Windows platform layer: process lookup, module identity, exact-or-fail reads.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "scan.hpp"

namespace rr::win {

class Handle {
public:
    Handle() = default;
    explicit Handle(HANDLE h) : h_(h) {}
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& other) noexcept : h_(other.h_) { other.h_ = nullptr; }
    Handle& operator=(Handle&& other) noexcept {
        if (this != &other) {
            close();
            h_ = other.h_;
            other.h_ = nullptr;
        }
        return *this;
    }
    ~Handle() { close(); }

    void close() {
        if (h_ != nullptr && h_ != INVALID_HANDLE_VALUE) CloseHandle(h_);
        h_ = nullptr;
    }
    [[nodiscard]] bool valid() const {
        return h_ != nullptr && h_ != INVALID_HANDLE_VALUE;
    }
    [[nodiscard]] HANDLE get() const { return h_; }

private:
    HANDLE h_ = nullptr;
};

struct ModuleInfo {
    std::string name;   // lowercase file name
    std::string path;   // full path as reported
    std::uint64_t base = 0;
    std::size_t size = 0;
};

// Image copy with a per-page readability mask. An unreadable page stays zeroed
// and is marked absent, so no search can match across a hole.
struct ImageBuffer {
    std::vector<std::uint8_t> bytes;
    std::vector<std::uint8_t> page_present;
    std::size_t page_size = 0x1000;
    std::size_t pages_missing = 0;

    [[nodiscard]] rr::scan::ImageView view() const {
        rr::scan::ImageView v;
        v.bytes = bytes.data();
        v.size = bytes.size();
        v.page_present = page_present.data();
        v.page_size = page_size;
        return v;
    }
};

std::string narrow(const std::wstring& wide);
std::string to_lower_copy(std::string s);

// Lowercase exe name, e.g. "robrun.exe". Returns the first match.
std::optional<DWORD> find_process(const std::string& exe_name_lower);
std::vector<std::string> list_processes();

class Process {
public:
    Process() = default;

    // Read-only access mask. Nothing here writes to the target.
    bool open(DWORD pid);
    [[nodiscard]] bool valid() const { return handle_.valid(); }
    [[nodiscard]] DWORD pid() const { return pid_; }
    [[nodiscard]] bool is_wow64() const { return wow64_; }
    [[nodiscard]] bool alive() const;
    [[nodiscard]] std::string image_path() const;

    [[nodiscard]] std::vector<ModuleInfo> modules() const;
    [[nodiscard]] std::optional<ModuleInfo> find_module(const std::string& name_lower) const;

    // Exact read or failure. A partial read returns false and leaves dst untouched.
    bool read_raw(std::uint64_t address, void* dst, std::size_t length) const;

    template <typename T>
    std::optional<T> read(std::uint64_t address) const {
        T value{};
        if (!read_raw(address, &value, sizeof(T))) return std::nullopt;
        return value;
    }

    // Page-by-page copy of a module image. Fails only if the module is absurd.
    [[nodiscard]] std::optional<ImageBuffer> read_image(const ModuleInfo& mod) const;

private:
    Handle handle_;
    DWORD pid_ = 0;
    bool wow64_ = false;
};

} // namespace rr::win
