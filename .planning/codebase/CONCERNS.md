# Codebase Concerns

**Analysis Date:** 2026-03-29

## Tech Debt

**Monolithic core parser and exporter implementation:**
- Issue: `cpp/acrp_core/src/api.cpp` concentrates replay parsing, CSP decoding, SHA-256, lap segmentation, pack building, and CSV export in one 1000+ line translation unit.
- Files: `cpp/acrp_core/src/api.cpp`, `cpp/acrp_core/include/acrp/api.hpp`, `cpp/acrp_core/include/acrp/replay_types.hpp`
- Impact: Small format changes create high regression risk because parsing, pack semantics, and legacy CSV behavior are coupled in one file. Reviews and targeted testing are harder because concerns are not isolated.
- Fix approach: Split `cpp/acrp_core/src/api.cpp` into format parsing, CSP extras, manifest construction, lap segmentation, pack encoding, and CSV export modules with fixture-backed tests per module.

**Reverse-engineered format knowledge is encoded as magic offsets and opaque structs:**
- Issue: The parser depends on hard-coded skipped blocks, unresolved frame gaps, and partially understood CSP payloads instead of a checked specification.
- Files: `cpp/acrp_core/src/api.cpp`, `cpp/acrp_core/include/acrp/replay_types.hpp`
- Impact: Support for new replay variants is fragile. A small upstream layout change can silently corrupt parsed data rather than fail cleanly.
- Fix approach: Move magic layout values into named constants/types, validate every skipped region against stream bounds, and grow the fixture set to cover more replay/CSP variants before extending the format.

**Cross-language contract is duplicated manually:**
- Issue: Channel ids, pack layout assumptions, and the wasm JSON/base64 contract are repeated across C++, TypeScript, and tests.
- Files: `cpp/acrp_core/include/acrp/channel_ids.hpp`, `cpp/acrp_core/src/api.cpp`, `cpp/acrp_wasm/exports.cpp`, `packages/telemetry-pack/src/channelIds.ts`, `packages/telemetry-pack/src/validateLapPack.ts`, `cpp/tests/smoke.cpp`, `cpp/tests/wasm_abi.cpp`
- Impact: Schema drift is likely when adding channels or changing pack fields because there is no generated/shared source of truth.
- Fix approach: Generate TS channel ids and pack metadata from the C++ source of truth, and replace string-fragile wasm ABI assertions with structured JSON parsing.

## Known Bugs

**`inspectReplay()` is not replay-wide and always reports car 0:**
- Symptoms: Callers cannot inspect replay-wide metadata or enumerate cars from the public manifest API. The returned manifest always describes car index `0`.
- Files: `cpp/acrp_core/src/api.cpp`, `cpp/acrp_core/include/acrp/manifest.hpp`
- Trigger: Any multi-car replay passed through `inspectReplay(std::span<const std::byte>)`.
- Workaround: Call `parseCar()` for each candidate car index and treat `inspectReplay()` as a shorthand for car `0`, not as a replay manifest.

**Lap-pack reader silently accepts unknown channel value types:**
- Symptoms: Any descriptor value type other than `1` or `2` is decoded as `Int8Array`, even for corrupted or future pack formats.
- Files: `packages/telemetry-pack/src/readLapPack.ts`, `packages/telemetry-pack/src/types.ts`
- Trigger: Malformed data or a future `LapTelemetryPack` version that introduces a new value type.
- Workaround: Validate descriptors with `packages/telemetry-pack/src/validateLapPack.ts` first and reject packs from unknown producers; there is no built-in strict guard yet.

## Security Considerations

**Untrusted replay metadata can drive large allocations:**
- Risk: The parser allocates directly from length-prefixed strings, compressed CSP sizes, and derived uncompressed frame counts without an upper bound.
- Files: `cpp/acrp_core/src/api.cpp`
- Current mitigation: Stream reads fail on EOF and unsupported replay version `!= 16` is rejected.
- Recommendations: Add maximum accepted string/blob/frame limits before allocation, validate `seekg()` targets against total input size, and reject CSP blobs whose declared sizes exceed replay length or a configured cap.

