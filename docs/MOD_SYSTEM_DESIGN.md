# Native PC mod system design

This is a design proposal only. No loader, manifest parser, runtime registry, or
plugin ABI is implemented by this audit.

## Goals and non-goals

The mod system should support data, graphics, audio, maps, trainers, Pokémon, moves,
items, and eventually script/event modifications while preserving vanilla Emerald
behavior when no mods are enabled. It should make normal content possible without
patching compiled C or depending on process addresses.

The initial runtime should discover and validate content before the game starts. It
should use stable IDs, explicit schemas, deterministic ordering, and vanilla fallback.
Native extensions are a later, opt-in capability and are not required for ordinary
mods.

The proposed user-facing layout is:

```text
mods/
  example-mod/
    mod.toml
    data/
    graphics/
    audio/
    maps/
    scripts/
```

The loader should also accept packaged archives in a future release, but a directory
is the simplest development and diagnostic format.

## Manifest

`mod.toml` is the authoritative identity and dependency file. A first schema could be:

```toml
schema_version = 1
id = "com.example.my-mod"
name = "My Mod"
version = "1.2.0"
game = "pokeemerald-multiplatform"
game_version = "pc-port-vanilla-<fingerprint>"
load_priority = 0

[[dependencies]]
id = "com.example.base-content"
version = ">=1.0.0 <2.0.0"
optional = false

[[conflicts]]
id = "com.example.alternate-content"
version = ">=1.0.0"
reason = "Both replace the same map set"
```

Required rules:

- `id` is globally unique, lowercase, immutable, and namespaced. Reverse-DNS-like
  IDs are preferable to display names. Directory names are not identity.
- `version` uses a documented semantic-version subset. Version comparisons must be
  deterministic and must not depend on locale or filesystem ordering.
- `schema_version` describes the manifest/content schema. `game_version` or a base
  content fingerprint prevents silently loading data authored for another base.
- `dependencies` may specify a version range and whether the dependency is optional.
  A dependency must resolve to one enabled mod before the dependent mod is active.
- `conflicts` may target an ID/range and should be checked after dependency resolution.
- `load_priority` is only a tie-breaker; it must not override dependency edges.
- Enable/disable state belongs to the user's profile/configuration, not to a mod's
  manifest. A downloaded mod must not enable itself by editing another mod.
- Unknown fields should be retained for forward compatibility but ignored unless the
  manifest declares a schema that requires them. Unknown required schema versions
  should disable the mod with a clear diagnostic.

Optional manifest fields can declare content capabilities, authorship/license,
homepage, supported platforms, minimum host API version, save schema/migration IDs,
and native-plugin permissions. They are metadata, not authority to bypass validation.

## Discovery, enablement, and deterministic load order

1. Determine the user data root using the platform service, not the current working
   directory. Search the configured `mods/` directory and optional built-in mod
   roots.
2. Enumerate directory entries, canonicalize paths, reject symlinks/path escapes as
   configured by the security policy, and parse every candidate manifest.
3. Assign each candidate a diagnostic state: valid, disabled by user, incompatible,
   missing dependency, conflict, invalid content, or active.
4. Resolve dependencies and reject cycles. Required-dependency failures disable the
   dependent mod; optional dependencies are recorded as absent.
5. Resolve conflicts deterministically. The safest default is to disable the later
   conflicting mod and report the exact pair. An explicit user choice may select one
   side, but a conflict must never be resolved by filesystem order.
6. Topologically sort active mods by dependency edges. Among otherwise independent
   nodes, sort by `load_priority` and then bytewise `id`. Do not use directory scan
   order, locale collation, timestamps, or hash-map iteration order.
7. Run a preflight merge/validation pass using that order. If a resource conflict is
   fatal, disable the offending mod(s) and repeat resolution; never leave a partially
   applied registry.
8. Commit one immutable content snapshot to the game runtime. A play session uses
   that snapshot; reload/hot reload is a later feature with its own state rules.

The loader log should include the final ordered IDs, versions, content hashes,
disabled reasons, dependency graph, and every override winner. This is necessary for
reproducible bug reports and deterministic saves.

## Resource model and vanilla fallback

Mods should refer to virtual resource IDs, not compiled symbol names or absolute
addresses. A resource key should include a type and namespace, for example:

```text
species:vanilla.species.torchic
move:vanilla.move.overheat
item:vanilla.item.potion
map:vanilla.map.littleroot_town
graphics:pokemon.front.torchic
audio:song.petalgurg
script:map.littleroot_town.intro
```

The base game provides the lowest-precedence provider for every supported vanilla ID.
With no active mods, the registry must select that provider without a different code
path that changes behavior. A missing override, optional field, or failed replacement
falls back to the base provider where the schema permits it.

