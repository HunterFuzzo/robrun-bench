#pragma once
// Pure classifier. Input is a module-name list and a marker-string list; output is
// an engine guess plus the entry-point recipe for it. Module names are evidence
// about which code paths exist, nothing else.
#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace rr::sig {

enum class Engine {
    Unknown,
    UnityIl2Cpp,
    UnityMono,
    Unreal,
    Source2,
    Source1,
    Godot,
    GameMaker,
    Roblox,
    SdlGeneric
};

struct Finding {
    Engine engine = Engine::Unknown;
    std::string label = "unknown";
    std::string basis;        // what matched, verbatim
    std::string recipe;       // where to start digging on this build
};

struct Evidence {
    std::vector<std::string> modules; // lowercase file names, e.g. "unityplayer.dll"
    std::vector<std::string> markers; // marker strings found in the main image
};

inline std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

inline bool has_module(const Evidence& ev, std::string_view needle) {
    for (const auto& m : ev.modules) {
        if (m.find(needle) != std::string::npos) return true;
    }
    return false;
}

inline bool has_marker(const Evidence& ev, std::string_view needle) {
    for (const auto& m : ev.markers) {
        if (m == needle) return true;
    }
    return false;
}

inline const char* engine_name(Engine e) {
    switch (e) {
        case Engine::Unknown:     return "unknown";
        case Engine::UnityIl2Cpp: return "unity-il2cpp";
        case Engine::UnityMono:   return "unity-mono";
        case Engine::Unreal:      return "unreal";
        case Engine::Source2:     return "source2";
        case Engine::Source1:     return "source1-goldsrc";
        case Engine::Godot:       return "godot";
        case Engine::GameMaker:   return "gamemaker";
        case Engine::Roblox:      return "roblox";
        case Engine::SdlGeneric:  return "sdl-generic";
    }
    return "unknown";
}

inline const char* recipe_for(Engine e) {
    switch (e) {
        case Engine::UnityIl2Cpp:
            return "Il2CppDumper or Cpp2IL over GameAssembly.dll + global-metadata.dat. "
                   "Check metadata version and whether the metadata is encrypted before "
                   "trusting any dumped offset. Static fields hold the singletons; camera "
                   "via Camera.main, worldToCameraMatrix and projectionMatrix.";
        case Engine::UnityMono:
            return "dnSpy on Assembly-CSharp.dll for logic and names. Internally the "
                   "mono_* exports walk domain, assembly, class, field. Externally resolve "
                   "statics through the class vtable, not a hardcoded address.";
        case Engine::Unreal:
            return "Engine version first -- FName layout and UE5 double vectors move with "
                   "it. GNames/FNamePool and GObjects by signature or Dumper-7. Then "
                   "GWorld -> OwningGameInstance -> LocalPlayers[0] -> PlayerController -> "
                   "AcknowledgedPawn; actors via PersistentLevel -> Actors; position via "
                   "RootComponent; camera via PlayerCameraManager -> CameraCachePrivate.POV.";
        case Engine::Source2:
            return "CreateInterface for the interface pointers. Field offsets through the "
                   "schema system, never hardcoded. Entity system for the entity list, view "
                   "matrix out of the client module.";
        case Engine::Source1:
            return "Flat entity array at a fixed stride, engine.dll view matrix, netvars "
                   "through RecvTable.";
        case Engine::Godot:
            return "Mono or GDScript build decides the path. Look for the ClassDB name "
                   "tables and the SceneTree singleton; object properties resolve by name "
                   "through the variant API.";
        case Engine::GameMaker:
            return "Runner build: instance list through the object pool, variables by name "
                   "through the hash table. UndertaleModTool reads data.win for names and "
                   "slot order.";
        case Engine::Roblox:
            return "Luau runtime. DataModel pointer from a known global, then Workspace and "
                   "Players by child name; property offsets move every weekly client build, "
                   "so re-derive after each update rather than pinning them.";
        case Engine::SdlGeneric:
            return "No managed metadata. Strings, the renderer's matrix upload, then the "
                   "input path, then outward from scanner hits. Cheat Engine pointer scan on "
                   "a known HP or ammo value, narrow on change, static base from the path.";
        case Engine::Unknown:
            return "Strings first, then the renderer's matrix uploads, then the input path, "
                   "then outward from scanner hits. Cheat Engine pointer scan on a known HP "
                   "or ammo value, narrow on change, static base from the pointer path.";
    }
    return "";
}

