# Coding Conventions

**Analysis Date:** 2026-03-29

## Naming Patterns

**Files:**
- Use `snake_case` for C++ source and header filenames under `cpp/`, for example `cpp/acrp_core/src/api.cpp` and `cpp/acrp_core/include/acrp/channel_ids.hpp`.
- Use `camelCase` for TypeScript module filenames under `packages/`, for example `packages/telemetry-pack/src/readLapPack.ts` and `packages/telemetry-pack/src/validateLapPack.ts`.
- Use `index.ts` as the barrel/module entrypoint in package roots, as in `packages/telemetry-pack/src/index.ts` and `packages/acreplay-wasm/src/index.ts`.

**Functions:**
- Use `camelCase` for functions in both C++ and TypeScript, for example `readFileBytes()` in `cpp/tests/smoke.cpp`, `escapeJson()` in `cpp/acrp_wasm/exports.cpp`, `readLapPack()` in `packages/telemetry-pack/src/readLapPack.ts`, and `createReplayParser()` in `packages/acreplay-wasm/src/index.ts`.
- Prefix internal constants with `k` in C++, for example `kCsvExtension` in `cpp/acrp_core/src/api.cpp` and `kFlagHasCsp` in `cpp/tests/smoke.cpp`.

**Variables:**
- Use `camelCase` locals and parameters, for example `recordingIntervalUs` in `cpp/tests/smoke.cpp` and `outputLenPtr` in `packages/acreplay-wasm/src/index.ts`.
- Use `g_` prefixes for mutable namespace-scope globals in C++, as in `g_last_error` and `g_owned_buffers` in `cpp/acrp_wasm/exports.cpp`.

**Types:**
- Use `PascalCase` for structs, classes, and type aliases, for example `ParseError` in `cpp/acrp_core/include/acrp/status.hpp`, `ChannelDescriptorView` in `cpp/tests/smoke.cpp`, `ReplayManifestV1` in `packages/acreplay-wasm/src/types.ts`, and `LapTelemetryPackView` in `packages/telemetry-pack/src/types.ts`.
- Suffix wire-format and public contract types with version markers such as `V1`, as in `ReplayManifestV1`, `PackHeaderV1`, and `ChannelDescriptorV1`.

## Code Style

**Formatting:**
- C++ formatting is enforced with `clang-format` using `cpp/.clang-format`.
- Key C++ settings from `cpp/.clang-format`: `BasedOnStyle: Google`, `ColumnLimit: 120`, left-aligned pointers and references, case-sensitive include sorting, and `Standard: Latest`.
- TypeScript formatting tool configuration is not detected. Current files under `packages/` use 2-space indentation, semicolon-free style, trailing commas in some multiline constructs, and `.js` import specifiers for local ESM imports, but formatting is inconsistent between files such as `packages/telemetry-pack/src/uplotAdapter.ts` and `packages/acreplay-wasm/src/index.ts`.

**Linting:**
- No ESLint, Biome, or Prettier configuration is detected at the repo root.
- TypeScript quality gates come from `strict: true` compiler settings in `packages/telemetry-pack/tsconfig.json` and `packages/acreplay-wasm/tsconfig.json`.
- C++ warnings are configured in `cpp/cmake/CompilerWarnings.cmake` with `-Wall`, `-Wextra`, `-Wshadow`, `-Wpedantic`, `-Wconversion`, `-Wsign-conversion`, `-Wdouble-promotion`, and related warning flags.
- IDE-level C++ diagnostics are part of the workflow through `cpp/.clangd`, which enables strict unused/missing include diagnostics and strict clang-tidy fast checks.

## Import Organization

**Order:**
1. Standard library or external imports first, for example `import type {AlignedData} from 'uplot'` in `packages/telemetry-pack/src/uplotAdapter.ts` and `<filesystem>`/`<vector>` includes in `cpp/acrp_cli/main.cpp`.
2. Blank-line separation before local project imports when the file is kept tidy, for example `packages/telemetry-pack/src/uplotAdapter.ts`.
3. `import type` is used for type-only TS dependencies, and package barrels re-export public symbols from `index.ts` files such as `packages/telemetry-pack/src/index.ts`.

**Path Aliases:**
- Not detected. TypeScript code uses relative ESM imports with explicit `.js` suffixes, for example `./types.js` and `./validateLapPack.js` in `packages/telemetry-pack/src/readLapPack.ts`.

## Error Handling

