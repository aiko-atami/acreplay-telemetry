# Codebase Structure

**Analysis Date:** 2026-03-29

## Directory Layout

```text
acreplay-parser/
├── cpp/                      # Native C++ workspace, CMake entrypoint, tests, and generated build trees
├── packages/                 # TypeScript library packages for wasm access and telemetry-pack reading
├── apps/                     # Reserved app/front-end workspace under the pnpm workspace
├── scripts/                  # Repository scripts such as wasm end-to-end smoke validation
├── docs/                     # Human-written project docs and format notes
├── .planning/codebase/       # Generated codebase maps for future planning and execution
├── package.json              # Root TS scripts for builds and smoke flows
├── pnpm-workspace.yaml       # Workspace membership for `packages/*` and `apps/*`
└── mise.toml                 # Shared toolchain and task definitions
```

## Directory Purposes

**`cpp/`:**
- Purpose: Native workspace root for all parser, CLI, wasm, and test code.
- Contains: `CMakeLists.txt`, `CMakePresets.json`, `cmake/`, product targets, tests, generated `build-*` directories.
- Key files: `cpp/CMakeLists.txt`, `cpp/CMakePresets.json`

**`cpp/acrp_core/`:**
- Purpose: Shared implementation and public native API.
- Contains: public headers in `cpp/acrp_core/include/acrp/` and the current consolidated implementation in `cpp/acrp_core/src/api.cpp`.
- Key files: `cpp/acrp_core/src/api.cpp`, `cpp/acrp_core/include/acrp/api.hpp`, `cpp/acrp_core/include/acrp/manifest.hpp`, `cpp/acrp_core/include/acrp/pack_format.hpp`

**`cpp/acrp_cli/`:**
- Purpose: Native command-line surface for CSV export.
- Contains: target-specific `CMakeLists.txt` and one `main.cpp`.
- Key files: `cpp/acrp_cli/main.cpp`, `cpp/acrp_cli/CMakeLists.txt`

**`cpp/acrp_wasm/`:**
- Purpose: Emscripten-facing target that exposes the native parser through a C ABI.
- Contains: wasm target `CMakeLists.txt` and `exports.cpp`.
- Key files: `cpp/acrp_wasm/exports.cpp`, `cpp/acrp_wasm/CMakeLists.txt`

**`cpp/tests/`:**
- Purpose: Fixture-backed native verification.
- Contains: smoke test, wasm ABI parity test, pack export helper, canonical replay fixture.
- Key files: `cpp/tests/smoke.cpp`, `cpp/tests/wasm_abi.cpp`, `cpp/tests/export_pack.cpp`, `cpp/tests/fixtures/example.acreplay`

**`cpp/cmake/`:**
- Purpose: Shared CMake modules for warnings, sanitizers, and Emscripten settings.
- Contains: reusable `.cmake` modules applied by native targets.
- Key files: `cpp/cmake/CompilerWarnings.cmake`, `cpp/cmake/Sanitizers.cmake`, `cpp/cmake/EmscriptenSettings.cmake`

**`packages/acreplay-wasm/`:**
- Purpose: TypeScript package that wraps the Emscripten module.
- Contains: authored TS in `src/`, generated JS and declarations in `dist/`, package manifest, tsconfig.
- Key files: `packages/acreplay-wasm/src/index.ts`, `packages/acreplay-wasm/src/module.ts`, `packages/acreplay-wasm/src/worker.ts`, `packages/acreplay-wasm/package.json`

**`packages/telemetry-pack/`:**
- Purpose: TypeScript package that validates and reads `LapTelemetryPackV1`.
- Contains: pack schema/types, readers, utilities in `src/`, generated output in `dist/`.
- Key files: `packages/telemetry-pack/src/readLapPack.ts`, `packages/telemetry-pack/src/validateLapPack.ts`, `packages/telemetry-pack/src/channelIds.ts`, `packages/telemetry-pack/package.json`

**`apps/`:**
- Purpose: Workspace slot for UI frontends consuming the wasm and telemetry packages.
- Contains: `apps/README.md`; no app implementation is present.
- Key files: `apps/README.md`

**`scripts/`:**
- Purpose: Cross-layer automation and smoke verification.
- Contains: ESM scripts that exercise built artifacts.
- Key files: `scripts/wasm-e2e-smoke.mjs`

**`docs/`:**
- Purpose: Project documentation and current implementation notes.
- Contains: architecture, overview, build/test, and data-format docs.
- Key files: `docs/architecture.md`, `docs/overview.md`, `docs/build-and-test.md`, `docs/data-formats.md`

## Key File Locations

**Entry Points:**
- `cpp/CMakeLists.txt`: Native workspace entrypoint.
- `cpp/acrp_cli/main.cpp`: CLI process entrypoint.
- `cpp/acrp_wasm/exports.cpp`: Wasm ABI entrypoint.
- `packages/acreplay-wasm/src/index.ts`: Main TS parser facade.
- `packages/telemetry-pack/src/index.ts`: Main TS telemetry-pack barrel.
- `scripts/wasm-e2e-smoke.mjs`: End-to-end smoke runner.

**Configuration:**
- `mise.toml`: Tooling versions and repeatable repo tasks.
- `package.json`: Root JS scripts.
- `pnpm-workspace.yaml`: Workspace package registration.
- `cpp/CMakePresets.json`: Canonical native configure/build/test presets.
- `packages/acreplay-wasm/tsconfig.json`: TS config for the wasm package.
- `packages/telemetry-pack/tsconfig.json`: TS config for the telemetry-pack package.

**Core Logic:**
- `cpp/acrp_core/src/api.cpp`: Main parser, lap segmentation, manifest builder, pack writer, CSV export.
- `cpp/acrp_core/include/acrp/api.hpp`: Public native parsing API.
- `cpp/acrp_core/include/acrp/replay_types.hpp`: Replay stream structs and core domain types.
- `cpp/acrp_core/include/acrp/pack_format.hpp`: Binary telemetry-pack contract.
- `packages/acreplay-wasm/src/index.ts`: Wasm memory and JSON boundary handling.
- `packages/telemetry-pack/src/readLapPack.ts`: Binary pack decoding.

**Testing:**
- `cpp/tests/CMakeLists.txt`: Native test targets.
- `cpp/tests/smoke.cpp`: End-to-end native parser smoke test.
- `cpp/tests/wasm_abi.cpp`: Native-vs-wasm ABI parity test.
- `cpp/tests/export_pack.cpp`: Helper utility to export lap-pack fixtures.
- `cpp/tests/fixtures/example.acreplay`: Canonical local replay fixture.

## Naming Conventions

**Files:**
- C++ target directories use target names in snake_case: `cpp/acrp_core/`, `cpp/acrp_cli/`, `cpp/acrp_wasm/`.
- C++ public headers use snake_case nouns: `cpp/acrp_core/include/acrp/pack_format.hpp`, `cpp/acrp_core/include/acrp/replay_types.hpp`.
- TS feature modules use camelCase when the module name is descriptive: `packages/telemetry-pack/src/readLapPack.ts`, `packages/telemetry-pack/src/validateLapPack.ts`, `packages/telemetry-pack/src/channelIds.ts`.
- TS package entry and support modules use short conventional names when they are package-wide boundaries: `packages/acreplay-wasm/src/index.ts`, `packages/acreplay-wasm/src/module.ts`, `packages/acreplay-wasm/src/worker.ts`.
- Generated TS outputs mirror source filenames in `dist/`: `packages/telemetry-pack/dist/readLapPack.js`, `packages/acreplay-wasm/dist/worker.d.ts`.
- Documentation and planning artifacts use uppercase or descriptive kebab/sentence names depending on purpose: `.planning/codebase/ARCHITECTURE.md`, `docs/build-and-test.md`.

**Directories:**
- Product and package directories use snake_case in `cpp/` and kebab-case in `packages/`: `cpp/acrp_core/`, `packages/telemetry-pack/`.
- Public native headers live under a namespace-mirrored include root: `cpp/acrp_core/include/acrp/`.
- Authored TS stays in `src/`; compiled outputs stay in `dist/`.
- Generated native build trees follow `build-*` naming under `cpp/`: `cpp/build-native/`, `cpp/build-wasm/`, `cpp/build-clang-tidy/`.

## Where to Add New Code

**New native parser feature:**
- Primary code: `cpp/acrp_core/src/api.cpp` unless the task explicitly includes splitting the current monolith.
- Public API additions: `cpp/acrp_core/include/acrp/`
- Tests: `cpp/tests/`

**New native surface or executable:**
- Implementation: add a sibling target directory under `cpp/` such as `cpp/<target_name>/`.
- Build wiring: `cpp/CMakeLists.txt` plus a target-local `cpp/<target_name>/CMakeLists.txt`.

**New wasm ABI function:**
- Implementation: `cpp/acrp_wasm/exports.cpp`
- Matching TS wrapper changes: `packages/acreplay-wasm/src/index.ts`, `packages/acreplay-wasm/src/module.ts`, and `packages/acreplay-wasm/src/types.ts`

**New telemetry-pack field or channel:**
- Native writer: `cpp/acrp_core/src/api.cpp`
- Native schema: `cpp/acrp_core/include/acrp/pack_format.hpp` and `cpp/acrp_core/include/acrp/channel_ids.hpp`
- TS mirror: `packages/telemetry-pack/src/types.ts`, `packages/telemetry-pack/src/channelIds.ts`, `packages/telemetry-pack/src/readLapPack.ts`, `packages/telemetry-pack/src/validateLapPack.ts`
- Verification: `cpp/tests/`, plus `scripts/wasm-e2e-smoke.mjs` if cross-layer behavior changes

**New TypeScript consumer helper:**
- Wasm-facing helper: `packages/acreplay-wasm/src/`
- Pack-reading or analysis helper: `packages/telemetry-pack/src/`
- Public export wiring: the package’s `src/index.ts`

**New app or UI frontend:**
- Implementation: create a new workspace package under `apps/<app-name>/`
- Shared parsing dependencies: import from `packages/acreplay-wasm/` and `packages/telemetry-pack/`

**Utilities:**
- Shared native utilities: keep them in `cpp/acrp_core/src/api.cpp` or promote to `cpp/acrp_core/include/acrp/` only if they become public API.
- Shared JS/TS utilities: place them inside the package that owns the concern rather than in a repo-global utility folder; no shared root utility directory exists today.

## Special Directories

**`cpp/build-native/`:**
- Purpose: configured native debug build tree and compile commands output.
- Generated: Yes
- Committed: No

**`cpp/build-wasm/`:**
- Purpose: Emscripten build tree and generated `acrp_wasm.js` / `.wasm` artifacts used by smoke tests.
- Generated: Yes
- Committed: No

**`cpp/build-clang-tidy/`:**
- Purpose: separate build tree used for clang-tidy-enabled compilation.
- Generated: Yes
- Committed: No

**`packages/acreplay-wasm/dist/`:**
- Purpose: compiled JS and declaration output published by `@aiko/acreplay-wasm`.
- Generated: Yes
- Committed: Yes

**`packages/telemetry-pack/dist/`:**
- Purpose: compiled JS and declaration output published by `@aiko/telemetry-pack`.
- Generated: Yes
- Committed: Yes

**`.planning/codebase/`:**
- Purpose: generated codebase maps consumed by planning and execution workflows.
- Generated: Yes
- Committed: Usually yes within the GSD workflow

**`Replay Blender Importer/`:**
- Purpose: legacy Blender addon consuming CSV export output.
- Generated: No
- Committed: Yes

---

*Structure analysis: 2026-03-29*
