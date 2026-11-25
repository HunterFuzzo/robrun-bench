# First run

Ordered. Each step says what it must print, so a wrong step is caught where it
happens instead of three steps later.

## 1. Toolchain, once

```
winget install Kitware.CMake
winget install Microsoft.VisualStudio.2022.BuildTools --override "--quiet --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```

Exclude the repository folder in Windows Defender before building. Defender
flags overlay binaries on sight, and the error it produces looks like a linker
failure.

## 2. Clone and build

```
gh repo clone HunterFuzzo/robrun-bench
cd robrun-bench
build.bat
```

`build.bat` must end with two paths and `100% tests passed`. If it stops earlier
it names the missing package. The binaries land in `build\Release\`.

## 3. Launch before any offset is filled

With the game running:

```
run.bat robrun.exe
```

What must happen, with every offset still at zero:

- the console prints `attached`, then one `gated` line per feature that is off
- `INSERT` opens the menu over the game
- the CONFIG tab shows the process, pid, module base and image size
- the CONFIG tab lists the gated features in red
- `END` closes it

Nothing draws in the world yet. That is correct: the view matrix is unset, so
there is no projection. If this step works, the overlay, the window tracking,
the input path and the menu are all confirmed, and everything left is offsets.

The overlay cannot draw over exclusive fullscreen. Use windowed or borderless.

## 4. Identify the build

```
probe.bat robrun.exe
```

Read three fields out of `robrun_probe.json`:

- `engine` — decides whether field offsets come from a dump or from a scanner
- `engine_recipe` — the entry chain for that engine
- `pe_timestamp` and `pe_checksum` — the build fingerprint. Write them into the
  commit message when you fill offsets, so a later session knows which build the
  values were confirmed on.

## 5. The three offsets that turn ESP and aim on

ESP and aim share one gate. Both need exactly these three, and nothing else:

| Constant | Kind |
|---|---|
| `kViewMatrix` | RVA, module-relative, 16 floats |
| `kEntityList` | RVA, module-relative |
| `kEntPosition` | byte offset inside the entity structure |

Fill those, run `build.bat`, and you have boxes and a working aim key. Everything
else — health bars, names, team filtering, skeleton, visibility, world distance —
is an upgrade on top, each gated on its own value, each independent.

Module-relative values are RVAs: the address minus the module base, which is the
`Module.dll+1A2B3C0` form Cheat Engine already shows. Zero means unset, so a
feature stays off until its line carries a real number.

## 6. Calibrate, in this order

1. **Projection.** Boxes mirrored or upside down means `kMatrixRowMajor` is the
   wrong reading — flip it. Both readings are implemented; neither is a guess
   about your build.
2. **Up axis.** Boxes lying on their side means `kUpAxisIndex` names the wrong
   component. Jump in game and watch which of the three position floats rises.
3. **Box size.** `player_height` and `player_width` are sliders in the MISC tab,
   both uncalibrated. Set height until the box caps the model, then width.
4. **Aim scale.** Set `smoothing` to 1 so the move is a single snap, then adjust
   `px -> counts` until one press lands on the target: overshooting means the
   scale is too high, stopping short means too low. Raise smoothing afterwards.

## 7. When something is wrong

| What you see | What it means |
|---|---|
| `attach failed` | the process is not running, or it has higher privileges than your console — start the console as administrator |
| `no visible window for that pid` | the game has not opened its window yet, or it is a launcher process rather than the game |
| menu opens, nothing in the world | expected until `kViewMatrix`, `kEntityList` and `kEntPosition` all carry values |
| boxes mirrored or inverted | `kMatrixRowMajor` |
| boxes on their side | `kUpAxisIndex` |
| boxes at the wrong size | `player_height` and `player_width` sliders |
| entities counted but nothing drawn | positions read but the projection is wrong, or every point is behind the camera plane — check the matrix first |
| `dropped` climbing in CONFIG | pointers or positions failing the plausibility check: wrong stride, or the list base is off |
| bones all on one spot | `kBonePositionOffset` points at the start of the entry instead of the translation |
| gaps in the bone index numbers | that slot is outside the array, or `kBoneStride` is wrong |
| overlay invisible but console healthy | exclusive fullscreen — switch the game to borderless |
| a feature that worked stops after a game update | the build changed; compare `pe_timestamp` against the one in your commit message and re-derive |
