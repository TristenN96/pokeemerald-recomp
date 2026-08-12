# Native PC port architecture audit

Status: repository audit performed 2026-08-09; Phase 1 implementation discoveries
recorded 2026-08-10. The audit remains authoritative for scope: no mod loading or
runtime content externalization is part of Phase 1.

## Executive summary

The native port is a host build of the GBA-shaped game runtime. The game still owns
GBA-style registers, VRAM/OAM/palette buffers, DMA state, flash sectors, script
bytecode, and many tables whose original representation contained 32-bit pointers.
`PORTABLE` replaces the fixed GBA memory addresses with host arrays and lets the
original game code run against those arrays. SDL2 supplies the main host loop, video,
audio, keyboard/controller input on some platforms, save-file access, and settings.

The current Linux baseline is a 32-bit i386 executable. The most important finding is
that this is not only a compiler setting: several runtime paths deliberately store
native pointers in `u32`, split callbacks across 16-bit task fields, or reinterpret a
32-bit GBA register as a host pointer. A 64-bit build therefore needs a pointer/handle
boundary and a relocation or hydration strategy for pointer-bearing generated data.

The largest blockers are:

1. `Makefile_pc` hard-codes `-m32`, GNU assembler `--32`, 32-bit Windows paths,
   32-bit Windows symbol-prefix handling, and a 32-bit MinGW link model.
2. `include/gba/defines.h:54-60` defines portable `VRAM` as `(u32)VRAM_`, which
   truncates a 64-bit host pointer immediately.
3. `src/platform/dma.c:80,99-100` writes host pointers into 32-bit emulated DMA
   registers and later reconstructs a pointer from those registers.
4. Game state stores callbacks and ordinary pointers in 32-bit task/sprite/script
   fields. This affects task follow-ups, field effects, battle animations, object
   events, menus, and several minigames.
5. Generated assembly and map/script tables emit symbol addresses with `.int` or
   `.4byte`; C tables also contain native pointer members. Their on-disk/object ABI
   cannot be assumed to remain valid when the host pointer width changes.
6. The platform API is a thin set of global functions. SDL windowing, timing, input,
   audio, settings, save paths, and rendering are coupled in `src/platform/sdl2.c`,
   while `src/platform/win32.c` is a separate, incomplete GDI implementation.
7. The normal Linux target links directly with GCC and does not use the linker script.
   The separate ELF target references `ld_script_pc.ld`, which is absent from this
   checkout. The GBA linker scripts and generated RAM scripts are not host layouts.
8. Linux desktop SDL controller support is absent; controller event handling is
   currently under the Android branch, and Windows XInput is a separate conditional
   path.
9. Save/config paths are relative to the current working directory on desktop, save
   writes are not flushed/truncated explicitly, and the Windows native implementation
   has invalid-handle/close logic that needs isolation before it becomes a supported
   backend.
10. There is no 64-bit CI target or cross-platform vanilla regression harness to catch
    pointer width, structure layout, timing, audio, or generated-table drift.

The recommended immediate implementation milestone is a vanilla-only **64-bit host
pointer quarantine**: introduce explicit logical GBA address types and host pointer or
callback handles, fix the portable VRAM/DMA paths and task callback storage, add static
layout assertions and a real `linux64` build target, then run the same vanilla smoke
suite on 32-bit and 64-bit Linux. This milestone must not add mod loading.

## Current architecture

### Runtime shape

The game core is still organized as a GBA decompilation:

- `src/main.c` runs the main game state machine and calls `Platform_GetKeyInput`.
- `src/overworld.c`, `src/battle_main.c`, `src/script.c`, `src/scrcmd.c`, and the
  many feature modules operate on GBA-sized globals and tables.
- `include/gba/io_reg.h` exposes register macros over either fixed GBA addresses or
  the portable `REG_BASE[]` array.
- `include/gba/defines.h` exposes GBA memory regions. In a portable build,
  `src/platform/bios.c` owns `REG_BASE`, `PLTT`, `VRAM_`, `OAM`, `FLASH_BASE`, and
  `SOUND_INFO_PTR`.
- `src/platform/bios.c` implements host versions of BIOS operations such as
  `CpuSet`, `CpuFastSet`, decompression, division, and affine helpers.
- `src/platform/dma.c` records DMA transfers and executes them at the simulated
  transfer point. `src/platform/gba_easy_draw.c` or `gba_fast_draw.c` renders the
  GBA scene into a 240x160 host pixel buffer.
- `src/agb_flash.c` retains the game flash API. In `PORTABLE` builds its read path
  delegates to `Platform_ReadFlash`; `src/save.c` performs the normal sector,
  checksum, signature, and save-slot logic.
- `src/platform/sdl2.c` starts `AgbMain` on a worker thread and provides the host
  frame loop. `src/platform/win32.c` is an alternative non-SDL native backend selected
  by `TARGET_PLATFORM=PLATFORM_WIN32`.

The port is consequently a GBA simulation boundary around the game, not a conventional
PC-native engine. Keeping the logical GBA data model is useful for vanilla fidelity,
but native host resources must be separated from that model before 64-bit or runtime
content loading is attempted.

### Important address categories

These categories must remain distinct during migration:

