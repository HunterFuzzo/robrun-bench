#pragma once
// Derivation harness: attach read-only, identify the build, classify the engine,
// scan for markers and for whatever patterns were asked for, write the report.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rr::probe {

struct Options {
    std::string process_name;             // lowercase exe name; empty when pid is given
    std::uint32_t pid = 0;                // 0 means resolve from process_name
    std::string out_path = "robrun_probe.json";
    std::vector<std::string> patterns;    // IDA-style, scanned in the main module
    std::vector<std::string> strings;     // ASCII/UTF-16LE needles, scanned in the main module
    bool list_only = false;               // print the process list and exit
};

struct SectionRow {
    std::string name;
    std::uint64_t virtual_address = 0;
    std::uint64_t virtual_size = 0;
    std::uint32_t characteristics = 0;
};

struct HitRow {
    std::string needle;
    std::string kind;                     // "pattern", "ascii", "utf16"
    std::vector<std::uint64_t> rvas;
    bool truncated = false;
};

struct Report {
    bool attached = false;
    std::uint32_t pid = 0;
    bool wow64 = false;
    std::string image_path;
    std::string main_module;
    std::uint64_t main_base = 0;
    std::uint64_t main_size = 0;
    std::uint64_t pe_size_of_image = 0;
    std::uint32_t pe_timestamp = 0;
    std::uint32_t pe_checksum = 0;
    std::uint16_t pe_machine = 0;
    bool pe_parsed = false;
    std::size_t pages_missing = 0;
    std::string engine;
    std::string engine_basis;
    std::string engine_recipe;
    std::string renderer;
    std::vector<std::string> anticheat;
    std::vector<std::string> modules;
    std::vector<SectionRow> sections;
    std::vector<HitRow> hits;
    std::vector<std::string> notes;
};

// Returns the report; attached stays false when the target could not be opened.
Report run(const Options& opt);

std::string to_json(const Report& r);
void print_summary(const Report& r);

} // namespace rr::probe
