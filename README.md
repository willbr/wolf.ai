# Wolfenstein 3D SDL3 Port

A work-in-progress port of the original 1992 Wolfenstein 3D DOS source code to modern systems using SDL3 and the Zig build system.

## Status

This port boots, loads shareware data, and displays the title screen and main menu.

**Implemented:**
- SDL3 window (640x400 with 2x integer scaling)
- 320x200 planar Mode Y screen buffer emulation in software
- SDL3 keyboard input (arrow keys, Enter, Escape)
- Palette-based rendering with fade in/out effects
- Title screen, credits, high scores, main menu
- Font rendering (proportional and color)
- Shareware data loading (VGA, maps, audio headers)
- Huffman and Carmack decompression
- Planar-to-linear framebuffer conversion

**Not Yet Implemented:**
- Gameplay (raycasting renderer, actors, shooting)
- Audio (Adlib sound, digitized effects)
- Save/load games
- Mouse input

## Quick Start

### Prerequisites
- [Zig 0.16.0](https://ziglang.org/download/)
- Shareware Wolfenstein 3D v1.4 WL1 data files

### Build

The first build compiles SDL3 from source (takes a few minutes). Subsequent builds are instant.

```bash
zig build
```

### Run

```bash
zig build run
```

Or run the executable directly:

```bash
# Windows
zig-out\bin\wolf3d.exe

# macOS / Linux
zig-out/bin/wolf3d
```

### Controls

| Key | Action |
|-----|--------|
| &uarr; / &darr; | Navigate menu |
| Enter | Select |
| Escape | Back / Quit |

## Data Files

Place the shareware Wolfenstein 3D v1.4 WL1 files in `wolf3d-data/`:

```
wolf3d-data/
  AUDIOHED.WL1
  AUDIOT.WL1
  VGADICT.WL1
  VGAHEAD.WL1
  VGAGRAPH.WL1
  MAPHEAD.WL1
  MAPTEMP.WL1
  VSWAP.WL1
```

You can get the shareware data from the [Internet Archive](https://archive.org/details/Wolfenstein3d) or [Wolfenstein 3D Dome](http://www.wolfenstein3d.co.uk/).

## Architecture

### Build System
- `build.zig` -- Zig build script
- SDL3 built as a static library via CMake+Ninja from `vendor/SDL3/`
- C sources compiled with `zig cc`

### Source Layout
- `src/compat.h` -- Borland C++ 3.0 to modern C compatibility layer
- `src/sdl3_main.c` -- Entry point, SDL init, Borland register stubs
- `src/id_vl.c` / `src/id_vl.h` -- SDL3 video backend
- `src/id_in.c` -- SDL3 input backend (keyboard, timer)
- `src/id_ca.c` -- Cache manager, Huffman/Carmack decompression
- `src/id_mm.c` -- Memory manager (flat malloc wrappers)
- `src/id_pm.c` -- Page manager (VSWAP file loader)
- `src/id_vh.c` / `src/id_vh.h` -- High-level drawing, font rendering
- `src/wl_main.c` -- Main game loop, demo sequence
- `src/wl_menu.c` -- Menu system
- `src/wl_def.h` -- Core game types and definitions

### Video
The game renders to a 320x200 planar Mode Y buffer (`screenbuffer[4][48000]`),
matching the original VGA hardware layout. Each frame is converted to a linear
32-bit RGB buffer and presented via SDL3 streaming texture.

### Input
SDL3 keyboard events are mapped to DOS scancodes. The global `Keyboard[]` array
is updated on key press/release, matching the original interrupt-driven input model.
`TimeCount` increments at ~70Hz using `SDL_GetTicks()`.

## License

The original Wolfenstein 3D source code was released by id Software under the
GNU GPL v2. See `COPYING` for details.

This port is also released under the GNU GPL v2.

SDL3 is licensed under the zlib license.

## Acknowledgements

- [id Software](https://www.idsoftware.com/) for releasing the original source
- [Wolfenstein 3D Dome](http://www.wolfenstein3d.co.uk/) for shareware data and community
- [SDL](https://libsdl.org/) for the cross-platform media library
- [Zig](https://ziglang.org/) for the build system and toolchain

## References

- Original source: `wolf3d-master/WOLFSRC/` (preserved, unmodified)
- Ported source: `src/` (active development)
- [Wolfenstein 3D Bible](http://www.wolfenstein3d.co.uk/Wolffiles.htm)
- [Fabien Sanglard's Wolfenstein 3D Black Book](https://fabiensanglard.net/gebbwolf3d/)
- [SDL3 Documentation](https://wiki.libsdl.org/SDL3/FrontPage)