| Category | Examples | 64-bit treatment |
| --- | --- | --- |
| Logical GBA address/offset | script operands, `VRAM` offsets, DMA register values, flash offsets, `EWRAM_START` checks | Keep a specified 32-bit wire/value format, preferably behind `GbaAddr`/`GbaOffset` types. Resolve through a host memory map rather than casting to a host pointer. |
| GBA-layout data | save blocks, `OamData`, register fields, battle protocol buffers, compressed asset formats | Preserve field widths and documented offsets. Use explicit serialization structs where the data crosses a file or generated-data boundary. |
| Native host pointer | `void *`, asset buffers, SDL objects, heap pointers | Use the actual host pointer type; do not store it in game `u32`/halfword fields. Use handles where a game field has only 32 bits. |
| Native callback | task, sprite, main, VBlank, HBlank, and audio callbacks | Keep as a typed function pointer or use a callback registry/handle. Never split a function pointer into two 16-bit words on a 64-bit host. |
| Function/data address in generated assembly | `.int Symbol`, `.4byte Symbol` | Treat as a relocation or symbolic reference. Do not widen only the assembler directive without changing table consumers. |

## Build pipeline and linker behavior

### Make targets and compiler selection

`Makefile_pc` is the native-port build entry point:

- `NATIVE_LINUX=1` selects the host Linux compiler names with no prefix, sets
  `ROM=pokeemerald`, `OBJ_DIR=build/linux`, and adds `-D NATIVE_LINUX`.
- The default/non-Linux branch assumes the cross toolchain prefix
  `i686-w64-mingw32-`, `SDL_DIR=/usr/i686-w64-mingw32`, and produces
  `pokeemerald.exe` in `build/pc`.
- `ASFLAGS` contains `--32`, and common `CFLAGS` and Linux `CPPFLAGS` contain
  `-m32`. Windows additionally uses `-fleading-underscore`; `FIX_UNDERSCORE` uses
  `objcopy --prefix-symbol _` for the 32-bit Windows object ABI.
- `TARGET_PLATFORM=PLATFORM_SDL2`, `TILE_RENDERER=RENDERER_EASY_DRAW`,
  `MODERN=1`, `PORTABLE=1`, and `UBFIX=1` are selected in the makefile.
- The makefile evaluates `GCC_VER` using `$(CC) -dumpversion` before the `linux`
  recursive target overrides `NATIVE_LINUX`. As a result, an outer Linux invocation
  can print `i686-w64-mingw32-gcc: command not found` even though the inner Linux
  build is selected. This is a build-system defect and can mask a no-op build when
  old artifacts already exist.

### Tools and generated inputs

`make_tools.mk` builds the repository tools used by the port: `aif2pcm`, `gbafix`,
`gbagfx`, `jsonproc`, `mapjson`, `mid2agb`, `preproc`, `ramscrgen`, `rsfont`, and
`scaninc`. The normal `rom` path also requests the generated targets.

The source-to-object flow is:

1. C sources under `src/` and `gflib/` are preprocessed by the selected CPP.
2. The custom `preproc` handles the decompilation's assembly/charmap conventions.
3. Linux uses host GCC to turn preprocessed C into assembly, then GNU `as --32`
   assembles it. Windows invokes the compiler's `cc1` path and then the same
   32-bit assembler.
4. Ordinary assembly is assembled directly. Data assembly is passed through
   `preproc`, CPP, `preproc -ie`, and `ASM_PSEUDO_OP_CONV`; that conversion changes
   `.4byte` to `.int` and `.2byte` to `.short` for GNU assembler compatibility.
5. MIDI is converted by `mid2agb`; AIF samples are converted by `aif2pcm`; graphics
   are converted/compressed by `gbagfx`; map and JSON data are expanded by `mapjson`
   and `jsonproc`.
6. The Linux `rom` target links all objects directly with `$(CC)`, `-no-pie`, the
   SDL2/SDL2_image pkg-config libraries, and `-lm`. This is the working baseline.

`C_ASM_SRCS` still enumerates GBA/ARM assembly such as `src/crt0.s`,
`src/m4a_1.s`, `src/libgcnmultiboot.s`, and `src/rom_header.s`, but the PC `OBJS`
list is built from `C_OBJS`, `GFLIB_OBJS`, `ASM_OBJS`, data objects, and song objects;
the C-assembly object list is not included in that PC link. This is why ARM assembly
does not prevent the current Linux build. It is also an important boundary: any future
64-bit target must explicitly classify these files as GBA-only, replace them with a
portable implementation, or provide an architecture-specific backend. They must not
be accidentally assembled as x86 objects.

### Linker targets

There are two materially different link paths:

- `$(ROM)` on Linux (`Makefile_pc:365-367`) uses GCC's normal host linker. It does
  not consume `LDFLAGS` or `ld_script_pc.ld`; the resulting executable is an ordinary
  non-PIE ELF executable.
- `$(ELF)` (`Makefile_pc:361-363`) invokes `ld -T ld_script.ld` from `build/linux`
  or `build/pc`, passes generated `sym_bss.ld`, `sym_common.ld`, and `sym_ewram.ld`
  through `LIB`, and then calls `gbafix`. `LD_SCRIPT` is set to `ld_script_pc.ld`,
  but that file is not present in the checkout. Therefore an explicit ELF target is
  currently a latent failure even though the normal Linux `rom` target succeeds.

