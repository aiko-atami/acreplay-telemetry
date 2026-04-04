# Packages Overview

Сontains different layers of the replay pipeline.

## `acreplay-wasm`

- Package: `@aiko/acreplay-wasm`
- Responsibility: bridge from raw Assetto Corsa replay bytes to parsed metadata and per-lap telemetry packs.
- Input: replay file bytes (`ArrayBuffer`)
- Output:
  - replay/car/lap manifest
  - `lapPacks` as binary `Uint8Array` payloads
- Source of truth for:
  - wasm module loading
  - JS <-> wasm ABI boundary
  - worker integration

## `telemetry-pack`

- Package: `@aiko/telemetry-pack`
- Responsibility: read, validate, and analyze the binary lap-pack format produced by the parser.
- Input: lap-pack bytes (`ArrayBuffer` / `Uint8Array`)
- Output:
  - validated pack header/descriptors
  - typed channel arrays by channel id
  - analysis/chart helpers such as nearest-sample lookup, delta-time, and uPlot adapter data

## Mental Model

Pipeline: `raw replay file` -> `acreplay-wasm` -> `lapPack bytes` -> `telemetry-pack` -> `telemetry channels / delta / chart data`
