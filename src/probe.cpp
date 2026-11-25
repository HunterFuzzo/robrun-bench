#include "probe.hpp"

#include <cstdio>
#include <string_view>

#include "config.hpp"
#include "engine_sig.hpp"
#include "process.hpp"
#include "scan.hpp"

namespace rr::probe {
namespace {

// Marker strings looked for in the main image. Each one is a label a developer
// typed into the binary; it selects which recipe to print, nothing else.
constexpr std::string_view kMarkers[] = {
    "Unreal Engine", "FNamePool",    "-Win64-Shipping", "Godot Engine",
    "GameMaker",     "YoYo",         "il2cpp",          "mono_jit_init",
    "UnityPlayer",   "RobloxPlayerBeta"};

std::string json_escape(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 8);
    for (const unsigned char c : in) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char tmp[7]{};
                    std::snprintf(tmp, sizeof(tmp), "\\u%04x", c);
                    out += tmp;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

std::string hex64(std::uint64_t v) {
    char tmp[32]{};
    std::snprintf(tmp, sizeof(tmp), "0x%llX", static_cast<unsigned long long>(v));
    return tmp;
}

bool parse_pe(const rr::scan::ImageView& img, Report& r) {
    if (!rr::scan::range_present(img, 0, sizeof(IMAGE_DOS_HEADER))) return false;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(img.bytes);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    if (dos->e_lfanew <= 0) return false;

    const std::size_t nt_off = static_cast<std::size_t>(dos->e_lfanew);
    if (!rr::scan::range_present(img, nt_off, sizeof(IMAGE_NT_HEADERS64))) return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(img.bytes + nt_off);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

    r.pe_machine = nt->FileHeader.Machine;
    r.pe_timestamp = nt->FileHeader.TimeDateStamp;

    if (nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        r.pe_size_of_image = nt->OptionalHeader.SizeOfImage;
        r.pe_checksum = nt->OptionalHeader.CheckSum;
    } else if (nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        const auto* nt32 = reinterpret_cast<const IMAGE_NT_HEADERS32*>(img.bytes + nt_off);
        r.pe_size_of_image = nt32->OptionalHeader.SizeOfImage;
        r.pe_checksum = nt32->OptionalHeader.CheckSum;
    } else {
        return false;
    }

    const std::size_t sec_off = nt_off + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) +
                                nt->FileHeader.SizeOfOptionalHeader;
    const std::size_t count = nt->FileHeader.NumberOfSections;
    if (count == 0 || count > 96) return true;
    if (!rr::scan::range_present(img, sec_off, count * sizeof(IMAGE_SECTION_HEADER))) {
        return true;
    }
    const auto* sec = reinterpret_cast<const IMAGE_SECTION_HEADER*>(img.bytes + sec_off);
    for (std::size_t i = 0; i < count; ++i) {
        SectionRow row;
        char name[9]{};
        for (int k = 0; k < 8; ++k) name[k] = static_cast<char>(sec[i].Name[k]);
        row.name = name;
        row.virtual_address = sec[i].VirtualAddress;
        row.virtual_size = sec[i].Misc.VirtualSize;
        row.characteristics = sec[i].Characteristics;
        r.sections.push_back(std::move(row));
    }
    return true;
}

void scan_requested(const rr::scan::ImageView& img, const Options& opt, Report& r) {
    for (const auto& text : opt.patterns) {
        const auto pat = rr::scan::parse_pattern(text);
        HitRow row;
        row.needle = text;
        row.kind = "pattern";
        if (pat.empty()) {
            r.notes.push_back("malformed pattern ignored: " + text);
            continue;
        }
        const auto hits = rr::scan::find_pattern_all(img, pat, config::kMaxHits);
        for (const auto off : hits) row.rvas.push_back(static_cast<std::uint64_t>(off));
        row.truncated = hits.size() >= config::kMaxHits;
        r.hits.push_back(std::move(row));
    }
    for (const auto& text : opt.strings) {
        HitRow ascii_row;
        ascii_row.needle = text;
        ascii_row.kind = "ascii";
        const auto a = rr::scan::find_ascii(img, text, config::kMaxHits);
        for (const auto off : a) ascii_row.rvas.push_back(static_cast<std::uint64_t>(off));
        ascii_row.truncated = a.size() >= config::kMaxHits;
        r.hits.push_back(std::move(ascii_row));

        HitRow wide_row;
        wide_row.needle = text;
        wide_row.kind = "utf16";
        const auto w = rr::scan::find_utf16(img, text, config::kMaxHits);
        for (const auto off : w) wide_row.rvas.push_back(static_cast<std::uint64_t>(off));
        wide_row.truncated = w.size() >= config::kMaxHits;
        r.hits.push_back(std::move(wide_row));
    }
}

} // namespace

