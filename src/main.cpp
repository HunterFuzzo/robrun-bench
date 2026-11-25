#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "config.hpp"
#include "probe.hpp"
#include "process.hpp"

namespace {

void print_usage() {
    std::printf(
        "rr_probe -- read-only derivation harness\n"
        "\n"
        "  rr_probe --process <exe>        attach by executable name\n"
        "  rr_probe --pid <n>              attach by pid\n"
        "  rr_probe --list                 print the process list and exit\n"
        "  rr_probe --out <path>           report path (default robrun_probe.json)\n"
        "  rr_probe --pattern \"48 8B ?? C0\"  scan the main module, repeatable\n"
        "  rr_probe --string <text>        scan ASCII and UTF-16LE, repeatable\n"
        "\n"
        "Nothing here writes to the target: the handle is opened with\n"
        "PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ.\n");
}

} // namespace

int main(int argc, char** argv) {
    rr::probe::Options opt;
    if (rr::config::kHasProcessName) {
        opt.process_name = std::string(rr::config::kProcessName);
    }

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto next = [&](std::string& dst) -> bool {
            if (i + 1 >= argc) {
                std::printf("missing value for %s\n", arg.c_str());
                return false;
            }
            dst = argv[++i];
            return true;
        };

        if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        }
        if (arg == "--list") {
            opt.list_only = true;
            continue;
        }
        if (arg == "--process") {
            std::string value;
            if (!next(value)) return 2;
            opt.process_name = rr::win::to_lower_copy(value);
            continue;
        }
        if (arg == "--pid") {
            std::string value;
            if (!next(value)) return 2;
            opt.pid = static_cast<std::uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
            continue;
        }
        if (arg == "--out") {
            std::string value;
            if (!next(value)) return 2;
            opt.out_path = value;
            continue;
        }
        if (arg == "--pattern") {
            std::string value;
            if (!next(value)) return 2;
            opt.patterns.push_back(value);
            continue;
        }
        if (arg == "--string") {
            std::string value;
            if (!next(value)) return 2;
            opt.strings.push_back(value);
            continue;
        }
        std::printf("unknown argument: %s\n", arg.c_str());
        print_usage();
        return 2;
    }

    if (opt.list_only) {
        for (const auto& name : rr::win::list_processes()) {
            std::printf("%s\n", name.c_str());
        }
        return 0;
    }

    if (opt.process_name.empty() && opt.pid == 0) {
        print_usage();
        return 2;
    }

    const rr::probe::Report report = rr::probe::run(opt);
    rr::probe::print_summary(report);

    const std::string json = rr::probe::to_json(report);
    std::FILE* f = std::fopen(opt.out_path.c_str(), "wb");
    if (f == nullptr) {
        std::printf("could not open %s for writing\n", opt.out_path.c_str());
        return report.attached ? 1 : 3;
    }
    std::fwrite(json.data(), 1, json.size(), f);
    std::fclose(f);
    std::printf("report     %s\n", opt.out_path.c_str());

    return report.attached ? 0 : 3;
}