`ld_script.ld` and `ld_script_modern.ld` are GBA-oriented scripts with fixed EWRAM,
IWRAM, and ROM regions. They are not a suitable 64-bit host linker layout. The
`ramscrgen` output and `sym_*.txt` files also encode the original object/section
organization and should not be treated as a PC ABI.

### Baseline verification

The requested command was run with:

```sh
PKG_CONFIG_LIBDIR=/usr/lib/i386-linux-gnu/pkgconfig:/usr/share/pkgconfig \
  make -f Makefile_pc linux -j$(nproc)
```

It exited successfully, but the outer make printed two instances of
`i686-w64-mingw32-gcc: command not found` while evaluating the default compiler
version. The recursive Linux target then reported `Nothing to be done for 'rom'`
because existing artifacts were current.

A forced rebuild of the actual Linux path was also run with:

```sh
PKG_CONFIG_LIBDIR=/usr/lib/i386-linux-gnu/pkgconfig:/usr/share/pkgconfig \
  make -f Makefile_pc NATIVE_LINUX=1 rom -B -j$(nproc)
```

It completed and linked successfully. The resulting `pokeemerald` is an ELF32,
i386, dynamically linked, non-stripped executable. The rebuild emitted many existing
GCC `-Wunused-const-variable=` warnings for unused graphics, palette, sprite, text,
and animation constants (notably in `battle_anim_water.c`,
`battle_anim_effects_3.c`, `battle_interface.c`, `berry_blender.c`, `contest.c`,
and `field_door.c`). No compilation or link failure occurred. The worktree remained
clean; build products are ignored.

## SDL2 platform layer map

The public declarations are in `include/platform.h`, but the interface is not yet a
complete platform boundary.

| Concern | Current implementation | Files/functions | Audit finding |
| --- | --- | --- | --- |
| Rendering | Software GBA renderer writes a 240x160 `uint16_t` image; SDL updates a streaming texture and draws it with nearest-neighbor scaling. Linux/SDL2 loads PNG borders/backgrounds; Windows/SDL2 loads BMPs. | `src/platform/gba_easy_draw.c:891`, `gba_fast_draw.c:2671`, `sdl2.c:VDraw`, `sdl2.c:300-369`, `win32.c:VDraw` | Rendering, border policy, scaling, and window presentation are intertwined in the backend. The Win32 GDI path has a different pixel conversion and lifecycle. |
| Event/input | SDL keyboard events map Z/X/Enter/backslash/A/S/arrows to GBA keys. SDL controller events are compiled only in the Android branch. SDL Windows has an additional XInput helper. | `sdl2.c:625-657`, `893-1001`, `1003-1068` | Desktop Linux has no SDL controller/gamepad path. Input mapping is hard-coded and not a device-independent API. |
| Audio | `cgb_audio_init` is fed 42060 Hz, float, stereo, 1024-sample SDL queue audio. Volume is applied in `Platform_QueueAudio`. | `sdl2.c:195-231`, `531-543`, `music_player.c:788` | The function parameter is named samples but the caller passes bytes (`samplesPerFrame * 4`); the callee derives a float count by dividing by `sizeof(float)`. This ABI needs a documented byte/frame contract before refactoring. |
| Timing | A worker thread runs `AgbMain`; the host loop uses an accumulator and a 1/60 fixed timestep. Speed-up changes `timeScale` to 5 and pauses/clears audio in SDL. | `sdl2.c:250-379`, `DoMain`, `VBlankIntrWait` | Host wall time, simulation time, render cadence, audio queueing, and GBA VBlank are coupled. Timing changes can alter input sampling and script/battle behavior. |
| VBlank/DMA | A frame-available atomic and semaphore synchronize the game thread and presentation thread. VBlank sets `REG_DISPSTAT`, runs HBlank DMAs, invokes `gIntrTable[4]`, and releases the semaphore. | `sdl2.c:357-369`, `1091-1101`; `src/platform/dma.c` | This is a behavioral scheduler, not just a display callback. It needs golden timing tests before changes. |
| Window | SDL creates a resizable window, supports desktop fullscreen, integer/non-integer scaling, a 320x180 default desktop size, and optional border/background textures. | `sdl2.c:137-161`, `480-493`, `300-350` | Resolution policy is hard-coded and not represented as a stable host service. |
| Config | Plain `key=value` file. Desktop paths default to `pokeemerald.cfg`; Android uses `SDL_GetPrefPath`. Settings are loaded and immediately written when changed. | `sdl2.c:58-64`, `430-478`, `572-600` | Desktop config/save locations are current-working-directory relative. Parsing is permissive and settings are stored in an unvalidated byte array. |
| RTC | SDL uses `time`/`localtime`; Win32 uses `GetLocalTime`; both convert to GBA BCD-like fields. | `sdl2.c:1103+`, `win32.c:702+` | RTC behavior is OS-specific and should be normalized behind a clock service for deterministic tests. |

`src/platform/win32.c` is a separate native window implementation rather than a
small OS adapter. It creates a Win32 window and GDI DIB, uses a semaphore/event for
VBlank, reads a relative save file, and implements no audio (`Platform_QueueAudio` is
a no-op). It also contains the comment `//todo: convert these to int64` near its
performance timing variables. Its save code uses `CreateFileA` and 32-bit-style
`SetFilePointer`; `StoreSaveFile` and `CloseSaveFile` test handle validity with `||`
where `&&` is required. The default make configuration is SDL2, so this file is
currently an alternate/partially maintained path.

