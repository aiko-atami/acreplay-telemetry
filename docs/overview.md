# Overview

## What the project does

The repository started as a replay-to-CSV converter plus a Blender addon. During the current migration it also gained a reusable telemetry pipeline:

- parse Assetto Corsa `.acreplay` files in native C++
- segment one car replay into lap-sized telemetry blobs
- expose the same parser through wasm
- read those telemetry blobs from TypeScript

## Main directories

- `cpp/`
  Native workspace root. It contains the CMake entrypoint, presets, helper modules, and build outputs.
- `cpp/acrp_core/`
  Shared parser implementation. It reads replay bytes, extracts per-car frames, optionally reads CSP extra data, builds `ReplayManifestV1`, and serializes `LapTelemetryPackV1`.
- `cpp/acrp_cli/`
  Native executable that preserves the old CSV-export workflow through `acrp::exportCsvFiles(...)`.
- `cpp/acrp_wasm/`
  Emscripten target exposing a C ABI that returns JSON and base64-encoded lap blobs.
- `cpp/tests/`
  Native smoke tests, wasm ABI parity tests, and a helper to export live lap-pack fixtures.
- `packages/acreplay-wasm/`
  TypeScript wrapper around the Emscripten module. It writes replay bytes into wasm memory and exposes `inspectReplay()` / `parseCar()`.
- `packages/telemetry-pack/`
  TypeScript reader/validator for `LapTelemetryPackV1`, plus helper utilities for charting and delta-time calculations.
- `apps/`
  Reserved for UI frontends consuming the TypeScript packages and wasm wrapper.
- `Replay Blender Importer/`
  Blender addon that still consumes CSV exported by the parser.

## Current data flow

```text
.acreplay
  -> acrp_core::parseReplayData(...)
  -> acrp_core::segmentLaps(...)
  -> acrp_core::buildLapPack(...)
  -> native CSV export / wasm JSON ABI
  -> @aiko/acreplay-wasm
  -> @aiko/telemetry-pack
```

## Important current constraints

- `inspectReplay(...)` currently returns the manifest for car index `0`. It is not yet a replay-level summary for all cars.
- The new code lives mostly in `cpp/acrp_core/src/api.cpp`. The plan expected more files, but the implementation is still consolidated in one translation unit.
- The canonical real fixture in this repo is `cpp/tests/fixtures/example.acreplay`.