There are three useful override modes:

- **replace:** an entire typed resource is replaced by the highest-precedence valid
  provider;
- **merge:** fields are merged according to a schema-defined policy; and
- **extend:** entries are appended to an explicitly ordered collection.

A mod must declare the mode when ambiguity is possible. Arrays should not silently
merge by position. For keyed arrays, the key and duplicate policy belong to the
schema. A mod may explicitly delete a vanilla entry only through a validated tombstone
and only where the game supports absence.

## Data and asset formats

### Structured data

Species, moves, items, trainers, wild encounters, learnsets, abilities, and map/event
records should use versioned structured files with symbolic IDs. JSON is suitable for
early authoring; a binary cache can be generated after validation. The runtime format
must use fixed-width fields and IDs, not native pointers.

Examples of schema-level keys:

- `species/<id>.toml` or a keyed species collection for base stats, types, abilities,
  growth, cries, learnsets, evolution, names, and graphics references;
- `moves/<id>.toml` for power, accuracy, type, effect ID, flags, animation ID,
  secondary effect, and script references;
- `items/<id>.toml` for pocket, price, field/battle effect ID, descriptions, and
  graphics/audio references;
- `trainers/<id>.toml` for class, party policy, AI flags, items, and species/move IDs;
- `maps/<id>/map.toml` for layout, connections, object events, warps, scripts, and
  encounter references.

The initial supported subset should overlay existing IDs. Adding new species/moves/
items needs an explicit ID allocation policy, save representation, network/link
compatibility policy, and bounds changes; it should not be hidden inside a replace
operation.

### Graphics

Graphics files should declare format, dimensions, palette policy, compression, and
the virtual asset ID they satisfy. The loader should validate size, alignment,
palette count, tile dimensions, decompression bounds, and references before creating
runtime buffers. The first implementation can decode supported GBA formats at startup;
the eventual cache can store validated host-ready data.

### Audio

Audio assets should declare sample rate, channels, sample format, loop points, and
whether they are a song, direct sound sample, cry, or voice resource. Song event
streams need a fixed serialized operand format. Runtime audio objects may contain host
pointers, but the mod files must contain offsets/IDs only. A mod should be able to
replace a song/sample by virtual ID without supplying a compiled `gSongTable` object.

### Maps

Maps should identify layouts, tilesets, connections, object events, warps, trainers,
encounters, and scripts by stable IDs. A map override should declare whether it
replaces the whole map, merges keyed event records, or extends encounters. Map graph
validation must check target existence, coordinates, dimensions, collision data,
connection reciprocity, and script references before activation.

## Scripts and event overrides

The first script integration should preserve the existing field, battle, contest, and
AI virtual machines. External scripts should compile to a documented intermediate
representation or bytecode whose operands are fixed-width values and symbolic IDs.
Branch targets should be offsets or local labels, not native addresses. Calls to game
functions should use a versioned command ID table, not a C symbol address.

Supported progression:

1. Replace or extend named vanilla script resources by ID.
2. Add event records that point to external scripts.
3. Add declarative hooks such as `before`, `after`, and `replace` around approved
   script commands or event phases.
4. Add new VM commands only through a versioned command registry.
5. Consider native callbacks only for an explicitly granted native extension.

Conflicting script overrides need a declared policy. Exact replacement conflicts are
fatal unless a load-order winner is explicitly allowed. `before`/`after` composition
must have deterministic order and a defined failure behavior. Script compilation and
reference binding must happen during preflight; a malformed script must not crash the
runtime or leave half a map loaded.

The current clean seams are `src/scrcmd.c`, battle script command tables and consumers,
`battle_ai_script_commands.c`, field-effect script consumers, and map lookup around
`gMapGroups`. The current assembly scripts contain four-byte pointer-like operands,
so they must be translated or hydrated rather than directly loaded as 64-bit pointers.

## Load order, conflicts, and failure handling

The loader should be transactional:

- Parse and hash files without mutating game tables.
- Resolve all manifests/dependencies/conflicts.
- Build a candidate registry over the vanilla provider.
- Validate schemas, references, decoded assets, maps, and scripts.
- Emit a report and only then publish an immutable registry snapshot.

Failure policy:

- Invalid manifest: disable that mod.
- Missing required dependency or dependency cycle: disable the affected dependency
  closure and report the graph.
- Declared conflict: disable the deterministic loser or require explicit user choice.
- Invalid individual optional asset: use vanilla for that asset and warn.
- Invalid required replacement or invalid map/script: disable the mod if its content
  can be isolated; otherwise abort activation of the candidate snapshot and use the
  last-known-good/vanilla snapshot.
