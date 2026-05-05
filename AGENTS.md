# AGENTS.md — Wolfenstein 3D SDL3 Port

## Workflow Conventions

### Commit frequency
**Commit early and often.** Each logical fix or feature should be its own commit. Do not batch multiple unrelated changes into a single large commit. When in doubt, commit after every distinct bug fix, refactoring step, or new file addition. This makes bisecting and reverting much easier.

### Before committing
1. Run `zig build` to ensure the project compiles.
2. Run `python test_mcp_gameplay.py` (or `python repro_crash.py`) to verify gameplay still loads.
3. Review `git diff --stat` to confirm the staged changes match what you intended.
4. Write a concise commit message that explains the **why**, not just the **what**.

### Commit and push
Commit early and often — no need to ask for confirmation. Push to remote after each logical commit unless instructed otherwise.

## Build

- `zig build` — first run compiles SDL3 from source via CMake+Ninja (slow). Subsequent runs are instant.
- `zig build run` — build and run.
- **Cache gotcha**: Zig caches C object builds aggressively. If you edit `.c`/`.h` files and the binary doesn't pick up changes, force-clear the cache:
  ```powershell
  Remove-Item -Recurse -Force .zig-cache
  zig build
  ```

## Data Files

Place shareware Wolfenstein 3D v1.4 **WL1** files in `wolf3d-data/`:

```
wolf3d-data/
  AUDIOHED.WL1
  AUDIOT.WL1
  VGADICT.WL1
  VGAHEAD.WL1
  VGAGRAPH.WL1
  MAPHEAD.WL1
  GAMEMAPS.WL1      <-- CARMACIZED map data (not MAPTEMP.WL1)
  VSWAP.WL1
```

The code expects `GAMEMAPS.WL1`, not `MAPTEMP.WL1`.

## Architecture

- **Original source** (read-only reference): `wolf3d-master/WOLFSRC/`
- **Ported source** (active): `src/`
- **Entry point**: `src/sdl3_main.c` → `wolf_main()` → `DemoLoop()` → `GameLoop()`
- **Build config**: `build.zig` — `zig cc` compiles C99 sources with `-DWOLF3D_SDL3`
- **Video**: Emulates VGA Mode Y planar buffer (`screenbuffer[4][50000]`), converted to linear RGBA for SDL3 each frame.

## Critical Porting Hazards

### 1. String literals are read-only
The original DOS code freely writes to string literals (e.g. `US_CPrint` does `*se = '\0'` on the input string). On modern systems this is an **immediate access violation** (`0xC0000005`).

**Rule**: Any function that mutates a `char *` argument must copy it to a local buffer first. Check `wolf3d-master/WOLFSRC/` for the original logic, but never assume the input buffer is writable.

### 2. Carmack / RLEW decompression length semantics
- `CAL_CarmackExpand` takes a **byte** length and must divide by 2 internally (`length /= 2`) — the port originally missed this and crashed on map load.
- `CA_RLEWexpand` takes a **byte** length, so the destination bound should be `dest + length / 2` (words), not `length` words directly.

### 3. Map loading is CARMACIZED
`CA_CacheMap` must perform two-stage decompression: read compressed plane data → `CAL_CarmackExpand` → `CA_RLEWexpand` into `mapsegs[plane]`. The original `CA_Startup` also loads `MAPHEAD.WL1` into `tinf` and reads all map headers into `mapheaderseg[]`.

### 4. `screenloc[]` initialization
Must be `{PAGE1START, PAGE2START, PAGE3START}` (not `{0,0,0}`) or page-flipping logic in `DrawAllPlayBorderSides` and similar functions will write to the wrong addresses.

## Testing / Verification

There is **no automated test suite**. Verify manually:

```powershell
zig build run
```

Then navigate: Main Menu → New Game → Episode 1 → Select Difficulty. If the game does not crash and reaches gameplay (or at least the level-loading screen), the change is good.

A Python repro script exists for automated smoke testing: `python repro_crash.py`.

## Reference Original Source

When fixing cache manager, menu, or game-loop bugs, the original DOS source in `wolf3d-master/WOLFSRC/` is the authoritative reference. Key files:

- `ID_CA.C` — Huffman / Carmack / RLEW decompression, map caching
- `ID_US_1.C` — String drawing, `US_CPrint`
- `WL_DRAW.C` — `screenloc[]`, renderer stubs
- `WL_MAIN.C` — `DemoLoop`, `GameLoop`, `NewGame`
- `WL_MENU.C` — Menu system, `CP_NewGame`
- `WL_GAME.C` — `SetupGameLevel`, level initialization

## Useful Commands

```powershell
# Force rebuild everything
Remove-Item -Recurse -Force .zig-cache; zig build

# Run with crash logging enabled (Windows SEH handler writes crash_log.txt)
python repro_crash.py

# Check binary was actually rebuilt
ls zig-out/bin/wolf3d.exe | Select-Object LastWriteTime
```
