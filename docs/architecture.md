# Architecture

## C++ targets

The `cpp/CMakeLists.txt` workspace builds four logical targets:

- `acrp_core`
  Static library with the parser, lap segmentation, manifest builder, CSV export, and pack writer.
- `acrp_cli`
  Native executable over `acrp_core`.
- `acrp_wasm`
  Emscripten module over the same `acrp_core`.
- `cpp/tests/*`
  Native test binaries linked against `acrp_core`, with `wasm_abi.cpp` also linking `cpp/acrp_wasm/exports.cpp`.

## Core parsing pipeline

The central implementation sits in `cpp/acrp_core/src/api.cpp`.

1. `parseReplayData(...)`
   Reads the binary replay stream into `ParsedReplayData`.
   It parses:
   - replay header
   - per-car headers
   - per-frame `CarFrame` samples
   - optional CSP `EXT_PERCAR` payloads if present

2. `segmentLaps(...)`
   Splits one car's frame timeline into lap slices whenever `currentLap` changes.

3. `buildManifest(...)`
   Converts lap slices into `ReplayManifestV1`.

4. `buildLapPack(...)`
   Serializes one lap into `LapTelemetryPackV1`.

5. Public API
   - `inspectReplay(...)` parses the replay and builds a manifest for the first car
   - `parseCar(...)` parses one car and returns all per-lap blobs
   - `exportCsvFiles(...)` preserves the old CSV flow for the CLI and Blender importer

## WASM layer

`cpp/acrp_wasm/exports.cpp` is intentionally narrow:

- it does not reimplement parsing logic
- it only wraps the core API
- results are returned as JSON because that keeps the JS boundary simple
- lap packs are embedded as base64 strings inside that JSON

Exported functions:

- `acrp_alloc_buffer`
- `acrp_free_buffer`
- `acrp_last_error_message`
- `acrp_last_error_length`
- `acrp_inspect_json`
- `acrp_parse_car_json`

## TypeScript layer

### `@aiko/acreplay-wasm`

Responsibilities:

- instantiate the Emscripten module
- copy an input replay into wasm memory
- call the C ABI
- decode JSON
- decode lap-pack base64 into `Uint8Array`

### `@aiko/telemetry-pack`

Responsibilities:

- validate the binary `LapTelemetryPackV1` header/layout
- read channel descriptors and typed arrays
- expose helpers such as nearest-sample lookup, delta-time generation, and `uPlot` adaptation