Report run(const Options& opt) {
    Report r;

    std::uint32_t pid = opt.pid;
    if (pid == 0) {
        if (opt.process_name.empty()) {
            r.notes.push_back("no --process and no --pid given");
            return r;
        }
        const auto found = rr::win::find_process(opt.process_name);
        if (!found.has_value()) {
            r.notes.push_back("process not found: " + opt.process_name);
            return r;
        }
        pid = static_cast<std::uint32_t>(*found);
    }

    rr::win::Process proc;
    if (!proc.open(static_cast<DWORD>(pid))) {
        r.notes.push_back("OpenProcess failed for pid " + std::to_string(pid) +
                          " (GetLastError " + std::to_string(GetLastError()) +
                          ") -- run this elevated");
        return r;
    }

    r.attached = true;
    r.pid = pid;
    r.wow64 = proc.is_wow64();
    r.image_path = proc.image_path();
    if (r.wow64) {
        r.notes.push_back("target is a 32-bit process under WOW64; build this probe x86 "
                          "to read it, offsets from an x64 dump do not apply");
    }

    const auto mods = proc.modules();
    if (mods.empty()) {
        r.notes.push_back("module snapshot empty -- architecture mismatch or the process "
                          "exited mid-snapshot");
        return r;
    }
    for (const auto& m : mods) r.modules.push_back(m.name);

    const rr::win::ModuleInfo& main_mod = mods.front();
    r.main_module = main_mod.name;
    r.main_base = main_mod.base;
    r.main_size = main_mod.size;
    if (config::kHasExpectedImageSize && main_mod.size != config::kExpectedImageSize) {
        r.notes.push_back("image size " + std::to_string(main_mod.size) +
                          " does not match the pinned build -- treat every stored offset "
                          "as stale");
    }

    rr::sig::Evidence ev;
    ev.modules = r.modules;

    const auto image = proc.read_image(main_mod);
    if (!image.has_value()) {
        r.notes.push_back("main image unreadable; engine call rests on the module list only");
    } else {
        r.pages_missing = image->pages_missing;
        const auto view = image->view();
        r.pe_parsed = parse_pe(view, r);
        if (!r.pe_parsed) {
            r.notes.push_back("PE headers did not parse from the mapped image");
        }
        for (const auto marker : kMarkers) {
            if (rr::scan::contains_marker(view, marker)) ev.markers.emplace_back(marker);
        }
        scan_requested(view, opt, r);
    }

    const auto finding = rr::sig::classify(ev);
    r.engine = finding.label;
    r.engine_basis = finding.basis;
    r.engine_recipe = finding.recipe;
    r.renderer = rr::sig::renderer_from_modules(ev);
    r.anticheat = rr::sig::anticheat_from_modules(ev);
    if (!r.anticheat.empty()) {
        // Anti-cheat module present: this probe stays read-only external. Injecting into
        // the same process is an account risk.
        r.notes.push_back("anti-cheat module present; this tool opened the process with "
                          "PROCESS_VM_READ only");
    }
    if (!proc.alive()) {
        r.notes.push_back("process exited during the probe; the report is partial");
    }
    return r;
}

