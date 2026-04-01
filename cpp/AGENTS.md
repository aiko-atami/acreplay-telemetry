# C++ Rules

Run repo root via `mise`.
Required verification order:
- `lint:cpp:format`
- `configure:cpp`
- `lint:cpp:clangd`
- `build:cpp`
- `lint:cpp:compile`
- `test:cpp`
- `lint:cpp:tidy`

Invariants:
- Most parser, lap segmentation, manifest building, and pack writing still live in `cpp/acrp_core/src/api.cpp`. Do not assume the behavior is split into smaller translation units.
- The canonical real replay fixture is `cpp/tests/fixtures/example.acreplay`. Trust fixture-backed tests over prose docs when they disagree.
- `inspectReplay(...)` is intentionally per-car and currently returns `buildManifest(..., 0)`. Do not turn it into a replay-wide summary unless explicitly asked.
- `parseCar(..., carIndex)` must keep returning one `ReplayManifestV1` plus one lap pack per `manifest.laps[]` entry for that same car. `manifest.carIndex` must equal the requested `carIndex`.
- Lap boundaries are inferred from changes in `CarFrame.currentLap` (zero-based on disk, exposed as `lapNumber = currentLap + 1`). The final segment is intentionally emitted with `isComplete = false`.
- `ReplayManifestV1.recordingIntervalMs` is rounded from the replay header's `double recordingInterval`. `PackHeaderV1.recordingIntervalUs` stores that rounded value multiplied by `1000`.
- Pack v1 normalizes gear to signed values by writing `frame.gear - 1` into channel `17 gear`. Do not copy the raw on-disk gear encoding into the pack by accident.
- `distance_m` in pack v1 is cumulative lap distance computed from X/Z deltas only. Treat changes here as a telemetry-schema change, not a refactor.

Pack and channel contracts:
- `LapTelemetryPackV1` is fixed at magic `ACTL`, version `1`, `headerBytes = 36`, `descriptorBytes = 16`. `ChannelDescriptorV1::byteOffset` is relative to `dataOffset`.
- `cpp/tests/smoke.cpp` is the executable contract for required channels, flags, monotonic `distance_m` / `time_ms`, and optional CSP channel `64 clutch_raw`.
- `EXT_PERCAR` only has trusted fixed record sizes for v6/v7. Pack v1 currently surfaces only `clutch_raw`; the rest of CSP extra fields are intentionally left unresolved.
- Channel id changes must stay in lockstep between `cpp/acrp_core/include/acrp/channel_ids.hpp` and `packages/telemetry-pack/src/channelIds.ts`.
- Pack layout changes must be mirrored in `packages/telemetry-pack/src/readLapPack.ts` and `packages/telemetry-pack/src/validateLapPack.ts`.

Wasm and ABI contracts:
- Before changing the wasm ABI, inspect `cpp/acrp_wasm/exports.cpp`, `packages/acreplay-wasm/src/module.ts`, and `packages/acreplay-wasm/src/index.ts`.
- `cpp/tests/wasm_abi.cpp` compiles `cpp/acrp_wasm/exports.cpp` natively with `ACRP_WASM_NO_MAIN`; keep non-Emscripten fallbacks working when editing exports.
- The wasm boundary is intentionally narrow: JSON for manifest/result structs, base64 for lap-pack bytes, and buffer ownership released through `acrp_free_buffer()`.
- `cpp/tests/wasm_abi.cpp` asserts the out-of-range error text `Car index is out of range`. If you change that message, update the test and any consumer assumptions in the same change.
- If a change touches the wasm ABI or JSON shape, also run `build:wasm`, `build:ts`, and `smoke:wasm`.
- If a wasm build hits Emscripten cache issues in this repo, retry with `EM_CACHE=/tmp/emscripten-cache`.

CSV export contract:
- `acrp_cli` and `acrp::exportCsvFiles(...)` define the supported CSV export surface. Do not change CSV behavior as an incidental side effect of telemetry-pack work.
