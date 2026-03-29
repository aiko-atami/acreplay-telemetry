# External Integrations

**Analysis Date:** 2026-03-29

## APIs & External Services

**Replay Parsing Toolchain:**
- Emscripten - Compiles `cpp/acrp_wasm/exports.cpp` and `cpp/acrp_core` into a modular ES module wasm target
  - SDK/Client: Emscripten toolchain via `emcmake` and link flags in `cpp/cmake/EmscriptenSettings.cmake`
  - Auth: Not applicable

**Local File Processing:**
- Host filesystem - The native CLI reads `.acreplay` files and writes CSV locally in `cpp/acrp_cli/main.cpp`
  - SDK/Client: C++ standard library `std::ifstream` and `std::filesystem`
  - Auth: Not applicable
- Node local filesystem - The wasm smoke script loads the fixture replay and wasm artifact from disk in `scripts/wasm-e2e-smoke.mjs`
  - SDK/Client: `node:fs/promises` and `node:path`
  - Auth: Not applicable

**Browser Runtime APIs:**
- Web Worker messaging - `packages/acreplay-wasm/src/worker.ts` exposes worker-based parser access through `self.onmessage` and `postMessage`
  - SDK/Client: Browser `Worker`/`MessageEvent` APIs
  - Auth: Not applicable

## Data Storage

**Databases:**
- Not detected
  - Connection: Not applicable
  - Client: Not applicable

**File Storage:**
- Local filesystem only
  - Native input/output path handling in `cpp/acrp_cli/main.cpp`
  - Fixture storage in `cpp/tests/fixtures/example.acreplay`
  - Built wasm artifacts under `cpp/build-wasm/acrp_wasm/` referenced by `scripts/wasm-e2e-smoke.mjs`

**Caching:**
- No application cache service detected
- Optional Emscripten build cache path override `EM_CACHE` is documented in `docs/build-and-test.md`; this is build-time tooling cache, not an app runtime cache

## Authentication & Identity

**Auth Provider:**
- None
  - Implementation: No auth, session, token, OAuth, API key, or identity provider code detected in `cpp/`, `packages/`, `scripts/`, or `docs/`

## Monitoring & Observability

**Error Tracking:**
- None

**Logs:**
- Local process logging only
  - CLI errors are written to stderr/stdout in `cpp/acrp_cli/main.cpp`
  - Smoke verification logs use `console.log` in `scripts/wasm-e2e-smoke.mjs`
  - wasm wrapper surfaces parser failures as thrown `Error` values in `packages/acreplay-wasm/src/index.ts` and worker error payloads in `packages/acreplay-wasm/src/worker.ts`

## CI/CD & Deployment

**Hosting:**
- Not detected
- `apps/README.md` reserves `apps/` for future frontends, but no deployed app configuration exists in the repository

**CI Pipeline:**
- No GitHub Actions, GitLab CI, CircleCI, or other CI config detected in project files reviewed for this analysis
- Local verification workflow is defined through `mise.toml` tasks and root `package.json` scripts

## Environment Configuration

**Required env vars:**
- `CC` - set by `mise.toml`
- `CXX` - set by `mise.toml`
- `CMAKE_GENERATOR` - set by `mise.toml`
- `CMAKE_EXPORT_COMPILE_COMMANDS` - set by `mise.toml`
- `EM_CACHE` - optional override documented in `docs/build-and-test.md` for restricted wasm build environments

**Secrets location:**
- No secrets storage detected
- No `.env`, `.env.*`, or `*.env` files were detected during this analysis

## Webhooks & Callbacks

**Incoming:**
- None

**Outgoing:**
- None

---

*Integration audit: 2026-03-29*
