# Architecture

**Analysis Date:** 2026-03-29

## Pattern Overview

**Overall:** Shared-core, multi-surface pipeline

**Key Characteristics:**
- Keep replay parsing and telemetry serialization in one native library: `cpp/acrp_core/`.
- Expose the same core behavior through thin delivery layers: `cpp/acrp_cli/`, `cpp/acrp_wasm/`, and `packages/acreplay-wasm/`.
- Treat binary lap packs as the stable interchange format between native parsing and TypeScript consumers in `packages/telemetry-pack/`.

## Layers

**Build and workspace orchestration:**
- Purpose: Define the repository-level native and TypeScript workspaces and wire targets together.
- Location: `cpp/CMakeLists.txt`, `cpp/CMakePresets.json`, `package.json`, `pnpm-workspace.yaml`, `mise.toml`
- Contains: CMake target graph, presets, pnpm workspace packages, dev tasks.
- Depends on: host compiler toolchain, Emscripten for wasm builds, pnpm for TS builds.
- Used by: all native targets under `cpp/`, all TS packages under `packages/`, smoke flow in `scripts/wasm-e2e-smoke.mjs`.

**Native core parsing and serialization:**
- Purpose: Parse `.acreplay` bytes, segment laps, build replay manifests, write telemetry packs, and preserve CSV export.
- Location: `cpp/acrp_core/src/api.cpp`, public headers in `cpp/acrp_core/include/acrp/`
- Contains: reverse-engineered replay parser, lap segmentation, pack writer, SHA-256 helper, CSV export path.
- Depends on: `zlib`, C++23 standard library, public data structures in `cpp/acrp_core/include/acrp/replay_types.hpp`, `cpp/acrp_core/include/acrp/manifest.hpp`, and `cpp/acrp_core/include/acrp/pack_format.hpp`.
- Used by: `cpp/acrp_cli/main.cpp`, `cpp/acrp_wasm/exports.cpp`, `cpp/tests/smoke.cpp`, `cpp/tests/wasm_abi.cpp`, `cpp/tests/export_pack.cpp`.

**Native CLI surface:**
- Purpose: Preserve the legacy replay-to-CSV flow for local users and the Blender addon.
- Location: `cpp/acrp_cli/main.cpp`
- Contains: argument parsing, file reading, `acrp::exportCsvFiles(...)` invocation, terminal error reporting.
- Depends on: `cpp/acrp_core/include/acrp/csv_export.hpp`, `cpp/acrp_core/include/acrp/status.hpp`.
- Used by: end users running the native executable built from `cpp/acrp_cli/CMakeLists.txt`.

**Wasm ABI boundary:**
- Purpose: Convert native core results into a JS-friendly exported C ABI.
- Location: `cpp/acrp_wasm/exports.cpp`
- Contains: exported alloc/free functions, last-error accessors, JSON serialization, base64 encoding of lap-pack bytes.
- Depends on: `acrp_core`, Emscripten when building wasm.
- Used by: `packages/acreplay-wasm/src/index.ts` and `cpp/tests/wasm_abi.cpp`.

**TypeScript wasm facade:**
- Purpose: Instantiate the generated Emscripten module, manage linear-memory I/O, and expose promise-based parser methods.
- Location: `packages/acreplay-wasm/src/index.ts`, `packages/acreplay-wasm/src/module.ts`, `packages/acreplay-wasm/src/types.ts`, `packages/acreplay-wasm/src/worker.ts`
- Contains: wasm module factory helpers, parser wrapper, result normalization, optional worker message handler.
- Depends on: generated Emscripten JS/WASM output from `cpp/build-wasm/acrp_wasm/`, JSON ABI from `cpp/acrp_wasm/exports.cpp`.
- Used by: `scripts/wasm-e2e-smoke.mjs` and any future app under `apps/`.

**TypeScript binary-pack reader layer:**
- Purpose: Validate `LapTelemetryPackV1`, decode descriptors and typed arrays, and provide telemetry utilities for consumers.
- Location: `packages/telemetry-pack/src/`
- Contains: `readLapPack`, `validateLapPack`, channel ids, delta helpers, nearest-sample lookup, chart adapter.
- Depends on: pack layout defined in `cpp/acrp_core/include/acrp/pack_format.hpp` and channel ids mirrored from `cpp/acrp_core/include/acrp/channel_ids.hpp`.
- Used by: `scripts/wasm-e2e-smoke.mjs` and future UI code in `apps/`.