inline Finding classify(const Evidence& ev) {
    Finding f;
    const auto set = [&f](Engine e, std::string basis) {
        f.engine = e;
        f.label = engine_name(e);
        f.basis = std::move(basis);
        f.recipe = recipe_for(e);
    };

    if (has_module(ev, "gameassembly.dll")) {
        set(Engine::UnityIl2Cpp, "module gameassembly.dll");
        return f;
    }
    if (has_module(ev, "mono-2.0-bdwgc.dll") || has_module(ev, "mono.dll") ||
        has_module(ev, "mono-2.0-sgen.dll")) {
        set(Engine::UnityMono, "module mono runtime present");
        return f;
    }
    if (has_module(ev, "unityplayer.dll")) {
        set(Engine::UnityMono, "module unityplayer.dll without gameassembly.dll");
        return f;
    }
    if (has_module(ev, "engine2.dll") || has_module(ev, "schemasystem.dll")) {
        set(Engine::Source2, "module engine2.dll / schemasystem.dll");
        return f;
    }
    if (has_module(ev, "hw.dll") || has_module(ev, "engine.dll")) {
        set(Engine::Source1, "module engine.dll / hw.dll");
        return f;
    }
    if (has_module(ev, "robloxplayerbeta") || has_marker(ev, "RobloxPlayerBeta")) {
        set(Engine::Roblox, "roblox client module");
        return f;
    }
    if (has_marker(ev, "Unreal Engine") || has_marker(ev, "FNamePool") ||
        has_marker(ev, "-Win64-Shipping")) {
        set(Engine::Unreal, "unreal marker string in main image");
        return f;
    }
    if (has_marker(ev, "Godot Engine")) {
        set(Engine::Godot, "godot marker string in main image");
        return f;
    }
    if (has_marker(ev, "GameMaker") || has_marker(ev, "YoYo")) {
        set(Engine::GameMaker, "gamemaker marker string in main image");
        return f;
    }
    if (has_module(ev, "sdl2.dll") || has_module(ev, "sdl3.dll")) {
        set(Engine::SdlGeneric, "module sdl present, no managed runtime");
        return f;
    }
    set(Engine::Unknown, "no engine marker matched");
    return f;
}

inline std::string renderer_from_modules(const Evidence& ev) {
    std::string out;
    const auto note = [&out](const char* name) {
        if (!out.empty()) out += "+";
        out += name;
    };
    if (has_module(ev, "d3d12.dll")) note("d3d12");
    if (has_module(ev, "d3d11.dll")) note("d3d11");
    if (has_module(ev, "d3d9.dll")) note("d3d9");
    if (has_module(ev, "dxgi.dll")) note("dxgi");
    if (has_module(ev, "vulkan-1.dll")) note("vulkan");
    if (has_module(ev, "opengl32.dll")) note("opengl");
    if (out.empty()) out = "none-detected";
    return out;
}

inline std::vector<std::string> anticheat_from_modules(const Evidence& ev) {
    static const char* const kNames[] = {
        "easyanticheat", "beclient",  "battleye",   "vgc.",     "vgk.",
        "xigncode",      "nprotect",  "gameguard",  "punkbuster",
        "faceit",        "vanguard"};
    std::vector<std::string> hits;
    for (const char* name : kNames) {
        if (has_module(ev, name)) hits.emplace_back(name);
    }
    return hits;
}

} // namespace rr::sig
