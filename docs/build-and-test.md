# Build And Test

## Preferred bootstrap with mise

This repo now uses a root-level [mise](https://mise.jdx.dev/) config to manage the shared developer toolchain.

From the repo root:

```bash
mise trust
mise install
mise run build:native
mise run test:cpp
mise run build:ts
```

Notes:

- `mise.toml` lives in the repo root and is the single shared config for `cpp/`, `packages/`, and future `apps/`.
- `mise.lock` should be committed when regenerated so the team gets reproducible tool resolution.
- `mise.local.toml` is for machine-local overrides and must stay uncommitted.
- `build:wasm` still expects an active `emsdk` / `emcmake` toolchain in `PATH`.

## Prerequisites

For native builds in WSL/Linux:

- `build-essential`
- `git`
- `zlib`

For wasm builds:

- `emsdk`
- `emcc`
- `emcmake`

When using `mise`, `python3`, `cmake`, `ninja`, `node`, `pnpm`, `clang-format`, `clang-tidy`, and `clangd` come from the shared project toolchain instead of the system package manager.

The native compiler and link/runtime libraries still come from the host system. In practice this means:

- use `mise` for shared developer tooling
- keep a working system C++ compiler toolchain installed for native builds
- keep `zlib` as a system dependency

## Native build

Preferred:

```bash
mise run build:native
mise run test:cpp
```

Equivalent direct commands:

```bash
cmake -S cpp -B cpp/build-native -G Ninja
cmake --build cpp/build-native
ctest --test-dir cpp/build-native --output-on-failure
```

What this covers:

- `acrp_core`
- `acrp_cli`
- fixture-based smoke test over `example.acreplay`
- native-vs-wasm-ABI parity test compiled natively

## Wasm build

Preferred when `emsdk` is already available:

```bash
mise run build:wasm
```

Equivalent direct commands:

```bash
emcmake cmake -S cpp -B cpp/build-wasm -G Ninja
cmake --build cpp/build-wasm
```

Expected output:

- `cpp/build-wasm/acrp_wasm/acrp_wasm.js`
- `cpp/build-wasm/acrp_wasm/acrp_wasm.wasm`

## TypeScript build

The repo currently uses package-local TypeScript builds:

Preferred:

```bash
mise run build:ts
```

Equivalent direct commands:

```bash
pnpx tsc -p packages/telemetry-pack/tsconfig.json
pnpx tsc -p packages/acreplay-wasm/tsconfig.json
```

Both packages emit ESM into `dist/`.

## End-to-end wasm smoke

After building native, wasm, and the TS packages:

Preferred:

```bash
mise run smoke:wasm
```

Equivalent direct command:

```bash
node scripts/wasm-e2e-smoke.mjs
```

This script verifies the full path:

```text
example.acreplay
  -> cpp/build-wasm/acrp_wasm.js + .wasm
  -> @aiko/acreplay-wasm
  -> @aiko/telemetry-pack
```

## Fixture notes

- Main fixture: `cpp/tests/fixtures/example.acreplay`
- Helper exporter: `cpp/build-native/tests/acrp_export_pack`
- Native smoke result currently confirms `7` lap packs for the example replay

## Sandbox note

Inside some restricted environments Emscripten cache may need to be redirected:

```bash
EM_CACHE=/tmp/emscripten-cache cmake --build cpp/build-wasm
```

In a normal WSL setup this is usually not needed.
