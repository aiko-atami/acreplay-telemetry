# Testing Patterns

**Analysis Date:** 2026-03-29

## Test Framework

**Runner:**
- Native C++ tests are built as standalone executables and run through CTest.
- Config: `cpp/tests/CMakeLists.txt`, `cpp/CMakePresets.json`, `mise.toml`

**Assertion Library:**
- C++ standard `assert()` from `<cassert>` is the primary assertion mechanism, as shown throughout `cpp/tests/smoke.cpp` and `cpp/tests/wasm_abi.cpp`.

**Run Commands:**
```bash
mise run test:cpp                    # Run all registered native C++ tests
ctest --preset native-debug --output-on-failure  # Direct CTest invocation
node scripts/wasm-e2e-smoke.mjs      # End-to-end wasm/package smoke check
```

## Test File Organization

**Location:**
- Native tests live in `cpp/tests/`.
- Test fixture data lives in `cpp/tests/fixtures/`.
- The wasm/package smoke validation lives outside the CTest suite in `scripts/wasm-e2e-smoke.mjs`.
- No `.test.ts`, `.spec.ts`, Vitest, or Jest test tree is detected under `packages/`.

**Naming:**
- Use scenario-style filenames for C++ test executables: `cpp/tests/smoke.cpp`, `cpp/tests/wasm_abi.cpp`, and `cpp/tests/export_pack.cpp`.
- Fixture files use descriptive stable names, for example `cpp/tests/fixtures/example.acreplay`.

**Structure:**
```text
cpp/tests/
├── smoke.cpp
├── wasm_abi.cpp
├── export_pack.cpp
└── fixtures/example.acreplay
scripts/
└── wasm-e2e-smoke.mjs
```

## Test Structure

**Suite Organization:**
```typescript
namespace {
  std::vector<std::byte> readFileBytes(const std::filesystem::path& path) { ... }
  void validatePack(const acrp::ParsedLapPack& lapPack, const acrp::LapManifestV1& lapManifest) { ... }
}

int main() {
  const auto replayBytes = readFileBytes(fixturePath);
  const auto manifest = acrp::inspectReplay(replayBytes);
  const auto parsedCar = acrp::parseCar(replayBytes, 0);
  assert(parsedCar.manifest.laps.size() == parsedCar.lapPacks.size());
}
```
- This pattern is taken from `cpp/tests/smoke.cpp`: helper functions in an anonymous namespace, followed by a linear `main()` that exercises the scenario end to end.

**Patterns:**
- Setup pattern: load the canonical binary fixture from `cpp/tests/fixtures/example.acreplay` using a local `readFileBytes()` helper, as in `cpp/tests/smoke.cpp` and `cpp/tests/wasm_abi.cpp`.
- Teardown pattern: rely on RAII and process exit. There are no per-test fixtures, hooks, or framework-managed cleanup callbacks.
- Assertion pattern: use dense `assert(...)` sequences for invariants, value parity, and range checks. When a helper needs richer failure semantics before an assertion, it throws `std::runtime_error`.

## Mocking

**Framework:** None

**Patterns:**
```typescript
const nativeManifest = acrp::inspectReplay(replayBytes);
const nativeCar = acrp::parseCar(replayBytes, 0);

const inspectStatus =
    acrp_inspect_json(reinterpret_cast<const std::uint8_t*>(replayBytes.data()),
                      static_cast<std::uint32_t>(replayBytes.size()), &inspectJsonPtr, &inspectJsonLen);
assert(inspectStatus == 0);
```
- From `cpp/tests/wasm_abi.cpp`: compare the exported wasm-facing ABI against the real native parser instead of mocking either boundary.

**What to Mock:**
- Not applicable in current tests. Current practice prefers real parser code, real fixture bytes, and the real wasm ABI bridge.

**What NOT to Mock:**
- Do not mock replay bytes, lap-pack bytes, or wasm ABI buffers when validating parser behavior. Existing tests intentionally exercise the real fixture file `cpp/tests/fixtures/example.acreplay`, real `acrp::parseCar()`, and real exported functions from `cpp/acrp_wasm/exports.cpp`.

## Fixtures and Factories

**Test Data:**
```typescript
const fixturePath = std::filesystem::path(__FILE__).parent_path() / "fixtures" / "example.acreplay";
const replayBytes = readFileBytes(fixturePath);
```
- This exact pattern is used in `cpp/tests/smoke.cpp` and `cpp/tests/wasm_abi.cpp`.

**Location:**
- Canonical replay fixture: `cpp/tests/fixtures/example.acreplay`
- No factory helpers or synthetic data builders are detected for C++ or TypeScript tests.

## Coverage

**Requirements:** None enforced. No coverage tool, threshold, or report generation config is detected in `package.json`, `mise.toml`, `cpp/CMakeLists.txt`, or package configs.

**View Coverage:**
```bash
Not detected
```

## Test Types

**Unit Tests:**
- Limited. The repo does not use a fine-grained unit-test framework. Small utility behavior is checked indirectly inside executable-style tests such as `cpp/tests/smoke.cpp` and `cpp/tests/wasm_abi.cpp`.

**Integration Tests:**
- Primary testing style. `cpp/tests/smoke.cpp` parses a real `.acreplay` file, inspects manifest fields, validates binary lap-pack layout, and checks channel-level invariants.
- `cpp/tests/wasm_abi.cpp` verifies parity between native parser results and the exported C ABI in `cpp/acrp_wasm/exports.cpp`.

**E2E Tests:**
- `scripts/wasm-e2e-smoke.mjs` is the end-to-end smoke path for the new stack. It loads the generated wasm module from `cpp/build-wasm/acrp_wasm/acrp_wasm.js`, uses the built JS wrapper from `packages/acreplay-wasm/dist/index.js`, decodes the produced lap pack with `packages/telemetry-pack/dist/index.js`, and checks cross-layer consistency.

## Common Patterns

**Async Testing:**
```typescript
const parser = await createReplayParser(
  createEmscriptenModuleFactory(createAcrpModule, { locateFile(...) { ... } }),
)
const manifest = await parser.inspectReplay(fileBytes.buffer.slice(...))
const parsedCar = await parser.parseCar(fileBytes.buffer.slice(...), 0)
```
- This pattern from `scripts/wasm-e2e-smoke.mjs` is the only async test flow detected.

**Error Testing:**
```typescript
const invalidStatus = acrp_inspect_json(invalidPtr, 4, &outputPtr, &outputLen);
assert(invalidStatus != 0);
assert(!readLastError().empty());

const invalidCarStatus =
    acrp_parse_car_json(reinterpret_cast<const std::uint8_t*>(replayBytes.data()),
                        static_cast<std::uint32_t>(replayBytes.size()), 999, &outputPtr, &outputLen);
assert(invalidCarStatus != 0);
assert(readLastError().find("Car index is out of range") != std::string::npos);
```
- From `cpp/tests/wasm_abi.cpp`: validate both non-zero status codes and exact/partial error messages at boundary layers.

---

*Testing analysis: 2026-03-29*
