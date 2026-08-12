# Linux64 migration

Phase 1 adds a native x86_64 Linux host target for the vanilla SDL2 port. It is a
pointer-quarantine milestone; it does not add mod loading, external content, or a
plugin ABI.

## Build targets

The existing i386 target remains `build/linux` and produces `pokeemerald`. Linux64
uses the native compiler/assembler, writes objects to `build/linux64`, and produces
`pokeemerald-linux64`. The Linux make path no longer evaluates the MinGW compiler
while selecting its compiler version.

Required 32-bit build:

```sh
PKG_CONFIG_LIBDIR=/usr/lib/i386-linux-gnu/pkgconfig:/usr/share/pkgconfig \
  make -f Makefile_pc NATIVE_LINUX=1 rom -j$(nproc)
```

Native 64-bit build:

```sh
make -f Makefile_pc linux64 -j$(nproc)
```

In the development environment used for this migration, the x86_64 SDL2 and
SDL2_image development files were staged under `/tmp/pokeemerald-sdl64` because
the system pkg-config installation did not expose all SDL2_image transitive
`.pc` files. The equivalent reproducible command was:

```sh
PKG_CONFIG_LIBDIR=/tmp/pokeemerald-sdl64/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig \
LIBRARY_PATH=/tmp/pokeemerald-sdl64/usr/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu:/usr/lib \
CPATH=/tmp/pokeemerald-sdl64/usr/include/x86_64-linux-gnu:/tmp/pokeemerald-sdl64/usr/include \
  make -f Makefile_pc NATIVE_LINUX=1 LINUX64=1 rom -j$(nproc)
```

Run Linux64 with its SDL runtime path when using that staging prefix:

```sh
SDL_AUDIODRIVER=dummy \
LD_LIBRARY_PATH=/tmp/pokeemerald-sdl64/usr/lib/x86_64-linux-gnu \
  ./pokeemerald-linux64
```

Both builds use SDL2. The native GDI backend was not expanded.

## Address and pointer representation

- `GbaAddr` and `GbaOffset` are explicit fixed-width `u32` logical values. GBA
  registers, save fields, script operands, serialized structures, and scalar
  generated `.int` data remain four bytes.
- Native pointers remain typed pointers or `uintptr_t`. `include/gba/defines.h`
  no longer converts portable VRAM pointers through `u32`.
- `HostPointerToGbaAddr` and `HostResolveGbaAddr` provide the boundary for a
  pointer that must cross a GBA-shaped field. High host pointers use a transient
  typed pointer registry; function pointers use a separate function registry.
  Handles are intentionally small and local to a process and are not save-file
  identities or a general object system.
- Task, sprite, field-effect, battle-animation, menu, and minigame paths use
  sidecars or typed handle hydration where their existing visible fields are only
  16/32 bits. Vanilla-shaped task/sprite layouts are not globally widened.
- DMA register mirrors store logical addresses. Transfer code resolves them and
  checks the emulated register/VRAM/PLTT/OAM ranges before copying.

## Generated-data fixes

Only identified pointer-bearing data changed width on Linux64:

- map event/template pointer members use controlled native-width entries and
  hydration; object-event script fields remain logical four-byte values, while
  runtime-native coordinate/sign/background records have explicit padding and
  size assertions; counts and scalar map/script fields stay four bytes;
- script-command and mystery-event command tables use native-width function
  entries, while field-script operands remain four-byte bytecode;
- `gSpecialVars`, `gSpecials`, and `gStdScripts` use native-width pointer entries;
- M4A voice groups use an explicit native pointer-bearing stride, while `Song`,
  `MusicPlayer`, mixer state, and cry bytecode operands preserve their fixed
  contracts;
- Pokémon cry `GOTO` remains a four-byte `GbaAddr`, then hydrates at runtime so
  compiler alignment cannot change the bytecode stream.

This is deliberately not a global `.4byte`/`.int` to `.quad` conversion. Scalar
four-byte operands are still parsed with `ScriptReadWord` and equivalent explicit
readers.

## Diagnostics and remaining assumptions

The build enables `-Wpointer-to-int-cast` and `-Wint-to-pointer-cast` without
globally disabling the resulting diagnostics. Fixed audio/map/GBA-layout contracts
have compile-time assertions. Host memory checks diagnose NULL, overflow, and
portable-region overrun cases.

Remaining 32-bit assumptions include legacy RFU/link paths, some GBA-only assembly
and protocol representations, and generated pointer tables outside the tables
encountered by the Linux64 vanilla path. They must be audited individually before
external content or a plugin ABI is attempted. Save files remain the vanilla
128 KiB flash image; host handles are never serialized into them.

## Tests performed

- The required i386 Linux build completed successfully.
- The native Linux64 SDL2 build completed successfully, repeatedly, with no
  pointer-width warning promoted to a workaround or suppressed globally.
- The final rebuild after the map-record padding fix passed both the Linux64
  assertions and the required i386 command.
- `file` identified `pokeemerald` as ELF32/i386 and `pokeemerald-linux64` as
  ELF64/x86-64.
- Linux64 was run under Xvfb with dummy audio. Boot/opening flow, title screen,
  main menu, new-game Birch speech, truck initialization, music changes, and
  Pokémon cry playback were exercised. A long audio/cry soak completed after the
  M4A layout and cry operand fixes.
- The first truck/overworld script failure was reproduced and traced to truncated
  `gSpecialVars` entries; the corrected native table was then exercised past the
  former crash point.

The automated Xvfb input probe reached the modal truck-box interaction but did not
complete a reliable route-to-grass/wild-battle/save/relaunch cycle. Those scenarios
remain required follow-up validation rather than being claimed as complete here.
No known vanilla save regression was introduced by the logical-address changes;
an end-to-end save checksum/round-trip result is still pending this harness fix.

## Known regressions and scope boundaries

The SDL2 desktop input probe needs a more reliable modal-dialog/menu driver for the
remaining overworld, battle, and save cases. This is a test-harness limitation
observed during migration, not a deliberate gameplay behavior change. Mod loading,
runtime content externalization, and native GDI modernization remain out of scope.