## Save, flash, and configuration architecture

The game-side save format remains the vanilla flash model:

- `src/load_save.c` owns the ASLR save-block storage and pointers. The random offset
  is an in-memory layout trick; `MoveSaveBlocks_ResetHeap` copies the blocks and
  recreates pointers without changing the serialized save format.
- `src/save.c` builds RAM sector locations, copies game state into sectors, writes
  signatures/checksums, and calls `Platform_StoreSaveFile` in `PORTABLE` builds after
  save operations (`src/save.c:768-789`).
- `src/agb_flash.c` keeps the flash API and function-pointer state. The portable
  `ReadFlash` path delegates to the platform backend (`src/agb_flash.c:145-149`).
- `src/platform/bios.c` owns a 131072-byte `FLASH_BASE` buffer, preserving the
  expected flash capacity and erased value.

The SDL backend opens `pokeemerald.sav` as `r+b` or creates it as `w+b`, reads at most
the flash buffer size, and fills missing bytes with `0xFF`. Flash reads reopen that
path and seek by sector offset. Full saves rewrite the 128 KiB buffer from offset zero.
The write path does not explicitly `fflush`, `fsync`, or truncate; shutdown closes the
file without calling the commented-out final `StoreSaveFile` in `sdl2.c:381`.
Normal save operations do call the store function, but a robust PC backend should use
atomic replacement and explicit error reporting.

Configuration is independent of the vanilla save block. Current desktop settings
include fullscreen, window scale, integer scale, VSync, border, volume, and a border
background selection. Existing game save fields are consulted as a legacy fallback for
the background selection. Future PC metadata (enabled mods, content fingerprint, and
mod save schema) should remain in a separate versioned profile/sidecar file rather
than changing the 128 KiB vanilla flash image.

## 32-bit dependency inventory

### Build and ABI assumptions

- `Makefile_pc:65,70,83` forces 32-bit assembly/C compilation.
- `Makefile_pc:6,13,15-20,76,97-110` assumes an i686 MinGW toolchain, i686 SDL
  installation, leading underscores, and 32-bit Windows libraries.
- `include/gba/types.h` intentionally defines `u32`, `s32`, and fixed-width GBA
  fields. These types must not be replaced with `unsigned long` or `size_t`.
- Many structures model the original GBA layout, including bitfields and arrays. Any
  structure containing a pointer or function pointer changes size/alignment on LP64.
- `-fno-builtin`, `-fno-dce`, `-O3`, assembly post-processing, and the custom
  preprocessor are part of the current behavior and should be changed independently.

### Immediate host-pointer truncation and reconstruction sites

The following are known code-level blockers, not merely candidates from a grep:

| File/function | Current representation | Failure on 64-bit |
| --- | --- | --- |
| `include/gba/defines.h:58-60` | `VRAM` is `(u32)VRAM_` | The portable VRAM base is truncated before address arithmetic or DMA calls. |
| `src/platform/dma.c:80` | DMA reload casts a 32-bit register value to `uintptr_t` and then `void *` | Reloaded destinations become invalid host pointers. |
| `src/platform/dma.c:99-100` | `src`/`dest` are cast through `uintptr_t` into `vu32` register mirrors | Register observability and pointer recovery lose the high bits. |
| `src/task.c:139-153` | A `TaskFunc` follow-up is split into two `s16` words via `u32` | Callback address is truncated and reconstructed as a 32-bit value. |
| `src/battle_anim_mons.c:417-430,1951-1960` | Sprite callbacks and pointers are split into 16-bit fields/`u32` | Function/data pointer truncation. |
| `src/apprentice.c:1290-1308` | Callback bits are stored in task data and cast back from `(u32)` | Same callback truncation. |
| `src/field_effect.c:2617,2785` | VBlank callbacks go through `StoreWordInTwoHalfwords` | Same callback truncation. |
| `src/field_door.c`, `fldeff_*.c`, `pokemon_jump.c`, `easy_chat.c`, `pokenav.c` | Task data stores callbacks, object pointers, or graphics pointers as words | Same truncation across field effects and minigames. |
| `src/pokeball.c:673-675,814-815` | `Pokemon *` is packed into task fields | Invalid pointer on reconstruction. |
| `src/event_object_movement.c:8902-8930`, `trainer_see.c:630-652` | Object-event pointers are passed through `u32` helper storage | Invalid object pointers. |
| `src/shop.c:413-458`, `slot_machine.c:1119-1127` | Main callbacks are packed and recovered from task/state words | Invalid callback on 64-bit. |
| `src/mystery_event_script.c:62`, `scrcmd.c:192-203` | Script base/address is held in `u32`; script words become pointers | Confuses logical script addresses with native pointers. |
| `src/m4a.c:1739`, `include/gba/m4a_internal.h:245` | Music `gotoTarget` is a `u32` native address | Cry/song control pointer is truncated. |
| `src/music_player.c:213-265` | `sizeof(u8 *)` is used to consume music command pointer operands | On LP64 the bytecode parser can consume 8 bytes from a 4-byte format. |
| `src/hall_of_fame.c`, `main_menu.c`, `starter_choose.c`, `diploma.c`, `berry_fix_program.c` | GBA memory macros are cast through `u32`/`uintptr_t` for DMA | Depends on the truncated `VRAM`/OAM address model. |
| `src/save_failed_screen.c:196,210-212`, `pokedex.c:5270`, `window.c:577` | Buffers/pointers are passed through `u32` window/DMA fields | High pointer bits are discarded. |
| `src/record_mixing.c:590-591` | Pointers are copied into `u16` protocol/record arrays | Native pointer cannot fit and is not portable data. |