**Verification layer:**
- Purpose: Exercise the parser from native-core and wasm-ABI perspectives using real replay fixtures.
- Location: `cpp/tests/`
- Contains: smoke tests against `cpp/tests/fixtures/example.acreplay`, ABI parity checks, helper exporter.
- Depends on: `acrp_core`, `cpp/acrp_wasm/exports.cpp`, fixture replay file.
- Used by: `ctest` through `cpp/tests/CMakeLists.txt` and `mise` tasks in `mise.toml`.

## Data Flow

**Replay inspection flow:**

1. A caller loads `.acreplay` bytes in `cpp/acrp_cli/main.cpp`, `packages/acreplay-wasm/src/index.ts`, or `scripts/wasm-e2e-smoke.mjs`.
2. `acrp::inspectReplay(...)` in `cpp/acrp_core/src/api.cpp` calls `parseReplayData(...)` and then `buildManifest(...)` for one car.
3. The native caller receives `ReplayManifestV1` directly, or the wasm boundary serializes it to JSON in `cpp/acrp_wasm/exports.cpp`.
4. `packages/acreplay-wasm/src/index.ts` parses the JSON into the TypeScript `ReplayManifestV1` type from `packages/acreplay-wasm/src/types.ts`.

**Per-car telemetry extraction flow:**

1. A caller invokes `acrp::parseCar(...)` in `cpp/acrp_core/src/api.cpp`.
2. The core reparses the replay with `parseReplayData(...)`, selects one `ParsedCarData`, and segments frames with `segmentLaps(...)`.
3. `buildLapPack(...)` serializes each lap slice into one `LapTelemetryPackV1` byte blob and `buildManifest(...)` builds manifest metadata.
4. `cpp/acrp_wasm/exports.cpp` wraps the result as JSON with base64-encoded lap packs for wasm callers.
5. `packages/acreplay-wasm/src/index.ts` decodes base64 into `Uint8Array` and returns `ParsedCarResultV1`.
6. `packages/telemetry-pack/src/readLapPack.ts` validates and materializes the binary blob into a `LapTelemetryPackView`.

**Legacy CSV export flow:**

1. `cpp/acrp_cli/main.cpp` reads replay bytes from disk and builds `CsvExportOptions`.
2. `acrp::exportCsvFiles(...)` in `cpp/acrp_core/src/api.cpp` reparses the replay and iterates all matching cars.
3. `exportCarCsv(...)` writes one CSV per selected car for downstream use by `Replay Blender Importer/`.

**State Management:**
- The architecture is mostly stateless and call-driven. Parsed state lives in local structs such as `ParsedReplayData`, `ParsedCarData`, `LapSlice`, and `ParsedCarResult` inside `cpp/acrp_core/src/api.cpp`.
- The wasm layer keeps only boundary-scoped mutable globals in `cpp/acrp_wasm/exports.cpp`: `g_last_error` and `g_owned_buffers`.
- TypeScript packages use function-local state; there is no app-wide store or service container in the current repository.

## Key Abstractions

**ParsedReplayData:**
- Purpose: Internal in-memory representation of the replay-wide parse result.
- Examples: `cpp/acrp_core/src/api.cpp`
- Pattern: private aggregate struct inside the core implementation that groups `ReplayHeader`, optional CSP driver names, and parsed cars.

**ParsedCarData:**
- Purpose: Internal per-car container for car metadata, frame samples, and optional CSP extra frames.
- Examples: `cpp/acrp_core/src/api.cpp`
- Pattern: private aggregate struct used as the boundary between parsing, lap segmentation, pack building, and CSV export.

**LapSlice:**
- Purpose: Represent one inferred lap span over a car’s frame timeline.
- Examples: `cpp/acrp_core/src/api.cpp`
- Pattern: derived slice metadata computed by `segmentLaps(...)` and reused by both `buildManifest(...)` and `buildLapPack(...)`.

**ReplayManifestV1 and ParsedCarResult:**
- Purpose: Public native API results for metadata-only and metadata-plus-pack use cases.
- Examples: `cpp/acrp_core/include/acrp/manifest.hpp`, `cpp/acrp_core/include/acrp/api.hpp`
- Pattern: plain data transfer structs exposed from the native library.

