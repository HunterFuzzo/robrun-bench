# robrun bench

External, read-only tooling for a single game build: a derivation harness that
identifies the target and a layered-window overlay with ESP and aim.

Nothing in this tree writes to the target process. The game handle is opened with
`PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ`, and the only output path is
a relative `SendInput` mouse move.

## State of the offsets

Every build-dependent value lives in `src/offsets.hpp` and **all of them are
unset**. Zero is the unset marker, never a real address, and each `kHas*` gate
derives itself from its value — filling a real number in is a one-line edit and
the feature that needs it turns itself on. No invented address is committed here,
because three weeks later an invented one is indistinguishable from a confirmed
one.

With the file as committed, the overlay and the menu run and every world-space
feature reports itself as gated off, with the reason, in the CONFIG tab.

## Layout

| Path | What it is |
|---|---|
| `src/offsets.hpp` | every build-dependent value, with provenance tags and gates |
| `src/config.hpp` | target identity and platform constants |
| `src/math.hpp` | projection and aim math, pure |
| `src/entity.hpp` | per-frame snapshot types, pure |
| `src/target.hpp` | hitbox resolution and target filtering, pure |
| `src/scan.hpp` | pattern parsing and masked image search, pure |
| `src/engine_sig.hpp` | engine classification from modules and markers, pure |
| `src/process.*` | process lookup, module identity, exact-or-fail reads |
| `src/world.*` | builds the snapshot, rejects implausible reads |
| `src/overlay.*` | layered window, GDI back buffer, draw helpers |
| `src/menu.*` | immediate-mode tabbed menu |
| `src/esp.*` | box, skeleton, health, names, snaplines, distance |
| `src/aim.*` | hitbox selection, sticky target, smoothing, triggerbot |
| `src/input.*` | polled keys, `SendInput` output |
| `src/settings.*` | runtime toggles and their JSON persistence |
| `src/probe.*`, `src/main.cpp` | the derivation harness (`rr_probe`) |
| `src/cheat_main.cpp` | the overlay entry point (`rr_overlay`) |
| `tests/` | four suites over the pure halves |

## Build

Windows, x64, C++20, MSVC. No external dependencies: Windows SDK and GDI only.

```
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release
```

The four test suites cover the game-independent logic — pattern parsing, the
engine classifier, both world-to-screen conventions, hitbox resolution, target
filtering and the settings round-trip — and build on any platform with a C++20
compiler.

## Use

```
rr_probe.exe --process <exe>                 identify the build, write the report
rr_probe.exe --process <exe> --string Health --pattern "48 8B 05 ?? ?? ?? ?? 48 85 C0"
rr_overlay.exe --process <exe>               attach and draw
```

`INSERT` toggles the menu, `END` shuts the overlay down, mouse2 is the default aim
key. `aim_pixel_scale`, `player_height` and `player_width` are uncalibrated and
meant to be set on the sliders. Network interpolation and input latency are not
modelled: the point aimed at is where the client last drew the entity.

## Filling the offsets

1. `rr_probe.exe --process <exe>` — PE build identity and the engine, which
   decides whether field offsets come out of a dump or a scanner.
2. A known value in Cheat Engine, then a pointer scan for the static base.
3. `ReClass.NET` on the structure for position, health, team, name.
4. The entity pointer scanned as 8-byte hex; contiguous regular hits are the list,
   and the gap between entries is the stride.
5. The view matrix from the renderer's constant upload, or by brute-forcing
   16-float candidates until the local player projects to screen centre.
6. Bone entries by stride and translation offset inside the entry; indices by
   moving them until the drawing lines up.

Module-relative values are RVAs. A value confirmed on one build is a hypothesis
on the next, which is why `pe_timestamp` and `pe_checksum` are in the probe
report.