**Wasm ABI state is global and request-shared:**
- Risk: `g_last_error` and `g_owned_buffers` are mutable globals. Concurrent requests can overwrite the last error or confuse ownership accounting.
- Files: `cpp/acrp_wasm/exports.cpp`
- Current mitigation: Single-threaded usage through the current worker flow in `packages/acreplay-wasm/src/worker.ts` avoids most collisions.
- Recommendations: Return explicit error buffers per call, replace global owned-buffer tracking with opaque handles or caller-provided memory, and document thread-safety expectations in the wasm package.

## Performance Bottlenecks

**Common wasm flow parses and hashes the same replay twice:**
- Problem: `inspectReplay()` and `parseCar()` both call `parseReplayData()` and `computeSha256()`. The smoke flow demonstrates the duplicated work in sequence.
- Files: `cpp/acrp_core/src/api.cpp`, `scripts/wasm-e2e-smoke.mjs`
- Cause: The public API has no shared parsed replay/session object, so each call starts from raw bytes again.
- Improvement path: Introduce a parsed replay handle or cache keyed by replay bytes/sha so manifest inspection and per-car parsing can reuse the same decoded representation.

**Wasm transport pays JSON + base64 + copy overhead for every lap pack:**
- Problem: Native pack bytes are base64-encoded into JSON in C++, decoded back to bytes in TS, and copied again when creating typed arrays.
- Files: `cpp/acrp_wasm/exports.cpp`, `packages/acreplay-wasm/src/index.ts`, `packages/telemetry-pack/src/readLapPack.ts`
- Cause: The ABI favors a text contract rather than exposing binary buffers/descriptor tables directly.
- Improvement path: Export manifest metadata and pack bytes through binary views or structured wasm memory access, and avoid `ArrayBuffer.slice()` copies when building channel arrays.

**Delta-time lookup is quadratic in sample count:**
- Problem: `buildDeltaTime()` does a full linear scan of the base lap for every comparison sample.
- Files: `packages/telemetry-pack/src/delta.ts`, `packages/telemetry-pack/src/nearestSample.ts`
- Cause: `findNearestSampleByDistance()` is `O(n)` and is called once per compared sample.
- Improvement path: Use monotonic distance to implement a two-pointer walk or binary search, reducing large-lap delta generation to near-linear time.

## Fragile Areas

**CSP extra-data parsing relies on partial tag parsing and silent decompression failure:**
- Files: `cpp/acrp_core/src/api.cpp`
- Why fragile: The `while (true)` scan around `EXT_PERCAR` depends on tag formatting, ignores `std::from_chars` error codes, and simply drops extra data when `uncompress()` fails.
- Safe modification: Keep replay/CSP fixture coverage in sync with every change to the scan loop or `CarFrameExtraV6`/`CarFrameExtraV7` layout assumptions.
- Test coverage: Only `cpp/tests/fixtures/example.acreplay` is exercised by `cpp/tests/smoke.cpp` and `cpp/tests/wasm_abi.cpp`; there are no negative CSP fixtures.

**Lap segmentation semantics are heuristic rather than authoritative:**
- Files: `cpp/acrp_core/src/api.cpp`
- Why fragile: Completeness is inferred from `currentLap` transitions and `currentLapTime == 0`, and the last segment is always marked incomplete.
- Safe modification: Treat `segmentLaps()` as behavior-defining code and add fixture assertions for partial-start, partial-end, pit-out, and replay-resume edge cases before changing it.
- Test coverage: `cpp/tests/smoke.cpp` checks that the last lap is incomplete for the example fixture, but does not cover alternative lap-boundary patterns.