- Base content failure: abort rather than pretending to load a partial game.
- Runtime lookup miss: return the vanilla provider or a safe empty result according
  to the resource schema; never dereference a null native pointer.

Diagnostics should identify mod ID/version, path, schema, resource key, byte offset or
field, dependency chain, and the fallback result. A `--safe-mode` or equivalent should
start with all third-party mods disabled.

## Enable/disable state and profiles

Enablement belongs in a versioned PC profile, for example:

```text
user-data/
  profiles/default/config.toml
  profiles/default/mods.lock
  profiles/default/saves/pokeemerald.sav
  profiles/default/saves/pokeemerald.pcmeta
mods/
  ...
```

`mods.lock` can record exact selected versions and content hashes after dependency
resolution. A UI may change enabled state, but the loader remains authoritative and
must produce the same snapshot from the same lock/config state.

The base save remains the vanilla 128 KiB flash image. A PC metadata sidecar should
record:

- base game/content fingerprint;
- ordered active mod IDs, versions, and content hashes;
- mod save schema versions and migration status;
- whether the save contains mod-created IDs or state.

Vanilla saves should load with no mods. A modded save may load only if all referenced
content is available or a declared migration/placeholder policy succeeds. Removing a
mod that owns species, items, maps, or scripts should warn and offer a backup before
loading. The system must never serialize a native pointer, filesystem path, or load
order index into a save.

## Save compatibility and migrations

Each mod that adds persistent state declares a namespaced save schema and migration
chain. The schema should be keyed by mod ID, not by the mod's load order. Mod state
should use fixed-width, versioned records in the PC sidecar or a separately allocated
PC metadata store. Do not alter vanilla `SaveBlock1`, `SaveBlock2`, Pokémon storage,
or flash-sector offsets for ordinary mod support.

Migrations run in a transaction against a backup. A failed migration leaves the old
save untouched and starts in safe/read-only mode. A mod update that changes gameplay
data but has no persistent schema change may still invalidate competitive/replay
compatibility; the content fingerprint should make that visible.

## Eventual native/plugin extensions

Native plugins are an escape hatch for behavior that cannot be represented as data or
script. They should use a separately versioned host ABI:

- plugin entry points receive an API version and an opaque host context;
- handles identify species, maps, sprites, audio, tasks, and save namespaces;
- no public struct contains a host pointer whose layout is ABI-visible;
- callbacks are registered by capability and called on documented threads;
- allocation/free, logging, errors, and shutdown are host-owned;
- plugins declare required capabilities and minimum/maximum host API versions;
- plugin state is not a raw pointer in a save file.

An in-process native plugin is not safely sandboxed. It can read files, execute code,
modify memory, crash the process, or compromise the user account. Therefore:

- do not load native plugins by default;
- require explicit per-plugin user approval and show the requested capabilities;
- accept only trusted/allowlisted or signed packages in a distribution build;
- verify hashes/signatures and keep plugin binaries outside the content search path;
- reject a plugin built for a different host ABI or architecture;
- provide a no-native safe mode;
- never claim that an in-process plugin is secure merely because the manifest is valid.

Where strong isolation is required, run extensions in a separate process with an IPC
protocol or use a platform sandbox. The IPC protocol should also use opaque IDs and
fixed-width messages.

## Security and file handling

The loader must canonicalize every path and reject `..`, absolute paths, symlink escapes,
device files, excessive file sizes, decompression bombs, malformed archives, and
unbounded script/data counts. Hash the exact bytes used for the content snapshot.
Avoid executing anything from `data/`, `graphics/`, `audio/`, `maps/`, or `scripts/`.
Native code is the only executable content and needs the stricter policy above.

Loading should happen from a user-data root selected by the platform API. Do not use
the repository/current working directory as the default writable save or configuration
location. Errors must be logged without leaking secrets from arbitrary paths.

## Implementation stages

1. Define resource IDs, manifest schema, base-game fingerprint, and diagnostics; no
   runtime overrides yet.
2. Implement discovery, enable/disable state, dependency/conflict resolution, and a
   no-op vanilla registry. Prove deterministic ordering and safe failure behavior.
3. Add a virtual asset registry for one low-risk graphics family with vanilla fallback.
4. Add structured item/move/species/trainer overlays with schema validation and ID
   allocation rules.
5. Add map/encounter/graphics/audio providers and save metadata/fingerprints.
6. Add external script IR and event override composition; add replay fixtures.
7. Add optional native extensions only after the stable data/script API is versioned.

Every stage must support an empty/disabled mod set that selects the vanilla provider
and passes the same gameplay regression suite.