**Patterns:**
- Throw typed `acrp::ParseError` for recoverable parser/CLI failures in the native stack, as in `cpp/acrp_cli/main.cpp` and `cpp/acrp_core/include/acrp/status.hpp`.
- Catch `acrp::ParseError` at the CLI boundary and convert it to stderr output plus non-zero exit, as in `cpp/acrp_cli/main.cpp`.
- Catch broad `std::exception` at the WASM C ABI boundary and convert failures into side-channel error state via `acrp_last_error_message()`, as in `cpp/acrp_wasm/exports.cpp`.
- Throw plain `Error` in TypeScript for invariant violations and malformed binary input, as in `packages/telemetry-pack/src/validateLapPack.ts`, `packages/telemetry-pack/src/readLapPack.ts`, and `packages/acreplay-wasm/src/index.ts`.
- Prefer explicit validation with descriptive messages before reading buffers or channel maps.

## Logging

**Framework:** `std::cout` / `std::cerr` in C++ and `console.log` in Node scripts

**Patterns:**
- Keep library/package code silent on success. Parsing helpers in `cpp/acrp_core/src/api.cpp`, `packages/telemetry-pack/src/*.ts`, and `packages/acreplay-wasm/src/*.ts` do not log.
- Emit human-readable status only in executable entrypoints and smoke scripts, for example `cpp/tests/smoke.cpp`, `cpp/tests/wasm_abi.cpp`, `cpp/tests/export_pack.cpp`, and `scripts/wasm-e2e-smoke.mjs`.

## Comments

**When to Comment:**
- Use concise comments to explain reverse-engineered binary layout, ownership boundaries, and non-obvious data-shape decisions.
- Good current patterns:
  ```cpp
  // Reverse-engineered layout of a version-16 `.acreplay` file...
  ```
  from `cpp/acrp_core/src/api.cpp`
  ```ts
  // The reader returns one typed array per channel id instead of a row-oriented structure...
  ```
  from `packages/telemetry-pack/src/readLapPack.ts`
  ```cpp
  // The exported C ABI returns ownership of the JSON buffer to JS/WASM callers.
  ```
  from `cpp/acrp_wasm/exports.cpp`
- Avoid decorative comments in straightforward control flow; many functions rely on naming and structure instead.

**JSDoc/TSDoc:**
- Not detected in `packages/`.
- Public C++ API headers such as `cpp/acrp_core/include/acrp/api.hpp` and `cpp/acrp_core/include/acrp/status.hpp` rely on declarations and names rather than docblocks.

## Function Design

**Size:** Keep helper functions small and single-purpose where possible, especially in tests and TypeScript utilities, for example `findNearestSampleByDistance()` in `packages/telemetry-pack/src/nearestSample.ts` and `readLastError()` in `cpp/tests/wasm_abi.cpp`. One exception is `cpp/acrp_core/src/api.cpp`, where parser implementation is still consolidated into a large translation unit.

**Parameters:** Prefer explicit typed parameters over option bags unless the boundary is public-facing. Examples include:
```cpp
int acrp_parse_car_json(const std::uint8_t* input_ptr, std::uint32_t input_len,
                        std::uint32_t car_index, std::uint8_t** output_ptr,
                        std::uint32_t* output_len);
```
from `cpp/acrp_wasm/exports.cpp`
```ts
export function buildDeltaTime(
    base: LapTelemetryPackView,
    comp: LapTelemetryPackView,
): Float32Array
```
from `packages/telemetry-pack/src/delta.ts`

**Return Values:**
- Return domain structs/objects rather than tuples, for example `ParsedCarResult` from `cpp/acrp_core/include/acrp/api.hpp`, `ReplayManifestV1` from `packages/acreplay-wasm/src/types.ts`, and `{ header, descriptors, channels }` from `packages/telemetry-pack/src/readLapPack.ts`.
- Use `void` plus thrown exceptions for validation helpers such as `packages/telemetry-pack/src/validateLapPack.ts`.

## Module Design

**Exports:** Keep package public surfaces narrow by re-exporting from a barrel file. `packages/telemetry-pack/src/index.ts` and `packages/acreplay-wasm/src/index.ts` are the intended public entrypoints.

**Barrel Files:** Use barrel files for TypeScript package surfaces. `packages/telemetry-pack/src/index.ts` re-exports all supported modules; `packages/acreplay-wasm/src/index.ts` exports both runtime functions and type/module helpers. No C++ barrel/header aggregation pattern is detected beyond public headers grouped under `cpp/acrp_core/include/acrp/`.

---

*Convention analysis: 2026-03-29*