The inventory is intentionally broader than compiler diagnostics: code that currently
works only because Linux is i386 may not warn when the cast is explicit. A first 64-bit
port should add compiler diagnostics and runtime assertions around every conversion.

There are also legacy fixed-address paths that are mostly excluded from `PORTABLE`
but must remain classified: `src/librfu_rfu.c:134-180,340-365` uses `EWRAM_START`/
`IWRAM_START` as real addresses in the non-portable RFU/multiboot implementation;
`src/libgcnmultiboot.s` embeds GBA memory addresses; and `src/libisagbprn.c:179`
writes a no$gba debug address. These are not current SDL gameplay paths, but a
64-bit build must either exclude them by an explicit target contract or route them
through the logical GBA address service.

### Pointer-bearing C structures and tables

C tables are not all fixed-width GBA data. Examples include:

- `src/data/pokemon/species_info.h`, `src/data/battle_moves.h`, `src/data/items.h`,
  and `src/data/trainers.h`. Trainer records contain pointers to party data and text;
  species/move/item records are consumed as indexed registries throughout
  `src/pokemon.c`, `src/battle_script_commands.c`, `src/item.c`, and battle modules.
- Sprite templates, image/frame tables, palette tables, animation tables, field
  effect tables, and graphics tables contain data pointers and callback pointers.
- `src/rom_header_gf.c` exposes pointers to the mon icon, species, items, and move
  registries in a GBA-shaped header structure.
- `gMapGroups` in `data/maps.s`/map data points to map headers; map headers point to
  layouts, events, connections, scripts, and encounter data.
- Music player structures and `gSongTable` contain pointers to tracks, voices,
  instrument data, and callbacks/targets.

On a 64-bit compiler these structures have different sizes and alignments even if the
individual scalar fields remain fixed. Generated assembly tables that assume 4-byte
pointer slots cannot safely be read as those native C structures. The migration needs
wire structs with explicit offsets plus runtime structs containing host pointers, or a
controlled hydration step that resolves each 32-bit symbolic reference.

### Intentional 32-bit values that should not be widened

GBA save fields, battle protocol buffers, bitfields, script opcodes/operands, DMA
control/count registers, register offsets, flash sector numbers, compressed asset
formats, and GBA affine/fixed-point values are logical 8/16/32-bit values. Widening
these changes serialization, arithmetic, or vanilla timing. The correct change is to
make their logical nature explicit and convert only at the host boundary.

## Generated data and assembly architecture

### Generated files and dependencies

- `map_data_rules.mk` uses `tools/mapjson/mapjson` to turn
  `data/maps/*/map.json`, `data/maps/map_groups.json`, and
  `data/layouts/layouts.json` into map headers, events, connections, layouts,
  `include/constants/map_groups.h`, `layouts.h`, and `map_event_ids.h`.
- `json_data_rules.mk` uses `tools/jsonproc/jsonproc` and Inja templates to generate
  wild encounter, region map, and healing-location headers from JSON.
- `graphics_file_rules.mk` and `spritesheet_rules.mk` invoke `gbagfx`, often followed
  by compression. Many C files refer to generated binary assets with `INCBIN_*`; the
  portable definition in `include/global.h` is a build-time compatibility macro, not
  a runtime asset loader.
- `audio_rules.mk` uses `midi.cfg` and `mid2agb` to generate song assembly, while
  `aif2pcm` creates sample binaries and compressed cries. Songs are linked into the
  executable; there is no runtime music package registry.
- `tools/preproc`, `scaninc`, and GNU assembler process ordinary and data assembly.
  `ramscrgen` creates linker fragments from `sym_bss.txt`, `sym_common.txt`, and
  `sym_ewram.txt`.

### Absolute and pointer-bearing assembly

The build converts `.4byte` to `.int`, retaining four-byte entries. Pointer/address
fields occur in, among others:

- `asm/macros/event.inc`: script destinations, text pointers, product lists,
  variables, and event command operands.
- `asm/macros/map.inc`: map event/header pointers.
- `asm/macros/battle_script.inc`, `battle_ai_script.inc`,
  `battle_anim_script.inc`, `contest_ai_script.inc`, and `field_effect_script.inc`:
  script branch destinations and tables.
- `asm/macros/m4a.inc` and `music_voice.inc`: music/player/voice references.
- `data/maps/*/header.inc`, `connections.inc`, `data/maps.s`, and
  `data/map_events.s`: map graph and event references.
- `data/event_scripts.s`, `battle_scripts_*.s`, `battle_anim_scripts.s`,
  `battle_ai_scripts.s`, `contest_ai_scripts.s`, and `field_effect_scripts.s`:
  script pointers and branch tables.
- `data/contest_ai_scripts.s` and tables such as the move-effect script tables:
  function/data symbols emitted as four-byte entries.

Some `.int` entries are scalar GBA values and some are addresses; the consumer schema
determines which. A textual replacement with `.quad` would therefore be unsafe. A
64-bit runtime needs one of:

