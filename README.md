# Mario is Missing! — Static Recompilation

> *"Have you seen Mario? He was right here a minute ago..."*

Static recompilation of **Mario is Missing!** (SNES, 1993) from WDC 65C816 assembly to native C code, playable on modern hardware via SDL2.

This is Luigi's time to shine — again. Nintendo may have forgotten about this game, but we haven't.

Part of the [sp00nznet](https://github.com/sp00nznet) SNES recompilation portfolio, powered by [snesrecomp](https://github.com/sp00nznet/snesrecomp).

## Why This Game?

Mario is Missing is one of those beautifully weird SNES titles that time forgot. It's an **educational geography game** starring Luigi — possibly his first solo starring role — where Bowser steals famous landmarks and Luigi has to travel the world answering trivia to get them back.

What makes it interesting from a recomp perspective:
- **Data-driven design** — the game is mostly question/answer databases and city data, making the code relatively straightforward to follow
- **Surprisingly high-res sprites** — Luigi's walking animations and the city backgrounds are detailed for the era
- **Simple game loop** — walk around, talk to NPCs, answer questions, return artifacts. No Mode 7 racing, no Super FX polygons, just clean 65816 code
- **Abandoned by Nintendo** — this game hasn't seen a re-release, virtual console port, or Switch Online appearance. It's a ghost in Nintendo's catalog. Perfect candidate for preservation through recompilation.

The real goal here is to **drive snesrecomp development** — every new game target pushes the framework forward and helps uncover edge cases in the hardware backend.

## Status

**Project Phase: Scaffolding**

We're just getting started. The project structure is in place, snesrecomp is linked, and we're ready to start disassembling and recompiling functions from the ROM.

### Progress Tracker

| Milestone | Status | Functions | Notes |
|-----------|--------|-----------|-------|
| Project setup | DONE | — | CMake, snesrecomp submodule, cpu_ops.h |
| ROM analysis | TODO | — | Map ROM header, find reset vector, identify banks |
| Boot chain | TODO | 0 | Reset vector, hardware init, stack setup |
| NMI handler | TODO | 0 | VBlank interrupt, DMA transfers, input reading |
| Main loop | TODO | 0 | State machine, frame dispatch |
| Title screen | TODO | 0 | Logo, menu, "Press Start" |
| World map | TODO | 0 | City selection, Luigi walking on map |
| City exploration | TODO | 0 | Luigi walking, NPC interaction, sprite rendering |
| Q&A system | TODO | 0 | Question display, answer selection, artifact return |
| Audio engine | TODO | 0 | SPC700 music/SFX upload and playback |
| Full playthrough | TODO | 0 | All cities, all artifacts, ending sequence |

**Total recompiled functions: 0** (we're at the starting line!)

### What's Done
- Project structure matching the snesrecomp ecosystem (CMake, includes, recomp layout)
- Full 65816 CPU instruction helper library (`cpu_ops.h`) — every op you'd need to recompile
- snesrecomp submodule linked — real PPU, real SPC700 audio, real DMA, all ready to go
- Launcher (`main.c`) with the standard init → boot → frame loop → shutdown lifecycle
- Stub boot chain ready for actual ROM disassembly

### What's Next
1. **Disassemble the ROM** — read the reset vector from the LoROM header, trace the boot chain
2. **Recompile the reset vector** — the first real function, setting up native mode and the stack
3. **NMI handler** — get VBlank processing working so we can render frames
4. **Title screen** — first visual output, the moment it stops being stubs

## Architecture

```
+---------------------------------------------------+
|                  mim_launcher                      |
|  +---------------------------------------------+  |
|  |  src/recomp/ — Recompiled functions          |  |
|  |  mim_boot.c  — Reset, NMI, main loop        |  |
|  |  (more files as recompilation progresses)    |  |
|  +---------------------------------------------+  |
|                       |                            |
|              bus_read8 / bus_write8                 |
|                       |                            |
|  +---------------------------------------------+  |
|  |  snesrecomp — SNES hardware backend          |  |
|  |  Real PPU (Mode 0-7, sprites, HDMA)          |  |
|  |  Real SPC700 + DSP (audio)                   |  |
|  |  Real DMA (8 channels)                       |  |
|  |  Cartridge bus (LoROM auto-detect)            |  |
|  +---------------------------------------------+  |
|                       |                            |
|              SDL2 (window + audio + input)          |
+---------------------------------------------------+
```

The game's 65816 CPU code is recompiled to C functions. Everything else — the PPU that renders pixels, the SPC700 that plays music, the DMA that moves data — is real SNES hardware emulation from [LakeSnes](https://github.com/elzo-d/LakeSnes), packaged as a library by snesrecomp.

## Game Info

| | |
|---|---|
| **Title** | Mario is Missing! |
| **Platform** | Super Nintendo (SNES) |
| **Year** | 1993 |
| **Developer** | Radical Entertainment / The Software Toolworks |
| **Publisher** | Mindscape (not Nintendo!) |
| **Genre** | Educational / Adventure |
| **ROM mapping** | LoROM |
| **ROM size** | 512 KB |
| **CPU** | WDC 65C816 @ 3.58 MHz |
| **Special hardware** | None (no DSP, no Super FX — just clean 65816) |

### The Game in a Nutshell

Bowser has stolen famous artifacts from cities around the world. Mario is... well, missing. Luigi has to travel to different cities, walk around talking to people, answer geography questions to prove which artifact belongs where, and return them to their rightful locations. Do this for enough cities and you rescue Mario.

It's genuinely charming in a 90s-educational-game kind of way. The sprite work is solid, the city backgrounds are surprisingly detailed, and the question database covers real geography that holds up today.

## Building

### Prerequisites
- **CMake 3.16+**
- **SDL2** (via vcpkg or system package)
- **C compiler** — MSVC 2022 (Windows) or GCC/Clang (Linux/macOS)

### Windows (MSVC + vcpkg)
```bash
git clone --recursive https://github.com/sp00nznet/marioismissing.git
cd marioismissing
cmake -B build -G "Visual Studio 17 2022" -A x64 \
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Debug
```

### Linux / macOS
```bash
git clone --recursive https://github.com/sp00nznet/marioismissing.git
cd marioismissing
cmake -B build
cmake --build build
```

### Running
```bash
# Place your legally obtained ROM in the project directory
./build/mim_launcher "Mario is Missing! (U).sfc"
```

You need to supply your own ROM file. This project contains no copyrighted game data.

## Project Structure

```
marioismissing/
+-- CMakeLists.txt          # Build configuration
+-- README.md               # You are here
+-- LICENSE                  # MIT
+-- include/
|   +-- mim/
|       +-- cpu_ops.h       # 65816 instruction helpers (LDA, STA, ADC, etc.)
|       +-- functions.h     # Recompiled function declarations
+-- src/
|   +-- main/
|   |   +-- main.c          # Launcher (init, frame loop, shutdown)
|   +-- recomp/
|       +-- mim_boot.c      # Boot chain (reset, NMI, main loop)
+-- tools/                  # Disassembly and analysis scripts (coming soon)
+-- ext/
    +-- snesrecomp/         # SNES hardware backend (git submodule)
```

## Sister Projects

This game is part of a family of SNES static recompilations, all sharing the same [snesrecomp](https://github.com/sp00nznet/snesrecomp) backend:

| Project | Game | Status |
|---------|------|--------|
| [mk](https://github.com/sp00nznet/mk) | Super Mario Kart | 46 functions, through character select |
| [stuntrace](https://github.com/sp00nznet/stuntrace) | Stunt Race FX | 37 functions, attract mode + title |
| [mariopaint](https://github.com/sp00nznet/mariopaint) | Mario Paint | 9 functions, boot chain |
| **marioismissing** | **Mario is Missing!** | **Just started — you are here** |

Each game project drives improvements in snesrecomp. Super Mario Kart pushed PPU rendering and DMA. Stunt Race FX added Super FX (GSU-2) support. Mario Paint brought SNES Mouse input. Mario is Missing will likely stress-test the **text rendering pipeline** and **data-driven game logic** paths — every game teaches us something new about the hardware.

## Contributing

This is an active development project. If you're interested in:
- **65816 disassembly** — tracing ROM code and figuring out what it does
- **Static recompilation** — converting assembly to C using the cpu_ops helpers
- **SNES hardware** — PPU modes, DMA patterns, SPC700 audio
- **Educational game preservation** — keeping weird 90s games alive

...then jump in! The codebase is intentionally straightforward. Each recompiled function is a direct translation of the original 65816 assembly using human-readable helper functions.

## Legal

This project contains **no copyrighted game data**. You must supply your own legally obtained ROM to run the recompiled executable. The recompilation code itself is original work licensed under MIT.

This is a preservation and education project. Mario is Missing! has not been re-released by Nintendo on any modern platform.
