# acreplay-parser

## What this repository is

This repo parses Assetto Corsa `.acreplay` files.

It currently has two parallel product surfaces:

- legacy replay-to-CSV flow for the Blender addon
- new telemetry flow: native core -> wasm -> TypeScript lap-pack reader

## Directory map

- `cpp/acrp_core/`
  Main shared implementation. Public API is in `include/acrp/`, most logic currently lives in `src/api.cpp`.
- `cpp/acrp_cli/`
  Thin native CLI for CSV export.
- `cpp/acrp_wasm/`
  Emscripten-facing C ABI over `acrp_core`.
- `cpp/tests/`
  Real fixture-based tests using `cpp/tests/fixtures/example.acreplay`.
- `packages/acreplay-wasm/`
  TS wrapper over the Emscripten module.
- `packages/telemetry-pack/`
  TS reader/validator/utilities for `LapTelemetryPackV1`.
- `Replay Blender Importer/`
  Blender addon that still consumes CSV output.

## Core concepts

- `inspectReplay(std::span<const std::byte>)`
  Parses the replay and currently returns the manifest for car `0`.
- `parseCar(std::span<const std::byte>, uint32_t carIndex)`
  Parses one car and returns:
  - `ReplayManifestV1`
  - one `LapTelemetryPackV1` blob per lap
- `LapTelemetryPackV1`
  Binary telemetry format with:
  - fixed header
  - channel descriptor table
  - packed channel payloads

## Reality of the implementation

- The migration plan expected many small core source files.
- In the current repo most new core logic is still consolidated in `cpp/acrp_core/src/api.cpp`.
- Tests are real and fixture-based; do not assume behavior without checking them.
- `example.acreplay` is the canonical local sample for validation.

## Build commands

Preferred bootstrap:

```bash
mise trust
mise install
```

Native:

```bash
mise run configure:cpp
mise run build:cpp
mise run test:cpp
```

Wasm:

```bash
mise run build:wasm
```

TypeScript packages:

```bash
mise run build:ts
```

End-to-end smoke:

```bash
mise run smoke:wasm
```

Equivalent direct commands still exist in `docs/build-and-test.md`.

## Working rules for future agents

- Prefer editing the new stack under `cpp/` and `packages/`.
- Keep `Replay Blender Importer/` stable unless the task explicitly targets it.
- When the task mentions C++ warnings/errors in an editor or IDE, treat `clangd` diagnostics as a first-class check. Do not stop after `clang-tidy` or a successful build.
- When changing binary pack layout, update:
  - C++ writer
  - TS reader/validator
  - fixture tests
  - wasm smoke flow if needed
- Before changing wasm ABI, inspect both:
  - `cpp/acrp_wasm/exports.cpp`
  - `packages/acreplay-wasm/src/index.ts`
- Before changing channel semantics, inspect both:
  - `cpp/acrp_core/include/acrp/channel_ids.hpp`
  - `packages/telemetry-pack/src/channelIds.ts`

## C++ verification workflow

Use this default order for work under `cpp/` unless the task explicitly says otherwise:

1. Formatting:

```bash
clang-format --dry-run -Werror $(rg --files cpp -g '*.[ch]' -g '*.[ch]pp' -g '*.cc' -g '*.hh' -g '*.hpp')
```

2. Native build and tests:

```bash
mise run configure:cpp
mise run build:cpp
mise run test:cpp
```

3. Full-project `clang-tidy` build:

```bash
mise run lint:cpp:tidy
```

4. File-level IDE/`clangd` verification for touched files or files reported by the user:

```bash
clangd --check=cpp/acrp_core/src/api.cpp --compile-commands-dir=cpp/build-native --clang-tidy
```

Notes:

- `mise.toml` currently manages shared tooling such as `cmake`, `ninja`, and `clang-tools`, and provides default C++ build env like `CC`, `CXX`, `CMAKE_GENERATOR`, and `CMAKE_EXPORT_COMPILE_COMMANDS`.
- If the compiler/toolchain changed, rerun the relevant configure step with `--fresh` so CMake does not keep stale compiler or `ninja` paths in cache.
- If `clangd` and `clang-tidy` disagree, the source of truth for IDE warnings is `clangd` with this repo's `cpp/.clangd` settings.
- If `ctest` fails in an agent/sandbox environment with `LeakSanitizer has encountered a fatal error` or `LeakSanitizer does not work under ptrace`, retry with `LSAN_OPTIONS=detect_leaks=0` and report clearly that this is an environment limitation, not necessarily a project test failure.
- For a single-file investigation, it is often useful to run both:

```bash
clang-tidy -p cpp/build-native cpp/acrp_core/src/api.cpp
clangd --check=cpp/acrp_core/src/api.cpp --compile-commands-dir=cpp/build-native --clang-tidy
```

## Known caveats

- `inspectReplay()` is per-car, not replay-wide.
- CSP support currently contributes `clutch_raw` into pack v1 when `EXT_PERCAR` data is available.
- Some environments need `EM_CACHE=/tmp/emscripten-cache` for wasm builds, though normal WSL shells usually do not.