1. generate native C runtime tables from a stable symbolic/intermediate format;
2. keep a 32-bit serialized image and relocate/hydrate references into runtime objects;
3. use explicit 32-bit logical offsets/handles and resolve them through registries; or
4. retain a strictly 32-bit content/object ABI in a separately loaded data VM.

The recommended path is (1) for new external content and (2)/(3) for vanilla tables,
with each table schema documented. Function pointers must use a registry or native
runtime function pointer, never a serialized process address.

## 64-bit migration blockers and migration rules

### Blocker classes

1. **Compiler/linker target:** remove unconditional `-m32`/`--32`, provide native
   Linux and MinGW-w64 64-bit toolchain selection, use 64-bit SDL libraries, and
   remove the leading-underscore workaround where the ABI does not need it.
2. **Host memory map:** replace `(u32)VRAM_` and all native-address register
   round-trips with a logical GBA address resolver. The register mirror may still
   contain a 32-bit logical address, but must not contain an unrecoverable host
   pointer.
3. **Pointer storage:** replace every callback/object/asset pointer packed in task
   data, script state, protocol buffers, or `u32` fields with a handle table or a
   native pointer member. Preserve task data bytes when they are part of vanilla
   behavior by maintaining a sidecar handle map.
4. **Generated data:** define schemas for every pointer-bearing assembly table and
   hydrate symbolic references. Keep scalar command operands four bytes.
5. **C ABI/layout:** add `_Static_assert` checks for fixed GBA structs and explicit
   serialization for any file/network/save structure. Do not rely on `sizeof(pointer)`
   or compiler packing for content data.
6. **Music/script bytecode:** make command operand widths explicit. In particular,
   `music_player.c` must never use host pointer size to parse a fixed GBA command.
7. **Platform separation:** extract window/input/audio/clock/storage services from
   `sdl2.c` and give each backend the same semantics. The game-facing API should not
   expose SDL, Win32 handles, `FILE *`, or host paths.
8. **Windows parity:** choose SDL2 as the supported Windows backend or deliberately
   finish the GDI backend; do not maintain two subtly different timing/audio/save
   implementations as if they were equivalent.
9. **Build reproducibility:** make compiler/pkg-config/tool paths explicit, stop
   evaluating the default Windows compiler during a Linux target, and add 32/64-bit
   CI jobs with clean builds.
10. **Behavioral proof:** compare vanilla save, map, battle, script, audio, input,
    rendering, RTC, and timing behavior before and after each class of change.

### Suggested host API boundary

The eventual stable PC API should be a small game-facing interface with opaque host
objects, for example:

- `Platform_Init`/`Platform_Shutdown`;
- `Platform_PollInput` returning a normalized GBA-key plus controller state;
- `Platform_BeginFrame`/`Platform_PresentFrame` accepting a fixed 240x160 buffer;
- `Platform_QueueAudio` with an explicit sample count, channel count, and format;
- `Platform_WaitVBlank`/`Platform_SignalFrame` with documented scheduler semantics;
- `Platform_SaveRead`/`Platform_SaveWriteAtomic` for bounded flash images;
- `Platform_ConfigLoad`/`Platform_ConfigStore` with an OS-specific user-data root;
- `Platform_ClockGet` with injectable deterministic test time;
- a separate `GbaMemory_Read/Write/Resolve` service for logical addresses and DMA.

`include/platform.h` should eventually declare this contract. SDL and Win32 details
should live under `src/platform/backends/`, not in game modules. The current
`DrawFrame`, `RunDMAs`, and `VBlankIntrWait` functions are useful seams but need
documented ownership and value semantics.

## Where a future mod system can hook cleanly

The cleanest first hooks are at existing indexed-table and resource lookup boundaries,
not compiled-C patch points:

- species, moves, items, trainers, wild encounters, and learnsets can be read through
  registries replacing direct `gSpeciesInfo`, `gBattleMoves`, `gItems`, `gTrainers`,
  and `gWildMonHeaders` array access. Existing consumers include `pokemon.c`,
  `battle_script_commands.c`, `item.c`, `battle_main.c`, and `wild_encounter.c`.
- map lookup can be centralized around `gMapGroups` and the map-header/layout/event
  consumers in `overworld.c`, `map.c`, `event_object_movement.c`, and `scrcmd.c`.
- graphics can be keyed by logical asset IDs at sprite frame/palette/object-event/
  Pokémon/trainer lookup tables rather than by replacing a compiled symbol.
- music and sound can be keyed at the song table/player entry points in
  `m4a_tables.c`, `m4a.c`, and `music_player.c`, with decoded/validated runtime
  resources.
- field, battle, contest, and AI scripts should first remain in the existing VMs.
  External scripts can be parsed to a stable intermediate representation and bound
  to symbolic map/species/move/event IDs. `scrcmd.c`, battle script command tables,
  `battle_ai_script_commands.c`, and field-effect script consumers are the likely
  integration points.
- native C extensions should be a late, explicitly versioned API and should call
  through opaque handles; they should not depend on internal globals or compiled
  addresses.

Direct global array references are currently widespread, so the first data milestone
needs accessors or generated registry indirection. This is a compatibility layer, not
a reason to rewrite all gameplay code at once.

## Compile-time-only systems and externalization work

Currently compile-time or link-time only:

