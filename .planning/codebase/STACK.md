# Technology Stack

**Analysis Date:** 2026-03-29

## Languages

**Primary:**
- C++23 - Native parser core, CLI wrapper, wasm export layer, and fixture-based tests under `cpp/CMakeLists.txt`, `cpp/acrp_core/src/api.cpp`, `cpp/acrp_cli/main.cpp`, `cpp/acrp_wasm/exports.cpp`, and `cpp/tests/*.cpp`
- TypeScript - ESM wrappers and binary telemetry readers under `packages/acreplay-wasm/src/*.ts` and `packages/telemetry-pack/src/*.ts`

**Secondary:**
- JavaScript (ESM) - Root workspace scripts in `package.json` and smoke automation in `scripts/wasm-e2e-smoke.mjs`
- CMake - Native and wasm build orchestration in `cpp/CMakeLists.txt`, `cpp/CMakePresets.json`, and `cpp/cmake/*.cmake`
- TOML - Toolchain and task management in `mise.toml`
- Markdown - Project documentation in `docs/*.md` and `apps/README.md`

## Runtime

**Environment:**
- Node.js 24 - Declared in `mise.toml`; used for workspace scripts and the wasm smoke test in `scripts/wasm-e2e-smoke.mjs`
- Browser/Worker-compatible JS runtime - `packages/acreplay-wasm/src/index.ts` uses `atob` and `TextDecoder`; `packages/acreplay-wasm/src/worker.ts` targets Web Workers
- Native C++ runtime on host OS - Required for `acrp_cli` and native tests in `cpp/acrp_cli/CMakeLists.txt` and `cpp/tests/CMakeLists.txt`
- Emscripten runtime - Required for `cpp/acrp_wasm/CMakeLists.txt`; configured in `cpp/cmake/EmscriptenSettings.cmake`

**Package Manager:**
- pnpm 10.17.1 - Declared in `package.json`
- Lockfile: Missing in project root during this analysis

## Frameworks

**Core:**
- CMake 3.28+ - Build system for the C++ workspace in `cpp/CMakeLists.txt` and `cpp/CMakePresets.json`
- Emscripten - wasm toolchain for `cpp/acrp_wasm/CMakeLists.txt` and `cpp/cmake/EmscriptenSettings.cmake`
- TypeScript compiler - Package-local builds configured by `packages/telemetry-pack/tsconfig.json` and `packages/acreplay-wasm/tsconfig.json`

**Testing:**
- CTest - Native test runner invoked from root `package.json`, `mise.toml`, and configured in `cpp/CMakePresets.json`
- Custom fixture-based test binaries - Implemented in `cpp/tests/smoke.cpp`, `cpp/tests/wasm_abi.cpp`, and `cpp/tests/export_pack.cpp`

**Build/Dev:**
- mise - Shared toolchain manager and task runner in `mise.toml`
- Ninja 1.12 - Generator/runtime declared in `mise.toml`
- clang / clang++ - Default native compiler env in `mise.toml`
- clang-format / clang-tidy / clangd - C++ quality tooling documented in `mise.toml`, `cpp/.clang-format`, and `cpp/.clangd`

## Key Dependencies

**Critical:**
- ZLIB - Required native dependency linked in `cpp/acrp_core/CMakeLists.txt`; Emscripten builds enable `-sUSE_ZLIB=1`
- Node built-ins `node:fs/promises` and `node:path` - Used by `scripts/wasm-e2e-smoke.mjs` for local fixture and wasm file loading
- Emscripten exported runtime methods `HEAPU8`, `getValue`, and `UTF8ToString` - Required by `packages/acreplay-wasm/src/index.ts` and exported in `cpp/cmake/EmscriptenSettings.cmake`

**Infrastructure:**
- `@aiko/acreplay-wasm` - Internal TS package exposing the wasm parser wrapper from `packages/acreplay-wasm/package.json`
- `@aiko/telemetry-pack` - Internal TS package exposing lap-pack readers and validators from `packages/telemetry-pack/package.json`
- `uplot` type surface - Referenced only as a type in `packages/telemetry-pack/src/uplotAdapter.ts`; local shim lives in `packages/telemetry-pack/src/uplot.d.ts`, so no manifest dependency is declared

## Configuration

**Environment:**
- Shared developer toolchain is configured in `mise.toml`
- Build env vars set in `mise.toml`: `CC`, `CXX`, `CMAKE_GENERATOR`, `CMAKE_EXPORT_COMPILE_COMMANDS`
- wasm builds require an active `emsdk` / `emcmake` toolchain in `PATH` per `docs/build-and-test.md`
- No `.env`, `.env.*`, or `*.env` files were detected at repo root or one level below during this analysis
- Optional build env override: `EM_CACHE=/tmp/emscripten-cache` is documented in `docs/build-and-test.md` for restricted environments

**Build:**
- Root scripts live in `package.json`
- Native/warnings/tidy presets live in `cpp/CMakePresets.json`
- Emscripten flags live in `cpp/cmake/EmscriptenSettings.cmake`
- Compiler warning and sanitizer modules live in `cpp/cmake/CompilerWarnings.cmake` and `cpp/cmake/Sanitizers.cmake`
- TypeScript emit targets `./dist` per `packages/telemetry-pack/tsconfig.json` and `packages/acreplay-wasm/tsconfig.json`

## Platform Requirements

**Development:**
- Host system C++ toolchain plus zlib, documented in `docs/build-and-test.md`
- Node.js, pnpm, Python, CMake, Ninja, and clang tools provisioned through `mise.toml`
- `emsdk`, `emcc`, and `emcmake` required for wasm builds per `docs/build-and-test.md`

**Production:**
- Native CLI target: local executable `acrp_cli` built from `cpp/acrp_cli/main.cpp`
- wasm distribution target: generated `cpp/build-wasm/acrp_wasm/acrp_wasm.js` and `cpp/build-wasm/acrp_wasm/acrp_wasm.wasm`, then consumed by `packages/acreplay-wasm/src/module.ts`
- No hosted deployment target is configured in repo files; `apps/` is reserved for future UI frontends per `apps/README.md`

---

*Stack analysis: 2026-03-29*
