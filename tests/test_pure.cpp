// Checks the game-independent halves: pattern parsing, masked search, classifier.
// Builds and runs anywhere, no Windows headers, no game.
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "engine_sig.hpp"
#include "scan.hpp"

namespace {

int g_failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::printf("FAIL %s\n", what);
        ++g_failures;
    }
}

void test_pattern_parse() {
    const auto pat = rr::scan::parse_pattern("48 8B 05 ?? ?? ?? ?? 48 85 C0");
    check(pat.size() == 10, "pattern length");
    check(pat[0].value == 0x48 && !pat[0].wildcard, "first byte");
    check(pat[3].wildcard, "wildcard marked");
    check(pat[9].value == 0xC0, "last byte");
    check(rr::scan::parse_pattern("4").empty(), "odd nibble rejected");
    check(rr::scan::parse_pattern("ZZ").empty(), "non-hex rejected");
    check(rr::scan::parse_pattern("4g 00").empty(), "bad second nibble rejected");
    const auto single = rr::scan::parse_pattern("48 ? C0");
    check(single.size() == 3 && single[1].wildcard, "single question mark hole");
}

void test_pattern_search() {
    const std::uint8_t data[] = {0x00, 0x48, 0x8B, 0x05, 0xDE, 0xAD, 0xBE,
                                 0xEF, 0x48, 0x85, 0xC0, 0x90};
    rr::scan::ImageView img;
    img.bytes = data;
    img.size = sizeof(data);
    img.page_size = 0x1000;

    const auto pat = rr::scan::parse_pattern("48 8B 05 ?? ?? ?? ?? 48 85 C0");
    const auto hit = rr::scan::find_pattern(img, pat);
    check(hit.has_value() && *hit == 1, "pattern found at offset 1");

    const auto miss = rr::scan::find_pattern(img, rr::scan::parse_pattern("11 22 33"));
    check(!miss.has_value(), "absent pattern misses");

    const auto all = rr::scan::find_pattern_all(img, rr::scan::parse_pattern("48"), 8);
    check(all.size() == 2, "two single-byte hits");
}

void test_presence_mask() {
    std::vector<std::uint8_t> data(0x2000, 0x00);
    data[0x0FFE] = 0x48;
    data[0x0FFF] = 0x8B;
    data[0x1000] = 0x05;

    std::uint8_t present[2] = {1, 0}; // second page unreadable
    rr::scan::ImageView img;
    img.bytes = data.data();
    img.size = data.size();
    img.page_present = present;
    img.page_size = 0x1000;

    const auto pat = rr::scan::parse_pattern("48 8B 05");
    check(!rr::scan::find_pattern(img, pat).has_value(), "no match across a missing page");
    check(!rr::scan::range_present(img, 0x0FFE, 3), "range spanning a hole is absent");
    check(rr::scan::range_present(img, 0x0000, 0x1000), "readable page reported present");

    present[1] = 1;
    const auto hit = rr::scan::find_pattern(img, pat);
    check(hit.has_value() && *hit == 0x0FFE, "match once the page is readable");
}

void test_string_search() {
    const char ascii[] = "xxUnityPlayerxx";
    rr::scan::ImageView a;
    a.bytes = reinterpret_cast<const std::uint8_t*>(ascii);
    a.size = sizeof(ascii) - 1;
    check(rr::scan::find_ascii(a, "UnityPlayer", 4).size() == 1, "ascii needle found");
    check(rr::scan::contains_marker(a, "UnityPlayer"), "marker via ascii");
    check(!rr::scan::contains_marker(a, "GameMaker"), "absent marker stays absent");

    const std::uint8_t wide[] = {'G', 0, 'o', 0, 'd', 0, 'o', 0, 't', 0};
    rr::scan::ImageView w;
    w.bytes = wide;
    w.size = sizeof(wide);
    check(rr::scan::find_utf16(w, "Godot", 4).size() == 1, "utf16 needle found");
    check(rr::scan::contains_marker(w, "Godot"), "marker via utf16");
}

void test_classifier() {
    rr::sig::Evidence il2cpp;
    il2cpp.modules = {"game.exe", "gameassembly.dll", "unityplayer.dll", "d3d11.dll"};
    const auto a = rr::sig::classify(il2cpp);
    check(a.engine == rr::sig::Engine::UnityIl2Cpp, "il2cpp wins over unityplayer");
    check(!a.recipe.empty(), "il2cpp recipe present");
    check(rr::sig::renderer_from_modules(il2cpp) == "d3d11", "renderer d3d11");

    rr::sig::Evidence mono;
    mono.modules = {"game.exe", "unityplayer.dll", "mono-2.0-bdwgc.dll"};
    check(rr::sig::classify(mono).engine == rr::sig::Engine::UnityMono, "mono runtime");

    rr::sig::Evidence unreal;
    unreal.modules = {"game-win64-shipping.exe", "dxgi.dll", "d3d12.dll"};
    unreal.markers = {"Unreal Engine"};
    const auto u = rr::sig::classify(unreal);
    check(u.engine == rr::sig::Engine::Unreal, "unreal from marker");
    check(rr::sig::renderer_from_modules(unreal) == "d3d12+dxgi", "renderer d3d12+dxgi");

    rr::sig::Evidence src2;
    src2.modules = {"game.exe", "engine2.dll", "schemasystem.dll"};
    check(rr::sig::classify(src2).engine == rr::sig::Engine::Source2, "source2");

    rr::sig::Evidence plain;
    plain.modules = {"robrun.exe", "kernel32.dll"};
    const auto p = rr::sig::classify(plain);
    check(p.engine == rr::sig::Engine::Unknown, "no marker means unknown");
    check(p.basis == "no engine marker matched", "unknown basis stated");
    check(!p.recipe.empty(), "unknown still ships a recipe");
    check(rr::sig::renderer_from_modules(plain) == "none-detected", "no renderer detected");

    rr::sig::Evidence guarded;
    guarded.modules = {"game.exe", "easyanticheat_x64.dll", "unityplayer.dll"};
    const auto ac = rr::sig::anticheat_from_modules(guarded);
    check(ac.size() == 1 && ac[0] == "easyanticheat", "anticheat module reported");
    check(rr::sig::anticheat_from_modules(plain).empty(), "no false anticheat hit");
}

} // namespace

int main() {
    test_pattern_parse();
    test_pattern_search();
    test_presence_mask();
    test_string_search();
    test_classifier();
    if (g_failures == 0) {
        std::printf("all pure checks passed\n");
        return 0;
    }
    std::printf("%d check(s) failed\n", g_failures);
    return 1;
}