- Pokémon species data, moves, items, abilities/text, learnsets, evolution, cries,
  graphics tables, trainer classes/parties, battle/contest/AI tables, and much of
  wild encounter data are C headers or assembly objects.
- Maps/layouts/events/connections are generated from JSON into assembly include files
  and linked pointer tables.
- Scripts are assembled into bytecode objects with pointer-like branch destinations.
- Graphics and audio are converted/compressed during the build and linked into the
  executable; C `INCBIN_*` references are not filesystem lookups at runtime.
- Constants and ID ranges are generated headers. Array order is often the ID scheme,
  and comments in `include/constants/items.h` document code that assumes contiguous
  IDs.

Externalization requires, per subsystem:

1. stable namespaced IDs independent of array order;
2. versioned schemas and an explicit endianness/width policy;
3. validation, bounds checks, and deterministic canonicalization;
4. runtime ownership/lifetime rules for decoded assets;
5. a registry/accessor layer that supplies vanilla data when no override exists;
6. reference resolution for scripts, maps, tables, callbacks, and asset dependencies;
7. memory/error budgets and a preflight phase before mutating live game state;
8. content fingerprints for diagnostics and save compatibility;
9. test fixtures that prove an empty registry is byte/behavior equivalent to vanilla.

Do not externalize native pointers. External content should contain symbolic IDs,
indices, offsets, or serialized data only. Runtime tables may contain host pointers
after validation and hydration.

## Recommended migration phases

### Phase 0: preserve and measure

- Capture clean 32-bit build, executable metadata, warnings, save round-trip, and
  deterministic smoke outputs.
- Add CI/build scripts for the existing 32-bit Linux baseline and a clean-build check.
- Inventory every pointer-to-integer conversion with a suppression/justification list.

### Phase 1: 64-bit pointer quarantine (next milestone)

- Add fixed-width `GbaAddr`/`GbaOffset` and host pointer/handle helpers.
- Fix portable VRAM address arithmetic and DMA register/reload handling.
- Replace task/field-effect/object-event callback packing with a sidecar handle table
  while preserving the game-visible task bytes.
- Make music/script operand widths explicit and add static assertions for fixed data.
- Add a real 64-bit Linux compile/link target and run vanilla smoke tests. No mods.

### Phase 2: stable PC platform API

- Split SDL backend responsibilities into storage/config/clock/input/audio/video and
  scheduler adapters.
- Normalize save paths, atomic writes, settings schema, controller mapping, window
  modes, and audio sample semantics on Linux and Windows.
- Choose SDL2 as the supported Windows renderer/audio backend or complete the native
  backend with equivalent tests.

### Phase 3: data registries with vanilla fallback

- Generate schemas and IDs for one low-risk resource family first, preferably graphics
  or item metadata.
- Keep vanilla tables as the default provider and prove disabled-mod equivalence.
- Add runtime asset loading and validation without changing script execution.

### Phase 4: structured content mods

- Add manifest discovery, deterministic dependency resolution, asset overrides, and
  structured species/move/item/trainer/map data overlays as described in
  `docs/MOD_SYSTEM_DESIGN.md`.
- Keep mod state and migrations outside the vanilla flash image.

### Phase 5: script/event extensions

- Define external script IR/bytecode and symbolic event targets.
- Add map/event replacement and composition policies with preflight validation.
- Add deterministic replay/regression fixtures for every script VM.

### Phase 6: optional native extensions

- Add a versioned, opaque-handle plugin ABI only after the data/script APIs are stable.
- Make loading opt-in and apply the security policy in the mod design document.

## Risks and regression-testing strategy

### High-risk changes

- Widening a GBA address, register, save field, bitfield, or bytecode operand.
- Changing pointer-containing C structure layout or assembly table entry width.
- Replacing task callback packing without preserving task lifecycle/order.
- Changing VBlank semaphore ordering, accumulator behavior, audio queue timing, or
  input sampling.
- Changing `sizeof`-based copy sizes in save blocks, DMA, structs, or music tracks.
- Moving generated tables or changing their ordering, alignment, or symbol names.
- Switching compilers/optimization/assembler behavior while also changing the runtime.
- Changing save paths or write behavior without migration, backup, and corruption tests.

### Required test layers

1. **Build:** clean 32-bit Linux, clean 64-bit Linux, 64-bit Windows, and supported
   sanitizer/diagnostic builds. Record compiler, SDL, generated-file, and executable
   fingerprints.
2. **ABI/layout:** compile-time assertions for GBA-layout structs; runtime checks for
   logical memory regions, DMA bounds, pointer handles, and table counts.
3. **Save:** new save, load existing vanilla save, both save slots, sector corruption,
   short/oversized file, atomic failure, clear-save, and modded-save compatibility.
4. **Gameplay:** boot, new game, overworld map transitions, NPC/event scripts, shops,
   field effects, wild/trainer/link battles, Pokémon storage, contests, Frontier,
   minigames, and reset paths.
5. **Data:** every indexed table at boundary IDs, missing/invalid assets, map graph
   traversal, graphics decompression, music pattern/goto/pointer commands, and text.
6. **Timing/input/audio:** one-frame input edges, held keys, controller hotplug,
   pause/speed-up, VBlank/HBlank callbacks, RTC rollover, audio underrun, and long
   deterministic runs.