**Wasm ABI tests are string-shape dependent:**
- Files: `cpp/tests/wasm_abi.cpp`, `cpp/acrp_wasm/exports.cpp`
- Why fragile: The test extracts fields by string searching instead of parsing JSON structurally, so benign formatting/order changes can break the test without changing behavior.
- Safe modification: Change wasm JSON formatting and field naming only together with a structured parser update in the ABI test.
- Test coverage: Only native parity against the single example replay is covered.

## Scaling Limits

**Replay processing is fully materialized in memory:**
- Current capacity: One full replay is parsed into `ParsedReplayData`, all car frames are stored in vectors, then lap-pack channel payloads are built into additional vectors.
- Limit: Large replays or multi-car sessions will scale memory and CPU roughly with `numCars * numFrames`; there is no streaming path.
- Scaling path: Parse one car at a time, stream pack generation per lap, and reuse scratch buffers instead of materializing all channels simultaneously.

**Format support is intentionally narrow:**
- Current capacity: Version `16` replays only, with CSP `EXT_PERCAR` decoding limited to v6/v7 fixed-size records.
- Limit: Unsupported versions fail fast, and partially understood CSP variants silently degrade to base parsing.
- Scaling path: Add explicit version capability reporting, more fixtures per replay/CSP variant, and tests for graceful degradation when extras are unknown.

## Dependencies at Risk

**Not detected:**
- Risk: No third-party package in `package.json`, `packages/acreplay-wasm/package.json`, or `packages/telemetry-pack/package.json` stands out as deprecated or near-end-of-life from repository state alone.
- Impact: Not applicable.
- Migration plan: Continue to keep the repo-level toolchain in `mise.toml` current and re-check if new runtime dependencies are introduced.

## Missing Critical Features

**Replay-wide car discovery API is missing:**
- Problem: The public manifest type contains only one `carIndex`, one `driverName`, and one `laps` list, and `inspectReplay()` hardcodes car `0`.
- Blocks: Building replay browsers, car selection UIs, or replay-wide summaries without reparsing every car individually.

**Automated TypeScript tests are missing:**
- Problem: There are no `.test.*` or `.spec.*` files under `packages/`, and `package.json` only runs native C++ tests.
- Blocks: Safe refactoring of `packages/acreplay-wasm/` and `packages/telemetry-pack/`, especially around malformed inputs and wasm/browser behavior.

## Test Coverage Gaps

**Malformed and adversarial replay inputs are untested:**
- What's not tested: Oversized string lengths, truncated frame sections, invalid CSP tags, decompression failures, and oversized car/frame counts.
- Files: `cpp/acrp_core/src/api.cpp`, `cpp/tests/smoke.cpp`, `cpp/tests/wasm_abi.cpp`
- Risk: Parser hardening regressions or denial-of-service style bugs can ship unnoticed.
- Priority: High

**TypeScript reader and analysis helpers are untested:**
- What's not tested: `readLapPack()`, `validateLapPack()`, `buildDeltaTime()`, `findNearestSampleByDistance()`, `toUplotSeries()`, and wasm worker behavior.
- Files: `packages/telemetry-pack/src/readLapPack.ts`, `packages/telemetry-pack/src/validateLapPack.ts`, `packages/telemetry-pack/src/delta.ts`, `packages/telemetry-pack/src/nearestSample.ts`, `packages/telemetry-pack/src/uplotAdapter.ts`, `packages/acreplay-wasm/src/index.ts`, `packages/acreplay-wasm/src/worker.ts`
- Risk: Contract drift between native output and TS consumers can break downstream tooling without any automated signal.
- Priority: High

**Fixture diversity is too narrow:**
- What's not tested: Multi-car manifests beyond car `0`, non-CSP replays, CSP variants other than the example fixture, and different lap-boundary scenarios.
- Files: `cpp/tests/fixtures/example.acreplay`, `cpp/tests/smoke.cpp`, `cpp/tests/wasm_abi.cpp`
- Risk: Behavior that looks stable on the canonical sample can fail on real-world replay diversity.
- Priority: Medium

---

*Concerns audit: 2026-03-29*