std::string to_json(const Report& r) {
    std::string out = "{\n";
    const auto kv_str = [&out](const char* key, const std::string& value, bool comma) {
        out += "  \"";
        out += key;
        out += "\": \"";
        out += json_escape(value);
        out += comma ? "\",\n" : "\"\n";
    };
    const auto kv_raw = [&out](const char* key, const std::string& value, bool comma) {
        out += "  \"";
        out += key;
        out += "\": ";
        out += value;
        out += comma ? ",\n" : "\n";
    };
    const auto list_str = [&out](const char* key, const std::vector<std::string>& items,
                                 bool comma) {
        out += "  \"";
        out += key;
        out += "\": [";
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (i != 0) out += ", ";
            out += "\"";
            out += json_escape(items[i]);
            out += "\"";
        }
        out += comma ? "],\n" : "]\n";
    };

    kv_raw("attached", r.attached ? "true" : "false", true);
    kv_raw("pid", std::to_string(r.pid), true);
    kv_raw("wow64", r.wow64 ? "true" : "false", true);
    kv_str("image_path", r.image_path, true);
    kv_str("main_module", r.main_module, true);
    kv_str("main_base", hex64(r.main_base), true);
    kv_str("main_size", hex64(r.main_size), true);
    kv_raw("pe_parsed", r.pe_parsed ? "true" : "false", true);
    kv_str("pe_size_of_image", hex64(r.pe_size_of_image), true);
    kv_str("pe_timestamp", hex64(r.pe_timestamp), true);
    kv_str("pe_checksum", hex64(r.pe_checksum), true);
    kv_str("pe_machine", hex64(r.pe_machine), true);
    kv_raw("pages_missing", std::to_string(r.pages_missing), true);
    kv_str("engine", r.engine, true);
    kv_str("engine_basis", r.engine_basis, true);
    kv_str("engine_recipe", r.engine_recipe, true);
    kv_str("renderer", r.renderer, true);
    list_str("anticheat", r.anticheat, true);
    list_str("modules", r.modules, true);

    out += "  \"sections\": [\n";
    for (std::size_t i = 0; i < r.sections.size(); ++i) {
        const auto& s = r.sections[i];
        out += "    {\"name\": \"";
        out += json_escape(s.name);
        out += "\", \"rva\": \"";
        out += hex64(s.virtual_address);
        out += "\", \"vsize\": \"";
        out += hex64(s.virtual_size);
        out += "\", \"flags\": \"";
        out += hex64(s.characteristics);
        out += "\"}";
        out += (i + 1 < r.sections.size()) ? ",\n" : "\n";
    }
    out += "  ],\n";

    out += "  \"hits\": [\n";
    for (std::size_t i = 0; i < r.hits.size(); ++i) {
        const auto& h = r.hits[i];
        out += "    {\"needle\": \"";
        out += json_escape(h.needle);
        out += "\", \"kind\": \"";
        out += json_escape(h.kind);
        out += "\", \"truncated\": ";
        out += h.truncated ? "true" : "false";
        out += ", \"rvas\": [";
        for (std::size_t k = 0; k < h.rvas.size(); ++k) {
            if (k != 0) out += ", ";
            out += "\"";
            out += hex64(h.rvas[k]);
            out += "\"";
        }
        out += "]}";
        out += (i + 1 < r.hits.size()) ? ",\n" : "\n";
    }
    out += "  ],\n";

    list_str("notes", r.notes, false);
    out += "}\n";
    return out;
}

void print_summary(const Report& r) {
    if (!r.attached) {
        std::printf("not attached\n");
        for (const auto& n : r.notes) std::printf("  note: %s\n", n.c_str());
        return;
    }
    std::printf("pid        %u%s\n", r.pid, r.wow64 ? " (wow64, 32-bit)" : " (x64)");
    std::printf("image      %s\n", r.image_path.c_str());
    std::printf("main       %s base %s size %s\n", r.main_module.c_str(),
                hex64(r.main_base).c_str(), hex64(r.main_size).c_str());
    if (r.pe_parsed) {
        std::printf("pe         machine %s timestamp %s checksum %s sizeofimage %s\n",
                    hex64(r.pe_machine).c_str(), hex64(r.pe_timestamp).c_str(),
                    hex64(r.pe_checksum).c_str(), hex64(r.pe_size_of_image).c_str());
    }
    std::printf("engine     %s  [%s]\n", r.engine.c_str(), r.engine_basis.c_str());
    std::printf("renderer   %s\n", r.renderer.c_str());
    std::printf("modules    %zu, unreadable pages %zu\n", r.modules.size(), r.pages_missing);
    if (!r.anticheat.empty()) {
        std::printf("anticheat  ");
        for (std::size_t i = 0; i < r.anticheat.size(); ++i) {
            std::printf("%s%s", i ? ", " : "", r.anticheat[i].c_str());
        }
        std::printf("\n");
    }
    for (const auto& h : r.hits) {
        std::printf("hit        %-6s %-28s %zu%s\n", h.kind.c_str(), h.needle.c_str(),
                    h.rvas.size(), h.truncated ? " (truncated)" : "");
        for (const auto rva : h.rvas) {
            std::printf("             rva %s\n", hex64(rva).c_str());
        }
    }
    std::printf("recipe     %s\n", r.engine_recipe.c_str());
    for (const auto& n : r.notes) std::printf("note       %s\n", n.c_str());
}

} // namespace rr::probe
