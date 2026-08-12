# pokeemerald-recomp

A native desktop recompilation/port of **Pokémon Emerald** for modern PCs.

`pokeemerald-recomp` runs the decompiled Pokémon Emerald game code directly on the host platform. It is **not a bundled GBA emulator**.

## v0.1.0-alpha

The first public alpha currently supports:

* **Linux x64**
* **Windows x64**

This is alpha software. Bugs and compatibility issues should be expected.

## ROM Required

**Pokémon Emerald is not included with this project.**

On first launch, `pokeemerald-recomp` requires the user to select a compatible Pokémon Emerald ROM they provide themselves.

The ROM is:

* selected locally
* validated locally
* never uploaded
* never included in the release package

Currently supported:

```text
Pokémon Emerald (USA)
SHA-1: f3ae088181bf583e55daf962a92bb46f4f1d07b7
```

Unsupported ROM revisions or regional versions will be rejected.

After the ROM is successfully validated and imported, the game data is stored locally and the ROM does not need to be selected again unless the imported game data is removed or reinstalled.

## Download

Prebuilt alpha releases are available from the **GitHub Releases** page.

Choose the package for your platform:

```text
pokeemerald-recomp-v0.1.0-alpha-linux-x64.tar.gz
pokeemerald-recomp-v0.1.0-alpha-windows-x64.zip
```

Do **not** download GitHub's automatically generated "Source code" archives if you simply want to play the game.

### Windows

1. Download the Windows x64 ZIP from Releases.
2. Extract the ZIP.
3. Open the extracted folder.
4. Run:

```text
pokeemerald-recomp.exe
```

5. Select a supported Pokémon Emerald ROM when prompted.
6. Create or select a profile.
7. Play.

Windows may display a SmartScreen warning because the current alpha executable is not code-signed.

### Linux

1. Download the Linux x64 `.tar.gz` from Releases.
2. Extract it.
3. Open a terminal in the extracted directory.
4. Run:

```sh
./pokeemerald-recomp
```

5. Select a supported Pokémon Emerald ROM when prompted.
6. Create or select a profile.
7. Play.

## Features

### Native desktop port

Pokémon Emerald runs as a native desktop application rather than through a bundled emulator frontend.

### Profile system

Multiple independent playthroughs can be maintained using profiles.

Each profile keeps its own:

* normal Pokémon Emerald save
* Quick Save
* manual save-state slots
* save-state screenshots and metadata
* profile-specific runtime data where appropriate

Profiles are selected from the Emerald-themed launcher before entering the game.

### Emerald-style launcher

The desktop launcher provides:

* profile selection
* new profile creation
* profile deletion
* Settings access
* ROM/game-data setup
* Play and Quit actions

### Remappable controls

Keyboard controls can be changed through the Settings interface.

Settings are available:

* from the launcher before starting the game
* in-game with `F10`

Bindings persist across relaunches.

### Fast-forward

Configurable fast-forward multipliers are supported.

Available speeds include:

```text
2x
3x
4x
5x
```

Fast-forward advances multiple simulation frames per presented frame rather than globally changing game timing.

### Native save states

The desktop port includes native save-state support in addition to Pokémon Emerald's normal save system.

Default actions:

| Action              | Key   |
| ------------------- | ----- |
| Quick Save          | `F5`  |
| Save State Manager  | `F6`  |
| Manual Save to Slot | `F7`  |
| Quick Load          | `F9`  |
| Settings            | `F10` |

Bindings may be changed through Settings.

#### Quick Save

`F5` immediately updates the active profile's Quick Save.

`F9` restores it.

#### Manual save states

`F7` opens the manual slot interface.

The Save State Manager provides:

```text
Quick Save
Slot 1
Slot 2
Slot 3
Slot 4
Slot 5
Slot 6
Slot 7
Slot 8
```

Save states include screenshot previews and metadata.

### Mid-battle save states

Native states can be created and restored during battles.

For example:

```text
battle command menu
→ save state
→ continue battle
→ load state
→ return to the saved battle state
```

States also support loading after completely quitting and restarting the application.

### Normal Pokémon saves

The original Pokémon Emerald save system remains supported independently of native save states.

Normal saves, Quick Saves, and manual native states serve different purposes and do not replace one another.

## Current Status

The current alpha has been manually tested across core gameplay including:

* startup and profile selection
* overworld movement
* menus
* dialogue
* trainer battles
* wild battles
* battle AI
* battle animations
* normal saving and relaunch
* Quick Save / Quick Load
* manual save states
* mid-battle state restoration
* cross-process state restoration
* fast-forward
* remappable controls

This does **not** mean every part of Pokémon Emerald has been exhaustively tested yet.

Bug reports and additional playthrough testing are welcome.

## Building

The project remains based on the Pokémon Emerald decompilation and retains the existing development/build infrastructure.

### Linux x64

The native Linux x64 target can be built with:

```sh
make -f Makefile_pc NATIVE_LINUX=1 LINUX64=1 rom -j"$(nproc)"
```

### Release packages

The repository includes packaging support for producing clean Linux and Windows release artifacts.

Release packages intentionally exclude:

* Pokémon Emerald ROMs
* locally imported game data
* player profiles
* `.sav` files
* native save states
* development/test artifacts

## Project Direction

`pokeemerald-recomp` is the Emerald implementation and initial foundation for a broader **Gen3Recomp** platform.

Future work may include:

* Pokémon FireRed support
* shared Gen III mod support
* additional desktop platforms
* Android
* broader content and tooling APIs

These are future goals and should not be considered supported features of the current alpha.

## Upstream

This project builds on:

* [pret/pokeemerald](https://github.com/pret/pokeemerald)
* the earlier multiplatform work this repository was forked from

The upstream Pokémon Emerald decompilation targets:

```text
pokeemerald.gba
SHA-1: f3ae088181bf583e55daf962a92bb46f4f1d07b7
```

## Legal

Pokémon, Pokémon Emerald, and related names and assets are trademarks and copyrights of their respective owners, including Nintendo, Creatures Inc., and GAME FREAK inc.

This is an unofficial fan project and is not affiliated with, authorized by, sponsored by, or endorsed by those companies.

**No Pokémon Emerald ROM is included with the release.**

Users are responsible for providing a compatible ROM for local use.

The license in [LICENSE](LICENSE) applies only to the portions of the project to which it legally applies. It does not relicense upstream code, third-party software, or copyrighted game material owned by others.