**PackHeaderV1 and ChannelDescriptorV1:**
- Purpose: Define the binary telemetry-pack contract consumed outside the native parser.
- Examples: `cpp/acrp_core/include/acrp/pack_format.hpp`, `packages/telemetry-pack/src/types.ts`
- Pattern: mirrored schema across C++ and TypeScript; write in native code, validate/read in TypeScript.

**WasmReplayParser:**
- Purpose: Stable async TS API over the Emscripten module.
- Examples: `packages/acreplay-wasm/src/types.ts`, `packages/acreplay-wasm/src/index.ts`
- Pattern: facade object exposing `inspectReplay(...)` and `parseCar(...)` without leaking raw ABI details.

**LapTelemetryPackView:**
- Purpose: Consumer-facing decoded view of one binary telemetry pack.
- Examples: `packages/telemetry-pack/src/types.ts`, `packages/telemetry-pack/src/readLapPack.ts`
- Pattern: immutable-by-convention read model with a parsed header, descriptor array, and channel map of typed arrays.

## Entry Points

**Native workspace root:**
- Location: `cpp/CMakeLists.txt`
- Triggers: `cmake -S cpp ...`, `mise run configure:cpp`, `mise run build:cpp`
- Responsibilities: define the C++23 workspace, enable options, add `acrp_core`, `acrp_cli`, `acrp_wasm`, and `tests`.

**Native CLI executable:**
- Location: `cpp/acrp_cli/main.cpp`
- Triggers: running the built `acrp_cli` binary.
- Responsibilities: parse CLI flags, read replay files, call `acrp::exportCsvFiles(...)`, return process exit codes.

**Wasm exports binary:**
- Location: `cpp/acrp_wasm/exports.cpp`
- Triggers: calls from generated Emscripten JS glue into `_acrp_*` symbols.
- Responsibilities: marshal raw buffers, expose last-error state, serialize native results to JSON.

**TypeScript parser facade:**
- Location: `packages/acreplay-wasm/src/index.ts`
- Triggers: `createReplayParser(...)` calls from consumers.
- Responsibilities: load the module, allocate wasm buffers, call exported functions, parse JSON, decode base64 lap packs.

**Worker entry for background parsing:**
- Location: `packages/acreplay-wasm/src/worker.ts`
- Triggers: browser worker `message` events after `attachWorkerHandler(...)`.
- Responsibilities: initialize one parser instance and proxy `inspectReplay` / `parseCar` requests.

**Binary pack reader API:**
- Location: `packages/telemetry-pack/src/index.ts`
- Triggers: imports from consumers or smoke scripts.
- Responsibilities: re-export the canonical TS utilities for pack validation and decoding.

**Wasm end-to-end smoke script:**
- Location: `scripts/wasm-e2e-smoke.mjs`
- Triggers: `pnpm smoke:wasm`, `mise run smoke:wasm`
- Responsibilities: load the built wasm module, parse the fixture replay, read one lap pack, and assert cross-layer consistency.

## Error Handling

**Strategy:** Throw exceptions in native and TS layers, then convert native exceptions to error codes only at the wasm ABI boundary.

**Patterns:**
- Native validation failures throw `acrp::ParseError` or helper-generated exceptions in `cpp/acrp_core/src/api.cpp`, with the error type declared in `cpp/acrp_core/include/acrp/status.hpp`.
- The CLI catches `acrp::ParseError` in `cpp/acrp_cli/main.cpp` and prints the message to `stderr`.
- `cpp/acrp_wasm/exports.cpp` catches `std::exception`, stores the message in `g_last_error`, and returns non-zero status.
- `packages/acreplay-wasm/src/index.ts` checks the returned status and throws `Error` using `_acrp_last_error_message()`.
- `packages/telemetry-pack/src/validateLapPack.ts` throws synchronously on invalid binary layout before decoding channel arrays.

## Cross-Cutting Concerns

**Logging:** Minimal and surface-specific. The CLI prints status/output paths in `cpp/acrp_cli/main.cpp` and `cpp/acrp_core/src/api.cpp`; the wasm and TS library layers avoid logging and rely on returned data or thrown errors.

**Validation:** Parse-time validation is concentrated in `cpp/acrp_core/src/api.cpp`; ABI-level error checks are in `cpp/acrp_wasm/exports.cpp`; binary pack validation is explicit in `packages/telemetry-pack/src/validateLapPack.ts`.

**Authentication:** Not applicable. The current repository parses local files and does not define network-facing auth flows.

---

*Architecture analysis: 2026-03-29*