7. **Vanilla equivalence:** mods disabled must use the same default providers and
   produce identical save semantics and stable gameplay checkpoints on 32-bit and
   64-bit hosts. Where pixel/audio bytes cannot be compared directly, compare decoded
   frame hashes, event traces, RNG checkpoints, and normalized audio buffers.

## Specific files and functions by area

| Area | Primary files/functions |
| --- | --- |
| Build/toolchain | `Makefile_pc`, `make_tools.mk`, `graphics_file_rules.mk`, `spritesheet_rules.mk`, `map_data_rules.mk`, `json_data_rules.mk`, `audio_rules.mk` |
| Linker | `Makefile_pc:337-363`, absent `ld_script_pc.ld`, `ld_script.ld`, `ld_script_modern.ld`, `sym_bss.txt`, `sym_common.txt`, `sym_ewram.txt`, `tools/ramscrgen` |
| Host memory/BIOS | `include/gba/defines.h`, `include/gba/io_reg.h`, `include/gba/flash_internal.h`, `src/platform/bios.c` |
| DMA/render | `src/platform/dma.c`, `include/platform/dma.h`, `src/platform/gba_easy_draw.c`, `src/platform/gba_fast_draw.c`, `include/platform/framedraw.h` |
| SDL platform | `src/platform/sdl2.c`, `include/platform.h` |
| Alternate Windows platform | `src/platform/win32.c` |
| Main/timing/input seam | `src/main.c`, `src/platform/sdl2.c:ProcessEvents`, `Platform_GetKeyInput`, `VBlankIntrWait` |
| Save/flash | `src/load_save.c`, `src/save.c`, `src/agb_flash.c`, `src/platform/bios.c`, `src/platform/sdl2.c`, `src/platform/win32.c` |
| Pointer packing | `src/task.c`, `src/util.c`, `src/field_effect.c`, `src/battle_anim_mons.c`, `src/apprentice.c`, `src/event_object_movement.c`, `src/trainer_see.c`, `src/shop.c`, `src/music_player.c`, `src/m4a.c`, `src/scrcmd.c` |
| Core registries | `src/data/pokemon/species_info.h`, `src/data/battle_moves.h`, `src/data/items.h`, `src/data/trainers.h`, `src/data/wild_encounters.h`, `src/rom_header_gf.c`, `src/pokemon.c`, `src/item.c`, `src/battle_main.c`, `src/wild_encounter.c` |
| Maps/scripts | `data/maps.s`, `data/map_events.s`, `data/event_scripts.s`, `data/battle_scripts_*.s`, `asm/macros/event.inc`, `asm/macros/map.inc`, `src/overworld.c`, `src/scrcmd.c`, `src/map.c` |
| Audio/assets | `audio_rules.mk`, `sound/songs`, `sound/songs/midi`, `src/m4a.c`, `src/m4a_tables.c`, `src/music_player.c`, `graphics_file_rules.mk`, `src/graphics.c` and `INCBIN_*` users |

## Phase 1 implementation discoveries

The first native x86_64 build refined several audit assumptions:

- `GbaAddr` and `GbaOffset` are logical four-byte values, not aliases for
  `uintptr_t`. Host pointers remain typed pointers or are represented by the
  short-lived registries in `include/platform/host_memory.h`. The handle ranges are
  deliberately reserved for transient pointer and function references; they are not
  a general object/plugin ABI.
- Portable VRAM and DMA need a resolver boundary. DMA register mirrors continue to
  hold logical values, while host transfers resolve those values and validate the
  emulated memory ranges. The portable VRAM macro therefore cannot cast a host
  pointer through `u32`.
- Task, sprite, field-effect, animation, menu, and minigame callback storage cannot
  be widened without changing vanilla-shaped data. The implementation uses typed
  sidecars/handles at the affected call sites and retains the visible task/sprite
  bytes.
- Map event/template pointer fields are a special case: their surrounding map data
  contains scalar four-byte GBA operands and four-byte counts. Object-event script
  fields remain logical four-byte values with accessors; runtime-native coordinate,
  sign, and background pointer records use explicit native-width entries and padding
  assertions. A global `.4byte` to `.quad` conversion would corrupt map/script
  scalar streams.
- The generated script-command and mystery-event command tables contain native
  function pointers and need native-width entries on Linux64. Script bytecode branch,
  message, movement, and data operands remain four-byte logical operands and are
  resolved by `scrcmd.c`.
- `gSpecialVars`, `gSpecials`, and `gStdScripts` are pointer tables rather than
  bytecode. They also need native-width Linux64 entries; the failure mode was a
  truncated special-variable pointer during the first truck/overworld script.
- Pokémon cry `GOTO` operands are authored four-byte bytecode data. Storing that
  field as a native pointer inserted compiler padding and shifted the following
  command stream, so it remains `GbaAddr` and is hydrated when the cry song is
  initialized.
- M4A voice groups have a native pointer-bearing layout on Linux64 and are parsed
  with an explicit stride. `Song`, `MusicPlayer`, and mixer state retain fixed GBA
  sizes/offsets where the vanilla runtime depends on them; static assertions cover
  those contracts.
- The Linux64 target is a non-PIE SDL2 executable in `build/linux64` and uses the
  native assembler width. The existing i386 target remains `build/linux` and keeps
  its `-m32`/`--32` path. Makefile compiler-version probing is no longer evaluated
  through the MinGW compiler for native Linux builds.
