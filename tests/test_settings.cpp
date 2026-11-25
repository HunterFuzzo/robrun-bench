// Round-trips the settings file and checks that a broken one changes nothing.
#include <cstdio>
#include <cstdlib>
#include <string>

#include "settings.hpp"

namespace {

int g_failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::printf("FAIL %s\n", what);
        ++g_failures;
    }
}

std::string temp_path(const char* name) {
    const char* const vars[] = {"TMPDIR", "TEMP", "TMP"};
    for (const char* var : vars) {
        const char* value = std::getenv(var);
        if (value != nullptr && value[0] != '\0') {
            std::string path = value;
            if (path.back() != '/' && path.back() != '\\') path.push_back('/');
            return path + name;
        }
    }
    return std::string("./") + name;
}

bool write_text(const std::string& path, const std::string& text) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) return false;
    const std::size_t written = std::fwrite(text.data(), 1, text.size(), f);
    std::fclose(f);
    return written == text.size();
}

void test_json_shape() {
    const rr::cfg::Settings s;
    const std::string json = rr::cfg::to_json(s);
    check(json.front() == '{', "json opens with a brace");
    check(json.find("\"esp_box\": true") != std::string::npos, "bool written as true");
    check(json.find("\"aim_enabled\": false") != std::string::npos, "bool written as false");
    check(json.find("\"aim_fov_px\": 120") != std::string::npos, "int written");
    check(json.find("\"aim_smoothing\": 6.0000") != std::string::npos, "float written");
    check(json.find(",\n}") == std::string::npos, "no trailing comma before the close");
}

void test_round_trip() {
    const std::string path = temp_path("rr_settings_round_trip.json");
    rr::cfg::Settings written;
    written.esp_box = false;
    written.esp_name = true;
    written.aim_enabled = true;
    written.aim_fov_px = 333;
    written.aim_key = 0x05;
    written.aim_smoothing = 12.5f;
    written.player_height = 180.25f;
    written.frame_cap = 240;
    check(rr::cfg::save(written, path), "save succeeds");

    rr::cfg::Settings read;
    check(rr::cfg::load(read, path), "load succeeds");
    check(read.esp_box == false, "false survives the trip");
    check(read.esp_name == true, "true survives the trip");
    check(read.aim_enabled == true, "aim flag survives");
    check(read.aim_fov_px == 333, "int survives");
    check(read.aim_key == 0x05, "keybind survives");
    check(read.aim_smoothing > 12.49f && read.aim_smoothing < 12.51f, "float survives");
    check(read.player_height > 180.2f && read.player_height < 180.3f, "height survives");
    check(read.frame_cap == 240, "frame cap survives");
    std::remove(path.c_str());
}

void test_bad_input() {
    rr::cfg::Settings s;
    const int original_fov = s.aim_fov_px;
    check(!rr::cfg::load(s, temp_path("rr_settings_does_not_exist.json")),
          "missing file reports failure");
    check(s.aim_fov_px == original_fov, "missing file changes nothing");

    const std::string junk_path = temp_path("rr_settings_junk.json");
    check(write_text(junk_path, "this is not json at all\n"), "junk file written");
    check(!rr::cfg::load(s, junk_path), "junk file reports failure");
    check(s.aim_fov_px == original_fov, "junk file changes nothing");
    std::remove(junk_path.c_str());

    const std::string partial_path = temp_path("rr_settings_partial.json");
    check(write_text(partial_path,
                     "{\n  \"unknown_key\": 7,\n  \"aim_fov_px\": 42,\n  \"esp_box\": "
                     "false\n}\n"),
          "partial file written");
    check(rr::cfg::load(s, partial_path), "partial file reports success");
    check(s.aim_fov_px == 42, "known key applied");
    check(s.esp_box == false, "known bool applied");
    std::remove(partial_path.c_str());
}

} // namespace

int main() {
    test_json_shape();
    test_round_trip();
    test_bad_input();
    if (g_failures == 0) {
        std::printf("all settings checks passed\n");
        return 0;
    }
    std::printf("%d check(s) failed\n", g_failures);
    return 1;
}
